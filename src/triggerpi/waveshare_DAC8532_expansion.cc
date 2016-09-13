#include <config.h>

#include "waveshare_DAC8532_expansion.h"
#include "bits.h"

#include <bcm2835.h>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif

//CS      -----   SPICS
//DIN     -----   MOSI
//DOUT    -----   MISO
//SCLK    -----   SCLK
//DRDY    -----   ctl_IO     data  starting
//RST     -----   ctl_IO     reset

#define	SPICS	RPI_GPIO_P1_16	//P4

#define CS_1() bcm2835_gpio_write(SPICS,HIGH)
#define CS_0()  bcm2835_gpio_write(SPICS,LOW)


namespace waveshare {

waveshare_DAC8532::waveshare_DAC8532(const po::variables_map &vm)
 :DAC_board(vm)
{
}


void waveshare_DAC8532::configure_options(void)
{
#if 0
  // Set up channels. If there are no channels, then nothing to do.
  if(_vm.count("ADC.waveshare.channel")) {
    const std::vector<std::string> &channel_vec =
      _vm["ADC.waveshare.channel"].as< std::vector<std::string> >();

    for(std::size_t i=0; i<channel_vec.size(); ++i)
      validate_assign_channel(channel_vec[i]);
  }

  if(channel_assignment.empty())
    _disabled = true;

  if(!_vm.count("ADC.waveshare.AINCOM"))
    throw std::runtime_error("Missing Waveshare AINCOM value");

  aincom = _vm["ADC.waveshare.AINCOM"].as<double>();

  buffer_enabled = _vm["ADC.waveshare.buffered"].as<bool>();

  if(!_vm.count("ADC.waveshare.Vref"))
    throw std::runtime_error("Missing Waveshare reference voltage");

  _Vref = detail::validate_translate_Vref(
    _vm["ADC.waveshare.Vref"].as<std::string>());

  _async = (_vm.count("async") && _vm["async"].as<bool>());

  _stats = (_vm.count("stats") && _vm["stats"].as<bool>());

  if(_vm.count("ADC.waveshare.sampleblocks"))
    row_block = _vm["ADC.waveshare.sampleblocks"].as<std::size_t>();
  else if(_async)
    row_block = 1024;
  else
    row_block = 10;

  if(!row_block)
    throw std::runtime_error("ADC.waveshare.sampleblocks must be a positive "
      "integer");
#endif
}


void waveshare_DAC8532::setup_com(void)
{
  // delay initialization of these so that we can check configuration options
  // first
  bcm2835lib_sentry.reset(new bcm2835_sentry());
  bcm2835SPI_sentry.reset(new bcm2835_SPI_sentry());

  // MSBFIRST is the only supported BIT order according to the bcm2835 library
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);

  // not clear why mode 1 is being used. Need to clarify with datasheet
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);

  // Datasheet for DAC8532 states that maximum SCLK frequency is 30MHz for a
  // 5V VDD. For a 250 MHz sytem clock, a clock divider of 8 yields a
  // SPI frequency of 31.25 MHz and a divider of 16 yields a SPI frequency of
  // 15.625 MHz
  bcm2835SPI_sentry->clock_divider_range((32768 | 16384 | 8192 | 4096 | 2048 |
    1024 | 512 | 256 | 128 | 64 | 32 | 16));

  assert(bcm2835SPI_sentry->clock_divider_range());

  bcm2835_spi_setClockDivider(bcm2835SPI_sentry->max_clock_divider());

  // set RPI_GPIO_P1_16 as output and set it high
  // Waveshare board does not use the normal SPI CS lines but rather uses
  // GPIO 16 for the DAC slave select requiring manual asserting
  bcm2835_gpio_fsel(RPI_GPIO_P1_16, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_write(RPI_GPIO_P1_16, HIGH);
}

void waveshare_DAC8532::initialize(void)
{
#if 0
  // probably should force reset first

  char regs[4] = {0};

  //STATUS : STATUS REGISTER (ADDRESS 00h);
  //  MSB first, enable auto-cal, set input buffer
  regs[0] = (0 << 3) | (1 << 2) | (buffer_enabled << 1);

  //MUX : Input Multiplexer Control Register (Address 01h)
  regs[1] = 0x08; // for now

  //ADCON: A/D Control Register (Address 02h)
  //  Turn off clock out
  regs[2] = (0 << 5) | (0 << 2) | _gain_code;

  //DRATE: A/D Data Rate (Address 03h)
  //  Set the sample rate
  regs[3] = _sample_rate_code;

  detail::write_to_registers(REG_STATUS,regs,4);

  // ADC should now start to auto-cal, DRDY goes low when done
  detail::wait_DRDY();
#endif
}

}
