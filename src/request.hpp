#pragma once
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <string>

class Request {
public:
    Request(evhttp_request* req, event_base* base) : req_(req), base_(base) {}

    event_base* base() const { return base_; }

    std::string method() const {
        switch (evhttp_request_get_command(req_)) {
            case EVHTTP_REQ_GET:    return "GET";
            case EVHTTP_REQ_POST:   return "POST";
            case EVHTTP_REQ_PUT:    return "PUT";
            case EVHTTP_REQ_DELETE: return "DELETE";
            default:                return "UNKNOWN";
        }
    }

    std::string path() const {
        const evhttp_uri* u = evhttp_request_get_evhttp_uri(req_);
        const char* p = u ? evhttp_uri_get_path(u) : nullptr;
        return p ? p : "/";
    }

    std::string body() const {
        evbuffer* buf = evhttp_request_get_input_buffer(req_);
        size_t len = evbuffer_get_length(buf);
        std::string out(len, '\0');
        evbuffer_copyout(buf, out.data(), len);
        return out;
    }

    std::string header(const std::string& name) const {
        evkeyvalq* h = evhttp_request_get_input_headers(req_);
        const char* v = evhttp_find_header(h, name.c_str());
        return v ? v : "";
    }

private:
    evhttp_request* req_;
    event_base*     base_;
};
