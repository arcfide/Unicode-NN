
# PREFIX is inserted above this line during Make

trap : 2 3

PATH=/bin:$PATH

# The .param file is found in $NNDIR/.param.  Formerly this was
# hardcoded, but with the nn-directory variable that no longer works.

if [ -z "$NNDIR" ]; then NNDIR=$HOME/.nn; fi

# Paramaters transferred from nn via .param file:
#	ART_ID		Article id to cancel
#	GROUP		Group of article to cancel
#	WORK		Temporary file for response (w initial contents)
#	FIRST_ACTION	First action to perform
#	EMPTY_CHECK	[empty-response-check]
#	EDITOR		[editor]
#	ED_LINE		First line of body in WORK file
#	SPELL_CHECKER	[spell-checker]
#	PAGER		[pager]
#	APPEND_SIG	[append-signature]
#	QUERY_SIG	[query-signature]
#	NOVICE		[expert]
#	WAIT_PERIOD 	[response-check-pause]
#	RECORD		[mail/news-record]
#	MMDF_SEP	[mmdf-format = ^A^A^A^A]
#	POST		[inews]
#	POST_PIPE	[inews-pipe-input]
#	POSTER_ADR	Reply address for follow-ups
#	MAILER		[mailer]
#	MAILER_PIPE	[mailer-pipe-input]
#	DFLT_ANSW	[response-default-answer]
#	ALIAS_EXPANDER	[mail-alias-expander]
#	CHSET_NAME	[charset]
#	CHSET_WIDTH	Charset width
#	DIST		distribution of cancelled article

CC=""

case "$1" in
COMPLETE)
  . $NNDIR/hold.param
  WORK="$2"
  EMPTY_CHECK=false
  cp $NNDIR/hold.work $WORK
  rm -f $NNDIR/hold.*
  ;;
*)
  . $NNDIR/.param
  OPERATION=$1
  ;;
esac

# first we handle 'cancel'

case "$OPERATION" in
cancel)
  TRACE="$2"
  CAN=${TRACE}can

  echo "Newsgroups: ${GROUP}" > $CAN
  echo "Subject: cmsg cancel $ART_ID" >> $CAN
  echo "Control: cancel $ART_ID" >> $CAN
  if [ $DIST != "none" ] ; then
    echo "Distribution: $DIST" >> $CAN
  fi
  echo "" >> $CAN
  echo "cancel $ART_ID in newsgroup $GROUP" >> $CAN
  echo "" >> $CAN
  echo "This article was cancelled from within NN version $VERSION" >> $CAN

  $INEWS -h < $CAN > $TRACE 2>&1

  x=$?
  case $x in
  0)
    ;;
  *)
    echo ''
    cat $TRACE
    sleep 2
    ;;
  esac
  rm -f $TRACE
  exit $x
  ;;
post|follow)
  LOOKFOR="Newsgroups:"
  SEND="post"
  SENT="posted"
  SENDPR="p)ost"
  MESSAGE="article"
  ;;
*)
  LOOKFOR="To:"
  SEND="send"
  SENT="sent"
  SENDPR="s)end"
  MESSAGE="letter"
  ;;
esac

TRACE=${WORK}T
FINAL=${WORK}F
TEMP=${WORK}M
COPY=""

case "$FIRST_ACTION" in
send)
  ;;
*)
  COPY=${WORK}C
  cp $WORK $COPY
  ;;
esac

# loop until sent or aborted.

loop=true
changed=true
prompt=false
addchset=false

pr="a)bort"
case "$POSTER_ADR" in
?*)
  pr="$pr c)c"
  ;;
esac
pr="$pr e)dit h)old"
case "$SPELL_CHECKER" in
?*)
  pr="$pr i)spell"
  ;;
esac
pr="$pr S)ign"
pr="$pr m)ail"
case "$OPERATION" in
post|follow)
  pr="$pr p)ost"
  ;;
esac
pr="$pr r)eedit"
case "$OPERATION" in
post|follow)
  ;;
*)
  pr="$pr s)end"
  ;;
esac
pr="$pr v)iew w)rite"

case "$DFLT_ANSW" in
  p*|s*)
    pr1=" ($SEND $MESSAGE)"
    ;;
  "")
    pr1=""
    ;;
  *)
    pr1=" ($DFLT_ANSW)"
    ;;
