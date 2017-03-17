/*
  Immediate trigger that simply triggers the sinks as soon as it is run

*/

#ifndef TRIGGERPI_TIMED_TRIGGER
#define TRIGGERPI_TIMED_TRIGGER

#include "expansion_board.h"

#include <ctime>
#include <chrono>
#include <string>
#include <iomanip>
#include <sstream>
#include <thread>
#include <regex>

#include <unistd.h> // for pause


#if 0
class trigger_base {
  public:
    virtual ~trigger_base(void) {}

    virtual void wait(void) const = 0;

    virtual const std::string & ascii_str(void) const = 0;
};


class immediate_trigger : public trigger_base {
  public:
    immediate_trigger(void) :_ascii_str("_immediate_") {}

    virtual void wait(void) const {}

    virtual const std::string & ascii_str(void) const {
      static const std::string _ascii_str("_immediate_");
      return _ascii_str;
    }

  private:
    std::string _ascii_str;
};

class indefinite_trigger : public trigger_base {
  public:
    indefinite_trigger(void) :_ascii_str("_indefinite_") {}

    virtual void wait(void) const {
      typedef std::chrono::system_clock::time_point time_point_type;

      std::this_thread::sleep_until(time_point_type::max());
    }

    virtual const std::string & ascii_str(void) const {
      return _ascii_str;
    }

  private:
    std::string _ascii_str;
};


class time_trigger : public trigger_base {
  public:
    time_trigger(const std::tm &const_when) {
      std::tm when = const_when;
      std::time_t when_t = std::mktime(&when);
      if(when_t < 0) {
        throw std::runtime_error("given calendar time cannot be "
          "represented as a system time since epoch");
      }

      _when = std::chrono::system_clock::from_time_t(when_t);

      std::stringstream out;
      out << std::put_time(&const_when,time_fmt);
      _ascii_str = out.str();
    }

    virtual void wait(void) const {
      std::this_thread::sleep_until(_when);
    }

    virtual const std::string & ascii_str(void) const {
      return _ascii_str;
    }

  private:
    std::chrono::system_clock::time_point _when;

    std::string _ascii_str;
};


class duration_trigger : public trigger_base {
  public:
    duration_trigger(std::chrono::nanoseconds duration) :_duration(duration) {
      _ascii_str = std::to_string(duration.count()) + "ns";
    }

    virtual void wait(void) const {
      std::this_thread::sleep_for(_duration);
    }

    virtual const std::string & ascii_str(void) const {
      return _ascii_str;
    }

  private:
    std::chrono::nanoseconds _duration;

    std::string _ascii_str;
};


class builtin_trigger :public expansion_board {
  public:
    builtin_trigger(const std::shared_ptr<trigger_base> &start,
      const std::shared_ptr<trigger_base> &stop)
        :expansion_board(trigger_type::intermittent,trigger_type::none),
          _start(start), _stop(stop)
    {
      _ascii_str = _start->ascii_str() + "@" + _stop->ascii_str();
    }

    virtual void run(void) {
      std::cerr << "timed_trigger running and waiting until trigger\n";

      _start->wait();

      std::cerr << "timed_trigger started\n";

      trigger_start();

      std::cerr << "timed_trigger waiting to stop trigger\n";

      _stop->wait();

      trigger_stop();

      std::cerr << "timed trigger exiting\n";
    }


    virtual std::string system_description(void) const {
      return _ascii_str;
    }

  private:
    std::shared_ptr<trigger_base> _start;
    std::shared_ptr<trigger_base> _stop;

    std::string _ascii_str;
};

#endif







    static const char * time_fmt = "%Y-%m-%d %H:%M:%S";




/*
  trigger start only happens if stop is not reached
  always a trigger stop.
*/
class basic_builtin_trigger :public expansion_board {
  public:
    // trigger constantly between \c start and \c stop
    template<typename Clock1, typename Clock2>
    basic_builtin_trigger(typename std::chrono::time_point<Clock1> start,
      typename std::chrono::time_point<Clock2> stop);

