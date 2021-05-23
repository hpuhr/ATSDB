#!/bin/bash

PROGNAME=`basename $0`
REF=
REF_NAME=
TST=
TST_NAME=
START_DATE=
TIME_SHIFT=
DATE=$(date '+%Y-%m-%d_%H%M%S')

help() {
       cat <<EOF
USAGE: $PROGNAME -r <reference> -t <test> [-R <name>] [-T <name>] [-d <date>] [-s <time>] [-h]
   -r <reference> ::= reference file name (final format)
   -R <name> ::= reference tracker name (default: REF)
   -t <test> ::= test file name (final format)
   -T <name> ::= test tracker name (default: TST)
   -d <date> ::= reference and test date (format: yyyy-mm-dd)
   -s <time> ::= difference in time between test and reference runs (seconds)
   -h     This help
EOF
}

while getopts "hd:s:r:R:t:T:" opt
  do
  case "$opt" in
      d) START_DATE="-sd $OPTARG" ;;
      s) TIME_SHIFT="-dt=$OPTARG" ;;
      r) REF="$OPTARG" ;;
      R) REF_NAME="-r $OPTARG" ;;
      t) TST="$OPTARG" ;;
      T) TST_NAME="-t $OPTARG" ;;
      h) help; exit 0 ;;
      *) help; exit 2 ;;
  esac
done


if [ -z "$REF" -o -z "$TST" ]; then
   help
   exit 1
fi

if [ ! -f "$REF" ]; then
   echo "reference run:$REF not found (bailing out...)"
   exit 1
fi
if [ ! -f "$TST" ]; then
   echo "test run:$TST not found (bailing out...)"
   exit 1
fi

aoc -hlp $TIME_SHIFT -asf $PWD/$REF $PWD/$TST aoc_$DATE.res > aoc_$DATE.out
mv aoc.hlp aoc_$DATE.hlp

aoc_vp.py -ah aoc_$DATE.hlp -ao aoc_$DATE.out $REF_NAME $TST_NAME $START_DATE -o aoc_vp_$DATE.json
