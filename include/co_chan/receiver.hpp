#pragma once

#include <iostream>
#include <coroutine>

#include <co_chan/channel.hpp>

class AwaitableReceive
{
  public:
    explicit AwaitableReceive( channel* theChan )
        : chan( theChan )
    {
        slot = new int();
    }

    ~AwaitableReceive()
    {
        // TODO: think what if channel destructed itself
        delete slot;
    }

    bool await_ready()
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->pop( std::make_pair( slot, handle ) );
    }

    // TODO: return std::optional<int>
    int await_resume()
    {
        // TODO: return {} in case closed
        return *slot;
    }

  private:
    int* slot;
    channel* chan;
};

class Receiver
{
  public:
    Receiver( channel* theChan )
        : chan( theChan )
    {
    }

    Receiver( const Receiver& receiver )
    {
        this->chan = receiver.chan;
    }

    AwaitableReceive receive()
    {
        return AwaitableReceive( chan );
    }

  private:
    channel* chan;
};