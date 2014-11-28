#!/usr/bin/env python

# TODO merge diffdummy into this script

import sys, os, re

if len ( sys.argv ) < 3 :
    print 'Usage: ' + os.path.basename ( sys.argv[0] ) + ' <sync-log-1> <sync-log-2>'
    exit ( 0 )


f = open ( sys.argv[1] )
g = open ( sys.argv[2] )

log1 = f.readlines()
log2 = g.readlines()

g.close()
f.close()


def mismatch ( i, j ):
    print 'Line', i + 1
    print '<', log1[i]
    print 'Line', j + 1
    print '>', log2[j]
    exit ( -1 )


count = 0

REGEX_FULL = '^[^ ]+ \[[0-9]+\] NetplayState::([A-Za-z]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'
REGEX_SHORT = '^([^ ]+) \[([0-9]+):([0-9]+)\] ([A-Za-z]+: .+)$'

regex1, regex2 = None, None
i, j = 4, 4


# Check SessionId
if log1[j] != log2[j]:
    mismatch ( i, j )


if re.match ( REGEX_FULL, log1[i + 1] ):
    regex1 = REGEX_FULL
    while log1[i].find ( 'CharaSelect' ) < 0:
        i += 1
    i -= 1
elif re.match ( REGEX_SHORT, log1[i + 1] ):
    regex1 = REGEX_SHORT
else:
    print 'Log 1 has unknown format!'
    exit ( -1 )


if re.match ( REGEX_FULL, log2[j + 1] ):
    regex2 = REGEX_FULL
    while log2[j].find ( 'CharaSelect' ) < 0:
        j += 1
    j -= 1
elif re.match ( REGEX_SHORT, log2[j + 1] ):
    regex2 = REGEX_SHORT
else:
    print 'Log 2 has unknown format!'
    exit ( -1 )


while i + 1 < len ( log1 ):
    i += 1
    m = re.match ( regex1, log1[i] )

    if not m:
        print 'Invalid line (%d) in log 1:' % ( i + 1 )
        print log1[i]
        exit ( -1 )

    # Skip Loading, Skippable, and RetryMenu states
    if ( m.group ( 1 ) == 'Loading' ) or ( m.group ( 1 ) == 'Skippable' ) or ( m.group ( 1 ) == 'RetryMenu' ):
        continue

    state1 = m.group ( 1 )
    index1 = int ( m.group ( 2 ) )
    frame1 = int ( m.group ( 3 ) )
    data1 = m.group ( 4 )

    while j + 1 < len ( log2 ):
        j += 1
        m = re.match ( regex2, log2[j] )

        if not m:
            print 'Invalid line (%d) in log 2:' % ( j + 1 )
            print log2[j]
            exit ( -1 )

        state2 = m.group ( 1 )
        index2 = int ( m.group ( 2 ) )
        frame2 = int ( m.group ( 3 ) )
        data2 = m.group ( 4 )

        if index2 < index1:
            # Skip line
            continue

        if ( index2 == index1 ) and ( frame2 < frame1 ):
            # Skip line
            continue

        if ( index2 == index1 ) and ( frame2 == frame1 ) and ( data2 != data1 ):
            mismatch ( i, j )

        if ( index2 == index1 ) and ( frame2 == frame1 ) and ( data2 == data1 ):
            # Matched
            count += 1
            break

        print 'Missing line (%d) in log 1 from log 2:' % ( i + 1 )
        print log1[i]
        exit ( -1 )


print 'Successfully matched', count, 'lines'
