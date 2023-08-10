#!/bin/bash

# consts
CSG2STPDIR=./
TMPDIR=$(mktemp -d)
PAR=$(nproc --all)

SCAD=$1
PCS=$2

ERR=0

if [ x"$SCAD" == x"" ]; then
  ERR=1
else
  shift
fi

if [ x"$PCS" == x"" ]; then
  PCS=1
else
  shift
fi

NAM=$(basename $SCAD .scad)

if [ ! -f $SCAD ]; then
  echo "$SCAD not found"
  ERR=1
fi

if [ $ERR -ne 0 ]; then
  echo "Usage: $0 part.scad [loops] [openscad argument list]"
  echo "       The generation contains some randomization"
  echo "       and because of that multiple generation may"
  echo "       necessary. Use the 'loops' parameter to define"
  echo "       the number of STEP files generated."
  exit 0
fi

openscad -o $TMPDIR/$NAM.csg "$@" $SCAD
test $? -eq 0 || exit 1
$CSG2STPDIR/csg2xml <$TMPDIR/$NAM.csg >$TMPDIR/$NAM.xml
test $? -eq 0 || exit 2
$CSG2STPDIR/xml2mak $TMPDIR/$NAM.xml $NAM.stp >$TMPDIR/$NAM.mak
test $? -eq 0 || exit 3

for((i=0;i<$PCS;i++)) do
  make -j$PAR -f $TMPDIR/$NAM.mak &>/dev/null
  if [ -f $NAM.stp ]; then
    if [ $PCS -gt 1 ]; then
      mv $NAM.stp $NAM-$i.stp
      echo "$NAM-$i.stp done"
    else
      echo "$NAM.stp done"
    fi
  fi
done

rm -rf $TMPDIR
