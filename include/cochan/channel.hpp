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

#include <cochan/utils.hpp>

namespace cochan
{
class ChannelClosedException: public std::exception
{
  public:
    const char* what() const noexcept override
    {
        return "Channel closed";
    }
};

using ScheduleFunc = std::function< void( std::coroutine_handle<> ) >;
const ScheduleFunc defaultScheduleFunc = []( std::coroutine_handle<> handle ) {
    handle.resume();
};

template< typename T >
class Sender;

template< typename T >
class Receiver;

// TODO: case for copy_constructible only
template< std::movable T >
class Channel
{
  public:
    ~Channel()
    {
        COCHAN_ASSERT( receiverWaiters.empty(), "Should be handled by last sendable object" );
        COCHAN_ASSERT( senderWaiters.empty(), "Should be handled ny last receivable object" );
    }

    std::size_t getSize() const
    {
        const std::lock_guard< std::mutex > guard( mutex );
        return sendQueue.size();
    }

    std::size_t getCapacity() const
    {
        return capacity;
    }

    bool isClosed() const
    {
        return closed;
    }

    std::list< std::pair< std::optional< T >*, std::coroutine_handle<> > > collectReceiveWaiters()
    {
        if( receiverWaiters.empty() )
        {
            return {};
        }

        COCHAN_ASSERT( sendQueue.empty() && senderWaiters.empty(), "Bug or wrong assumption of that being impossible" );
        const auto waitersCopy = receiverWaiters;
        receiverWaiters.clear();

        return waitersCopy;
    }

    std::list< std::pair< T*, std::coroutine_handle<> > > collectSendWaiters()
    {
        if( senderWaiters.empty() )
        {
            return {};
        }

        COCHAN_ASSERT( sendQueue.size() == capacity && receiverWaiters.empty(), "" )
        const auto waitersCopy = senderWaiters;
        senderWaiters.clear();

        return waitersCopy;
    }

    bool handleSend( std::pair< T*, std::coroutine_handle<> > value )
    {
        std::unique_lock< std::mutex > guard( mutex );
        const auto currentSize = sendQueue.size();

        COCHAN_ASSERT( currentSize <= capacity, "Queue got larger than capacity. bug" );
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
            COCHAN_ASSERT( receiverWaiters.empty(), "Bug or wrong assumption of that being impossible" );
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

            scheduleFunc( receiverHandle );
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

        COCHAN_ASSERT( receiverWaiters.empty(), "If element's in queue receiverWaiters shall be empty" )

        T value = std::move( sendQueue.front() );
        sendQueue.pop();

        // Was full and have sender waiters
        if( sendQueue.size() + 1 == capacity && !senderWaiters.empty() )
        {
            const auto [ senderValue, senderHandle ] = senderWaiters.front();
            senderWaiters.pop_front();
            sendQueue.emplace( std::move( *senderValue ) );

            // Prevent double-locks
            guard.unlock();

            scheduleFunc( senderHandle );
        }

        *receiver.first = std::move( value );
        return false;
    }

  private:
    explicit Channel( std::size_t theCapacity, const ScheduleFunc& theScheduleFunc )
        : capacity( theCapacity )
        , scheduleFunc( theScheduleFunc )
    {
        COCHAN_ASSERT_FORMAT( theCapacity != 0, "Channel capacity must be greater than 0" );
    }

    Channel( const Channel& ) = delete;
    Channel( Channel&& ) = delete;

    mutable std::mutex mutex;

    template< typename U >
    friend class Sender;

    template< class U >
    friend class AwaitableSend;

    template< typename U >
    friend class Receiver;

    template< class U >
    friend class AwaitableReceive;

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity, const ScheduleFunc& );

    ScheduleFunc scheduleFunc;

    std::size_t capacity;
    std::queue< T > sendQueue; // TODO: change queue type + circular buffer
    std::atomic_bool closed = false;

    std::atomic_uint32_t senders = 0;
    std::atomic_uint32_t receivers = 0;
    std::atomic_uint32_t awaitableSenders = 0;
    std::atomic_uint32_t awaitableReceivers = 0;

    // TODO: rename parkedSender
    std::list< std::pair< T*, std::coroutine_handle<> > > senderWaiters;
    std::list< std::pair< std::optional< T >*, std::coroutine_handle<> > > receiverWaiters;
};

template< class T >
std::tuple< Sender< T >, Receiver< T > > makeChannel( std::size_t capacity = 1, const ScheduleFunc& schedule = defaultScheduleFunc )
{
    auto chan = new Channel< T >( capacity, defaultScheduleFunc );
    return { Sender{ chan }, Receiver{ chan } };
}

} // namespace cochan