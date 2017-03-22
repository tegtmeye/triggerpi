/*
  Immediate trigger that simply triggers the sinks as soon as it is run

*/

#ifndef TRIGGERPI_BUILTIN_TRIGGER_H
#define TRIGGERPI_BUILTIN_TRIGGER_H

#include "expansion_board.h"

#include <ctime>
#include <chrono>
#include <string>
#include <iomanip>
#include <sstream>
#include <thread>
#include <regex>

#include <unistd.h> // for pause


/*
  Built in trigger

  Trigger can start and stop based on either a wall time or a duration
  from the start of the run method. Between the start and stop time, the
  trigger can alternate based on an on duration and an off duration. The
  trigger also supports starting immediately and running indefinitely.

  A word about start/stop time and duration. Time is a funny thing.
  Absolute time is considered \em system time and duration is considered
  \em elapsed time. For example, if the time between execution start and
  the trigger start/stop timespec spans a daylight savings time switch
  or clock adjustment due to an NTP update, the trigger will behave as
  expected and start/stop at the correct time. If the start/stop is
  given as a duration, it is taken as an absolute duration from
  execution start. That is, 2 seconds from now is not affected by any
  time synchronization adjustments. This phenomenon also exhibits itself
  when there is resource contention issues in a non real-time OS
  (always). For example, if the start is given as a timespec of noon,
  the trigger will not fire before noon but it will very likely not fire
  at \em exactly noon but some time after---the length depending on how
  resource constrained the system is. Likewise for a given stop
  timespec. Therefore, the total interval may be more or less depending
  on the systems available resources. For example, a start of noon and a
  stop of noon plus 1 second could be fired at noon plus 100 ms and the
  stop at noon plus 1 second and 10 ms (case of less) or noon plus 1
  second and 200 ms (case of more). Contrast this with a duration spec.
  In this case, you are requesting the trigger interval to be no less
  than the given duration. In a resource constrained system, the total
  interval may be more but it will never be less.
*/
template<typename TimeBaseT = std::chrono::nanoseconds>
class builtin_trigger :public expansion_board {
  public:
    typedef TimeBaseT time_base;

    // trigger constantly between \c start and \c stop
    template<typename Clock1, typename Clock2>
    builtin_trigger(typename std::chrono::time_point<Clock1> start,
      typename std::chrono::time_point<Clock2> stop);

    template<typename Clock>
    builtin_trigger(time_base start,
      typename std::chrono::time_point<Clock> stop);

    template<typename Clock>
    builtin_trigger(typename std::chrono::time_point<Clock> start,
      time_base stop);

    builtin_trigger(time_base start,
      time_base stop);

    // trigger constantly starting at \c start and continuing indefinitely
    template<typename Clock>
    builtin_trigger(typename std::chrono::time_point<Clock> start, bool);

    builtin_trigger(time_base start, bool);


    // trigger constantly starting immediately and stopping at \c stop
    template<typename Clock>
    builtin_trigger(bool, typename std::chrono::time_point<Clock> stop);

    builtin_trigger(bool, time_base stop);

    /*
      cycle on for \c on_dur and off for \c off_dur starting at
      \const_start and stopping at \c const_stop. Each interval will not
      be less that \c on_dur + \c off_dur long which means that if the
      last cycle cannot be full length, it will not be triggered
    */
    template<typename Clock1, typename Clock2>
    builtin_trigger(typename std::chrono::time_point<Clock1> start,
      time_base on_dur, time_base off_dur,
      typename std::chrono::time_point<Clock2> stop);

    template<typename Clock>
    builtin_trigger(time_base start,
      time_base on_dur, time_base off_dur,
      typename std::chrono::time_point<Clock> stop);

    template<typename Clock>
    builtin_trigger(typename std::chrono::time_point<Clock> start,
      time_base on_dur, time_base off_dur,
      time_base stop);

    builtin_trigger(time_base start,
      time_base on_dur, time_base off_dur,
      time_base stop);

