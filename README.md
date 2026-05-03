# cppflux

> ⚠️ Experimental proof-of-concept — not production ready.

An exploration of implementing [Spring WebFlux](https://docs.spring.io/spring-framework/reference/web/webflux.html)-style reactive HTTP server patterns in C++20, using libevent as the I/O backend and C++20 coroutines as the async primitive.

## Concept

WebFlux's reactive model — non-blocking I/O, bounded elastic scheduler, async pipelines — maps surprisingly well onto C++20 coroutines:

| WebFlux (Java) | cppflux (C++) |
|---|---|
| `Mono<T>` | `Task<T>` |
| `.flatMap(f)` | `co_await f(...)` |
| `Schedulers.boundedElastic()` | `Schedulers::boundedElastic()` |
| `Mono.delay()` | `async_sleep(base, ms)` |
| `mono.subscribe()` | `task.detach(callback)` |

## Architecture

```
                 ┌─────────────────────────────────────┐
incoming         │  I/O threads (epoll, SO_REUSEPORT)  │
requests  ──────▶│  libevent event_base per thread     │
                 └────────────────┬────────────────────┘
                                  │ co_await async_sleep → timer, no thread held
                                  │ co_await Schedulers::boundedElastic() → worker thread
                 ┌────────────────▼────────────────────┐
                 │  Worker pool (bounded elastic)       │
                 │  Lazy-spawning, up to N threads      │
                 └─────────────────────────────────────┘
```

## Usage

```cpp
Schedulers::init(24);  // bounded elastic pool

Router router;

// Sync handler — runs on I/O thread
router.get("/ping", [](Request req, Response res) {
    res.send(200, "pong\n");
});

// Async pipeline — like Mono.flatMap chain
router.get("/data", [](Request req) -> Task<HttpResult> {
    auto raw      = co_await db_fetch(req.base(), req.path());
    auto enriched = co_await service_call(req.base(), raw);
    co_return HttpResult::ok(enriched);
});

Server server(6);  // I/O threads
server.routes(router);
server.listen(8080);
```

## Building

**Dependencies:** `libevent`, `libevent-devel`, `glog-devel`, `gflags-devel`, `ninja-build`, `mold`

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/server
```

## Benchmark

Tested on AMD Ryzen 5 3600 (6c/12t), compared against equivalent Go (`net/http`) and Node.js (`cluster`) servers:

| Route | cppflux | Go | Node |
|---|---|---|---|
| `/ping` (sync) | 235k req/s | 228k req/s | 169k req/s |
| `/pipeline` (2× async hops) | 22.6k req/s | 23.3k req/s | 24k req/s |

All three use true async I/O — no threads blocked during waits.

## Key files

| File | Description |
|---|---|
| `src/task.hpp` | `Task<T>` — lazy coroutine type, analogous to `Mono<T>` |
| `src/timer.hpp` | `async_sleep` — non-blocking coroutine timer via libevent |
| `src/thread_pool.hpp/cpp` | Lazy-spawning bounded thread pool |
| `src/schedulers.hpp/cpp` | `Schedulers::boundedElastic()` global named scheduler |
| `src/router.hpp/cpp` | Route registration, sync/async dispatch |
| `src/server.hpp/cpp` | Multi-threaded HTTP server (libevent + SO_REUSEPORT) |
| `bench/` | Equivalent Go and Node.js servers for comparison |
