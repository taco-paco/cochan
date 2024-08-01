#pragma once

#include <iostream>
#include <coroutine>
#include <memory>

#include <cochan/channel.hpp>

namespace cochan
{

template< class T >
class Receiver;

template< class T >
class AwaitableReceive
{
  public:
    AwaitableReceive() = delete;
    AwaitableReceive( AwaitableReceive&& other ) noexcept
        : result( std::move( other.result ) )
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

        std::unique_lock< std::mutex > guard( chan->mutex );
        if( --chan->awaitableReceivers != 0 || chan->receivers != 0 )
        {
            return;
        }

        if( chan->senders == 0 && chan->awaitableSenders == 0 )
        {
            delete chan;
            return;
        }

        const auto waitersCopy = chan->collectSendWaiters();
        chan->closed = true;
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ val_ptr, handle ] = el;
            chan->scheduleFunc( handle );
        } );
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
    explicit AwaitableReceive( Channel< T >* theChan )
        : chan( theChan )
    {
        chan->awaitableReceivers++;
    }

    friend Receiver< T >;

    Channel< T >* chan;
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

        std::unique_lock< std::mutex > guard( chan->mutex );
        if( --chan->receivers != 0 || chan->awaitableReceivers != 0 )
        {
            return;
        }

        if( chan->senders == 0 && chan->awaitableSenders == 0 )
        {
            delete chan;
            return;
        }

        const auto waitersCopy = chan->collectSendWaiters();
        chan->closed = true;
        guard.unlock();

        std::for_each( waitersCopy.begin(), waitersCopy.end(), [ this ]( const auto& el ) {
            const auto [ val_ptr, handle ] = el;
            chan->scheduleFunc( handle );
        } );
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
    explicit Receiver( Channel< T >* theChan )
        : chan( theChan )
    {
        chan->receivers++;
    }

    template< class U >
    friend std::tuple< Sender< U >, Receiver< U > > makeChannel( std::size_t capacity, const ScheduleFunc& );

    Channel< T >* chan;
};

}; // namespace cochan

// 1. if( --chan->awaitableReceivers != 0 || chan->receivers != 0 )
//{
//     return;
// }
//
// 2. if( chan->awaitableReceivers != 0 || --chan->receivers != 0 )
//{
//     return;
// }

// first 2, if chan->awaitableReceivers != 0 - true. Ok.