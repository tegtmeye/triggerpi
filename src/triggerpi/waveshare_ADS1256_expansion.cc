/*
    Setup the SPI interface using the bcm2835 library
 */

#include "waveshare_ADS1256_expansion.h"

#include <bcm2835.h>

#include <boost/utility/binary.hpp>
#include <boost/lexical_cast.hpp>

#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

//CS    -----   SPICS
//DIN   -----   MOSI
//DOUT  -----   MISO
//SCLK  -----   SCLK
//DRDY  -----   ctl_IO     data  starting
//RST   -----   ctl_IO     reset

#define  DRDY  RPI_GPIO_P1_11
#define  RST  RPI_GPIO_P1_12
#define	SPICS	RPI_GPIO_P1_15

#define CS_1() bcm2835_gpio_write(SPICS,HIGH)
#define CS_0()  bcm2835_gpio_write(SPICS,LOW)

#define DRDY_IS_LOW()	((bcm2835_gpio_lev(DRDY)==0))

#define RST_1() 	bcm2835_gpio_write(RST,HIGH);
#define RST_0() 	bcm2835_gpio_write(RST,LOW);

namespace b = boost;
namespace po = boost::program_options;


enum {
	/*Register address, followed by reset the default values */
	REG_STATUS = 0,	// x1H
	REG_MUX    = 1, // 01H
	REG_ADCON  = 2, // 20H
	REG_DRATE  = 3, // F0H
	REG_IO     = 4, // E0H
	REG_OFC0   = 5, // xxH
	REG_OFC1   = 6, // xxH
	REG_OFC2   = 7, // xxH
	REG_FSC0   = 8, // xxH
	REG_FSC1   = 9, // xxH
	REG_FSC2   = 10 // xxH
};

/* Command definition£∫ TTable 24. Command Definitions --- ADS1256 datasheet Page 34 */
enum {
	CMD_WAKEUP  = 0x00,	// Completes SYNC and Exits Standby Mode 0000  0000 (00h)
	CMD_RDATA   = 0x01, // Read Data 0000  0001 (01h)
	CMD_RDATAC  = 0x03, // Read Data Continuously 0000   0011 (03h)
	CMD_SDATAC  = 0x0F, // Stop Read Data Continuously 0000   1111 (0Fh)
	CMD_RREG    = 0x10, // Read from REG rrr 0001 rrrr (1xh)
	CMD_WREG    = 0x50, // Write to REG rrr 0101 rrrr (5xh)
	CMD_SELFCAL = 0xF0, // Offset and Gain Self-Calibration 1111    0000 (F0h)
	CMD_SELFOCAL= 0xF1, // Offset Self-Calibration 1111    0001 (F1h)
	CMD_SELFGCAL= 0xF2, // Gain Self-Calibration 1111    0010 (F2h)
	CMD_SYSOCAL = 0xF3, // System Offset Calibration 1111   0011 (F3h)
	CMD_SYSGCAL = 0xF4, // System Gain Calibration 1111    0100 (F4h)
	CMD_SYNC    = 0xFC, // Synchronize the A/D Conversion 1111   1100 (FCh)
	CMD_STANDBY = 0xFD, // Begin Standby Mode 1111   1101 (FDh)
	CMD_RESET   = 0xFE // Reset to Power-Up Values 1111   1110 (FEh)
};