esac

while $loop ; do
  case $changed in
  true)
    case "$FIRST_ACTION" in
    edit)
      ;;
    *)
      # this works for v6/v7/bsd/sysv tr
      sed -e '1,/^$/d' $WORK | tr -d '[\1-\177]' > $TEMP
      if test -s $TEMP ; then
        pr2=" 7)bit"
        case "$CHSET_WIDTH" in
	8)
	  cansend=true
	  ;;
	*)
	  cansend=false
	  echo
          echo "Warning: body of $MESSAGE contains 8-bit characters"
          echo "You must delete them or select an 8-bit character set from nn."
          echo "In its current form the $MESSAGE cannot be $SENT."
	  ;;
	esac
	case "$CHSET_NAME" in
	""|unknown)
	  addchset=false
	  ;;
	*)
	  addchset=true
	  ;;
	esac
      else
        pr2=""
	cansend=true
	addchset=false
      fi
      rm -f $TEMP
      changed=false
      ;;
    esac
    ;;
  esac

  case "$FIRST_ACTION" in
  "")
    case $prompt in
    true)
      echo ''
      echo "$pr$pr2"
      $AWK 'END{printf "Action:'"$pr1"' "}' < /dev/null
      read act
      case "$act" in
      "")
        act="$DFLT_ANSW"
        ;;
      esac
      ;;
    esac
    ;;
  *)
    act="$FIRST_ACTION"
    FIRST_ACTION=""
    ;;
  esac
  prompt=true

  case "$act" in
  7*)
    # this works for v6/v7/bsd/sysv tr
    sed '1,/^$/d' $WORK | tr '[\201-\377]' '[\1-\177]' >> $TEMP
    mv $TEMP $WORK
    changed=true
    ;;

  a*)
    $AWK 'END{printf "Confirm abort: (y) "}' < /dev/null
    read act
    case "$act" in
    ""|y*)
      rm -f $WORK* $COPY
      exit 22
      ;;
    esac
    ;;

  c*)
    case "$POSTER_ADR" in
    "")
      echo "Not doing follow-up"
      ;;
    *)
      case $cansend in
      true)
        $AWK 'END{printf "CC: '"$POSTER_ADR"' (y) "}' </dev/null
        read act
        case "$act" in
        ""|"y*")
	  echo "Sending copy to poster...."
	  CC="$POSTER_ADR"
	  final=true
	  ;;
        esac
	;;
      false)
        echo
	echo "Error: body of $MESSAGE contains 8-bit characters"
	echo "You must delete them or select an 8-bit character set from nn."
	echo "In its current form the $MESSAGE cannot be mailed."
	;;
      esac
      ;;
    esac
    ;;

  e*)
    # call editor to enter at line $ED_LINE of work file

    case `basename "${EDITOR-vi}"` in
    vi|emacs|emacsclient|pico|joe )
      # Berkeley vi display editor
      # GNU emacs display editor
      # pico display editor
      # joe display editor
      eval ${EDITOR-vi} +$ED_LINE $WORK
      ;;
    ded )
      # QMC ded display editor
      $EDITOR -l$ED_LINE $WORK
      ;;
    uemacs )
      # micro emacs
      $EDITOR -g$ED_LINE $WORK
      ;;
    * )
      # Unknown editor
      $EDITOR $WORK
      ;;
    esac

    if test ! -s $WORK ; then
      rm -f $WORK* $COPY
      exit 22
    fi

    if $EMPTY_CHECK ; then
      if cmp -s $WORK $COPY ; then
         rm -f $WORK* $COPY
        exit 22
      fi
    fi

    changed=true

    case "$LOOKFOR" in
    ?*)
      if grep "^$LOOKFOR" $WORK > /dev/null ; then
	:
      else
	echo "Warning: no $LOOKFOR line in $MESSAGE"
      fi
      ;;
    esac
    ;;

  S*)
    sed -e '/^$/q' < $WORK > $WORK.HDRS
    awk '{if (S== 1) print $0; if ($0 == "") S=1}' < $WORK > $WORK.BDY
    pgp -stfaw < $WORK.BDY > $WORK.SGN
    cat $WORK.HDRS $WORK.SGN > $WORK
    rm -f $WORK.*
    ;;


  h*)
    $AWK 'END{printf "Complete response later: (y) "}' < /dev/null
    read act
    case "$act" in
    ""|y*)
      cp $WORK $NNDIR/hold.work
      cp $NNDIR/.param $NNDIR/hold.param
      echo "OPERATION=$OPERATION" >> $NNDIR/hold.param
      case "$COPY" in
      ?*)
        rm -f $COPY
        ;;
      esac
      rm -f $WORK*
      exit 22
      ;;
    esac
    ;;

  i*)
    case "$SPELL_CHECKER" in
    "")
      echo "spell-checker not defined"
      ;;
    *)
      $SPELL_CHECKER $WORK
      ;;
    esac
    ;;

  m*)
    case $cansend in
    true)
      $AWK 'END{printf "To: "}' </dev/null
      read CC
      case "$CC" in
      ?*)
        echo "Sending copy...."
        final=true
        ;;
      esac
      ;;
    false)
      echo
      echo "Error: body of $MESSAGE contains 8-bit characters"
      echo "You must delete them or select an 8-bit character set from nn."
      echo "In its current form the $MESSAGE cannot be mailed."
      ;;
    esac
    ;;

  n*)
    act="abort"
    prompt=false
    ;;

  r*)
    $AWK 'END{printf "Undo all changes? (n) "}' < /dev/null
    read act
    case "$act" in
    [yY]*)
      cp $COPY $WORK
      changed=true
      ;;
    esac
    FIRST_ACTION=edit
    ;;

  p*|s*)
    case $cansend in
    true)
      loop=false
      final=true
      ;;
    *)
      echo
      echo "Error: body of $MESSAGE contains 8-bit characters"
      echo "You must delete them or select an 8-bit character set from nn."
      echo "In its current form the $MESSAGE cannot be $SENT."
      ;;
    esac
    ;;

  v*)
    ${PAGER-cat} $WORK
    ;;

  w*)
    $AWK 'END{printf "Append '$MESSAGE' to file: "}' < /dev/null
    read FNAME
    case "$FNAME" in
    ?*)
      { cat $WORK ; echo ; } >> $FNAME
      ;;
    esac
    ;;

  y*)
    act="$DFLT_ANSW"
    prompt=false
    ;;

  ENV)
    set
    ;;
  esac

  case $final in
  true)
    cp $WORK $TEMP
    case $addchset in
    true)
      if grep -i -v "^MIME-Version: " $WORK > /dev/null &&
         grep -i -v "^Content-Type: " $WORK > /dev/null &&
         grep -i -v "^Content-Transfer-Encoding: " $WORK > /dev/null
      then
	echo '/^$/i' > ${TEMP}ed
	echo "MIME-Version: 1.0" >> ${TEMP}ed
	echo "Content-Type: text/plain; charset=$CHSET_NAME" >> ${TEMP}ed
	echo "Content-Transfer-Encoding: 8bit" >> ${TEMP}ed
	echo "." >> ${TEMP}ed
	echo "w" >> ${TEMP}ed
	echo "q" >> ${TEMP}ed
	ed -s $TEMP < ${TEMP}ed >/dev/null 2>&1
	rm -f ${TEMP}ed
      fi
      ;;
    esac
    grep -i -v "^Orig-To: " $TEMP | grep -i -v "^User-Agent: " > $FINAL
    rm -f $TEMP
    if $XNEWSREADER ; then
      echo '/^$/i' > ${TEMP}ed
      echo "User-Agent: nn/$VERSION" >> ${TEMP}ed
      echo "." >> ${TEMP}ed
      echo "w" >> ${TEMP}ed
      echo "q" >> ${TEMP}ed
      ed -s $FINAL < ${TEMP}ed >/dev/null 2>&1
      rm -f ${TEMP}ed
    fi
    final=false
    ;;
  esac
  
  case "$CC" in
  ?*)
    echo "To: $CC" > $TEMP
    sed -e "s/^To:/X-To:/" $FINAL >>$TEMP
    if $MAILER_PIPE ; then
      $MAILER < $TEMP
      x=$?
    else
      $MAILER $TEMP
      x=$?
    fi
    rm -f $TEMP
    case $x in
    0)
      echo Done
      ;;
    *)
      echo $MAILER failed
      ;;
    esac
    CC=""
    ;;
  esac
