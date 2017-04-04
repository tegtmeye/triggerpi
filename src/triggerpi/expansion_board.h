/*
    Abstract class for an expansion board
 */

#ifndef EXPANSION_BOARD_H
#define EXPANSION_BOARD_H

#include <config.h>

#include "bits.h"

#include "shared_block_buffer.h"

#include <boost/program_options.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <atomic>

#include <iostream>

namespace b = boost;
namespace po = boost::program_options;


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


enum class trigger_type_t : unsigned int {
  none = 0,
  single_shot_source = (1 << 0),
  intermittent_source = ((1 << 1) | single_shot_source),
  single_shot_sink = (1 << 2),
  intermittent_sink = ((1 << 3) | single_shot_sink),
};

inline constexpr trigger_type_t
operator&(trigger_type_t __x, trigger_type_t __y)
{
  return static_cast<trigger_type_t>
    (static_cast<unsigned int>(__x) & static_cast<unsigned int>(__y));
}

inline constexpr trigger_type_t
operator|(trigger_type_t __x, trigger_type_t __y)
{
  return static_cast<trigger_type_t>
    (static_cast<unsigned int>(__x) | static_cast<unsigned int>(__y));
}

inline constexpr trigger_type_t
operator^(trigger_type_t __x, trigger_type_t __y)
{
  return static_cast<trigger_type_t>
    (static_cast<unsigned int>(__x) ^ static_cast<unsigned int>(__y));
}

inline constexpr trigger_type_t source(trigger_type_t __x)
{
  return (__x & trigger_type_t::intermittent_source);
}

inline constexpr trigger_type_t sink(trigger_type_t __x)
{
  return (__x & trigger_type_t::intermittent_sink);
}

inline constexpr bool compatable_trigger(trigger_type_t source,
  trigger_type_t sink)
{
  return ((static_cast<unsigned int>(source) &
    (static_cast<unsigned int>(sink) >> 2)));
}

inline std::ostream & operator<<(std::ostream &os, const trigger_type_t &val)
{
  if(val == trigger_type_t::none)
    os << "none";

  if((source(val) & trigger_type_t::single_shot_source) ==
    trigger_type_t::single_shot_source)
  {
    os << "single shot source";
  }
  else if((source(val) & trigger_type_t::intermittent_source) ==
    trigger_type_t::intermittent_source)
  {
    os << "intermittent source";
  }

  if((sink(val) & trigger_type_t::single_shot_sink) ==
    trigger_type_t::single_shot_sink)
  {
    os << "single shot sink";
  }
  else if((sink(val) & trigger_type_t::intermittent_sink) ==
    trigger_type_t::intermittent_sink)
  {
    os << "intermittent sink";
  }


  return os;
}

enum class data_flow_t : unsigned int {
  none = 0,
  source = (1 << 0),
  sink = (1 << 1),
  transform = (source | sink)
};

inline constexpr data_flow_t
operator&(data_flow_t __x, data_flow_t __y)
{
  return static_cast<data_flow_t>
    (static_cast<unsigned int>(__x) & static_cast<unsigned int>(__y));
}

inline constexpr data_flow_t
operator|(data_flow_t __x, data_flow_t __y)
{
  return static_cast<data_flow_t>
    (static_cast<unsigned int>(__x) | static_cast<unsigned int>(__y));
}

inline constexpr data_flow_t
operator^(data_flow_t __x, data_flow_t __y)
{
  return static_cast<data_flow_t>
    (static_cast<unsigned int>(__x) ^ static_cast<unsigned int>(__y));
}

inline std::ostream & operator<<(std::ostream &os, const data_flow_t &val)
{
  if(val == data_flow_t::none)
    return (os << "none");

  if(val == data_flow_t::source)
    return (os << "source");

  if(val == data_flow_t::sink)
    return (os << "sink");

  return (os << "transform");
}



class expansion_board {
  public:
    /*
      If a board trigger configuration is: - none: This expansion is not
      a trigger source or sink depending on where none is used

        - trigger_type_t::single_shot: the source is enabled and then
        optionally disabled at some later point in time. Once disabled,
        it should never again be enabled.

        - intermittent: the source may be continuously enabled and then
        disabled. Intermittent implies trigger_type_t::single_shot


      The triggers may be 'or-ed' together if it can be used as either a
      trigger_type_t::single_shot or intermittent. That is:
      (trigger_type_t::single_shot | intermittent).
    */

    typedef std::map<std::string,
      std::shared_ptr<basic_expansion_factory> > factory_map_type;

    static void register_expansion(const basic_expansion_factory &factory);
    static const factory_map_type & factory_map(void);


    typedef shared_block_buffer::shared_block_ptr data_block_ptr;


