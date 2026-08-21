// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the C++20 coroutine task type.
//
// piricad.md §7.1 lists coroutines as the single most critical C++20 feature in
// this project, and §2.4 makes them mandatory for interactive commands: a CAD
// command is an ask–wait–ask flow, and a hand-written state machine for it is
// unreadable. §7.1 also says to write this type ourselves rather than depend on
// cppcoro — about 300 lines buys full control.
//
// Semantics:
//   * lazy: nothing runs until the task is resumed by a driver
//   * symmetric transfer on completion, so long chains do not grow the stack
//   * an unhandled exception is captured and rethrown at the awaiting site
#pragma once

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>

namespace piricad::command {

namespace detail {

/// Resumes the awaiting coroutine (if any) via symmetric transfer.
struct FinalAwaiter
{
    bool await_ready() const noexcept { return false; }

    template<class Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
    {
        auto cont = h.promise().continuation;
        return cont ? cont : std::noop_coroutine();
    }

    void await_resume() const noexcept {}
};

struct PromiseBase
{
    std::coroutine_handle<> continuation{};
    std::exception_ptr error{};

    std::suspend_always initial_suspend() noexcept { return {}; }

    FinalAwaiter final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept { error = std::current_exception(); }
};

} // namespace detail

template<class T = void> class Task;

template<class T> class Task
{
public:
    struct promise_type : detail::PromiseBase
    {
        alignas(T) unsigned char storage[sizeof(T)]{};
        bool has_value{false};

        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        template<class U = T> void return_value(U&& v)
        {
            ::new (static_cast<void*>(storage)) T(std::forward<U>(v));
            has_value = true;
        }

        T& result()
        {
            if (error) std::rethrow_exception(error);
            return *std::launder(reinterpret_cast<T*>(storage));
        }

        ~promise_type()
        {
            if (has_value) std::launder(reinterpret_cast<T*>(storage))->~T();
        }
    };

    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    Task(Task&& o) noexcept : handle_(std::exchange(o.handle_, {})) {}

    Task& operator=(Task&& o) noexcept
    {
        if (this != &o) {
            destroy();
            handle_ = std::exchange(o.handle_, {});
        }
        return *this;
    }

    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;

    ~Task() { destroy(); }

    bool valid() const noexcept { return static_cast<bool>(handle_); }

    bool done() const noexcept { return !handle_ || handle_.done(); }

    /// Drives the task from the outside (the command bus is the driver).
    void resume()
    {
        if (handle_ && !handle_.done()) handle_.resume();
    }

    T& result() { return handle_.promise().result(); }

    std::coroutine_handle<promise_type> handle() const noexcept { return handle_; }

    auto operator co_await() && noexcept
    {
        struct Awaiter
        {
            std::coroutine_handle<promise_type> h;

            bool await_ready() const noexcept { return !h || h.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                h.promise().continuation = awaiting;
                return h;
            }

            T& await_resume() { return h.promise().result(); }
        };

        return Awaiter{handle_};
    }

private:
    void destroy()
    {
        if (handle_) {
            handle_.destroy();
            handle_ = {};
        }
    }

    std::coroutine_handle<promise_type> handle_{};
};

template<> class Task<void>
{
public:
    struct promise_type : detail::PromiseBase
    {
        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_void() noexcept {}

        void result()
        {
            if (error) std::rethrow_exception(error);
        }
    };

    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    Task(Task&& o) noexcept : handle_(std::exchange(o.handle_, {})) {}

    Task& operator=(Task&& o) noexcept
    {
        if (this != &o) {
            destroy();
            handle_ = std::exchange(o.handle_, {});
        }
        return *this;
    }

    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;

    ~Task() { destroy(); }

    bool valid() const noexcept { return static_cast<bool>(handle_); }

    bool done() const noexcept { return !handle_ || handle_.done(); }

    void resume()
    {
        if (handle_ && !handle_.done()) handle_.resume();
    }

    void result()
    {
        if (handle_) handle_.promise().result();
    }

    std::coroutine_handle<promise_type> handle() const noexcept { return handle_; }

    auto operator co_await() && noexcept
    {
        struct Awaiter
        {
            std::coroutine_handle<promise_type> h;

            bool await_ready() const noexcept { return !h || h.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                h.promise().continuation = awaiting;
                return h;
            }

            void await_resume()
            {
                if (h) h.promise().result();
            }
        };

        return Awaiter{handle_};
    }

private:
    void destroy()
    {
        if (handle_) {
            handle_.destroy();
            handle_ = {};
        }
    }

    std::coroutine_handle<promise_type> handle_{};
};

} // namespace piricad::command
