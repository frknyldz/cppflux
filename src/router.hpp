#pragma once
#include <event2/event.h>
#include <event2/http.h>
#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include "request.hpp"
#include "response.hpp"
#include "task.hpp"

// Value type returned by async (coroutine) handlers.
// Unlike Response (which holds evhttp state), this is a plain DTO
// safe to construct on any thread.
struct HttpResult {
    int         code         = 200;
    std::string body;
    std::string content_type = "text/plain";

    static HttpResult ok(std::string b, std::string ct = "text/plain") {
        return {200, std::move(b), std::move(ct)};
    }
    static HttpResult err(int code, std::string b) {
        return {code, std::move(b)};
    }
};

class Router {
public:
    using SyncHandler  = std::function<void(Request, Response)>;
    using AsyncHandler = std::function<Task<HttpResult>(Request)>;

    // Unified get/post — auto-detects sync vs async from return type.
    // Sync:  [](Request req, Response res) { res.send(200, "..."); }
    // Async: [](Request req) -> Task<HttpResult> { co_return HttpResult::ok("..."); }
    template<typename F>
    void get(std::string path, F&& h) { add("GET",  std::move(path), std::forward<F>(h)); }

    template<typename F>
    void post(std::string path, F&& h) { add("POST", std::move(path), std::forward<F>(h)); }

    void handle(evhttp_request* req, event_base* base) const;

private:
    template<typename F>
    void add(std::string method, std::string path, F&& h) {
        using Clean = std::decay_t<F>;
        if constexpr (std::is_invocable_r_v<Task<HttpResult>, Clean, Request>) {
            routes_.push_back({std::move(method), std::move(path),
                               AsyncHandler(std::forward<F>(h))});
        } else {
            routes_.push_back({std::move(method), std::move(path),
                               SyncHandler(std::forward<F>(h))});
        }
    }

    struct Route {
        std::string                      method;
        std::string                      path;
        std::variant<SyncHandler,
                     AsyncHandler>       handler;
    };
    std::vector<Route> routes_;
};
