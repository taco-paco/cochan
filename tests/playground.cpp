#include <coroutine>
#include <iostream>

struct promise_type;

struct MyCoroutine: std::coroutine_handle< promise_type >
{
    using promise_type = ::promise_type;

    MyCoroutine( std::coroutine_handle< promise_type > h )
        : handle( h )
    {
    }
    ~MyCoroutine()
    {
        if( handle )
            handle.destroy();
    }

    std::coroutine_handle< promise_type > handle;
};

struct suspend_always_custom
{
    constexpr bool await_ready() const noexcept
    {
        return false;
    }

    constexpr void await_suspend( std::coroutine_handle< promise_type > ) const noexcept
    {
        return;
    }

    void await_resume() const noexcept
    {
        std::cout << "suspend_always_custom" << std::endl;
    }
};

struct promise_type
{
    MyCoroutine get_return_object()
    {
        return MyCoroutine{ std::coroutine_handle< promise_type >::from_promise( *this ) };
    }

    std::suspend_always initial_suspend()
    {
        return {};
    }
    suspend_always_custom final_suspend() noexcept
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

MyCoroutine my_coroutine()
{
    std::cout << "Step 1\n";
    co_await std::suspend_always{};
    std::cout << "Step 2\n";
}

int main()
{
    auto coro = my_coroutine();
    coro.handle.resume();
    coro.handle.resume();
}