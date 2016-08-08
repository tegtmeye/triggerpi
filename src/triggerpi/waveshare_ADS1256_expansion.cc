/*
    Setup the SPI interface using the bcm2835 library
 */

#include "waveshare_ADS1256_expansion.h"

#include <bcm2835.h>

// for htonl
#include <arpa/inet.h>

#include <boost/utility/binary.hpp>
#include <boost/lexical_cast.hpp>

#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

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

// integer power function for base 10. Modified from:
// http://stackoverflow.com/questions/101439/the-most-efficient-way-to-implement-an-integer-based-power-function-powint-int
// Does not check for overflow or unsigned values
template<typename T>
T ipow10(T exp)
{
    T result = 1;
    T base = 10;
    while (exp) {
      if (exp & 1)
        result *= base;
      exp >>= 1;
      base *= base;
    }

    return result;
}

unsigned char validate_translate_sample_rate(const po::variables_map &vm)
{
  // 30000 is the default sample rate
  unsigned char result = BOOST_BINARY(11110000);

  if(vm.count("ADC.waveshare.sample_rate")) {
    const std::string &sample_rate =
      vm["ADC.waveshare.sample_rate"].as<std::string>();

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

std::tuple<unsigned char,std::uint32_t>
validate_translate_gain(const po::variables_map &vm)
{
  // 1 is the default gain value
  std::tuple<unsigned char,std::uint32_t> result;

  if(vm.count("ADC.waveshare.gain")) {
    const std::string &gain = vm["ADC.waveshare.gain"].as<std::string>();

    // Don't be overly clever. Just map to datasheet.
    if(gain == "1")
      result = std::make_tuple(BOOST_BINARY(0),1);
    else if(gain == "2")
      result = std::make_tuple(BOOST_BINARY(001),2);
    else if(gain == "4")
      result = std::make_tuple(BOOST_BINARY(010),4);
    else if(gain == "8")
      result = std::make_tuple(BOOST_BINARY(011),8);
    else if(gain == "16")
      result = std::make_tuple(BOOST_BINARY(100),16);
    else if(gain == "32")
      result = std::make_tuple(BOOST_BINARY(101),32);
    else if(gain == "64")
      result = std::make_tuple(BOOST_BINARY(110),64);
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

ADC_board::rational_type validate_translate_Vref(const std::string Vref_str)
{
  typedef ADC_board::rational_type::int_type rint_type;

  static const std::regex digits_regex("[0-9]*");
  static const ADC_board::rational_type min_vref(5,10);
  static const ADC_board::rational_type max_vref(26,10);

  rint_type integral = 0;
  rint_type decimal = 0;
  rint_type factor = 1;

  bool missing_integral = false;
  std::stringstream str(Vref_str);
  try {
    for(std::size_t i=0; str.good(); ++i) {
      std::string substr;
      std::getline(str,substr,'.');

      if(i == 0) {
        if(substr.empty()) {
          missing_integral = true;
          continue;
        }

        if(!std::regex_match(substr,digits_regex))
          throw std::runtime_error(Vref_str);

        try {
          // although stoull accepts a lot more formats that would interfere
          // here, we've limited the string to just decimal digits. Use stoull
          // for overflow protection
          std::size_t pos = 0;
          integral = std::stoull(substr,&pos);
          if(pos != substr.size())
            throw std::runtime_error(substr.substr(pos));
        }
        catch(const std::invalid_argument &ex) {
          throw std::runtime_error(substr);
        }
        catch(const std::out_of_range &ex) {
          std::stringstream err;
          err << substr << " (overflow)";
          throw std::runtime_error(err.str());
        }
      }
      else if(i == 1) {
        if(substr.empty()) {
          if(missing_integral)
            throw std::runtime_error(".");

          continue;
        }

        if(!std::regex_match(substr,digits_regex))
          throw std::runtime_error(Vref_str);

        try {
          // although stoull accepts a lot more formats that would interfere
          // here, we've limited the string to just decimal digits. Use stoull
          // for overflow protection
          std::size_t pos = 0;
          decimal = std::stoull(substr,&pos);
          if(pos != substr.size())
            throw std::runtime_error(substr.substr(pos));
        }
        catch(const std::invalid_argument &ex) {
          throw std::runtime_error(substr);
        }
        catch(const std::out_of_range &ex) {
          std::stringstream err;
          err << substr << " (overflow)";
          throw std::runtime_error(err.str());
        }

        factor = ipow10<rint_type>(substr.size());
      }
      else
        throw std::runtime_error(substr);
    }
  }
  catch(const std::runtime_error &ex) {
    std::stringstream err;
    err << "Unexpected value '" << ex.what() << "' in waveshare Vref config";
    throw std::runtime_error(err.str());
  }

  ADC_board::rational_type result(integral*factor+decimal,factor);

  if(result < min_vref || max_vref < result) {
    std::stringstream err;
    err << "Invalid Vref. Values for the ADS1256 is: "
      "0.5 < Vref < 2.6 volts. Given " << result << " volts.";
    throw std::runtime_error(err.str());
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
  used_pins(9,0)
{
  std::tie(_gain_code,_gain) = detail::validate_translate_gain(vm);
}


void waveshare_ADS1256::configure_options(void)
{
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

  if(!_vm.count("ADC.waveshare.Vref"))
    throw std::runtime_error("Missing Waveshare reference voltage");

  _Vref = detail::validate_translate_Vref(_vm["ADC.waveshare.Vref"].as<std::string>());

  std::cerr << "VREF= " << _Vref << "\n";
}


void waveshare_ADS1256::setup_com(void)
{
  // delay initialization of these so that we can check configuration options
  // first
  bcm2835lib_sentry.reset(new bcm2835_sentry());
  bcm2835SPI_sentry.reset(new bcm2835_SPI_sentry());

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
  regs[2] = (0 << 5) | (0 << 2) | _gain_code;

  //DRATE: A/D Data Rate (Address 03h)
  //  Set the sample rate
  regs[3] = sample_rate;

  detail::write_to_registers(REG_STATUS,regs,4);

  // ADC should now start to auto-cal, DRDY goes low when done
  detail::wait_DRDY();

}


void waveshare_ADS1256::trigger_sampling(
  const ADC_board::sample_callback_type &callback, std::size_t samples)
{
  if(!prepped_initial_channel) {
    // set the MUX to the first channel so that the conversion will be the
    // first one read. After a sampling cycle has taken place, the ADC will
    // already have the correct channel loaded in MUX
    detail::wait_DRDY();

    // switch to the first channel
    detail::write_to_registers(REG_MUX,&(channel_assignment[0]),1);
    bcm2835_delayMicroseconds(5);

    CS_0();
    bcm2835_spi_transfer(CMD_SYNC);
    CS_1();
    bcm2835_delayMicroseconds(5);

    CS_0();
    bcm2835_spi_transfer(CMD_WAKEUP);
    CS_1();
    bcm2835_delayMicroseconds(25);

    // wait?
    CS_0();
    bcm2835_spi_transfer(CMD_RDATA);
    bcm2835_delayMicroseconds(10);

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
      bcm2835_delayMicroseconds(5);

      CS_0();
      bcm2835_spi_transfer(CMD_SYNC);
      CS_1();
      bcm2835_delayMicroseconds(5);

      CS_0();
      bcm2835_spi_transfer(CMD_WAKEUP);
      CS_1();
      bcm2835_delayMicroseconds(25);

      CS_0();
      bcm2835_spi_transfer(CMD_RDATA);
      bcm2835_delayMicroseconds(10);

      union {
        char buff[4];
        int32_t ADC_counts = 0;
      };
      bcm2835_spi_transfern(buff,3);
      CS_1();

      // Data comes out of the ADC MSB first---or big endian. Convert to
      // int32_t by placing the first 24 bits into the high bits,
      // then convert to host endian by dividing by 8. We do this for
      // portability and to take advantage of the hardware automatically
      // extending the sign bit during division. For example, say the 24-bit
      // value is (7F)(FF)(FF) which is positive. We want (in little endian)
      // (FF)(FF)(7F)(00). So place the 24-bits into the high-bits in
      // big-endian format. So our 4-byte integer or int32_t in big-endian is:
      // (7F)(FF)(FF)(00). Next call ntohl which swaps the bytes into the host's
      // endian. If the host is little endian, it becomes: (00)(FF)(FF)(7F).
      // Now shift the bites by 8 or one whole byte which is the same as
      // dividing by 8. Now our number is (FF)(FF)(7F)(00). The advantage here
      // is that if the number was negative, the division operator would
      // automatically extend the signbits for 2's complement. If this code
      // was run on a big-endian machine, the ntohl is a no-op and the division
      // still applies.
      sample_buffer[idx] = (static_cast<int32_t>(ntohl(ADC_counts))>>8);
    }

    done = callback(sample_buffer.data(),enabled_channels(),samples);
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
