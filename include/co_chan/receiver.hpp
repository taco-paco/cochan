#pragma once

#include <iostream>
#include <coroutine>

#include <co_chan/channel.hpp>

template< class T >
class AwaitableReceive
{
  public:
    explicit AwaitableReceive( channel< T >* theChan )
        : chan( theChan )
    {
        // Need to check if this is ok
        slot = new T();
    }

    ~AwaitableReceive()
    {
        // TODO: think what if channel destructed itself
        delete slot;
    }

    constexpr bool await_ready()
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->handleReceive( std::make_pair( slot, handle ) );
    }

    // TODO: return std::optional<int>
    T await_resume()
    {
        // TODO: return {} in case closed
        return *slot;
    }

  private:
    T* slot;
    channel< T >* chan;
};

template< class T >
class Receiver
{
  public:
    Receiver( channel< T >* theChan )
        : chan( theChan )
    {
    }

    Receiver( const Receiver& receiver )
    {
        this->chan = receiver.chan;
    }

    AwaitableReceive< T > receive()
    {
        return AwaitableReceive( chan );
    }

  private:
    channel< T >* chan;
};