namespace waveshare {

namespace detail {

unsigned char validate_translate_sample_rate(const po::variables_map &vm)
{
  // 30000 is the default sample rate
  unsigned char result = BOOST_BINARY(11110000);

  if(vm.count("ADC.sample_rate")) {
    const std::string &sample_rate = vm["ADC.sample_rate"].as<std::string>();

    // Don't be overly clever. Just map to datasheet.
    if(sample_rate == "30000")
      result = BOOST_BINARY(11110000);
    else if(sample_rate == "15000")
      result = BOOST_BINARY(11100000);
    else if(sample_rate == "7500")
      result = BOOST_BINARY(11010000);
    else if(sample_rate == "3750")
      result = BOOST_BINARY(11000000);
    else if(sample_rate == "2000")
      result = BOOST_BINARY(10110000);
    else if(sample_rate == "1000")
      result = BOOST_BINARY(10100001);
    else if(sample_rate == "500")
      result = BOOST_BINARY(10010010);
    else if(sample_rate == "100")
      result = BOOST_BINARY(10000010);
    else if(sample_rate == "60")
      result = BOOST_BINARY(01110010);
    else if(sample_rate == "50")
      result = BOOST_BINARY(01100011);
    else if(sample_rate == "30")
      result = BOOST_BINARY(01010011);
    else if(sample_rate == "25")
      result = BOOST_BINARY(01000011);
    else if(sample_rate == "15")
      result = BOOST_BINARY(00110011);
    else if(sample_rate == "10")
      result = BOOST_BINARY(00100011);
    else if(sample_rate == "5")
      result = BOOST_BINARY(00010011);
    else if(sample_rate == "2.5")
      result = BOOST_BINARY(00000011);
    else {
      std::stringstream err;
      err << "Invalid ADC.sample_rate setting for the Waveshare "
        "expansion board. Possible settings are: 30000, 15000, 7500, "
        "3750, 2000, 1000, 500, 100, 60, 50, 30, 25, 15, 10, 5, or 2.5 "
        "samples per second";
      throw std::runtime_error(err.str());
    }
  }

  return result;
}

unsigned char validate_translate_gain(const po::variables_map &vm)
{
  // 1 is the default gain value
  unsigned char result = 0;

  if(vm.count("ADC.gain")) {
    const std::string &gain = vm["ADC.gain"].as<std::string>();

    // Don't be overly clever. Just map to datasheet.
    if(gain == "1")
      result = BOOST_BINARY(0);
    else if(gain == "2")
      result = BOOST_BINARY(001);
    else if(gain == "4")
      result = BOOST_BINARY(010);
    else if(gain == "8")
      result = BOOST_BINARY(011);
    else if(gain == "16")
      result = BOOST_BINARY(100);
    else if(gain == "32")
      result = BOOST_BINARY(101);
    else if(gain == "64")
      result = BOOST_BINARY(110);
    else {
      std::stringstream err;
      err << "Invalid ADC.gain setting for the Waveshare "
        "expansion board. Possible settings are: 1, 2, 4, 8, 16, 32, or "
        "64";
      throw std::runtime_error(err.str());
    }
  }

  return result;
}


void wait_DRDY(void)
{
  // Wait forever.
  // Depending on what we are doing, this may make more sense as an interrupt
  for(std::size_t to=0; to<5; ++to) {
    for(std::size_t i=0; i<400000; ++i) {
      if(bcm2835_gpio_lev(DRDY)==0)
        return;
    }

    std::cerr << "Warning: ADS1256 timed out waiting on DRDY register\n";
  }

  // if here, abort
  throw std::runtime_error("Timed out waiting on ADS1256");
}

/*
  Write to num registers starting at \c reg_start. \c data is expected to be
  at least num bytes long
*/
void write_to_registers(uint8_t reg_start, char *data, uint8_t num)
{
  assert(reg_start <= 10 && num > 0);

  CS_0();

  // first nibble is the command, second is the register number
  bcm2835_spi_transfer(CMD_WREG | reg_start);
  // send the number of registers to write to - 1
  bcm2835_spi_transfer(num-1);

  bcm2835_spi_writenb(data,num);

  CS_1();
}


}


waveshare_ADS1256::waveshare_ADS1256(const po::variables_map &vm)
 :ADC_board(vm), _disabled(false),
  sample_rate(detail::validate_translate_sample_rate(vm)),
  gain(detail::validate_translate_gain(vm)),used_pins(9,0) {}


void waveshare_ADS1256::configure_options(void)
{
  // Set up channels first. If there are no channels, then nothing to do.
  if(_vm.count("ADC.channel")) {
    const std::vector<std::string> &channel_vec =
      _vm["ADC.channel"].as< std::vector<std::string> >();

    for(std::size_t i=0; i<channel_vec.size(); ++i)
      validate_assign_channel(channel_vec[i]);
  }

  if(channel_assignment.empty())
    _disabled = true;
}


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

void waveshare_ADS1256::initialize(void)
{
  // probably should force reset first

  char regs[4] = {0};

  //STATUS : STATUS REGISTER (ADDRESS 00h);
  //  MSB first, enable auto-cal, disable input buffer
  regs[0] = (0 << 3) | (1 << 2) | (0 << 1);

  //MUX : Input Multiplexer Control Register (Address 01h)
  regs[1] = 0x08; // for now

  //ADCON: A/D Control Register (Address 02h)
  //  Turn off clock out
  regs[2] = (0 << 5) | (0 << 2) | gain;

  //DRATE: A/D Data Rate (Address 03h)
  //  Set the sample rate
  regs[3] = sample_rate;

  detail::write_to_registers(REG_STATUS,regs,4);

  // ADC should now start to auto-cal, DRDY goes low when done
  detail::wait_DRDY();

}


void waveshare_ADS1256::trigger_sampling(
  const std::function<bool(void *)> &callback, std::size_t samples)
{
  if(!prepped_initial_channel) {
    // set the MUX to the first channel so that the conversion will be the
    // first one read. After a sampling cycle has taken place, the ADC will
    // already have the correct channel loaded in MUX
    detail::wait_DRDY();

    // switch to the jth channel
    detail::write_to_registers(REG_MUX,&(channel_assignment[0]),1);

    // wait?
    CS_0();
    bcm2835_spi_transfer(CMD_SYNC);
    CS_1();

    // wait?
    CS_0();
    bcm2835_spi_transfer(CMD_WAKEUP);
    CS_1();

    // wait?
    CS_0();
    bcm2835_spi_transfer(CMD_RDATA);

    //don't actually read the data yet
    prepped_initial_channel = true;
  }

  sample_buffer.resize(samples*channel_assignment.size());

  bool done = false;
  while(!done) {
    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    std::size_t channels = channel_assignment.size();
    for(std::size_t idx = 0; idx < sample_buffer.size(); ++idx) {
      detail::wait_DRDY();

      // switch to the next channel
      detail::write_to_registers(REG_MUX,
        &(channel_assignment[(idx+1)%channels]),1);

      // wait?
      CS_0();
      bcm2835_spi_transfer(CMD_SYNC);
      CS_1();

      // wait?
      CS_0();
      bcm2835_spi_transfer(CMD_WAKEUP);
      CS_1();

      // wait?
      CS_0();
      bcm2835_spi_transfer(CMD_RDATA);

      // wait?
      union {
        char buff[4];
        uint32_t ADC_counts;
      };
      bcm2835_spi_transfern(buff+1,3);

      sample_buffer[idx] = ADC_counts;

      CS_1();
    }

    done = callback(sample_buffer.data());
  }
}









void waveshare_ADS1256::validate_assign_channel(const std::string config_str)
{
  static const std::regex com_regex("com", std::regex_constants::icase);

  // pin numbers are 0-7 and 8=COM
  unsigned int pinA = 0;
  unsigned int pinB = 0;
  std::stringstream str(config_str);
  for(std::size_t i=0; str.good(); ++i) {
    std::string substr;
    std::getline(str,substr,',');

    if(i == 0) {
      if(std::regex_match(substr,com_regex))
        pinA = 8;
      else {
        try {
          pinA = b::lexical_cast<unsigned int>(substr);
        }
        catch(const boost::bad_lexical_cast &ex) {
          std::stringstream err;
          err << "'" << substr << "' is an invalid pin name";
          throw std::runtime_error(err.str());
        }

        if(pinA > 8) {
          std::stringstream err;
          err << "'" << substr << "' is an invalid pin. Pins must be "
            "labeled 1-8 or COM where COM is a synonym for 8";
          throw std::runtime_error(err.str());
        }
        if(used_pins[pinA]) {
          std::stringstream err;
          err << "'" << substr << "' has been used previously. Pins must be "
            "only used once in a channel assignment";
          throw std::runtime_error(err.str());
        }
      }
    }
    else if(i == 1) {
      if(std::regex_match(substr,com_regex))
        pinB = 8;
      else {
        try {
          pinB = b::lexical_cast<unsigned int>(substr);
        }
        catch(const boost::bad_lexical_cast &ex) {
          std::stringstream err;
          err << "'" << substr << "' is an invalid pin name";
          throw std::runtime_error(err.str());
        }

        if(pinB > 8) {
          std::stringstream err;
          err << "'" << substr << "' is an invalid pin. Pins must be "
            "labeled 1-8 or COM where COM is a synonym for 8";
          throw std::runtime_error(err.str());
        }
        if(used_pins[pinB] || pinB == pinA) {
          std::stringstream err;
          err << "'" << substr << "' has been used previously. Pins must be "
            "only used once in a channel assignment";
          throw std::runtime_error(err.str());
        }
      }
    }
    else {
      std::stringstream err;
      err << "unexpected '" << substr << "' in channel assignment. Channel "
        "directives should be of the form ADC.channel=pinA,pinB";
      throw std::runtime_error(err.str());
    }
  }

  // all is well, now do the assignment and log
  char MUX = (pinA << 4) | pinB;
  channel_assignment.push_back(MUX);
  used_pins.at(pinA) = true;
  used_pins.at(pinB) = true;
}



}
