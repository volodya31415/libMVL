# Makefile.am - docs level automakefile

if HAVE_DOXYGEN
pkgdocdir=$(datadir)/doc/libMVL
examplesdocdir=$(pkgdocdir)/examples
htmldocdir=$(pkgdocdir)/html
latexdocdir=$(pkgdocdir)/latex

all-local:
	$(DOXYGEN) $(top_srcdir)/docs/Doxyfile

install-data-local:
	$(INSTALL) -d $(DESTDIR)$(examplesdocdir)
	$(INSTALL) -d $(DESTDIR)$(htmldocdir)
	$(INSTALL) -d $(DESTDIR)$(htmldocdir)/search
	$(INSTALL) -d $(DESTDIR)$(latexdocdir)
	$(INSTALL_DATA) -p $(top_srcdir)/COPYING $(DESTDIR)$(pkgdocdir)
	$(INSTALL_DATA) -p $(top_srcdir)/README.md $(DESTDIR)$(pkgdocdir)
	$(INSTALL_DATA) -p -t $(DESTDIR)$(examplesdocdir) $(top_srcdir)/examples/*.c $(top_srcdir)/examples/*.R
	mv $(builddir)/html/search $(builddir)/
	$(INSTALL_DATA) -t $(DESTDIR)$(htmldocdir) $(builddir)/html/*
	$(INSTALL_DATA) -t $(DESTDIR)$(htmldocdir)/search $(builddir)/search/*
	mv $(builddir)/search $(builddir)/html/
	$(INSTALL_DATA) -t $(DESTDIR)$(latexdocdir) $(builddir)/latex/*

uninstall-local:
	$(RM) -fR $(DESTDIR)$(pkgdocdir)

clean-local:
	$(RM) -fR html latex
endif

EXTRA_DIST = _config.yml Doxyfile DoxygenLayout.xml index.md
