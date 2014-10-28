#!/usr/bin/env python

import sys, socket, select, struct, traceback

IP_ADDR_PORT = ( '', 3939 )

TCP_BACKLOG = 5

BUFFER_SIZE = 4096


tcpServerSocket = None
udpServerSocket = None
inputSockets = []
outputSockets = []

# socket -> address
addresses = None

# address -> host socket
hosts = None

# matchId -> [ client socket, host socket ]
matches = {}


currentMatchId = 0

def nextMatchId():
    global currentMatchId

    if currentMatchId == 0:
        currentMatchId = 1
    else:
        currentMatchId += 1

    while currentMatchId in matches:
        currentMatchId += 1
        currentMatchId %= ( 2**32 )

    if currentMatchId == 0:
        currentMatchId = 1

    return currentMatchId


def close():
    for s in inputSockets:
        s.close()

    exit ( 0 )


def reset():
    global tcpServerSocket, udpServerSocket, inputSockets, addresses, hosts

    for s in inputSockets:
        s.close()

    try:
        tcpServerSocket = socket.socket ( socket.AF_INET, socket.SOCK_STREAM )
        tcpServerSocket.setsockopt ( socket.SOL_SOCKET, socket.SO_REUSEPORT, 1 )
        tcpServerSocket.setblocking ( 0 )
        tcpServerSocket.bind ( IP_ADDR_PORT )

        udpServerSocket = socket.socket ( socket.AF_INET, socket.SOCK_DGRAM )
        udpServerSocket.setsockopt ( socket.SOL_SOCKET, socket.SO_REUSEPORT, 1 )
        udpServerSocket.setblocking ( 0 )
        udpServerSocket.bind ( IP_ADDR_PORT )

        tcpServerSocket.listen ( TCP_BACKLOG )

        inputSockets = [ tcpServerSocket, udpServerSocket ]

        addresses = {}

        hosts = {}

    except:
        print '=' * 60
        traceback.print_exc ( file=sys.stdout )
        print '=' * 60

        close()


reset()


while True:
    try:
        readable, writable, exceptional = select.select ( inputSockets, outputSockets, inputSockets )

        if len ( exceptional ) != 0:
            print len ( exceptional ), 'exceptional sockets'

            reset()
            continue

        for s in readable:
            if s is tcpServerSocket:
                tcpSocket, address = s.accept()
                tcpSocket.setblocking ( 0 )

                inputSockets.append ( tcpSocket )

                addresses[tcpSocket] = address[0];

                print 'accepted', address[0]

            elif s is udpServerSocket:
                data, address = s.recvfrom ( BUFFER_SIZE )

                print 'UDP data', repr ( data ), 'address', address

                try:
                    index, matchId = struct.unpack ( '<BI', data )

                    if ( 0 <= index <= 1 ) and ( matchId in matches ):
                        # if matching TCP socket is found, send UDP address once
                        if matches[matchId][index]:
                            tunInfo = 'TunInfo' + struct.pack ( '<I', matchId ) + '%s:%u\0' % address

                            matches[matchId][index].send ( tunInfo )
                            matches[matchId][index] = None

                        # remove the match once both have been sent
                        if ( not matches[matchId][0] ) and ( not matches[matchId][1] ):
                            del matches[matchId]
                            print 'matches', matches.keys()

                    continue
                except:
                    continue

            elif s in inputSockets:
                try:
                    data = s.recv ( BUFFER_SIZE )
                except:
                    continue

                print 'TCP data', repr ( data )

                if data:
                    if len ( data ) == 2:
                        port = struct.unpack ( '<H', data ) [0]

                        print 'port', port

                        # port must be non-zero
                        if port:
                            address = '%s:%u' % ( addresses[s], port )

                            hosts[address] = s
                            addresses[s] = address

                            print 'hosts', hosts.keys()
                            continue

                    elif 2 < len ( data ) <= 21:
                        # if matching
                        if data in hosts:
                            matchId = nextMatchId()

                            matchInfo = 'MatchInfo' + struct.pack ( '<I', matchId )

                            s.send ( matchInfo )
                            hosts[data].send ( matchInfo )

                            matches[matchId] = [ s, hosts[data] ]

                            print 'matched', data
                            print 'matches', matches.keys()
                            continue

                # otherwise disconnect the client
                if s in addresses:
                    if addresses[s] in hosts:
                        del hosts[addresses[s]]
                        print 'hosts', hosts.keys()
                    del addresses[s]

                for matchId, pair in matches.items():
                    if ( s == pair[0] ) or ( s == pair[1] ):
                        del matches[matchId]
                        print 'matches', matches.keys()

                inputSockets.remove ( s )
                s.close()

    except KeyboardInterrupt:
        close()

    except:
        print '=' * 60
        traceback.print_exc ( file=sys.stdout )
        print '=' * 60

        reset()
        continue
