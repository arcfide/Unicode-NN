BEGIN {
	linebuf = indent = ""
	curcol = indcol = 0
	maxcol = 78
	progname = ""
	firstsh = 1
	numcol = 0
	spacing = 1
	wordspace = " "
	tab = sprintf("%c",9)
}

/^\.SH / {
	if (firstsh == 0) printf("%s\n\n", linebuf)
	firstsh = 0

	printf("From: %s\nSubject:", progname);
	if ($2 == "NAME") {
		getline
		for (i = 2; i <= NF; i++) printf(" %s", $i);
		printf("\n\n")
		print
	} else {
		for (i = 2; i <= NF; i++) printf(" %s", $i);
		printf("\n\n")
	}

	linebuf = indent = ""
	curcol = indcol = 0
	next
}

/^\.TH / {
	progname = $2
	next
}

/^\.UC / {
	next
}

/^\.(br|sp)/ {
	if (linebuf != indent) {
		printf("%s\n", linebuf)
	}
	linebuf = indent
	curcol = indcol
	next
}

/^\.PP/ {
	if (linebuf != indent) printf("%s\n", linebuf)

	printf("\n")

	linebuf = "   " ; curcol = 3
	indent = "" ; indcol = 0
	next
}

/^\.LP/ {
	if (linebuf != indent) printf("%s\n", linebuf)

	printf("\n")

	linebuf = indent = ""
	curcol = indcol = 0
	next
}

/^\.TP/ {
	if (linebuf != indent) printf("%s\n", linebuf)

	printf("\n")

	getline; linebuf = $0
	indent = "     "
	curcol = indcol = 5
	if (length(linebuf) >= 5) {
		printf("%s\n", linebuf)
		linebuf = indent
	} else {
		while (length(linebuf) < 4) linebuf = linebuf " "
	}
	next
}

/^\.\\"ta/ {
	for (numcol = 2; numcol <= NF; numcol++) tabcol[numcol-1] = $numcol
	numcol = NF
	next
}

/^\.DT/ {
	numcol = 0
	next
}

numcol != 0 {
	j = length($0)
	k = 0
	g = 1
	for (i = 1; i<=j; i++) {
		while (k < tabcol[g]) {
			printf(" ")
			k++
		}
		c = substr($0,i,1)
		if (c == tab) {
			g++
		} else {
			printf("%s", c)
			k++
		}
	}
	printf("\n")
	next
}

/^[ 	]/ {
	if (linebuf != indent) printf("%s\n",linebuf)
	linebuf = indent "     "
	curcol = indcol+5
}

{
	word = 1
	wordspace = " "
	spacing = 1
}

/^\.[IB] / {
	word = 2
}

/^\.[IB]R / {
	wordspace = ""
	word = 2
	spacing = 0
}

{
 	sep = " "
	if (linebuf == indent) sep = ""

	while (word <= NF) {
		k = length($word)
		if ((curcol + k) > maxcol) {
			printf("%s\n", linebuf)
			linebuf = indent
			curcol = indcol
			sep = ""
		}
		linebuf = linebuf sep $word
		sep = wordspace
		curcol += spacing + k
		word++
	}
}

END {
	if (linebuf != indent) printf("%s\n\n", linebuf)
}
