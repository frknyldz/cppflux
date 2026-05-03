#include <glog/logging.h>
#include "router.hpp"
#include "schedulers.hpp"
#include "server.hpp"
#include "task.hpp"
#include "timer.hpp"

// Each step suspends the coroutine for the I/O wait (no thread held),
// then does instant CPU work — like a real async DB/RPC client would.
// Analogous to WebFlux's Mono.delay() + map(), not fromCallable(blocking).

Task<std::string> db_fetch(event_base* base, const std::string& key) {
    co_await async_sleep(base, std::chrono::milliseconds(10));  // simulate async DB round-trip
    co_return "db_result:" + key;
}

Task<std::string> service_call(event_base* base, const std::string& raw) {
    co_await async_sleep(base, std::chrono::milliseconds(10));  // simulate async RPC
    co_return "enriched:" + raw;
}

Task<std::string> format_response(const std::string& enriched) {
    co_return "formatted:" + enriched + "\n";  // CPU only, no wait
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr      = true;
    FLAGS_colorlogtostderr = true;
    FLAGS_minloglevel      = 2;

    Schedulers::init(24);

    Router router;

    router.get("/ping", [](Request req, Response res) {
        res.send(200, "pong\n");
    });

    // True async pipeline — zero threads held during I/O waits.
    // 500 concurrent requests all sleep simultaneously on the event loop timer,
    // no worker thread consumed — same model as Go goroutines / Node await.
    router.get("/pipeline", [](Request req) -> Task<HttpResult> {
        auto* base = req.base();
        auto raw      = co_await db_fetch(base, "pipeline");
        auto enriched = co_await service_call(base, raw);
        auto response = co_await format_response(enriched);
        co_return HttpResult::ok(response);
    });

    router.post("/echo", [](Request req) -> Task<HttpResult> {
        auto body = req.body();
        auto reply = co_await Schedulers::boundedElastic().dispatch([=] {
            return body.empty() ? std::string("(empty)\n") : body;
        });
        co_return HttpResult::ok(reply);
    });

    Server server(6);
    server.routes(router);
    server.listen(8080);
    return 0;
}
