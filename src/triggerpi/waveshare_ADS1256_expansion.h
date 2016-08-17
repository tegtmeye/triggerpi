/*
    Board particulars for the waveshare ADS1256 expansion board
 */

#ifndef WAVESHARE_ADS1256_EXPANSION_H
#define WAVESHARE_ADS1256_EXPANSION_H


#include "ADC_board.h"
#include "bcm2835_sentry.h"

#include "basic_screen_printer.h"
#include "basic_file_printer.h"


#include <boost/program_options.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tuple>
#include <cstdint>
#include <vector>
#include <atomic>

namespace waveshare {

class waveshare_ADS1256 :public ADC_board {
  public:
    typedef basic_screen_printer<std::uint32_t,true,3> screen_printer_type;
    typedef basic_file_printer<std::uint32_t,true,3> file_printer_type;
    typedef do_nothing_handler null_consumer_type;

    waveshare_ADS1256(const po::variables_map &vm);

    virtual void configure_options(void);
    virtual void setup_com(void);
    virtual void initialize(void);

    virtual void trigger_sampling(const data_handler &handler,
      basic_trigger &trigger);

    virtual rational_type row_sampling_rate(void) const;

    virtual std::uint32_t bit_depth(void) const;

    virtual bool ADC_counts_signed(void) const;

    virtual bool ADC_counts_big_endian(void) const;

    virtual rational_type sensitivity(void) const;

    virtual std::uint32_t enabled_channels(void) const;

    virtual bool sample_time_prefix(void) const;

    virtual bool disabled(void) const;

    virtual data_handler screen_printer(void) const;

    virtual data_handler file_printer(const fs::path &loc) const;

    virtual data_handler null_consumer(void) const;

  private:
    typedef std::vector<char> sample_buffer_type;
    typedef std::shared_ptr<sample_buffer_type> sample_buffer_ptr;
    typedef b::lockfree::spsc_queue<sample_buffer_ptr> ringbuffer_type;

    std::size_t row_block;

    unsigned char _sample_rate_code;
    rational_type _row_sampling_rate;
    unsigned char _gain_code;
    std::uint32_t _gain;
    rational_type _Vref;
    double aincom;

    std::vector<char> channel_assignment;
    std::vector<int> used_pins;

    bool _disabled;
    bool _sample_time_prefix;
    bool _async;


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
      std::atomic<bool> &done);
};

inline ADC_board::rational_type
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

inline ADC_board::rational_type
waveshare_ADS1256::sensitivity(void) const
{
  // sensitivity = 1/(2^23-1) * FSR/gain
  return (_Vref*2)*rational_type(1,8388607 * _gain);
}

inline std::uint32_t waveshare_ADS1256::enabled_channels(void) const
{
  return channel_assignment.size();
}

inline bool waveshare_ADS1256::sample_time_prefix(void) const
{
  return true;;
}

inline bool waveshare_ADS1256::disabled(void) const
{
  return _disabled;
}

inline waveshare_ADS1256::data_handler
waveshare_ADS1256::screen_printer(void) const
{
  return screen_printer_type();
}

inline waveshare_ADS1256::data_handler
waveshare_ADS1256::file_printer(const fs::path &loc) const
{
  return file_printer_type(loc,enabled_channels());
}

inline waveshare_ADS1256::data_handler
waveshare_ADS1256::null_consumer(void) const
{
  return null_consumer_type();
}



}

#endif
