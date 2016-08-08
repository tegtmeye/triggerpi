/*
    Abstract class for an ADC board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

#include <boost/program_options.hpp>
#include <boost/rational.hpp>

#include <tuple>
#include <functional>
#include <cstdint>

namespace b = boost;
namespace po = boost::program_options;

class ADC_board {
  public:
    // All functions returning this rational type are exact as to preserve
    // downstream calculations such that significant figures can be applied
    // as appropriate. That is, if this value is 1/3, then it is exact. If
    // this value needs to be later converted to 'n' significant figures,
    // then it can without compromising later calculations due to premature
    // conversion.
    typedef b::rational<std::uint64_t> rational_type;
    typedef std::function<
      bool(void *, std::size_t, std::size_t)> sample_callback_type;

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

    // Start sampling the ADC. The callback will be called with the first
    // argument a pointer of a two dimensional array of data, the second
    // argument is the first dimension and corresponds to the number of
    // enabled channels, and the third argument is the second dimension
    // that corresponds to the number of samples given in trigger_sampling.
    // If this callback returns false, sampling stops. The type of the data
    // in the first argument is the same or next size larger unsigned data type
    // corresponding to the bit-depth of the ADC. That is, for an 8-bit ADC
    // the data is of type uint8_t. If it is a 24-bit ADC, the data is uint32_t.
    virtual void trigger_sampling(const sample_callback_type &callback,
      std::size_t samples) = 0;

    // State information

    // number of bits for each sample
    virtual std::uint32_t bit_depth(void) const = 0;

    // True if the ADC counts be considered a signed integer. This is the
    // native ADC format---not whether or not your measurement will generate a
    // negative number or not. Most true-differential ADCs will have a native
    // unsigned format.
    virtual bool ADC_counts_signed(void) const = 0;

    // The full-scale range expressed as a positive rational number.
    // Using the rational form ensures significant digit carryover to
    // calculations. Thus, the sensitivity can be expressed as:
    // FSR/(2^24*gain) for unsigned ADC counts and FSR/(2^23-1*gain) for
    // signed ADC counts
    virtual rational_type sensitivity(void) const = 0;

    // number of active channels that have been configured
    virtual std::uint32_t enabled_channels(void) const = 0;

    // This function is called prior to calling \c setup_com. If this
    // function returns true, then no further calls will be made to this
    // object.
    virtual bool disabled(void) const = 0;


  protected:
    const po::variables_map &_vm;
};

#endif
