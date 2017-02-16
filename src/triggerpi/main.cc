#include <config.h>

#include "bits.h"
#include "expansion_board.h"
#include "waveshare_ADS1256.h"
#include "timed_trigger.h" // to remove
#include "immediate_trigger.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <thread>

namespace b = boost;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

#if !defined(TRIGGERPI_SITE_CONFIGDIR)
#error missing TRIGGERPI_SITE_CONFIGDIR
#endif

typedef std::map<std::string,
  std::shared_ptr<expansion_board> > expansion_map_type;

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

std::pair<std::size_t, std::shared_ptr<expansion_board> >
parse_trigger_source(const expansion_map_type &expansion_map,
  const std::string &raw_str)
{
  std::string board_str;
  size_t trigger_id = 0;
  std::size_t count_tag_loc = raw_str.find('#');
  if(count_tag_loc != std::string::npos) {
    board_str = raw_str.substr(0,count_tag_loc);
    std::string raw_id = raw_str.substr(count_tag_loc+1);
    std::size_t unconverted = 0;
    try {
      trigger_id = stoull(raw_id,&unconverted);
    }
    catch(const std::exception &) {
      // conversion errors will be caught by incorrect 'unconverted'
    }

    if(unconverted != raw_id.size()) {
      std::stringstream err;
      err << "Expected nonnegative integer for expansion board trigger "
        "id. Received '" << raw_id << "'";
      throw std::runtime_error(err.str());
    }
  }
  else
    board_str = raw_str;

  std::shared_ptr<expansion_board> expansion;
  if(board_str == "immediate") {
    expansion.reset(new immediate_trigger());
  }
  else {
    expansion_map_type::const_iterator res = expansion_map.find(board_str);

    if(res == expansion_map.end()) {
      std::stringstream err;
      err << "Unknown expansion system: '" << board_str << "' used as "
        "a trigger";
      throw std::runtime_error(err.str());
    }

    expansion = res->second;
  }

  return std::make_pair(trigger_id,expansion);
}

std::pair<std::size_t, std::shared_ptr<expansion_board> >
parse_trigger_sink(const expansion_map_type &expansion_map,
  const std::string &raw_str)
{
  std::string board_str;
  size_t trigger_id = 0;
  std::size_t count_tag_loc = raw_str.find('#');
  if(count_tag_loc != std::string::npos) {
    board_str = raw_str.substr(0,count_tag_loc);
    std::string raw_id = raw_str.substr(count_tag_loc+1);
    std::size_t unconverted = 0;
    try {
      trigger_id = stoull(raw_id,&unconverted);
    }
    catch(const std::exception &) {
      // conversion errors will be caught by incorrect 'unconverted'
    }

    if(unconverted != raw_id.size()) {
      std::stringstream err;
      err << "Expected nonnegative integer for expansion board trigger "
        "id. Received '" << raw_id << "'";
      throw std::runtime_error(err.str());
    }
  }
  else
    board_str = raw_str;

  expansion_map_type::const_iterator res = expansion_map.find(board_str);

  if(res == expansion_map.end()) {
    std::stringstream err;
    err << "Unknown expansion system: '" << board_str << "' used as "
      "a trigger";
    throw std::runtime_error(err.str());
  }

  return std::make_pair(trigger_id,res->second);
}












