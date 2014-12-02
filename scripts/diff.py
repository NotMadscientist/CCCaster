#!/usr/bin/env python

import sys, os, re

if len ( sys.argv ) < 3 :
    print 'Usage: ' + os.path.basename ( sys.argv[0] ) + ' <sync-log-1> <sync-log-2>'
    exit ( 0 )


f = open ( sys.argv[1] )
g = open ( sys.argv[2] )

log = [ f.readlines(), g.readlines() ]

g.close()
f.close()


def mismatch ( i, j ):
    print 'Line', i + 1
    print '<', log[0][i]
    print 'Line', j + 1
    print '>', log[1][j]
    exit ( -1 )


REGEX_FULL = '^[^ ]+ \[[0-9]+\] NetplayState::([A-Za-z]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'
REGEX_SHORT = '^([A-Za-z]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'

SKIP_STATES = set ( [ 'Loading', 'Skippable', 'RetryMenu' ] )


count = 0
regex = [ None, None ]
index = [ 4, 4 ]


# Check SessionId
if log[0][4] != log[1][4]:
    mismatch ( 4, 4 )


for i in range ( 0, 2 ):

    if re.match ( REGEX_FULL, log[i][index[i] + 1] ):
        regex[i] = REGEX_FULL
        while log[i][index[i] + 1].find ( 'CharaSelect' ) < 0:
            index[i] += 1

    elif re.match ( REGEX_SHORT, log[i][index[i] + 1] ):
        regex[i] = REGEX_SHORT
        index[i] += 1

    else:
        print 'Log %d has unknown format!' % ( i + 1 )
        exit ( -1 )


# Flag log types
dummy = [ ( regex[0] == REGEX_SHORT ), ( regex[1] == REGEX_SHORT ) ]


while ( index[0] < len ( log[0] ) ) and ( index[1] < len ( log[1] ) ):

    data = [ None, None ]

    for i in range ( 0, 2 ):
        m = re.match ( regex[i], log[i][index[i]] )

        if not m:
            print 'Invalid line (%d) in log %d:' % ( index[i], i + 1 )
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

    mismatch ( index[0], index[1] )


print 'Successfully matched', count, 'lines'
