'use strict';

(function () {
    var disabled = false;
    var sNode, eofNode, smsgNode;
    var lastDay, lastLine;
    var nextDay, nextLine;
    var socket;
    var pingTimer = null;
    var retryAttempt = 0, retryPending = false;

    function init() {
        try {
            sNode = document.getElementById('s');
            eofNode = document.getElementById('eof');
            smsgNode = document.getElementById('smsg');
            if (!sNode || !eofNode || !smsgNode)
                throw 'stalker page elements not found';

            setMessage('none');
            setMessage('note', 'Test message.');

            lastDay = parseInt(sNode.dataset.stalkerDay);
            lastLine = parseInt(sNode.dataset.stalkerLine);
            if (!lastDay)
                throw 'missing index of last message';

            window.setTimeout(scrollToBottom, 100);
        } catch (err) {
            disable(err);
            return;
        }

        connect();
    }

    function connect() {
        if (disabled) return;
        retryPending = false;
        try {
            var loc = window.location;
            if (!loc.pathname.endsWith('/stalker.html'))
                throw 'unable to build ws url: not .../stalker.html';
            var ws =
                (loc.protocol == 'https:' ? 'wss:' : 'ws:')
                + '//' + loc.host
                + loc.pathname.substr(0, loc.pathname.length - 12)
                + 'stalker.ws';
            console.info('stalker:', 'connecting to:', ws);

            socket = new WebSocket(ws, 'v1.stalker.logs.esolangs.org');
            socket.binaryType = 'arraybuffer';
            socket.onopen = onSocketOpen;
            socket.onmessage = onSocketMessage;
            socket.onclose = onSocketClose;
            socket.onerror = onSocketError;
        } catch (err) {
            disable(err);
        }
    }

    function onSocketOpen() {
        if (disabled) return;
        try {
            console.info('stalker:', 'connection open, registering as stalker');
            setMessage('none');
            sendStatus();
            pingTimer = window.setInterval(sendStatus, 60000);
        } catch (err) {
            disable(err);
        }
    }

    function onSocketMessage(event) {
        if (disabled) return;
        try {
            if (event.data instanceof ArrayBuffer) {
                window.debugStalkerEvent = event;
                if (event.data.byteLength != 8)
                    throw 'invalid header length';
                if (nextDay)
                    throw 'two consecutive headers';
                var dataView = new DataView(event.data);
                nextDay = dataView.getInt32(0, /* littleEndian: */ true);
                nextLine = dataView.getUint32(4, /* littleEndian: */ true);
            } else if (event.data instanceof String || typeof event.data === 'string') {
                if (!nextDay)
                    throw 'missing header';
                if (nextDay > lastDay || (nextDay == lastDay && nextLine > lastLine)) {
                    var scroll = window.scrollY + window.innerHeight >= document.documentElement.scrollHeight - 10;
                    var div = document.createElement('div');
                    div.innerHTML = event.data;
                    while (div.firstChild)
                        sNode.appendChild(div.firstChild);
                    if (scroll)
                        scrollToBottom();
                    lastDay = nextDay; lastLine = nextLine;
                    retryAttempt = 0;  // reset retry counter
                }
                nextDay = null; nextLine = null;
            } else {
                throw 'unexpected websocket message';
            }
        } catch (err) {
            disable(err);
        }
    }

    function onSocketClose(event) {
        if (disabled) return;
        console.warn('stalker:', 'websocket closed with code:', event.code);
        retry();
    }

    function onSocketError(event) {
        if (disabled) return;
        console.warn('stalker:', 'websocket error', event);
        retry();
    }

    function sendStatus() {
        if (disabled || !socket) return;
        var message = new ArrayBuffer(8);
        var view = new DataView(message);
        view.setInt32(0, lastDay, /* littleEndian: */ true);
        view.setUint32(4, lastLine, /* littleEndian: */ true);
        socket.send(message);
    }

    function retry() {
        if (disabled || retryPending) return;
        retryPending = true;

        socket = null;
        if (pingTimer !== null) {
            window.clearInterval(pingTimer);
            pingTimer = null;
        }

        try {
            ++retryAttempt;
            if (retryAttempt <= 5) {
                setMessage('note', 'Stalker server connection lost. Retrying (attempt ' + retryAttempt + '/5)...');
                window.setTimeout(connect, retryAttempt * 5000);
            } else {
                setMessage('note', 'Stalker server connection lost. Click this message to retry.', connect);
            }
        } catch (err) {
            disable(err);
        }
    }

    function disable(reason) {
        console.error('stalker:', reason);
        setMessage('error', 'Something went wrong with the scripts. :(');
        if (pingTimer !== null) {
            window.clearInterval(pingTimer);
            pingTimer = null;
        }
    }

    function setMessage(type, body, action) {
        if (!eofNode || !smsgNode) return;

        if (type == 'none') {
            smsgNode.textContent = '';
            smsgNode.onclick = null;
            eofNode.style.display = 'none';
        } else {
            smsgNode.textContent = body;
            smsgNode.className = (type == 'error' ? 'err' : 'note');
            if (action) {
                smsgNode.onclick = action;
                smsgNode.classList.add('act');
            } else {
                smsgNode.onclick = null;
            }
            eofNode.style.display = 'block';
        }
    }

    function scrollToBottom() {
        window.scrollTo(0, document.documentElement.scrollHeight);
    }

    document.addEventListener('DOMContentLoaded', init);
})();
