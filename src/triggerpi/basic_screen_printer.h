#ifndef BASIC_SCREEN_PRINTER_H
#define BASIC_SCREEN_PRINTER_H

#include <config.h>

#include "ADC_board.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif

/*
    Must have callable signature matching that of ADC_board::data_handler
    or bool(void *data, std::size_t rows, const ADC_board &board)
 */
template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
struct basic_screen_printer {
  bool operator()(void *_data, std::size_t num_rows, const ADC_board &adc_board)
  {
    static_assert(sizeof(NativeT) >= NBytes,
      "Native type must be larger then NBytes");

    char *data = static_cast<char *>(_data);

    if(adc_board.stats()) {
      for(std::size_t row=0; row<num_rows; ++row) {
        for(std::size_t col=0; col<adc_board.enabled_channels(); ++col) {
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

          std::chrono::nanoseconds::rep elapsed;
          std::memcpy(&elapsed,data,sizeof(std::chrono::nanoseconds::rep));

          if(ADCBigEndian)
            elapsed = be_to_native(elapsed);
          else
            elapsed = le_to_native(elapsed);

          data += sizeof(std::chrono::nanoseconds::rep);

          std::printf(" (0x%08X)[%lld ns]",adc_counts,elapsed);
        }
        std::printf("\n");
      }
    }
    else {

    }

    printf("\033[2J");

    return false;
  }
};


#endif
