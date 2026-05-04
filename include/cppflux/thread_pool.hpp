#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  ThreadPool  —  bounded elastic thread pool
// ─────────────────────────────────────────────────────────────────────────────
//  Mirrors Spring's Schedulers.boundedElastic(): threads are created on demand
//  up to a configured maximum; idle threads stay alive for reuse.
//  Use via Schedulers::boundedElastic() — direct construction is rarely needed.
//
//  Offloading CPU-bound work from an async handler:
//
//    auto result = co_await Schedulers::boundedElastic().dispatch([=] {
//        return heavy_compute(input);   // runs on a worker thread
//    });
//    // resumes with result
// ─────────────────────────────────────────────────────────────────────────────

#include <condition_variable>
#include <coroutine>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(int max_threads);
    ~ThreadPool();

    void submit(std::function<void()> task);

    int size() const { return static_cast<int>(workers_.size()); }
    int max_size() const { return max_workers_; }

    // Suspend the calling coroutine, run f() on a worker thread, return the result.
    template <typename F>
    auto dispatch(F&& f);  // defined below

private:
    void run();

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mu_;
    std::condition_variable           cv_;
    int                               max_workers_;
    int                               idle_{0};
    bool                              stop_{false};
};

// ── Awaitable returned by dispatch() — implementation detail ─────────────────
// What happens when you co_await pool.dispatch(f):
//   1. coroutine suspends; f is queued on a worker thread
//   2. worker runs f(), stores result
//   3. coroutine resumes on the worker thread with the result

template <typename F, typename T = std::invoke_result_t<std::decay_t<F>>>
struct PoolAwaitable {
    ThreadPool&        pool;
    std::decay_t<F>    func;
    std::optional<T>   result;
    std::exception_ptr exception;

    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        pool.submit([this, h]() mutable {
            try {
                result.emplace(std::invoke(func));
            } catch (...) {
                exception = std::current_exception();
            }
            h.resume();
        });
    }

    T await_resume() {
        if (exception) {
            std::rethrow_exception(exception);
        }
        return std::move(*result);
    }
};

template <typename F>
auto ThreadPool::dispatch(F&& f) {
    return PoolAwaitable<F>{*this, std::forward<F>(f)};
}
