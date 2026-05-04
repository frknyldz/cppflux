#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Logging  —  pluggable log callback
// ─────────────────────────────────────────────────────────────────────────────
//  cppflux logs internally (connection lifecycle, routing decisions, errors).
//  By default messages go to stderr. Provide your own callback to route them
//  into your application's logging framework (spdlog, absl, glog, etc.).
//
//  Call set_logger() before Server::listen(). Do not change it while the
//  server is running — the callback is read from multiple threads.
//
//  Example — routing into spdlog:
//
//    cppflux::set_logger([](cppflux::LogLevel level, const std::string& msg) {
//        if      (level == cppflux::LogLevel::Warning) spdlog::warn(msg);
//        else if (level == cppflux::LogLevel::Error)   spdlog::error(msg);
//        else                                           spdlog::info(msg);
//    });
// ─────────────────────────────────────────────────────────────────────────────

#include <functional>
#include <string>

namespace cppflux {

enum class LogLevel { Info, Warning, Error };

using LogCallback = std::function<void(LogLevel, const std::string&)>;

// Replace the default stderr logger. Pass nullptr to restore the default.
void set_logger(LogCallback fn);

}  // namespace cppflux
