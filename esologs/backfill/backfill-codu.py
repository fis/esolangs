#! /usr/bin/env python3

# backfill #esoteric logs from codu.org raw log files

# protoc --python_out=esologs/backfill esologs/log.proto

import brotli
import datetime
import glob
import os
import re

from esologs import log_pb2
from google.protobuf.internal import encoder

epoch = datetime.datetime(1970, 1, 1)

for fname in sorted(glob.glob('tmp/codu/*-raw.txt')):
    m = re.search(r'/(\d+)-(\d+)-(\d+)-raw\.txt$', fname)
    if m is None: continue
    year, month, day = [int(v) for v in m.group(1, 2, 3)]

    logdir = 'tmp/logs/{}/{}'.format(year, month)
    logfile = '{}/{}.pb.br'.format(logdir, day)

    date = datetime.datetime(year, month, day)
    offset = (date - epoch).days * 86400

    print('{} -> {} -> {}'.format(fname, date.strftime('%Y-%m-%d'), logfile))
    log = bytearray()

    with open(fname, 'rb') as f:
        for line in f.readlines():
            line = line.rstrip(b'\r\n')
            m = re.match(rb'([<>]) (\d+) (\d+|\?) (?::(\S*) )?([^:]\S*)(?: (.*))?$', line)
            if m is None:
                raise Exception(line)

            direction, time_s, time_us, prefix, cmd, args = m.group(1, 2, 3, 4, 5, 6)
            if time_us == b'?': time_us = b'0'
            time_s, time_us = [int(v) for v in (time_s, time_us)]
            if time_s < offset or time_us < 0 or time_us >= 1000000:
                raise Exception('bad time: {} {}'.format(time_s, time_us))

            event = log_pb2.LogEvent()
            event.time_us = (time_s - offset) * 1000000 + time_us
            if prefix:
                event.prefix = prefix
            event.command = cmd
            if direction == '>':
                event.direction = log_pb2.LogEvent.SENT

            while args:
                if args[0] == b':'[0]:
                    event.args.append(args[1:])
                    break

                m = re.match(rb'([^:]\S*)(?: (.*))?$', args)
                if m is None:
                    raise Exception('bad args: ' + repr(args))
                event.args.append(m.group(1))

                args = m.group(2)

            event = event.SerializeToString()
            log += encoder._VarintBytes(len(event))
            log += event

    os.makedirs(logdir, exist_ok=True)
    with open(logfile, 'wb') as f:
        f.write(brotli.compress(bytes(log)))
