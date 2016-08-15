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
#include <fstream>
#include <cstring>
#include <cerrno>

namespace po = boost::program_options;
namespace fs = boost::filesystem;








void enable_adc(const po::variables_map &vm)
{
  const std::string &adc_system = vm["ADC.system"].as<std::string>();

  std::shared_ptr<ADC_board> adc_board;

  // waveshare ADS1256 expansion board is the only board supported at the
  // moment
  if(adc_system == "waveshare") {
    adc_board.reset(new waveshare::waveshare_ADS1256(vm));
  }
  else {
    std::stringstream err;
    err << "Unknown ADC board: '" << adc_system << "'";
    throw std::runtime_error(err.str());
  }

  adc_board->configure_options();

  if(adc_board->disabled())
    return; // empty probably should output something

  ADC_board::data_handler hdlr;

  if(vm.count("output")) {
    try {
      hdlr = adc_board->file_printer(fs::path(vm["output"].as<std::string>()));

      if(!hdlr)
        throw std::runtime_error("Unsupported output mode for this ADC board");

    }
    catch(const fs::filesystem_error &ex) {
      std::stringstream err;
      err << "File error for'" << vm["output"].as<std::string>()
        << "'. " << ex.code() << ": " << ex.what();
      throw std::runtime_error(err.str());
    }
  }
  else {
    ADC_board::data_handler hdlr;

    // don't produce anything if silent
    if(vm.count("silent"))
      hdlr = adc_board->null_consumer();
    else
      hdlr = adc_board->screen_printer();

    if(!hdlr)
      throw std::runtime_error("Unsupported output mode for this ADC board");
  }

  try {
    adc_board->setup_com();
    adc_board->initialize();

    if(vm.count("async") && vm["async"].as<bool>())
      adc_board->trigger_sampling_async(hdlr);
    else
      adc_board->trigger_sampling(hdlr);
  }
  catch(const fs::filesystem_error &ex) {
    std::stringstream err;
    err << "File error for'" << vm["output"].as<std::string>()
      << "'. " << ex.code() << ": " << ex.what();
    throw std::runtime_error(err.str());
  }


//
//

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
}
