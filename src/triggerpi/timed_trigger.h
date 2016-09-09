#ifndef TIMED_TRIGGER_H
#define TIMED_TRIGGER_H

#include "basic_trigger.h"

#include <cstdint>
#include <chrono>
#include <thread>
#include <functional>

/*
    Trigger that starts immediately and triggers stop after the given time
    interval
*/
template<typename RepT=std::chrono::nanoseconds>
class basic_timed_trigger :public coordinated_trigger {
  public:
    typedef RepT duration_type;

    basic_timed_trigger(const duration_type &dur);

    template<typename Rep2T, typename Period>
    basic_timed_trigger(const std::chrono::duration<Rep2T,Period> &dur);
    basic_timed_trigger(const basic_timed_trigger &rhs) = delete;
    basic_timed_trigger & operator=(const basic_timed_trigger &rhs) = delete;
    ~basic_timed_trigger(void);

    virtual void wait_start(void);

  private:
    duration_type _duration;
    std::chrono::high_resolution_clock::time_point _start_time;

    std::mutex _start_mutex;
    std::thread _thread;

    void init(void);
    void wait_duration(void);
};

template<typename RepT>
inline basic_timed_trigger<RepT>::basic_timed_trigger(const duration_type &dur)
  :_duration(dur)
{
  init();
}

template<typename RepT>
template<typename Rep2T, typename Period>
inline basic_timed_trigger<RepT>::
  basic_timed_trigger(const std::chrono::duration<Rep2T,Period> &dur)
    :_duration(std::chrono::duration_cast<RepT>(dur))
{
  init();
}

template<typename RepT>
void basic_timed_trigger<RepT>::wait_start(void)
{
  //trigger the underlying system
  coordinated_trigger::trigger_start();
  //call base class wait (should return immediately)
  coordinated_trigger::wait_start();
  // get the current time
  _start_time = std::chrono::high_resolution_clock::now();
  //start the timing thread
  _start_mutex.unlock();
}

template<typename RepT>
basic_timed_trigger<RepT>::~basic_timed_trigger(void)
{
  _thread.join();
}

template<typename RepT>
inline void basic_timed_trigger<RepT>::init(void)
{
  /*
    must be in this order:
      -lock the mutex
      -spin off our thread. It will wait until the mutex is unlocked
  */
  _start_mutex.lock();
  _thread = std::thread(&basic_timed_trigger<RepT>::wait_duration,this);
}

/*
    Runs in another thread. Simply waits until the time duration has passed
    and then triggers stop.
*/
template<typename RepT>
void basic_timed_trigger<RepT>::wait_duration(void)
{
  // should wait on unlock until wait_start() is called
  _start_mutex.lock();

  while(std::chrono::high_resolution_clock::now() < (_start_time+_duration))
    std::this_thread::sleep_until(_start_time+_duration);

  _start_mutex.unlock();

  trigger_stop();
}

typedef basic_timed_trigger<> timed_trigger;

#endif
