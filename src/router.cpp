#include "router.hpp"
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <glog/logging.h>

namespace {

// Sends an HttpResult. Must be called on the I/O thread that owns req.
void send_result(evhttp_request* req, HttpResult result) {
    evkeyvalq* hdrs = evhttp_request_get_output_headers(req);
    evhttp_add_header(hdrs, "Content-Type", result.content_type.c_str());

    evbuffer* buf = evbuffer_new();
    evbuffer_add(buf, result.body.data(), result.body.size());
    evhttp_send_reply(req, result.code, nullptr, buf);
    evbuffer_free(buf);

    LOG(INFO) << "[io  ] async response sent  code=" << result.code
              << " bytes=" << result.body.size();
}

// Used when the pipeline's last step runs on a worker thread (pool.dispatch).
// Routes the send back to the owning I/O thread via event_base_once.
struct AsyncSend {
    evhttp_request* req;
    HttpResult      result;

    static void dispatch(evutil_socket_t, short, void* arg) {
        auto* d = static_cast<AsyncSend*>(arg);
        send_result(d->req, std::move(d->result));
        delete d;
    }
};

} // namespace

void Router::handle(evhttp_request* req, event_base* base) const {
    Request  request(req, base);
    Response response(req, base);

    const std::string method = request.method();
    const std::string path   = request.path();

    for (const auto& route : routes_) {
        if (route.method != method || route.path != path) continue;

        if (std::holds_alternative<SyncHandler>(route.handler)) {
            LOG(INFO) << "[io  ] " << method << " " << path << " → sync handler";
            std::get<SyncHandler>(route.handler)(request, response);

        } else {
            LOG(INFO) << "[io  ] " << method << " " << path << " → async pipeline";
            auto task = std::get<AsyncHandler>(route.handler)(request);

            std::move(task).detach([req, base](HttpResult result) {
                // Always route through event_base_once — safe from any thread
                // (I/O thread or worker). Fires on the next event loop tick.
                auto* data = new AsyncSend{req, std::move(result)};
                timeval tv{0, 0};
                event_base_once(base, -1, EV_TIMEOUT, AsyncSend::dispatch, data, &tv);
            });
        }
        return;
    }

    LOG(WARNING) << "[io  ] 404 " << method << " " << path;
    response.send(404, "Not Found\n");
}
