#ifndef BASIC_FILE_PRINTER_H
#define BASIC_FILE_PRINTER_H

#include <config.h>

#include "ADC_board.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <iomanip>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif

namespace fs = boost::filesystem;

/*
    Must have callable signature matching that of ADC_board::data_handler
    or bool(void *data, std::size_t rows, const ADC_board &board)
 */
template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
class basic_file_printer {
  public:
    basic_file_printer(const fs::path &loc, const ADC_board &adc_board);

    bool operator()(void *_data, std::size_t num_rows,
      const ADC_board &adc_board);

  private:
    std::string board_name;

    bool with_stats;
    double sensitivity;
    std::vector<std::chrono::nanoseconds::rep> diff;
    std::shared_ptr<fs::ofstream> out;
};

template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
basic_file_printer<NativeT,ADCBigEndian,NBytes>::basic_file_printer(
  const fs::path &loc, const ADC_board &adc_board)
    :board_name(adc_board.board_name()),
      with_stats(adc_board.stats()),
      sensitivity(boost::rational_cast<double>(adc_board.sensitivity())),
      diff(adc_board.enabled_channels()), out(new fs::ofstream(loc))
{
}

template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
bool basic_file_printer<NativeT,ADCBigEndian,NBytes>::operator()(void *_data,
  std::size_t num_rows, const ADC_board &adc_board)
{
  static_assert(sizeof(NativeT) >= NBytes,
    "Native type must be larger then NBytes");

  char *data = static_cast<char *>(_data);

  for(std::size_t row=0; row<num_rows; ++row) {
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

      if(col != 0)
        *out << ", ";

      *out << std::hex << std::setw(8) << std::setfill('0') << adc_counts;

      if(with_stats) {
        std::chrono::nanoseconds::rep elapsed;
        std::memcpy(&elapsed,data,sizeof(std::chrono::nanoseconds::rep));

        if(ADCBigEndian)
          elapsed = be_to_native(elapsed);
        else
          elapsed = le_to_native(elapsed);

        data += sizeof(std::chrono::nanoseconds::rep);

        *out << ", " << std::dec << std::setw(8)
                  << (elapsed-diff[col]);

        diff[col] = elapsed;
      }

    }

    *out << "\n";
  }

  return false;
}

#endif
