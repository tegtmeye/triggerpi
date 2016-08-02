# ===========================================================================
#    Modeled from ax_lib_BCM2835.m4 from
#     http://www.gnu.org/software/autoconf-archive/ax_lib_BCM2835.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_BCM2835([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the BCM2835 library of a particular version (or newer)
#
#   This macro takes only one optional argument, required version of BCM2835
#   library. If required version is not passed, 1.5 is used in the test
#   of existance of BCM2835.
#
#   If no intallation prefix to the installed BCM2835 library is given the
#   macro searches under /usr, /usr/local, and /opt.
#
#   This macro calls:
#
#     AC_SUBST(BCM2835_CFLAGS)
#     AC_SUBST(BCM2835_LDFLAGS)
#     AC_SUBST(BCM2835_VERSION)
#
#   And sets:
#
#     HAVE_BCM2835
#
# LICENSE
#
#   Copyright (c) 2015 Michael Tegtmeyer <tegtmeye@gmail.com>
#   Modified from original code
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

AC_DEFUN([AX_LIB_BCM2835],
[
    AC_ARG_WITH([BCM2835],
        AS_HELP_STRING(
            [--with-BCM2835=@<:@ARG@:>@],
            [use BCM2835 library @<:@default=yes@:>@, optionally specify the prefix for BCM2835 library]
        ),
        [
        if test "$withval" = "no"; then
            WANT_BCM2835="no"
        elif test "$withval" = "yes"; then
            WANT_BCM2835="yes"
            ac_BCM2835_path=""
        else
            WANT_BCM2835="yes"
            ac_BCM2835_path="$withval"
        fi
        ],
        [WANT_BCM2835="yes"]
    )

    BCM2835_CFLAGS=""
    BCM2835_LDFLAGS=""
    BCM2835_VERSION=""

    if test "x$WANT_BCM2835" = "xyes"; then

        ac_BCM2835_header="bcm2835.h"

        BCM2835_version_req=ifelse([$1], [], [1.5.0], [$1])
        BCM2835_version_req_shorten=`expr $BCM2835_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
        BCM2835_version_req_major=`expr $BCM2835_version_req : '\([[0-9]]*\)'`
        BCM2835_version_req_minor=`expr $BCM2835_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
        BCM2835_version_req_micro=`expr $BCM2835_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
        if test "x$BCM2835_version_req_micro" = "x" ; then
            BCM2835_version_req_micro="0"
        fi

        BCM2835_version_req_number=`expr $BCM2835_version_req_major \* 1000000 \
                                   \+ $BCM2835_version_req_minor \* 1000 \
                                   \+ $BCM2835_version_req_micro`

        AC_MSG_CHECKING([for BCM2835 library >= $BCM2835_version_req])

        if test "$ac_BCM2835_path" != ""; then
            ac_BCM2835_ldflags="-L$ac_BCM2835_path/lib"
            ac_BCM2835_cppflags="-I$ac_BCM2835_path/include"
        else
            for ac_BCM2835_path_tmp in /usr /usr/local /opt ; do
                if test -f "$ac_BCM2835_path_tmp/include/$ac_BCM2835_header" \
                    && test -r "$ac_BCM2835_path_tmp/include/$ac_BCM2835_header"; then
                    ac_BCM2835_path=$ac_BCM2835_path_tmp
                    ac_BCM2835_cppflags="-I$ac_BCM2835_path_tmp/include"
                    ac_BCM2835_ldflags="-L$ac_BCM2835_path_tmp/lib"
                    break;
                fi
            done
        fi

        ac_BCM2835_ldflags="$ac_BCM2835_ldflags -lbcm2835"

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $ac_BCM2835_cppflags"

        AC_LANG_PUSH(C)
        AC_COMPILE_IFELSE(
            [
            AC_LANG_PROGRAM([[@%:@include <bcm2835.h>]],
                [[
#if (BCM2835_VERSION*100 >= $BCM2835_version_req_number)
/* Everything is okay */
#else
#  error BCM2835 library version is too old
#endif
                ]]
            )
            ],
            [
            AC_MSG_RESULT([yes])
            success="yes"
            ],
            [
            AC_MSG_RESULT([not found])
            errmsg="cannot find BCM2835 headers version >= $BCM2835_version_req_number"
            AC_MSG_ERROR([$errmsg])
            success="no"
            ]
        )
        AC_LANG_POP(C)

        CPPFLAGS="$saved_CPPFLAGS"

        if test "$success" = "yes"; then

            BCM2835_CFLAGS="$ac_BCM2835_cppflags"
            BCM2835_LDFLAGS="$ac_BCM2835_ldflags"

            AC_SUBST(BCM2835_CFLAGS)
            AC_SUBST(BCM2835_LDFLAGS)
            AC_DEFINE([HAVE_BCM2835], [], [Have the BCM2835 library])
        fi
    fi
])
