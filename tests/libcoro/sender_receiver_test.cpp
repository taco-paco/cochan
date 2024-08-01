#include <iostream>
#include <vector>
#include <coroutine>
#include <atomic>
#include <coro/coro.hpp>

#include <gtest/gtest.h>

#include <cochan/channel.hpp>
#include <cochan/sender.hpp>
#include <cochan/receiver.hpp>

using namespace cochan;

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

class ChannelTest: public ::testing::Test
{
  protected:
    coro::thread_pool tp;

    ChannelTest()
        : tp( coro::thread_pool::options{
              .thread_count = 4,
          } )
    {
    }

    void SetUp() override
    {
        receiveCounter = 0;
    }
};

TEST_F( ChannelTest, OneToOne )
{
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto oneToOne = [ this, &scheduleFunc ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 3, scheduleFunc );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 1, 1 ) );
    };

    coro::sync_wait( oneToOne() );
    ASSERT_EQ( receiveCounter, NUM_SEND_ITEMS ) << "Wrong counter, expected 1000";
}

TEST_F( ChannelTest, TwoToOne )
{
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto twoToOne = [ this, &scheduleFunc ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 3, scheduleFunc );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 2, 1 ) );
    };

    coro::sync_wait( twoToOne() );
    ASSERT_EQ( receiveCounter, 2 * NUM_SEND_ITEMS ) << "Wrong counter, expected 2000";
}

TEST_F( ChannelTest, TwoToTwo )
{
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto twoToTwo = [ this, &scheduleFunc ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 10 );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 2, 1 ) );
    };

    coro::sync_wait( twoToTwo() );
    ASSERT_EQ( receiveCounter, 2 * NUM_SEND_ITEMS ) << "Wrong counter, expected 2000";
}

TEST_F( ChannelTest, ManyToMany )
{
    int senders = 20;
    int receivers = 31;

    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto manyToMany = [ this, &scheduleFunc, senders, receivers ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 10 );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), senders, receivers ) );
    };

    coro::sync_wait( manyToMany() );
    ASSERT_EQ( receiveCounter, senders * NUM_SEND_ITEMS ) << "Wrong counter, expected 20000";
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
