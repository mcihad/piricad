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

/// What a finished coroutine awaits, so that it hands control to its awaiter
/// rather than returning to the driver.
///
/// The three members are the awaiter protocol the compiler calls; their names are
/// fixed by the language, not chosen here.
struct FinalAwaiter
{
    /// Never ready: a finished task must always suspend so that `await_suspend`
    /// below gets the chance to transfer to the awaiter.
    bool await_ready() const noexcept { return false; }

    /// SYMMETRIC TRANSFER, and the reason this type exists at all.
    ///
    /// Returning the awaiter's handle makes the compiler tail-call into it instead
    /// of resuming it on top of the current frame. A command that awaits a command
    /// that awaits a command is an ordinary shape here — `ÇİZGİ` awaiting a point
    /// awaiting a snap — and without this the stack would grow with every link and
    /// a long interactive session would eventually overflow it.
    ///
    /// `noop_coroutine()` is the "nobody is waiting" case: the task was driven
    /// from the outside and control returns to the driver.
    template<class Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
    {
        auto cont = h.promise().continuation;
        return cont ? cont : std::noop_coroutine();
    }

    /// Nothing to hand back: the coroutine has already finished.
    void await_resume() const noexcept {}
};

/// The parts of a promise that do not depend on what the task returns.
///
/// Split out so `Task<T>` and `Task<void>` share one definition of laziness,
/// continuation and error capture — the three behaviours that make this type
/// correct — instead of two copies that could drift apart.
struct PromiseBase
{
    /// The coroutine waiting on this one, set by the awaiter. Empty when the task
    /// is driven from the outside.
    std::coroutine_handle<> continuation{};

    /// An exception thrown inside the coroutine, kept until somebody asks for the
    /// result. A coroutine cannot let an exception escape its frame, so it is
    /// caught here and rethrown at the awaiting site where a caller can handle it.
    std::exception_ptr error{};

    /// LAZY. Nothing in the body runs when the coroutine is created; it runs when
    /// a driver resumes it. A command that ran on construction could not be
    /// validated before its first edit, and Article 1.3 puts validation before
    /// the transaction.
    std::suspend_always initial_suspend() noexcept { return {}; }

    /// Suspends at the end rather than destroying the frame, so the result stays
    /// readable and the awaiter can be resumed by symmetric transfer.
    FinalAwaiter final_suspend() noexcept { return {}; }

    /// Captures instead of terminating. `noexcept` because letting this throw
    /// would call `std::terminate` and take the editor down with the drawing.
    void unhandled_exception() noexcept { error = std::current_exception(); }
};

} // namespace detail

/// A lazy coroutine returning `T`. `Task<void>` is specialised below.
template<class T = void> class Task;

/// A coroutine that produces a value.
///
/// This is what a command body returns and what `co_await ctx.point(...)` composes
/// with. The driver is the command bus: it resumes the task, the task suspends on
/// each input it needs, and the bus resumes it again when the value arrives — from
/// a mouse click, a typed coordinate, a script argument or an AI tool call, with
/// the body unable to tell which (piricad.md §2.4).
template<class T> class Task
{
public:
    /// The promise the compiler builds for this coroutine. Its member names are
    /// fixed by the language.
    struct promise_type : detail::PromiseBase
    {
        /// The result, stored in place rather than in an `optional<T>`.
        ///
        /// `T` is not required to be default-constructible — a `Result<Point2>` is
        /// not — so the slot is raw storage and the value is constructed into it
        /// exactly once, when `return_value` runs.
        alignas(T) unsigned char storage[sizeof(T)]{};

        /// Whether `storage` holds a live object, and therefore whether the
        /// destructor below has anything to destroy. A coroutine abandoned before
        /// it returned leaves this false.
        bool has_value{false};

        /// Hands the caller its `Task` handle. Called by the compiler at the top
        /// of the coroutine, before the body runs.
        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        /// Receives `co_return v`. Templated so a value convertible to `T` moves
        /// straight into place instead of being converted and then copied.
        template<class U = T> void return_value(U&& v)
        {
            ::new (static_cast<void*>(storage)) T(std::forward<U>(v));
            has_value = true;
        }

        /// The value, or the exception the body threw.
        ///
        /// Rethrowing HERE is the point: the throw happens on the awaiting side,
        /// where a caller has a try/catch and a transaction to roll back, rather
        /// than inside a coroutine frame where it could only terminate.
        T& result()
        {
            if (error) std::rethrow_exception(error);
            return *std::launder(reinterpret_cast<T*>(storage));
        }

        /// Destroys the stored value if one was constructed. `launder` because the
        /// object was created into raw storage.
        ~promise_type()
        {
            if (has_value) std::launder(reinterpret_cast<T*>(storage))->~T();
        }
    };

    /// An empty task. Awaiting or resuming one is a no-op rather than a crash:
    /// a command that never started is a normal state on the cancellation path.
    Task() = default;

    /// Adopts a coroutine handle. Only `get_return_object` calls this.
    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    /// Move-only, because a Task OWNS its coroutine frame and two owners would
    /// destroy it twice. The moved-from task is left empty.
    Task(Task&& o) noexcept : handle_(std::exchange(o.handle_, {})) {}

    /// Destroys any frame this task already held before adopting the other's.
    Task& operator=(Task&& o) noexcept
    {
        if (this != &o) {
            destroy();
            handle_ = std::exchange(o.handle_, {});
        }
        return *this;
    }

    /// Deleted: see the move constructor. Copying would double-free the frame.
    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;

    /// Destroys the coroutine frame. A task abandoned mid-suspension — an ESC in
    /// the middle of `ÇİZGİ` — is destroyed here, and its locals with it.
    ~Task() { destroy(); }

    /// Whether this task owns a frame at all.
    bool valid() const noexcept { return static_cast<bool>(handle_); }

    /// Whether the body has run to completion. An empty task counts as done, so a
    /// driver loop terminates instead of spinning on nothing.
    bool done() const noexcept { return !handle_ || handle_.done(); }

    /// Drives the task from the outside; the command bus is that driver. Safe to
    /// call on an empty or finished task.
    void resume()
    {
        if (handle_ && !handle_.done()) handle_.resume();
    }

    /// The value the body returned, or a rethrow of what it threw. Undefined
    /// before the task is `done()`.
    T& result() { return handle_.promise().result(); }

    /// The raw handle, for a driver that needs to inspect the frame. Nothing
    /// outside /src/command should need this.
    std::coroutine_handle<promise_type> handle() const noexcept { return handle_; }

    /// Makes a Task awaitable from another coroutine.
    ///
    /// Rvalue-qualified: awaiting consumes the task, because the awaiter takes
    /// over resuming it and two owners resuming one frame is a crash.
    auto operator co_await() && noexcept
    {
        /// The awaiter protocol, whose member names the language fixes.
        struct Awaiter
        {
            /// The awaited task's frame.
            std::coroutine_handle<promise_type> h;

            /// An already-finished task does not suspend the awaiter at all.
            bool await_ready() const noexcept { return !h || h.done(); }

            /// Records who is waiting, then transfers into the awaited task —
            /// symmetric transfer again, so a chain of awaits is flat on the
            /// stack.
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                h.promise().continuation = awaiting;
                return h;
            }

            /// Hands the awaiting coroutine the value, or rethrows.
            T& await_resume() { return h.promise().result(); }
        };

        return Awaiter{handle_};
    }

