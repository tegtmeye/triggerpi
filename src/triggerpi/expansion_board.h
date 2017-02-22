/*
    Abstract class for an expansion board
 */

#ifndef EXPANSION_BOARD_H
#define EXPANSION_BOARD_H

#include <config.h>

#include "bits.h"
#include "basic_trigger.h" // needs to go away

#include <boost/program_options.hpp>
#include <boost/rational.hpp>
#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <atomic>

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
  virtual expansion_board * construct(void) const = 0;

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

  expansion_board * construct(void) const {
    return new T();
  }

  expansion_factory * clone(void) const {
    return new expansion_factory<T>();
  }
};


enum class trigger_type : unsigned int {
  none = 0,
  single_shot = (1 << 0),
  intermittent = ((1 << 1) | single_shot)
};

inline constexpr trigger_type
operator&(trigger_type __x, trigger_type __y)
{
  return static_cast<trigger_type>
    (static_cast<unsigned int>(__x) & static_cast<unsigned int>(__y));
}

inline constexpr trigger_type
operator|(trigger_type __x, trigger_type __y)
{
  return static_cast<trigger_type>
    (static_cast<unsigned int>(__x) | static_cast<unsigned int>(__y));
}

inline constexpr trigger_type
operator^(trigger_type __x, trigger_type __y)
{
  return static_cast<trigger_type>
    (static_cast<unsigned int>(__x) ^ static_cast<unsigned int>(__y));
}


class expansion_board {
  public:
    /*
      If a board trigger configuration is:
        - none: This expansion is not a trigger source or sink depending on
        where `c none is used

        - trigger_type::single_shot: the source is enabled and then optionally disabled at
        some later point in time. Once disabled, it should never again be
        enabled.

        - intermittent: the source may be continuously enabled and then
        disabled. Intermittent implies trigger_type::single_shot


      The triggers may be 'or-ed' together if it can be used as either a
      trigger_type::single_shot or intermittent. That is: (trigger_type::single_shot | intermittent).
    */

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

    expansion_board(trigger_type source=trigger_type::none,
      trigger_type sink=trigger_type::none);

    virtual ~expansion_board(void) {}


    // These member functions are called in this order

    // configure any necessary options. This includes options validation prior
    // to bringing up the board
    // Pre: none
    // Post: none
    virtual void configure_options(const po::variables_map &) {}

    // setup and enable whatever communication mechanism is needed to talk to
    // this system.
    // Pre: none
    // Post: The system is set up for communication only
    virtual void setup_com(void) {}

    /*
      Ready the sytem to begin to function.
      Pre: communications is set up for this system (see setup_com)
      Post: The board is ready to function
    */
    virtual void initialize(void) {}

    /*
      Run this expansion board. Listen for a potential asynchronous stop
      call.

      Pre: the board has been initialized
      Post: The board has not hanging state. That is, the expansion board
      can be safely stopped at any time
    */
    virtual void run(void) = 0;

    /*
      Stop--may be called asynchronously

      Pre: the board has been started
      Post: The run function should return as soon as possible.
    */
     virtual void stop(void) {}

    // shutdown whatever communication mechanism is needed to talk to
    // this system.
    // Pre: setup_com() has been previously called
    // Post: No further attempts to communicate with this system will be
    //      made by this object
    virtual void finalize(void) {}



    // Board identifiers

    // return the system unique description in human readable form
    // ie (make and model)
    virtual std::string system_description(void) const = 0;


    // Trigger state query, waiting, and notification
    void configure_trigger_sink(
      const std::shared_ptr<expansion_board> &sink);

    /*
      Wait until receiving a trigger from the configured trigger source
    */
    void wait_on_trigger(void);

    /*
      Return true if received a trigger from the configured trigger source
    */
    bool is_triggered(void) const;

    /*
      Trigger all listening trigger sinks
    */
    void trigger_start(void);

    /*
      Cancel the trigger for all listening trigger sinks
    */
    void trigger_stop(void);

    /*
      Enabling/disabling of this expansion
    */
    void enable(void) {_enabled = true;}

    void disable(void) {_enabled = false;}

    bool is_enabled(void) const {return _enabled;}

    /*
      Trigger source/sink configuration

      By default all expansions can be configured as both sources and sinks.
      To tailor in inherited classes, use constructor. That is,
      source or sink-ness is not a configuration option, it is a class trait.
    */
    trigger_type trigger_source_type(void) const;
    trigger_type trigger_sink_type(void) const;


  private:
    struct _trigger {
      std::mutex m;
      std::condition_variable cv;
      std::atomic<bool> _flag;
    };

    static factory_map_type & _factory_map(void);

    // This object is a trigger source to other objects
    std::shared_ptr<_trigger> _trigger_source;

    // This object listens as a trigger sink
    std::shared_ptr<_trigger> _trigger_sink;

    bool _enabled;
    trigger_type _trigger_source_type;
    trigger_type _trigger_sink_type;
};




inline expansion_board::expansion_board(trigger_type source, trigger_type sink)
  :_enabled(false), _trigger_source_type(source), _trigger_sink_type(sink)
{
}

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

inline void expansion_board::configure_trigger_sink(
  const std::shared_ptr<expansion_board> &sink)
{
  // lazy instantiate
  if(!_trigger_source)
    _trigger_source.reset(new _trigger());

  sink->_trigger_sink = _trigger_source;
}

inline void expansion_board::wait_on_trigger(void)
{
  assert(_trigger_sink);

  std::unique_lock<std::mutex> lk(_trigger_sink->m);
  _trigger_sink->cv.wait(lk, [=]{return _trigger_sink->_flag.load();});
}

inline bool expansion_board::is_triggered(void) const
{
  return (_trigger_sink && _trigger_sink->_flag.load());
}

inline void expansion_board::trigger_start(void)
{
  if(!_trigger_source)
    return;

  std::unique_lock<std::mutex> lk(_trigger_source->m);
  _trigger_source->_flag.store(true);
  lk.unlock();
  _trigger_source->cv.notify_all();
}

inline void expansion_board::trigger_stop(void)
{
  if(!_trigger_source)
    return;

  std::unique_lock<std::mutex> lk(_trigger_source->m);
  _trigger_source->_flag.store(false);
  lk.unlock();
  _trigger_source->cv.notify_all();
}

inline trigger_type
expansion_board::trigger_source_type(void) const
{
  return _trigger_source_type;
}

inline trigger_type
expansion_board::trigger_sink_type(void) const
{
  return _trigger_sink_type;
}


#endif
