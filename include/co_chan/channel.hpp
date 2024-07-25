#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <queue>
#include <mutex>
#include <optional>
#include <list>
#include <coroutine>
#include <utility>

#include <co_chan/utils.hpp>

class ChannelClosedException: public std::exception
{
  public:
    const char* what() const noexcept override
    {
        return "Channel closed";
    }
};

class DummyScheduler
{
  public:
    DummyScheduler() = default;

    void schedule( std::coroutine_handle<> handle )
    {
        handle.resume();
    }
};

template< typename T >
class Sender;

template< typename T >
class Receiver;

// TODO: case for copy_constructible only
template< std::movable T >
class channel
{
  public:
    ~channel()
    {
        ASSERT( receiverWaiters.empty(), "Should be handled by last sendable object" );
        ASSERT( senderWaiters.empty(), "Should be handled ny last receivable object" );
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

    void onSenderClose()
    {
        closed = true;

        std::unique_lock< std::mutex > guard( mutex );
        if( receiverWaiters.empty() )
        {
            return;
        }

        ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible" );
        const auto waitersCopy = receiverWaiters;
        receiverWaiters.clear();
        guard.unlock(); // Prevent double-lock

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ result, receiverHandle ] = el;
            *result = std::nullopt;
            this->scheduler.schedule( receiverHandle );
        } );
    }

    void onReceiverClose()
    {
        closed = true;

        std::unique_lock< std::mutex > guard( mutex );
        if( senderWaiters.empty() )
        {
            return;
        }

        ASSERT( sendQueue.size() == capacity && receiverWaiters.empty(), "" )

        const auto waitersCopy = senderWaiters;
        senderWaiters.clear();
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ val_ptr, handle ] = el;
            this->scheduler.schedule( handle );
        } );
    }

    bool handleSend( std::pair< T*, std::coroutine_handle<> > value )
    {
        std::unique_lock< std::mutex > guard( mutex );
        const auto currentSize = sendQueue.size();

        ASSERT( currentSize <= capacity, "Queue got larger than capacity. bug" );
        if( sendQueue.size() == capacity )
        {
            if( receivers == 0 && awaitableReceivers == 0 )
            {
                return false;
            }

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
            const auto [ result, receiverHandle ] = receiverWaiters.front();
            receiverWaiters.pop_front();
            *result = std::move( *value.first );

            // Prevent double-locks
            guard.unlock();

            scheduler.schedule( receiverHandle );
            return false;
        }

        sendQueue.emplace( std::move( *value.first ) );
        return false;
    }

    bool handleReceive( std::pair< std::optional< T >*, std::coroutine_handle<> > receiver )
    {
        std::unique_lock< std::mutex > guard( mutex );
        if( sendQueue.empty() )
        {
            // No one will send anything already
            if( ( senders == 0 || closed ) && awaitableSenders == 0 )
            {
                *receiver.first = std::nullopt;
                return false;
            }

            // Nothing to receive - park
            receiverWaiters.push_back( receiver );
            return true;
        }

        ASSERT( receiverWaiters.empty(), "If element's in queue receiverWaiters shall be empty" )

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

        *receiver.first = std::move( value );
        return false;
    }

    std::atomic_bool closed = false;

    std::atomic_uint32_t senders = 0;
    std::atomic_uint32_t receivers = 0;
    std::atomic_uint32_t awaitableSenders = 0;
    std::atomic_uint32_t awaitableReceivers = 0;

  private:
    explicit channel( std::size_t theCapacity )
        : capacity( theCapacity )
    {
        ASSERT_FORMAT( theCapacity != 0, "Channel capacity must be greater than 0" );
    }

    channel( const channel& ) = delete;
    channel( channel&& ) = delete;

    mutable std::mutex mutex;

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity );

    // TODO: generic
    DummyScheduler scheduler;

    std::size_t capacity;
    std::queue< T > sendQueue; // TODO: change queue type + circular buffer

    std::list< std::pair< T*, std::coroutine_handle<> > > senderWaiters;
    std::list< std::pair< std::optional< T >*, std::coroutine_handle<> > > receiverWaiters;
};

template< class T >
std::tuple< Sender< T >, Receiver< T > > makeChannel( std::size_t capacity = 1 )
{
    auto chan = new channel< T >( capacity );
    return { Sender{ chan }, Receiver{ chan } };
}