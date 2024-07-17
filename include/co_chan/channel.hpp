#pragma once

#include <cstdint>
#include <cstddef>
#include <queue>
#include <mutex>
#include <optional>
#include <list>
#include <coroutine>
#include <utility>

class DummyScheduler
{
  public:
    DummyScheduler() = default;

    void schedule( std::coroutine_handle<> handle )
    {
        handle.resume();
    }
};

// TODO
// Copyable
// Movable?
// template <class T>
class channel
{
  public:
    channel();
    channel( std::size_t capacity );

    std::size_t getSize() const
    {
        const std::lock_guard< std::mutex > guard( mutex );
        return sendQueue.size();
    }

    std::size_t getCapacity() const
    {
        // TODO: check if safe without mutex
        return capacity;
    }

    bool push( std::pair< int, std::coroutine_handle<> > value );
    bool pop( std::pair< int*, std::coroutine_handle<> > receiver );

  private:
    // TODO: allow?
    channel( const channel& ) = delete;
    channel( channel&& ) = delete;

    mutable std::mutex mutex;

    // TODO: generic
    DummyScheduler scheduler;

    std::size_t capacity;
    std::queue< int > sendQueue; // TODO: change queue type + circular buffer

    std::list< std::pair< int, std::coroutine_handle<> > > senderWaiters;
    std::list< std::pair< int*, std::coroutine_handle<> > > receiverWaiters;
};