#noinst_PROGRAMS = filter_dup
include_HEADERS = linebreak.h linebreakdef.h wordbreak.h wordbreakdef.h
lib_LTLIBRARIES = libunibreak.la
pkgconfig_DATA  = libunibreak.pc
pkgconfigdir    = ${libdir}/pkgconfig

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
	cd ${top_srcdir} && doxygen

LineBreak.txt:
	wget http://unicode.org/Public/UNIDATA/LineBreak.txt

WordBreakProperty.txt:
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt

linebreakdata: ${builddir}/filter_dup LineBreak.txt
	sed -n -f ${top_srcdir}/LineBreak1.sed LineBreak.txt > tmp.txt
	sed -f ${top_srcdir}/LineBreak2.sed tmp.txt | ${builddir}/filter_dup > tmp.c
	head -2 LineBreak.txt > tmp.txt
	cat ${top_srcdir}/linebreakdata1.tmpl tmp.txt ${top_srcdir}/linebreakdata2.tmpl tmp.c ${top_srcdir}/linebreakdata3.tmpl > ${top_srcdir}/linebreakdata.c
	rm tmp.txt tmp.c

wordbreakdata: WordBreakProperty.txt
	sed -E -n 's/(^[0-9A-F.]+)/\1/p' WordBreakProperty.txt > tmp2.txt
	sed -E -i.bak 's/^([0-9A-F]+) +/\1..\1/' tmp2.txt
	${top_srcdir}/sort_numeric_hex.py tmp2.txt > tmp.txt
	rm tmp2.txt tmp2.txt.bak
	sed -E -i.bak -n 's/^([0-9A-F]+)..([0-9A-F]+) *; *([A-Za-z]+).*/'$$'\t''{0x\1, 0x\2, WBP_\3},/p' tmp.txt 
	echo "/* The content of this file is generated from:" > ${top_srcdir}/wordbreakdata.c
	head -2 WordBreakProperty.txt >> ${top_srcdir}/wordbreakdata.c
	echo "*/" >> ${top_srcdir}/wordbreakdata.c
	cat ${top_srcdir}/wordbreakdata1.tmpl tmp.txt ${top_srcdir}/wordbreakdata2.tmpl >> ${top_srcdir}/wordbreakdata.c
	rm tmp.txt tmp.txt.bak
