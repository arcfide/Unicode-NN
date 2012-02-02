# CONFIG file is inserted here

OPT=""
SORTMODE=""
ALL=false

LOOP=true
while $LOOP
do
	case "$1" in
	-a)	OPT="$OPT -a"
		ALL=true
		shift ;;
	-t)	SORTMODE="+1nr"
		shift ;;
	-at)	OPT="$OPT -a"
		ALL=true
		SORTMODE="+1nr"
		shift ;;
	-*)	echo "$0: unknown option: $1"
		exit 1
		;;
	*)	LOOP=false
		;;
	esac
done

if [ -f $DB/acct -a -f $BIN/nnacct ] ; then
	if $AUTH ; then
	echo "USER        USAGE  QUOTA  LAST_ACTIVE   COST/PERIOD   POLICY"
	else
	echo "USER        USAGE  QUOTA  LAST_ACTIVE   COST/PERIOD"
	fi
	$BIN/nnacct -r $OPT $@ | sed -e 1d | sort $SORTMODE
	exit
fi

OLDLOG=${LOG}.old
if [ ! -s ${OLDLOG} ]
then
  OLDLOG=""
fi

cat $OLDLOG $LOG |
if $ALL
then
	grep '^U:'
else
	grep "^U:.*(${LOGNAME-$USER})"
fi |

${AWK} '
BEGIN {
	any=0
}
NF == 7 {
	if (split($7, t, ".") == 2) {
		u[$5] += t[1] * 60 + t[2]
		if (any == 0) printf("Usage since %s %d, %s\n", $2, $3, $4)
		any=1
	}
}
END {
	if (!any) {
		printf("No usage statistics\n")
		exit
	}
	for (n in u) {
		name=substr(n, 2, length(n)-3)
		printf("%-10.10s%8d.%02d\n", name, u[n]/60, u[n]%60);
	}
}' |

sort $SORTMODE