void run_expansion_board(const std::shared_ptr<expansion_board> &expansion)
{
  try {
    if(expansion->disabled())
      return;

    expansion->setup_com();
    expansion->initialize();
    expansion->run();
  }
  catch(const std::exception &e) {
    std::cerr << e.what() << "\n";
  }
  catch (...) {
    std::cerr << "Unknown error\n";
    abort();
  }
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
    system_help << "Enable expansion system ARG. Specify as many as "
      "necessary\n";

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
      ("system,s",po::value<std::vector<std::string> >(),
        system_help_str.c_str())
      ("tsource",po::value<std::vector<std::string> >(),
        "  Set the trigger source with [id] to be [arg]. The trigger id is a "
        "positive and not necessarily sequential integer separated by the "
        "source with the '#' character. For example, the "
        "'foo' system is set to be a trigger source for trigger id '5', [arg] "
        "would be: 'foo#5'. If the id is missing, then the trigger is assumed "
        "to have id '0'. The system must be the name of a system previously "
        "set under --system or one of the following built-in sources:\n"
        "    'immediate' - all sinks will be triggered immediately and will "
        "continue until program exit.\n"
//         "    [timespec] - the sink will be triggered according to the given "
//         "timespec. If the timespec is an absolute time then "
//         "the sinks will be triggered not before that time. If "
//         "the timespec is a relative time, the sinks will be "
//         "triggered no earlier than program execution start "
//         "plus [timespec].\n"
        "    N.B. A trigger sink must be configured for the "
        "system to be notified. See --tsink.")
      ("tsink",po::value<std::vector<std::string> >(),
        "  Set the trigger sink with [id] to be [arg]. The trigger id is a "
        "positive and not necessarily sequential integer separated by the "
        "source with the '#' character. For example, the "
        "'foo' system is set to be a trigger source for trigger id '5', [arg] "
        "would be: 'foo#5'. If the id is missing, then the trigger is assumed "
        "to have id '0'. The system must be the name of a system previously "
        "set under --system. A trigger source must be configured for the "
        "system to be notified. See --tsource.")
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
        std::cout << "FAILED!\nNonexistant or non-regular user configuration "
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

    // determine and instantiate expansion system
    typedef std::vector<std::string> stringvec;

    expansion_map_type expansion_map;
    std::vector<std::thread> thread_vec;

    if(!vm.count("system"))
      throw std::runtime_error("Missing system configuration item");

    const std::vector<std::string> &systemvec =
      vm["system"].as<std::vector<std::string> >();

    for (auto & system : systemvec) {
      if(!registered_expansion.count(system)) {
        std::stringstream err;
        err << "Unknown expansion system: '" << system << "'";
        throw std::runtime_error(err.str());
      }

      if(expansion_map.find(system) != expansion_map.end()) {
        std::stringstream err;
        err << "Duplicate system: '" << system << "'";
        throw std::runtime_error(err.str());
      }

      std::shared_ptr<expansion_board> expansion(
        registered_expansion.at(system)->construct(vm));

      expansion_map[system] = expansion;
    }

    std::map<std::size_t,std::shared_ptr<expansion_board> > trigger_sources;
    std::map<std::size_t,
      std::vector<std::shared_ptr<expansion_board> > > trigger_sinks;

    if(vm.count("tsource")) {
      const std::vector<std::string> &tsource_vec =
        vm["tsource"].as<std::vector<std::string> >();

      for(auto & tsource : tsource_vec) {
        std::pair<std::size_t,std::shared_ptr<expansion_board> > result =
          parse_trigger_source(expansion_map,tsource);

        if(trigger_sources.find(result.first) != trigger_sources.end()) {
          std::cerr << "Warning: duplicate trigger source id "
            << result.first << " given in : '" << tsource << "'\n";
        }

        trigger_sources.insert(result);

        if(detail::is_verbose<1>(vm))
          std::cout << "Registered trigger source: '" << tsource << "\n";
      }
    }


    if(vm.count("tsink")) {
      const std::vector<std::string> &tsink_vec =
        vm["tsink"].as<std::vector<std::string> >();

      for(auto & tsink : tsink_vec) {
        std::pair<std::size_t,std::shared_ptr<expansion_board> > result =
          parse_trigger_sink(expansion_map,tsink);

        trigger_sinks[result.first].emplace_back(result.second);

        if(detail::is_verbose<1>(vm))
          std::cout << "Registered trigger sink: '" << tsink << "\n";
      }
    }

    // configure options for all. Must be done before registering the sinks
    for(auto & pair : expansion_map)
      pair.second->configure_options(vm);

    // register the sinks with all of the sources
    for(auto & psource : trigger_sources) {
      auto sink_iter = trigger_sinks.find(psource.first);
      if(sink_iter != trigger_sinks.end()) {
        for(auto &psink : sink_iter->second) {
          psource.second->configure_trigger_sink(psink);
        }
      }
      else {
        std::cerr << "Warning: configured trigger source id "
          << psource.first << " has no sinks\n";
      }
    }

    for(auto & pair : expansion_map)
      thread_vec.push_back(std::thread(run_expansion_board,pair.second));

    for(auto & thread : thread_vec)
      thread.join();

    for(auto & pair : expansion_map)
      pair.second->finalize();






#if 0
    // configure trigger
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


    // set up data handler
    expansion_board::data_handler hdlr;

    if(vm.count("outfile")) {
      try {
        hdlr = expansion_system->file_printer(
            fs::path(vm["outfile"].as<std::string>()));
      }
      catch(const fs::filesystem_error &ex) {
        std::stringstream err;
        err << "File error for'" << vm["outfile"].as<std::string>()
          << "'. " << ex.code() << ": " << ex.what();
        throw std::runtime_error(err.str());
      }
    }
    else {
      hdlr = expansion_system->screen_printer();
    }

    // Setup communications, initialize, and go
    expansion_system->setup_com();
    expansion_system->initialize();
//    expansion_system->trigger_sampling(hdlr,*trigger);

    expansion_system->start();

    expansion_system->finalize();

#endif
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
