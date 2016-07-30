#ifndef BCM2835_SENTRY_H
#define BCM2835_SENTRY_H


#include <bcm2835.h>

#include <stdexcept>

/**
    Sentry class to ensure setup and disposal of bcm2835 library.

    Singleton class
 */
class bcm2835_sentry {
  public:
    bcm2835_sentry(void) {
      if(!init_count()) {
        if(!bcm2835_init())
          throw std::runtime_error("bcm2835_init failed");
        ++init_count();
      }
    }

    ~bcm2835_sentry(void) {
      if(!init_count())
        throw std::logic_error("unmatched bcm2835_close");

      --init_count();
      if(!bcm2835_close())
        throw std::runtime_error("bcm2835_close failed");
    }

  private:
    std::size_t & init_count(void) {
      static std::size_t value=0;

      return value;
    }
};



/**
    Sentry class to enable and disable SPI interface of bcm2835 library

    Singleton class
 */
class bcm2835_SPI_sentry {
  public:
    bcm2835_SPI_sentry(void) {
      if(!init_count()) {
        if(!bcm2835_spi_begin())
          throw std::runtime_error("bcm2835_spi_begin failed");
        ++init_count();
      }
    }

    ~bcm2835_SPI_sentry(void) {
      if(!init_count())
        throw std::logic_error("unmatched bcm2835_spi_end()");

      --init_count();
      bcm2835_spi_end();
    }

  private:
    std::size_t & init_count(void) {
      static std::size_t value=0;

      return value;
    }
};


#endif
