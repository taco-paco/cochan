#pragma once

#include <cstdint>
#include <cstddef>
#include <queue>
#include <mutex>
#include <optional>
#include <list>
#include <coroutine>
#include <utility>

#include <co_chan/utils.hpp>

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
template< class T >
class channel
{
  public:
    using storage_type = std::aligned_storage< sizeof( T ), alignof( T ) >::type;

    channel()
        : capacity( 1 )
    {
    }

    explicit channel( std::size_t theCapacity )
        : capacity( theCapacity )
    {
        ASSERT_FORMAT( theCapacity != 0, "Channel capacity must be greater than 0" );
    }

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

    bool handleSend( std::pair< T*, std::coroutine_handle<> > value )
    {
        std::unique_lock< std::mutex > guard( mutex );
        const auto currentSize = sendQueue.size();

        ASSERT( currentSize <= capacity, "Queue got larger than capacity. bug" );
        if( sendQueue.size() == capacity )
        {
            senderWaiters.push_back( value );
            return true;
        }

        // If not empty there are no receivers so just handleSend
        if( !sendQueue.empty() )
        {
            ASSERT( receiverWaiters.empty(), "Bug or wrong assumption of that being impossible" );
            sendQueue.emplace( std::move( *value.first ) );
            return false;
        }

        // If there's receiver just propagate value in its slot
        if( !receiverWaiters.empty() )
        {
            const auto [ storage_ptr, receiverHandle ] = receiverWaiters.front();
            receiverWaiters.pop_front();
            ::new( storage_ptr ) T( std::move( *value.first ) );

            // Prevent double-locks
            guard.unlock();

            scheduler.schedule( receiverHandle );
            return false;
        }

        sendQueue.emplace( std::move( *value.first ) );
        return false;
    }

    bool handleReceive( std::pair< storage_type*, std::coroutine_handle<> > receiver )
    {
        std::unique_lock< std::mutex > guard( mutex );

        // Nothing to receive - park
        if( sendQueue.empty() )
        {
            receiverWaiters.push_back( receiver );
            return true;
        }

        const T value = std::move( sendQueue.front() );
        sendQueue.pop();

        // Was full and have sender waiters
        if( sendQueue.size() + 1 == capacity && !senderWaiters.empty() )
        {
            const auto [ senderValue, senderHandle ] = senderWaiters.front();
            senderWaiters.pop_front();
            sendQueue.emplace( std::move( *senderValue ) );

            // Prevent double-locks
            guard.unlock();

            scheduler.schedule( senderHandle );
        }

        new( receiver.first ) T( std::move( value ) );
        return false;
    }

  private:
    // TODO: allow?
    channel( const channel& ) = delete;
    channel( channel&& ) = delete;

    mutable std::mutex mutex;

    // TODO: generic
    DummyScheduler scheduler;

    std::size_t capacity;
    std::queue< T > sendQueue; // TODO: change queue type + circular buffer

    std::list< std::pair< T*, std::coroutine_handle<> > > senderWaiters;
    std::list< std::pair< storage_type*, std::coroutine_handle<> > > receiverWaiters;
};