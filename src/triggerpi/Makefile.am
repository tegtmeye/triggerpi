# -Wno-unused-local-typedefs is used to suppress numerous BOOST_STATIC_ASSERT
# warnings on modern compilers
additional_cppflags= \
        -pedantic -Wall -Wno-unused-local-typedefs \
        -DTRIGGERPI_SITE_CONFIGDIR='"$(pkgdatadir)"' \
        -I$(top_srcdir) \
        $(BOOST_CPPFLAGS)

additional_libs= \
        $(BOOST_REGEX_LIBS) \
        $(BOOST_SYSTEM_LIBS) \
        $(BOOST_FILESYSTEM_LIBS) \
        $(BOOST_PROGRAM_OPTIONS_LIBS)

additional_ldflags= \
	-lbcm2835 \
        $(BOOST_LDFLAGS) \
        $(BOOST_REGEX_LDFLAGS) \
        $(BOOST_SYSTEM_LDFLAGS) \
        $(BOOST_FILESYSTEM_LDFLAGS) \
        $(BOOST_PROGRAM_OPTIONS_LDFLAGS)

bin_PROGRAMS= \
        triggerpi

triggerpi_SOURCES= \
	bcm2835_sentry.h \
	ADC_board.h \
	waveshare_ADS1256_expansion.h \
	waveshare_ADS1256_expansion.cc \
        main.cc \
	board_init.cc

triggerpi_CPPFLAGS=$(additional_cppflags)
triggerpi_LDADD=$(additional_libs)
triggerpi_LDFLAGS=$(additional_ldflags)


triggerpi_configdir=$(pkgdatadir)
dist_triggerpi_config_DATA = \
        triggerpi_config
