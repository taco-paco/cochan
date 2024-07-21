#pragma once

#include <iostream>
#include <coroutine>

#include <co_chan/channel.hpp>

template< class T >
class AwaitableSend
{
  public:
    AwaitableSend( const T& theValue, channel< T >* theChan )
        : value( theValue )
        , chan( theChan )
    {
    }

    AwaitableSend( T&& theValue, channel< T >* theChan )
        : value( std::move( theValue ) )
        , chan( theChan )
    {
    }

    bool await_ready() const
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->handleSend( std::make_pair( &value, handle ) );
    }

    void await_resume()
    {
        // here value can be d
        //        std::cout << "Sender::await_resume" << std::endl;
        // TODO: handle closed chan
    }

  private:
    T value;
    channel< T >* chan;
};

template< class T >
class Sender
{
  public:
    Sender( channel< T >* theChan )
        : chan( theChan )
    {
    }

    Sender( const Sender& sender )
    {
        this->chan = sender.chan;
    }

    AwaitableSend< T > send( const T& value )
    {
        return AwaitableSend{ value, chan };
    }

    AwaitableSend< T > send( T&& value )
    {
        return AwaitableSend{ value, chan };
    }

  private:
    channel< T >* chan;
};