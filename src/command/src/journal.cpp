// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/journal.hpp"

#include "kentos_cad/command/log.hpp"

#include <fstream>
#include <sstream>

namespace kentos::command {
namespace {

Origin origin_from_name(std::string_view s)
{
    if (s == "gui") return Origin::Gui;
    if (s == "cli") return Origin::CommandLine;
    if (s == "script") return Origin::Script;
    if (s == "ai") return Origin::Ai;
    if (s == "batch") return Origin::Batch;
    return Origin::Test;
}

} // namespace

core::Json JournalEntry::to_json(bool with_timestamp) const
{
    core::Json j;
    j.set("seq", core::Json::integer(static_cast<std::int64_t>(seq)));
    j.set("cmd", core::Json::string(command_id));
    j.set("args", args.to_json());
    j.set("origin", core::Json::string(origin_name(origin)));
    j.set("crs", core::Json::string(crs));
    j.set("katman", core::Json::string(layer));
    if (with_timestamp) j.set("ts", core::Json::integer(timestamp_ms));
    return j;
}

core::Result<JournalEntry> JournalEntry::from_json(const core::Json& j)
{
    using core::ErrorCode;

    const core::Json* cmd = j.find("cmd");
    if (!cmd || !cmd->is_string())
        return core::err(ErrorCode::ParseError,
                         "Günlük satırında metin türünde \"cmd\" alanı yok.");

    JournalEntry e;
    e.command_id = cmd->as_string();

    if (const core::Json* s = j.find("seq")) e.seq = static_cast<std::uint64_t>(s->as_int());
    if (const core::Json* o = j.find("origin")) e.origin = origin_from_name(o->as_string());
    if (const core::Json* c = j.find("crs")) e.crs = c->as_string();
    if (const core::Json* l = j.find("katman")) e.layer = l->as_string();
    if (const core::Json* t = j.find("ts")) e.timestamp_ms = t->as_int();

    if (const core::Json* a = j.find("args")) {
        auto parsed = Args::from_json(*a);
        if (!parsed) return parsed.error();
        e.args = std::move(parsed.value());
    }
    return e;
}

Journal::Journal() = default;

Journal::~Journal()
{
    close_sink();
}

void Journal::append(JournalEntry e)
{
    e.seq = next_seq_++;
    std::string line;

    {
        std::lock_guard lock(mtx_);
        if (sink_open_) {
            line = e.to_json(true).dump();
            line += '\n';
        }
    }

    entries_.push_back(std::move(e));

    if (!line.empty()) {
        std::lock_guard lock(mtx_);
        queue_.push_back(std::move(line));
        ++queued_;
        cv_.notify_one();
    }
}

void Journal::append_meta(const core::Json& record)
{
    // `kind` FIRST and written here, so the file always reads
    // `{"kind":"meta",...}` and no caller can spell it differently. Key order is
    // insertion order (`core::JsonObject`) and golden fixtures record the exact
    // bytes, so where this key lands is a decision rather than an accident.
    core::Json line;
    line.set("kind", core::Json::string("meta"));
    if (record.is_object())
        for (const auto& [key, value] : record.as_object())
            if (key != "kind") line.set(key, value);

    std::string text;
    {
        std::lock_guard lock(mtx_);
        if (sink_open_) {
            text = line.dump();
            text += '\n';
        }
    }

    metas_.push_back(std::move(line));

    if (!text.empty()) {
        std::lock_guard lock(mtx_);
        queue_.push_back(std::move(text));
        ++queued_;
        cv_.notify_one();
    }
}

void Journal::clear()
{
    entries_.clear();
    metas_.clear();
    next_seq_ = 1;
}

std::string Journal::canonical() const
{
    std::string out;
    for (const auto& e : entries_) {
        out += e.to_json(false).dump();
        out += '\n';
    }
    return out;
}

core::Status Journal::open_sink(const std::string& path)
{
    close_sink();

    std::ofstream probe(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!probe)
        return core::err(core::ErrorCode::IoFailure,
                         "Günlük dosyası yazmak için açılamadı: '" + path + "'");
    probe.close();

    {
        std::lock_guard lock(mtx_);
        sink_path_ = path;
        sink_open_ = true;
        stop_      = false;
        queued_    = 0;
        written_   = 0;
    }
    writer_ = std::thread(&Journal::writer_loop, this);
    return core::ok();
}

void Journal::close_sink()
{
    {
        std::lock_guard lock(mtx_);
        if (!sink_open_ && !writer_.joinable()) return;
        stop_ = true;
    }
    cv_.notify_all();
    if (writer_.joinable()) writer_.join();

    std::lock_guard lock(mtx_);
    sink_open_ = false;
    queue_.clear();
}

void Journal::flush()
{
    std::unique_lock lock(mtx_);
    if (!sink_open_) return;
    cv_.wait(lock, [this] { return queue_.empty(); });
}

void Journal::writer_loop()
{
    std::ofstream out;
    {
        std::lock_guard lock(mtx_);
        out.open(sink_path_, std::ios::out | std::ios::app | std::ios::binary);
    }
    if (!out) {
        log_error("journal: sink could not be opened for writing");
        return;
    }

    while (true) {
        std::deque<std::string> batch;
        {
            std::unique_lock lock(mtx_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (queue_.empty() && stop_) break;
            batch.swap(queue_);
        }

        for (const auto& line : batch)
            out << line;
        out.flush();

        {
            std::lock_guard lock(mtx_);
            written_ += batch.size();
        }
        cv_.notify_all();
    }
}

core::Result<std::vector<JournalEntry>> Journal::read_jsonl(const std::string& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
        return core::err(core::ErrorCode::IoFailure, "Günlük dosyası okunamadı: '" + path + "'");

    std::vector<JournalEntry> out;
    std::string line;
    std::size_t lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;
        if (line.empty()) continue;
        auto j = core::Json::parse(line);
        if (!j)
            return core::err(core::ErrorCode::ParseError,
                             "Günlük satırı " + std::to_string(lineno) + ": " + j.error().message);

        // The second line kind is not replayed. `.claude/command.md` R20: replay
        // applies `{cmd,...}` and IGNORES `{kind:"meta",...}` — a sandbox level or
        // a plugin consent is a record of what was permitted, not an edit to
        // reapply. Reading it as a command is how a journal with one meta line in
        // it stops being replayable at all.
        if (const core::Json* kind = j.value().find("kind");
            kind != nullptr && kind->is_string() && kind->as_string() == "meta")
            continue;

        auto e = JournalEntry::from_json(j.value());
        if (!e)
            return core::err(e.error().code,
                             "Günlük satırı " + std::to_string(lineno) + ": " + e.error().message);
        out.push_back(std::move(e.value()));
    }
    return out;
}

} // namespace kentos::command
