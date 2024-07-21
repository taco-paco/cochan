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
    }

    // TODO
    ~AwaitableReceive() = default;

    constexpr bool await_ready()
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        return chan->handleReceive( std::make_pair( &storage, handle ) );
    }

    // TODO: return std::optional<int>
    T await_resume()
    {
        // TODO: return {} in case closed
        return std::move( *std::launder( reinterpret_cast< T* >( &storage ) ) );
    }

  private:
    channel< T >* chan;
    channel< T >::storage_type storage;
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