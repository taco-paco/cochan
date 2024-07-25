//
// Created by Edwin Paco on 7/24/24.
//
#include <iostream>
#include <thread>

#include <co_chan/channel.hpp>
#include <co_chan/receiver.hpp>
#include <co_chan/sender.hpp>

struct promise_type;

struct MyCoroutine: std::coroutine_handle< promise_type >
{
    using promise_type = ::promise_type;

    MyCoroutine( std::coroutine_handle< promise_type > h )
        : handle( h )
    {
    }

    ~MyCoroutine()
    {
        if( handle && !handle.done() )
        {
            int a = 1;
        }
    }

    std::coroutine_handle< promise_type > handle;
};

struct promise_type
{
    MyCoroutine get_return_object()
    {
        return MyCoroutine{ std::coroutine_handle< promise_type >::from_promise( *this ) };
    }

    std::suspend_never initial_suspend()
    {
        return {};
    }

    std::suspend_never final_suspend() noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
        std::terminate();
    }

    void return_void()
    {
    }
};

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
