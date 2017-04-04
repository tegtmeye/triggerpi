#include "waveshare_ADS1256.h"

#include <boost/program_options.hpp>

#include <iostream>
#include <regex>

/*
    waveshare_ADS1256 is broken into 3 files: configuration, ADC, and DAC

    This file is for class-wide config including registration and class factory
    configuration.
*/

#define WAVESHARE_EXPANSION "waveshare_ADC"


namespace waveshare {

// integer power function for base 10. Modified from:
// http://stackoverflow.com/questions/101439/the-most-efficient-way-to-implement-an-integer-based-power-function-powint-int
// Does not check for overflow or unsigned values
template<typename T>
static T ipow10(T exp)
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

#if 0
static std::tuple<unsigned char,expansion_board::rational_type>
validate_translate_sample_rate(const po::variables_map &vm)
{
  // 30000 is the default sample rate
  unsigned char code_result = BOOST_BINARY(11110000);
  expansion_board::rational_type row_rate_result(30000,1);

  if(vm.count("waveshare_ADC.sample_rate")) {
    const std::string &sample_rate =
      vm["waveshare_ADC.sample_rate"].as<std::string>();

    // Don't be overly clever. Just map to datasheet.
    if(sample_rate == "30000") {
      code_result = BOOST_BINARY(11110000);
      row_rate_result = expansion_board::rational_type(30000,1);
    }
    else if(sample_rate == "15000") {
      code_result = BOOST_BINARY(11100000);
      row_rate_result = expansion_board::rational_type(15000,1);
    }
    else if(sample_rate == "7500") {
      code_result = BOOST_BINARY(11010000);
      row_rate_result = expansion_board::rational_type(7500,1);
    }
    else if(sample_rate == "3750") {
      code_result = BOOST_BINARY(11000000);
      row_rate_result = expansion_board::rational_type(3750,1);
    }
    else if(sample_rate == "2000") {
      code_result = BOOST_BINARY(10110000);
      row_rate_result = expansion_board::rational_type(2000,1);
    }
    else if(sample_rate == "1000") {
      code_result = BOOST_BINARY(10100001);
      row_rate_result = expansion_board::rational_type(1000,1);
    }
    else if(sample_rate == "500") {
      code_result = BOOST_BINARY(10010010);
      row_rate_result = expansion_board::rational_type(500,1);
    }
    else if(sample_rate == "100") {
      code_result = BOOST_BINARY(10000010);
      row_rate_result = expansion_board::rational_type(100,1);
    }
    else if(sample_rate == "60") {
      code_result = BOOST_BINARY(01110010);
      row_rate_result = expansion_board::rational_type(60,1);
    }
    else if(sample_rate == "50") {
      code_result = BOOST_BINARY(01100011);
      row_rate_result = expansion_board::rational_type(50,1);
    }
    else if(sample_rate == "30") {
      code_result = BOOST_BINARY(01010011);
      row_rate_result = expansion_board::rational_type(30,1);
    }
    else if(sample_rate == "25") {
      code_result = BOOST_BINARY(01000011);
      row_rate_result = expansion_board::rational_type(25,1);
    }
    else if(sample_rate == "15") {
      code_result = BOOST_BINARY(00110011);
      row_rate_result = expansion_board::rational_type(15,1);
    }
    else if(sample_rate == "10") {
      code_result = BOOST_BINARY(00100011);
      row_rate_result = expansion_board::rational_type(10,1);
    }
    else if(sample_rate == "5") {
      code_result = BOOST_BINARY(00010011);
      row_rate_result = expansion_board::rational_type(5,1);
    }
    else if(sample_rate == "2.5") {
      code_result = BOOST_BINARY(00000011);
      row_rate_result = expansion_board::rational_type(25,10);
    }
    else {
      std::stringstream err;
      err << "Invalid ADC sample_rate setting for the Waveshare "
        "expansion board. Possible settings are: 30000, 15000, 7500, "
        "3750, 2000, 1000, 500, 100, 60, 50, 30, 25, 15, 10, 5, or 2.5 "
        "samples per second";
      throw std::runtime_error(err.str());
    }
  }

  return std::make_tuple(code_result,row_rate_result);
}


static std::tuple<unsigned char,std::uint32_t>
validate_translate_gain(const po::variables_map &vm)
{
  // 1 is the default gain value
  std::tuple<unsigned char,std::uint32_t> result;

  if(vm.count("waveshare_ADC.gain")) {
    const std::string &gain = vm["waveshare_ADC.gain"].as<std::string>();

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
      err << "Invalid ADC gain setting for the Waveshare "
        "expansion board. Possible settings are: 1, 2, 4, 8, 16, 32, or "
        "64";
      throw std::runtime_error(err.str());
    }
  }

  return result;
}

