//
// Created by Edwin Paco on 7/17/24.
//

#include <iostream>
#include <thread>
#include <chrono>

#include <co_chan/channel.hpp>
#include <co_chan/sender.hpp>
#include <co_chan/receiver.hpp>
#include <co_chan/utils.hpp>

struct promise_type;

struct MyCoroutine: std::coroutine_handle< promise_type >
{
    using promise_type = ::promise_type;

    MyCoroutine( std::coroutine_handle< promise_type > h )
        : handle( h )
    {
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

MyCoroutine sender( Sender< int > s )
{
    std::cout << "sending" << std::endl;

    int val1 = 1;
    co_await s.send( val1 );
    std::cout << "sent 1" << std::endl;

    co_await s.send( 2 );
    std::cout << "sent 2" << std::endl;

    co_await s.send( 3 );
    std::cout << "sent 3" << std::endl;

    co_await s.send( 4 );
    std::cout << "sent 4" << std::endl;

    co_await s.send( 5 );
    std::cout << "sent 5" << std::endl;
}

void send( Sender< int > s )
{
    const auto handle = sender( s );
    std::cout << "send sleeping" << std::endl;
    std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
}

MyCoroutine receiver( Receiver< int > r )
{
    int result;
    std::cout << "receiving" << std::endl;

    result = co_await r.receive();
    std::cout << "received(1): " << result << std::endl;

    result = co_await r.receive();
    std::cout << "received(2): " << result << std::endl;

    result = co_await r.receive();
    std::cout << "received(3): " << result << std::endl;

    result = co_await r.receive();
    std::cout << "received(4): " << result << std::endl;

    result = co_await r.receive();
    std::cout << "received(5): " << result << std::endl;
}

template< class T >
void receive( Receiver< T > r )
{
    std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
    MyCoroutine coro = receiver( r );
}

int main()
{
    auto [ s, r ] = makeChannel< int >( 3 );
    std::thread asd( send, s );

    receive( r );

    asd.join();
}
