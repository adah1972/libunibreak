#noinst_PROGRAMS = filter_dup
include_HEADERS = linebreak.h linebreakdef.h wordbreak.h wordbreakdef.h
lib_LTLIBRARIES = libunibreak.la

libunibreak_la_LDFLAGS = -no-undefined -version-info 1:0
libunibreak_la_SOURCES = \
	linebreak.c \
	linebreakdata.c \
	linebreakdef.c \
	wordbreak.c

EXTRA_DIST = \
	LineBreak1.sed \
	LineBreak2.sed \
	linebreakdata1.tmpl \
	linebreakdata2.tmpl \
	linebreakdata3.tmpl \
	wordbreakdata1.tmpl \
	wordbreakdata2.tmpl \
	wordbreakdata.c \
	LICENCE \
	Doxyfile \
	Makefile.gcc \
	Makefile.msvc \
	doc \
	sort_numeric_hex.py

install-exec-hook:
	rm -f ${libdir}/liblinebreak.a
	${LN_S} ${libdir}/libunibreak.a ${libdir}/liblinebreak.a

distclean-local:
	rm -f LineBreak.txt WordBreakData.txt filter_dup${EXEEXT}

doc:
	cd ${srcdir} && doxygen

LineBreak.txt:
	wget http://unicode.org/Public/UNIDATA/LineBreak.txt

WordBreakProperty.txt:
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt

linebreakdata: ${builddir}/filter_dup LineBreak.txt
	sed -n -f ${srcdir}/LineBreak1.sed LineBreak.txt > tmp.txt
	sed -f ${srcdir}/LineBreak2.sed tmp.txt | ${builddir}/filter_dup > tmp.c
	head -2 LineBreak.txt > tmp.txt
	cat ${srcdir}/linebreakdata1.tmpl tmp.txt ${srcdir}/linebreakdata2.tmpl tmp.c ${srcdir}/linebreakdata3.tmpl > ${srcdir}/linebreakdata.c
	rm tmp.txt tmp.c

wordbreakdata: WordBreakProperty.txt
	sed -E -n 's/(^[0-9A-F.]+)/\1/p' WordBreakProperty.txt > tmp2.txt
	sed -E -i.bak 's/^([0-9A-F]+) +/\1..\1/' tmp2.txt
	${srcdir}/sort_numeric_hex.py tmp2.txt > tmp.txt
	rm tmp2.txt tmp2.txt.bak
	sed -E -i.bak -n 's/^([0-9A-F]+)..([0-9A-F]+) *; *([A-Za-z]+).*/'$$'\t''{0x\1, 0x\2, WBP_\3},/p' tmp.txt 
	echo "/* The content of this file is generated from:" > ${srcdir}/wordbreakdata.c
	head -2 WordBreakProperty.txt >> ${srcdir}/wordbreakdata.c
	echo "*/" >> ${srcdir}/wordbreakdata.c
	cat ${srcdir}/wordbreakdata1.tmpl tmp.txt ${srcdir}/wordbreakdata2.tmpl >> ${srcdir}/wordbreakdata.c
	rm tmp.txt tmp.txt.bak
