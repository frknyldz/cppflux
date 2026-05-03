#include "server.hpp"
#include "schedulers.hpp"
#include <event2/thread.h>
#include <arpa/inet.h>
#include <csignal>
#include <stdexcept>
#include <sys/socket.h>
#include <glog/logging.h>

Server::Server(int n_io) : n_io_(n_io) {}

void Server::routes(Router& r) {
    router_ = &r;
}

void Server::listen(int port) {
    if (!router_) throw std::runtime_error("call routes() before listen()");

    evthread_use_pthreads();

    io_threads_.resize(n_io_);
    try {
        for (int i = 0; i < n_io_; ++i) {
            auto& t  = io_threads_[i];
            t.base   = event_base_new();
            t.http   = evhttp_new(t.base);
            t.ctx    = new HandlerCtx{t.base, router_};

            evhttp_set_gencb(t.http, on_request, t.ctx);

            evutil_socket_t fd = make_socket(port);
            if (evhttp_accept_socket(t.http, fd) != 0) {
                evutil_closesocket(fd);
                throw std::runtime_error("evhttp_accept_socket failed");
            }

            t.thread = std::thread([base = t.base, i] {
                LOG(INFO) << "[io " << i << "] started"
                          << " thread=" << std::this_thread::get_id()
                          << " backend=" << event_base_get_method(base);
                event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);
                LOG(INFO) << "[io " << i << "] stopped";
            });
        }
    } catch (...) {
        cleanup_io_threads();
        throw;
    }

    LOG(INFO) << "listening :" << port
              << "  io_threads=" << n_io_
              << "  workers_max=" << Schedulers::boundedElastic().max_size();

    event_base* sig_base = event_base_new();
    event* sig_int  = evsignal_new(sig_base, SIGINT,  on_signal, sig_base);
    event* sig_term = evsignal_new(sig_base, SIGTERM, on_signal, sig_base);
    evsignal_add(sig_int,  nullptr);
    evsignal_add(sig_term, nullptr);

    event_base_dispatch(sig_base);

    LOG(INFO) << "[main] shutting down...";

    event_free(sig_int);
    event_free(sig_term);
    event_base_free(sig_base);

    cleanup_io_threads();
}

void Server::cleanup_io_threads() {
    for (auto& t : io_threads_)
        if (t.base) event_base_loopbreak(t.base);

    for (auto& t : io_threads_)
        if (t.thread.joinable()) t.thread.join();

    for (auto& t : io_threads_) {
        delete t.ctx;   t.ctx  = nullptr;
        if (t.http) { evhttp_free(t.http); t.http = nullptr; }
        if (t.base) { event_base_free(t.base); t.base = nullptr; }
    }
}

evutil_socket_t Server::make_socket(int port) {
    evutil_socket_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");

    evutil_make_socket_nonblocking(fd);
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    sockaddr_in sin{};
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port        = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) < 0) {
        evutil_closesocket(fd);
        throw std::runtime_error("bind() failed on port " + std::to_string(port));
    }
    if (::listen(fd, 128) < 0) {
        evutil_closesocket(fd);
        throw std::runtime_error("listen() failed");
    }
    return fd;
}

void Server::on_request(evhttp_request* req, void* arg) {
    auto* ctx = static_cast<HandlerCtx*>(arg);
    ctx->router->handle(req, ctx->base);
}

void Server::on_signal(evutil_socket_t, short, void* arg) {
    event_base_loopbreak(static_cast<event_base*>(arg));
}
