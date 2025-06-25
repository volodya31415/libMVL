# Makefile.am - Top level automakefile

SUBDIRS = . src docs examples

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libMVL.pc

man_MANS = $(top_srcdir)/docs/libMVL.3

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

test_programs = example1 example3 example4 example5

DISTCLEANFILES = libMVL.pc
DISTCHECK_CONFIGURE_FLAGS = --disable-silent-rules

EXTRA_DIST = README.md $(man_MANS)
nodist_EXTRA_DATA = .git .github

