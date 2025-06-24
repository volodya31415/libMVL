# Makefile.am - src level automakefile

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CFLAGS = ${CFLAGS}

LIBTOOL_DEPS = @LIBTOOL_DEPS@

lib_LTLIBRARIES = libMVL.la

include_HEADERS = libMVL.h

libMVL_la_LIBADD = ${LDFLAGS}
libMVL_la_LDFLAGS = -no-undefined -version-info ${LIBMVL_VERSION}
libMVL_la_SOURCES = libMVL.c libMVL_sort.cc pdqsort.h pdqidxsort.h

EXTRA_DIST = MVL-config.h.in
nodist_EXTRA_DATA = .deps .libs MVL-config.h
