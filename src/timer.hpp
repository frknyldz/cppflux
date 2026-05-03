#pragma once
#include <chrono>
#include <coroutine>
#include <event2/event.h>

// Suspends the coroutine for `ms` milliseconds without holding any thread.
// Resumes on the I/O thread that owns `base` — like reactor's Mono.delay().
struct TimerAwaitable {
    event_base* base;
    timeval     tv;

    bool await_ready() noexcept { return tv.tv_sec == 0 && tv.tv_usec == 0; }

    void await_suspend(std::coroutine_handle<> h) {
        event_base_once(base, -1, EV_TIMEOUT,
            [](evutil_socket_t, short, void* arg) {
                std::coroutine_handle<>::from_address(arg).resume();
            },
            h.address(), &tv);
    }

    void await_resume() noexcept {}
};

inline TimerAwaitable async_sleep(event_base* base, std::chrono::milliseconds ms) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(ms).count();
    return {base, {static_cast<long>(us / 1'000'000), static_cast<long>(us % 1'000'000)}};
}
