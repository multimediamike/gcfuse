AC_INIT([gcfuse], [0.9.1], [mike@multimedia.cx])
AM_INIT_AUTOMAKE([1.11 foreign])
AC_CONFIG_HEADER([src/config.h])

AC_PROG_CC

CPPFLAGS="$CPPFLAGS -Wall `getconf LFS_CFLAGS`"
LDFLAGS="$LDFLAGS `getconf LFS_LDFLAGS`"

PKG_CHECK_MODULES([FUSE], [fuse >= 2.5])

AC_C_CONST

AC_FUNC_MALLOC
AC_FUNC_STAT

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
