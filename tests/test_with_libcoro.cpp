#include <iostream>
#include <vector>
#include <coroutine>
#include <atomic>

#include <coro/coro.hpp>

#include <co_chan/channel.hpp>
#include <co_chan/sender.hpp>
#include <co_chan/receiver.hpp>

constexpr uint32_t NUM_SEND_ITEMS = 1000;
std::atomic_uint32_t receiveCounter = 0;

template< class T >
void drop( T t )
{
}

coro::task< void > send( coro::thread_pool& tp, Sender< int > sender )
{
    co_await tp.schedule();

    for( uint i = 0; i < NUM_SEND_ITEMS; i++ )
    {
        co_await sender.send( i );
    }

    drop( std::move( sender ) );
}

coro::task< void > receive( coro::thread_pool& tp, Receiver< int > receiver )
{
    co_await tp.schedule();

    while( true )
    {
        auto res = co_await receiver.receive();
        if( !res )
        {
            break;
        }

        receiveCounter++;
    }

    drop( std::move( receiver ) );
}

std::vector< coro::task< void > > createTasks(
    coro::thread_pool& tp, Sender< int > s, Receiver< int > r, uint sendTasks, uint receiveTasks )
{
    std::vector< coro::task< void > > tasks;
    tasks.reserve( sendTasks + receiveTasks );

    for( uint i = 0; i < sendTasks; i++ )
    {
        tasks.emplace_back( send( tp, s ) );
    }

    for( uint i = 0; i < receiveTasks; i++ )
    {
        tasks.emplace_back( receive( tp, r ) );
    }

    drop( std::move( s ) );
    drop( std::move( r ) );

    return std::move( tasks );
}

int main()
{
    coro::thread_pool tp{ coro::thread_pool::options{ .thread_count = 4,
        .on_thread_start_functor = []( std::size_t worker_idx ) -> void {
            std::cout << "thread pool worker " << worker_idx << " is starting up.\n";
        },
        .on_thread_stop_functor = []( std::size_t worker_idx ) -> void {
            std::cout << "thread pool worker " << worker_idx << " is shutting down.\n";
        } } };

    auto scheduleFunc = [ &tp ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    {
        auto oneToOne = [ & ]() -> coro::task< void > {
            auto [ sender, receiver ] = makeChannel< int >( 3, scheduleFunc );
            co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 1, 1 ) );

            co_return;
        };

        coro::sync_wait( oneToOne() );
        ASSERT( receiveCounter == NUM_SEND_ITEMS, "Wrong counter, expected 1000" );
        receiveCounter = 0;
    }

    {
        auto twoToOne = [ & ]() -> coro::task< void > {
            auto [ sender, receiver ] = makeChannel< int >( 3, scheduleFunc );
            co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 2, 1 ) );

            co_return;
        };

        coro::sync_wait( twoToOne() );
        ASSERT( receiveCounter == 2 * NUM_SEND_ITEMS, "Wrong counter, expected 2000" );
        receiveCounter = 0;
    }

    {
        auto twoToTwo = [ & ]() -> coro::task< void > {
            auto [ sender, receiver ] = makeChannel< int >( 10 );
            co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 2, 1 ) );
        };

        coro::sync_wait( twoToTwo() );
        ASSERT( receiveCounter == 2 * NUM_SEND_ITEMS, "Wrong counter, expected 2000" );
        receiveCounter = 0;
    }

    {
        int senders = 20;
        int receivers = 31;

        auto manyToMany = [ & ]() -> coro::task< void > {
            auto [ sender, receiver ] = makeChannel< int >( 10 );
            co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), senders, receivers ) );
        };

        coro::sync_wait( manyToMany() );
        ASSERT( receiveCounter == senders * NUM_SEND_ITEMS, "Wrong counter, expected 2000" );
        receiveCounter = 0;
    }
}
