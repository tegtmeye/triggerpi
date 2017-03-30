#ifndef TRIGGERPI_DATA_POOL_H
#define TRIGGERPI_DATA_POOL_H

#include <boost/circular_buffer.hpp>

#include <iostream>
#include <sstream>

#if 0
std::string find_alignment(void *ptr)
{
  std::size_t val = reinterpret_cast<std::size_t>(ptr);
  std::stringstream out;

  std::size_t one(1);
  std::size_t mod;
  for(std::size_t i=0; i<sizeof(std::size_t)*8; ++i) {
    mod = (one << i);
    //std::cerr << "i= " << i << " " << mod << "\n";
    if(val % mod == 0)
      out << ":" << mod;
  }

  return out.str();
}
#endif

inline bool check_alignment(void *ptr, std::size_t align)
{
  return (reinterpret_cast<std::size_t>(ptr) % align) == 0;
}

/*

*/
class shared_block_buffer {
  public:
    class shared_block_ptr;

    shared_block_buffer(void) = default;
    shared_block_buffer(std::size_t buff_size, std::size_t buff_align,
      std::size_t capacity);
    ~shared_block_buffer(void) = default;

    shared_block_buffer(shared_block_buffer &&rhs) = default;
    shared_block_buffer(const shared_block_buffer &rhs) = delete;
    shared_block_buffer & operator=(shared_block_buffer &rhs) = delete;

    std::size_t capacity(void) const;

    // if empty return value then no room in queue
    shared_block_ptr allocate(void);

  private:
    friend class shared_block_ptr;

    typedef boost::circular_buffer<std::size_t> index_queue_type;

    /*
      Storage memory Layout:

      | PRE_PAD |          MANAGED STORAGE                 | POST_PAD |
      |         | if buff_align > head_align               |          |
      |         |   BUFFER-PAD-HEADER                      |          |
      |         | else                                     |          |
      |         |   HEADER-BUFFER-PAD                      |          |
      |         *<- _storage (first occurrence of HEADER)             |
      |<-_storage_base                                                |


      Managed storage layout:

      |   BUFFER   |   PAD   |   HEADER   |   BUFFER   |   PAD   |   HEADER   |
      |                      |            |                      |            |
      |< buff_align          |< head_align|< buff_align          |< head_align|
      |<-------------------->|            |<-------------------->|            |
      |     _block_size      |            |     _block_size      |            |
      |<--------------------------------->|<--------------------------------->|
      |           _element_size           |           _element_size           |
    */

    /*
      If buff_align > head_align, then increase capacity by 1 element to
      account for an offset left for the header. Align the whole storage
      against the greater of buff_align and head_align.

    */
    struct _header {
      index_queue_type *_queue;
      std::size_t _id;
      std::atomic<std::size_t> _count;
    };

    std::size_t _buff_size;
    std::size_t _buff_align;
    std::size_t _block_size;
    std::size_t _element_size;

    std::size_t _capacity;
    std::shared_ptr<char> _storage_base;
    char *_storage;

    index_queue_type _queue;

    _header * n_header(std::size_t n);
    char * n_buffer(std::size_t n);

    /*
      C++11 align is only guaranteed for sizes up to max fundamental types.
      C++17 fixes this to guarantee for all powers of two. LLVM's implementation
      (even C++11) implements for all powers of two. Reproduce it here to
      guarantee correct behavior;
    */
    char* llvm_align(size_t alignment, size_t size, char*& ptr, size_t& space);
};

class shared_block_buffer::shared_block_ptr {
  public:
    shared_block_ptr(void) :_block(0) {}

    ~shared_block_ptr(void);

    shared_block_ptr(shared_block_ptr &&) = default;
    shared_block_ptr(const shared_block_ptr &rhs);

    shared_block_ptr & operator=(const shared_block_ptr &rhs);

    void * get(void) {return _block;}
    operator bool(void) const {return _block;}

  private:
    friend class shared_block_buffer;

    constexpr static const std::size_t _hoff =
      sizeof(shared_block_buffer::_header);
    /*
      _block points to the first element of &(_superblock._buff[0]);
    */
    char *_block;

    // assumes count has already been incremented
    shared_block_ptr(char *block) :_block(block) {}

    void increment(void);
    std::size_t decrement(void);
    void deallocate(void);
};











