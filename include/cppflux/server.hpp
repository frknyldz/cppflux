#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Server  —  multi-threaded HTTP server
// ─────────────────────────────────────────────────────────────────────────────
//  Runs N I/O threads, each with its own libevent loop and SO_REUSEPORT socket,
//  so all threads accept connections in parallel with no shared lock.
//
//  Usage:
//
//    Server server(6);         // 6 I/O threads
//    server.routes(router);
//    server.listen(8080);      // blocks until SIGINT/SIGTERM
// ─────────────────────────────────────────────────────────────────────────────

#include <thread>
#include <vector>

#include "router.hpp"

// Opaque libevent handles — consumers never interact with these directly.
struct event_base;
struct evhttp;

class Server {
public:
    explicit Server(int n_io_threads);
    ~Server() = default;

    void routes(Router& r);
    void listen(int port);  // blocks until SIGINT/SIGTERM

private:
    struct IOThread {
        event_base* base{nullptr};
        evhttp*     http{nullptr};
        void*       ctx{nullptr};  // HandlerCtx* — defined in server.cpp
        std::thread thread;
    };

    void cleanup_io_threads();

    int                   n_io_;
    std::vector<IOThread> io_threads_;
    Router*               router_{nullptr};
};
