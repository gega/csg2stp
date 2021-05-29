#!/bin/bash

# consts
CSG2STPDIR=./
TMPDIR=$(mktemp -d)
PAR=6

SCAD=$1
ERR=0

if [ x"$SCAD" == x"" ]; then
  ERR=1
fi

NAM=$(basename $SCAD .scad)

if [ ! -f $SCAD ]; then
  echo "$SCAD not found"
  ERR=1
fi

if [ $ERR -ne 0 ]; then
  echo "Usage: $0 part.scad [openscad argument list]"
  exit 0
fi

shift

openscad -o $TMPDIR/$NAM.csg "$@" $SCAD
test $? -eq 0 || exit 1
$CSG2STPDIR/csg2xml <$TMPDIR/$NAM.csg >$TMPDIR/$NAM.xml
test $? -eq 0 || exit 2
$CSG2STPDIR/xml2mak $TMPDIR/$NAM.xml $NAM.stp >$TMPDIR/$NAM.mak
test $? -eq 0 || exit 3
make -j$PAR -f $TMPDIR/$NAM.mak &>$TMPDIR/$NAM.log
test $? -eq 0 || exit 4
rm -rf $TMPDIR
