# Upgrade from release 6.3
#
# Convert old rc file to .newsrc

cd

if [ ! -d .nn ]
then
	echo "No .nn directory"
	exit 1
fi

if [ ! -f .nn/rc ]
then
	echo "No rc file -- upgrade not possible"
	exit 2
fi

if [ x"$1" = "xn" ]
then
	echo "Using existing .newsrc"
else

	if [ -f .newsrc ]
	then
		rm -f .newsrc.old
		mv .newsrc .newsrc.old
		echo "Old .newsrc saved in .newsrc.old"
	fi
	echo "Creating .newsrc"

	${AWK} '
	NF != 3 {
		next
	}

	$1 == "+" || $1 == "!" {
		if ($1 == "+")
			printf("%s:", $3)
		else
			printf("%s!", $3)
		if ($2+0 > 1)
			printf(" 1-%d\n", $2+0)
		else
		if ($2 == 1)
			printf(" 1\n")
		else
			printf("\n")
	}' < .nn/rc > .newsrc

fi

cd .nn
rm -f rc-6.3 S.[0-9]*
mv rc rc-6.3
echo "Old rc file saved in rc-6.3"

echo "Upgrade completed"
exit 0
