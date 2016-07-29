/*
    Board particulars for the waveshare ADS1256 expansion board
 */

#ifndef WAVESHARE_ADS1256_EXPANSION_H
#define WAVESHARE_ADS1256_EXPANSION_H


#include "ADC_board.h"
#include "bcm2835_sentry.h"

namespace waveshare {

class waveshare_ADS1256 :public ADC_board {
  public:
    virtual void setup_com(void);
//     virtual void initialize(void);

  private:
    // In this order...
    bcm2835_sentry bcm2835lib_sentry;
    bcm2835_SPI_sentry bcm2835SPI_sentry;
};




}

#endif