private:
    /// Frees the frame exactly once and clears the handle, so a second call and a
    /// destructor after a move are both harmless.
    void destroy()
    {
        if (handle_) {
            handle_.destroy();
            handle_ = {};
        }
    }

    std::coroutine_handle<promise_type> handle_{};
};

/// A coroutine that produces nothing.
///
/// This is what almost every command body returns: a command's output is the
/// transaction it built and the journal line it recorded, not a return value. The
/// specialisation exists because the language requires `return_void` and
/// `return_value` to be mutually exclusive, and because there is no result to
/// store — so no raw storage, no placement new and no destructor to run.
template<> class Task<void>
{
public:
    /// The promise for a value-less coroutine. Member names are fixed by the
    /// language.
    struct promise_type : detail::PromiseBase
    {
        /// Hands the caller its `Task` handle, before the body runs.
        Task get_return_object()
        {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        /// Receives a bare `co_return`, or falling off the end of the body.
        void return_void() noexcept {}

        /// Rethrows what the body threw, on the awaiting side where it can be
        /// caught and the transaction rolled back. Does nothing when it did not
        /// throw.
        void result()
        {
            if (error) std::rethrow_exception(error);
        }
    };

    /// An empty task. Awaiting or resuming one is a no-op rather than a crash:
    /// a command that never started is a normal state on the cancellation path.
    Task() = default;

    /// Adopts a coroutine handle. Only `get_return_object` calls this.
    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}

    /// Move-only, because a Task OWNS its coroutine frame and two owners would
    /// destroy it twice. The moved-from task is left empty.
    Task(Task&& o) noexcept : handle_(std::exchange(o.handle_, {})) {}

    /// Destroys any frame this task already held before adopting the other's.
    Task& operator=(Task&& o) noexcept
    {
        if (this != &o) {
            destroy();
            handle_ = std::exchange(o.handle_, {});
        }
        return *this;
    }

    /// Deleted: see the move constructor. Copying would double-free the frame.
    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;

    /// Destroys the coroutine frame. A command abandoned mid-suspension — an ESC
    /// halfway through `ÇİZGİ` — is destroyed here, and its locals with it.
    ~Task() { destroy(); }

    /// Whether this task owns a frame at all.
    bool valid() const noexcept { return static_cast<bool>(handle_); }

    /// Whether the body has run to completion. An empty task counts as done, so a
    /// driver loop terminates instead of spinning on nothing.
    bool done() const noexcept { return !handle_ || handle_.done(); }

    /// Drives the task from the outside; the command bus is that driver. Safe to
    /// call on an empty or finished task.
    void resume()
    {
        if (handle_ && !handle_.done()) handle_.resume();
    }

    /// Rethrows what the body threw, if anything. There is no value to return.
    void result()
    {
        if (handle_) handle_.promise().result();
    }

    /// The raw handle, for a driver that needs to inspect the frame. Nothing
    /// outside /src/command should need this.
    std::coroutine_handle<promise_type> handle() const noexcept { return handle_; }

    /// Makes a Task awaitable from another coroutine.
    ///
    /// Rvalue-qualified: awaiting consumes the task, because the awaiter takes
    /// over resuming it and two owners resuming one frame is a crash.
    auto operator co_await() && noexcept
    {
        /// The awaiter protocol, whose member names the language fixes.
        struct Awaiter
        {
            /// The awaited task's frame.
            std::coroutine_handle<promise_type> h;

            /// An already-finished task does not suspend the awaiter at all.
            bool await_ready() const noexcept { return !h || h.done(); }

            /// Records who is waiting, then transfers into the awaited task, so a
            /// chain of awaits stays flat on the stack.
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                h.promise().continuation = awaiting;
                return h;
            }

            /// Rethrows if the awaited task threw; otherwise resumes silently.
            void await_resume()
            {
                if (h) h.promise().result();
            }
        };

        return Awaiter{handle_};
    }

private:
    /// Frees the frame exactly once and clears the handle, so a second call and a
    /// destructor after a move are both harmless.
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
