#pragma once
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(int n);
    ~ThreadPool();

    void submit(std::function<void()> task);
    int  size()    const { return static_cast<int>(workers_.size()); }
    int  max_size() const { return max_workers_; }

    // co_await pool.dispatch([]{ return heavy_work(); })
    // suspends coroutine, runs lambda on worker, resumes with result
    template<typename F>
    auto dispatch(F&& f);  // defined after PoolAwaitable below

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

// Awaitable returned by ThreadPool::dispatch<F>(f).
// Suspends the calling coroutine, runs f() on a worker thread,
// then resumes the coroutine with the result — like
// Mono.fromCallable(f).subscribeOn(Schedulers.boundedElastic())
template<typename F,
         typename T = std::invoke_result_t<std::decay_t<F>>>
struct PoolAwaitable {
    ThreadPool&         pool;
    std::decay_t<F>     func;
    std::optional<T>    result;

    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        pool.submit([this, h]() mutable {
            result.emplace(std::invoke(func));
            h.resume();  // resume coroutine on this worker thread
        });
    }

    T await_resume() { return std::move(*result); }
};

template<typename F>
auto ThreadPool::dispatch(F&& f) {
    return PoolAwaitable<F>{*this, std::forward<F>(f)};
}
