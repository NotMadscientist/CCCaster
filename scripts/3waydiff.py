#!/usr/bin/env python

import sys

if len ( sys.argv ) <= 3:
    print 'Usage: %s good1 good2 bad' % sys.argv[0]
    print 'Lines in good1 are written to stdout'
    print 'Lines in bad are written to stderr'
    exit ( -1 )

with open ( sys.argv[1], 'r' ) as good1:
    with open ( sys.argv[2], 'r' ) as good2:
        with open ( sys.argv[3], 'r' ) as bad:
            one = good1.readlines()
            two = good2.readlines()
            three = bad.readlines()

for i in range ( len ( one ) ):

    if one[i] != two[i]:
        continue

    if one[i] != three[i]:
        print one[i],
        sys.stderr.write ( three[i] )
