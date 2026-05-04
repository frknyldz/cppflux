# cppflux

[![Build](https://github.com/frknyldz/cppflux/actions/workflows/build.yml/badge.svg)](https://github.com/frknyldz/cppflux/actions/workflows/build.yml)

An exploration of [Spring WebFlux](https://docs.spring.io/spring-framework/reference/web/webflux.html)-style reactive HTTP in C++20, using libevent as the I/O backend and C++20 coroutines as the async primitive.

> **Experimental.** This is a proof-of-concept, not a production framework. It lacks path parameters, middleware, TLS, and many other features you would expect from a real HTTP library.

## The idea

WebFlux's reactive model — non-blocking I/O, bounded elastic scheduler, async pipelines — maps cleanly onto C++20 coroutines:

| Spring WebFlux | cppflux |
|---|---|
| `Mono<T>` | `Task<T>` |
| `.flatMap(f)` | `co_await f(...)` |
| `Schedulers.boundedElastic()` | `Schedulers::boundedElastic()` |
| `Mono.delay(duration)` | `co_await async_sleep(ms)` |
| `mono.subscribe(cb)` | `task.detach(cb)` |

## Usage

```cpp
#include "cppflux/cppflux.hpp"
using namespace std::chrono_literals;

// Async steps — each co_await suspends without holding a thread
Task<std::string> db_fetch(const std::string& key) {
    co_await async_sleep(10ms);
    co_return "db_result:" + key;
}

Task<std::string> service_call(const std::string& raw) {
    co_await async_sleep(10ms);
    co_return "enriched:" + raw;
}

int main() {
    Schedulers::init(24);   // bounded elastic worker pool

    Router router;

    // Sync handler — runs directly on an I/O thread
    router.get("/ping", [](Request req, Response res) {
        res.send(200, "pong\n");
    });

    // Async pipeline — suspends between steps, no thread held during waits
    router.get("/pipeline", [](Request req) -> HttpTask {
        auto raw      = co_await db_fetch("key");
        auto enriched = co_await service_call(raw);
        co_return HttpResult::ok("formatted:" + enriched + "\n");
    });

    // CPU-bound work offloaded to the worker pool
    router.post("/echo", [](Request req) -> HttpTask {
        auto body  = req.body();
        auto reply = co_await Schedulers::boundedElastic().dispatch([=] {
            return body.empty() ? std::string("(empty)\n") : body;
        });
        co_return HttpResult::ok(reply);
    });

    Server server(6);    // I/O threads
    server.routes(router);
    server.listen(8080);
}
```

## Architecture

```
                 ┌─────────────────────────────────────────┐
 incoming        │  I/O threads  (epoll + SO_REUSEPORT)    │
 requests ──────▶│  one libevent event_base per thread     │
                 └──────────────────┬──────────────────────┘
                                    │
                      co_await async_sleep(ms)
                        → timer callback, zero threads held
                                    │
                      co_await pool.dispatch(fn)
                        → fn runs on worker thread, resumes coroutine
                                    │
                 ┌──────────────────▼──────────────────────┐
                 │  Worker pool  (bounded elastic)          │
                 │  threads spawned on demand, up to max   │
                 └─────────────────────────────────────────┘
```

## Building

**System requirements:** C++20 compiler (GCC 12+ / Clang 14+), CMake 3.20+, Ninja, libevent.

```bash
# Ubuntu / Debian
sudo apt install cmake ninja-build libevent-dev

# Fedora / RHEL
sudo dnf install cmake ninja-build libevent-devel
```

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/examples/server
```

**Options:**

| CMake flag | Default | Description |
|---|---|---|
| `CPPFLUX_BUILD_EXAMPLES` | `ON` | Build the example server |
| `CPPFLUX_BUILD_TESTS` | `OFF` | Build the test suite |
| `BUILD_SHARED_LIBS` | `OFF` | Build as a shared library |
| `NATIVE_ARCH` | `ON` | Enable `-march=native` |

## Testing

```bash
cmake -B build -G Ninja -DCPPFLUX_BUILD_TESTS=ON
ninja -C build
ctest --test-dir build --output-on-failure -j$(nproc)
```

## Benchmarks

`wrk` — 100 connections, 4 threads, 10 seconds. Each server is an equivalent implementation of the same routes. Run with `cd bench && bash run.sh`.

**`/ping` — sync handler, no I/O**

| | Req/sec | p50 | p99 |
|---|---|---|---|
| Rust (Axum + Tokio) | 301k | 262µs | 801µs |
| **cppflux** | **260k** | 351µs | 1.30ms |
| Go (`net/http`) | 218k | 277µs | 2.61ms |
| Node.js (cluster) | 161k | 415µs | 3.76ms |

**`/pipeline` — 2× async sleeps (~20ms total latency)**

| | Req/sec | p50 | p99 |
|---|---|---|---|
| Node.js (cluster) | 4902 | 20.5ms | 21.3ms |
| **cppflux** | **4891** | 20.5ms | 21.4ms |
| Go (`net/http`) | 4655 | 21.5ms | 22.1ms |
| Rust (Axum + Tokio) | 4481 | 22.3ms | 23.6ms |

On throughput cppflux is close to Rust and ahead of Go. On the async pipeline, all four converge — the sleep dominates — but cppflux and Node edge ahead because libevent's timer callbacks have slightly less overhead than Tokio's sleep scheduler.

*Tested on AMD Ryzen 5 3600 (6c/12t), Fedora Linux.*

## Pluggable logging

By default cppflux logs to stderr. Replace it with any logger:

```cpp
#include "cppflux/logger.hpp"

// Silence all logs (useful in tests)
cppflux::set_logger([](cppflux::LogLevel, const std::string&) {});

// Plug in spdlog
cppflux::set_logger([](cppflux::LogLevel level, const std::string& msg) {
    if (level == cppflux::LogLevel::Error) spdlog::error(msg);
    else spdlog::info(msg);
});
```

## Repository layout

```
include/cppflux/       Public API headers (no libevent types exposed)
  cppflux.hpp            Single umbrella include
  task.hpp               Task<T> — lazy coroutine value
  timer.hpp              async_sleep()
  thread_pool.hpp        ThreadPool + PoolAwaitable
  schedulers.hpp         Schedulers::boundedElastic()
  router.hpp             Router, HttpResult, HttpTask
  request.hpp            Request
  response.hpp           Response
  logger.hpp             Pluggable logging API
  internal/
    task_impl.hpp        Coroutine protocol machinery (not part of public API)

lib/                   Implementation (libevent stays here)
  io_thread.hpp          thread_local event_base* — private to lib/

examples/              Usage example (opt-out: -DCPPFLUX_BUILD_EXAMPLES=OFF)
tests/                 GTest suite (opt-in: -DCPPFLUX_BUILD_TESTS=ON)
bench/                 Equivalent Go, Node.js, and Rust servers for comparison
```

## What's missing

This is a foundation, not a complete framework. Notable gaps compared to mature HTTP libraries:

- No path parameters (`/users/:id`)
- No middleware chain
- No query string parsing
- No TLS / HTTPS
- No WebSockets
- No graceful programmatic shutdown
- No `cmake --install` / `find_package` support

## License

MIT — see [LICENSE](LICENSE).
