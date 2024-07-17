#pragma once

#include <iostream>
#include <coroutine>

#include <co_chan/channel.hpp>

class AwaitableSend
{
  public:
    AwaitableSend( int theValue, channel* theChan )
        : value( theValue )
        , chan( theChan )
    {
    }

    bool await_ready() const
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->push( std::make_pair( value, handle ) );
    }

    void await_resume()
    {
        //        std::cout << "Sender::await_resume" << std::endl;
        // TODO: handle closed chan
    }

  private:
    int value;
    channel* chan;
};

class Sender
{
  public:
    Sender( channel* chan );
    Sender( const Sender& sender );

    AwaitableSend send( int value );

  private:
    channel* chan;
};