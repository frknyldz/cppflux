#pragma once
// Private header — not installed, not visible to library consumers.
// Provides CPPFLUX_LOG(Level) << ... stream-style logging internally,
// routing through whatever callback set_logger() installed.

#include <sstream>
#include "cppflux/logger.hpp"

namespace cppflux::detail {

void log(LogLevel level, const std::string& msg);

// RAII stream: accumulates the message, emits it on destruction.
// Same usage pattern as glog's LOG(INFO) << ..., no glog dependency.
struct LogStream {
    explicit LogStream(LogLevel l) : level_(l) {}
    LogStream(const LogStream&)            = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream() { log(level_, buf_.str()); }

    template <typename T>
    LogStream& operator<<(const T& v) { buf_ << v; return *this; }

private:
    LogLevel           level_;
    std::ostringstream buf_;
};

}  // namespace cppflux::detail

#define CPPFLUX_LOG(level) \
    cppflux::detail::LogStream{cppflux::LogLevel::level}
