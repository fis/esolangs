import os
import re
import requests
import sys
import time
import zipfile

import BaseHTTPServer
import SocketServer
import urlparse

_DEFAULT_PORT = 28082
_REFRESH_TIME = 5

def content_type(path):
    if path.endswith('.html'): return 'text/html; charset=utf-8'
    if path.endswith('.js'): return 'application/javascript; charset=utf-8'
    if path.endswith('.css'): return 'text/css; charset=utf-8'
    return 'application/octet-stream'

class ZipServer(object):
    def __init__(self, archives, redir):
        self._archives = {}
        for i, archive in enumerate(archives):
            self._archives[archive] = {
                'zip': zipfile.ZipFile(archive, 'r'),
                'mtime': os.stat(archive).st_mtime,
                'priority': i,
            }
        self._CachePaths()
        self._last_check = time.time()
        self._redir = redir

    def Lookup(self, path):
        for redir_re, redir_target in self._redir.iteritems():
            m = redir_re.match(path)
            if m:
                return self._Fetch(redir_target, m.groups())

        path = urlparse.urlparse(path).path
        if path.endswith('/'): path += 'index.html'

        if time.time() >= self._last_check + _REFRESH_TIME:
            self._Refresh()
        item = self._paths.get(path)
        if item:
            return item['zip'].read(item['info']), content_type(path)
        return None, None

    def _Refresh(self):
        reloaded = False
        for path, info in self._archives.iteritems():
            mtime = os.stat(path).st_mtime
            if mtime > info['mtime']:
                sys.stderr.write('reloading modified archive: %s\n' % path)
                info['zip'].close()
                info['zip'] = zipfile.ZipFile(path, 'r')
                info['mtime'] = mtime
                reloaded = True
        if reloaded:
            self._CachePaths()
        self._last_check = time.time()

    def _CachePaths(self):
        self._paths = {}
        for archive in self._archives.itervalues():
            for info in archive['zip'].infolist():
                path = '/' + info.filename
                if path in self._paths and self._paths[path]['priority'] < archive['priority']:
                    continue
                self._paths[path] = {
                    'zip': archive['zip'],
                    'info': info,
                    'priority': archive['priority'],
                }

    def _Fetch(self, url, groups):
        for i, g in enumerate(groups):
            url = url.replace("$%d" % (i+1), g)
        sys.stderr.write('-> %s\n' % url)
        r = requests.get(url)
        r.raise_for_status()
        return r.content, r.headers['content-type']

    def GetHandler(self, request, client_address, server):
        return ZipHandler(self, request, client_address, server)

class ZipHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def __init__(self, zipserver, request, client_address, server):
        self._zipserver = zipserver
        BaseHTTPServer.BaseHTTPRequestHandler.__init__(self, request, client_address, server)

    def do_GET(self):
        data, ctype = None, None
        try:
            data, ctype = self._zipserver.Lookup(self.path)
        except Exception as e:
            self.send_error(500, str(e))
            raise
        if data is not None:
            self.send_response(200)
            self.send_header('Content-Type', ctype)
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_error(404, 'unknown path: ' + self.path)

def serve(archives, redir):
    r = redir.split('`')
    if len(r) % 2 != 0:
        sys.exit('expected even number of redir items: ' + repr(r))
    handler_factory = ZipServer(archives, dict((re.compile(r[i]), r[i+1]) for i in range(0, len(r), 2)))

    min_port, max_port = _DEFAULT_PORT, _DEFAULT_PORT+100
    selected_port = None
    for port in range(min_port, max_port+1):
        try:
            server = SocketServer.TCPServer(('', port), handler_factory.GetHandler)
            selected_port = port
            break
        except:
            sys.stderr.write('failed to bind to port %d, trying next free port\n' % port)
    if not selected_port: system.exit('failed to find a free port')

    sys.stderr.write('SERVER READY: http://localhost:%d/\n' % selected_port)
    server.serve_forever()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit('usage: preview.py site.zip fallback.zip ...')
    archives = sys.argv[1:]
    serve(archives)