static expansion_board::rational_type
validate_translate_Vref(const std::string Vref_str)
{
  typedef expansion_board::rational_type::int_type rint_type;

  static const std::regex digits_regex("[0-9]*");
  static const expansion_board::rational_type min_vref(5,10);
  static const expansion_board::rational_type max_vref(26,10);

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
    err << "Unexpected value '" << ex.what() << "' in Waveshare Vref config";
    throw std::runtime_error(err.str());
  }

  expansion_board::rational_type result(integral*factor+decimal,factor);

  if(result < min_vref || max_vref < result) {
    std::stringstream err;
    err << "Invalid Vref. Values for the ADS1256 is: "
      "0.5 < Vref < 2.6 volts. Given " << result << " volts.";
    throw std::runtime_error(err.str());
  }

  return result;
}
#endif






const bool waveshare_ADS1256::did_register_config =
  waveshare_ADS1256::register_config();

po::options_description waveshare_ADS1256::cmd_options(void)
{
  // Waveshare High-Precision ADC/DA Board Configuration options
  std::string config_header(waveshare_ADS1256::system_config_desc_short());
  po::options_description waveshare_config(config_header + " Config Options");

  waveshare_config.add_options()
    ("waveshare_ADC.sample_rate",po::value<std::string>(),
      "  Set the sample rate. Actual data rates depend on the number of "
      "channels to read. For the ADS1256 ADC chip, each channel is "
      "multiplexed using a single ADC core. Each additional channel "
      "sampled reduces the per-channel sampling rate linearly. This cycling "
      "also imposes an overhead that further reduces the peak sample rate "
      "from the advertised maximums. Valid values reflect the peak sample "
      "rate based on a single channel but the approximate corresponding "
      "multiplexed rate is also listed. For example 30000 ~4165 "
      "indicates that the valid setting is '30000' which corresponds to "
      "the single-channel sampling rate and a multiplexed rate of "
      "approximately 4165 samples-per-second which is evenly divided by "
      "the number of channels enabled. Valid values are one of:\n"
      "  Setting:     Multichannel rate (sps):\n"
      "   30000        ~4165 [default]\n"
      "   15000        ~3600\n"
      "    7500        ~2910\n"
      "    3750        ~2095\n"
      "    2000        ~1405\n"
      "    1000        ~825\n"
      "     500        ~475\n"
      "     100        ~98\n"
      "      60        ~59\n"
      "      50        ~49.5\n"
      "      30        ~30\n"
      "      25        ~?\n"
      "      15        ~?\n"
      "      10        ~?\n"
      "       5        ~?\n"
      "       2.5      ~?\n"
      "Currently rates below about 60 may cause a timeout error waiting "
      "for data to be ready on the ADC. This is a bug and needs to be fixed.")
    ("waveshare_ADC.gain",po::value<std::string>(),
      "  Set the gain for the configured ADC. Valid values are one of 1 "
      "[default], 2, 4, 8, 16, 32, or 64.")
    ("waveshare_ADC.Vref",po::value<std::string>()->default_value("2.5"),
      "  Set the ADC reference voltage. This is the positive voltage "
      "difference between pin 4 and pin 3 on the ADS1256 ADC chip itself. "
      "This setting is only needed if you have actually measured the "
      "voltage difference between these two pins and you need a higher-"
      "accuracy measurement. The nominal value for these boards is 2.5V. "
      "This only affects voltage calculations")
    ("waveshare_ADC.AINCOM",po::value<double>()->default_value(0.0),
      "  Set the ADC center voltage for single-ended inputs. This value is "
      "only meaningful for ADC count to voltage conversions. That is, the "
      "ADC counts will be centered around the AINCOM voltage input "
      "of this value. An ADC count of zero means "
      "the value set here. If the AINCOM/AGND jumper is installed, then "
      "this value should be 0 and the ADC value will be from 0->2^23 for "
      "a unity gain. If the jumper is removed and a 2.5V source is applied "
      "to AINCOM, set this option to 2.5 and the ADC values will vary from "
      "-2^22 to +2^22 for a unity gain (23-bits of precision) for a 0->5V "
      "input. If the gain is set to 2, then the ADC values will vary from "
      "-2^23 to +2^23 (24-bits of precision) for a 0->5V input")
    ("waveshare_ADC.buffered",po::value<bool>()->default_value(false),
      "  Enable/disable the ADS1256 analog input buffer. If enabled, then "
      "the input impedance presented to the analog input will scale "
      "according to the sampling frequency: 30 ksps to 2 ksps = ~10 MOhm, "
      "1 ksps = ~20 MOhm, 500 to 60 sps = ~40 MOhm, and <= 50 sps = ~80 "
      "MOhm. Default is disabled. NOTE: if enabled, then the voltage "
      "difference between analog ground and any analog in must be below "
      "AVDD-2V. Since the board is pre-configured for an AVDD of 5V, "
      "AD0-AD7 must be below 3V. See the ADS1255/6 datasheet for more "
      "information.")
   ("waveshare_ADC.sampleblocks",po::value<std::size_t>(),
      "  Override the number of samples to process in each block operation. "
      "This is a function of the number of channels currently configured, "
      "whether or not asynchronous operations are enabled, and is affected "
      "by system memory. This value must be a positive integer greater than "
      "one.")
   ("waveshare_ADC.ADC",
      po::value<std::vector<std::string> >(),
      "  Configure each ADC channel. There can be multiple occurrences "
      "of waveshare_ADC.ADC as need to configure the desired input. For the "
      "Waveshare ADC/DA expansion board, there are 9 pins that can be "
      "configured as up to eight single-ended channels, up to four "
      "differential channels, or any combination thereof. The pins are "
      "labeled 0-8 and 'COM' where COM is a synonym for 8. The value is a "
      "comma-separated list of pin assignments and channel type id. Pins can "
      "only be listed once with the exception of the COM pin. One end of the "
      "single-ended channels should "
      "assigned to 'COM'. Differential channels can be assigned to any "
      "two pins but it is recommended that they be adjacent to each "
      "other for optimum performance. For differential channels, the "
      "first pin is taken to be the positive pin. "
      "Unlisted pins will be turned off [preferred]. Examples:\n"
      "  To configure pins 1 for singled ended input and pin 3 and 4 "
      "for differential:\n"
      "  --waveshare_ADC.ADC 0,COM\n  --waveshare_ADC.ADC 2,3\n")
    ;

  return waveshare_config;
}



