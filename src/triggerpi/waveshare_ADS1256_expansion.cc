/*
    Setup the SPI interface using the bcm2835 library
 */

#include "waveshare_ADS1256_expansion.h"

#include <bcm2835.h>

//CS    -----   SPICS
//DIN   -----   MOSI
//DOUT  -----   MISO
//SCLK  -----   SCLK
//DRDY  -----   ctl_IO     data  starting
//RST   -----   ctl_IO     reset

// DRDY   RPI_GPIO_P1_11
// RST    RPI_GPIO_P1_12
// SPICS  RPI_GPIO_P1_15

namespace waveshare {


void waveshare_ADS1256::setup_com(void)
{
  // MSBFIRST is the only supported BIT order according to the bcm2835 library
  // and appears to be the preferred order according to the ADS1255/6 datasheet
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);

  // not clear why mode 1 is being used. Need to clarify with datasheet
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);

  // 1024 = 4.096us = 244.140625kHz from 250MHz system clock
  // not sure why this was chosen
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024);

  // select SPI interface 0 for ADS1256 ADC
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);

  // Need to confirm with datasheet that this is LOW
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

  // set RPI_GPIO_P1_15 as output and set it high
  bcm2835_gpio_fsel(RPI_GPIO_P1_15, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_write(RPI_GPIO_P1_15, HIGH);

  // set RPI_GPIO_P1_11 ans INPUT and set as pull-up resistor
  bcm2835_gpio_fsel(RPI_GPIO_P1_11, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_set_pud(RPI_GPIO_P1_11, BCM2835_GPIO_PUD_UP);
}

// void waveshare_ADS1256::initialize(void)
// {
//
// }



}
