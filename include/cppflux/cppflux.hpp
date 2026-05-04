#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  cppflux  —  WebFlux-style reactive HTTP in C++20
// ─────────────────────────────────────────────────────────────────────────────
//  Single include for all library types and functions.
//
//  Quickstart:
//
//    #include "cppflux/cppflux.hpp"
//    using namespace std::chrono_literals;
//
//    Task<std::string> fetch_data(const std::string& key) {
//        co_await async_sleep(10ms);
//        co_return "data:" + key;
//    }
//
//    int main() {
//        Schedulers::init(16);
//
//        Router router;
//        router.get("/hello", [](Request req) -> HttpTask {
//            auto data = co_await fetch_data(req.path());
//            co_return HttpResult::ok(data);
//        });
//
//        Server server(4);
//        server.routes(router);
//        server.listen(8080);
//    }
// ─────────────────────────────────────────────────────────────────────────────

#include "logger.hpp"
#include "request.hpp"
#include "response.hpp"
#include "router.hpp"
#include "schedulers.hpp"
#include "server.hpp"
#include "task.hpp"
#include "thread_pool.hpp"
#include "timer.hpp"
