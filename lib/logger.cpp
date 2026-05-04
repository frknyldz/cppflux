#include "cppflux/logger.hpp"

#include <iostream>

namespace cppflux {

namespace {

void default_logger(LogLevel level, const std::string& msg) {
    const char* prefix = (level == LogLevel::Warning) ? "W cppflux: "
                         : (level == LogLevel::Error) ? "E cppflux: "
                                                      : "I cppflux: ";
    std::cerr << prefix << msg << "\n";
}

LogCallback g_logger = default_logger;

}  // namespace

void set_logger(LogCallback fn) {
    g_logger = fn ? std::move(fn) : default_logger;
}

namespace detail {

void log(LogLevel level, const std::string& msg) {
    g_logger(level, msg);
}

}  // namespace detail

}  // namespace cppflux
