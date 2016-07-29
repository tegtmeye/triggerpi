#include <config.h>

#include "bcm2835_sentry.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace b = boost;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

#if !defined(TRIGGERPI_SITE_CONFIGDIR)
#error missing TRIGGERPI_SITE_CONFIGDIR
#endif


int board_init(void);

int main(int argc, char *argv[])
{
  try {
    // configuration file
    int opt;
    std::string config_file;

    std::stringstream usage;
      usage <<
        PACKAGE_STRING "\n\n"
        "Utility for triggering things based on ADC input for the "
        "Raspberry Pi.\n"
        "Report bugs to: "PACKAGE_BUGREPORT "\n\n"
        "Usage: " << PACKAGE " [OPTIONS] [CONFIG_FILE]\n";

    std::string brief_header = "Try `" PACKAGE " --help' for more "
      "information";

    // general options
    po::options_description general("General options");
    general.add_options()
    // First parameter describes option name/short name
    // The second is parameter to option
    // The third is description
    ("help,h", "Print this message")
    ("version", "Print version string")
    ("verbose,v", "Be verbose")
    ("config,c", po::value<std::string>(),
                  "Use configuration file at <path>\n\n"
                  "A configuration file used to define the relevant "
                  "parameters used for reading and triggering. If one exists, "
                  "first use the default site configuration file located "
                  "at:\n\n  "
                    "\"" TRIGGERPI_SITE_CONFIGDIR "/triggerpi_config\"\n\n"
                  "These values are overridden by the local configuration "
                  "if it exists at:\n\n  \"$(HOME)/." PACKAGE "\"\n\n"
                  "If this option is given, do not use any of the predefined "
                  "configuration files but instead only use the one provided. "
                  "Any command line options will override the options "
                  "set in any configuration file. [CONFIG_FILE] is a shortcut "
                  "for\n\n  --config CONFIG_FILE.")
    ;


    // Configuration options
    po::options_description config("Configuration");
    config.add_options()
        ("optimization", po::value<int>(&opt)->default_value(10),
              "optimization level")
        ("include-path,I",
             po::value<std::vector<std::string> >()->composing(),
             "include path")
        ;

    // Hidden options, will be allowed both on command line and
    // in config file, but will not be shown to the user.
    po::options_description hidden("Hidden options");

    po::options_description cmdline_options;
    cmdline_options.add(general).add(config).add(hidden);

    po::options_description config_file_options;
    config_file_options.add(config).add(hidden);

    po::options_description visible;
    visible.add(general).add(config);

    po::positional_options_description pos_arg;
    pos_arg.add("config", 1);


    // read command line args
    po::variables_map vm;
    store(po::command_line_parser(argc,argv).
          options(cmdline_options).positional(pos_arg).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cout << usage.str() << visible << "\n";
        return 0;
    }

    if (vm.count("version")) {
        std::cout << "Version " VERSION "\n";
        return 0;
    }

    if (!vm.count("config")) {
      fs::path site_config_path(TRIGGERPI_SITE_CONFIGDIR"/triggerpi_config");

      if (vm.count("verbose")) {
        std::cout << "Attempting to read site configuration at: "
          << site_config_path << ": ";
      }

      if(fs::exists(site_config_path) && is_regular_file(site_config_path)) {
        fs::ifstream ifs(site_config_path);
        if(!ifs) {
          if (vm.count("verbose"))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open site configuration file at: "
          << site_config_path << ", skipping\n";
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if (vm.count("verbose"))
            std::cout << "Success\n";
        }
      }
      else if (vm.count("verbose")) {
        std::cout << "FAILED!\nNonexistant or non-regular site configuration"
          "file: " << site_config_path << ", skipping\n";
      }
    }
    else {
      // Only read the given config
      fs::path given_config_path(vm["config"].as<std::string>());

      if (vm.count("verbose")) {
        std::cout << "Attempting to read given configuration at: "
          << given_config_path << ": ";
      }

      if(fs::exists(given_config_path) && is_regular_file(given_config_path)) {
        fs::ifstream ifs(given_config_path);
        if(!ifs) {
          if (vm.count("verbose"))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open given configuration file at: "
          << given_config_path << ", aborting\n";
          return 1;
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if (vm.count("verbose"))
            std::cout << "Success\n";
        }
      }
      else {
        if (vm.count("verbose"))
          std::cout << "FAILED!\n";

        std::cerr << "Nonexistant or non-regular site configuration file: "
          << given_config_path << ", aborting\n";

        return 1;
      }
    }

    // Initialize the bcm2835 library. The library will automatically close
    // when this goes out of scope.
    bcm2835_sentry bcm2835lib_sentry;


    /*
        Enable the SPI interface. SPI will automatically disable and return the
        Raspberry PI pins to normal when this goes out of scope. If this fails,
        it could be because we are not executing with enough privileges to
        access the GPIO pins. TODO-should check for proper permission in the
        future. In any case, let the user know what is going on before failing
     */
    try {
      bcm2835_SPI_sentry bcm2835lib_sentry;
    }
    catch (const std::runtime_error &ex) {
      std::stringstream err;
      err << "Failed to initialize SPI interface. Are you running as root? "
        "Error was: " << ex.what();
      throw std::runtime_error(err.str());
    }

    board_init();

  }
  catch(const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
  catch (...) {
    std::cerr << "Unknown error\n";
    abort();
  }

  return 0;
}
