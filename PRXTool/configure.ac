#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([prxtool], [PRXTOOL_VERSION], [])
AC_CONFIG_SRCDIR([main.cpp])
AC_CONFIG_HEADER([config.h])
AC_PRXTOOL_VERSION

AM_INIT_AUTOMAKE([prxtool], [PRXTOOL_VERSION])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h unistd.h])
AX_CREATE_STDINT_H

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])
AC_C_BIGENDIAN

# Checks for library functions.
AC_CHECK_FUNCS([memset strchr strtoul])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
