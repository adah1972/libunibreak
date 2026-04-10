LIBUNIBREAK
===========

Overview
--------

This is the README file for libunibreak, an implementation of the line
breaking and word/grapheme breaking algorithms as described in [Unicode
Standard Annex 14][1] (UAX #14) and [Unicode Standard Annex 29][2] (UAX
#29).  Check the project's [home page][3] for up-to-date information.

Unicode 15.0 support for line breaking, as well as full Unicode 15.1
support for word/grapheme breaking, is currently provided.  Full Unicode
15.1 support for line breaking has not been implemented, mostly because
some rules (like LB28a of UAX #14-51) are incompatible with the
incremental line-breaking API (`lb_process_next_char`, introduced in
libunibreak 1.1).  The current version uses deferred fixups to
accommodate newer rules, with the incremental API falling back to
conservative behaviour.  This approach is complex, and the incremental
API may be removed in a future release.

  [1]: http://www.unicode.org/reports/tr14/
  [2]: http://www.unicode.org/reports/tr29/
  [3]: https://github.com/adah1972/libunibreak


Licence
-------

This library is released under an open-source licence, the zlib/libpng
licence.  Please check the file *LICENCE* for details.

Apart from using the algorithm, part of the code is derived from the
[Unicode Public Data][4], and the [Unicode Terms of Use][5] may apply.

  [4]: http://www.unicode.org/Public/
  [5]: http://www.unicode.org/copyright.html


Installation
------------

There are three ways to build the library:

1. On \*NIX systems supported by the autoconfiscation tools, do the
   normal

        ./configure
        make
        sudo make install

   to build and install both the dynamic and static libraries.  In
   addition, one may
   - type `make doc` to generate the doxygen documentation; or
   - type `make check` to run the self-check tests.

2. On systems where GCC and Binutils are supported, one can type

        cd src
        cp -p Makefile.gcc Makefile
        make

   to build the static library.  In addition, one may
   - type `make debug` or `make release` to explicitly generate the
     debug or release build; or
   - type `make doc` to generate the doxygen documentation.

3. On Windows, apart from using method 1 (Cygwin/MSYS) and method 2
   (MinGW), MSVC can also be used.  Type

        cd src
        nmake -f Makefile.msvc

   to build the static library.  By default the debug version is built.
   To build the release version

        nmake -f Makefile.msvc CFG="libunibreak - Win32 Release"


Documentation
-------------

Check the generated document *doc/html/linebreak\_8h.html*,
*doc/html/wordbreak\_8h.html*, and *doc/html/graphemebreak\_8h.html* in
the downloaded file for the public interfaces exposed to applications.


Examples
--------

See the *tools* directory for basic examples.  A more *real* utility is
[breaktext][6].

[6]: https://github.com/adah1972/breaktext


<!--
vim:autoindent:expandtab:formatoptions=tcqlmn:textwidth=72:
-->