    template<typename Clock>
    basic_builtin_trigger(std::chrono::nanoseconds start,
      typename std::chrono::time_point<Clock> stop);

    template<typename Clock>
    basic_builtin_trigger(typename std::chrono::time_point<Clock> start,
      std::chrono::nanoseconds stop);

    basic_builtin_trigger(std::chrono::nanoseconds start,
      std::chrono::nanoseconds stop);

    // trigger constantly starting at \c start and continuing indefinitely
    template<typename Clock>
    basic_builtin_trigger(typename std::chrono::time_point<Clock> start, bool);

    basic_builtin_trigger(std::chrono::nanoseconds start, bool);


    // trigger constantly starting immediately and stopping at \c stop
    template<typename Clock>
    basic_builtin_trigger(bool, typename std::chrono::time_point<Clock> stop);

    basic_builtin_trigger(bool, std::chrono::nanoseconds stop);

    /*
      cycle on for \c on_dur and off for \c off_dur starting at
      \const_start and stopping at \c const_stop. Each interval will not
      be less that \c on_dur + \c off_dur long which means that if the
      last cycle cannot be full length, it will not be triggered
    */
    template<typename Clock1, typename Clock2>
    basic_builtin_trigger(typename std::chrono::time_point<Clock1> start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
      typename std::chrono::time_point<Clock2> stop);

    template<typename Clock>
    basic_builtin_trigger(std::chrono::nanoseconds start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
      typename std::chrono::time_point<Clock> stop);

    template<typename Clock>
    basic_builtin_trigger(typename std::chrono::time_point<Clock> start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
      std::chrono::nanoseconds stop);

    basic_builtin_trigger(std::chrono::nanoseconds start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
      std::chrono::nanoseconds stop);

    /*
      cycle on for \c on_dur and off for \c off_dur starting at
      \const_start and continuing indefinitely.
    */
    template<typename Clock>
    basic_builtin_trigger(typename std::chrono::time_point<Clock> start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur);

    basic_builtin_trigger(std::chrono::nanoseconds start,
      std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur, bool);

    /*
      cycle on for \c on_dur and off for \c off_dur starting immediately
      and stopping at \c const_stop. Each interval will not be less that
      \c on_dur + \c off_dur long which means that if the last cycle
      cannot be full length, it will not be triggered
    */
    template<typename Clock>
    basic_builtin_trigger(std::chrono::nanoseconds on_dur,
      std::chrono::nanoseconds off_dur,
      typename std::chrono::time_point<Clock> stop);

    basic_builtin_trigger(bool, std::chrono::nanoseconds on_dur,
      std::chrono::nanoseconds off_dur, std::chrono::nanoseconds stop);

    virtual void run(void) {
      _run();
    }


    virtual std::string system_description(void) const {
      return _ascii_str;
    }

  private:
    std::function<void(void)> _run;

    std::string _ascii_str;
};

template<typename Clock1, typename Clock2>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock1> start,
  typename std::chrono::time_point<Clock2> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << start.time_since_epoch().count() << "[]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  std::chrono::nanoseconds start, typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << start.count() << "ns[]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock> start, std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    std::this_thread::sleep_until(std::chrono::steady_clock::now()+stop);


    trigger_stop();
  };

  std::stringstream out;
  out << start.time_since_epoch().count() << "[]"
    << stop.count() << "ns";
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(std::chrono::nanoseconds start,
  std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now()+stop;

    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    std::this_thread::sleep_until(later);

    trigger_stop();
  };

  std::stringstream out;
  out << start.count() << "ns[]" << stop.count() << "ns";
  _ascii_str = out.str();
}


template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock> start, bool)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(start);

    trigger_start();

    pause();
  };

  std::stringstream out;
  out << start.time_since_epoch().count() << "[]";
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(
  std::chrono::nanoseconds start, bool)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    trigger_start();

    pause();
  };

  std::stringstream out;
  out << start.count() << "ns[]";
  _ascii_str = out.str();
}



