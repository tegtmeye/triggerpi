#include "waveshare_expansion.h"

#include <boost/program_options.hpp>

/*
    waveshare_expansion is broken into 3 files: configuration, ADC, and DAC

    This file is for class-wide config including registration and class factory
    configuration.
*/

#define WAVESHARE_EXPANSION "waveshare"


namespace waveshare {

const bool waveshare_expansion::did_register_config =
  waveshare_expansion::register_config();

po::options_description waveshare_expansion::cmd_options(void)
{
  // Waveshare High-Precision ADC/DA Board Configuration options
  std::string config_header(waveshare_expansion::system_config_desc_short());
  po::options_description waveshare_config(config_header + " Config Options");
  waveshare_config.add_options()
    ("waveshare.sample_rate",po::value<std::string>(),
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
    ("waveshare.gain",po::value<std::string>(),
      "  Set the gain for the configured ADC. Valid values are one of 1 "
      "[default], 2, 4, 8, 16, 32, or 64.")
    ("waveshare.Vref",po::value<std::string>()->default_value("2.5"),
      "  Set the ADC reference voltage. This is the positive voltage "
      "difference between pin 4 and pin 3 on the ADS1256 ADC chip itself. "
      "This setting is only needed if you have actually measured the "
      "voltage difference between these two pins and you need a higher-"
      "accuracy measurement. The nominal value for these boards is 2.5V. "
      "This only affects voltage calculations")
    ("waveshare.AINCOM",po::value<double>()->default_value(0.0),
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
    ("waveshare.buffered",po::value<bool>()->default_value(false),
      "  Enable/disable the ADS1256 analog input buffer. If enabled, then "
      "the input impedance presented to the analog input will scale "
      "according to the sampling frequency: 30 ksps to 2 ksps = ~10 MOhm, "
      "1 ksps = ~20 MOhm, 500 to 60 sps = ~40 MOhm, and <= 50 sps = ~80 "
      "MOhm. Default is disabled. NOTE: if enabled, then the voltage "
      "difference between analog ground and any analog in must be below "
      "AVDD-2V. Since the board is pre-configured for an AVDD of 5V, "
      "AD0-AD7 must be below 3V. See the ADS1255/6 datasheet for more "
      "information.")
   ("waveshare.sampleblocks",po::value<std::size_t>(),
      "  Override the number of samples to process in each block operation. "
      "This is a function of the number of channels currently configured, "
      "whether or not asynchronous operations are enabled, and is affected "
      "by system memory. This value must be a positive integer greater than "
      "one.")
   ("waveshare.ADC",
      po::value<std::vector<std::string> >(),
      "  Configure each ADC channel. There can be multiple occurrences "
      "of waveshare.ADC as need to configure the desired input. For the "
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
      "  --waveshare.ADC 0,COM\n  --waveshare.ADC 2,3\n")
    ;

  return waveshare_config;
}

}
