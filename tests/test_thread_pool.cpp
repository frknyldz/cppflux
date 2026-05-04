#include "cppflux/thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <mutex>
#include <thread>

TEST(ThreadPool, ExecutesSingleTask) {
    ThreadPool      pool(1);
    std::latch      done{1};
    std::atomic<int> ran{0};

    pool.submit([&] {
        ++ran;
        done.count_down();
    });

    done.wait();
    EXPECT_EQ(ran, 1);
}

TEST(ThreadPool, ExecutesAllTasks) {
    ThreadPool       pool(4);
    constexpr int    N = 100;
    std::latch       done{N};
    std::atomic<int> ran{0};

    for (int i = 0; i < N; ++i) {
        pool.submit([&] {
            ++ran;
            done.count_down();
        });
    }

    done.wait();
    EXPECT_EQ(ran, N);
}

TEST(ThreadPool, RespectsMaxWorkers) {
    constexpr int MAX   = 3;
    constexpr int TASKS = MAX + 5;
    ThreadPool    pool(MAX);

    std::mutex gate;
    gate.lock();
    std::latch done{TASKS};

    for (int i = 0; i < TASKS; ++i) {
        pool.submit([&] {
            // All tasks block until gate unlocks — forces MAX threads to be in-flight
            std::lock_guard<std::mutex> l(gate);
            done.count_down();
        });
    }

    // Give the pool time to create threads up to its limit
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_LE(pool.size(), MAX);
    EXPECT_EQ(pool.max_size(), MAX);

    gate.unlock();
    done.wait();
}

TEST(ThreadPool, ThreadsAreReused) {
    // Submit tasks in two waves. Threads created in wave 1 should handle wave 2.
    ThreadPool    pool(4);
    std::latch    wave1{4};
    std::latch    wave2{4};
    std::atomic<int> ran{0};

    for (int i = 0; i < 4; ++i) {
        pool.submit([&] { ++ran; wave1.count_down(); });
    }
    wave1.wait();
    int size_after_wave1 = pool.size();

    for (int i = 0; i < 4; ++i) {
        pool.submit([&] { ++ran; wave2.count_down(); });
    }
    wave2.wait();

    EXPECT_EQ(pool.size(), size_after_wave1);  // no new threads needed
    EXPECT_EQ(ran, 8);
}

TEST(ThreadPool, DestroyDrainsQueue) {
    std::atomic<int> ran{0};
    {
        ThreadPool pool(2);
        // Flood the queue before threads can drain it
        for (int i = 0; i < 50; ++i) {
            pool.submit([&] { ++ran; });
        }
    }  // destructor blocks until all tasks complete
    EXPECT_EQ(ran, 50);
}

TEST(ThreadPool, TasksRunOnWorkerThreads) {
    ThreadPool                pool(2);
    std::thread::id           main_id = std::this_thread::get_id();
    std::atomic<bool>         on_worker{false};
    std::latch                done{1};

    pool.submit([&] {
        on_worker = (std::this_thread::get_id() != main_id);
        done.count_down();
    });

    done.wait();
    EXPECT_TRUE(on_worker);
}