    expansion_board(trigger_type_t trigger_type=trigger_type_t::none,
      data_flow_t data_flow=data_flow_t::none);

    expansion_board(data_flow_t data_flow=data_flow_t::none,
      trigger_type_t trigger_type=trigger_type_t::none);

    virtual ~expansion_board(void) {}


    // These member functions are called in this order

    // configure any necessary options. This includes options validation prior
    // to bringing up the board
    // Pre: none
    // Post: none
    virtual void configure_options(const po::variables_map &) {}

    /*
      Configure this object to receive a data buffer according to the
      configuration map \c config as described by the configured data
      source. If this object is not initialized as a data sink, then
      this function is skipped.

      Pre: The object is initialized as a data_sink
      Post: Data blocks retrieved by pop_data_block will be laid out
            according to the configuration described in \c config
    */
    virtual void data_sink_config(const std::map<std::string,std::string> &) {}

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

    // shutdown whatever communication mechanism is needed to talk to
    // this system.
    // Pre: setup_com() has been previously called
    // Post: No further attempts to communicate with this system will be
    //      made by this object
    virtual void finalize(void) {}



    // Board identifiers

    /*
      Return a short identifier representing this expansion. If possible,
      this string should be sufficient to unambiguously identify the
      expansion on the command line
    */
    virtual std::string system_identifier(void) const = 0;

    /*
      Return a description of the expansion in human readable form. This is
      string is used only when a long descriptive form is needed.
    */
    virtual std::string system_description(void) const = 0;


    // Trigger state query, waiting, and notification

    /*
      Wait until receiving a trigger start from the configured trigger source.
      This returns the value of is_triggered(). That is, if
      wait_on_trigger_start() returns true, the trigger was fired. If false,
      then the trigger was not fired and the wake up was due to the trigger
      source begin shut down.

      Example of proper use would be:

      if(!wait_on_trigger_start())
        //exit the run function
      else
        // do something now that triggered
    */
    bool wait_on_trigger_start(void);

    /*
      Wait until receiving a trigger stop from the configured trigger source
    */
    void wait_on_trigger_stop(void);

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
      Let all listening trigger sinks know that they will not be receiving
      any more trigger_start/stops and should should return from their run
      function. This call is sticky, that is, any call to \c trigger_start or
      \c trigger_stop will throw an error.

      This call is optional and will be called automatically when the run
      function returns. Reasons to call this manually would be if any
      nontrivial computation needs to be done after the last trigger_stop.
    */
    void trigger_shutdown(void);


    /*
      The trigger_stop has been called for the last time. Derived
      classes should poll this regularly during execution of the run
      function and return as soon as possible once \c final_trigger
      returns true.
    */
    bool final_trigger(void) const;

    /*
      Trigger source/sink configuration

      By default all expansions can be configured as both sources and sinks.
      To tailor in inherited classes, use constructor. That is,
      source or sink-ness is not a configuration option, it is a class trait.
    */
    trigger_type_t trigger_type(void) const;

    // This expansion is receiving trigger notifications from somewhere
    bool is_trigger_sink(void) const {
      return _trigger_sink.get();
    }

    // This expansion is providing trigger notifications to others
    bool is_trigger_source(void) const {
      return _trigger_source.get();
    }


    /*
      Data source/sink configuration

    */
    data_flow_t data_flow(void) const;

    /*
      Configure this object to use built-in data source functionality.

      If this object is configured as a data source, then \c buff_size
      blocks of raw memory aligned at \c align memory boundaries are
      allocated to be used by the data source object to pass data to
      configured data_sinks via the \c push_data_block function. This
      call reserves \c capacity data blocks ensuring that calls to \c
      allocate_data_block do not dynamically allocate memory. This also
      means that if data_sinks do not consume data blocks fast enough,
      then the reserved data_block capacity will be exceeded and calls
      to \c allocate_data_block will fail. Potential remedies included
      reducing the \c buff_size and/or increasing \c capacity.

      The \c config map is intended to be used to publish details of the
      buffer of data provided in \c push_data_block. Derived classes can
      provide whatever key/value pairs that best suit the application.
      The \c config map will be saved and provided to configured data
      sinks via the \c data_sink_config call.

      This call is independent of initializing this object as a
      data_flow_t::source although an exception will be thrown if the
      object is not initialized as such. That is, if
      \c configure_data_source_blocks is called, then derived objects may use
      \c allocate_data_block and \c push_data_block as needed. If
      \c configure_data_source_blocks is not called, then derived objects may
      still operate as a data source but implementation of the plumbing
      is entirely up to the derived classes.

      Pre: Object has been initialized as data_flow_t::source
    */
    void configure_data_source_blocks(std::size_t buff_size, std::size_t align,
      std::size_t capacity, const std::map<std::string,std::string> &config);

