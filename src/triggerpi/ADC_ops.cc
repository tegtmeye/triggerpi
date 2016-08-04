#include "ADC_ops.h"
#include "waveshare_ADS1256_expansion.h"


#include <tuple>
#include <cstdint>
#include <memory>
#include <cstdio>

namespace po = boost::program_options;


template<typename T>
struct data_printer {
  data_printer(double val) :_sensitivity(val) {}

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

  std::uint_fast32_t snum;
  std::uint_fast32_t sdenom;
  std::tie(snum,sdenom) = adc_board->sensitivity();

  double sensitivity = double(snum)/sdenom;

  ADC_board::sample_callback_type
    callback((data_printer<int32_t>(sensitivity)));
  adc_board->trigger_sampling(callback,2);

  return adc_board;
}
