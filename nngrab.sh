# nngrab -- quick news retrieval by keyword
#
# From: James A. Woods (ames!jaw), NASA Ames Research Center
#
# Naturally, you're running fast e?grep (GNU-style) or this is all for
# naught.


FOLDCASE=""
case $1 in
-c)
	FOLDCASE="-i"
	shift
esac

case $# in
1) ;;
*)
	echo >&2 "usage: $0 [-c] keyword-pattern"
	exit 1
esac

case $1 in
*[A-Z]*) KW="`echo "$1" | tr '[A-Z]' '[a-z]'`";;
*) KW=$1
esac

if [ -s $DB/subjects ] ; then
	groups=`
		egrep "^[^:]*:.*${KW}" $DB/subjects |
		sed 's/:.*//' |
		uniq
	`

	case $groups in
	'')
		echo >&2 "Pattern '$1' not found in any subjects"
		exit 1
	esac

	groups="-G $groups"
else
	groups=all
fi

exec $BIN/nn -Q -mxX $FOLDCASE -s/"$1" $groups
