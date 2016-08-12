/*
    Abstract class for an ADC board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

#include <config.h>

#include <boost/program_options.hpp>
#include <boost/rational.hpp>
#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <functional>

namespace b = boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;


// Endian swap convenience functions
inline std::uint16_t swap_bytes(std::uint16_t val)
{
  return
    ((val & 0xFF00) >> 8) |
    ((val & 0x00FF) << 8);
}

inline std::int16_t swap_bytes(std::int16_t val)
{
  return
    ((val & 0xFF00) >> 8) |
    ((val & 0x00FF) << 8);
}

inline std::uint32_t swap_bytes(std::uint32_t val)
{
  return
    ((val & 0xFF000000) >> 24) |
    ((val & 0x00FF0000) >> 8) |
    ((val & 0x0000FF00) << 8) |
    ((val & 0x000000FF) << 24);
}

inline std::int32_t swap_bytes(std::int32_t val)
{
  return
    ((val & 0xFF000000) >> 24) |
    ((val & 0x00FF0000) >> 8) |
    ((val & 0x0000FF00) << 8) |
    ((val & 0x000000FF) << 24);
}

inline std::uint64_t swap_bytes(std::uint64_t val)
{
  return
    ((val & 0xFF00000000000000ULL) >> 56) |
    ((val & 0x00FF000000000000ULL) >> 40) |
    ((val & 0x0000FF0000000000ULL) >> 24) |
    ((val & 0x000000FF00000000ULL) >>  8) |
    ((val & 0x00000000FF000000ULL) <<  8) |
    ((val & 0x0000000000FF0000ULL) << 24) |
    ((val & 0x000000000000FF00ULL) << 40) |
    ((val & 0x00000000000000FFULL) << 56);
}

inline std::int64_t swap_bytes(std::int64_t val)
{
  return
    ((val & 0xFF00000000000000ULL) >> 56) |
    ((val & 0x00FF000000000000ULL) >> 40) |
    ((val & 0x0000FF0000000000ULL) >> 24) |
    ((val & 0x000000FF00000000ULL) >>  8) |
    ((val & 0x00000000FF000000ULL) <<  8) |
    ((val & 0x0000000000FF0000ULL) << 24) |
    ((val & 0x000000000000FF00ULL) << 40) |
    ((val & 0x00000000000000FFULL) << 56);
}

template<typename T>
inline T ensure_be(T val)
{
#if WORDS_BIGENDIAN
  return val;
#else
  return swap_bytes(val);
#endif
}

template<typename T>
inline T ensure_le(T val)
{
#if WORDS_BIGENDIAN
  return swap_bytes(val);
#else
  return val;
#endif
}

template<typename T>
inline T le_to_native(T val)
{
#if WORDS_BIGENDIAN
  return swap_bytes(val);
#else
  return val;
#endif
}

template<typename T>
inline T be_to_native(T val)
{
#if WORDS_BIGENDIAN
  return val;
#else
  return swap_bytes(val);
#endif
}


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
      bool(void *data, std::size_t rows, const ADC_board &board)> data_handler;

    ADC_board(const po::variables_map &vm) :_vm(vm) {}
    virtual ~ADC_board(void) {}

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
    // same for this->ADC_counts_signed()
    virtual void trigger_sampling(const data_handler &handler) = 0;

    virtual void trigger_sampling_async(const data_handler &handler) = 0;

    // State information

    // number of bits for each sample
    virtual std::uint32_t bit_depth(void) const = 0;

    // True if the ADC counts be considered a signed integer. This is the
    // native ADC format---not whether or not your measurement will generate a
    // negative number or not. Most true-differential ADCs will have a native
    // unsigned format.
    virtual bool ADC_counts_signed(void) const = 0;

    virtual bool ADC_counts_big_endian(void) const = 0;

    // The full-scale range expressed as a positive rational number.
    // Using the rational form ensures significant digit carryover to
    // calculations. Thus, the sensitivity can be expressed as:
    // FSR/(2^24*gain) for unsigned ADC counts and FSR/(2^23-1*gain) for
    // signed ADC counts
    virtual rational_type sensitivity(void) const = 0;

    // number of active channels that have been configured
    virtual std::uint32_t enabled_channels(void) const = 0;

    // if returns true, then each column will include the time each sample
    // was taken relative to the start trigger in std::nanoseconds. This
    // includes the size needed to store this value. ie
    // sizeof(std::chrono::nanoseconds::rep). Endian is native.
    virtual bool sample_time_prefix(void) const = 0;


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
};


struct do_nothing_handler {
  bool operator()(void *_data, std::size_t num_rows, const ADC_board &adc_board)
  {
    return false;
  }
};


#endif
