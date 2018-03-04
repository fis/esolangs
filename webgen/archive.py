import sys
import zipfile

_WEB_DIR = '/web/'
_DATE_TIME = (2000, 1, 1, 0, 0, 0)

def main(args):
    if len(args) < 2:
        sys.exit('usage: archive.py out.zip file1 file2 ...')
    out, files = args[0], args[1:]

    prefix = files[0]
    for f in files[1:]:
        prefix = ''.join([a if a == b else '\0' for a, b in zip(prefix, f)]).rstrip('\0')
    prefix_len = prefix.rfind(_WEB_DIR)
    if prefix_len < 0:
        raise RuntimeError('no common /web/ prefix: ' + repr(files))
    prefix = prefix[:prefix_len+len(_WEB_DIR)]

    with zipfile.ZipFile(out, 'w') as archive:
        for path in files:
            if not path.startswith(prefix):
                raise RuntimeError('not in common prefix: ' + path)
            suffix = path[len(prefix):]
            with open(path, 'rb') as f:
                data = f.read()
            archive.writestr(zipfile.ZipInfo(suffix, _DATE_TIME), data)

if __name__ == '__main__':
    main(sys.argv[1:])
