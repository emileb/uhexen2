#!/bin/sh

echo ""
echo "========================================================"
echo "This will patch Hexen2 PAK files (cdrom-1.03 version)"
echo "to 1.11, which is a required update by Raven/Activision."
echo -n "Would you like to apply this update? [Y/n]: "
read answer
case $answer in
    [Nn]*)
	exit 0
	;;
    *)
	;;
esac

echo ""

if [ ! -f "loki_patch" ]; then
	echo "Patch binary loki_patch not found. Quitting."
	echo "You can compile it from its up-to-date source"
	echo "tarball loki_patch-src2.tgz, downloadable from"
	echo -e "the Hammer of Thyrion website.\n"
	exit 1
fi

PATCH_DAT="update_loki.dat"

if [ ! -f ${PATCH_DAT} ]; then
	echo -e "Patch data file ${PATCH_DAT} not found. Quitting.\n"
	exit 1
fi

./loki_patch ${PATCH_DAT} .
status=$?
echo -e "                \n"
exit $status

