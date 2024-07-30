//
// Created by Edwin Paco on 7/17/24.
//

#include <iostream>
#include <thread>
#include <chrono>

#include "dummy_coro.hpp"

#include <co_chan/channel.hpp>
#include <co_chan/sender.hpp>
#include <co_chan/receiver.hpp>

const uint32_t NUM_SEND_ITEMS = 5;

MyCoroutine send( Sender< int > s )
{
    std::cout << "sending" << std::endl;

    for( uint i = 0; i < NUM_SEND_ITEMS; i++ )
    {
        co_await s.send( i );
        std::cout << "sent " << i << std::endl;
    }

    co_return;
}

void syncSend( Sender< int > s )
{
    MyCoroutine coro = send( std::move( s ) );
    while( !coro.handle.done() )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

MyCoroutine receive( Receiver< int > r )
{
    uint32_t received = 0;
    while( true )
    {
        auto val = co_await r.receive();
        if( !val )
        {
            break;
        }

        std::cout << "received " << *val << std::endl;
        received++;
    }

    ASSERT( received == NUM_SEND_ITEMS, "Received wrong amount" );

    co_return;
}

void syncReceive( Receiver< int > r )
{
    MyCoroutine coro = receive( std::move( r ) );
    while( !coro.handle.done() )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
}

const ScheduleFunc dumbSchedule = []( std::coroutine_handle<> handle ) {
    std::thread t( [ handle ]() {
        handle.resume();
    } );

    t.detach();
};

template< class T >
void drop( T t )
{
}

int main()
{
    {
        auto [ s, r ] = makeChannel< int >( 3 );

        auto sendCoro = send( std::move( s ) );
        auto receiveCoro = receive( std::move( r ) );

        ASSERT( sendCoro.handle.done(), "Coroutines should complete each other within 1 thread." )
        drop( std::move( sendCoro ) );

        ASSERT( receiveCoro.handle.done(), "Coroutines should complete each other within 1 thread." )
    }

    {
        auto [ s, r ] = makeChannel< int >( 3 );

        auto receiveCoro = receive( std::move( r ) );
        auto sendCoro = send( std::move( s ) );
        ASSERT( sendCoro.handle.done(), "Coroutines should complete each other within 1 thread." )
        drop( std::move( sendCoro ) );

        ASSERT( receiveCoro.handle.done(), "Coroutines should complete each other within 1 thread." )
    }

    {
        auto [ s, r ] = makeChannel< int >( 3, dumbSchedule );

        std::thread st( syncSend, std::move( s ) );
        std::thread rt( syncReceive, std::move( r ) );

        st.join();
        rt.join();
    }
}
