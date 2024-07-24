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
    const char* what() const override
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
        guard.unlock(); // Prevent double-lock

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ result, receiverHandle ] = el;
            *result = std::nullopt;
            this->scheduler.schedule( receiverWaiters );
        } );

        // TODO receivePermits shall get res as well
        // If sendq.empty() && senders == 0  && sendersPermit == 0
        // return std::nullopt
        // If sendq.empty() && senders == 0  && sendersPermit != 0
        // park

        // But will it be awakened?
        // So parked ones will be awakened by sendPermitted(awaitables) directly
        // reamnant will be awakened by onSenderClose.
        // If after that receiverPermit triggered it will fall under 1st branch above
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
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ val_ptr, handle ] = el;
            this->scheduler.schedule( handle );
        } );
    }

    void close()
    {
        closed = true;

        std::unique_lock< std::mutex > guard( mutex );
        // TODO: remove?
        if( !receiverWaiters.empty() )
        {
            ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible" );

            auto it = receiverWaiters.begin();
            std::advance( it, std::min( awaitableSenders.load(), receiverWaiters.size() ) );

            std::list< std::pair< T*, std::coroutine_handle<> > > toSchedule;
            toSchedule.splice( toSchedule.end(), receiverWaiters, it, receiverWaiters.end() );

            // Prevent double-lock
            guard.unlock();

            // In case senderPermits < receiverWaiters. Wake up the difference with std::nullopt.
            // otherwise they never will be called since no new senders after close.
            std::for_each( toSchedule.begin(), toSchedule.end(), [ this ]( const auto& el ) {
                const auto [ result, receiverHandle ] = el;
                *result = std::nullopt;
                this->scheduler.schedule( receiverWaiters );
            } );

            // Remaining receiverWaiters have to be woken up by 'senderPermits'.
            // What happens if senderPermits > receiverWaiters. We can't wake any receiver.
            // Have to wait until co_await permit called.

            return;
        }

        if( !senderWaiters.empty() )
        {
            std::for_each( senderWaiters.begin(), senderWaiters.end(), [ this ]( const auto& el ) {
                const auto [ val_ptr, handle ] = el;
                this->scheduler.schedule( handle );
            } );
        }
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
        // TODO: Move to AwaitableReceive::await_suspend?
        // TODO: safe here? Last sender destroyed right after. will call delete on chan
        // If move to destructor same as receiverWaiters.size()
        // receiverPermits--;

        std::unique_lock< std::mutex > guard( mutex );
        if( sendQueue.empty() )
        {
            // No one will send anything already
            // -----
            // This is not working if --senderPermits in destructor
            // Cause all of them can be suspended. so we will park

            // if( senders == 0 && ( sendAwaiters - senderWaiters.size() ) == 0 )
            // senderWaiters shall be empty. Optimization found?
            if( ( senders == 0 || closed ) && awaitableSenders == 0 )
            {
                *receiver.first = std::nullopt;
                return false;
            }

            // So here the idea is:
            // If we have senders and not triggered permits.
            // Then we can park because they will wake us up.
            // This logic above won't work if

            // Nothing to receive - park
            // Senders or AwaitableSends will wake up everyone eventually
            // By handleSend or destruct logic
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

        // TODO: if last receiver call. Wake up all senders
        // replace with: if (receivers == 0 && (awaiters - receiverWaiter.size())) == 0)
        // but if sendQueue.size() != 0, receiverWaiter.size() == 0, so
        // replace with (receivers == 0 && awaiters == 1) (the last one)
        if( receivers == 0 && awaitableReceivers == 0 )
        {
            //
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

    // TODO: allow?
    channel( const channel& ) = delete;
    channel( channel&& ) = delete;

    mutable std::mutex mutex;

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity );

    // TODO: generic
    DummyScheduler scheduler;

    std::size_t capacity;
    std::queue< T > sendQueue; // TODO: change queue type + circular buffer

    // TODO: sender + receiver(?) permits

    std::list< std::pair< T*, std::coroutine_handle<> > > senderWaiters;
    std::list< std::pair< std::optional< T >*, std::coroutine_handle<> > > receiverWaiters;
};

template< class T >
std::tuple< Sender< T >, Receiver< T > > makeChannel( std::size_t capacity = 1 )
{
    auto chan = new channel< T >( capacity );
    return { Sender{ chan }, Receiver{ chan } };
}

/// closing
// senders could dropped but handle could still be in different threads for example.
// They could be co_await. At this point we throw exception. Otherwise there's
// a difference between Receiver::close and close when senders dropped.

// In case all senders are dropped we set closed. There could be pending senders.
// But they are ok. What will throw exception is somehow calling AwaitableSend::await_suspend.
// But I ain't sure how is this possible if it would mean access to sender

// Not awakened Sender or Receivers
// In case

// Need to decide when send is prohibited.
// Right after close or when last awaitable called.

// Channel can be considered closed when all senders and awaitables destroyed.

/// Sender closing design choice
// Close when both all awaitables and senders dropped.
// Awaitables suspend. TODO: Close before last suspended or after(a.k.a dropped?)

/// Receiver close scheme
// As above and also case for explicit Receiver::close
// 1. trivial
// 2.

// Meaning of close?
// Different for s & r?