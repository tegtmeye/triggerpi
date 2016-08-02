#include <config.h>

#include "ADC_board.h"
#include "ADC_ops.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace b = boost;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

#if !defined(TRIGGERPI_SITE_CONFIGDIR)
#error missing TRIGGERPI_SITE_CONFIGDIR
#endif


int board_init(void);


// attempt to get user defaults directory in a platform agnostic way
fs::path user_pref_dir(void)
{
  fs::path pref_dir; // empty

  const char * home = 0;
  if ((home=getenv("HOME")) || (home = getenv("USERPROFILE")))
    pref_dir = home;

  const char *hdrive = 0;
  const char *hpath = 0;
  if((hdrive = getenv("HOMEDRIVE")) && (hpath = getenv("HOMEPATH")))
    pref_dir = (fs::path(hdrive)/fs::path(hpath));

  return pref_dir;
}


int main(int argc, char *argv[])
{
  try {
    // configuration file
    std::string config_file;

    std::stringstream usage;
      usage <<
        PACKAGE_STRING "\n\n"
        "Utility for triggering things based on ADC input for the "
        "Raspberry Pi.\n"
        "Report bugs to: " PACKAGE_BUGREPORT "\n\n"
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
        ("ADC.system",po::value<std::string>(),
          "System to configure and use. Exactly one of:\n"
          "  'waveshare' - The Waveshare High Precision ADC/DAC expansion board"
          " based on the ADS1256 24-bit ADC and the DAC8532 16-bit DAC\n\n"
          "NOTE: Currently this is the only supported system.")
        ("ADC.sample_rate",po::value<std::string>(),
          "Set the sample rate for the configured ADC. Valid values are based "
          "on the chosen value of 'system'. If the value of 'ADC.system' is:\n"
          "  'waveshare' - Then 'sample_rate' is one of 30000, 15000, 7500, "
          "3750, 2000, 1000, 500, 100, 60, 50, 30, 25, 15, 10, 5, or 2.5 "
          "samples per second. If missing, the default is 30000 samples per "
          "second.")
        ("ADC.gain",po::value<std::string>(),
          "Set the gain for the configured ADC. Valid values are based "
          "on the chosen value of 'system'. If the value of 'ADC.system' is:\n"
          "  'waveshare' - Then 'gain' is one of 1,2,4,8,16,32, or 64. "
          "If missing, the default is unity gain (1).")

//        ("include-path,I",
//             po::value<std::vector<std::string> >()->composing(),
//             "include path")
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

    // No config file given, order of options precedence is:
    //  1: command line args
    //  2: user-local config file
    //  3: site-config file
    if (!vm.count("config")) {
      // Try and read user-local config file
      fs::path user_config_path = user_pref_dir()/fs::path(".triggerpi");

      if (vm.count("verbose")) {
        std::cout << "Attempting to read user configuration at: "
          << user_config_path << ": ";
      }

      if(fs::exists(user_config_path) && is_regular_file(user_config_path)) {
        fs::ifstream ifs(user_config_path);
        if(!ifs) {
          if (vm.count("verbose"))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open user configuration file at: "
          << user_config_path << ", skipping\n";
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if (vm.count("verbose"))
            std::cout << "Success\n";
        }
      }
      else if (vm.count("verbose")) {
        std::cout << "FAILED!\nNonexistant or non-regular user configuration"
          "file: " << user_config_path << ", skipping\n";
      }

      // Try and read site config file
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
      // User provided a config file. Overlay it with the command line args
      // Order of precedence is:
      //  1: command line args
      //  2: provided config file
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

    std::unique_ptr<ADC_board> adc_board;

    // check for required configuration items
    if(!vm.count("ADC.system") || vm["ADC.system"].as<std::string>()=="none") {
      if (vm.count("verbose"))
        std::cout << "Disabling ADC\n";
    }
    else
      adc_board = enable_adc(vm);





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
