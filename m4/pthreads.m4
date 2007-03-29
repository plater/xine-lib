dnl Detection of the Pthread implementation flags and libraries
dnl Diego Pettenò <flameeyes-aBrp7R+bbdUdnm+yROfE0A@public.gmane.org> 2006-11-03
dnl
dnl CC_PTHREAD_FLAGS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl This macro checks for the Pthread flags to use to build
dnl with support for PTHREAD_LIBS and PTHREAD_CFLAGS variables
dnl used in FreeBSD ports.
dnl
dnl This macro is released as public domain, but please mail
dnl to flameeyes@gmail.com if you want to add support for a
dnl new case, or if you're going to use it, so that there will
dnl always be a version available.
AC_DEFUN([CC_PTHREAD_FLAGS], [
  AC_REQUIRE([CC_CHECK_WERROR])
  AC_ARG_VAR([PTHREAD_CFLAGS], [C compiler flags for Pthread support])
  AC_ARG_VAR([PTHREAD_LIBS], [linker flags for Pthread support])

  dnl if PTHREAD_* are not set, default to -pthread (GCC)
  if test "${PTHREAD_CFLAGS-unset}" = "unset"; then
     case $host in
       *-hpux11*) PTHREAD_CFLAGS=""		;;
       *-darwin*) PTHREAD_CFLAGS=""		;;
       *)	  PTHREAD_CFLAGS="-pthread"	;;
     esac
  fi
  if test "${PTHREAD_LIBS-unset}" = "unset"; then
     case $host in
       *-hpux11*) PTHREAD_LIBS="-lpthread"	;;
       *-darwin*) PTHREAD_LIBS=""		;;
       *)	  PTHREAD_LIBS="-pthread"	;;
     esac
  fi

  AC_CACHE_CHECK([if $CC supports Pthread],
    AS_TR_SH([cc_cv_pthreads]),
    [ac_save_CFLAGS="$CFLAGS"
     ac_save_LIBS="$LIBS"
     CFLAGS="$CFLAGS $cc_cv_werror $PTHREAD_CFLAGS"
     LIBS="$LIBS $PTHREAD_LIBS"
     AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <pthread.h>]],
          [[pthread_create(NULL, NULL, NULL, NULL);]]
        )],
       [cc_cv_pthreads=yes],
       [cc_cv_pthreads=no])
     CFLAGS="$ac_save_CFLAGS"
     LIBS="$ac_save_LIBS"
    ])

  AC_SUBST([PTHREAD_LIBS])
  AC_SUBST([PTHREAD_CFLAGS])

  if test x$cc_cv_pthreads = xyes; then
    ifelse([$1], , [:], [$1])
  else
    ifelse([$2], , [:], [$2])
  fi
])