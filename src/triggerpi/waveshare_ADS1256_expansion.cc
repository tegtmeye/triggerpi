#include <config.h>

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
#include <chrono>
#include <thread>
#include <functional>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif


//CS    -----   SPICS
//DIN   -----   MOSI
//DOUT  -----   MISO
//SCLK  -----   SCLK
//DRDY  -----   ctl_IO     data  starting
//RST   -----   ctl_IO     reset

#define DRDY  RPI_GPIO_P1_11
#define RST   RPI_GPIO_P1_12
#define PDWN  RPI_GPIO_P1_13
#define SPICS RPI_GPIO_P1_15

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

std::tuple<unsigned char,ADC_board::rational_type>
validate_translate_sample_rate(const po::variables_map &vm)
{
  // 30000 is the default sample rate
  unsigned char code_result = BOOST_BINARY(11110000);
  ADC_board::rational_type row_rate_result(30000,1);

  if(vm.count("ADC.waveshare.sample_rate")) {
    const std::string &sample_rate =
      vm["ADC.waveshare.sample_rate"].as<std::string>();

    // Don't be overly clever. Just map to datasheet.
    if(sample_rate == "30000") {
      code_result = BOOST_BINARY(11110000);
      row_rate_result = ADC_board::rational_type(30000,1);
    }
    else if(sample_rate == "15000") {
      code_result = BOOST_BINARY(11100000);
      row_rate_result = ADC_board::rational_type(15000,1);
    }
    else if(sample_rate == "7500") {
      code_result = BOOST_BINARY(11010000);
      row_rate_result = ADC_board::rational_type(7500,1);
    }
    else if(sample_rate == "3750") {
      code_result = BOOST_BINARY(11000000);
      row_rate_result = ADC_board::rational_type(3750,1);
    }
    else if(sample_rate == "2000") {
      code_result = BOOST_BINARY(10110000);
      row_rate_result = ADC_board::rational_type(2000,1);
    }
    else if(sample_rate == "1000") {
      code_result = BOOST_BINARY(10100001);
      row_rate_result = ADC_board::rational_type(1000,1);
    }
    else if(sample_rate == "500") {
      code_result = BOOST_BINARY(10010010);
      row_rate_result = ADC_board::rational_type(500,1);
    }
    else if(sample_rate == "100") {
      code_result = BOOST_BINARY(10000010);
      row_rate_result = ADC_board::rational_type(100,1);
    }
    else if(sample_rate == "60") {
      code_result = BOOST_BINARY(01110010);
      row_rate_result = ADC_board::rational_type(60,1);
    }
    else if(sample_rate == "50") {
      code_result = BOOST_BINARY(01100011);
      row_rate_result = ADC_board::rational_type(50,1);
    }
    else if(sample_rate == "30") {
      code_result = BOOST_BINARY(01010011);
      row_rate_result = ADC_board::rational_type(30,1);
    }
    else if(sample_rate == "25") {
      code_result = BOOST_BINARY(01000011);
      row_rate_result = ADC_board::rational_type(25,1);
    }
    else if(sample_rate == "15") {
      code_result = BOOST_BINARY(00110011);
      row_rate_result = ADC_board::rational_type(15,1);
    }
    else if(sample_rate == "10") {
      code_result = BOOST_BINARY(00100011);
      row_rate_result = ADC_board::rational_type(10,1);
    }
    else if(sample_rate == "5") {
      code_result = BOOST_BINARY(00010011);
      row_rate_result = ADC_board::rational_type(5,1);
    }
    else if(sample_rate == "2.5") {
      code_result = BOOST_BINARY(00000011);
      row_rate_result = ADC_board::rational_type(25,10);
    }
    else {
      std::stringstream err;
      err << "Invalid ADC.sample_rate setting for the Waveshare "
        "expansion board. Possible settings are: 30000, 15000, 7500, "
        "3750, 2000, 1000, 500, 100, 60, 50, 30, 25, 15, 10, 5, or 2.5 "
        "samples per second";
      throw std::runtime_error(err.str());
    }
  }

  return std::make_tuple(code_result,row_rate_result);
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


/**
    Spin wait on DRDY

    OK, this is stupid as a low sample rate will cause the CPU to block but
    for now this works. Need to rework low sample rate code to just manually
    poll and sleep instead of waiting on the DRDY to go low. An better
    alternative is to get GPIO interrupts working but that will likely need
    this to be run in the kernel which is a major rewrite
 */
inline void wait_DRDY(void)
{
  // Depending on what we are doing, this may make more sense as an interrupt
  while(bcm2835_gpio_lev(DRDY) != 0) {
    // Wait forever.
  }
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
 :ADC_board(vm), row_block(1), used_pins(9,0), _disabled(false)
{
  std::tie(_sample_rate_code,_row_sampling_rate) =
    detail::validate_translate_sample_rate(vm);
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

  // original had 1024 = 4.096us = 244.140625kHz from 250MHz system clock
  // not sure why this was chosen
  // Board clock is nominal 7.68 MHz or 130.2083 ns period
  // SCLK period is 4*1/fCLKIN per datasheet or 4*130.2083 = 520.8332 ns
  // This is optimal. SPI driver appears to only support clockdividers
  // that are a power of two. Thus, we should use
  // 256 = 1.024us = 976.5625MHz from 250MHz system clock
  // Might be possible to get away with a divider of 128, ie
  // 128 = 512ns = 1.953MHz from 250MHz system clock
  // Cycling throughput for 521 ns is 4374 with 30,000 SPS setting. Thus we
  // should expect to see lower performace. On my system, I achieve
  // ~0.24 ms sample period or about 4,166.6 interleaved samples per second
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);

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
  regs[3] = _sample_rate_code;

  detail::write_to_registers(REG_STATUS,regs,4);

  // ADC should now start to auto-cal, DRDY goes low when done
  detail::wait_DRDY();

}


void waveshare_ADS1256::trigger_sampling(const data_handler &handler,
  basic_trigger &trigger)
{
  if(_async) {
    if(_stats)
      trigger_sampling_async_wstat_impl(handler,trigger);
    else
      trigger_sampling_async_impl(handler,trigger);
  }
  else {
    if(_stats)
      trigger_sampling_wstat_impl(handler,trigger);
    else
      trigger_sampling_impl(handler,trigger);
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

void waveshare_ADS1256::trigger_sampling_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  sample_buffer_type sample_buffer(
    row_block*channel_assignment.size()*bit_depth());

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    detail::wait_DRDY();

    // switch to the next channel
    detail::write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

    bcm2835_spi_transfern(dummy_buf,3);
    CS_1();
  }

  // correct channel is now staged for conversion
  bool done = false;
  while(!done && !trigger.should_stop()) {
    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer.data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        detail::wait_DRDY();

        // switch to the next channel
        detail::write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

        bcm2835_spi_transfern(data_buffer,3);
        CS_1();

        data_buffer += 3;
      }
    }

    done = handler(sample_buffer.data(),rows,*this);
  }
}