template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(bool,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    trigger_start();

    std::this_thread::sleep_until(stop);

    trigger_stop();
  };

  std::stringstream out;
  out << "[]" << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(
  bool, std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    trigger_start();

    std::this_thread::sleep_until(std::chrono::steady_clock::now()+stop);

    trigger_stop();
  };

  std::stringstream out;
  out << "[]" << stop.count() << "ns";
  _ascii_str = out.str();
}

template<typename Clock1, typename Clock2>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock1> start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  typename std::chrono::time_point<Clock2> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out << start.time_since_epoch().count()
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}


template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(std::chrono::nanoseconds start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out << start.count() << "ns"
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock> start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now() + stop;

    std::this_thread::sleep_until(start);

    while(std::chrono::steady_clock::now()+on_dur+off_dur < later) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << start.time_since_epoch().count()
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.count() << "ns";
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(std::chrono::nanoseconds start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
{
  _run = [=](void) {
    std::chrono::steady_clock::time_point later =
      std::chrono::steady_clock::now()+stop;

    std::this_thread::sleep_until(std::chrono::steady_clock::now()+start);

    while(std::chrono::steady_clock::now()+on_dur+off_dur < later) {
      trigger_start();
      std::this_thread::sleep_for(on_dur);

      trigger_stop();

      std::this_thread::sleep_for(off_dur);
    }
  };

  std::stringstream out;
  out << start.count() << "ns"
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.count() << "ns";
  _ascii_str = out.str();
}


template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  typename std::chrono::time_point<Clock> start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out << start.time_since_epoch().count()
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]";
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(std::chrono::nanoseconds start,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur, bool)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out << start.count() << "ns"
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]";
  _ascii_str = out.str();
}

template<typename Clock>
basic_builtin_trigger::basic_builtin_trigger(
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  typename std::chrono::time_point<Clock> stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.time_since_epoch().count();
  _ascii_str = out.str();
}

basic_builtin_trigger::basic_builtin_trigger(bool,
  std::chrono::nanoseconds on_dur, std::chrono::nanoseconds off_dur,
  std::chrono::nanoseconds stop)
    :expansion_board(trigger_type::intermittent,trigger_type::none)
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
  out
    << "["
    << on_dur.count() << "ns:" << off_dur.count() << "ns"
    << "]"
    << stop.count() << "ns";
  _ascii_str = out.str();
}














