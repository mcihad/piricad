// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: minimal severity-tagged logging sink.
// spdlog replaces this behind PIRICAD_WITH_SPDLOG (piricad.md §9.1).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace piricad::core {

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

} // namespace piricad::core
