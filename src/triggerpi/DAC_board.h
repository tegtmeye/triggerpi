/*
    Abstract class for an DAC board
 */

#ifndef DAC_BOARD_H
#define DAC_BOARD_H

#include <config.h>

#include "bits.h"

#include <boost/rational.hpp>
#include <boost/program_options.hpp>


namespace b = boost;
namespace po = boost::program_options;



class DAC_board {
  public:
    // All functions returning this rational type are exact as to preserve
    // downstream calculations such that significant figures can be applied
    // as appropriate. That is, if this value is 1/3, then it is exact. If
    // this value needs to be later converted to 'n' significant figures,
    // then it can without compromising later calculations due to premature
    // conversion.
    typedef b::rational<std::uint64_t> rational_type;

    DAC_board(const po::variables_map &vm) :_vm(vm) {}
    virtual ~DAC_board(void) {}

    // These member functions are called in this order

    // configure any necessary options. This includes options validation prior
    // to bringing up the board
    // Pre: none
    // Post: none
    virtual void configure_options(void) = 0;

    // setup and enable whatever communication mechanism is needed to talk to
    // this system.
    // Pre: none
    // Post: The system is set up for communication only
    virtual void setup_com(void) = 0;

    // Ready the sytem to handle data.
    // Pre: communications is set up for this system (see setup_com)
    // Post: The system is ready to handle data
    virtual void initialize(void) = 0;



    // Board identifiers

    // return the system make and model
    virtual std::string board_name(void) const = 0;

    // State information

    // number of bits for each sample
    virtual std::uint32_t bit_depth(void) const = 0;

    // The sensitivity expressed as a rational number.
    // Using the rational form ensures significant digit carryover to
    // calculations. Thus, the sensitivity can be expressed as:
    // FSR/(2^bit_depth*gain) for unsigned ADC counts and
    // FSR/(2^bit_depth-1*gain) for signed ADC counts. Since we are using
    // the rational form, the unit is always in volts.
    virtual rational_type sensitivity(void) const = 0;

    // number of active channels that have been configured
    virtual std::uint32_t enabled_channels(void) const = 0;

  protected:
    const po::variables_map &_vm;
};



#endif
