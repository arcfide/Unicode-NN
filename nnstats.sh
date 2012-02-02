# --- prefix is inserted above by make ---
#
# Extract collection & expiration statistics from Log file.
#
# From: Mark Moraes <moraes@csri.toronto.edu>
#
# Heavily modified by Kim F. Storm:
#  Added expiration statistics
#  Will now make daily statistics & grand summary
#  Split into two awk scripts to fit in argument list space
#  (and as a side-effect to be able to use functions in old awk!!!)

CHECK="0>0"
LONG=0
SUMMARY=1
debug=false

LOOP=true
while $LOOP; do
  case "$1" in
  -l)	LONG=1
	shift;;
  -d)	CHECK="\$2!=\"$2\" || \$3!=\"$3\""
	shift; shift; shift;;
  -m)	CHECK="\$2!=\"$2\""
	shift; shift;;
  -t)   SUMMARY=0
	shift;;
  -D)	debug=true
	shift;;
  -*)   echo "unknown option: $1"
	exit 1;;
  *)	LOOP=false
	;;
  esac
done

grep '^[CX]:' `ls -tr ${@-$LOG}` |
${AWK} 'BEGIN{
  day="0"; l='$LONG'; t='$SUMMARY'
  f=" %d %d %d %d %d %d %d\n"
}
'"$CHECK"' { next }
day!=$3{
  if(day!="0")print "d " date
  if(l && day!="0" && (run>0 || xrun>0)){
    print "h"
    printf "c" f,run,art,Mart,gr,Mgr,sec,Msec
    printf "e" f,xrun,xart,xMart,xgr,xMgr,xsec,xMsec
  }
  day=$3
  date=$2 " " $3
  run=art=gr=sec=0
  Mart=Mgr=Msec=0
  xrun=xart=xgr=xsec=0
  xMart=xMgr=xMsec=0
}
$6=="Collect:" {
  if ($7 <= 0) next
  run++; art+=$7; gr+=$9; sec+=$11
  Trun++; Tart+=$7; Tgr+=$9; Tsec+=$11
  if ($7 > Mart) Mart=$7;
  if ($9 > Mgr) Mgr=$9;
  if ($11 > Msec) Msec=$11;
  if ($11 > 0) A=$7 / $11; else A=$7;
  if (A > Mps) Mps=A;
  next
}
$6=="Expire:" {
  if ($7 <= 0) next
  xrun++; xart+=$7; xgr+=$9; xsec+=$11
  xTrun++; xTart+=$7; xTgr+=$9; xTsec+=$11
  if ($7 > xMart) xMart=$7
  if ($9 > xMgr) xMgr=$9
  if ($11 > xMsec) xMsec=$11
  if ($11 > 0) A=$7 / $11; else A=$7
  if (A > xMps) xMps=A
  next
}
END{
  if (day == "0") { print "Z"; exit }
  print "d " date
  if (l && (run > 0 || xrun > 0)) {
    print "h"
    printf "c" f,run,art,Mart,gr,Mgr,sec,Msec
    printf "e" f,xrun,xart,xMart,xgr,xMgr,xsec,xMsec
  }
  if (t) {
    print "H"
    printf "C %d %d %d %d\n",Trun,Tart,Tsec,Mps
    printf "E %d %d %d %d\n",xTrun,xTart,xTsec,xMps
  }
}' |
if $debug ; then
  cat
else
${AWK} 'BEGIN{
 first=""
}
/^d/{
 last=$2 " " $3
 if (first == "") first=last
 next
}
/^h/{
 printf "\nStatistics for %s\n", last
 next
}
/^H/{
 tostr=""
 if (first != last) tostr=" to " last
 printf "\nSummary for %s%s:\n", first, tostr
 next
}
/^[cC]/{tp="Collection"}
/^[eE]/{tp="Expiration"}
/^[ce]/{
 if ($2 == 0) next
 printf "\n  %s runs: %d\n", tp, $2
 printf "%10d articles, average of %5d per run, maximum %6d\n", $3, $3/$2, $4
 printf "%10d groups,   average of %5d per run, maximum %6d\n", $5, $5/$2, $6
 printf "%10d seconds,  average of %5d per run, maximum %6d\n", $7, $7/$2, $8
 next
}
/^[CE]/{
 printf "\n  %s runs: %d\n", tp, $2
 if ($2 == 0) next
 printf "%10d articles, average of %5d per run\n", $3, $3/$2
 printf "%10d seconds,  average of %5d per run\n", $4, $4/$2
 if ($4>0) avg=$3/$4; else avg=$3
 printf "    average of %d articles per second, maximum %d\n", avg, $5
}
/^Z/{
 printf "Log is empty\n"
}'
fi

