#include "cppflux/router.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

#include "log.hpp"

namespace {

void send_result(evhttp_request* req, HttpResult result) {
    evkeyvalq* hdrs = evhttp_request_get_output_headers(req);
    evhttp_add_header(hdrs, "Content-Type", result.content_type.c_str());

    evbuffer* buf = evbuffer_new();
    evbuffer_add(buf, result.body.data(), result.body.size());
    evhttp_send_reply(req, result.code, nullptr, buf);
    evbuffer_free(buf);

    CPPFLUX_LOG(Info) << "response  code=" << result.code << " bytes=" << result.body.size();
}

struct AsyncSend {
    evhttp_request* req;
    HttpResult      result;

    static void dispatch(evutil_socket_t, short, void* arg) {
        auto* d = static_cast<AsyncSend*>(arg);
        send_result(d->req, std::move(d->result));
        delete d;
    }
};

}  // namespace

void Router::handle(evhttp_request* req, event_base* base) const {
    Request  request(req, base);
    Response response(req, base);

    const std::string method = request.method();
    const std::string path   = request.path();

    for (const auto& route : routes_) {
        if (route.method != method || route.path != path) {
            continue;
        }

        if (std::holds_alternative<SyncHandler>(route.handler)) {
            CPPFLUX_LOG(Info) << method << " " << path << " sync";
            std::get<SyncHandler>(route.handler)(request, response);
        } else {
            CPPFLUX_LOG(Info) << method << " " << path << " async";
            auto task = std::get<AsyncHandler>(route.handler)(request);
            std::move(task).detach(
                [req, base](HttpResult result) {
                    auto*   data = new AsyncSend{req, std::move(result)};
                    timeval tv{0, 0};
                    event_base_once(base, -1, EV_TIMEOUT, AsyncSend::dispatch, data, &tv);
                },
                [req, base](const std::exception_ptr&) {
                    CPPFLUX_LOG(Error) << "unhandled exception in async handler — sending 500";
                    auto*   data = new AsyncSend{req, HttpResult::err(500, "Internal Server Error\n")};
                    timeval tv{0, 0};
                    event_base_once(base, -1, EV_TIMEOUT, AsyncSend::dispatch, data, &tv);
                });
        }
        return;
    }

    CPPFLUX_LOG(Warning) << "404 " << method << " " << path;
    response.send(404, "Not Found\n");
}
