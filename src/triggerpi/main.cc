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
      "  Use configuration file at <path>\n"
      "A configuration file used to define the relevant "
      "parameters used for reading and triggering. If one exists, "
      "first use the default site configuration file at "
        "[" TRIGGERPI_SITE_CONFIGDIR "/triggerpi_config] "
      "These values are overridden by the local configuration "
      "if it exists at [~/." PACKAGE "]\n"
      "  If this option is given, do not use any of the predefined "
      "configuration files but instead only use the one provided. "
      "All command line options will override the options "
      "set in any configuration file. [CONFIG_FILE] is a shortcut "
      "for --config CONFIG_FILE")
    ;


    // Global Configuration options
    po::options_description global_config("Global Config Options");
    global_config.add_options()
      ("outfile,o", po::value<std::string>(),
        "  Output the configured channels into [file] according to format "
        "given by --format. If 'output' is not specified, output the "
        "configured channels to screen unless the --silent options is given.")
      ("format,f", po::value<std::string>()->default_value("csv"),
        "  Output the configured channels into [file] according to the given "
        "format. Only meaningful if --output is also given. Currently the only "
        "supported format is csv")
      ("duration,d",po::value<double>()->default_value(-1),
        "  Collection duration in seconds. Specify a negative value "
        "for indefinite collection length. Note: collection performance "
        "and duration is affected and ultimately limited by available memory "
        "or disk space depending on the value of --output and --format")
