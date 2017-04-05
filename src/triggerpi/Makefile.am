# -Wno-unused-local-typedefs is used to suppress numerous BOOST_STATIC_ASSERT
# warnings on modern compilers
additional_cppflags= \
        -pedantic -Wall -Wno-unused-local-typedefs \
        -DTRIGGERPI_SITE_CONFIGDIR='"$(pkgdatadir)"' \
        -I$(top_srcdir) \
        $(BOOST_CPPFLAGS)

additional_cflags= \
	$(BCM2835_CFLAGS)

additional_libs= \
        $(BOOST_REGEX_LIBS) \
        $(BOOST_SYSTEM_LIBS) \
        $(BOOST_FILESYSTEM_LIBS) \
        $(BOOST_PROGRAM_OPTIONS_LIBS)

additional_ldflags= \
	-lpthread \
	$(BCM2835_LDFLAGS) \
        $(BOOST_LDFLAGS) \
        $(BOOST_REGEX_LDFLAGS) \
        $(BOOST_SYSTEM_LDFLAGS) \
        $(BOOST_FILESYSTEM_LDFLAGS) \
        $(BOOST_PROGRAM_OPTIONS_LDFLAGS)

bin_PROGRAMS= \
        triggerpi

triggerpi_SOURCES= \
	bits.h \
	expansion_board.h \
	production_chain.h \
	shared_block_buffer.h \
	builtin_trigger.h \
	builtin_const_datasource.h \
	waveshare_ADS1256.h \
	waveshare_ADS1256.cc \
	waveshare_ADS1256_config.cc \
        main.cc


triggerpi_CFLAGS=$(additional_cflags)
triggerpi_CPPFLAGS=$(additional_cppflags)
triggerpi_LDADD=$(additional_libs)
triggerpi_LDFLAGS=$(additional_ldflags)


triggerpi_configdir=$(pkgdatadir)
dist_triggerpi_config_DATA = \
        triggerpi_config