done

if test $APPEND_SIG = "true" -a -s $HOME/.signature ; then
  case $QUERY_SIG in
  true)
    $AWK 'END{printf "Append .signature? (y) "}' < /dev/null
    read ans
    ;;
  *)
    ans=y
    ;;
  esac
  case "$ans" in
  ""|y*|Y*)
    echo "-- " >> $FINAL
    cat $HOME/.signature >> $FINAL
    ;;
  esac
fi

case "$OPERATION" in
  follow|post)
    case $NOVICE in
    true)
      echo "Be patient! Your new $MESSAGE will not show up immediately."
      case "${WAIT_PERIOD-0}" in
	0|1)
          WAIT_PERIOD=2
          ;;
      esac
      ;;
    esac
    ;;
esac

{
  trap 'echo SIGNAL' 1 2 3

  LOGNAME="${LOGNAME-$USER}"
  case "$LOGNAME" in
  "")
    set `who am i`
    LOGNAME="$1"
    ;;
  esac

  case "$RECORD" in
  ?*)
    {
      # keep a copy of message in $RECORD (in mail format)
      set `date`
      case "$MMDF_SEP" in
      ?*)
        echo "$MMDF_SEP"
        ;;
      esac
      case $3 in
      [0-9])
        echo From $LOGNAME $1 $2 " $3" $4 $6 $7
        ;;
      *)
        echo From $LOGNAME $1 $2 $3 $4 $6 $7
        ;;
      esac
      echo "From: $LOGNAME"
      cat $FINAL
      echo "$MMDF_SEP"
    } >> "$RECORD"
    ;;
  esac

  case "$OPERATION" in

    reply|forward|mail)
      case "$ALIAS_EXPANDER" in
      ?*)
	$ALIAS_EXPANDER $FINAL
	;;
      esac
      case $MAILER_PIPE in
      true)
	$MAILER < $FINAL
	x=$?
	;;
      *)
	$MAILER $FINAL
	x=$?
	;;
      esac
      case $x in
      0)
	;;
      *)
	echo $MAILER failed
	;;
      esac
      ;;

    follow|post)
      {
	case $POST_PIPE in
	true)
          $POST < $FINAL 2>&1
          x=$?
          ;;
	*)
          $POST $FINAL 2>&1
          x=$?
          ;;
	esac
	case $x in
	0)
          ;;
	*)
          echo $INEWS failed
          ;;
	esac
      } | sed \
		-e "/spooled for later processing/d" \
		-e "/problem has been taken care of/d" \
		-e "/mailing your article to/d" \
		-e "/being mailed to/d" \
		-e "/mailed to the moderator/d" \
		-e "/is moderated/d"
      ;;

    *)
      echo "Invalid operation: $OPERATION -- help"
      OPERATION="nn response operation"
      ;;
  esac > $TRACE 2>&1

  if test -s $TRACE ; then
    if test -s $HOME/dead.letter ; then
      cat $HOME/dead.letter >> $HOME/dead.letters
      echo '' >> $HOME/dead.letters
    fi
    cat $WORK > $HOME/dead.letter

    # Gripe: Error-report is lost if REC_MAIL was the problem
    {
      echo "To: $LOGNAME"
      echo "Subject: $OPERATION failed"
      echo ""
      cat $TRACE
      echo ""
      echo "Your response has been saved in ~/dead.letter"
      echo ""
      echo "Your $MESSAGE follows:"
      cat $WORK
    } > $TEMP
    case $MAILER_PIPE in
    true)
      $MAILER < $TEMP
      ;;
    *)
      $MAILER $TEMP
      ;;
    esac
    rm -f $TEMP
  else
    # keep TRACE file a little while for test at end of script
    sleep 3
  fi
  rm -f $WORK* $COPY

} > /dev/null 2>&1 &

# if in synchronous mode, then wait untill it's done.  Most useful for INN.
if $SYNCHRO ; then
  wait
else
  case "${WAIT_PERIOD-0}" in
  0)
    ;;
  *)
    sleep $WAIT_PERIOD
    ;;
  esac
fi

if test -s "$TRACE" ; then
  exit 1
fi

exit 0
