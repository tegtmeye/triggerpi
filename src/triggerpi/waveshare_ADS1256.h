/*
    Board particulars for the waveshare ADS1256/DAC8532 expansion board
 */

#ifndef WAVESHARE_EXPANSION_H
#define WAVESHARE_EXPANSION_H


#include "ADC_board.h"

// #include "basic_screen_printer.h"
// #include "basic_file_printer.h"

#include <bcm2835.h>


#include <boost/program_options.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tuple>
#include <cstdint>
#include <vector>
#include <atomic>

namespace po = boost::program_options;

namespace waveshare {

/**
    Sentry class to ensure setup and disposal of bcm2835 library.

    Singleton class
 */
class bcm2835_sentry {
  public:
    bcm2835_sentry(void) {
      if(did_init())
        throw std::logic_error("Multiple initializations of bcm2835 library");
      if(bcm2835_init())
        throw std::runtime_error("bcm2835_init failed");
      if(!bcm2835_spi_begin())
        throw std::runtime_error("bcm2835_spi_begin failed");

      did_init() = true;
    }

    ~bcm2835_sentry(void) {
      if(!did_init())
        throw std::logic_error("unmatched bcm2835_close");

      bcm2835_spi_end();

      if(!bcm2835_close())
        throw std::runtime_error("bcm2835_close failed");

      did_init() = false;
    }

  private:
    bool & did_init(void) {
      static bool value = false;

      return value;
    }
};

class waveshare_ADS1256 :public ADC_board {
  public:
//     typedef basic_screen_printer<std::uint32_t,true,3> screen_printer_type;
//     typedef basic_file_printer<std::uint32_t,true,3> file_printer_type;
//     typedef do_nothing_handler null_consumer_type;

    // required expansion_factory functions
    static po::options_description cmd_options(void);
    static std::string system_config_name(void);
    static std::string system_config_desc_short(void);
    static std::string system_config_desc_long(void);

    waveshare_ADS1256(const po::variables_map &vm);

    // Expansion board overrides
    virtual void configure_options(const po::variables_map &vm);
    virtual void setup_com(void);
    virtual void initialize(void);

    virtual void start(void) {}


    // Start sampling the ADC. The callback will be called with the first
    // argument a pointer of a n dimensional array of data, the second
    // argument is this object. The n dimensional array type and size is
    // determined by the state variables in this object. That is, if
    // this->bit_depth() = 24, then the data is laid out as a 24 bit number
    // same for this->ADC_counts_signed(). If seconds is less than zero
    // then sample continusously
    virtual void trigger_sampling(const data_handler &handler,
      basic_trigger &trigger);

    virtual void finalize(void);

    virtual std::string system_description(void) const;

    // ADC_board overrides

    virtual rational_type row_sampling_rate(void) const;

    virtual std::uint32_t bit_depth(void) const;

    virtual bool ADC_counts_signed(void) const;

    virtual bool ADC_counts_big_endian(void) const;

    virtual rational_type sensitivity(void) const;

    virtual std::uint32_t enabled_channels(void) const;

    virtual bool stats(void) const;

    virtual bool disabled(void) const;
#if 0
    virtual data_handler screen_printer(void) const;

    virtual data_handler file_printer(const fs::path &loc) const;

    virtual data_handler null_consumer(void) const;
#endif

  private:
    typedef std::vector<char> sample_buffer_type;
    typedef std::shared_ptr<sample_buffer_type> sample_buffer_ptr;
    typedef b::lockfree::spsc_queue<sample_buffer_ptr> ringbuffer_type;

    static bool register_config(void);
    static const bool did_register_config;

    std::size_t row_block;

    unsigned char _sample_rate_code;
    rational_type _row_sampling_rate;
    unsigned char _gain_code;
    std::uint32_t _gain;
    rational_type _Vref;
    double aincom;
    bool buffer_enabled;

    std::vector<char> channel_assignment;
    std::vector<int> used_pins;

    bool _async;
    bool _stats;


    std::shared_ptr<bcm2835_sentry> bcm2835lib_sentry;

    void validate_assign_channel(const std::string config_str, bool verbose);
#if 0
    void trigger_sampling_impl(const data_handler &handler,
      basic_trigger &trigger);
    void trigger_sampling_wstat_impl(const data_handler &handler,
      basic_trigger &trigger);

    void trigger_sampling_async_impl(const data_handler &handler,
      basic_trigger &trigger);
    void trigger_sampling_async_wstat_impl(const data_handler &handler,
      basic_trigger &trigger);

    void async_handler(ringbuffer_type &allocation_ringbuffer,
      ringbuffer_type &ready_ringbuffer,const data_handler &handler,
      std::atomic_int &done);
#endif
};

inline bool waveshare_ADS1256::register_config(void)
{
  expansion_board::register_expansion(expansion_factory<waveshare_ADS1256>());

  return true;
}

inline std::string waveshare_ADS1256::system_config_name(void)
{
  return "waveshare_ADC";
}

inline std::string waveshare_ADS1256::system_config_desc_short(void)
{
  return "Waveshare High-Precision ADC Board";
}

inline std::string waveshare_ADS1256::system_config_desc_long(void)
{
  return "Waveshare High-Precision ADC Board based on the ADS1256";
}



inline std::string waveshare_ADS1256::system_description(void) const
{
  return system_config_desc_short();
}

inline expansion_board::rational_type
waveshare_ADS1256::row_sampling_rate(void) const
{
  return _row_sampling_rate;
}

inline std::uint32_t waveshare_ADS1256::bit_depth(void) const
{
  return 24;
}

inline bool waveshare_ADS1256::ADC_counts_signed(void) const
{
  return true;
}

inline bool waveshare_ADS1256::ADC_counts_big_endian(void) const
{
  return true;
}

inline expansion_board::rational_type
waveshare_ADS1256::sensitivity(void) const
{
  // sensitivity = 1/(2^23-1) * FSR/gain
  return (_Vref*2)*rational_type(1,8388607 * _gain);
}

inline std::uint32_t waveshare_ADS1256::enabled_channels(void) const
{
  return channel_assignment.size();
}

inline bool waveshare_ADS1256::stats(void) const
{
  return _stats;
}

inline bool waveshare_ADS1256::disabled(void) const
{
  return channel_assignment.empty();
}



#if 0
inline waveshare_ADS1256::data_handler
waveshare_ADS1256::screen_printer(void) const
{
  return screen_printer_type(*this);
}

inline waveshare_ADS1256::data_handler
waveshare_ADS1256::file_printer(const fs::path &loc) const
{
  return file_printer_type(loc,*this);
}

inline waveshare_ADS1256::data_handler
waveshare_ADS1256::null_consumer(void) const
{
  return null_consumer_type();
}
#endif


}

#endif
