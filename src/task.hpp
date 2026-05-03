#pragma once
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <variant>

template<typename T> class Task;

// ---- Internal fire-and-forget driver coroutine ----------------------
// Drives a Task<T> to completion and calls on_done. Like Reactor's
// subscribe() — this is what "subscribes" to the lazy Task pipeline.
namespace detail {

template<typename T>
struct Driven {
    struct promise_type {
        Driven get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend()   noexcept { return {}; }
        void return_void()                   noexcept {}
        void unhandled_exception()           noexcept { std::terminate(); }
    };
};

template<typename T>
Driven<T> drive(Task<T> task, std::function<void(T)> on_done) {
    on_done(co_await std::move(task));
}

} // namespace detail

// ---- Task<T> — lazy async value, analogous to Reactor's Mono<T> ----
//
// Lazy:  nothing executes until detach() or co_await
// Safe:  result lives in shared State, survives frame destruction
// Chain: co_await another Task<T> for sequential async steps

template<typename T>
class Task {
    template<typename U>
    friend detail::Driven<U> detail::drive(Task<U>, std::function<void(U)>);

public:
    // ---- shared state (outlives the coroutine frame) ----------------
    struct State {
        std::variant<std::monostate, T, std::exception_ptr> result;
        std::coroutine_handle<> continuation{std::noop_coroutine()};
        std::coroutine_handle<> self;  // this task's handle, for cleanup
    };

    // ---- promise_type -----------------------------------------------
    struct promise_type {
        std::shared_ptr<State> state = std::make_shared<State>();

        Task get_return_object() {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            state->self = h;
            return Task{h, state};
        }

        std::suspend_always initial_suspend() noexcept { return {}; } // lazy

        // On completion: symmetric-transfer to whoever is awaiting this task.
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                return h.promise().state->continuation; // resume parent/driver
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend()           noexcept { return {}; }
        void return_value(T v)                          { state->result.template emplace<1>(std::move(v)); }
        void unhandled_exception()                      { state->result.template emplace<2>(std::current_exception()); }
    };

    // ---- awaitable: co_await task -----------------------------------
    bool await_ready() noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
        state_->continuation = parent;
        return std::exchange(handle_, {}); // symmetric transfer: start this task
    }

    T await_resume() {
        // child frame is suspended at final_suspend — safe to destroy
        if (auto h = state_->self; h && h.done()) h.destroy();
        if (auto* ex = std::get_if<2>(&state_->result))
            std::rethrow_exception(*ex);
        return std::move(std::get<1>(state_->result));
    }

    // ---- detach: start pipeline (like Mono.subscribe()) -------------
    // Analogous to reactor's subscribe() — kicks off execution.
    // on_done is called on whichever thread the last step ran on,
    // so callers must route back to the I/O thread if needed.
    void detach(std::function<void(T)> on_done) && {
        detail::drive(std::move(*this), std::move(on_done));
    }

    // ---- lifecycle --------------------------------------------------
    ~Task() { if (handle_) handle_.destroy(); } // destroy if never started

    Task(Task&& o) noexcept
        : handle_(std::exchange(o.handle_, {}))
        , state_(std::move(o.state_)) {}

    Task(const Task&)            = delete;
    Task& operator=(const Task&) = delete;
    Task& operator=(Task&&)      = delete;

private:
    Task(std::coroutine_handle<promise_type> h, std::shared_ptr<State> s)
        : handle_(h), state_(std::move(s)) {}

    std::coroutine_handle<promise_type> handle_;
    std::shared_ptr<State>              state_;
};
