#include "cppflux/cppflux.hpp"

using namespace std::chrono_literals;

// ── Async step functions ──────────────────────────────────────────────────────
//
// These look like ordinary functions. Under the hood, each co_await suspends the
// coroutine on a timer without holding any thread — the same model as Node's
// `await` or a Go goroutine sleeping.

Task<std::string> db_fetch(const std::string& key) {
    co_await async_sleep(10ms);
    co_return "db_result:" + key;
}

Task<std::string> service_call(const std::string& raw) {
    co_await async_sleep(10ms);
    co_return "enriched:" + raw;
}

Task<std::string> format_response(const std::string& enriched) {
    co_return "formatted:" + enriched + "\n";
}

// ── Server setup ──────────────────────────────────────────────────────────────

int main() {
    if (std::getenv("CPPFLUX_QUIET")) {
        cppflux::set_logger([](cppflux::LogLevel, const std::string&) {});
    }

    Schedulers::init(24);

    Router router;

    router.get("/ping", [](Request req, Response res) { res.send(200, "pong\n"); });

    router.get("/pipeline", [](Request req) -> HttpTask {
        auto raw      = co_await db_fetch("pipeline");
        auto enriched = co_await service_call(raw);
        auto body     = co_await format_response(enriched);
        co_return HttpResult::ok(body);
    });

    router.post("/echo", [](Request req) -> HttpTask {
        auto body  = req.body();
        auto reply = co_await Schedulers::boundedElastic().dispatch(
            [=] { return body.empty() ? std::string("(empty)\n") : body; });
        co_return HttpResult::ok(reply);
    });

    Server server(6);
    server.routes(router);
    server.listen(8080);
    return 0;
}
