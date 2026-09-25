// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/script/json_runner.hpp"

#include "kentos_cad/core/json.hpp"

#include <fstream>
#include <sstream>

namespace kentos::script {

namespace {

/// `said` ending in a full stop, so a sentence can follow it.
std::string sentence(std::string said)
{
    if (!said.empty() && said.back() != '.') said += '.';
    return said;
}

} // namespace

namespace {

using core::ErrorCode;

} // namespace

JsonRunner::JsonRunner(command::Bus& bus, Sandbox sandbox) : bus_(bus), sandbox_(sandbox) {}

namespace {

/// A script read and checked, not yet run: its name and its steps.
struct ReadScript
{
    std::string label;
    std::vector<command::Invocation> steps;
};

/// Reads `json` into its steps. EVERY LINE IS READ BEFORE ANY RUNS (TODOS F-05):
/// a malformed 251st item used to close the batch over the 250 before it —
/// committed, one undo step, and an error saying the script had failed. A
/// script whose text is broken is refused as a whole, by line number, with the
/// drawing untouched. The same reading serves a run and a preview.
core::Result<ReadScript> read_script(std::string_view json, std::string label)
{
    auto parsed = core::Json::parse(json);
    if (!parsed) return parsed.error();

    const core::Json& doc  = parsed.value();
    const core::Json* list = nullptr;

    if (doc.is_array()) {
        list = &doc;
    } else if (doc.is_object()) {
        if (const core::Json* n = doc.find("ad"); n && n->is_string() && !n->as_string().empty())
            label = n->as_string();
        list = doc.find("komutlar");
        if (list == nullptr) list = doc.find("commands");
    }

    if (list == nullptr || !list->is_array())
        return core::err(
            ErrorCode::ParseError,
            "Betik ya bir komut dizisi ya da \"komutlar\" alanı olan bir nesne olmalı");

    ReadScript out;
    out.label = std::move(label);
    out.steps.reserve(list->as_array().size());
    for (const core::Json& item : list->as_array()) {
        const std::string at = "Betik satırı " + std::to_string(out.steps.size() + 1);
        if (!item.is_object())
            return core::err(ErrorCode::ParseError,
                             at + " bir nesne olmalı: " + item.dump() + ". Betik çalıştırılmadı.");

        const core::Json* cmd = item.find("cmd");
        if (cmd == nullptr) cmd = item.find("komut");
        if (cmd == nullptr || !cmd->is_string())
            return core::err(ErrorCode::ParseError, at + ": \"cmd\" alanı yok: " + item.dump() +
                                                        ". Betik çalıştırılmadı.");

        command::Invocation inv;
        inv.name   = cmd->as_string();
        inv.origin = command::Origin::Script;

        if (const core::Json* a = item.find("args")) {
            auto args = command::Args::from_json(*a);
            if (!args)
                return core::err(args.error().code, at + " (" + inv.name +
                                                        "): " + args.error().message +
                                                        " Betik çalıştırılmadı.");
            inv.args = std::move(args.value());
        }
        out.steps.push_back(std::move(inv));
    }
    return out;
}

/// The file's text, or why it could not be read — under the sandbox's rule.
core::Result<std::string> read_file_text(const std::string& path, Sandbox sandbox)
{
    if (sandbox == Sandbox::Safe)
        return core::err(ErrorCode::Unsupported,
                         "Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. "
                         "Gerekli seviye: 'proje' veya 'tam'.");
    const std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return core::err(ErrorCode::IoFailure, "Betik dosyası açılamadı: " + path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

} // namespace

core::Result<RunReport> JsonRunner::run_text(std::string_view json, std::string label)
{
    auto read = read_script(json, std::move(label));
    if (!read) return read.error();
    label                                         = std::move(read.value().label);
    const std::vector<command::Invocation>& steps = read.value().steps;

    // The run's own record, BEFORE the first command, so a journal read top to
    // bottom says what a script was permitted before it says what it did
    // (`.claude/script.md` R11). Written for every level and every outcome.
    journal_run(bus_, "json", label, sandbox_, script_identity(json), /*consented=*/false);

    // One script block is ONE undo step (§2.5) and ONE validation pass (§10.4).
    if (auto st = bus_.begin_batch(label); !st) return st.error();

    RunReport report;
    report.label = label;

    for (const command::Invocation& inv : steps) {
        auto result = bus_.dispatch(inv);
        if (!result) {
            // A failing script leaves nothing behind — not in the drawing, not
            // on the redo stack, not in the journal. Half-applied cadastral or
            // zoning edits are never acceptable (§2.5, TODOS F-05).
            bus_.abort_batch();
            // SAID, because the transcript still shows what the lines before it
            // answered as they ran — "1 nesne taşındı" — and none of it stands.
            return core::err(result.error().code,
                             "Betik satırı " + std::to_string(report.commands + 1) + " (" +
                                 inv.name + "): " + sentence(result.error().message) +
                                 " Betik bütünüyle geri alındı; çizim betikten önceki hâlinde.");
        }

        ++report.commands;
    }

    auto closed = bus_.end_batch();
    if (!closed) return closed.error();

    report.ops     = closed.value().ops;
    report.changes = closed.value().changes;
    report.said    = closed.value().message;
    return report;
}

core::Result<RunReport> JsonRunner::run_file(const std::string& path)
{
    auto text = read_file_text(path, sandbox_);
    if (!text) return text.error();
    return run_text(text.value(), path);
}

core::Result<command::Preview> JsonRunner::preview_text(std::string_view json)
{
    auto read = read_script(json, "Betik");
    if (!read) return read.error();
    return bus_.preview(read.value().steps);
}

core::Result<command::Preview> JsonRunner::preview_file(const std::string& path)
{
    auto text = read_file_text(path, sandbox_);
    if (!text) return text.error();
    return preview_text(text.value());
}

void install(command::Bus& bus, JsonRunner& runner)
{
    bus.on_run_script = [&runner](const std::string& path) -> core::Result<std::string> {
        auto r = runner.run_file(path);
        if (!r) return r.error();
        return r.value().said;
    };
    bus.on_preview_script = [&runner](const std::string& path) {
        return runner.preview_file(path);
    };
}

} // namespace kentos::script
