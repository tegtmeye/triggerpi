/*
    Board particulars for the waveshare ADS1256 expansion board
 */

#ifndef WAVESHARE_ADS1256_EXPANSION_H
#define WAVESHARE_ADS1256_EXPANSION_H


#include "ADC_board.h"
#include "bcm2835_sentry.h"

#include <boost/program_options.hpp>

#include <tuple>
#include <cstdint>
#include <vector>

namespace waveshare {

class waveshare_ADS1256 :public ADC_board {
  public:
    waveshare_ADS1256(const po::variables_map &vm);

    virtual void configure_options(void);
    virtual void setup_com(void);
    virtual void initialize(void);

    virtual void trigger_sampling(
      const ADC_board::sample_callback_type &callback, std::size_t samples);

    virtual unsigned int bit_depth(void);

    virtual bool ADC_counts_signed(void);

    virtual std::tuple<std::uint_fast32_t,uint_fast32_t> sensitivity(void);

    virtual unsigned int enabled_channels(void);

    virtual bool disabled(void) const;

  private:
    typedef std::vector<uint32_t> sample_buffer_type;

    bool _disabled;

    unsigned char sample_rate;
    unsigned char gain;
    double aincom;

    std::vector<char> channel_assignment;
    std::vector<int> used_pins;

    sample_buffer_type sample_buffer;
    bool prepped_initial_channel;


    // In this order...
    std::shared_ptr<bcm2835_sentry> bcm2835lib_sentry;
    std::shared_ptr<bcm2835_SPI_sentry> bcm2835SPI_sentry;

    void validate_assign_channel(const std::string config_str);
};

inline bool waveshare_ADS1256::disabled(void) const
{
  return _disabled;
}

inline unsigned int waveshare_ADS1256::bit_depth(void)
{
  return 24;
}

inline bool waveshare_ADS1256::ADC_counts_signed(void)
{
  return true;
}

inline std::tuple<std::uint_fast32_t,uint_fast32_t>
waveshare_ADS1256::sensitivity(void)
{
  // Vref is assumed to be exactly 2.5 (which it is likely not)
  //2*Vref/(2^23-1*gain)
  return std::make_tuple(std::uint_fast32_t(5),
    std::uint_fast32_t(8388607*gain));
}

inline unsigned int waveshare_ADS1256::enabled_channels(void)
{
  return channel_assignment.size();
}


}

#endif
