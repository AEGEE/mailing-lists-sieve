#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.68)
AC_INIT(mailing-lists-sieve, 1.0.3, mailing-lists-sieve@aegee.org)
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2 subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/mailing-lists-sieve.c])
AC_CONFIG_MACRO_DIR([m4])
# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_LIBTOOL_DLOPEN
LT_INIT([disable-shared, dlopen])
AC_CHECK_HEADERS_ONCE(signal.h, errno.h, sys/time.h, sys/stat.h, sys/types.h, ltdl.h)

# Checks for libraries.
AC_CHECK_LIB([ltdl], [lt_dlsym])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
PKG_PROG_PKG_CONFIG(0.25)
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.28.1], [check_glib=yes], [check_glib=no])
PKG_CHECK_MODULES([liblistserv], [liblistserv >= 1.0.1], [liblistserv=yes], [liblistserv=no])
PKG_CHECK_MODULES([gsasl], [libgsasl >= 1.6.0], [gsasl=yes], [gsasl=no])
AC_FUNC_MALLOC
AC_HEADER_STDC
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
