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


class timed_trigger : public expansion_board {
  public:
    static const char * time_fmt(void) {
      return "%Y-%m-%d %H:%M:%S";
    }

    timed_trigger(std::chrono::nanoseconds duration, const std::tm &when)
      :expansion_board(trigger_type::intermittent,trigger_type::none),
        _duration(duration), _when(when) {}

    virtual void run(void) {
      std::cerr << "timed_trigger running\n";
      trigger_start();
      std::cerr << "timed trigger exiting\n";
    }

    virtual std::string system_description(void) const {
      std::stringstream out;
      out << "built-in trigger: " << _duration.count() << "ns @ "
        << std::put_time(&_when,time_fmt());
      return out.str();
    }

    std::chrono::nanoseconds duration(void) const;

    const std::tm & when(void) const;

  private:
    std::chrono::nanoseconds _duration;
    std::tm _when;
};

inline std::chrono::nanoseconds timed_trigger::duration(void) const
{
  return _duration;
}

inline const std::tm & timed_trigger::when(void) const
{
  return _when;
}

#endif
