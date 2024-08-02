# cochan

This library provides C++20 implementation of channels using coroutines.
Design inspired by Go & Rust

## Smart channels

The channel will destruct itself once there are no _**receivables**_ and _**sendables**_. This means that the channel's
lifetime
is automatically managed, and it will clean up its resources when it is no longer in use.

### Example

Here's an example using libcoro.

```c++
// Use your custom scheduler or cochan::defaultScheduleFunc
auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
    auto scheduleAwaitable = tp.schedule();
    scheduleAwaitable.await_suspend( handle );
};

constexpr uint NUM_OF_SENDS = 257;
auto send = [ & ]( Sender< message > sender ) -> coro::task< void > {
    for( uint32_t i = 0; i < NUM_OF_SENDS; i++ )
    {
        // Pass std::movable object
        auto msg = message{ i, "hello there" };
        co_await sender.send( std::move( msg ) );
    }

    // Notify parked receivers
    drop( std::move( sender ) );
};

auto receive = [ & ]( Receiver< message > receiver ) -> coro::task< uint > {
    uint counter = 0;
    while( true )
    {
        std::optional< message > val = co_await receiver.receive();
        if( !val )
        {
            break;
        }

        counter++;
    }

    drop( std::move( receiver ) );
    co_return counter;
};

auto task = [ & ]() -> coro::task< uint > {
    auto [ sender, receiver ] = makeChannel< message >( 2, scheduleFunc );

    auto sendTask = send( std::move( sender ) );
    auto receiveTask = receive( std::move( receiver ) );
    auto [ _, counter ] = co_await coro::when_all( std::move( sendTask ), std::move( receiveTask ) );

    co_return counter.return_value();
};

uint counter = coro::sync_wait( task() );
```

You also don't need to track awaiter object's lifetime and can just send them to different coroutine.

_Full code at awaitables_test.cpp_

```c++
coro::task< void > triggerSend( coro::thread_pool& tp, std::vector< cochan::AwaitableSend< int > > sendAwaitables )
{
    co_await tp.schedule();
    for( cochan::AwaitableSend< int >& el : sendAwaitables )
    {
        co_await el;
    }

    drop( std::move( sendAwaitables ) );
}

constexpr uint NUM_OF_SENDS = 401;
auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
    auto scheduleAwaitable = tp.schedule();
    scheduleAwaitable.await_suspend( handle );
};

auto task = [ & ]() -> coro::task< int > {
    auto [ sender, receiver ] = cochan::makeChannel< int >( 21, scheduleFunc );

    std::vector< cochan::AwaitableSend< int > > sendAwaitables;
    sendAwaitables.reserve( NUM_OF_SENDS );

    for( int i = 0; i < NUM_OF_SENDS; i++ )
    {
        sendAwaitables.emplace_back( sender.send( i ) );
    }

    drop( std::move( sender ) );
    auto sendTask = triggerSend( tp, std::move( sendAwaitables ) );
    auto receiverTask = receive( tp, std::move( receiver ) );

    auto [ _, received ] = co_await coro::when_all( std::move( sendTask ), std::move( receiverTask ) );
    co_return received.return_value();
};
```

### Send & Receive

Sending and receiving messages from the channel is done via corresponding classes _**Sender**_ and _**Receiver**_. Here
the
library takes Rust approach
in separating them from single entity channel. _Senders_ & _Receivers_ can be copied and sent to different coroutines.

### Closing channel

Channel can be explicitly closed via `Receiver::close` call. It is also closed implicitly
once whether all ***sendable***s or ***receivable***s are destructed. <br />
***sendable***: _**Sender**_ or _**AwaitableSend**_ <br />
***receivable***: _**Receiver**_ or _**AwaitableReceive**_

### Permits

_**AwaitableSend**_ works as permit for sending. If _**AwaitableSend**_ was given by the _**Sender**_ before channel was
closed,
you can send your message even after the channel is closed by Receiver side. <br />
If Receiver side is closed due to destruction of all ***receivable***s the message will be sent into _**void**_.
If `Receiver::closed` explicitly, remaining permitted senders can still send and be consumed until they don't run.

### Lifetime of channel

The channel is destructed by last entity from sendables and receivables.