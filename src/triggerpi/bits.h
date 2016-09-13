/*
    Bit/Byte and other utilities
*/

#ifndef BITS_H
#define BITS_H

#include <boost/program_options.hpp>

#include <config.h>

#include <cstdint>

namespace detail {

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

// convenience function for verbosity
template<unsigned int Level>
inline bool is_verbose(const boost::program_options::variables_map &vm)
{
  return (vm.count("verbose") && vm["verbose"].as<unsigned int>() >= Level);
}

}


#endif
