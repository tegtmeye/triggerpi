#ifndef BASIC_TRIGGER_H
#define BASIC_TRIGGER_H

#include <atomic>
#include <mutex>
#include <condition_variable>


/*
    Abstract class for dealing with triggers
*/
class basic_trigger {
  public:
    virtual ~basic_trigger(void) {}

    virtual void wait_start(void) = 0;
    virtual void trigger_start(void) = 0;

    virtual bool should_stop(void) const = 0;
    virtual void trigger_stop(void) = 0;
};


/*
    Trivial trigger that starts immediately and never stops
*/
class indefinite_trigger :public basic_trigger {
  public:
    virtual void wait_start(void) {}
    virtual void trigger_start(void) {}

    virtual bool should_stop(void) const { return false; }
    virtual void trigger_stop(void) {}
};


/*
    Base class for coordinated trigger operations. That is, triggering and
    waiting can happen on different threads
*/
class coordinated_trigger :public basic_trigger {
  public:
    coordinated_trigger(void);
    virtual ~coordinated_trigger(void) {}

    virtual void wait_start(void);
    virtual void trigger_start(void);

    virtual bool should_stop(void) const;
    virtual void trigger_stop(void);

  private:
    std::mutex _mutex;
    std::condition_variable _condvar;
    bool start; // protected by mutex and condition variable
    std::atomic_bool stop;
};

inline coordinated_trigger::coordinated_trigger(void)
  :start(false), stop(false)
{
}

inline void coordinated_trigger::wait_start(void)
{
  std::unique_lock<std::mutex> lock(_mutex);
  while(!start)
    _condvar.wait(lock);
}

inline void coordinated_trigger::trigger_start(void)
{
  std::unique_lock<std::mutex> lock(_mutex);
  start = true;
  _condvar.notify_all();
}

inline bool coordinated_trigger::should_stop(void) const
{
  return stop.load();
}

inline void coordinated_trigger::trigger_stop(void)
{
  stop.store(true);
}

#endif