void waveshare_ADS1256::trigger_sampling_wstat_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  typedef std::chrono::high_resolution_clock::time_point time_point_type;
  typedef std::chrono::nanoseconds::rep nanosecond_rep_type;

  static const std::size_t time_size = sizeof(nanosecond_rep_type);

  sample_buffer_type sample_buffer(
    row_block*channel_assignment.size()*(bit_depth()/8+time_size));

  std::vector<time_point_type> start_vec(channel_assignment.size());

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    detail::wait_DRDY();

    // switch to the next channel
    detail::write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

    bcm2835_spi_transfern(dummy_buf,3);
    CS_1();

    start_vec[chan] = std::chrono::high_resolution_clock::now();
  }

  // correct channel is now staged for conversion
  bool done = false;
  while(!done && !trigger.should_stop()) {
    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer.data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        detail::wait_DRDY();

        // switch to the next channel
        detail::write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

        bcm2835_spi_transfern(data_buffer,3);
        CS_1();

        std::chrono::high_resolution_clock::time_point now =
          std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds::rep elapsed =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            now-start_vec[chan]).count();

        data_buffer += 3;
        elapsed = ensure_be(elapsed);
        std::memcpy(data_buffer,&elapsed,time_size);
        data_buffer += time_size;
      }
    }

    done = handler(sample_buffer.data(),rows,*this);
  }
}

