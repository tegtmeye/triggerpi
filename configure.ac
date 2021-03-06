AC_PREREQ(2.59)
AC_INIT([triggerpi],[0.0.0],[michael.b.tegtmeyer.civ@mail.mil])
AC_CONFIG_SRCDIR([Makefile.am])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign tar-pax])
AC_CONFIG_MACRO_DIR([m4])

# early checks
AC_C_BIGENDIAN(
	[AC_DEFINE([WORDS_BIGENDIAN],[1],"Big endian architecture")],
	[AC_DEFINE([WORDS_BIGENDIAN],[0],"Little endian architecture")],
	[AC_MSG_ERROR([Unknown architecture endian])])

# Checks for programs.

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Check for C++11 support
AX_CXX_COMPILE_STDCXX([11],[noext],[mandatory])

# Configure libtool
AC_PROG_LIBTOOL

# Check for libraries
AX_LIB_BCM2835([1.5])

# We need boost
BOOST_REQUIRE([1.5])
BOOST_FILESYSTEM
BOOST_SYSTEM
BOOST_REGEX
BOOST_TEST
BOOST_PROGRAM_OPTIONS



# Check for single testsuite
AC_ARG_WITH([single-testsuite],
[AS_HELP_STRING([--with-single-testsuite],
        [single testsuite with XML output @<:@default=no@:>@])],
[case "${withval}" in
  yes) ;;
  no) ;;
  *) AC_MSG_ERROR([bad value ${withval} for --with-single-testsuite]) ;;
esac],[with_single_testsuite=no])
AM_CONDITIONAL([SINGLE_TESTSUITE], [test "x$with_single_testsuite" == xyes])


AC_CONFIG_FILES( \
		 Makefile \
		 src/Makefile \
		 src/triggerpi/Makefile)

AC_OUTPUT
