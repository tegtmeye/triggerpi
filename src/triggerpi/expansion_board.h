/*
    Abstract class for an ADC board
 */

#ifndef EXPANSION_BOARD_H
#define EXPANSION_BOARD_H

#include <config.h>

#include "bits.h"
#include "basic_trigger.h"

#include <boost/program_options.hpp>
#include <boost/rational.hpp>
#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <iostream>

namespace b = boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;


class expansion_board;

struct basic_expansion_factory {
  /*
    Options to be available to the command line parser. As a convention,
    each should be in the namespace specified by system_config_name. That is,
    if the expansion config name is 'foo', then the board options should be
    foo.option1, foo.option2, etc. This is not checked so be careful!
  */
  virtual po::options_description cmd_options(void) const = 0;

  /*
    The expansion name that will cause this expansion to be enabled. It is
    checked for uniqueness. By convention, this name should be a prefix
    followed by a period before each board option therefore it should be as
    short and distinct as possible
  */
  virtual std::string system_config_name(void) const = 0;

  /*
    A short human readable to describe this expansion system. It is not checked
    for uniqueness. Although can be longer then the value of
    'system_config_name', it should not be more than about a line.
  */
  virtual std::string system_config_desc_short(void) const = 0;

  /*
    A human readable to describe this expansion system. It is not checked
    for uniqueness. Although can be longer as needed, it should not be more
    than a few lines.
  */
  virtual std::string system_config_desc_long(void) const = 0;

  /*
    Construct an object of the factory's expansion board type.
  */
  virtual expansion_board * construct(const po::variables_map &vm) const = 0;

  /*
    Clone this expansion factory for storage
  */
  virtual basic_expansion_factory * clone(void) const = 0;
};

template<typename T>
struct expansion_factory : public basic_expansion_factory {
  po::options_description cmd_options(void) const {
    return T::cmd_options();
  }

  std::string system_config_name(void) const {
    return T::system_config_name();
  }

  std::string system_config_desc_short(void) const {
    return T::system_config_desc_short();
  }

  std::string system_config_desc_long(void) const {
    return T::system_config_desc_long();
  }

  expansion_board * construct(const po::variables_map &vm) const {
    return new T(vm);
  }

  expansion_factory * clone(void) const {
    return new expansion_factory<T>();
  }
};

class expansion_board {
  public:
    typedef std::map<std::string,
      std::shared_ptr<basic_expansion_factory> > factory_map_type;

    static void register_expansion(const basic_expansion_factory &factory);
    static const factory_map_type & factory_map(void);


    // All functions returning this rational type are exact as to preserve
    // downstream calculations such that significant figures can be applied
    // as appropriate. That is, if this value is 1/3, then it is exact. If
    // this value needs to be later converted to 'n' significant figures,
    // then it can without compromising later calculations due to premature
    // conversion.
    typedef b::rational<std::uint64_t> rational_type;
    typedef std::function<
      bool(void *data, std::size_t rows, const expansion_board &board)>
        data_handler;

    expansion_board(const po::variables_map &vm) :_vm(vm) {}
    virtual ~expansion_board(void) {}

    // These member functions are called in this order

    // configure any necessary options. This includes options validation prior
    // to bringing up the board
    // Pre: none
    // Post: none
    virtual void configure_options(void) = 0;

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
    // argument a pointer of a n dimensional array of data, the second
    // argument is this object. The n dimensional array type and size is
    // determined by the state variables in this object. That is, if
    // this->bit_depth() = 24, then the data is laid out as a 24 bit number
    // same for this->ADC_counts_signed(). If seconds is less than zero
    // then sample continusously
    virtual void trigger_sampling(const data_handler &handler,
      basic_trigger &trigger) = 0;

    // Board identifiers

    // return the system unique description in human readable form
    // ie (make and model)
    virtual std::string system_description(void) const = 0;

    // State information

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


    // This function is called prior to calling \c setup_com. If this
    // function returns true, then no further calls will be made to this
    // object.
    virtual bool disabled(void) const = 0;


    // board-specific data handlers. If not applicable, or not implemented,
    // then return empty data handler to indicate n/a

    // output to screen
    virtual data_handler screen_printer(void) const = 0;

    // output to file
    virtual data_handler file_printer(const fs::path &loc) const = 0;

    // nothing with the data
    virtual data_handler null_consumer(void) const = 0;

  protected:
    const po::variables_map &_vm;

  private:
    static factory_map_type & _factory_map(void);
};


struct do_nothing_handler {
  bool operator()(void *_data, std::size_t num_rows, const expansion_board &adc_board)
  {
    return false;
  }
};

inline void
expansion_board::register_expansion(const basic_expansion_factory &factory)
{
  std::string config_name = factory.system_config_name();
  if(_factory_map().count(config_name)) {
    std::stringstream err;
    err << "Expansion board configuration name clash for " << config_name;
    throw std::logic_error(err.str());
  }

  _factory_map()[config_name] =
    std::shared_ptr<basic_expansion_factory>(factory.clone());
}

inline const expansion_board::factory_map_type &
expansion_board::factory_map(void)
{
  return _factory_map();
}

inline expansion_board::factory_map_type & expansion_board::_factory_map(void)
{
  static factory_map_type fmap;

  return fmap;
}

// inline po::options_description expansion_board::registered_options(void)
// {
//   po::options_description result;
//
//   for (auto & cur : factory_map())
//     result.add(cur.second.registered_options());
//
//   return result;
// }


#endif
