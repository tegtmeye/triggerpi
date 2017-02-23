#include <config.h>

#include "bits.h"
#include "expansion_board.h"
#include "waveshare_ADS1256.h"
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
#include <thread>
#include <regex>

namespace b = boost;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace chrono = std::chrono;

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

std::string to_string(trigger_type type)
{
  std::string result;

  if(type == trigger_type::none)
    result = "no_trigger";
  else {
    if((type & trigger_type::single_shot) != trigger_type::none)
      result += "single_shot_trigger ";

    if((type & trigger_type::intermittent) != trigger_type::none)
      result += "intermittent_trigger ";
  }

  return result;
}

std::pair<std::size_t,std::string>
parse_triggerspec(const std::string &raw_str)
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

  return std::make_pair(trigger_id,board_str);
}

std::pair<std::tm,bool>
parse_timespec(const std::string &raw_str, std::string &remainder)
{
  std::pair<std::tm,bool> result({},false);

  std::istringstream in(raw_str);
  in >> std::get_time(&result.first, timed_trigger::time_fmt());
  if (!in.fail()) {
    in >> remainder;
    result.second = true;
  }

  return result;
}

std::pair<std::chrono::nanoseconds,bool>
parse_durspec(const std::string &raw_str)
{
  static const std::regex expr("^"
    "(([[:digit:]]+)[hH])"
    "(:([[:digit:]]+)[mM])?"
    "(:([[:digit:]]+)[sS])?"
    "(:([[:digit:]]+)(ms|MS))?"
    "(:([[:digit:]]+)(us|US))?"
    "(:([[:digit:]]+)(ns|NS))?");

  std::pair<std::chrono::nanoseconds,bool>
    results(std::chrono::nanoseconds(0),false);

  try {
    std::smatch match;
    if(!std::regex_match(raw_str,match,expr)) {
      results.second = true;

      if(match[2].length())
        results.first += std::chrono::hours(std::stoull(match[2]));
      if(match[4].length())
        results.first += std::chrono::minutes(std::stoull(match[4]));
      if(match[6].length())
        results.first += std::chrono::seconds(std::stoull(match[6]));
      if(match[8].length())
        results.first += std::chrono::milliseconds(std::stoull(match[8]));
      if(match[11].length())
        results.first += std::chrono::microseconds(std::stoull(match[11]));
      if(match[14].length())
        results.first += std::chrono::nanoseconds(std::stoull(match[14]));
    }
  }
  catch (...) {
    // if throw then error in validation regex
    abort();
  }

  return results;
}

std::pair<std::chrono::nanoseconds,std::tm>
parse_durtimespec(const std::string &raw_str)
{
  static const std::regex expr("^"
    "(([[:digit:]]+)[hH])"
    "(:([[:digit:]]+)[mM])?"
    "(:([[:digit:]]+)[sS])?"
    "(:([[:digit:]]+)(ms|MS))?"
    "(:([[:digit:]]+)(us|US))?"
    "(:([[:digit:]]+)(ns|NS))?");

  std::pair<std::chrono::nanoseconds,std::tm> results({},{});

  if(raw_str.empty())
    throw std::runtime_error("empty duration and timespec");

  std::size_t at_loc = raw_str.find('@');

  if(at_loc != 0) {
    std::smatch match;
    if(!std::regex_match(raw_str.substr(0,at_loc),match,expr)) {
      std::stringstream err;
      err << "Invalid duration specification in: '" << raw_str << "'";
      throw std::runtime_error(err.str());
    }

    try {
      if(match[2].length())
        results.first += std::chrono::hours(std::stoull(match[2]));
      if(match[4].length())
        results.first += std::chrono::minutes(std::stoull(match[4]));
      if(match[6].length())
        results.first += std::chrono::seconds(std::stoull(match[6]));
      if(match[8].length())
        results.first += std::chrono::milliseconds(std::stoull(match[8]));
      if(match[11].length())
        results.first += std::chrono::microseconds(std::stoull(match[11]));
      if(match[14].length())
        results.first += std::chrono::nanoseconds(std::stoull(match[14]));
    }
    catch (...) {
      // if throw then error in validation regex
      abort();
    }
  }
  if(at_loc < raw_str.size()-1) {
    std::istringstream in(raw_str.substr(at_loc+1));
    in >> std::get_time(&results.second, "%Y-%m-%d %H:%M:%S");
    if(!(!in.fail() && in.eof())) {
      std::stringstream err;
      err << "Invalid date and time specification in: '" << raw_str << "'";
      throw std::runtime_error(err.str());
    }
  }

  return results;
}


