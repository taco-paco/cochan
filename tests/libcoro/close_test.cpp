#include <atomic>

#include <gtest/gtest.h>
#include <coro/coro.hpp>
#include <cochan/cochan.hpp>

template< class T >
void drop( T t )
{
}

class AwaitableLibcoroTest: public ::testing::Test
{
  protected:
    AwaitableLibcoroTest()
        : tp( coro::thread_pool::options{ .thread_count = 4 } )
    {
    }

    coro::thread_pool tp;
};

TEST_F( AwaitableLibcoroTest, ReceiverClose )
{
    std::atomic_bool stop = false;
    coro::latch l{ 1 };
    auto receive = [ & ]( cochan::Receiver< int > receiver ) -> coro::task< void > {
        while( true )
        {
            auto val = co_await receiver.receive();
            if( !val )
            {
                COCHAN_ASSERT( false, "Shouldn;t be reached" );
            }

            if( stop )
            {
                drop( std::move( receiver ) );
                l.count_down();
                break;
            }
        }
    };

    constexpr uint NUM_OF_SENDS = 237;
    constexpr uint DROP_AT = 101;
    auto send = [ & ]( cochan::Sender< int > sender ) -> coro::task< bool > {
        for( int i = 0; i < NUM_OF_SENDS; i++ )
        {
            if( i == DROP_AT )
            {
                stop = true;
            }

            co_await sender.send( i );

            if( i == DROP_AT )
            {
                co_await l;
                break;
            }
        }

        try
        {
            co_await sender.send( 101 );
        }
        catch( cochan::ChannelClosedException& ex )
        {
            co_return true;
        }

        co_return false;
    };

    auto scheduleFunc = [ this ]( std::coroutine_handle<> handle ) {
        auto scheduleAwaitable = tp.schedule();
        scheduleAwaitable.await_suspend( handle );
    };

    auto task = [ & ]() -> coro::task< uint > {
        auto [ sender, receiver ] = cochan::makeChannel< int >( 7, scheduleFunc );

        auto sendTask = send( std::move( sender ) );
        auto receiveTask = receive( std::move( receiver ) );
        auto [ flag, _ ] = co_await coro::when_all( std::move( sendTask ), std::move( receiveTask ) );

        co_return flag.return_value();
    };

    auto flag = coro::sync_wait( task() );
    ASSERT_EQ( flag, true );
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
