#!/bin/sh
#  This is my version of a mailer that parses the To: lines and invokes the
#  domain-based mailer		mfr Jul 89

export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh $0; kill $$)

# System dependencies

mailer="/usr/local/smtp"
# your site name
case undef in
define) sitename=`hostname` ;;
undef) sitename="puppsr" ;;
esac

test=test
sed=/bin/sed
echo=/bin/echo
cat=/bin/cat
grep=/bin/grep
rm=/bin/rm

dotdir=${DOTDIR-${HOME-$LOGDIR}}
tmpart=$dotdir/.letter

	$cat >$tmpart

	case $mailer in
	*sendmail)
	    $mailer -t <$tmpart
	    ;;
# but recmail does not know about Bcc, alas
	*recmail)
	    $mailer <$tmpart
	    ;;
	*)
	    set X `$sed <$tmpart -n -e '/^To:/{' -e 's/To: *//p' -e q -e '}'`
	    shift
	    set X "$@" `$sed <$tmpart -n -e '/^Cc:/{' -e 's/Cc: *//p' -e q -e '}'`
	    shift
	    set X "$@" `$sed <$tmpart -n -e '/^Bcc:/{' -e 's/Bcc: *//p' -e q -e '}'`
	    shift
	    for i in "$@"
	    do
		thost=`echo $i | cut -s -d@ -f2`
		recip=`echo $i | cut -s -d@ -f1`
		sender=${LOGNAME}@${sitename}
		$grep -v "^Bcc:"  <$tmpart | $mailer $thost $sender $recip
	    done
	    ;;
	esac
	case $? in
	0)
	    state=cleanup
	    ;;
	*)
	    state=rescue
	    ;;
	esac
case $state in
    rescue)
	$cat $tmpart >> ${HOME-$LOGDIR}/dead.letter
	$echo "Message appended to ${HOME-$LOGDIR}/dead.letter"
	$echo "A copy may be temporarily found in $tmpart"
	exit
	;;
    cleanup)
	case "${MAILRECORD-none}" in
	none)
	    ;;
	*)  if $cat $tmpart >> $MAILRECORD; then
		$echo "Article appended to $MAILRECORD"
	    else
		$echo "Cannot append to $MAILRECORD"
	    fi
	    ;;
	esac
	exit
	;;
    esac
done


