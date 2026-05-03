#pragma once
#include <event2/event.h>
#include <event2/http.h>
#include <string>

class Response {
public:
    Response(evhttp_request* req, event_base* base);

    void send(int code, const std::string& body,
              const std::string& content_type = "text/plain");

private:
    struct SendTask {
        evhttp_request* req;
        int             code;
        std::string     body;
        std::string     content_type;
        static void dispatch(evutil_socket_t, short, void* arg);
    };

    evhttp_request* req_;
    event_base*     base_;
};
