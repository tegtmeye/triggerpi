#ifndef ADC_OPS_H
#define ADC_OPS_H

#include "ADC_board.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

void enable_adc(const po::variables_map &vm);

#endif