    /*
      Configure this object to use built-in data sink functionality.

      If this object is configured as a data sink, then calls to
      \c push_data_block by configured data sources will enqueue a
      \c data_block_ptr containing the sources data into a private queue
      of size \c queue_size within this object. This enqueue
      action happens on another thread and cannot be intercepted by
      derived classes. This is done to ensure that the other thread does
      not block while the data is processed by this object. If the
      internal queue is full, the sent data block will be lost and the
      sender will be notified that one of it's sink's enqueue failed. It
      is up to the data source on how this condition is best handled
      (ignore, resend, fail, etc).

      If this object has been initialized as a \c data_flow_t::sink,
      \c configure_data_sink_queue is not called, and another object is
      configured as this object's built in data source, then data sent
      to this object by calls to \c push_data_block by the data source
      will be silently dropped. This is not always a bad thing as a
      derived class may want to ignore data sent to it for certain
      run-time configurations.

      This call is independent of initializing this object as a
      data_flow_t::sink although an exception will be thrown if the
      object is not initialized as such. That is, if
      \c configure_data_sink_queue is called, then derived objects may use
      \c pop_data_block and \c data_block_available as needed. If
      \c configure_data_sink_queue is not called, then derived objects may
      still operate as a data sink but implementation of the plumbing
      is entirely up to the derived classes.

      Pre: Object has been initialized as data_flow_t::sink
    */
    void configure_data_sink_queue(std::size_t queue_size);

    /*
      Get an empty data block of size \c buff_size configured in the
      prerequisite call to \c configure_data_source_blocks. Block allocation is
      guaranteed to not allocate dynamic memory. In addition, when the
      last data_bloc_ptr goes out of scope, the allocated data block
      will automatically be returned to the allocation pool. This
      dynamic memory-free strategy ensures fast runtime behavior but
      also means that if the data block allocation pool is not
      configured to be large enough or data sinks do not process the
      block fast enough, then allocate_data_block may fail. That is, if
      return is empty, then all available data blocks are being
      processed by configured sinks and none are available.

      Pre: configure_data_source_blocks has been called
    */
    data_block_ptr allocate_data_block(void);

    /*
      Send a populated data block as the data source to all configured
      data sinks.

      If return is false, then one or more of the configured data sink
      queues is full and could not be enqueued. It is up to derived
      classes on how to most appropriately handle the failed enqueue. If
      a data source chooses to ignore data pushes by not calling \c
      configure_data_sink_queue, then this will have no effect on the
      return value;

      Configured built-in data sinks will be provided the length of the
      data block as configured in the prerequisite call to
      \c configure_data_source_blocks. It is the responsibility of the data
      source to provide an unambiguous description of how the contents
      of the data block are laid out and aligned so that sinks may
      appropriately consume the contents. Configured data sinks must
      also accept that the data may be laid out or sized differently
      based on runtime configuration of the data source and must react
      appropriately. At this time, there is no way to "pre-publish" to
      configured sinks a description of how this data is laid out.
      Although this may change in the future, this is done with the
      intention that source->sink exchanges are mostly stateless and
      lightweight.


      Pre: configure_data_source_blocks has been called
    */
    bool push_data_block(data_block_ptr ptr);

    /*
      Pull a populated data block as the data sink from the queue. If
      the returned data_block_ptr is empty, then there was no blocks
      enqueued at this time.
    */
    data_block_ptr pop_data_block(void);

    /*
      Return whether or not a data block has been enqueued and is
      available via \pop_data_block
    */
    bool data_block_available(void) const;


    /*
      Enabling/disabling of this expansion
    */
    void enable(void) {_enabled = true;}

    void disable(void) {_enabled = false;}

    bool is_enabled(void) const {return _enabled;}




    /*
      create the trigger source/sink hierarchy.
    */
    void configure_trigger_sink(const std::shared_ptr<expansion_board> &sink);

    /*
      create the data source/sink hierarchy.
    */
    void configure_data_sink(const std::shared_ptr<expansion_board> &sink);

  private:
    struct _trigger {
      std::mutex m;
      std::condition_variable cv;
      std::atomic<bool> _flag;

      std::atomic<bool> _final;
    };

    static factory_map_type & _factory_map(void);

    // This object is a trigger source to other objects
    std::shared_ptr<_trigger> _trigger_source;

    // The object that we are listening to as a trigger sink
    // (ie upstream object)
    std::shared_ptr<_trigger> _trigger_sink;

    std::map<std::string,std::string> _data_sink_config;

    // buffer of data blocks to be filled by source and enqueued to sinks
    std::shared_ptr<shared_block_buffer> _data_source_buff;

