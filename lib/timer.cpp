#include "cppflux/timer.hpp"

#include <event2/event.h>

#include <cassert>

#include "io_thread.hpp"

bool TimerAwaitable::await_ready() noexcept {
    return duration_.count() == 0;
}

void TimerAwaitable::await_suspend(std::coroutine_handle<> h) {
    assert(tl_base && "async_sleep: must be called from an I/O thread");
    auto    us = duration_.count();
    timeval tv{static_cast<long>(us / 1'000'000), static_cast<long>(us % 1'000'000)};
    event_base_once(
        tl_base, -1, EV_TIMEOUT,
        [](evutil_socket_t, short, void* arg) {
            std::coroutine_handle<>::from_address(arg).resume();
        },
        h.address(), &tv);
}

void TimerAwaitable::await_resume() noexcept {}

TimerAwaitable async_sleep(std::chrono::milliseconds ms) {
    return TimerAwaitable{std::chrono::duration_cast<std::chrono::microseconds>(ms)};
}
