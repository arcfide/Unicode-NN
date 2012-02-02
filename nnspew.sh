# nnspew -- generate subject line database
#
# From: James A. Woods (ames!jaw), NASA Ames Research Center
#
# Updates the auxiliary 'subjects' file.  It must be activated regularly
# via a system-wide 'cron' or 'at' command, e.g.
#
# 	2 6,9,12,15,18,21 * * *  root /bin/nice /usr/lib/nn/nnspew

trap "rm -f $TMP/nnsubj$$* ; exit" 0 1 2 15

# We use a "secret" 'nn -SPEW' call to do the hard part:

if $BIN/nn -SPEW < /dev/null > $TMP/nnsubj$$a
then
	if sort -u $TMP/nnsubj$$a > $TMP/nnsubj$$b
	then
		mv $TMP/nnsubj$$b $DB/subjects
		chmod 644 $DB/subjects
	fi
fi
