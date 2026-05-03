#include "thread_pool.hpp"

ThreadPool::ThreadPool(int n) : max_workers_(n) {}

ThreadPool::~ThreadPool() {
    { std::lock_guard lock(mu_); stop_ = true; }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard lock(mu_);
        tasks_.push(std::move(task));

        // Spawn a new thread only if all existing ones are busy and we're under the limit
        if (idle_ == 0 && static_cast<int>(workers_.size()) < max_workers_)
            workers_.emplace_back([this] { run(); });
    }
    cv_.notify_one();
}

void ThreadPool::run() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mu_);
            ++idle_;
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            --idle_;
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
