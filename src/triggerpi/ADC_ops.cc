#include "ADC_ops.h"
#include "waveshare_ADS1256_expansion.h"


#include <memory>
#include <cstdio>

namespace po = boost::program_options;


template<typename T>
bool print_data(void *_data, std::size_t channels, std::size_t samples)
{
  T *data = reinterpret_cast<T*>(_data);

  std::size_t idx = 0;
  for(std::size_t i=0; i<samples; ++i) {
    for(std::size_t j=0; j<channels; ++j) {
      std::printf(" %010i(%X)",data[idx],data[idx]);
      ++idx;
    }
    std::printf("\r");
  }

  return false;
}

std::unique_ptr<ADC_board> enable_adc(const po::variables_map &vm)
{
  const std::string &adc_system = vm["ADC.system"].as<std::string>();

  std::unique_ptr<ADC_board> adc_board;

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
    return std::unique_ptr<ADC_board>(); // empty

  adc_board->setup_com();
  adc_board->initialize();



  ADC_board::sample_callback_type callback(print_data<int32_t>);
  adc_board->trigger_sampling(callback,1);

  return adc_board;
}
