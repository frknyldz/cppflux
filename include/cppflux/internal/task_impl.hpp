#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Task<T> — coroutine protocol implementation
//
//  You do not need to read this file to use the library.
//  See task.hpp for the public API and usage examples.
//
//  How the pieces fit together:
//
//    ┌──────────────┐  co_await   ┌──────────────┐
//    │ parent task  │ ──────────► │  child task  │
//    │  (suspended) │             │   (running)  │
//    └──────────────┘             └──────┬───────┘
//            ▲                           │ co_return
//            │   symmetric transfer      ▼
//            └─────────────────── FinalAwaiter
//                                 (jumps directly to parent, no stack growth)
//
//    Task::detach(callback):
//      Spins up a fire-and-forget driver coroutine (detail::Driven) that
//      subscribes to the lazy Task and delivers the final value to callback.
//      The router calls detach() for every async handler — user code rarely
//      needs it directly.
// ─────────────────────────────────────────────────────────────────────────────

#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <variant>

template <typename T>
class Task;

namespace detail {

// Fire-and-forget driver coroutine.
// Created by Task::detach(); starts eagerly and self-destructs when done.
template <typename T>
struct Driven {
    struct promise_type {
        Driven             get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }  // start immediately
        std::suspend_never final_suspend() noexcept { return {}; }    // self-destruct on done
        void               return_void() noexcept {}
        void               unhandled_exception() noexcept { std::terminate(); }
    };
};

template <typename T>
Driven<T> drive(Task<T>                            task,
                std::function<void(T)>             on_done,
                std::function<void(std::exception_ptr)> on_error) {
    try {
        on_done(co_await std::move(task));
    } catch (...) {
        if (on_error) {
            on_error(std::current_exception());
        }
    }
}

}  // namespace detail

// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
class Task {
    template <typename U>
    friend detail::Driven<U> detail::drive(Task<U>, std::function<void(U)>);

    // Shared result bag. Lives in a shared_ptr so the value is still accessible
    // after the coroutine frame is destroyed at final_suspend.
    struct State {
        std::variant<std::monostate, T, std::exception_ptr> result;
        std::coroutine_handle<>                             continuation{std::noop_coroutine()};
        std::coroutine_handle<>                             self;
    };

public:
    // ── C++20 coroutine protocol — the compiler looks for this nested type ───
    struct promise_type {
        std::shared_ptr<State> state = std::make_shared<State>();

        Task get_return_object() {
            auto h      = std::coroutine_handle<promise_type>::from_promise(*this);
            state->self = h;
            return Task{h, state};
        }

        // Lazy: body doesn't run until someone co_awaits or calls detach().
        std::suspend_always initial_suspend() noexcept { return {}; }

        // Symmetric transfer: on co_return, jump directly to whoever is waiting
        // rather than returning through C-style frames — O(1) stack depth
        // regardless of how many tasks are chained.
        struct FinalAwaiter {
            bool                    await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                return h.promise().state->continuation;
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void         return_value(T v) { state->result.template emplace<1>(std::move(v)); }
        void unhandled_exception() { state->result.template emplace<2>(std::current_exception()); }
    };

    // ── Awaitable: what happens when a parent coroutine co_awaits this Task ──
    bool await_ready() noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
        state_->continuation = parent;
        return std::exchange(handle_, {});  // symmetric transfer into this task
    }

    T await_resume() {
        if (auto h = state_->self; h && h.done()) {
            h.destroy();
        }
        if (auto* ex = std::get_if<2>(&state_->result)) {
            std::rethrow_exception(*ex);
        }
        return std::move(std::get<1>(state_->result));
    }

    // ── Public interface ──────────────────────────────────────────────────────

    // Start execution. on_done is called with the resolved value on whichever
    // thread the last step ran on. The router uses this; user code rarely does.
    void detach(std::function<void(T)>                  on_done,
                std::function<void(std::exception_ptr)> on_error = nullptr) && {
        detail::drive(std::move(*this), std::move(on_done), std::move(on_error));
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    Task(Task&& o) noexcept : handle_(std::exchange(o.handle_, {})), state_(std::move(o.state_)) {}

    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;
    Task& operator=(Task&&)      = delete;

private:
    Task(std::coroutine_handle<promise_type> h, std::shared_ptr<State> s)
        : handle_(h), state_(std::move(s)) {}

    std::coroutine_handle<promise_type> handle_;
    std::shared_ptr<State>              state_;
};
