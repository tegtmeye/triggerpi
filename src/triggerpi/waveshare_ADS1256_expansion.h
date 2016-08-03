/*
    Board particulars for the waveshare ADS1256 expansion board
 */

#ifndef WAVESHARE_ADS1256_EXPANSION_H
#define WAVESHARE_ADS1256_EXPANSION_H


#include "ADC_board.h"
#include "bcm2835_sentry.h"

#include <boost/program_options.hpp>

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

    virtual unsigned int enabled_channels(void);

    virtual bool disabled(void) const;

  private:
    typedef std::vector<uint32_t> sample_buffer_type;

    bool _disabled;

    unsigned char sample_rate;
    unsigned char gain;

    std::vector<char> channel_assignment;
    std::vector<int> used_pins;

    sample_buffer_type sample_buffer;
    bool prepped_initial_channel;


    // In this order...
    bcm2835_sentry bcm2835lib_sentry;
    bcm2835_SPI_sentry bcm2835SPI_sentry;

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

inline unsigned int waveshare_ADS1256::enabled_channels(void)
{
  return channel_assignment.size();
}


}

#endif
