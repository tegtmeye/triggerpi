#include <config.h>

#include "bits.h"
#include "expansion_board.h"
#include "waveshare_ADS1256.h"
#include "builtin_trigger.h"
#include "builtin_const_datasource.h"
#include "production_chain.h"

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




struct capture_productions {
  typedef std::vector<std::pair<std::string,std::string> > prod_pair_vec;

  capture_productions(prod_pair_vec &vec) :keyval_vec(vec) {}

  std::pair<std::string, std::string> operator()(const std::string &s)
  {
    auto loc = std::find(s.begin(),s.end(),'=');
    std::string key(s.begin(),loc);
    std::string value;

    if(loc != s.end())
      value.assign(++loc,s.end());

    if(key == "--trigger" || key == "--dataflow")// || key == "--chain")
      keyval_vec.get().push_back(std::make_pair(key,value));

    return std::make_pair(std::string(), std::string());
  }

  std::reference_wrapper<prod_pair_vec> keyval_vec;
};


void check_source_sink(std::shared_ptr<expansion_board> expansion)
{
  unsigned int _trigger_source = source_type(expansion->trigger_type());
  unsigned int _trigger_sink = sink_type(expansion->trigger_type());
  unsigned int _data_source = source_type(expansion->dataflow_type());
  unsigned int _data_sink = sink_type(expansion->dataflow_type());

  /*
    failure if no sink and single or multi source is specified but the
    optional flag is not provided.

    Same goes for the other conditions
  */

  // not a source but must be one
  if(!expansion->is_trigger_source() &&
      (_trigger_source == 2 || _trigger_source == 6))
  {
    std::stringstream err;
    err << "expansion: '" << expansion->system_identifier()
      << "' requires a trigger sink but none is provided.";
    throw std::runtime_error(err.str());
  }


  // not a sink but must be one
  if(!expansion->is_trigger_sink() &&
      (_trigger_sink == 2 || _trigger_sink == 6))
  {
    std::stringstream err;
    err << "expansion: '" << expansion->system_identifier()
      << "' requires a trigger source but none is provided.";
    throw std::runtime_error(err.str());
  }

  // not a data source but must be one
  if(!expansion->is_data_source() && (_data_source == 2)) {
    std::stringstream err;
    err << "expansion: '" << expansion->system_identifier()
      << "' requires a data sink but none is provided.";
    throw std::runtime_error(err.str());
  }

  // not a data sink but must be one
  if(!expansion->is_data_sink() && (_data_sink == 2)) {
    std::stringstream err;
    err << "expansion: '" << expansion->system_identifier()
      << "' requires a data sink but none is provided.";
    throw std::runtime_error(err.str());
  }
}


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
    _barrier->wait();

    expansion->run();
    expansion->trigger_shutdown();
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
        "Utility for controlling builtin and system expansions for "
        "embedded computers.\n"
        "Report bugs to: " PACKAGE_BUGREPORT "\n\n"
        "Usage: " << PACKAGE " [OPTIONS] [CONFIG_FILE]\n\n"
        "Try `" PACKAGE " --help' for more information.\n";

    // general options
    po::options_description general("General options");
    general.add_options()
    // First parameter describes option name/short name
    // The second is parameter to option
    // The third is description
    ("help,h", "Print this message\n")
    ("version", "Print version string\n")
    ("verbose,v", po::value<unsigned int>()->implicit_value(1),
      "Be verbose. An optional level between 1 and 3 may be provided where "
      "-v1 (or --verbose=1) means least verbose and -v3 (or --verbose=3) "
      "means debug.\n")
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
      "for --config CONFIG_FILE\n")
    ;

    const expansion_board::factory_map_type & registered_expansion =
      expansion_board::factory_map();

    // build the help string for the list of registered expansion systems
    std::stringstream system_help;
    system_help << "Enable expansion system ARG. Specify as many as "
      "necessary.\n";

    for (auto & cur : registered_expansion)
      system_help << "  '" << cur.second->system_config_name() << "' - "
        << cur.second->system_config_desc_long() << "\n";
    // avoid temp bug when using c_str()
    std::string system_help_str = system_help.str();

    // Global Configuration options
    po::options_description global_config("Global Config Options");
    global_config.add_options()
