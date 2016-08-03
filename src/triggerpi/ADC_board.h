/*
    Abstract class for an ADC board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

#include <boost/program_options.hpp>

namespace po = boost::program_options;

class ADC_board {
  public:
    ADC_board(const po::variables_map &vm) :_vm(vm) {}
    virtual ~ADC_board(void) {}

    // These member functions are called in this order

    // configure any necessary options. This includes options validation prior
    // to bringing up the board
    // Pre: none
    // Post: none
    virtual void configure_options(void) {}

    // setup and enable whatever communication mechanism is needed to talk to
    // this ADC system.
    // Pre: none
    // Post: The ADC is set up for communication only
    virtual void setup_com(void) = 0;

    // Ready the sytem to collect data.
    // Pre: communications is set up for this ADC (see setup_com_
    // Post: The ADC is ready to collect data once triggered
    virtual void initialize(void) = 0;

    // State information

    // This function is called prior to calling \c setup_com. If this
    // function returns true, then no further calls will be made to this
    // object.
    virtual bool disabled(void) const = 0;

  protected:
    const po::variables_map &_vm;
};

#endif
