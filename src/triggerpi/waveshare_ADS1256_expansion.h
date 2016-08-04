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

    virtual std::uint32_t bit_depth(void) const;

    virtual bool ADC_counts_signed(void) const;

    virtual std::tuple<std::uint64_t,std::uint64_t> sensitivity(void) const;

    virtual std::uint32_t enabled_channels(void) const;

    virtual bool disabled(void) const;

  private:
    typedef std::vector<uint32_t> sample_buffer_type;

    bool _disabled;

    unsigned char sample_rate;
    unsigned char _gain_code;
    std::uint32_t _gain;
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

inline std::uint32_t waveshare_ADS1256::bit_depth(void) const
{
  return 24;
}

inline bool waveshare_ADS1256::ADC_counts_signed(void) const
{
  return true;
}

inline std::tuple<std::uint64_t,std::uint64_t>
waveshare_ADS1256::sensitivity(void) const
{
  // Vref is assumed to be exactly 2.5 (which it is likely not)
  // sensitivity = 1/(2^23-1) * FSR/gain
  std::uint64_t num = 5;
  std::uint64_t denom = 8388607*_gain;
  // doesn't appear to be an advantage to factoring, not close to overflowing
  // can change though if a 32-bit ADC with big gains.
  // std::uint64_t factor = boost::math::gcd(num,denom);

  return std::make_tuple(num,denom);
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
