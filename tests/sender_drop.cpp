//
// Created by Edwin Paco on 7/24/24.
//
#include <iostream>
#include <vector>
#include <thread>

#include "dummy_coro.hpp"

#include <cochan/channel.hpp>
#include <cochan/receiver.hpp>
#include <cochan/sender.hpp>

using namespace cochan;

constexpr uint32_t NUM_SEND_ITEMS = 10;

MyCoroutine send( std::vector< AwaitableSend< int > > senderPermits )
{
    for( auto& permit : senderPermits )
    {
        co_await permit;
        std::cout << "sent" << std::endl;
    }
}

MyCoroutine recv( Receiver< int > receiver )
{
    while( true )
    {
        auto res = co_await receiver.receive();
        if( !res )
        {
            std::cout << "none" << std::endl;
            break;
        }

        std::cout << "res: " << *res << std::endl;
    }
}

template< class T >
void receive( Receiver< T > receiver )
{
    recv( std::move( receiver ) );
}

void drop( Sender< int > s )
{
}

// tests:
// opposite to this
// generate buch of sAwaitable & rAwaitables and drop sender and receiver
// Multithreaded send
// Multithreaded receive
// Multithreaded both

std::vector< AwaitableSend< int > > generatePermits()
{
}

int main()
{
    auto [ sender, receiver ] = makeChannel< int >( 3 );

    std::vector< AwaitableSend< int > > a;
    for( int i = 0; i < 5; i++ )
    {
        a.push_back( sender.send( i ) );
    }

    std::thread t( receive< int >, receiver );

    send( std::move( a ) );
    drop( std::move( sender ) );
    t.join();
}