struct immediate_trigger_equal :
  std::unary_function<const std::shared_ptr<timed_trigger> &,bool>
{
  immediate_trigger_equal(std::chrono::nanoseconds dur, const std::tm &when)
    :_duration(dur), _when(&when) {}

  bool operator()(const std::shared_ptr<timed_trigger> &trig) const {
    return (trig->duration() == _duration) &&
      std::memcmp(_when,&trig->when(),sizeof(std::tm)) == 0;
  }

  std::chrono::nanoseconds _duration;
  const std::tm *_when;
};



void register_builtin(const std::shared_ptr<expansion_board> &expansion,
  expansion_map_type &expansion_map, const po::variables_map &vm,
  const std::string &arg_name)
{
  if(detail::is_verbose<2>(vm))
    std::cout << "Registering builtin expansion: '"
      << expansion->system_description() << "'\n";

  expansion->configure_options(vm);
  expansion_map[arg_name] = expansion;
}








class barrier
{
  public:
    explicit barrier(std::size_t count) : _count{count} { }

    void wait(void) {
      std::unique_lock<std::mutex> lock{_mutex};
      if (--_count == 0) {
          _cv.notify_all();
      } else {
          _cv.wait(lock, [this] { return _count == 0; });
      }
    }

  private:
      std::mutex _mutex;
      std::condition_variable _cv;
      std::size_t _count;
};