#if 0
      ("outfile,o", po::value<std::string>(),
        "  Output the configured channels into [file] according to format "
        "given by --format. If 'output' is not specified, output the "
        "configured channels to screen unless the --silent options is given.\n")
      ("format,f", po::value<std::string>()->default_value("csv"),
        "  Output the configured channels into [file] according to the given "
        "format. Only meaningful if --output is also given. Currently the only "
        "supported format is csv\n")
      ("duration,d",po::value<double>()->default_value(-1),
        "  Collection duration in seconds. Specify a negative value "
        "for indefinite collection length. Note: collection performance "
        "and duration is affected and ultimately limited by available memory "
        "or disk space depending on the value of --output and --format\n")
      ("async,a",po::value<bool>()->default_value(true),
        "  Hint to run in asynchronous mode if possible. That is, reading from "
        "the ADC and writing to memory/disk will run in separate threads. "
        "This option will be overridden to 'false' for single-core CPUs\n")
      ("stats",po::value<bool>()->default_value(true),
        "  Collect statistics on system performance. For the ADC, this "
        "means that the per-sample delay is recorded.\n")
#endif
      ("system,s",po::value<std::vector<std::string> >(),
        system_help_str.c_str())
      ("trigger",po::value<std::vector<std::string> >(),
        "  Set a trigger production of the form: "
        "sourcespec[[|source_sinkspec]...|sinkspec][#label]\n\n"
        "  Any meaningful production will either need to escape the '|' "
        "character or enclose the production in quotes.\n\n"
        "  A trigger production is a directed graph of expansions or "
        "built-in triggers such that if a trigger source A and a sink B are "
        "linked together and A notifies B (A -> B), then A -> B form a "
        "trigger production. If an expansion is capable of receiving trigger "
        "notifications then it is a trigger_sink. If it is capable of "
        "providing trigger notifications, then it is a trigger_source. Not "
        "all expansions are capable of being sources or sinks and any sink "
        "must have only one source whereas a source can have many sinks. "
        "If a label is provided, then it is considered a named production. "
        "A named production can be referred to in other productions as needed "
        "provided that the the source/sink requirements are met and the "
        "named production appears before its use. For example\n"
        "    --trigger=\"B|C@foo\" --trigger=\"A|foo|D\"\n"
        "  is equivalent to:\n"
        "    --trigger=\"A|B|C|D\"\n\n"
        "  A triggerspec is either a named production, a known system "
        "specified by the --system command, or a built-in trigger spec. "
        "Built in triggers can be specified directly in the trigger "
        "production if desired although specifying separately and assigning "
        "a label to them allows reuse. "
        "Currently available built-in triggers are:\n\n"
        "  timed_trigger: A trigger source that fires the trigger at "
        "startspec, alternates according to intervalspec, and stops at "
        "stopspec. The syntax has the following form:\n"
        "    [startspec][[intervalspec]][stopspec] where startspec and "
        "stopspec have the form timespec|durationspec and intervalspec "
        "has the form durationspec:durationspec. The time specification "
        "is a date and time parsed with "
        "the POSIX function strptime as '%Y-%m-%d %H:%M:%S'. That is, a "
        "date and time string of the form 'YYYY-MM-DD hh:mm:ss' where "
        "YYYY indicates the 4-digit year, MM indicates the 2-digit month "
        "and DD indicates the 2-digit day, hh indicates the hour (24 hour "
        "clock), mm indicates the 2-digit minute, and ss indicates the "
        "2-digit second. For example, the string '2017-02-28 12:15:06' "
        "means 'noon plus 15 minutes and 6 seconds on February 28 of "
        "2017'. The duration specification is a string of the form: "
        "'[Xh][Xm][Xs][Xms][Xus][Xns]' where 'X' is a non-negative "
        "integer and h,m,s,ms,us,ns indicates the number of hours, "
        "minutes, seconds, milliseconds, microseconds, and nanoseconds. "
        "Each grouping is optional. For example: 5m1s means 5 minutes and "
        "one second. If startspec is missing, the trigger will fire "
        "immediately. If stopspec is missing, the trigger will fire "
        "indefinitely. Each duration of intervalspec must be nonzero."
        "A word about start/stop time and duration. Time is "
        "a funny thing. Absolute time is considered system time and "
        "duration is considered elapsed time. For example, if the "
        "time between execution start and the trigger start/stop timespec "
        "spans a daylight savings time switch or clock adjustment due to "
        "an NTP update, the trigger will behave as expected and "
        "start/stop at the correct time. If the start/stop is given as a "
        "duration, it is taken as an absolute duration from execution "
        "start. That is, 2 seconds from now is not affected by any time "
        "synchronization adjustments. This phenomenon also exhibits "
        "itself when there is resource contention issues in a non "
        "real-time OS (always). For example, if the start is given as a "
        "timespec of noon, the trigger will not fire before noon but it "
        "will very likely not fire at exactly noon but some time "
        "after---the length depending on how resource constrained the "
        "system is. Likewise for a given stop timespec. Therefore, the "
        "total interval may be more or less depending on the systems "
        "available resources. For example, a start of noon and a stop of "
        "noon plus 1 second could be fired at noon plus 100 ms and the "
        "stop at noon plus 1 second and 10 ms (case of less) or noon plus "
        "1 second and 200 ms (case of more). Contrast this with a "
        "duration spec. In this case, you are requesting the trigger "
        "interval to be no less than the given duration. In a resource "
        "constrained system, the total interval may be more but it will "
        "never be less.\n\n"
        "   Examples:\n"
        "     --trigger=\"2017-02-28 12:15:06#2\"\n"
        "     Create a builtin trigger at slot 2 and start at noon plus "
        "15 minutes and 6 seconds on February 28 of 2017 and continue "
        "indefinitely\n\n"
        "     --trigger=\"2m[]2020-02-28 12:15:06\"\n"
        "     Start 2 minutes from now and stop at noon plus 15 minutes "
        "and 6 seconds on February 28 of 2020 and continue indefinitely\n\n"
        "     --trigger=\"2m[100ms:100ms]\"\n"
        "     Start 2 minutes from now, turn on for 100 ms and then off for "
        "100 ms repeating indefinitely\n\n"
        "     --trigger=\"[100ms:100ms]2m\"\n"
        "     Start immediately, then repeatedly turn on for 100 ms and "
        "then off for 100 ms until 2 minutes have elapsed\n\n"
        "     --trigger=\"2s@start\" --trigger=\"start|foo\" "
        "--trigger=\"start|bar\"\n"
        "     Make a built-in trigger at 2 seconds, assign 'start' as an "
        "alias to it, and use it to trigger both 'foo' and 'bar'\n\n")
      ("dataflow",po::value<std::vector<std::string> >(),"")
