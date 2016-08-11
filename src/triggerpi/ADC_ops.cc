#include <config.h>

#include "ADC_ops.h"
#include "ADC_board.h"
#include "waveshare_ADS1256_expansion.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <tuple>
#include <cstdint>
#include <memory>
#include <cstdio>
#include <iostream>
#include <chrono>

namespace po = boost::program_options;

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif

template<typename T>
struct screen_printer {
  screen_printer(double val) :_sensitivity(val) {}

  bool operator()(void *_data, std::size_t channels, std::size_t samples)
  {
    T *data = reinterpret_cast<T*>(_data);

    std::size_t idx = 0;
    for(std::size_t i=0; i<samples; ++i) {
      for(std::size_t j=0; j<channels; ++j) {
        std::printf(" %010i(0x%08X)[%04f]",data[idx],data[idx],
          data[idx]*_sensitivity);
        ++idx;
      }
      std::printf("\n");
    }

    printf("\033[2J");

    return false;
  }

  double _sensitivity;
};

#if 0
template <be::order Order, class T, std::size_t Nbits>
bool file_printer(void *_data, std::size_t num_rows, const ADC_board &adc_board)
{
  static const std::size_t Nbytes = Nbits/8;

  char *data = static_cast<char *>(_data);

  if(adc_board.sample_time_prefix()) {
    for(std::size_t row=0; row<num_rows; ++row) {
      for(std::size_t col=0; col<adc_board.enabled_channels(); ++col) {
        // deserialize data
        be::endian_buffer<Order,T,Nbits> raw_adc_counts;
        std::memcpy(&raw_adc_counts,data,Nbytes);
        T adc_counts = raw_adc_counts.value();

        data += Nbytes;

        std::chrono::nanoseconds::rep elapsed;
        std::memcpy(&elapsed,data+Nbytes,sizeof(std::chrono::nanoseconds::rep));
        data += sizeof(std::chrono::nanoseconds::rep);

        std::cout << " " << adc_counts << "(" << elapsed << " ns)";
      }
      std::cout << std::endl;
    }
  }
  else {

  }

  printf("\033[2J");

  return false;
}
#endif


template<typename NativeT, bool ADCBigEndian, std::size_t NBytes>
bool file_printer(void *_data, std::size_t num_rows, const ADC_board &adc_board)
{
  static_assert(sizeof(NativeT) >= NBytes,
    "Native type must be larger then NBytes");

  char *data = static_cast<char *>(_data);

  if(adc_board.sample_time_prefix()) {
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

        std::printf(" %010i(0x%08X)[%lld ns]",adc_counts,adc_counts,elapsed);
      }
      std::printf("\n");
    }
  }
  else {

  }

  printf("\033[2J");

  return false;
}


// struct file_printer {
//   file_printer(const std::shared_ptr<ADC_board> &_adc_board,
//     std::atomic<bool> &_done) :adc_board(_adc_board), done(_done) {}
//
//   void operator()(ADC_board::ringbuffer_type &_allocation_ringbuffer,
//     ADC_board::ringbuffer_type &_data_ringbuffer)
//   {
//     while(!done.get()) {
//       sleep(1);
//     }
//
//
// #if 0
//     T *data = reinterpret_cast<T*>(_data);
//
//     std::size_t idx = 0;
//     for(std::size_t i=0; i<samples; ++i) {
//       for(std::size_t j=0; j<channels; ++j) {
//         std::printf(" %010i(0x%08X)[%04f]",data[idx],data[idx],
//           data[idx]*_sensitivity);
//         ++idx;
//       }
//       std::printf("\n");
//     }
//
//     printf("\033[2J");
// #endif
//
//   }
//
//   std::shared_ptr<ADC_board> adc_board;
//   std::reference_wrapper<std::atomic<bool> > done;
// };


std::shared_ptr<ADC_board> enable_adc(const po::variables_map &vm)
{
  const std::string &adc_system = vm["ADC.system"].as<std::string>();

  std::shared_ptr<ADC_board> adc_board;

  // waveshare ADS1256 expansion board is the only board supported at the
  // moment
  if(adc_system == "waveshare")
    adc_board.reset(new waveshare::waveshare_ADS1256(vm));
  else {
    std::stringstream err;
    err << "Unknown ADC board: '" << adc_system << "'";
    throw std::runtime_error(err.str());
  }


  adc_board->configure_options();

  if(adc_board->disabled())
    return std::shared_ptr<ADC_board>(); // empty

  adc_board->setup_com();
  adc_board->initialize();

  adc_board->trigger_sampling(file_printer<std::int32_t,true,3>);

////   sensitivity is 1/(2^23-1) * FSRn/(FSRd * gain) * ADC_count
//    ADC_board::rational_type rational_sens = adc_board->sensitivity();
//    double sensitivity = boost::rational_cast<double>(rational_sens);
//
//   ADC_board::sample_callback_type
//     callback((file_printer<int32_t>(sensitivity)));
//   adc_board->trigger_sampling(callback,2);

  // size of the ring buffer. If the data_sink cannot keep up with the
  // data produced from the ADC, new data will be dropped
//   static const std::size_t max_size = 32;
//   std::shared_ptr<ADC_board::ringbuffer_type> allocation_ringbuffer(
//     new ADC_board::ringbuffer_type(max_size));
//   std::shared_ptr<ADC_board::ringbuffer_type> data_ringbuffer(
//     new ADC_board::ringbuffer_type(max_size));
//
//
//   std::atomic<bool> done(false);
//   std::thread handler(file_printer(adc_board,std::ref(done)),
//     std::ref(*allocation_ringbuffer),std::ref(*data_ringbuffer));
//   adc_board->trigger_psampling(*allocation_ringbuffer,*data_ringbuffer);
//
//   done = true;
//   handler.join();

  return adc_board;
}
