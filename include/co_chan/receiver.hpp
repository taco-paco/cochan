#pragma once

#include <iostream>
#include <coroutine>
#include <memory>

#include <co_chan/channel.hpp>

template< class T >
class Receiver;

template< class T >
class AwaitableReceive
{
  public:
    AwaitableReceive() = delete;
    AwaitableReceive( AwaitableReceive&& other ) noexcept
        : result( std::move( other.value ) )
        , chan( other.chan )
    {
        other.chan = nullptr;
    }

    AwaitableReceive( const AwaitableReceive& ) = delete;

    ~AwaitableReceive()
    {
        if( !chan )
        {
            return;
        }

        if( --chan->awaitableReceivers == 0 && chan->receivers == 0 )
        {
            if( chan->senders == 0 && chan->awaitableSenders == 0 )
            {
                delete chan;
                return;
            }

            chan->onReceiverClose();
            return;
        }
    }

    AwaitableReceive& operator=( const AwaitableReceive& ) = delete;
    AwaitableReceive& operator=( AwaitableReceive&& ) = delete;

    constexpr bool await_ready()
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->handleReceive( std::make_pair( &result, handle ) );
    }

    std::optional< T > await_resume()
    {
        return result;
    }

  private:
    explicit AwaitableReceive( channel< T >* theChan )
        : chan( theChan )
    {
        chan->awaitableReceivers++;
    }

    friend Receiver< T >;

    channel< T >* chan;
    std::optional< T > result;
};

template< class T >
class Receiver
{
  public:
    Receiver() = delete;

    Receiver( const Receiver& receiver )
    {
        chan = receiver.chan;
        chan->receivers++;
    }

    Receiver( Receiver&& other ) noexcept
        : chan( other.chan )
    {
        other.chan = nullptr;
    }

    ~Receiver()
    {
        if( !chan )
        {
            return;
        }

        if( --chan->receivers == 0 && chan->awaitableReceivers == 0 )
        {
            if( chan->senders == 0 && chan->awaitableSenders == 0 )
            {
                delete chan;
                return;
            }

            chan->onReceiverClose();
            return;
        }
    }

    void close()
    {
        chan->closed = true;
    }

    AwaitableReceive< T > receive()
    {
        return AwaitableReceive( chan );
    }

  private:
    explicit Receiver( channel< T >* theChan )
        : chan( theChan )
    {
        chan->receivers++;
    }

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity, const ScheduleFunc& );

    channel< T >* chan;
};