//       ("chain",po::value<std::vector<std::string> >(),"")
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


    capture_productions::prod_pair_vec production_keyvalue_vec;

    // read command line args
    po::variables_map vm;
    store(po::command_line_parser(argc,argv).
      options(cmdline_options).positional(pos_arg).
        extra_parser(capture_productions(production_keyvalue_vec)).
        run(), vm);
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

    /*
      production map is a mapping of named productions. All
      registered systems are also a named production of size 1
    */
    std::map<std::string,production_chain> production_map;

    /*
      expansion set is a set of all registered expansions
    */
    std::set<std::shared_ptr<expansion_board> > expansion_set;

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

      if(production_map.find(system) != production_map.end()) {
        std::stringstream err;
        err << "Duplicate system: '" << system << "'";
        throw std::runtime_error(err.str());
      }

      expansion.reset(registered_expansion.at(system)->construct());

      if(detail::is_verbose<2>(vm))
        std::cout << "Registering expansion: '"
          << expansion->system_description() << "'\n";

      // each system is added as a production of size 1
      production_map[system] = production_chain(system,expansion,expansion);

      expansion_set.emplace(expansion);
    }

    // process the productions
    for(auto keyval : production_keyvalue_vec) {
      std::string prodkey = keyval.first;
      std::string prodspec = keyval.second;

      std::string::iterator loc =
        std::find(prodspec.begin(),prodspec.end(),'@');

      std::string spec_str(prodspec.begin(),loc);

      std::string label;
      if(loc != prodspec.end())
        label.assign(++loc,prodspec.end());

      std::vector<production_chain> new_prod;

      if(prodkey == "--trigger") {
        new_prod = make_production_chain(spec_str,production_map,
          make_builtin_trigger<char>);
      }
      else if(prodkey == "--dataflow") {
        new_prod = make_production_chain(spec_str,production_map,
          make_builtin_dataflow<char>);
      }
//       else if(prodkey == "--chain") {
//         new_prod = make_production_chain(spec_str,production_map);
//       }

      assert(!new_prod.empty());

      /*
        new_prod is considered to be well formed but unlinked
        and may contain new builtin expansions. Ensure new expansions
        are contained in expansion_set. Simply calling emplace works
        because: if the prod refers to a new expansion, prod.first ==
        prod.second. No new expansions will be embedded in between
        prod.first and prod.second. Emplace will only add the
        expansion if it doesn't already exist
      */
      for(auto data_prod : new_prod) {
        const auto &result_pair = expansion_set.emplace(data_prod.head);

        if(result_pair.second && detail::is_verbose<2>(vm)) {
          std::cout << "Registered builtin expansion '"
            << (*result_pair.first)->system_description() << "'\n";
        }
      }

      // now link the production together
      if(prodkey == "--trigger") {
        for(std::size_t i=1; i<new_prod.size(); ++i)
          new_prod[i-1].tail->configure_trigger_sink(new_prod[i].head);
      }
      else if(prodkey == "--dataflow") {
        for(std::size_t i=1; i<new_prod.size(); ++i)
          new_prod[i-1].tail->configure_data_sink(new_prod[i].head);
      }

      // add the production to the named production map if so labeled
      if(!label.empty()) {
        production_map[label] =
          production_chain(label,new_prod.front().head,new_prod.front().tail);
      }

      if(detail::is_verbose<3>(vm)) {
        if(label.empty())
          std::cout << "Created anonymous production:\n";
        else
          std::cout << "Assigned label '" << label << "' to production:\n";

        for(auto &trig_prod : new_prod)
          std::cout << "  '" << trig_prod.label << "'\n";
      }
    }

    for(auto expansion : expansion_set) {
      if(detail::is_verbose<3>(vm))
        std::cout << "Configuring expansion: '"
          << expansion->system_identifier() << "'\n";

      expansion->configure_options(vm);
    }

    /*
      Disable any expansions that have no affect. Must be done after
      configure_options otherwise the client may inadvertently re-enable.
    */
    for(auto expansion : expansion_set) {
      if(expansion->is_disabled() &&
        (expansion->is_data_source() || expansion->is_data_sink()
          || expansion->is_trigger_source() || expansion->is_trigger_sink()))
      {
        std::cerr << "Warning: expansion: '"
          << expansion->system_identifier() << "' is configured as a trigger "
            "or data source or sink but is configured as disabled\n";
      }

      // will throw an exception if trigger or data requirements are not met
      check_source_sink(expansion);
    }

    for(auto expansion : expansion_set) {
      if(detail::is_verbose<3>(vm))
        std::cout << "Initializing expansion: '"
          << expansion->system_identifier() << "'\n";

      expansion->initialize();
    }



    // set up a barrier so that all threads start at once
    std::vector<std::thread> thread_vec;
    barrier _barrier(expansion_set.size()+1); // add one for main thread

    for(auto expansion : expansion_set) {
      thread_vec.push_back(
        std::thread(run_expansion_board,expansion,&_barrier));
    }

    _barrier.wait();

    // wait until done
    for(auto & thread : thread_vec)
      thread.join();

    for(auto expansion : expansion_set) {
      if(detail::is_verbose<3>(vm))
        std::cout << "Finalizing expansion: '"
          << expansion->system_identifier() << "'\n";

      expansion->finalize();
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
