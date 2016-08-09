/*
    Board particulars for the waveshare ADS1256 expansion board
 */

#ifndef WAVESHARE_ADS1256_EXPANSION_H
#define WAVESHARE_ADS1256_EXPANSION_H


#include "ADC_board.h"
#include "bcm2835_sentry.h"

#include <boost/program_options.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tuple>
#include <cstdint>
#include <vector>
#include <atomic>

namespace waveshare {

class waveshare_ADS1256 :public ADC_board {
  public:
    waveshare_ADS1256(const po::variables_map &vm);

    virtual void configure_options(void);
    virtual void setup_com(void);
    virtual void initialize(void);

    virtual void trigger_sampling(const data_handler &handler);

    virtual void trigger_sampling_async(const data_handler &handler);

    virtual std::uint32_t bit_depth(void) const;

    virtual bool ADC_counts_signed(void) const;

    virtual bool ADC_counts_big_endian(void) const;

    virtual rational_type sensitivity(void) const;

    virtual std::uint32_t enabled_channels(void) const;

    virtual bool disabled(void) const;

  private:
    typedef std::vector<char> sample_buffer_type;
    typedef std::shared_ptr<sample_buffer_type> sample_buffer_ptr;
    typedef b::lockfree::spsc_queue<sample_buffer_ptr> ringbuffer_type;

    static const std::size_t row_block = 1024;

    bool _disabled;

    unsigned char sample_rate;
    unsigned char _gain_code;
    std::uint32_t _gain;
    rational_type _Vref;
    double aincom;

    std::vector<char> channel_assignment;
    std::vector<int> used_pins;

    bool prepped_initial_channel;

    // In this order...
    std::shared_ptr<bcm2835_sentry> bcm2835lib_sentry;
    std::shared_ptr<bcm2835_SPI_sentry> bcm2835SPI_sentry;

    void validate_assign_channel(const std::string config_str);

    void async_handler(ringbuffer_type &allocation_ringbuffer,
      ringbuffer_type &ready_ringbuffer,const data_handler &handler,
      std::atomic<bool> &done);
};

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

inline waveshare_ADS1256::rational_type
waveshare_ADS1256::sensitivity(void) const
{
  // sensitivity = 1/(2^23-1) * FSR/gain
  return (_Vref*2)*rational_type(1,8388607 * _gain);
}

inline std::uint32_t waveshare_ADS1256::enabled_channels(void) const
{
  return channel_assignment.size();
}

inline bool waveshare_ADS1256::disabled(void) const
{
  return _disabled;
}




}

#endif
