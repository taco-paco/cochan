#pragma once

#include <iostream>
#include <coroutine>
#include <exception>

#include <cochan/channel.hpp>

namespace cochan
{

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

        std::unique_lock< std::mutex > guard( chan->mutex );
        if( --chan->awaitableSenders != 0 || chan->senders != 0 )
        {
            return;
        }

        if( chan->receivers == 0 && chan->awaitableReceivers == 0 )
        {
            delete chan;
            return;
        }

        const auto waitersCopy = chan->collectReceiveWaiters();
        chan->closed = true;
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ result, receiverHandle ] = el;
            *result = std::nullopt;
            chan->scheduleFunc( receiverHandle );
        } );
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
    AwaitableSend( const T& theValue, Channel< T >* theChan )
        : value( theValue )
        , chan( theChan )
    {
        chan->awaitableSenders++;
    }

    AwaitableSend( T&& theValue, Channel< T >* theChan )
        : value( std::move( theValue ) )
        , chan( theChan )
    {
        chan->awaitableSenders++;
    }

    friend Sender< T >;

    T value;
    Channel< T >* chan;
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

        std::unique_lock< std::mutex > guard( chan->mutex );
        if( --chan->senders != 0 || chan->awaitableSenders != 0 )
        {
            return;
        }

        if( chan->receivers == 0 && chan->awaitableReceivers == 0 )
        {
            delete chan;
            return;
        }

        const auto waitersCopy = chan->collectReceiveWaiters();
        chan->closed = true;
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ result, receiverHandle ] = el;
            *result = std::nullopt;
            chan->scheduleFunc( receiverHandle );
        } );
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
        if( isClosed() )
        {
            throw ChannelClosedException{};
        }

        return AwaitableSend{ std::forward< T >( value ), chan };
    }

    [[nodiscard]] std::size_t getCapacity() const
    {
        return chan->getCapacity();
    }

    bool isClosed() const
    {
        return chan->isClosed();
    }

  private:
    Sender( Channel< T >* theChan )
        : chan( theChan )
    {
        chan->senders++;
    }

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity, const ScheduleFunc& );

    Channel< T >* chan;
};

} // namespace cochan

// After closed receiver no sender shall be postponed
// assume handleSend called twice