inline shared_block_buffer::shared_block_buffer(std::size_t buff_size,
  std::size_t buff_align, std::size_t capacity)
    :_buff_size(buff_size), _buff_align(buff_align), _capacity(capacity),
      _queue(capacity)
{
  std::size_t stored_elements = capacity;
  std::size_t align = alignof(_header);

  if(buff_align > alignof(_header)) {
    ++stored_elements;
    align = buff_align;
  }

  /*
    Calculate PAD so that _element_size is rounded up to a multiple of align.
    _align is decremented to allow fast alignment computation
  */
  std::size_t _align = align - 1;
  _element_size = ((buff_size + sizeof(_header)) + _align) & ~_align;
  _block_size = _element_size - sizeof(_header);

  /*
    Worse case scenario is that operator new returns a pointer to memory
    that we need to adjust by alignof(_header)-1
  */
  std::size_t length = _element_size*stored_elements+align;

  _storage_base.reset(new char[length]);

  _storage = _storage_base.get();
  _storage = llvm_align(align,_element_size*stored_elements,_storage,length);

  assert(_storage != 0);

  /*
    if BUFFER-PAD-HEADER, adjust for a partial first element if needed.
    ie make sure _storage always points to the start of HEADER
  */
  if(buff_align > alignof(_header))
    _storage += _block_size;

#if 0
  std::cerr << "allocating:\n"
    << (buff_align > alignof(_header)?"BUFFER-PAD-HEADER":"HEADER-BUFFER-PAD")
      << " with alignment: " << align << "\n"
    << "_storage_base: " << static_cast<void*>(_storage_base.get())
      << " (" << find_alignment(_storage_base.get()) << ")\n"
    << "_storage: " << static_cast<void*>(_storage)
      << " (" << find_alignment(_storage) << ")\n"
    << "storage-base offset: " << (_storage - _storage_base.get()) << "\n"
    << "alignof(_header): " << alignof(_header) << "\n"
    << "alignof(_buffer): " << buff_align << "\n"
    << "HEADER: " << sizeof(_header) << "\n"
    << "BUFFER: " << _buff_size << "\n"
    << "PAD: " << _block_size-_buff_size << "\n"
    << "_block_size: " << _block_size << "\n"
    << "_element_size: " << _element_size << "\n"
    << "capacity: " << capacity << "\n"
    << "stored_elements: " << stored_elements << "\n"
    << "total bytes: " << length << "\n";
#endif

  // initialize the header and fill the queue
  for(std::size_t i=0; i<capacity; ++i) {
    _header *head = n_header(i);
    head->_queue = &_queue;
    head->_id = i;
    head->_count = 0;
    _queue.push_back(i);
  }
}

inline std::size_t shared_block_buffer::capacity(void) const
{
  return _capacity;
}

inline shared_block_buffer::_header *
shared_block_buffer::n_header(std::size_t n)
{
  void *elem = _storage+(_element_size*n);
  assert(check_alignment(elem,alignof(_header)));

  return static_cast<_header*>(elem);
}

inline char * shared_block_buffer::n_buffer(std::size_t n)
{
  char *buf = (_storage+(_element_size*n)+sizeof(_header));

  assert(check_alignment(buf,_buff_align));

  return buf;
}

inline shared_block_buffer::shared_block_ptr shared_block_buffer::allocate(void)
{
  if(_queue.empty())
    return shared_block_ptr();

  std::size_t id = _queue.front();
  _queue.pop_front();

  n_header(id)->_count = 1;

  return shared_block_ptr(n_buffer(id));
}

/*
  Reproduces LLVM's std::align function in memory.cpp under the MIT license.

  Call parameters, results, and side effects are same as std::align
*/
inline char* shared_block_buffer::llvm_align(std::size_t alignment,
  std::size_t size, char*& ptr, std::size_t& space)
{
  char* r = 0;
  if (size <= space) {
    char* p1 = ptr;
    char* p2 = reinterpret_cast<char*>(
      reinterpret_cast<size_t>(p1 + (alignment - 1)) & -alignment);

    std::size_t d = static_cast<std::size_t>(p2 - p1);
    if (d <= space - size) {
      r = p2;
      ptr = r;
      space -= d;
    }
  }
  return r;
}







inline shared_block_buffer::shared_block_ptr::
  shared_block_ptr(const shared_block_ptr &rhs) :_block(rhs._block)
{
  if(_block)
    increment();
}

inline shared_block_buffer::shared_block_ptr::~shared_block_ptr(void)
{
  if(_block && decrement() == 0)
    deallocate();
}

inline shared_block_buffer::shared_block_ptr &
shared_block_buffer::shared_block_ptr::operator=(const shared_block_ptr &rhs)
{
  if(_block && decrement() == 0)
    deallocate();

  _block = rhs._block;
  if(_block)
    increment();

  return *this;
}

inline void shared_block_buffer::shared_block_ptr::increment(void)
{
  ++(reinterpret_cast<shared_block_buffer::_header *>(
    _block-_hoff)->_count);
}

inline std::size_t shared_block_buffer::shared_block_ptr::decrement(void)
{
  return --(reinterpret_cast<shared_block_buffer::_header *>(
    _block-_hoff)->_count);
}

inline void shared_block_buffer::shared_block_ptr::deallocate(void)
{
  shared_block_buffer::_header *header =
    reinterpret_cast<shared_block_buffer::_header *>(_block-_hoff);
  header->_queue->push_back(header->_id);
}


#endif
