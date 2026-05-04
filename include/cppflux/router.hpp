#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Router  —  HTTP route registry
// ─────────────────────────────────────────────────────────────────────────────
//  Register GET/POST handlers, then pass the router to Server::routes().
//  Handlers can be synchronous (Request + Response) or async (Task<HttpResult>).
//
//  Sync handler — for trivial responses with no I/O:
//
//    router.get("/ping", [](Request req, Response res) {
//        res.send(200, "pong\n");
//    });
//
//  Async handler — for I/O pipelines; the router sends the result for you:
//
//    router.get("/hello", [](Request req) -> HttpTask {
//        auto data = co_await fetch(req.path());
//        co_return HttpResult::ok(data, "application/json");
//    });
// ─────────────────────────────────────────────────────────────────────────────

#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "request.hpp"
#include "response.hpp"
#include "task.hpp"

// Plain data transfer object returned by async handlers.
// Safe to construct on any thread; no libevent state.
struct HttpResult {
    int         code = 200;
    std::string body;
    std::string content_type = "text/plain";

    static HttpResult ok(std::string b, std::string ct = "text/plain") {
        return {200, std::move(b), std::move(ct)};
    }
    static HttpResult err(int code, std::string b) { return {code, std::move(b)}; }
};

// Shorthand return type for async handlers.
using HttpTask = Task<HttpResult>;

// Opaque libevent handles used only in the internal dispatch path.
struct evhttp_request;
struct event_base;

class Router {
public:
    using SyncHandler  = std::function<void(Request, Response)>;
    using AsyncHandler = std::function<Task<HttpResult>(Request)>;

    template <typename F>
    void get(std::string path, F&& h) {
        add("GET", std::move(path), std::forward<F>(h));
    }

    template <typename F>
    void post(std::string path, F&& h) {
        add("POST", std::move(path), std::forward<F>(h));
    }

    // Called by Server — not part of the user-facing API.
    void handle(evhttp_request* req, event_base* base) const;

private:
    template <typename F>
    void add(std::string method, std::string path, F&& h) {
        using Clean = std::decay_t<F>;
        if constexpr (std::is_invocable_r_v<Task<HttpResult>, Clean, Request>) {
            routes_.push_back(
                {std::move(method), std::move(path), AsyncHandler(std::forward<F>(h))});
        } else {
            routes_.push_back(
                {std::move(method), std::move(path), SyncHandler(std::forward<F>(h))});
        }
    }

    struct Route {
        std::string                             method;
        std::string                             path;
        std::variant<SyncHandler, AsyncHandler> handler;
    };
    std::vector<Route> routes_;
};
