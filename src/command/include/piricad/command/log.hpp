// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: severity-tagged logging, backed by spdlog.
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

namespace kentos::command {

/// Severity, in the order spdlog uses.
enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error };

/// A destination for log lines. The shell installs one so that lines land in the
/// transcript widget the user is actually reading.
using LogSink = std::function<void(LogLevel, std::string_view)>;

/// Installs a sink, REPLACING spdlog rather than adding to it. A line written to
/// both would appear twice in the one place a user looks. Pass an empty sink to
/// go back to spdlog's own output.
void set_log_sink(LogSink sink);

/// Sets the threshold below which messages are dropped.
void set_log_level(LogLevel level);

/// Logs one message. The message is passed as an ARGUMENT and never as a format
/// string: a log line carries user text — a layer name, a file path — and any of
/// those may contain a brace.
void log_message(LogLevel level, std::string_view message);

/// Shorthands, one per level. Inline so a dropped message costs a comparison and
/// no call.
inline void log_info(std::string_view m)
{
    log_message(LogLevel::Info, m);
}

/// See `log_info`.
inline void log_warn(std::string_view m)
{
    log_message(LogLevel::Warn, m);
}

/// See `log_info`.
inline void log_error(std::string_view m)
{
    log_message(LogLevel::Error, m);
}

/// See `log_info`.
inline void log_debug(std::string_view m)
{
    log_message(LogLevel::Debug, m);
}

} // namespace kentos::command
