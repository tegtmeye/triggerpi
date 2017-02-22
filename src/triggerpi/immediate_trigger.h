/*
  Immediate trigger that simply triggers the sinks as soon as it is run

*/

#ifndef TRIGGERPI_IMMEDIATE_TRIGGER
#define TRIGGERPI_IMMEDIATE_TRIGGER

#include "expansion_board.h"

#include <iostream>


class immediate_trigger : public expansion_board {
  public:
    immediate_trigger(void)
      :expansion_board(trigger_type::intermittent,trigger_type::none) {}

    virtual void run(void) {
      std::cerr << "immediate_trigger running\n";
      trigger_start();
      std::cerr << "immediate trigger exiting\n";
    }

    virtual std::string system_description(void) const {
      return "built-in immediate trigger";
    }

  private:
};

#endif
