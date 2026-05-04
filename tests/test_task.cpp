#include "cppflux/task.hpp"
#include "cppflux/thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Blocks the calling thread until the detached task's callback fires.
// Needed for tasks that resume on a worker thread (pool dispatch).
struct AsyncResult {
    std::mutex              mu;
    std::condition_variable cv;
    bool                    ready{false};

    void notify() {
        std::lock_guard l(mu);
        ready = true;
        cv.notify_one();
    }

    void wait() {
        std::unique_lock l(mu);
        cv.wait(l, [this] { return ready; });
    }
};

// ── Sync tasks (no async_sleep — resolve on the calling thread) ───────────────

Task<int> immediate(int n) {
    co_return n;
}

Task<std::string> chain() {
    int n = co_await immediate(7);
    co_return "result:" + std::to_string(n);
}

Task<int> nested_chain(int depth) {
    if (depth == 0) {
        co_return 0;
    }
    int sub = co_await nested_chain(depth - 1);
    co_return sub + 1;
}

TEST(Task, SyncValuePropagation) {
    int result = -1;
    std::move(immediate(42)).detach([&](int v) { result = v; });
    EXPECT_EQ(result, 42);
}

TEST(Task, SyncChaining) {
    std::string result;
    std::move(chain()).detach([&](std::string v) { result = std::move(v); });
    EXPECT_EQ(result, "result:7");
}

TEST(Task, DeepChainDoesNotStackOverflow) {
    // Symmetric transfer means this is O(1) stack depth regardless of depth.
    int result = -1;
    std::move(nested_chain(10000)).detach([&](int v) { result = v; });
    EXPECT_EQ(result, 10000);
}

TEST(Task, MoveSemantics) {
    auto t1 = immediate(99);
    auto t2 = std::move(t1);

    int result = -1;
    std::move(t2).detach([&](int v) { result = v; });
    EXPECT_EQ(result, 99);
}

// ── Async tasks via ThreadPool (resume on worker thread) ─────────────────────

TEST(Task, PoolDispatchReturnsValue) {
    ThreadPool pool(2);
    AsyncResult ar;
    std::string result;

    auto task = [&]() -> Task<std::string> {
        co_return co_await pool.dispatch([] { return std::string("from_worker"); });
    };

    std::move(task()).detach([&](std::string v) {
        result = std::move(v);
        ar.notify();
    });

    ar.wait();
    EXPECT_EQ(result, "from_worker");
}

TEST(Task, PoolDispatchRunsOnWorkerThread) {
    ThreadPool         pool(2);
    AsyncResult        ar;
    std::thread::id    main_id = std::this_thread::get_id();
    std::thread::id    worker_id;

    auto task = [&]() -> Task<int> {
        co_return co_await pool.dispatch([&] {
            worker_id = std::this_thread::get_id();
            return 1;
        });
    };

    std::move(task()).detach([&](int) { ar.notify(); });
    ar.wait();

    EXPECT_NE(worker_id, main_id);
}

TEST(Task, MultiplePoolDispatchesChained) {
    ThreadPool   pool(2);
    AsyncResult  ar;
    int          result = 0;

    auto task = [&]() -> Task<int> {
        int a = co_await pool.dispatch([] { return 10; });
        int b = co_await pool.dispatch([] { return 32; });
        co_return a + b;
    };

    std::move(task()).detach([&](int v) {
        result = v;
        ar.notify();
    });

    ar.wait();
    EXPECT_EQ(result, 42);
}

// ── Exception propagation — the 500 path ─────────────────────────────────────

TEST(Task, SyncThrowCallsOnError) {
    auto task = []() -> Task<int> {
        throw std::runtime_error("sync error");
        co_return 0;
    }();

    bool               fired = false;
    std::exception_ptr captured;

    std::move(task).detach(
        [](int) { FAIL() << "on_done must not be called when task throws"; },
        [&](std::exception_ptr ex) {
            fired    = true;
            captured = ex;
        });

    EXPECT_TRUE(fired);
    ASSERT_TRUE(captured);
    EXPECT_THROW(std::rethrow_exception(captured), std::runtime_error);
}

TEST(Task, ChainedThrowCallsOnError) {
    // Exception thrown in an inner task propagates through the chain
    auto inner = []() -> Task<int> {
        throw std::runtime_error("inner error");
        co_return 0;
    };

    auto outer = [&inner]() -> Task<int> {
        int n = co_await inner();
        co_return n + 1;
    };

    bool fired = false;
    std::move(outer()).detach(
        [](int) { FAIL(); },
        [&](std::exception_ptr ex) {
            fired = true;
            EXPECT_THROW(std::rethrow_exception(ex), std::runtime_error);
        });

    EXPECT_TRUE(fired);
}

TEST(Task, PoolDispatchThrowCallsOnError) {
    // Exception thrown inside a dispatched lambda propagates back to the coroutine
    ThreadPool  pool(2);
    AsyncResult ar;
    bool        fired = false;

    auto task = [&]() -> Task<int> {
        co_return co_await pool.dispatch([]() -> int {
            throw std::runtime_error("worker error");
        });
    };

    std::move(task()).detach(
        [](int) { FAIL(); },
        [&](std::exception_ptr ex) {
            fired = true;
            EXPECT_THROW(std::rethrow_exception(ex), std::runtime_error);
            ar.notify();
        });

    ar.wait();
    EXPECT_TRUE(fired);
}

TEST(Task, NoOnErrorNoop) {
    // detach without on_error should not crash when task throws — exception is silently dropped
    auto task = []() -> Task<int> {
        throw std::runtime_error("dropped");
        co_return 0;
    }();

    std::move(task).detach([](int) { FAIL(); });
    // reaching here without crashing is the pass condition
}
