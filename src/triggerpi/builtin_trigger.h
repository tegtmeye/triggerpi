/*
  Trigger source that can be started stopped and oscillate
*/

#ifndef TRIGGERPI_BUILTIN_TRIGGER_H
#define TRIGGERPI_BUILTIN_TRIGGER_H

#include "builtin_log_trigger.h"
#include "builtin_time_trigger.h"

template<typename CharT>
std::shared_ptr<expansion_board>
make_builtin_trigger(const std::basic_string<CharT> &raw_str)
{
  if(raw_str == "log_trigger")
    return std::make_shared<log_trigger>();

  return make_builtin_time_trigger(raw_str);
}

#endif
