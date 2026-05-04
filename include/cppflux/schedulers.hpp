#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Schedulers  —  named thread pools, mirroring Spring's Schedulers API
// ─────────────────────────────────────────────────────────────────────────────
//  Call init() once at startup, then access the pool anywhere without passing
//  it as an argument — the same pattern as Spring's Schedulers.boundedElastic().
//
//  Setup:
//
//    Schedulers::init(24);   // max 24 worker threads, created on demand
//
//  Usage in a handler:
//
//    auto result = co_await Schedulers::boundedElastic().dispatch([=] {
//        return expensive_db_query();   // runs on a worker thread
//    });
// ─────────────────────────────────────────────────────────────────────────────

#include "thread_pool.hpp"

namespace Schedulers {
void        init(int bounded_elastic_threads);
ThreadPool& boundedElastic();
}  // namespace Schedulers