void waveshare_ADS1256::async_handler(ringbuffer_type &allocation_ringbuffer,
  ringbuffer_type &ready_ringbuffer, const data_handler &handler,
  std::atomic<bool> &done)
{
  static const std::chrono::milliseconds nap(100);

  sample_buffer_ptr sample_buffer;
  while(!done) {
    while(!ready_ringbuffer.pop(sample_buffer))
      std::this_thread::sleep_for(nap);

    done = handler(sample_buffer->data(),row_block,*this);

    allocation_ringbuffer.push(sample_buffer);
  }
}


void waveshare_ADS1256::trigger_sampling_async_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  static const std::size_t max_allocation = 32;
  static const std::chrono::milliseconds nap(100);

  ringbuffer_type allocation_ringbuffer(max_allocation);
  ringbuffer_type ready_ringbuffer(max_allocation);

  std::size_t buffer_size = row_block*channel_assignment.size()*bit_depth();
  for(std::size_t i=0; i<max_allocation; ++i)
    allocation_ringbuffer.push(
      sample_buffer_ptr(new sample_buffer_type(buffer_size)));

  std::atomic<bool> done(false);

  std::thread servicing_thread(&waveshare_ADS1256::async_handler,*this,
    std::ref(allocation_ringbuffer), std::ref(ready_ringbuffer),
    std::cref(handler), std::ref(done));


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

  sample_buffer_ptr sample_buffer;
  while(!done) {
    // get the next data_block
    while(!allocation_ringbuffer.pop(sample_buffer)) {
      std::cerr << "Warning, no buffers available for writing. Ignoring data "
        "for this time slice!\n";
      std::this_thread::sleep_for(nap);
    }

    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    std::size_t channels = channel_assignment.size();
    for(std::size_t idx = 0; idx < sample_buffer->size(); idx+=bit_depth()) {
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

      bcm2835_spi_transfern(sample_buffer->data()+idx,3);
      CS_1();
    }

    ready_ringbuffer.push(sample_buffer);
  }

  servicing_thread.join();
}

void waveshare_ADS1256::trigger_sampling_async_wstat_impl(
  const data_handler &handler, basic_trigger &trigger)
{
  typedef std::chrono::high_resolution_clock::time_point time_point_type;
  typedef std::chrono::nanoseconds::rep nanosecond_rep_type;

  static const std::size_t time_size = sizeof(nanosecond_rep_type);

  static const std::size_t max_allocation = 32;

  ringbuffer_type allocation_ringbuffer(max_allocation);
  ringbuffer_type ready_ringbuffer(max_allocation);

  std::size_t buffer_size =
    row_block*channel_assignment.size()*(bit_depth()/8+time_size);

  for(std::size_t i=0; i<max_allocation; ++i)
    allocation_ringbuffer.push(
      sample_buffer_ptr(new sample_buffer_type(buffer_size)));

  std::atomic<bool> done(false);

  std::thread servicing_thread(&waveshare_ADS1256::async_handler,*this,
    std::ref(allocation_ringbuffer), std::ref(ready_ringbuffer),
    std::cref(handler), std::ref(done));

  std::vector<time_point_type> start_vec(channel_assignment.size());

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    detail::wait_DRDY();

    // switch to the next channel
    detail::write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

    bcm2835_spi_transfern(dummy_buf,3);
    CS_1();

    start_vec[chan] = std::chrono::high_resolution_clock::now();
  }

  // correct channel is now staged for conversion
  sample_buffer_ptr sample_buffer;
  while(!done && !trigger.should_stop()) {
    // get the next data_block
    while(!allocation_ringbuffer.pop(sample_buffer)) {
//       std::cerr << "Warning, no buffers available for writing. Ignoring data "
//         "for this time slice!\n";
//       std::this_thread::sleep_for(nap);
    }

    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer->data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        detail::wait_DRDY();

        // switch to the next channel
        detail::write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
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

        bcm2835_spi_transfern(data_buffer,3);
        CS_1();

        std::chrono::high_resolution_clock::time_point now =
          std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds::rep elapsed =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            now-start_vec[chan]).count();

        data_buffer += 3;
        elapsed = ensure_be(elapsed);
        std::memcpy(data_buffer,&elapsed,time_size);
        data_buffer += time_size;
      }
    }

    ready_ringbuffer.push(sample_buffer);
  }

  servicing_thread.join();
}



}
