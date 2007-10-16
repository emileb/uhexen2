#!/bin/sh

UHEXEN2_TOP=../..
. $UHEXEN2_TOP/scripts/cross_defs

BIN_DIR=../bin

if [ "$1" = "strip" ]; then
	$STRIPPER $BIN_DIR/jsh2colour.exe
	exit 0
fi

HOST_OS=`uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]'`

case "$HOST_OS" in
freebsd|openbsd|netbsd)
	MAKE_CMD=gmake
	;;
linux)
	MAKE_CMD=make
	;;
*)
	MAKE_CMD=make
	;;
esac

if [ "$1" = "clean" ]; then
	$MAKE_CMD -s clean
	exit 0
fi

exec $MAKE_CMD $*