template<typename CharT>
std::pair<typename std::chrono::system_clock::time_point,bool>
parse_timespec(const std::basic_string<CharT> &raw_str)
{
  typedef std::chrono::system_clock clock_type;

  std::pair<typename clock_type::time_point,bool> results{{},false};

  std::tm _tm;
  std::istringstream in(raw_str);
  in >> std::get_time(&_tm, "%Y-%m-%d %H:%M:%S");
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

(([[:digit:]]+)[hH])?(([[:digit:]]+)[mM](?![sS]))?(([[:digit:]]+)[sS])?(([[:digit:]]+)m[sS])?(([[:digit:]]+)u[sS])?(([[:digit:]]+)n[sS])?
  */
//   static const std::regex expr("^"
//     "(([[:digit:]]+)[hH])?"
//     "(:([[:digit:]]+)[mM])?"
//     "(:([[:digit:]]+)[sS])?"
//     "(:([[:digit:]]+)(ms|MS))?"
//     "(:([[:digit:]]+)(us|US))?"
//     "(:([[:digit:]]+)(ns|NS))?");

  static const std::regex expr(
    "(([[:digit:]]+)[hH])?"
    "(([[:digit:]]+)[mM](?![sS]))?"
    "(([[:digit:]]+)[sS])?"
    "(([[:digit:]]+)m[sS])?"
    "(([[:digit:]]+)u[sS])?"
    "(([[:digit:]]+)n[sS])?");

  std::pair<std::chrono::nanoseconds,bool> results{{},false};

  std::smatch match;
  if(std::regex_match(raw_str,match,expr)) {
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
  std::pair<std::chrono::nanoseconds,bool> start_durspec{{},false};
  std::pair<std::chrono::nanoseconds,bool> on_durspec{{},false};
  std::pair<std::chrono::nanoseconds,bool> off_durspec{{},false};
  std::pair<system_clock::time_point,bool> stop_timespec{{},false};
  std::pair<std::chrono::nanoseconds,bool> stop_durspec{{},false};


  if(!start_raw.empty()) {
    start_timespec = parse_timespec(start_raw);

    if(!start_timespec.second) {
      std::pair<std::chrono::nanoseconds,bool> durspec =
        parse_durspec(start_raw);

      if(!durspec.second || durspec.first == std::chrono::nanoseconds::zero()) {
        std::stringstream err;
        err << "Invalid builtin trigger start specification in '"
          << start_raw << "'";
        throw std::runtime_error(err.str());
      }

      start_durspec.second = true;
    }
  }

  if(!on_raw.empty()) {
    on_durspec = parse_durspec(on_raw);
    if(!on_durspec.second) {
      std::stringstream err;
      err << "Invalid builtin trigger interval on value in '"
        << on_raw << "'";
      throw std::runtime_error(err.str());
    }
  }

  if(!off_raw.empty()) {
    off_durspec = parse_durspec(off_raw);
    if(!off_durspec.second) {
      std::stringstream err;
      err << "Invalid builtin trigger interval off value in '"
        << off_raw << "'";
      throw std::runtime_error(err.str());
    }
  }

  if(!stop_raw.empty()) {
    stop_timespec = parse_timespec(stop_raw);

    if(!stop_timespec.second) {
      std::pair<std::chrono::nanoseconds,bool> durspec =
        parse_durspec(stop_raw);

      if(!durspec.second || durspec.first == std::chrono::nanoseconds::zero()) {
        std::stringstream err;
        err << "Invalid builtin trigger stop specification in '"
          << start_raw << "'";
        throw std::runtime_error(err.str());
      }

      stop_durspec.second = true;
    }
  }

  if(
      (start_timespec.second && stop_timespec.second &&
        stop_timespec.first <= start_timespec.first)
    ||
      (start_durspec.second && stop_durspec.second &&
        stop_durspec.first <= start_durspec.first)
    )
  {
    std::stringstream err;
    err << "Builtin trigger stop time must be greater than the start time";
    throw std::runtime_error(err.str());
  }

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
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_timespec.first,on_durspec.first,off_durspec.first,
        stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_timespec.first,stop_timespec.first));
  }
  else if(time_start && dur_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_timespec.first,on_durspec.first,off_durspec.first,
        stop_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_timespec.first,stop_durspec.first));
  }
  else if(time_start && no_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_timespec.first,on_durspec.first,off_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_timespec.first,0));
  }
  else if(dur_start && time_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_durspec.first,on_durspec.first,off_durspec.first,
        stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_durspec.first,stop_timespec.first));
  }
  else if(dur_start && dur_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_durspec.first,on_durspec.first,off_durspec.first,
        stop_durspec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_durspec.first,stop_durspec.first));
  }
  else if(dur_start && no_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        start_durspec.first,on_durspec.first,off_durspec.first,0));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      start_durspec.first,0));
  }
  else if(no_start && time_stop) {
    if(on_durspec.second) {
      return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
        on_durspec.first,off_durspec.first,stop_timespec.first));
    }

    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      0,stop_timespec.first));
  }

  // else if(no_start && dur_stop)...
  if(on_durspec.second) {
    return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
      0,on_durspec.first,off_durspec.first,stop_durspec.first));
  }

  return std::shared_ptr<expansion_board>(new basic_builtin_trigger(
    0,stop_durspec.first));
}


#endif
