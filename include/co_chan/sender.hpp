#pragma once

#include <iostream>
#include <coroutine>
#include <exception>

#include <co_chan/channel.hpp>

template< class T >
class AwaitableSend
{
  public:
    AwaitableSend( const T& theValue, channel< T >* theChan )
        : value( theValue )
        , chan( theChan )
    {
        // weakptr on Sender instead?
        chan->senderPermits++;
    }

    AwaitableSend( T&& theValue, channel< T >* theChan )
        : value( std::move( theValue ) )
        , chan( theChan )
    {
        chan->senderPermits++;
    }

    AwaitableSend( const AwaitableSend& ) = delete;
    AwaitableSend( AwaitableSend&& other ) noexcept
    {
        // TODO: check
        this->value = std::move( other.value );
        std::swap( this->chan, other.chan );
    }

    ~AwaitableSend()
    {
        if( !executed )
        {
            chan->senderPermits--;
        }

        if( chan->canDelete() )
        {
            delete chan;
            return;
        }

        if( chan->senders == 0 && chan->senderPermits )
        {
            chan->onSenderClose();
            return;
        }
    }

    AwaitableSend& operator=( const AwaitableSend& ) = delete;
    AwaitableSend& operator=( AwaitableSend&& other ) = delete;

    bool await_ready() const
    {
        return false;
    }

    bool await_suspend( std::coroutine_handle<> handle )
    {
        bool suspend = chan->handleSend( std::make_pair( &value, handle ) );
        executed = true;

        // TODO: double free?
        // Assume marked. Last receiver called and got it. It an use will free this
        chan->senderPermits--;
    }

    void await_resume()
    {
    }

  private:
    T value;
    // TODO: use weak_ptr instead and get rid of permits?
    channel< T >* chan;

    // Determines if co_await was called on it
    bool executed = false;
};

template< class T >
class Sender
{
  public:
    Sender( channel< T >* theChan )
        : chan( theChan )
    {
        chan->senders++;
    }

    Sender( const Sender& sender )
    {
        chan = sender.chan;
        chan->senders++;
    }

    Sender( Sender&& sender ) noexcept
    {
        // TODO: check
        std::swap( this->chan, sender.chan );
    }

    ~Sender()
    {
        --chan->senders;

        if( chan->canDelete() )
        {
            delete chan;
            return;
        }

        if( chan->senders == 0 && chan->senderPermits )
        {
            chan->onSenderClose();
            return;
        }
    }

    AwaitableSend< T > send( const T& value )
    {
        if( chan->closed )
        {
            throw ChannelClosedException{};
        }

        return AwaitableSend{ value, chan };
    }

    AwaitableSend< T > send( T&& value )
    {
        if( chan->closed )
        {
            throw ChannelClosedException{};
        }

        return AwaitableSend{ value, chan };
    }

  private:
    // TODO: use shared_ptr instead?
    channel< T >* chan;
};