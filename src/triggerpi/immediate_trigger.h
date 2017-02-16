/*
  Immediate trigger that simply triggers the sinks as soon as it is run

*/

#ifndef TRIGGERPI_IMMEDIATE_TRIGGER
#define TRIGGERPI_IMMEDIATE_TRIGGER

#include <config.h>

#include "expansion_board.h"

#include <boost/program_options.hpp>


class immediate_trigger : public expansion_board {
  public:
    virtual void run(void) {
      trigger_start();
    }

    virtual std::string system_description(void) const {
      return "built-in immediate trigger";
    }
};



#endif
