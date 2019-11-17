#! /usr/bin/env python3

# backfill #esoteric logs from PostgreSQL logs database

# protoc --python_out=esologs/backfill esologs/log.proto

import brotli
import datetime
import os
import psycopg2

from esologs import log_pb2
from google.protobuf.internal import encoder

conn = psycopg2.connect('')
cur = conn.cursor()

cur.execute('SET search_path TO logs')

cur.execute('''
SELECT
  e.tstamp at time zone 'UTC', e.type,
  n.name, u.user, u.host,
  o.name,
  body
FROM
  event e
  LEFT JOIN nick n ON n.id = e.nick
  LEFT JOIN uhost u ON u.id = e.uhost
  LEFT JOIN nick o ON o.id = e.other
WHERE
  e.target = 2
  AND e.tstamp >= timestamp '2019-05-28 00:00:00' at time zone 'UTC'
  and e.tstamp < timestamp '2019-06-04 00:00:00' at time zone 'UTC'
ORDER BY e.idx ASC
''')

cur_log = bytearray()
cur_date = datetime.date(1, 1, 1)

def write_log(date, log):
    logdir = 'tmp/logs/{}/{}'.format(date.year, date.month)
    logfile = '{}/{}.pb.br'.format(logdir, date.day)

    print('{} -> {}'.format(date.strftime('%Y-%m-%d'), logfile))

    os.makedirs(logdir, exist_ok=True)
    with open(logfile, 'wb') as f:
        f.write(brotli.compress(bytes(log)))

def time_us(time):
    return time.hour * 3600000000 + time.minute * 60000000 + time.second * 1000000 + time.microsecond

events = {
    'msg':    (b'PRIVMSG', ['chan', 'body']),
    'notice': (b'PRIVMSG', ['chan', 'body']),
    'act':    (b'PRIVMSG', ['chan', 'act']),
    'join':   (b'JOIN',    ['chan']),
    'part':   (b'PART',    ['chan', 'body']),
    'quit':   (b'QUIT',    ['body']),
    'nick':   (b'NICK',    ['nick']),
    'kick':   (b'KICK',    ['chan', 'nick']),
    'topic':  (b'TOPIC',   ['chan', 'body']),
    'mode':   (b'MODE',    'mode'),
}
# act, mode

while True:
    row = cur.fetchone()
    if row is None: break

    tstamp, etype, nick, user, host, othernick, body = row
    if nick is None:
        continue

    if body is not None: body = body.encode('utf-8')
    if othernick is not None: othernick = othernick.encode('utf-8')

    if tstamp.date() != cur_date:
        if cur_log:
            write_log(cur_date, cur_log)
        cur_log = bytearray()
        cur_date = tstamp.date()

    event = log_pb2.LogEvent()
    event.time_us = time_us(tstamp.time())
    event.prefix = (nick + '!' + (user or '?') + '@' + (host or '?')).encode('utf-8')

    cmd, args = events[etype]
    event.command = cmd
    if args == 'mode':
        event.args.append('#esoteric')
        if othernick is not None:
            event.args.append(body)
            event.args.append(othernick)
        elif len(body) > 2 and body[2] == ' ':
            event.args.append(body[:2])
            event.args.append(body[3:])
        else:
            event.args.append(body)
    else:
        for arg in args:
            if   arg == 'chan': event.args.append(b'#esoteric')
            elif arg == 'body': event.args.append(body or b'')
            elif arg == 'act':  event.args.append(b'\x01ACTION ' + (body or b'') + b'\x01')
            elif arg == 'nick': event.args.append(othernick or b'?unknown?')

    event = event.SerializeToString()
    cur_log += encoder._VarintBytes(len(event))
    cur_log += event

if cur_log:
    write_log(cur_date, cur_log)

