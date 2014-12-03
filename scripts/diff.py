#!/usr/bin/env python

import sys, os, re

if len ( sys.argv ) < 3 :
    print 'Usage: ' + os.path.basename ( sys.argv[0] ) + ' sync-logs...'
    exit ( 0 )


REGEX_FULL = '^[^ ]+ \[[0-9]+\] NetplayState::([A-Za-z]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'
REGEX_SHORT = '^([A-Za-z]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'

SKIP_STATES = set ( [ 'Loading', 'Skippable', 'RetryMenu' ] )


log = [ None, None ]


def printMismatch ( i, j, k ):
    print 'Line %d in %s' % ( i + 1, sys.argv[1] )
    print '<', log[0][i]
    print 'Line %d in %s' % ( j + 1, sys.argv[k] )
    print '>', log[1][j]


with open ( sys.argv[1] ) as f:
    log[0] = f.readlines()


for k in range ( 2, len ( sys.argv ) ):

    with open ( sys.argv[k] ) as f:
        log[1] = f.readlines()


    count = 0
    regex = [ None, None ]
    index = [ 4, 4 ]


    # Check SessionId
    if log[0][4] != log[1][4]:
        printMismatch ( 4, 4, k )
        continue


    # Check log format
    for i in range ( 0, 2 ):

        if index[i] + 1 >= len ( log[i] ):
            print '%s is empty!' % sys.argv [ ( 1, k )[i] ]
            break

        if re.match ( REGEX_FULL, log[i][index[i] + 1] ):
            regex[i] = REGEX_FULL
            while log[i][index[i] + 1].find ( 'CharaSelect' ) < 0:
                index[i] += 1

        elif re.match ( REGEX_SHORT, log[i][index[i] + 1] ):
            regex[i] = REGEX_SHORT
            index[i] += 1

        else:
            print '%s has unknown format!' % sys.argv [ ( 1, k )[i] ]
            break

    if ( not regex[0] ) or ( not regex[1] ):
        continue


    # Flag log types
    dummy = [ ( regex[0] == REGEX_SHORT ), ( regex[1] == REGEX_SHORT ) ]


    while ( index[0] < len ( log[0] ) ) and ( index[1] < len ( log[1] ) ):

        data = [ None, None ]


        for i in range ( 0, 2 ):
            m = re.match ( regex[i], log[i][index[i]] )

            if not m:
                print 'Invalid line (%d) in log %d:' % ( index[i], ( 1, k )[i] )
                print log[i][index[i]]
                exit ( -1 )

            # NetplayState, index, frame, input/RNG data
            data[i] = [ m.group ( 1 ), int ( m.group ( 2 ) ), int ( m.group ( 3 ) ), m.group ( 4 ) ]


        # Assign dummy NetplayState
        if ( data[0][0] == 'Dummy' ) and ( data[0][1] == data[1][1] ):
            data[0][0] = data[1][0]

        if ( data[1][0] == 'Dummy' ) and ( data[1][1] == data[0][1] ):
            data[1][0] = data[0][0]

        # Ignored NetplayStates
        if data[0][0] in SKIP_STATES:
            # Skip state
            index[0] += 1
            continue

        if data[1][0] in SKIP_STATES:
            # Skip state
            index[1] += 1
            continue

        # Ignore RngStates if dummy sync log
        if dummy[0] and ( data[1][3].find ( 'RngState' ) == 0 ):
            # Skip RngState data
            index[1] += 1
            continue

        if dummy[1] and ( data[0][3].find ( 'RngState' ) == 0 ):
            # Skip RngState data
            index[0] += 1
            continue

        # Skip older transition indicies
        if data[0][1] < data[1][1]:
            # Skip line
            index[0] += 1
            continue

        if data[1][1] < data[0][1]:
            # Skip line
            index[1] += 1
            continue

        # Skip initial frames before we match the first line
        if ( count == 0 ) and ( data[0][2] < data[1][2] ):
            # Skip line
            index[0] += 1
            continue

        if ( count == 0 ) and ( data[1][2] < data[0][2] ):
            # Skip line
            index[1] += 1
            continue

        # Match the line
        if data[0] == data[1]:
            # Matched
            count += 1
            index[0] += 1
            index[1] += 1
            continue

        printMismatch ( index[0], index[1], k )
        break


    print 'Successfully matched', count, 'lines (%s vs %s)' % ( sys.argv[1], sys.argv[k] )
