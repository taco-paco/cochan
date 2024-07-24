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
        if( !receiverWaiters.empty() )
        {
            ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible. channel::~channel" );
            std::for_each( receiverWaiters.begin(), receiverWaiters.end(), [ this ]( const auto& el ) {
                const auto [ result, receiverHandle ] = el;
                *result = std::nullopt;
                this->scheduler.schedule( receiverWaiters );
            } );

            return;
        }

        // Questionable: value may be sent but not received by anyone.
        std::for_each( senderWaiters.begin(), senderWaiters.end(), [ this ]( const auto& el ) {
            const auto [ senderValue, senderHandle ] = senderWaiters.front();
            scheduler.schedule( senderHandle );
        } );
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

    bool canDelete()
    {
        return senders == 0 && senderPermits == 0 && receivers == 0 && receiverPermits == 0;
    }

    void onSenderClose()
    {
        closed = true;
        if( receiverWaiters.empty() )
        {
            return;
        }

        ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible" );
        std::for_each( receiverWaiters.begin(), receiverWaiters.end(), [ this ]( const auto& el ) {
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

    void close()
    {
        closed = true;

        std::unique_lock< std::mutex > guard( mutex );
        if( !receiverWaiters.empty() )
        {
            ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible" );

            auto it = receiverWaiters.begin();
            std::advance( it, std::min( senderPermits.load(), receiverWaiters.size() ) );

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
        // Move to AwaitableSend::await_suspend?
        senderPermits--;

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
        receiverPermits--;

        std::unique_lock< std::mutex > guard( mutex );
        if( sendQueue.empty() )
        {
            // No one will send anything already
            if( senders == 0 && senderPermits == 0 )
            {
                *receiver.first = std::nullopt;
                return false;
            }

            // Nothing to receive - park
            // Senders or AwaitableSends will wake up everyone eventually
            // By handleSend or destruct logic
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

        *receiver.first = std::move( value );
        return false;
    }

    std::atomic_bool closed = false;

    std::atomic_uint32_t senders = 0;
    std::atomic_uint32_t receivers = 0;
    std::atomic_uint32_t senderPermits = 0;
    std::atomic_uint32_t receiverPermits = 0;

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