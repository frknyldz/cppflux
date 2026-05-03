#include "schedulers.hpp"
#include <memory>
#include <stdexcept>

namespace Schedulers {

static std::unique_ptr<ThreadPool> g_bounded_elastic;

void init(int n) {
    g_bounded_elastic = std::make_unique<ThreadPool>(n);
}

ThreadPool& boundedElastic() {
    if (!g_bounded_elastic)
        throw std::runtime_error("Schedulers::init() not called");
    return *g_bounded_elastic;
}

} // namespace Schedulers
