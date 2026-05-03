#pragma once
#include "thread_pool.hpp"

// Mirrors WebFlux's Schedulers — call init() once at startup, then use
// Schedulers::boundedElastic() anywhere without passing a pool around.
namespace Schedulers {
    void        init(int bounded_elastic_threads);
    ThreadPool& boundedElastic();
}
