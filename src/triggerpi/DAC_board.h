/*
    Abstract class for an DAC expansion board
 */

#ifndef DAC_BOARD_H
#define DAC_BOARD_H

#include <config.h>

#include "bits.h"
#include "expansion_board.h"

#include <boost/rational.hpp>
#include <boost/program_options.hpp>


namespace b = boost;
namespace po = boost::program_options;

/*
  Abstract class for DAC expansion boards. Defines additional
  board-specific calls. Any base class calls are provided here for
  exposition only

*/
class DAC_board : public expansion_board {
  public:
    // All functions returning this rational type are exact as to preserve
    // downstream calculations such that significant figures can be applied
    // as appropriate. That is, if this value is 1/3, then it is exact. If
    // this value needs to be later converted to 'n' significant figures,
    // then it can without compromising later calculations due to premature
    // conversion.
    typedef b::rational<std::uint64_t> rational_type;
};



#endif
