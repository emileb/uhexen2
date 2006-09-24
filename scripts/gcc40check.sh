#!/bin/sh
#
# gcc-4.0.x causes bad hexen2 binaries when unit-at-a-time mode is active:
# crusader's tomed ice mace causes segmentation fault. the bug showed up
# after the cmd layer synchronization between hexen2 and hexenworld trees.
#
# this script is here to detect gcc-4.0 versions. on the other hand, 3.3.x,
# 3.4.x and 4.1 versions seem to produce good binaries.
#

if [ $# -lt 1 ]; then
	echo "No parameters given!"
	exit 1
fi

$1 --version 2> /dev/null 1> /dev/null
if [ $? -ne 0 ]; then
	echo "command $1 not found"
	exit 1
fi

compiler="$*"

MAJOR=`echo __GNUC__ | $compiler -E -xc - | tail -n 1`
MINOR=`echo __GNUC_MINOR__ | $compiler -E -xc - | tail -n 1`

if [ $MAJOR -eq 4 ] && [ $MINOR -eq 0 ]; then
	echo "yes"
elif [ $MAJOR -eq 3 ] && [ $MINOR -eq 5 ]; then
	echo "yes"
else
	echo "no"
fi