    /*
      cycle on for \c on_dur and off for \c off_dur starting at
      \const_start and continuing indefinitely.
    */
    template<typename Clock>
    builtin_trigger(typename std::chrono::time_point<Clock> start,
      time_base on_dur, time_base off_dur);

    builtin_trigger(time_base start,
      time_base on_dur, time_base off_dur, bool);

    /*
      cycle on for \c on_dur and off for \c off_dur starting immediately
      and stopping at \c const_stop. Each interval will not be less that
      \c on_dur + \c off_dur long which means that if the last cycle
      cannot be full length, it will not be triggered
    */
    template<typename Clock>
    builtin_trigger(time_base on_dur,
      time_base off_dur,
      typename std::chrono::time_point<Clock> stop);

    builtin_trigger(bool, time_base on_dur,
      time_base off_dur, time_base stop);

    virtual void run(void) {
      _run();
    }


    virtual std::string system_description(void) const {
      return _ascii_str;
    }

  private:
    static constexpr const char * system_prefix = "builtin trigger";

    std::function<void(void)> _run;

    std::string _ascii_str;

    // convert to reduced string. ie lots of ns -> h,m,s,...
    static std::string to_string(time_base dur);
};

template<typename TimeBaseT>
template<typename Clock1, typename Clock2>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock1> start,
  typename std::chrono::time_point<Clock2> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count() << "[]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  time_base start, typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start) << "[]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock> start, time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    std::this_thread::sleep_for(stop);


    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count() << "[]"
    << to_string(stop);
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(time_base start,
  time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    std::this_thread::sleep_for(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start) << "[]" << to_string(stop);
  _ascii_str = out.str();
}


template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock> start, bool)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    pause();
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count() << "[]";
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(
  time_base start, bool)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    pause();
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start) << "[]";
  _ascii_str = out.str();
}



template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(bool,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << "[]" << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(
  bool, time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    trigger_start();

    std::this_thread::sleep_for(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << system_prefix << " "
    << "[]" << to_string(stop);
  _ascii_str = out.str();
}

template<typename TimeBaseT>
template<typename Clock1, typename Clock2>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock1> start,
  time_base on_dur, time_base off_dur,
  typename std::chrono::time_point<Clock2> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    while(Clock2::now()+on_dur+off_dur < stop) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count()
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}


template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(time_base start,
  time_base on_dur, time_base off_dur,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    while(Clock::now()+on_dur+off_dur < stop) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start)
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock> start,
  time_base on_dur, time_base off_dur,
  time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now() + stop;

    while(std::chrono::steady_clock::now()+on_dur+off_dur < later) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count()
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << to_string(stop);
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(time_base start,
  time_base on_dur, time_base off_dur,
  time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now()+stop;

    while(std::chrono::steady_clock::now()+on_dur+off_dur < later) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start)
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << to_string(stop);
  _ascii_str = out.str();
}


template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  typename std::chrono::time_point<Clock> start,
  time_base on_dur, time_base off_dur)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    while(true) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << start.time_since_epoch().count()
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]";
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(time_base start,
  time_base on_dur, time_base off_dur, bool)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    while(true) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << to_string(start)
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]";
  _ascii_str = out.str();
}

