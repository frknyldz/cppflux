#pragma once
#include <event2/event.h>
#include <event2/http.h>
#include <thread>
#include <vector>
#include "router.hpp"

class Server {
public:
    explicit Server(int n_io_threads);
    ~Server() = default;

    void routes(Router& r);
    void listen(int port);  // blocks until SIGINT/SIGTERM

private:
    static evutil_socket_t make_socket(int port);
    static void on_request(evhttp_request* req, void* arg);
    static void on_signal(evutil_socket_t, short, void* arg);

    struct HandlerCtx {
        event_base* base;
        Router*     router;
    };

    struct IOThread {
        event_base*  base{nullptr};
        evhttp*      http{nullptr};
        HandlerCtx*  ctx{nullptr};
        std::thread  thread;
    };

    void cleanup_io_threads();

    int                   n_io_;
    std::vector<IOThread> io_threads_;
    Router*               router_{nullptr};
};