void run_expansion_board(const std::shared_ptr<expansion_board> &expansion,
  barrier *_barrier)
{
  try {
    expansion->setup_com();
    expansion->initialize();

    _barrier->wait();

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
        "    [duration][@timespec] - the sink will be triggered for the given "
        "duration at the indicated timespec. [duration] is a string of the "
        "form: '[Xh][:Xm][:Xs][:Xms][:Xus][:Xns]' where 'X' is a non-negative "
        "integer and h,m,s,ms,us,ns indicates the number of hours, minutes, "
        "seconds, milliseconds, microseconds, and nanoseconds. Each grouping "
        "is optional as long as it is separated by a colon. For example: "
        "5m:1s means 5 minutes and one second. [timespec] is a date and time "
        "parsed with the POSIX function strptime as '%Y-%m-%d %H:%M:%S'. That "
        "is, a date and time string of the form 'YYYY-MM-DD hh:mm:ss' "
        "where YYYY indicates the 4-digit year, MM indicates the 2-digit month "
        "and DD indicates the 2-digit day, hh indicates the hour "
        "(24 hour clock), mm indicates the 2-digit minute, and ss indicates "
        "the 2-digit second. For example, the string '2017-02-28 12:15:06' "
        "means 'noon plus 15 minutes and 6 seconds on February 28 of 2017'. "
        "A missing [duration] means continue "
        "forever and a missing [timespec] means start immediately. Thus "
        "'--tsource=@' means 'start immediately and continue forever'.\n"
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
    expansion_map_type expansion_map;

    if(!vm.count("system"))
      throw std::runtime_error("Missing system configuration item");

    const std::vector<std::string> &systemvec =
      vm["system"].as<std::vector<std::string> >();

    std::shared_ptr<expansion_board> expansion;

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

      expansion.reset(registered_expansion.at(system)->construct());

      if(detail::is_verbose<2>(vm))
        std::cout << "Registering expansion: '"
          << expansion->system_description() << "'\n";

      expansion->configure_options(vm);

      expansion_map[system] = expansion;
    }

    std::map<std::size_t,std::shared_ptr<expansion_board> > trigger_sources;
    std::map<std::size_t,
      std::set<std::shared_ptr<expansion_board> > > trigger_sinks;

    if(vm.count("tsource")) {
      const std::vector<std::string> &tsource_vec =
        vm["tsource"].as<std::vector<std::string> >();

      for(auto & tsource_str : tsource_vec) {
        std::pair<std::size_t,std::string> triggerspec =
          parse_triggerspec(tsource_str);

        std::shared_ptr<expansion_board> tsource;

        auto installed_expansion = expansion_map.find(triggerspec.second);
        if(installed_expansion == expansion_map.end()) {
          // not an expansion. try making an timed trigger
          // ne may already exist though so
          // build the key string and check. If so, use the currently installed
          // one instead
          auto timespec = parse_durtimespec(triggerspec.second);

          tsource.reset(new timed_trigger(timespec.first,timespec.second));

          std::string keystr = tsource->system_description();
          installed_expansion = expansion_map.find(keystr);
          if(installed_expansion == expansion_map.end()) {
            // still not found, so add it
            expansion_map[keystr] = tsource;

            if(detail::is_verbose<3>(vm)) {
              std::cout << "Created new trigger source: '"
                << keystr << "\n";
            }
          }
        }
        else {
          tsource = installed_expansion->second;

          if(tsource->trigger_source_type() == trigger_type::none) {
            std::stringstream err;
            err << "Error: system: '" << tsource->system_description()
              << "' cannot be configured as a trigger source";
            throw std::runtime_error(err.str());
          }
        }

        if(trigger_sources.find(triggerspec.first) != trigger_sources.end()) {
          std::cerr << "Warning: duplicate trigger source id "
            << triggerspec.first << " given in : '" << tsource_str << "'\n";
        }

        trigger_sources[triggerspec.first] = tsource;;

        tsource->enable();

        if(detail::is_verbose<2>(vm))
          std::cout << "Registered trigger source: '" << triggerspec.second
            << "' for slot " << triggerspec.first << "\n";
      }
    }


    if(vm.count("tsink")) {
      const std::vector<std::string> &tsink_vec =
        vm["tsink"].as<std::vector<std::string> >();

      for(auto & tsink_str : tsink_vec) {
//         std::pair<std::size_t,std::shared_ptr<expansion_board> > result =
//           parse_trigger(expansion_map,tsink_str);

        std::pair<std::size_t,std::string> triggerspec =
          parse_triggerspec(tsink_str);

        std::shared_ptr<expansion_board> tsink;

        auto installed_expansion = expansion_map.find(triggerspec.second);

        if(installed_expansion == expansion_map.end()) {
          std::stringstream err;
          err << "Unknown expansion system in: '" << tsink_str << "' used as "
            "a trigger sink";
          throw std::runtime_error(err.str());
        }

        tsink = installed_expansion->second;

        if(tsink->trigger_sink_type() == trigger_type::none) {
          std::stringstream err;
          err << "Error: system: '" << tsink->system_description()
            << "' cannot be configured as a trigger sink";
          throw std::runtime_error(err.str());
        }

        auto & sink_set = trigger_sinks[triggerspec.first];
        if(sink_set.find(tsink) != sink_set.end()) {
          std::stringstream err;
          err << "Error: duplicate trigger sink: '"
            << tsink->system_description()
            << "' for id " << triggerspec.first;
          throw std::runtime_error(err.str());
        }

        sink_set.insert(tsink);

        tsink->enable();

        if(detail::is_verbose<2>(vm))
          std::cout << "Registered trigger sink: '" << tsink_str
            << "' for slot " << triggerspec.first << "\n";
      }
    }

    /*
      register the sinks with all of the sources. If the source and sinks have
      incompatible trigger_types, then throw an error.
    */
    for(auto & psource : trigger_sources) {
      auto sink_iter = trigger_sinks.find(psource.first);
      if(sink_iter != trigger_sinks.end()) {
        if(detail::is_verbose<2>(vm))
          std::cout << "Configuring trigger #: source -> sink\n";

        for(auto &sink : sink_iter->second) {
          trigger_type trig =
            (psource.second->trigger_source_type() & sink->trigger_sink_type());

          if(trig == trigger_type::none) {
            std::stringstream err;
            err << "Error: incompatible trigger types: '"
              << psource.second->system_description() << "' configured as: "
              << to_string(psource.second->trigger_source_type())
              << "' and system: '"
              << sink->system_description() << "' configured as: "
              << to_string(sink->trigger_sink_type());
            throw std::runtime_error(err.str());
          }

          if(detail::is_verbose<2>(vm)) {
            std::cout << "  #" << sink_iter->first
              << ": '" << psource.second->system_description()
              << "' -> '" << sink->system_description() << "\n";
          }

          psource.second->configure_trigger_sink(sink);
        }
      }
      else {
        std::cerr << "Warning: configured trigger source at id "
          << psource.first << " does not have a sink!\n";
      }
    }

    // check to ensure all sinks have sources
    for(auto & psink : trigger_sinks) {
      auto source_iter = trigger_sources.find(psink.first);
      if(source_iter == trigger_sources.end()) {
        std::cerr << "Warning: configured trigger sink(s) at id "
          << psink.first << " does not have a source.\n";
        for(auto & sink : psink.second) {
          std::cerr << "  Disabling: " << sink->system_description() << "\n";
          sink->disable();
        }
      }
    }

    std::size_t num_enabled = 1; // plus one for main thread
    for(auto & pair : expansion_map) {
      if(pair.second->is_enabled())
        ++num_enabled;
    }

    // set up a barrier so that all threads start at once
    std::vector<std::thread> thread_vec;
    barrier _barrier(num_enabled);

    for(auto & pair : expansion_map) {
      if(pair.second->is_enabled()) {
        thread_vec.push_back(
          std::thread(run_expansion_board,pair.second,&_barrier));
      }
    }

    _barrier.wait();

    // wait until done
    for(auto & thread : thread_vec)
      thread.join();

    for(auto & pair : expansion_map)
      pair.second->finalize();

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
