/*
    Abstract class for an ADC board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

#include <boost/program_options.hpp>

#include <functional>

namespace po = boost::program_options;

class ADC_board {
  public:
    typedef std::function<bool(void *)> sample_callback_type;

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

    // Start sampling the ADC. The callback will be called with a 2-dimensional
    // array of \c enabled_channels columns and \c samples rows. If this
    // callback returns false, sampling stops
    virtual void trigger_sampling(const sample_callback_type &callback,
      std::size_t samples) = 0;

    // State information

    // number of bits for each sample
    virtual unsigned int bit_depth(void) = 0;

    // number of active channels that have been configured
    virtual unsigned int enabled_channels(void) = 0;

    // This function is called prior to calling \c setup_com. If this
    // function returns true, then no further calls will be made to this
    // object.
    virtual bool disabled(void) const = 0;

  protected:
    const po::variables_map &_vm;
};

#endif
