/*
    Abstract class for an ADC expansion board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

#include <config.h>

#include "bits.h"
#include "expansion_board.h"

#include <boost/rational.hpp>
#include <boost/program_options.hpp>


namespace b = boost;
namespace po = boost::program_options;

/*
  Abstract class for ADC expansion boards. Defines additional
  board-specific calls. Any base class calls are provided here for
  exposition only

*/
class ADC_board : public expansion_board {
  public:
    // All functions returning this rational type are exact as to preserve
    // downstream calculations such that significant figures can be applied
    // as appropriate. That is, if this value is 1/3, then it is exact. If
    // this value needs to be later converted to 'n' significant figures,
    // then it can without compromising later calculations due to premature
    // conversion.
    typedef b::rational<std::uint64_t> rational_type;

    using expansion_board::expansion_board;

    virtual ~ADC_board(void) {}

    // current sampling rate in rows per second.
    virtual rational_type row_sampling_rate(void) const = 0;

    // number of bits for each sample
    virtual std::uint32_t bit_depth(void) const = 0;

    // True if the ADC counts be considered a signed integer. This is the
    // native ADC format---not whether or not your measurement will generate a
    // negative number or not. Most true-differential ADCs will have a native
    // unsigned format.
    virtual bool ADC_counts_signed(void) const = 0;

    virtual bool ADC_counts_big_endian(void) const = 0;

    // The sensitivity expressed as a rational number.
    // Using the rational form ensures significant digit carryover to
    // calculations. Thus, the sensitivity can be expressed as:
    // FSR/(2^bit_depth*gain) for unsigned ADC counts and
    // FSR/(2^bit_depth-1*gain) for signed ADC counts. Since we are using
    // the rational form, the unit is always in volts.
    virtual rational_type sensitivity(void) const = 0;

    // number of active channels that have been configured
    virtual std::uint32_t enabled_channels(void) const = 0;

    // if returns true, then each column will include the time each sample
    // was taken relative to the start trigger in std::nanoseconds. This
    // includes the size needed to store this value. ie
    // sizeof(std::chrono::nanoseconds::rep). Endian is the same as that of
    // ADC_counts_big_endian
    virtual bool stats(void) const = 0;


    // board-specific data handlers. If not applicable, or not implemented,
    // then return empty data handler to indicate n/a
#if 0
    // output to screen
    virtual data_handler screen_printer(void) const = 0;

    // output to file
    virtual data_handler file_printer(const fs::path &loc) const = 0;

    // nothing with the data
    virtual data_handler null_consumer(void) const = 0;
#endif

};



#endif
