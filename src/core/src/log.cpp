// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/log.hpp"

#include <cstdio>
#include <mutex>

namespace piricad::core {
namespace {

std::mutex& sink_mutex()
{
    static std::mutex m;
    return m;
}

LogSink& sink()
{
    static LogSink s = [](LogLevel lvl, std::string_view msg) {
        static const char* kTag[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR"};
        std::fprintf(lvl >= LogLevel::Warn ? stderr : stdout, "[piricad][%s] %.*s\n",
                     kTag[static_cast<int>(lvl)], static_cast<int>(msg.size()), msg.data());
    };
    return s;
}

LogLevel& threshold()
{
    static LogLevel l = LogLevel::Info;
    return l;
}

} // namespace

void set_log_sink(LogSink s)
{
    std::lock_guard lock(sink_mutex());
    sink() = s ? std::move(s) : LogSink{};
}

void set_log_level(LogLevel level)
{
    std::lock_guard lock(sink_mutex());
    threshold() = level;
}

void log_message(LogLevel level, std::string_view message)
{
    std::lock_guard lock(sink_mutex());
    if (level < threshold() || !sink()) return;
    sink()(level, message);
}

} // namespace piricad::core
