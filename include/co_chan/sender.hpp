#pragma once

#include <iostream>
#include <coroutine>
#include <exception>

#include <co_chan/channel.hpp>

template< class T >
class Sender;

template< class T >
class AwaitableSend
{
  public:
    AwaitableSend( const AwaitableSend& ) = delete;
    AwaitableSend( AwaitableSend&& other ) noexcept
        : value( std::move( other.value ) )
        , chan( other.chan )
    {
        other.chan = nullptr;
    }

    ~AwaitableSend()
    {
        if( !chan )
        {
            return;
        }

        // The desctruction of all meands permits == 0, and empty senderWaiters
        // Would be the case if it ouldn't be 0
        if( --chan->awaitableSenders == 0 && chan->senders == 0 )
        {
            if( chan->receivers == 0 && chan->awaitableReceivers == 0 )
            {
                delete chan;
                return;
            }

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
        return chan->handleSend( std::make_pair( &value, handle ) );
    }

    void await_resume()
    {
    }

  private:
    AwaitableSend( const T& theValue, channel< T >* theChan )
        : value( theValue )
        , chan( theChan )
    {
        chan->awaitableSenders++;
    }

    AwaitableSend( T&& theValue, channel< T >* theChan )
        : value( std::move( theValue ) )
        , chan( theChan )
    {
        chan->awaitableSenders++;
    }

    friend Sender< T >;

    T value;
    channel< T >* chan;
};

template< class T >
class Sender
{
  public:
    Sender() = delete;

    Sender( const Sender& sender )
    {
        chan = sender.chan;
        chan->senders++;
    }

    Sender( Sender&& other ) noexcept
        : chan( other.chan )
    {
        other.chan = nullptr;
    }

    ~Sender()
    {
        if( !chan )
        {
            return;
        }

        // TODO: Check if  chan->senders == 0 is atomic in relation to awaitableSenders == 0
        // Or if it can cause problems
        if( --chan->senders == 0 && chan->awaitableSenders == 0 )
        {
            if( chan->receivers == 0 && chan->awaitableReceivers == 0 )
            {
                delete chan;
                return;
            }

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
    Sender( channel< T >* theChan )
        : chan( theChan )
    {
        chan->senders++;
    }

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity );

    channel< T >* chan;
};