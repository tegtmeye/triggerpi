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
      if(!init_count()) {
        if(!bcm2835_close())
          throw std::runtime_error("bcm2835_close failed");
      }
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

    Includes some statefull information to get around limitations of using
    BCM2835 library
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
      if(!init_count())
        bcm2835_spi_end();
    }

    /*
        Range of clock dividers for supported divices. This call will 'and' the
        range with the range currently set. Future calls will 'and' this value
        with future values. This is a cooperative method. That is, each device
        should set the SPI clock divider to the maximum speed that it can
        support based on the return value. Later, the device can poll the
        current value but determining the highest value set. If the result is
        zero, then there is no clock divider that all devices can cooperatively
        support.
    */
    std::uint16_t clock_divider_range(std::uint16_t range = 0xFFFF);

    /*
        Convenience function to get the max clock divider currently in
        clock_divider_range
    */
    std::uint16_t max_clock_divider(void);

  private:
    std::size_t & init_count(void) {
      static std::size_t value=0;

      return value;
    }
};

inline std::uint16_t
bcm2835_SPI_sentry::clock_divider_range(std::uint16_t range)
{
  static std::uint16_t current_range = 0xFFFF;

  return (current_range & range);
}

inline std::uint16_t bcm2835_SPI_sentry::max_clock_divider(void)
{
  std::uint16_t cur = clock_divider_range();

  while(cur)
    cur >>= 2;

  return cur;
}

#endif
