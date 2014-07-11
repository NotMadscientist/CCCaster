#!/bin/sh

RUN=`ls cccaster*.exe | sort -r | head -1`

./$RUN $@

i686-w64-mingw32-gprof $RUN gmon.out > profile.txt
