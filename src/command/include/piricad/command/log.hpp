// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: severity-tagged logging, backed by spdlog.
//
// It lives HERE and not in /src/core because core.md P9 bans a logging sink in
// core outright: a process-wide mutable sink is exactly the global state that
// makes two documents open at once able to disagree, and core is the layer that
// may not have any. CLAUDE.md Article 8.4 recorded that as a deviation with one
// removal condition — "logging moves to spdlog outside /src/core" — and this is
// that move.
//
// /src/command is the right home rather than a new module: every layer above core
// (io, script, domain, ai, app) already depends on it, and core no longer needs
// logging at all — its one caller now records registration failures where a test
// can see them instead.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace piricad::command {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error };

using LogSink = std::function<void(LogLevel, std::string_view)>;

void set_log_sink(LogSink sink);
void set_log_level(LogLevel level);
void log_message(LogLevel level, std::string_view message);

inline void log_info(std::string_view m)
{
    log_message(LogLevel::Info, m);
}

inline void log_warn(std::string_view m)
{
    log_message(LogLevel::Warn, m);
}

inline void log_error(std::string_view m)
{
    log_message(LogLevel::Error, m);
}

inline void log_debug(std::string_view m)
{
    log_message(LogLevel::Debug, m);
}

} // namespace piricad::command
