#include "cppflux/request.hpp"

#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

Request::Request(evhttp_request* req, event_base* base) : req_(req), base_(base) {}

std::string Request::method() const {
    switch (evhttp_request_get_command(req_)) {
        case EVHTTP_REQ_GET:
            return "GET";
        case EVHTTP_REQ_POST:
            return "POST";
        case EVHTTP_REQ_PUT:
            return "PUT";
        case EVHTTP_REQ_DELETE:
            return "DELETE";
        default:
            return "UNKNOWN";
    }
}

std::string Request::path() const {
    const evhttp_uri* uri = evhttp_request_get_evhttp_uri(req_);
    const char*       p   = uri ? evhttp_uri_get_path(uri) : nullptr;
    return p ? p : "/";
}

std::string Request::body() const {
    evbuffer*   buf = evhttp_request_get_input_buffer(req_);
    size_t      len = evbuffer_get_length(buf);
    std::string out(len, '\0');
    evbuffer_copyout(buf, out.data(), len);
    return out;
}

std::string Request::header(const std::string& name) const {
    evkeyvalq*  hdrs = evhttp_request_get_input_headers(req_);
    const char* val  = evhttp_find_header(hdrs, name.c_str());
    return val ? val : "";
}
