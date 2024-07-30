#include <coroutine>
#include <iostream>

struct promise_type;

struct MyCoroutine
{
    using promise_type = ::promise_type;

    MyCoroutine( std::coroutine_handle< promise_type > h )
        : handle( h )
    {
    }

    MyCoroutine( MyCoroutine&& other )
        : handle( other.handle )
    {
        other.handle = nullptr;
    }

    ~MyCoroutine()
    {
        if( handle )
        {
            handle.destroy();
        }
    }

    bool resume() const
    {
        if( !handle || handle.done() )
        {
            return false;
        }

        handle.resume();
        return !handle.done();
    }

    std::coroutine_handle< promise_type > handle;
};

struct promise_type
{
    MyCoroutine get_return_object()
    {
        return MyCoroutine{ std::coroutine_handle< promise_type >::from_promise( *this ) };
    }

    std::suspend_never initial_suspend()
    {
        return {};
    }

    std::suspend_always final_suspend() noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
        std::terminate();
    }

    void return_void()
    {
    }
};
