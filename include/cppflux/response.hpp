#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Response  —  HTTP response for sync handlers
// ─────────────────────────────────────────────────────────────────────────────
//  Used only in sync (non-coroutine) handlers via res.send().
//  Async handlers return HttpResult instead — the router sends it for you.
//
//  Sync handler:
//
//    router.get("/ping", [](Request req, Response res) {
//        res.send(200, "pong\n");
//    });
//
//  Async handler (return HttpResult, not Response):
//
//    router.get("/hello", [](Request req) -> HttpTask {
//        co_return HttpResult::ok("hello\n");
//    });
// ─────────────────────────────────────────────────────────────────────────────

#include <string>

// Opaque libevent handles — consumers never interact with these directly.
struct evhttp_request;
struct event_base;

class Response {
public:
    // Called by the router — not intended for direct use.
    Response(evhttp_request* req, event_base* base);

    void send(int code, const std::string& body, const std::string& content_type = "text/plain");

private:
    evhttp_request* req_;
    event_base*     base_;
};