    // data sources place enqueued data_block_ptrs here
    std::shared_ptr<b::lockfree::spsc_queue<data_block_ptr> > _data_sink_queue;

    // Pointer to all the configured data sinks. Use weak to avoid reference
    // loops
    std::vector<expansion_board *> _data_sinks;

    bool _enabled;
    trigger_type_t _trigger_type;
    data_flow_t _data_flow;
};




inline expansion_board::expansion_board(trigger_type_t trigger_type,
  data_flow_t data_flow) :_enabled(false), _trigger_type(trigger_type),
    _data_flow(data_flow)
{
}

inline expansion_board::expansion_board(data_flow_t data_flow,
  trigger_type_t trigger_type) :_enabled(false), _trigger_type(trigger_type),
    _data_flow(data_flow)
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

inline bool expansion_board::wait_on_trigger_start(void)
{
  assert(_trigger_sink);

  std::unique_lock<std::mutex> lk(_trigger_sink->m);
  if(!_trigger_sink->_flag && !_trigger_sink->_final) {
    _trigger_sink->cv.wait(lk,
      [=] {
        return (_trigger_sink->_flag || _trigger_sink->_final);
      }
    );
  }

  return _trigger_sink->_flag;
}

inline void expansion_board::wait_on_trigger_stop(void)
{
  assert(_trigger_sink);

  std::unique_lock<std::mutex> lk(_trigger_sink->m);
  if(_trigger_sink->_flag)
    _trigger_sink->cv.wait(lk, [=]{return !_trigger_sink->_flag;});
}

inline bool expansion_board::is_triggered(void) const
{
  return (_trigger_sink && _trigger_sink->_flag);
}

inline void expansion_board::trigger_start(void)
{
  if(!is_trigger_source())
    return;

  assert(!(_trigger_source && _trigger_source->_final));

  std::unique_lock<std::mutex> lk(_trigger_source->m);
  _trigger_source->_flag = true;
  lk.unlock();
  _trigger_source->cv.notify_all();
}

inline void expansion_board::trigger_stop(void)
{
  if(!is_trigger_source())
    return;

  assert(!(_trigger_source && _trigger_source->_final));

  std::unique_lock<std::mutex> lk(_trigger_source->m);
  _trigger_source->_flag = false;
  lk.unlock();
  _trigger_source->cv.notify_all();
}

inline void expansion_board::trigger_shutdown(void)
{
  if(!is_trigger_source())
    return;

  std::unique_lock<std::mutex> lk(_trigger_source->m);
  _trigger_source->_final = true;
  lk.unlock();
  _trigger_source->cv.notify_all();
}

inline bool expansion_board::final_trigger(void) const
{
  assert(_trigger_sink);

  return _trigger_sink->_final;
}

inline trigger_type_t expansion_board::trigger_type(void) const
{
  return _trigger_type;
}





inline data_flow_t expansion_board::data_flow(void) const
{
  return _data_flow;
}

inline void expansion_board::configure_data_source_blocks(
  std::size_t buff_size, std::size_t align, std::size_t capacity,
  const std::map<std::string,std::string> &config)
{
  assert((_data_flow & data_flow_t::source) != data_flow_t::none);

  _data_sink_config = config;
  _data_source_buff
    = std::make_shared<shared_block_buffer>(buff_size,align,capacity);
}

inline void
expansion_board::configure_data_sink_queue(std::size_t queue_size)
{
  assert((_data_flow & data_flow_t::sink) != data_flow_t::none);

  _data_sink_queue = std::make_shared<
    b::lockfree::spsc_queue<data_block_ptr> >(queue_size);
}

inline expansion_board::data_block_ptr
expansion_board::allocate_data_block(void)
{
  assert(_data_source_buff);
  return _data_source_buff->allocate();
}

inline bool expansion_board::push_data_block(data_block_ptr ptr)
{
  bool result = true;

  for(auto &board_ptr : _data_sinks) {
    if(board_ptr->_data_sink_queue)
      result = board_ptr->_data_sink_queue->push(ptr) && result; //must be last
  }

  return result;
}

inline expansion_board::data_block_ptr expansion_board::pop_data_block(void)
{
  data_block_ptr result;
  _data_sink_queue->pop(result);
  return result;
}

inline bool expansion_board::data_block_available(void) const
{
  return (_data_sink_queue && _data_sink_queue->read_available());
}

inline void expansion_board::configure_trigger_sink(
  const std::shared_ptr<expansion_board> &sink)
{
  // lazy instantiate
  if(!_trigger_source)
    _trigger_source = std::make_shared<_trigger>();

  sink->_trigger_sink = _trigger_source;
}

inline void expansion_board::configure_data_sink(const std::shared_ptr<expansion_board> &sink)
{
  _data_sinks.push_back(sink.get());
}

#endif
