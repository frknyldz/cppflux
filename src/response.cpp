#include "response.hpp"
#include "schedulers.hpp"
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

Response::Response(evhttp_request* req, event_base* base)
    : req_(req), base_(base) {}

void Response::send(int code, const std::string& body,
                    const std::string& content_type) {
    auto* task = new SendTask{req_, code, body, content_type};
    timeval tv{0, 0};
    event_base_once(base_, -1, EV_TIMEOUT, SendTask::dispatch, task, &tv);
}

void Response::subscribeOn(std::function<void(Response)> work) {
    Schedulers::boundedElastic().submit([r = *this, work = std::move(work)]() mutable {
        work(r);
    });
}

void Response::SendTask::dispatch(evutil_socket_t, short, void* arg) {
    auto* t = static_cast<SendTask*>(arg);

    evkeyvalq* hdrs = evhttp_request_get_output_headers(t->req);
    evhttp_add_header(hdrs, "Content-Type", t->content_type.c_str());

    evbuffer* buf = evbuffer_new();
    evbuffer_add(buf, t->body.data(), t->body.size());
    evhttp_send_reply(t->req, t->code, nullptr, buf);
    evbuffer_free(buf);

    delete t;
}