waveshare_ADS1256::waveshare_ADS1256(void)
  :ADC_board(trigger_type_t::intermittent_sink), row_block(1),
    used_pins(9,0)
{
}

void waveshare_ADS1256::validate_assign_channel(const std::string config_str,
  bool verbose)
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
      }

      if(pinA > 8) {
        std::stringstream err;
        err << "'" << substr << "' is an invalid pin. Pins must be "
          "labeled 0-8 or COM where COM is a synonym for 8";
        throw std::runtime_error(err.str());
      }
      if(used_pins[pinA]) {
        std::stringstream err;
        err << "'" << substr << "' has been used previously. Pins must be "
          "only used once in a channel assignment";
        throw std::runtime_error(err.str());
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
      }

      if(pinB > 8) {
        std::stringstream err;
        err << "'" << substr << "' is an invalid pin. Pins must be "
          "labeled 0-8 or COM where COM is a synonym for 8";
        throw std::runtime_error(err.str());
      }
      if(used_pins[pinB] || pinB == pinA) {
        std::stringstream err;
        err << "'" << substr << "' has been used previously. Pins must be "
          "only used once in a channel assignment";
        throw std::runtime_error(err.str());
      }
    }
    else {
      std::stringstream err;
      err << "unexpected '" << substr << "' in ADC channel assignment. "
        "ADC Channel directives should be of the form "
        "waveshare_ADC.ADC=pinA,pinB";
      throw std::runtime_error(err.str());
    }
  }

  // all is well, now do the assignment and log
  char MUX = (pinA << 4) | pinB;
  channel_assignment.push_back(MUX);
  if(pinA < 8)
    used_pins.at(pinA) = true;

  if(pinB < 8)
    used_pins.at(pinB) = true;

  if(verbose) {
    std::cout << "Enabled ADC channel with configuration: " << pinA
      << "," << pinB << "\n";
  }
}

void waveshare_ADS1256::configure_options(const po::variables_map &_vm)
{
  assert(did_register_config);
#if 0
  std::tie(_sample_rate_code,_row_sampling_rate) =
    validate_translate_sample_rate(_vm);
  std::tie(_gain_code,_gain) = validate_translate_gain(_vm);


  // Set up channels. If there are no channels, then nothing to do.
  if(_vm.count("waveshare_ADC.ADC")) {
    const std::vector<std::string> &channel_vec =
      _vm["waveshare_ADC.ADC"].as< std::vector<std::string> >();

    for(std::size_t i=0; i<channel_vec.size(); ++i)
      validate_assign_channel(channel_vec[i],detail::is_verbose<1>(_vm));
  }

  if(!_vm.count("waveshare_ADC.AINCOM"))
    throw std::runtime_error("Missing Waveshare AINCOM value");

  aincom = _vm["waveshare_ADC.AINCOM"].as<double>();

  buffer_enabled = _vm["waveshare_ADC.buffered"].as<bool>();

  if(!_vm.count("waveshare_ADC.Vref"))
    throw std::runtime_error("Missing Waveshare reference voltage");

  _Vref = validate_translate_Vref(
    _vm["waveshare_ADC.Vref"].as<std::string>());

  _async = (_vm.count("async") && _vm["async"].as<bool>());

  _stats = (_vm.count("stats") && _vm["stats"].as<bool>());

  if(_vm.count("waveshare_ADC.sampleblocks"))
    row_block = _vm["waveshare_ADC.sampleblocks"].as<std::size_t>();
  else if(_async)
    row_block = 1024;
  else
    row_block = 10;

  if(!row_block)
    throw std::runtime_error("waveshare_ADC.sampleblocks must be a positive "
      "integer");
#endif
}

}
