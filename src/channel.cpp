//
// Created by Edwin Paco on 7/12/24.
//
#include <iostream>

#include <co_chan/channel.hpp>
#include <co_chan/utils.hpp>

channel::channel()
    : capacity( 1 )
{
}

channel::channel( std::size_t theCapacity )
    : capacity( theCapacity )
{
}

bool channel::push( std::pair< int, std::coroutine_handle<> > sender )
{
    std::unique_lock< std::mutex > guard( mutex );
    const auto currentSize = sendQueue.size();

    ASSERT( currentSize <= capacity, "Queue got larger than capacity. bug" );
    if( sendQueue.size() == capacity )
    {
        senderWaiters.push_back( sender );
        return true;
    }

    // If not empty there are no receivers so just push
    if( !sendQueue.empty() )
    {
        ASSERT( receiverWaiters.empty(), "Bug or wrong assumption of that being impossible" );
        sendQueue.push( sender.first );
        return false;
    }

    // If there's receiver just propagate value in its slot
    if( !receiverWaiters.empty() )
    {
        const auto [ slot, receiverHandle ] = receiverWaiters.front();
        receiverWaiters.pop_front();
        *slot = sender.first;

        // Prevent double-locks
        guard.unlock();

        scheduler.schedule( receiverHandle );
        return false;
    }

    sendQueue.push( sender.first );
    return false;
}

bool channel::pop( std::pair< int*, std::coroutine_handle<> > receiver )
{
    std::unique_lock< std::mutex > guard( mutex );

    // Nothing to receive - park
    if( sendQueue.empty() )
    {
        receiverWaiters.push_back( receiver );
        return true;
    }

    const int value = sendQueue.front();
    sendQueue.pop();

    // Was full and have sender waiters
    if( sendQueue.size() + 1 == capacity && !senderWaiters.empty() )
    {
        const auto [ senderValue, senderHandle ] = senderWaiters.front();
        senderWaiters.pop_front();
        sendQueue.push( senderValue );

        // Prevent double-locks
        guard.unlock();

        scheduler.schedule( senderHandle );
    }

    *receiver.first = std::move( value );
    return false;
}