template<typename TimeBaseT>
template<typename Clock>
builtin_trigger<TimeBaseT>::builtin_trigger(
  time_base on_dur, time_base off_dur,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    while(Clock::now()+on_dur+off_dur < stop) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename TimeBaseT>
builtin_trigger<TimeBaseT>::builtin_trigger(bool,
  time_base on_dur, time_base off_dur,
  time_base stop)
    :expansion_board(trigger_type::intermittent,
      trigger_type::none)
{
  _run = [=](void) {
    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now()+stop;

    while(std::chrono::steady_clock::now()+on_dur+off_dur < later) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << system_prefix << " "
    << "["
    << to_string(on_dur) << ":" << to_string(off_dur)
    << "]"
    << to_string(stop);
  _ascii_str = out.str();
}

template<typename TimeBaseT>
std::string builtin_trigger<TimeBaseT>::to_string(time_base dur)
{
  std::chrono::hours::rep hours =
    std::chrono::duration_cast<std::chrono::hours>(dur).count();
  std::chrono::minutes::rep min =
    std::chrono::duration_cast<std::chrono::minutes>(
      dur % std::chrono::hours(1)).count();
  std::chrono::seconds::rep sec =
    std::chrono::duration_cast<std::chrono::seconds>(
      dur % std::chrono::minutes(1)).count();
  std::chrono::milliseconds::rep ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(
      dur % std::chrono::seconds(1)).count();
  std::chrono::microseconds::rep us =
    std::chrono::duration_cast<std::chrono::microseconds>(
      dur % std::chrono::milliseconds(1)).count();
  std::chrono::nanoseconds::rep ns =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      dur % std::chrono::microseconds(1)).count();

  std::stringstream out;
    if(hours)
      out << hours << "h";
    if(min)
      out << min << "m";
    if(sec)
      out << sec << "s";
    if(ms)
      out << ms << "ms";
    if(us)
      out << us << "us";
    if(ns)
      out << ns << "ns";

  return out.str();
}












template<typename CharT>
std::pair<typename std::chrono::system_clock::time_point,bool>
parse_timespec(const std::basic_string<CharT> &raw_str)
{
  static const char * timespec_fmt = "%Y-%m-%d %H:%M:%S";

  typedef std::chrono::system_clock clock_type;

  std::pair<typename clock_type::time_point,bool> results{{},false};

  std::tm _tm;
  std::istringstream in(raw_str);
  in >> std::get_time(&_tm, timespec_fmt);
  if(!in.fail() && in.eof()) {
    results.second = true;

    std::time_t when_t = std::mktime(&_tm);
    if(when_t < 0) {
      throw std::runtime_error("given calendar time cannot be "
        "represented as a system time since epoch");
    }

    results.first = clock_type::from_time_t(when_t);
  }

  return results;
}

template<typename CharT>
std::pair<std::chrono::nanoseconds,bool>
parse_durspec(const std::basic_string<CharT> &raw_str)
{
  /*
    uses backreferences so doesn't work
   (?:(?:^([[:digit:]]+)[hH])|(?:(?<!^):([[:digit:]]+)[hH]))?
   (?:(?:^([[:digit:]]+)[mM](?![sS]))|(?:(?<!^):([[:digit:]]+)[mM](?![sS])))?
   (?:(?:^([[:digit:]]+)[sS])|(?:(?<!^):([[:digit:]]+)[sS]))?
   (?:(?:^([[:digit:]]+)m[sS])|(?:(?<!^):([[:digit:]]+)m[sS]))?
   (?:(?:^([[:digit:]]+)u[sS])|(?:(?<!^):([[:digit:]]+)u[sS]))?
   (?:(?:^([[:digit:]]+)n[sS])|(?:(?<!^):([[:digit:]]+)n[sS]))?
  */

  static const std::regex expr(
    "(?:([[:digit:]]+)[hH])?"
    "(?:([[:digit:]]+)[mM](?![sS]))?"
    "(?:([[:digit:]]+)[sS])?"
    "(?:([[:digit:]]+)(?:ms|MS))?"
    "(?:([[:digit:]]+)(?:us|US))?"
    "(?:([[:digit:]]+)(?:ns|NS))?");

  std::pair<std::chrono::nanoseconds,bool> results =
    {std::chrono::nanoseconds::zero(),false};

  std::smatch match;
  if(std::regex_match(raw_str,match,expr)) {
    try {
// std::cerr << "Matches: \n";
// for(std::size_t i=0; i<match.size(); ++i)
//   std::cerr << "  " << i << " '" << match[i] << "'\n";
      if(match[1].length())
        results.first += std::chrono::hours(std::stoull(match[1]));
      if(match[2].length())
        results.first += std::chrono::minutes(std::stoull(match[2]));
      if(match[3].length())
        results.first += std::chrono::seconds(std::stoull(match[3]));
      if(match[4].length())
        results.first += std::chrono::milliseconds(std::stoull(match[4]));
      if(match[5].length())
        results.first += std::chrono::microseconds(std::stoull(match[5]));
      if(match[6].length())
        results.first += std::chrono::nanoseconds(std::stoull(match[6]));

      results.second = true;
    }
    catch (...) {
      // if throw then error in validation regex
      abort();
    }
  }

  return results;
}


/*
  Format is:

  20s^5s_3s@40s
  20s[20s:30s]40s
  20s[]20s

  There appears to be no really elegant way of doing this unfortunately...

*/
template<typename CharT>
std::shared_ptr<expansion_board>
make_builtin_trigger(const std::basic_string<CharT> &raw_str)
{
  typedef std::basic_string<CharT> string_type;
  typedef std::chrono::nanoseconds time_base;
  typedef builtin_trigger<time_base> builtin_trigger_type;

  using std::chrono::system_clock;
  using std::chrono::steady_clock;

  string_type start_raw;
  string_type on_raw;
  string_type off_raw;
  string_type stop_raw;

  // First break down the specification string...

  auto open_bracket = std::find(raw_str.begin(),raw_str.end(),'[');

  start_raw.assign(raw_str.begin(),open_bracket);
  if(open_bracket != raw_str.end()) {
    auto close_bracket = std::find(open_bracket,raw_str.end(),']');

    if(close_bracket == raw_str.end()) {
      std::stringstream err;
      err << "Missing builtin trigger interval closing ']' in '"
        << raw_str << "'";
      throw std::runtime_error(err.str());
    }

    // process open/close
    if(std::distance(open_bracket,close_bracket) > 1) {
      auto on_start = open_bracket+1;
      auto colon = std::find(on_start,close_bracket,':');
      if(colon == on_start) {
        std::stringstream err;
        err << "Missing builtin trigger interval on value in '"
          << raw_str << "'";
        throw std::runtime_error(err.str());
      }

      if(colon == close_bracket) { // not found
        std::stringstream err;
        err << "Missing builtin trigger interval on specifier ':' in '"
          << raw_str << "'";
        throw std::runtime_error(err.str());
      }

      on_raw.assign(on_start,colon);

      auto off_start = colon+1;

      if(off_start == close_bracket) {
        std::stringstream err;
        err << "Missing builtin trigger interval off value in '"
          << raw_str << "'";
        throw std::runtime_error(err.str());
      }

      off_raw.assign(off_start,close_bracket);
    }

    stop_raw.assign(++close_bracket,raw_str.end());
  }


  // Now parse the actual string values


  std::pair<system_clock::time_point,bool> start_timespec{{},false};
  std::pair<time_base,bool> start_durspec{{},false};
  std::pair<time_base,bool> on_durspec{{},false};
  std::pair<time_base,bool> off_durspec{{},false};
  std::pair<system_clock::time_point,bool> stop_timespec{{},false};
  std::pair<time_base,bool> stop_durspec{{},false};


  if(!start_raw.empty()) {
    start_timespec = parse_timespec(start_raw);

    if(!start_timespec.second) {
      start_durspec = parse_durspec(start_raw);

      if(!start_durspec.second) {
        std::stringstream err;
        err << "Invalid builtin trigger start specification in '"
          << start_raw << "'";
        throw std::runtime_error(err.str());
      }
    }
  }

  if(!on_raw.empty()) {
    on_durspec = parse_durspec(on_raw);
    if(!on_durspec.second || on_durspec.first == time_base::zero()) {
      std::stringstream err;
      err << "Invalid builtin trigger interval on value in '"
        << on_raw << "'";
      throw std::runtime_error(err.str());
    }
  }

  if(!off_raw.empty()) {
    off_durspec = parse_durspec(off_raw);
    if(!off_durspec.second || off_durspec.first == time_base::zero()) {
      std::stringstream err;
      err << "Invalid builtin trigger interval off value in '"
        << off_raw << "'";
      throw std::runtime_error(err.str());
    }
  }

  if(!stop_raw.empty()) {
    stop_timespec = parse_timespec(stop_raw);

    if(!stop_timespec.second) {
      stop_durspec = parse_durspec(stop_raw);

      if(!stop_durspec.second) {
        std::stringstream err;
        err << "Invalid builtin trigger start specification in '"
          << stop_raw << "'";
        throw std::runtime_error(err.str());
      }
    }
  }

  if(
      (start_timespec.second && stop_timespec.second &&
        stop_timespec.first <= start_timespec.first)
    ||
      (start_durspec.second && stop_timespec.second &&
        std::chrono::system_clock::now()+start_durspec.first <=
          stop_timespec.first)
    )
  {
    std::stringstream err;
    err << "Builtin trigger stop time must be greater than the start time";
    throw std::runtime_error(err.str());
  }

  /*
  std::cerr << "start_raw: " << start_raw << "\n"
    << "  start_timespec: " << start_timespec.first.time_since_epoch().count()
      << " -> " << start_timespec.second << "\n"
    << "  start_durspec: " << start_durspec.first.count() << "ns -> "
      << start_durspec.second << "\n";

  std::cerr << "on_raw: " << on_raw << "\n"
    << "  on_durspec: " << on_durspec.first.count() << "ns -> "
      << on_durspec.second << "\n";

  std::cerr << "off_raw: " << off_raw << "\n"
    << "  off_durspec: " << off_durspec.first.count() << "ns -> "
      << off_durspec.second << "\n";


  std::cerr << "stop_raw: " << stop_raw << "\n"
    << "  stop_timespec: " << stop_timespec.first.time_since_epoch().count()
      << " -> " << stop_timespec.second << "\n"
    << "  stop_durspec: " << stop_durspec.first.count() << "ns -> "
      << stop_durspec.second << "\n";
  */

  /*
    convenience breakdown. Possible cases are

    time_start,time_stop
    time_start,dur_stop
    time_start,no_stop

    dur_start,time_stop
    dur_start,dur_stop
    dur_start,no_stop

    no_start,time_stop
    no_start,dur_stop
    no_start,no_stop --- invalid
  */
  bool time_start = (start_timespec.second && !start_durspec.second);
  bool dur_start = (!start_timespec.second && start_durspec.second);
  bool no_start = (!start_timespec.second && !start_durspec.second);

  bool time_stop = (stop_timespec.second && !stop_durspec.second);
  bool dur_stop = (!stop_timespec.second && stop_durspec.second);
  bool no_stop = (!stop_timespec.second && !stop_durspec.second);

  if(no_start && no_stop) {
    // failed error checking logic. Should never get here
    abort();
  }
  else if(time_start && time_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_timespec.first,on_durspec.first,off_durspec.first,
        stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_timespec.first,stop_timespec.first));
  }
  else if(time_start && dur_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_timespec.first,on_durspec.first,off_durspec.first,
        stop_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_timespec.first,stop_durspec.first));
  }
  else if(time_start && no_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_timespec.first,on_durspec.first,off_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_timespec.first,0));
  }
  else if(dur_start && time_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_durspec.first,on_durspec.first,off_durspec.first,
        stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_durspec.first,stop_timespec.first));
  }
  else if(dur_start && dur_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_durspec.first,on_durspec.first,off_durspec.first,
        stop_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_durspec.first,stop_durspec.first));
  }
  else if(dur_start && no_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        start_durspec.first,on_durspec.first,off_durspec.first,0));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      start_durspec.first,0));
  }
  else if(no_start && time_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new builtin_trigger_type(
        on_durspec.first,off_durspec.first,stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      0,stop_timespec.first));
  }

  // else if(no_start && dur_stop)...
  if(on_durspec.second) {
    return std::shared_ptr<expansion_board>(new builtin_trigger_type(
      0,on_durspec.first,off_durspec.first,stop_durspec.first));
  }

  return std::shared_ptr<expansion_board>(new builtin_trigger_type(
    0,stop_durspec.first));
}


#endif
