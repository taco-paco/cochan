//
// Created by Edwin Paco on 8/1/24.
//

#include <iostream>

#include <gtest/gtest.h>
#include <coro/coro.hpp>

#include <cochan/cochan.hpp>

template< class T >
void drop( T t )
{
}

class AwaitableLibcoroTest: public ::testing::Test
{
  protected:
    AwaitableLibcoroTest()
        : tp( coro::thread_pool::options{ .thread_count = 4 } )
    {
    }

    coro::thread_pool tp;
};

coro::task< void > triggerSend( coro::thread_pool& tp, std::vector< cochan::AwaitableSend< int > > sendAwaitables )
{
    co_await tp.schedule();
    for( cochan::AwaitableSend< int >& el : sendAwaitables )
    {
        co_await el;
    }

    drop( std::move( sendAwaitables ) );
}

coro::task< void > send( coro::thread_pool& tp, cochan::Sender< int > sender, uint numToSend )
{
    co_await tp.schedule();

    for( int i = 0; i < numToSend; i++ )
    {
        co_await sender.send( i );
    }

    drop( std::move( sender ) );
}

coro::task< void > triggerReceive( coro::thread_pool& tp, std::vector< cochan::AwaitableReceive< int > > receiveAwaitables )
{
    co_await tp.schedule();
    for( cochan::AwaitableReceive< int >& el : receiveAwaitables )
    {
        auto val = co_await el;
        if( !val )
        {
            break;
        }
    }

    drop( std::move( receiveAwaitables ) );
}

coro::task< int > receive( coro::thread_pool& tp, cochan::Receiver< int > receiver )
{
    co_await tp.schedule();

    uint counter = 0;
    while( true )
    {
        auto value = co_await receiver.receive();
        if( !value )
        {
            break;
        }

        counter++;
    }

    drop( std::move( receiver ) );
    co_return counter;
}

TEST_F( AwaitableLibcoroTest, SingleSendAwaitableReceiveReceiver )
{
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

    int received = coro::sync_wait( task() );
    ASSERT_EQ( received, NUM_OF_SENDS );
}

TEST_F( AwaitableLibcoroTest, MultipleSendAwaitableReceiveReceiver )
{
    constexpr uint NUM_OF_SENDS1 = 301;
    constexpr uint NUM_OF_SENDS2 = 450;
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto task = [ & ]() -> coro::task< int > {
        auto [ sender, receiver ] = cochan::makeChannel< int >( 21, scheduleFunc );

        std::vector< cochan::AwaitableSend< int > > sendAwaitables1, sendAwaitables2;
        sendAwaitables1.reserve( NUM_OF_SENDS1 );
        sendAwaitables2.reserve( NUM_OF_SENDS2 );

        for( int i = 0; i < NUM_OF_SENDS1; i++ )
        {
            sendAwaitables1.emplace_back( sender.send( i ) );
        }

        for( int i = 0; i < NUM_OF_SENDS2; i++ )
        {
            sendAwaitables2.emplace_back( sender.send( i ) );
        }

        drop( std::move( sender ) );

        auto sendTask1 = triggerSend( tp, std::move( sendAwaitables1 ) );
        auto sendTask2 = triggerSend( tp, std::move( sendAwaitables2 ) );
        auto receiverTask = receive( tp, std::move( receiver ) );

        auto [ _, _, received ] = co_await coro::when_all( std::move( sendTask1 ), std::move( sendTask2 ), std::move( receiverTask ) );
        co_return received.return_value();
    };

    int received = coro::sync_wait( task() );
    ASSERT_EQ( received, NUM_OF_SENDS1 + NUM_OF_SENDS2 );
}

TEST_F( AwaitableLibcoroTest, SendAwaitableAndSenderMultipleReceivers )
{
    constexpr uint NUM_OF_SENDS1 = 3000;
    constexpr uint NUM_OF_SENDS2 = 1030;
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto task = [ & ]() -> coro::task< int > {
        auto [ sender, receiver ] = cochan::makeChannel< int >( 21, scheduleFunc );

        std::vector< cochan::AwaitableSend< int > > sendAwaitables;
        sendAwaitables.reserve( NUM_OF_SENDS1 );
        for( int i = 0; i < NUM_OF_SENDS1; i++ )
        {
            sendAwaitables.emplace_back( sender.send( i ) );
        }

        auto sendTask1 = triggerSend( tp, std::move( sendAwaitables ) );
        auto sendTask2 = send( tp, std::move( sender ), NUM_OF_SENDS2 );

        auto receiverTask1 = receive( tp, receiver );
        auto receiverTask2 = receive( tp, receiver );
        auto receiverTask3 = receive( tp, receiver );
        drop( std::move( receiver ) );

        auto [ _, _, received1, received2, received3 ] = co_await coro::when_all( std::move( sendTask1 ), std::move( sendTask2 ),
            std::move( receiverTask1 ), std::move( receiverTask2 ), std::move( receiverTask3 ) );

        co_return received1.return_value() + received2.return_value() + received3.return_value();
    };

    int received = coro::sync_wait( task() );
    ASSERT_EQ( received, NUM_OF_SENDS1 + NUM_OF_SENDS2 );
}

TEST_F( AwaitableLibcoroTest, SenderAndReceivebles )
{
    constexpr uint NUM_OF_RECEIVES1 = 1030;
    constexpr uint NUM_OF_RECEIVES2 = 300;
    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto task = [ & ]() -> coro::task< void > {
        auto [ sender, receiver ] = cochan::makeChannel< int >( 21, scheduleFunc );
        auto sendTask = send( tp, std::move( sender ), NUM_OF_RECEIVES1 + NUM_OF_RECEIVES2 );

        std::vector< cochan::AwaitableReceive< int > > receivables1, receivables2;
        receivables1.reserve( NUM_OF_RECEIVES1 );
        receivables2.reserve( NUM_OF_RECEIVES2 );

        for( int i = 0; i < NUM_OF_RECEIVES1; i++ )
        {
            receivables1.emplace_back( receiver.receive() );
        }
        for( int i = 0; i < NUM_OF_RECEIVES2; i++ )
        {
            receivables2.emplace_back( receiver.receive() );
        }
        // Add one for fun
        receivables1.emplace_back( receiver.receive() );
        receivables2.emplace_back( receiver.receive() );

        drop( std::move( receiver ) );

        auto receive1 = triggerReceive( tp, std::move( receivables1 ) );
        auto receive2 = triggerReceive( tp, std::move( receivables2 ) );

        co_await coro::when_all( std::move( sendTask ), std::move( receive1 ), std::move( receive2 ) );
    };

    coro::sync_wait( task() );
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
