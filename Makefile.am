SUBDIRS = src

pkgconfig_DATA  = libunibreak.pc
pkgconfigdir    = ${libdir}/pkgconfig

EXTRA_DIST = \
	Doxyfile \
	LICENCE \
	doc

doc:
	cd ${srcdir} && doxygen

linebreakdata:
	cd src && ${MAKE} linebreakdata

wordbreakdata:
	cd src && ${MAKE} wordbreakdata
