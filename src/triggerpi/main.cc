#include <config.h>

#include "bits.h"
#include "expansion_board.h"
#include "waveshare_expansion.h"
#include "timed_trigger.h"

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


void enable_adc(const po::variables_map &vm);

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
    ("verbose,v", po::value<unsigned int>()->implicit_value(1),
      "Be verbose. An optional level between 1 and 3 may be provided where "
      "-v1 (or --verbose=1) means least verbose and -v3 (or --verbose=3) "
      "means debug.")
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

    const expansion_board::factory_map_type & registered_expansion =
      expansion_board::factory_map();

    // build the help string for the list of registered expansion systems
    std::stringstream system_help;
    system_help << "Expansion system to configure and use. Exactly one of:\n";
    for (auto & cur : registered_expansion)
      system_help << "  '" << cur.second->system_config_name() << "' - "
        << cur.second->system_config_desc_long() << "\n";
    // avoid temp bug when using c_str()
    std::string system_help_str = system_help.str();

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
      ("async,a",po::value<bool>()->default_value(true),
        "  Hint to run in asynchronous mode if possible. That is, reading from "
        "the ADC and writing to memory/disk will run in separate threads. "
        "This option will be overridden to 'false' for single-core CPUs")
      ("stats",po::value<bool>()->default_value(true),
        "  Collect statistics on system performance. For the ADC, this "
        "means that the per-sample delay is recorded.")
      ("system,s",po::value<std::string>(),system_help_str.c_str())
      ;





    // build registered expansion options
    po::options_description expansion_options;
    for (auto & cur : registered_expansion)
      expansion_options.add(cur.second->cmd_options());

    // Hidden options, will be allowed both on command line and
    // in config file, but will not be shown to the user.
    po::options_description hidden("Hidden options");

    po::options_description cmdline_options;
    cmdline_options.add(general).add(global_config).
      add(expansion_options).add(hidden);

    po::options_description config_file_options;
    config_file_options.add(global_config).add(expansion_options).add(hidden);

    po::options_description visible;
    visible.add(general).add(global_config).add(expansion_options);

    po::positional_options_description pos_arg;
    pos_arg.add("config", 1);


    // read command line args
    po::variables_map vm;
    store(po::command_line_parser(argc,argv).
          options(cmdline_options).positional(pos_arg).run(), vm);
    notify(vm);

    if(vm.count("help")) {
      std::cout << usage.str() << visible << "\n";
      return 0;
    }

    if(vm.count("version")) {
      std::cout << "Version " VERSION "\n";
      return 0;
    }

    if(vm.count("verbose") && vm["verbose"].as<unsigned int>() > 3)
      throw std::runtime_error("Verbosity must be between 1 and 3");

    // No config file given, order of options precedence is:
    //  1: command line args
    //  2: user-local config file
    //  3: site-config file
    if (!vm.count("config")) {
      // Try and read user-local config file
      fs::path user_config_path = user_pref_dir()/fs::path(".triggerpi");

      if(detail::is_verbose<3>(vm)) {
        std::cout << "Attempting to read user configuration at: "
          << user_config_path << ": ";
      }

      if(fs::exists(user_config_path) && is_regular_file(user_config_path)) {
        fs::ifstream ifs(user_config_path);
        if(!ifs) {
          if(detail::is_verbose<3>(vm))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open user configuration file at: "
          << user_config_path << ", skipping\n";
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if(detail::is_verbose<3>(vm))
            std::cout << "Success\n";
        }
      }
      else if(detail::is_verbose<3>(vm)) {
        std::cout << "FAILED!\nNonexistant or non-regular user configuration"
          "file: " << user_config_path << ", skipping\n";
      }

      // Try and read site config file
      fs::path site_config_path(TRIGGERPI_SITE_CONFIGDIR"/triggerpi_config");

      if(detail::is_verbose<3>(vm)) {
        std::cout << "Attempting to read site configuration at: "
          << site_config_path << ": ";
      }

      if(fs::exists(site_config_path) && is_regular_file(site_config_path)) {
        fs::ifstream ifs(site_config_path);
        if(!ifs) {
          if(detail::is_verbose<3>(vm))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open site configuration file at: "
          << site_config_path << ", skipping\n";
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if(detail::is_verbose<3>(vm))
            std::cout << "Success\n";
        }
      }
      else if(detail::is_verbose<3>(vm)) {
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

      if(detail::is_verbose<2>(vm)) {
        std::cout << "Attempting to read given configuration at: "
          << given_config_path << ": ";
      }

      if(fs::exists(given_config_path) && is_regular_file(given_config_path)) {
        fs::ifstream ifs(given_config_path);
        if(!ifs) {
          if(detail::is_verbose<2>(vm))
            std::cout << "FAILED!\n";

          std::cerr << "Unable to open given configuration file at: "
          << given_config_path << ", aborting\n";
          return 1;
        }
        else {
          store(po::parse_config_file(ifs, config_file_options),vm);
          notify(vm);

          if(detail::is_verbose<2>(vm))
            std::cout << "Success\n";
        }
      }
      else {
        if(detail::is_verbose<2>(vm))
          std::cout << "FAILED!\n";

        std::cerr << "Nonexistant or non-regular site configuration file: "
          << given_config_path << ", aborting\n";

        return 1;
      }
    }

    // check for required configuration items
    if(!vm.count("system") || vm["system"].as<std::string>().empty())
      throw std::runtime_error("Missing system configuration item");

    const std::string &system = vm["system"].as<std::string>();

    std::shared_ptr<expansion_board> expansion_system;



    if(!registered_expansion.count(system)) {
      std::stringstream err;
      err << "Unknown expansion system: '" << system << "'";
      throw std::runtime_error(err.str());
    }

    expansion_system.reset(registered_expansion.at(system)->construct(vm));

    if(detail::is_verbose<1>(vm))
      std::cout << "Enabling "
        << registered_expansion.at(system)->system_config_desc_short() << "\n";

    expansion_system->configure_options();

    if(expansion_system->disabled()) {
      std::cout << "Nothing to do! Check configuration settings for the chosen "
        "system\n";
      return 0;
    }

    std::unique_ptr<basic_trigger> trigger;

    double duration = vm["duration"].as<double>();
    if(std::signbit(duration)) {
      trigger.reset(new indefinite_trigger());
      if(vm.count("verbose"))
        std::cout << "Collecting indefinitely\n";
    }
    else {
      // convert from floating point time duration in seconds to timed_trigger's
      // time duration representation (integer ticks)
      typedef std::chrono::duration<double,
        std::chrono::seconds::period> dseconds_type;

      trigger.reset(new timed_trigger(dseconds_type(duration)));

      if(vm.count("verbose"))
        std::cout << "Collecting for " << duration << "seconds.\n";
    }


    if(vm.count("outfile")) {
      try {
        expansion_board::data_handler hdlr =
          expansion_system->file_printer(
            fs::path(vm["outfile"].as<std::string>()));

        if(!hdlr)
          throw std::runtime_error("Unsupported output mode for this system");

        expansion_system->setup_com();
        expansion_system->initialize();
        expansion_system->trigger_sampling(hdlr,*trigger);
      }
      catch(const fs::filesystem_error &ex) {
        std::stringstream err;
        err << "File error for'" << vm["outfile"].as<std::string>()
          << "'. " << ex.code() << ": " << ex.what();
        throw std::runtime_error(err.str());
      }
    }
    else {
      expansion_board::data_handler hdlr;

      // don't produce anything if silent
      if(vm.count("silent"))
        hdlr = expansion_system->null_consumer();
      else
        hdlr = expansion_system->screen_printer();

      if(!hdlr)
        throw std::runtime_error("Unsupported output mode for this ADC board");

      expansion_system->setup_com();
      expansion_system->initialize();
      expansion_system->trigger_sampling(hdlr,*trigger);
    }
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
