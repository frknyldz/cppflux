#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Task<T>  —  lazy async value
// ─────────────────────────────────────────────────────────────────────────────
//  Analogous to JavaScript's Promise<T> or Reactor's Mono<T>.
//  Nothing executes until the task is co_awaited or handed to the router.
//
//  Defining an async step:
//
//    Task<std::string> fetch(const std::string& key) {
//        co_await async_sleep(10ms);       // suspend — no thread held
//        co_return "result:" + key;         // resolve with a value
//    }
//
//  Chaining steps into a sequential pipeline:
//
//    Task<std::string> pipeline() {
//        auto raw      = co_await fetch("id");     // wait for step 1
//        auto enriched = co_await transform(raw);  // wait for step 2
//        co_return enriched;
//    }
//
//  Offloading CPU-bound work to a thread pool:
//
//    Task<int> compute() {
//        auto n = co_await Schedulers::boundedElastic().dispatch([=] {
//            return heavy_work();      // runs on a worker thread
//        });
//        co_return n;
//    }
//
//  Coroutine protocol machinery is in internal/task_impl.hpp.
// ─────────────────────────────────────────────────────────────────────────────

#include "internal/task_impl.hpp"
