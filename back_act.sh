# prefix is inserted above by make

#
#	back_act will maintain a set of `old' active files
#	in the DB directory where they can be used by nngoback
#	to backtrack the rc file a number of days.
#
#	It should be invoked by cron every day at midnight.
#	It should run as user `news'!
#
#	Call:  back_act [days]
#	Default: keep copy of active file for the last 14 days.

cd $DB || exit 1

p=${1-15}
l=""
while [ "$p" -gt 0 ]
do
	i="`expr $p - 1`"
	if [ -f active.$i ]
	then
		mv active.$i active.$p
		l=$p
	elif [ -n "$l" ]
	then
		ln active.$l active.$p
	fi
	p=$i
done

cp $ACTIVE active.0
chmod 644 active.0
