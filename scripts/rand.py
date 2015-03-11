#!/usr/bin/env python

import struct

# MBAA's rand() function implemented in Python
def rand ( randStr, applyCount=1 ):
    randStr = randStr.replace ( ' ','' ).decode ( 'hex' )
    rand0, rand1, rand2 = struct.unpack ( 'III', randStr[0:12] )
    rand3 = [ struct.unpack ( 'I', x ) [0] for x in map ( ''.join, zip ( * [ iter ( randStr[12:] ) ] * 4 ) ) ]

    ecx = rand2

    ecx += 1
    if ecx >= 0x38:
        ecx = 1

    rand2 = ecx

    edx = ecx + 0x15

    if ecx > 0x22:
        edx = ecx - 0x22

    eax = rand3 [ ecx - 1 ]
    eax -= rand3 [ edx - 1 ]

    if eax < 0:
        eax += 0x7FFFFFFF

    rand1 += 1

    rand3 [ ecx - 1 ] = ( eax & 0xFFFFFFFF )

    rand0 = ( eax & 0xFFFFFFFF )

    ret = struct.pack ( 'III', rand0, rand1, rand2 ) + ''.join ( [ struct.pack ( 'I', x ) for x in rand3 ] )
    ret = ret.encode ( 'hex' )
    ret = ' '.join ( map ( ''.join, zip ( * [ iter ( ret ) ] * 2 ) ) )

    if applyCount == 1:
        return ret
    else:
        return rand ( ret, applyCount - 1 )
