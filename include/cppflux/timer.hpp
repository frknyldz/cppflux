#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  async_sleep  —  non-blocking timer, analogous to Reactor's Mono.delay()
// ─────────────────────────────────────────────────────────────────────────────
//  Suspends the current coroutine for the given duration without holding any
//  thread. The I/O event loop fires a timer and resumes the coroutine when it
//  expires — the same model as Node's `await sleep(ms)` or Go's `time.Sleep`.
//
//  Usage:
//
//    Task<std::string> fetch(const std::string& key) {
//        co_await async_sleep(10ms);
//        co_return "result:" + key;
//    }
//
//  Must be called from a cppflux I/O thread (inside a handler or a Task<T>
//  started by the router). Asserts at runtime otherwise.
// ─────────────────────────────────────────────────────────────────────────────

#include <chrono>
#include <coroutine>

// Returned by async_sleep — awaitable by the coroutine runtime.
// Consumers never interact with this type directly; they only write co_await.
struct TimerAwaitable {
    explicit TimerAwaitable(std::chrono::microseconds d) : duration_(d) {}

    bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<> h);
    void await_resume() noexcept;

private:
    std::chrono::microseconds duration_;
};

TimerAwaitable async_sleep(std::chrono::milliseconds ms);
