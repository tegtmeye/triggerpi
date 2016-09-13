#ifndef BASIC_SCREEN_PRINTER_H
#define BASIC_SCREEN_PRINTER_H

#include <config.h>

#include "expansion_board.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <iomanip>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif

/*
    Must have callable signature matching that of ADC_board::data_handler
    or bool(void *data, std::size_t rows, const ADC_board &board)
 */
template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
struct basic_screen_printer {
  basic_screen_printer(const expansion_board &adc_board);

  bool operator()(void *_data, std::size_t num_rows, const expansion_board &adc_board)
  {
    static_assert(sizeof(NativeT) >= NBytes,
      "Native type must be larger then NBytes");

    char *data = static_cast<char *>(_data);

    // clear the screen and move to top
    std::cout << "\033[2J\033[H"
      << board_name << "\n\n";

    for(std::size_t col=0; col<diff.size(); ++col) {
      // deserialize data
      NativeT adc_counts = 0;
      char *raw_adc_count = reinterpret_cast<char *>(&adc_counts);

      // compiler should pick one
      if(ADCBigEndian && WORDS_BIGENDIAN) {
        std::copy(data,data+NBytes,raw_adc_count);
        adc_counts >>= ((sizeof(NativeT)-NBytes)*8);
      }
      else if(ADCBigEndian && !WORDS_BIGENDIAN) {
        std::reverse_copy(data,data+NBytes,
          raw_adc_count+(sizeof(NativeT)-NBytes));
        adc_counts >>= ((sizeof(NativeT)-NBytes)*8);
      }
      else if(!ADCBigEndian && WORDS_BIGENDIAN) {
        std::reverse_copy(data,data+NBytes,raw_adc_count);
        adc_counts >>= ((sizeof(NativeT)-NBytes)*8);
      }
      else { // !ADCBigEndian && !WORDS_BIGENDIAN
        std::copy(data,data+NBytes,raw_adc_count+(sizeof(NativeT)-NBytes));
        adc_counts >>= ((sizeof(NativeT)-NBytes)*8);
      }

      data += NBytes;

      std::cout
        << "Channel " << col << ": "
        << std::fixed << std::setprecision(6) << sensitivity*adc_counts
        << "V (0x" << std::hex << std::setw(8) << std::setfill('0')
        << adc_counts << ") ";

      if(with_stats) {
        std::chrono::nanoseconds::rep elapsed;
        std::memcpy(&elapsed,data,sizeof(std::chrono::nanoseconds::rep));

        if(ADCBigEndian)
          elapsed = detail::be_to_native(elapsed);
        else
          elapsed = detail::le_to_native(elapsed);

        data += sizeof(std::chrono::nanoseconds::rep);

        std::cout << std::dec << std::setw(8)
                  << (elapsed-diff[col]) << " ns";

        diff[col] = elapsed;
      }

      std::cout << "\n";
    }

    return false;
  }

  std::string board_name;

  bool with_stats;
  double sensitivity;
  std::vector<std::chrono::nanoseconds::rep> diff;
};

template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
basic_screen_printer<NativeT,ADCBigEndian,NBytes>::basic_screen_printer(
  const expansion_board &adc_board) :board_name(adc_board.system_description()),
    with_stats(adc_board.stats()),
    sensitivity(boost::rational_cast<double>(adc_board.sensitivity())),
    diff(adc_board.enabled_channels())
{
}


#endif