//       ("silent,s","  Quash all non-error data processing screen printing.")
      ("async,a",po::value<bool>()->default_value(true),
        "  Hint to run in asynchronous mode if possible. That is, reading from "
        "the ADC and writing to memory/disk will run in separate threads. "
        "This option will be overridden to 'false' for single-core CPUs")
      ("stats",po::value<bool>()->default_value(true),
        "  Collect statistics on system performance. For the ADC, this "
        "means that the per-sample delay is recorded.")
      ("ADC.system",po::value<std::string>(),
        "  ADC system to configure and use. Exactly one of:\n"
        "  'waveshare' - The Waveshare High Precision ADC/DAC expansion board"
        " based on the ADS1256 24-bit ADC and the DAC8532 16-bit DAC. "
        "NOTE: Currently this is the only supported system.")
      ;

    // Waveshare High-Precision ADC/DA Board Configuration options
    po::options_description waveshare_config(
      "Waveshare ADC/DA Config Options");
    waveshare_config.add_options()
      ("ADC.waveshare.sample_rate",po::value<std::string>(),
        "  Set the sample rate. Actual data rates depend on the number of "
        "channels to read. For the ADS1256 ADC chip, each channel is "
        "multiplexed using a single ADC core. Each additional channel "
        "sampled reduces the per-channel sampling rate linearly. This cycling "
        "also imposes an overhead that further reduces the peak sample rate "
        "from the advertised maximums. Valid values reflect the peak sample "
        "rate based on a single channel but the approximate corresponding "
        "multiplexed rate is also listed. For example 30000 ~4165 "
        "indicates that the valid setting is '30000' which corresponds to "
        "the single-channel sampling rate and a multiplexed rate of "
        "approximately 4165 samples-per-second which is evenly divided by "
        "the number of channels enabled. Valid values are one of:\n"
        "  Setting:     Multichannel rate (sps):\n"
        "   30000        ~4165 [default]\n"
        "   15000        ~3600\n"
        "    7500        ~2910\n"
        "    3750        ~2095\n"
        "    2000        ~1405\n"
        "    1000        ~825\n"
        "     500        ~475\n"
        "     100        ~98\n"
        "      60        ~59\n"
        "      50        ~49.5\n"
        "      30        ~30\n"
        "      25        ~?\n"
        "      15        ~?\n"
        "      10        ~?\n"
        "       5        ~?\n"
        "       2.5      ~?\n"
        "Currently rates below about 60 may cause a timeout error waiting "
        "for data to be ready on the ADC. This is a bug and needs to be fixed.")
      ("ADC.waveshare.gain",po::value<std::string>(),
        "  Set the gain for the configured ADC. Valid values are one of 1 "
        "[default], 2, 4, 8, 16, 32, or 64.")
      ("ADC.waveshare.Vref",po::value<std::string>()->default_value("2.5"),
        "  Set the ADC reference voltage. This is the positive voltage "
        "difference between pin 4 and pin 3 on the ADS1256 ADC chip itself. "
        "This setting is only needed if you have actually measured the "
        "voltage difference between these two pins and you need a higher-"
        "accuracy measurement. The nominal value for these boards is 2.5V. "
        "This only affects voltage calculations")
      ("ADC.waveshare.AINCOM",po::value<double>()->default_value(0.0),
        "  Set the ADC center voltage for single-ended inputs. This value is "
        "only meaningful for ADC count to voltage conversions. That is, the "
        "ADC counts will be centered around the AINCOM voltage input "
        "of this value. An ADC count of zero means "
        "the value set here. If the AINCOM/AGND jumper is installed, then "
        "this value should be 0 and the ADC value will be from 0->2^23 for "
        "a unity gain. If the jumper is removed and a 2.5V source is applied "
        "to AINCOM, set this option to 2.5 and the ADC values will vary from "
        "-2^22 to +2^22 for a unity gain (23-bits of precision) for a 0->5V "
        "input. If the gain is set to 2, then the ADC values will vary from "
        "-2^23 to +2^23 (24-bits of precision) for a 0->5V input")
      ("ADC.waveshare.buffered",po::value<bool>()->default_value(false),
        "  Enable/disable the ADS1256 analog input buffer. If enabled, then "
        "the input impedance presented to the analog input will scale "
        "according to the sampling frequency: 30 ksps to 2 ksps = ~10 MOhm, "
        "1 ksps = ~20 MOhm, 500 to 60 sps = ~40 MOhm, and <= 50 sps = ~80 "
        "MOhm. Default is disabled. NOTE: if enabled, then the voltage "
        "difference between analog ground and any analog in must be below "
        "AVDD-2V. Since the board is pre-configured for an AVDD of 5V, "
        "AD0-AD7 must be below 3V. See the ADS1255/6 datasheet for more "
        "information.")
     ("ADC.waveshare.sampleblocks",po::value<std::size_t>(),
        "  Override the number of samples to process in each block operation. "
        "This is a function of the number of channels currently configured, "
        "whether or not asynchronous operations are enabled, and is affected "
        "by system memory. This value must be a positive integer greater than "
        "one.")
     ("ADC.waveshare.channel",
        po::value<std::vector<std::string> >(),
        "  Configure each ADC channel. There can be multiple occurrences "
        "of ADC.channel as need to configure the desired input. For the "
        "Waveshare ADC/DA expansion board, there are 9 pins that can be "
        "configured as up to eight single-ended channels, up to four "
        "differential channels, or any combination thereof. The pins are "
        "labeled 0-8 and 'COM' where COM is a synonym for 8. The value is a "
        "comma-separated list of pin assignments and channel type id. Pins can "
        "only be listed once with the exception of the COM pin. One end of the "
        "single-ended channels should "
        "assigned to 'COM'. Differential channels can be assigned to any "
        "two pins but it is recommended that they be adjacent to each "
        "other for optimum performance. For differential channels, the "
        "first pin is taken to be the positive pin. "
        "Unlisted pins will be turned off [preferred]. Examples:\n"
        "  To configure pins 1 for singled ended input and pin 3 and 4 "
        "for differential:\n"
        "  --ADC.waveshare.channel 0,COM\n  --ADC.waveshare.channel 2,3\n")
      ;










    // Hidden options, will be allowed both on command line and
    // in config file, but will not be shown to the user.
    po::options_description hidden("Hidden options");

    po::options_description cmdline_options;
    cmdline_options.add(general).add(global_config).
      add(waveshare_config).add(hidden);

    po::options_description config_file_options;
    config_file_options.add(global_config).add(waveshare_config).add(hidden);

    po::options_description visible;
    visible.add(general).add(global_config).add(waveshare_config);

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

    // check for required configuration items
    if(!vm.count("ADC.system") || vm["ADC.system"].as<std::string>()=="none") {
      if (vm.count("verbose"))
        std::cout << "Disabling ADC\n";
    }
    else
      enable_adc(vm);





//     board_init();

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
