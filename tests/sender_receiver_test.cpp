//
// Created by Edwin Paco on 7/17/24.
//

#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "dummy_coro.hpp"
#include <cochan/cochan.hpp>

using namespace cochan;

const uint32_t NUM_SEND_ITEMS = 5;

MyCoroutine send( Sender< int > s )
{
    for( uint i = 0; i < NUM_SEND_ITEMS; i++ )
    {
        co_await s.send( i );
    }

    co_return;
}

MyCoroutine receive( Receiver< int > r, uint& receiveCounter )
{
    while( true )
    {
        auto val = co_await r.receive();
        if( !val )
        {
            break;
        }

        receiveCounter++;
    }

    co_return;
}

template< class T >
void drop( T t )
{
}

class SenderReceiverLibcoroTest: public ::testing::Test
{
  protected:
    void SetUp() override
    {
        receiveCounter = 0;
    }

    uint receiveCounter = 0;
};

TEST_F( SenderReceiverLibcoroTest, SingleThreadSendReceive )
{
    auto [ s, r ] = makeChannel< int >( 3 );

    auto sendCoro = send( std::move( s ) );
    auto receiveCoro = receive( std::move( r ), receiveCounter );

    ASSERT_TRUE( sendCoro.handle.done() ) << "Coroutines should complete each other within 1 thread.";
    drop( std::move( sendCoro ) );

    ASSERT_TRUE( receiveCoro.handle.done() ) << "Coroutines should complete each other within 1 thread.";
    ASSERT_EQ( receiveCounter, NUM_SEND_ITEMS );
}

TEST_F( SenderReceiverLibcoroTest, SingleThreadReceiveSend )
{
    auto [ s, r ] = makeChannel< int >( 3 );

    auto receiveCoro = receive( std::move( r ), receiveCounter );
    auto sendCoro = send( std::move( s ) );

    ASSERT_TRUE( sendCoro.handle.done() ) << "Coroutines should complete each other within 1 thread.";
    drop( std::move( sendCoro ) );

    ASSERT_TRUE( receiveCoro.handle.done() ) << "Coroutines should complete each other within 1 thread.";
    ASSERT_EQ( receiveCounter, NUM_SEND_ITEMS );
}

void syncReceive( Receiver< int > r, uint& receiveCounter )
{
    MyCoroutine coro = receive( std::move( r ), receiveCounter );
    while( !coro.handle.done() )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

void syncSend( Sender< int > s )
{
    MyCoroutine coro = send( std::move( s ) );
    while( !coro.handle.done() )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

TEST_F( SenderReceiverLibcoroTest, MultiThreadSendReceive )
{
    const ScheduleFunc dumbSchedule = []( std::coroutine_handle<> handle ) {
        std::thread t( [ handle ]() {
            handle.resume();
        } );

        t.detach();
    };

    auto [ s, r ] = makeChannel< int >( 3, dumbSchedule );

    std::thread st( syncSend, std::move( s ) );
    std::thread rt( syncReceive, std::move( r ), std::ref( receiveCounter ) );

    st.join();
    rt.join();

    ASSERT_EQ( receiveCounter, NUM_SEND_ITEMS );
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
