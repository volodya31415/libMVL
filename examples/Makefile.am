# Makefile.am - examples level automakefile

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

AM_CFLAGS = ${CFLAGS} -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src

LDADDS = $(top_builddir)/src/.libs/libMVL.la

EXTRA_DIST = example1.dox example2.dox example3.dox

# The tests
noinst_PROGRAMS = example1 example3 example4 example5

example1_SOURCES = example1.c
example1_LDADD = $(LDADDS)

EXTRA_DIST += example2.R


example3_SOURCES = example3.c
example3_LDADD = $(LDADDS)

example4_SOURCES = example4.c
example4_LDADD = $(LDADDS)

example5_SOURCES = example5.c
example5_LDADD = $(LDADDS)

TESTS = $(noinst_PROGRAMS)

nodist_EXTRA_DATA = $(noinst_PROGRAMS) *.log *.trs *.mvl .deps .libs

clean-local:
	$(RM) -fR $(nodist_EXTRA_DATA)
