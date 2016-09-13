/*
    Board particulars for the waveshare ADS1256/DAC8532 expansion board
 */

#ifndef WAVESHARE_EXPANSION_H
#define WAVESHARE_EXPANSION_H


#include "expansion_board.h"
#include "bcm2835_sentry.h"

#include "basic_screen_printer.h"
#include "basic_file_printer.h"


#include <boost/program_options.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tuple>
#include <cstdint>
#include <vector>
#include <atomic>

namespace po = boost::program_options;

namespace waveshare {

class waveshare_expansion :public expansion_board {
  public:
    typedef basic_screen_printer<std::uint32_t,true,3> screen_printer_type;
    typedef basic_file_printer<std::uint32_t,true,3> file_printer_type;
    typedef do_nothing_handler null_consumer_type;

    // required expansion_factory functions
    static po::options_description cmd_options(void);
    static std::string system_config_name(void);
    static std::string system_config_desc_short(void);
    static std::string system_config_desc_long(void);

    waveshare_expansion(const po::variables_map &vm);

    virtual void configure_options(void);
    virtual void setup_com(void);
    virtual void initialize(void);

    virtual void trigger_sampling(const data_handler &handler,
      basic_trigger &trigger);

    virtual std::string system_description(void) const;

    virtual rational_type row_sampling_rate(void) const;

    virtual std::uint32_t bit_depth(void) const;

    virtual bool ADC_counts_signed(void) const;

    virtual bool ADC_counts_big_endian(void) const;

    virtual rational_type sensitivity(void) const;

    virtual std::uint32_t enabled_channels(void) const;

    virtual bool stats(void) const;

    virtual bool disabled(void) const;

    virtual data_handler screen_printer(void) const;

    virtual data_handler file_printer(const fs::path &loc) const;

    virtual data_handler null_consumer(void) const;

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


    // In this order...
    std::shared_ptr<bcm2835_sentry> bcm2835lib_sentry;
    std::shared_ptr<bcm2835_SPI_sentry> bcm2835SPI_sentry;

    void validate_assign_channel(const std::string config_str);

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
};

inline bool waveshare_expansion::register_config(void)
{
  expansion_board::register_expansion(expansion_factory<waveshare_expansion>());

  return true;
}

inline std::string waveshare_expansion::system_config_name(void)
{
  return "waveshare";
}

inline std::string waveshare_expansion::system_config_desc_short(void)
{
  return "Waveshare High-Precision AD/DA Board";
}

inline std::string waveshare_expansion::system_config_desc_long(void)
{
  return "Waveshare High-Precision AD/DA Board based on the ADS1256 and "
    "DAC8532";
}



inline std::string waveshare_expansion::system_description(void) const
{
  return system_config_desc_short();
}

inline expansion_board::rational_type
waveshare_expansion::row_sampling_rate(void) const
{
  return _row_sampling_rate;
}

inline std::uint32_t waveshare_expansion::bit_depth(void) const
{
  return 24;
}

inline bool waveshare_expansion::ADC_counts_signed(void) const
{
  return true;
}

inline bool waveshare_expansion::ADC_counts_big_endian(void) const
{
  return true;
}

inline expansion_board::rational_type
waveshare_expansion::sensitivity(void) const
{
  // sensitivity = 1/(2^23-1) * FSR/gain
  return (_Vref*2)*rational_type(1,8388607 * _gain);
}

inline std::uint32_t waveshare_expansion::enabled_channels(void) const
{
  return channel_assignment.size();
}

inline bool waveshare_expansion::stats(void) const
{
  return _stats;
}

inline bool waveshare_expansion::disabled(void) const
{
  return channel_assignment.empty();
}

inline waveshare_expansion::data_handler
waveshare_expansion::screen_printer(void) const
{
  return screen_printer_type(*this);
}

inline waveshare_expansion::data_handler
waveshare_expansion::file_printer(const fs::path &loc) const
{
  return file_printer_type(loc,*this);
}

inline waveshare_expansion::data_handler
waveshare_expansion::null_consumer(void) const
{
  return null_consumer_type();
}



}

#endif
