#include "cppflux/response.hpp"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

namespace {

// Internal send callback — not exposed in response.hpp.
struct SendTask {
    evhttp_request* req;
    int             code;
    std::string     body;
    std::string     content_type;

    static void dispatch(evutil_socket_t, short, void* arg) {
        auto* t = static_cast<SendTask*>(arg);

        evkeyvalq* hdrs = evhttp_request_get_output_headers(t->req);
        evhttp_add_header(hdrs, "Content-Type", t->content_type.c_str());

        evbuffer* buf = evbuffer_new();
        evbuffer_add(buf, t->body.data(), t->body.size());
        evhttp_send_reply(t->req, t->code, nullptr, buf);
        evbuffer_free(buf);

        delete t;
    }
};

}  // namespace

Response::Response(evhttp_request* req, event_base* base) : req_(req), base_(base) {}

void Response::send(int code, const std::string& body, const std::string& content_type) {
    auto*   task = new SendTask{req_, code, body, content_type};
    timeval tv{0, 0};
    event_base_once(base_, -1, EV_TIMEOUT, SendTask::dispatch, task, &tv);
}
