// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/log.hpp"

#include <mutex>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace kentos::command {
namespace {

spdlog::level::level_enum to_spdlog(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return spdlog::level::trace;
    case LogLevel::Debug: return spdlog::level::debug;
    case LogLevel::Info: return spdlog::level::info;
    case LogLevel::Warn: return spdlog::level::warn;
    case LogLevel::Error: return spdlog::level::err;
    }
    return spdlog::level::info;
}

std::mutex& sink_mutex()
{
    static std::mutex m;
    return m;
}

/// The application's own sink, when it has installed one.
///
/// The shell needs log lines in its transcript widget, not on a console nobody is
/// looking at, so an installed sink REPLACES spdlog rather than adding to it. A
/// line written to both would appear twice in the one place a user reads.
LogSink& override_sink()
{
    static LogSink s;
    return s;
}

/// Built once, on first use. Not at static-init time: spdlog's registry is itself
/// a static, and two statics in different translation units have no order.
std::shared_ptr<spdlog::logger>& logger()
{
    static std::shared_ptr<spdlog::logger> log = [] {
        auto made = spdlog::stderr_color_mt("piricad");
        made->set_pattern("[kentos][%^%l%$] %v");
        made->set_level(spdlog::level::info);
        return made;
    }();
    return log;
}

} // namespace

void set_log_sink(LogSink s)
{
    const std::lock_guard lock(sink_mutex());
    override_sink() = std::move(s);
}

void set_log_level(LogLevel level)
{
    const std::lock_guard lock(sink_mutex());
    logger()->set_level(to_spdlog(level));
}

void log_message(LogLevel level, std::string_view message)
{
    const std::lock_guard lock(sink_mutex());

    if (override_sink()) {
        override_sink()(level, message);
        return;
    }

    // `{}` and the message as an argument, never the message as the format
    // string: a log line carries user text — a layer name, a file path, an error
    // from a catalogue — and any of those may contain a brace. Formatting it as a
    // pattern would throw on `KATMAN {ada}` and lose the line that was trying to
    // explain what went wrong.
    logger()->log(to_spdlog(level), "{}", message);
}

} // namespace kentos::command
