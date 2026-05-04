#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Request  —  incoming HTTP request, passed to every handler
// ─────────────────────────────────────────────────────────────────────────────
//  Read-only access to the request method, path, body, and headers.
//  Constructed by the router; never instantiated directly in user code.
//
//  Usage:
//
//    router.post("/items", [](Request req) -> HttpTask {
//        auto body = req.body();
//        auto auth = req.header("Authorization");
//        ...
//    });
// ─────────────────────────────────────────────────────────────────────────────

#include <string>

// Opaque libevent handles — consumers never interact with these directly.
struct evhttp_request;
struct event_base;

class Request {
public:
    // Called by the router — not intended for direct use.
    Request(evhttp_request* req, event_base* base);

    std::string method() const;
    std::string path() const;
    std::string body() const;
    std::string header(const std::string& name) const;

private:
    evhttp_request* req_;
    event_base*     base_;
};
