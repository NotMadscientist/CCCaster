#!/usr/bin/env python

import sys, socket, md5
from time import time

UDP_IP = ""
UDP_PORT = 3939

BUFFER_SIZE = 4096 # 4 bytes

IDLE_TIMEOUT = 60 # 1 minutes for single rooms

ACTIVE_TIMEOUT = 20 # 20 seconds for occupied rooms

sock = None

def resetSocket():
    global sock
    sock = socket.socket ( socket.AF_INET, socket.SOCK_DGRAM )
    sock.bind ( ( UDP_IP, UDP_PORT ) )

rooms = {}
times = {}

resetSocket()

while True:
    try:
        data, addr = sock.recvfrom ( BUFFER_SIZE )
    except KeyboardInterrupt:
        break
    except:
        resetSocket()
        continue

    if len ( data ) == 0:
        if data in rooms:
            del rooms[data]
        if data in times:
            del times[data]
        continue

    print "got '%s' from %s" % ( data, addr )

    now = time()

    # Check if any rooms have timed out
    todelete = []
    for ( k, v ) in times.items():
        if len ( rooms[k] ) == 4 and now - v > ACTIVE_TIMEOUT:
            todelete.append ( k )
        elif now - v > IDLE_TIMEOUT:
            todelete.append ( k )

    # Delete those rooms
    for k in todelete:
        print "removed '%s'" % k
        del rooms[k]
        del times[k]

    # Check for existing room
    if not data in rooms:
        rooms[data] = ( addr, )
        times[data] = now
    else:
        room = rooms[data]

        # Add address to the room
        if len(room) == 1 and addr != room[0]:
            msg0 = '2 %s %d' % room[0]
            msg1 = '1 %s %d' % addr

            m = md5.new()
            m.update ( msg0 )
            msg0 = m.digest() + msg0

            m = md5.new()
            m.update ( msg1 )
            msg1 = m.digest() + msg1

            rooms[data] = ( room[0], addr, msg0, msg1 )

        # If the room is full, start replying
        if len ( room ) == 4:
            print repr ( room[0] )
            print repr ( room[1] )
            sock.sendto ( room[3], room[0] )
            sock.sendto ( room[2], room[1] )
