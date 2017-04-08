#ifndef TRIGGERPI_BUILTIN_LOG_TRIGGER_H
#define TRIGGERPI_BUILTIN_LOG_TRIGGER_H

#include "expansion_board.h"

#include "time_util.h"

#include <string>
#include <chrono>

class log_trigger :public expansion_board {
  public:
    log_trigger(void)
      :expansion_board(trigger_type_t::multi_sink) {}

      virtual void run(void) {
        std::chrono::steady_clock::time_point begin =
          std::chrono::steady_clock::now();

        while(wait_on_trigger_start()) {
          std::chrono::steady_clock::time_point start =
            std::chrono::steady_clock::now();

          std::cerr << "trigger start at: +" << to_string(start-begin) << "\n";

          wait_on_trigger_stop();

          std::chrono::steady_clock::time_point stop =
            std::chrono::steady_clock::now();

          std::cerr << "trigger stop at:  +" << to_string(stop-begin)
            << " (" << to_string(stop-start) << ")\n";
        }
      }

      virtual std::string system_identifier(void) const {
        return "log_trigger";
      }

      virtual std::string system_description(void) const {
        return system_identifier();
      }

  private:
};

#endif
