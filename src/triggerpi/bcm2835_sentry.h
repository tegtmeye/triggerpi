/**
    Sentry class to ensure setup and disposal of bcm2835 library
 */

#include <bcm2835.h>

#include <stdexcept>

class bcm2835_sentry {
  public:
    bcm2835_sentry(void) {
      if(!init_count) {
        if(!bcm2835_init())
          throw std::runtime_error("bcm2835_init failed");
        ++init_count;
      }
    }

    ~bcm2835_sentry(void) {
      if(!init_count)
        throw std::logic_error("unmatched bcm2835_close");

      --init_count;
      if(!bcm2835_close())
        throw std::runtime_error("bcm2835_close failed");
    }

  private:
    static std::size_t init_count;
};

std::size_t bcm2835_sentry::init_count = 0;
