# -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.
dnl NOTE that running autoconf, automake will replace the existing
dnl Makefile(s).

# Copyright (C) 2025 by Joe Da Silva

AC_PREREQ([2.69])
#-------------------------------------------
# Making point releases:
#   MVL_major_version += 0;
#   MVL_minor_version += 1; (patches or added function(s))
#
# If any new functions have been added:
#   MVL_major_version += 0;
#   MVL_minor_version += 1; (added function(s))
#
# If backwards compatibility has been broken:
#   MVL_major_version += 1;
#   MVL_minor_version = 0;
#
m4_define([MVL_major_version], [0])
m4_define([MVL_minor_version], [0])
m4_define([MVL_version],[MVL_major_version.MVL_minor_version])

#-------------------------------------------
# Updating the libtool version should be set
# independently of the library's major/minor
# version, execute the following for C:R:A
#
# 1. If the code has changed at all (e.g., an update):
#   MVL_revision += 1
#
# 2. If any interfaces are added, removed, or changed
#    (e.g., a new function has been added):
#   MVL_current += 1
#   MVL_revision = 0
#
# 3. If any interfaces have been added:
#   MVL_age += 1
#
# 4. If any interfaces have been removed or changed
#    (i.e., backwards compatibility has been broken):
#  MVL_age = 0
#
m4_define([MVL_current], [1])
m4_define([MVL_revision],[1])
m4_define([MVL_age],     [0])
m4_define([MVL_libver],[MVL_current:MVL_revision:MVL_age])

m4_define([MVL_package_name],  [libMVL])
m4_define([MVL_package_stamp], [MVL_version])
m4_define([MVL_package_site],  [https://github.com/volodya31415/libMVL])
m4_define([MVL_package_mail],  [https://github.com/volodya31415/libMVL/issues])

AC_INIT([MVL_package_name],[MVL_package_stamp],[MVL_package_mail],
	[MVL_package_name],[MVL_package_site])

#-------------------------------------------
AC_CONFIG_SRCDIR([src/libMVL.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AM_INIT_AUTOMAKE([foreign -Wall])

#-------------------------------------------
# automake 1.12 seems to require AM_PROG_AR,
# but automake 1.11 doesn't recognize it.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT
AC_SUBST([LIBTOOL_DEPS])

# Check building environment
AC_PROG_CC
AC_PROG_CXX
AC_ENABLE_SHARED
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_PROG_MAKE_SET
AC_PROG_SED
AC_PATH_PROG([STRIP],[strip],[:])
AC_PATH_PROG([PKG_CONFIG],[pkg-config],[:])
AC_CONFIG_HEADERS([src/MVL-config.h])
AC_PROG_INSTALL

#-------------------------------------------
# Indicate this is a release build and that
# dependancies for changes between Makefile.am
# and Makefile.in should not be checked. This
# makes compiling faster. If you are working
# on the library, run:
# ./configure --enable-maintainer-mode
# to enable the dependancies
dnl AM_MAINTAINER_MODE([enable])

#-------------------------------------------
# Enable silent build rules by default, this
# requires atleast Automake-1.11. Disable by
# either passing --disable-silent-rules to
# configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],[AC_SUBST([AM_DEFAULT_VERBOSITY],[1])])

#-------------------------------------------
# Check for math.h include and math library.
have_libm=no
AC_CHECK_HEADER([math.h],
  AC_SEARCH_LIBS([cos],[m],[have_libm=yes]))
if test x"${have_libm}" != xyes; then
   AC_MSG_FAILURE([ERROR: Please install Math libraries and math.h include files for libm],[1])
fi

#-------------------------------------------
# Default using threading if it's available.
# We need header and lib function locations.
have_pthreads=no
have_pthreads64=no
AC_CHECK_HEADER([pthread.h],
  AC_CHECK_FUNC([posix_fallocate64],[have_pthreads64=yes],
    AC_CHECK_FUNC([posix_fallocate],[have_pthreads=yes])))
dnl make this info available in config file.
if test x"${have_pthreads64}" = xyes; then
  AC_DEFINE(HAVE_PTHREADS64,[1],[Have pthreads.h available.])
  AC_SEARCH_LIBS([posix_fallocate64],[c])
  have_pthreads=yes
fi
if test x"${have_pthreads}" = xyes; then
  AC_DEFINE(HAVE_PTHREADS,[1],[Have pthreads.h available.])
  AC_SEARCH_LIBS([posix_fallocate],[c])
fi
AM_CONDITIONAL([HAVEPTHREADS],[test x$have_pthreads = xyes])

#-------------------------------------------
# auto-run doxygen if it's available or else
# enable/disable build and install manually.
run_doxygen=no
AC_ARG_ENABLE([docs],
  AS_HELP_STRING([--enable-docs],[Build and install libMVL documentation [default=auto]]),
    [case "${enableval}" in
     true)  ac_enable_docs=yes ;;
     false) ac_enable_docs=no  ;;
     yes)   ac_enable_docs=yes ;;
     no)    ac_enable_docs=no  ;;
     *) AC_MSG_ERROR([ERROR: Bad value ${enableval} for --enable-docs]) ;;
     esac],[ac_enable_docs=auto])
if test "x$ac_enable_docs" != "xno"; then
  AC_PATH_PROG([DOXYGEN],[doxygen],[:])
  if test "x$DOXYGEN" = "x:" -a "x$ac_enable_docs" = "xyes"; then
    AC_MSG_ERROR([ERROR: Documentation requires Doxygen. Please install Doxygen and try again.])
  else
    run_doxygen=yes
    AC_MSG_NOTICE([libMVL Documentation will be generated and installed.])
  fi
else
  run_doxygen=no
  AC_MSG_NOTICE([libMVL documentation will not be generated or installed.])
fi
AM_CONDITIONAL([HAVE_DOXYGEN],[test "x$run_doxygen" = "xyes"])

#-------------------------------------------
# Pass some MVL.c definitions
AC_DEFINE([MVL_VERSION_MJ],[MVL_major_version],[package version major value])
AC_DEFINE([MVL_VERSION_MN],[MVL_minor_version],[package version minor value])

#-------------------------------------------
# Put ifdef wrapper around MVL-config.h so
# that we don't accidently call it twice.
AH_TOP([#ifndef _MVL_CONFIG_H
#define _MVL_CONFIG_H 1])
AH_BOTTOM([#endif])

#-------------------------------------------
# Pass variables to MAKEFILE.AM
AC_SUBST([LIBMVL_VERSION],[MVL_libver])

#-------------------------------------------

AC_CONFIG_FILES([
Makefile
src/Makefile
docs/Makefile
examples/Makefile
libMVL.pc
])

AC_OUTPUT

AC_MSG_NOTICE([

Configuration:
  Source code location	${srcdir}
  Build code location	${builddir}
  Destination prefix	${prefix}
  Library Destination	${libdir}
  Make and install docs	${run_doxygen}
  Have Posix threads	${have_pthreads}
  Compilers		${CC} ${CXX}
  Config CFLAGS		"${CFLAGS}"
  Config LDFLAGS	"${LDFLAGS}"

])
