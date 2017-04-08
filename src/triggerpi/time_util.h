#ifndef TRIGGERPI_TIME_UTIL_H
#define TRIGGERPI_TIME_UTIL_H

#include <chrono>
#include <string>

// convert to reduced string. ie lots of ns -> h,m,s,...
template<typename TimeBaseT>
std::string to_string(TimeBaseT dur)
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



#endif
