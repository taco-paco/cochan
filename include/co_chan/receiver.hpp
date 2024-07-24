#pragma once

#include <iostream>
#include <coroutine>
#include <memory>

#include <co_chan/channel.hpp>

template< class T >
class AwaitableReceive
{
  public:
    explicit AwaitableReceive( channel< T >* theChan )
        : chan( theChan )
    {
        chan->receiverPermits++;
    }

    AwaitableReceive( AwaitableReceive&& other ) noexcept
    {
        // TODO: check
        this->result = std::move( other.result );
        std::swap( this->chan, other.chan );
    }

    AwaitableReceive( const AwaitableReceive& ) = delete;

    ~AwaitableReceive()
    {
        if( !executed )
        {
            chan->receiverPermits--;
        }

        if( !( chan->senderPermits == 0 && chan->receiverPermits == 0 && chan->senders == 0 && chan->receivers == 0 ) )
        {
            return;
        }

        delete chan;
    }

    AwaitableReceive& operator=( const AwaitableReceive& ) = delete;
    AwaitableReceive& operator=( AwaitableReceive&& ) = delete;

    constexpr bool await_ready()
    {
        // Mark as executed
        chan->receiverPermits--;
        executed = true;
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        executed = true;
        return chan->handleReceive( std::make_pair( &result, handle ) );
    }

    std::optional< T > await_resume()
    {
        return result;
    }

  private:
    bool executed = false;

    channel< T >* chan;
    std::optional< T > result;
};

template< class T >
class Receiver
{
  public:
    explicit Receiver( channel< T >* theChan )
        : chan( theChan )
    {
        chan->receivers++;
    }

    Receiver( const Receiver& receiver )
    {
        chan = receiver.chan;
        chan->receivers++;
    }

    Receiver( Receiver&& receiver ) noexcept
    {
        // TODO: check
        std::swap( chan, receiver.chan );
    }

    ~Receiver()
    {
        if( --chan->receivers != 0 )
        {
            return;
        }

        // TODO: logic
    }

    AwaitableReceive< T > receive()
    {
        return AwaitableReceive( chan );
    }

  private:
    channel< T >* chan;
};