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

class SenderReceiverLibcoroTest: public ::testing::Test
{
  protected:
    coro::thread_pool tp;

    SenderReceiverLibcoroTest()
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

TEST_F( SenderReceiverLibcoroTest, OneToOne )
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

TEST_F( SenderReceiverLibcoroTest, TwoToOne )
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

TEST_F( SenderReceiverLibcoroTest, TwoToTwo )
{
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto twoToTwo = [ this, &scheduleFunc ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 10, scheduleFunc );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), 2, 1 ) );
    };

    coro::sync_wait( twoToTwo() );
    ASSERT_EQ( receiveCounter, 2 * NUM_SEND_ITEMS ) << "Wrong counter, expected 2000";
}

TEST_F( SenderReceiverLibcoroTest, ManyToMany )
{
    int senders = 20;
    int receivers = 31;

    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto manyToMany = [ this, &scheduleFunc, senders, receivers ]() -> coro::task< void > {
        auto [ sender, receiver ] = makeChannel< int >( 10, scheduleFunc );
        co_await coro::when_all( createTasks( tp, std::move( sender ), std::move( receiver ), senders, receivers ) );
    };

    coro::sync_wait( manyToMany() );
    ASSERT_EQ( receiveCounter, senders * NUM_SEND_ITEMS ) << "Wrong counter, expected 20000";
}

TEST_F( SenderReceiverLibcoroTest, ComplexType )
{
    struct message
    {
        message( uint32_t i, std::string t )
            : id( i )
            , text( std::move( t ) )
        {
        }
        message( const message& ) = delete;
        message( message&& other )
            : id( other.id )
            , text( std::move( other.text ) )
        {
        }
        auto operator=( const message& ) -> message& = delete;
        auto operator=( message&& other ) -> message&
        {
            if( std::addressof( other ) != this )
            {
                this->id = std::exchange( other.id, 0 );
                this->text = std::move( other.text );
            }

            return *this;
        }

        ~message()
        {
            id = 0;
        }

        uint32_t id;
        std::string text;
    };

    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    constexpr uint NUM_OF_SENDS = 257;
    auto send = [ & ]( Sender< message > sender ) -> coro::task< void > {
        for( uint32_t i = 0; i < NUM_OF_SENDS; i++ )
        {
            auto msg = message{ i, "hello there" };
            co_await sender.send( std::move( msg ) );
        }

        drop( std::move( sender ) );
    };

    auto receive = [ & ]( Receiver< message > receiver ) -> coro::task< uint > {
        uint counter = 0;
        while( true )
        {
            auto val = co_await receiver.receive();
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
    ASSERT_EQ( counter, NUM_OF_SENDS );
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
