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

core::Result<RunReport> JsonRunner::run_text(std::string_view json, std::string label)
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

    // The run's own record, BEFORE the first command, so a journal read top to
    // bottom says what a script was permitted before it says what it did
    // (`.claude/script.md` R11). Written for every level and every outcome.
    journal_run(bus_, "json", label, sandbox_, script_identity(json), /*consented=*/false);

    // EVERY LINE IS READ BEFORE ANY RUNS (TODOS F-05). A malformed 251st item
    // used to close the batch over the 250 before it — committed, one undo step,
    // and an error saying the script had failed. A script whose text is broken
    // is refused as a whole, by line number, with the drawing untouched.
    std::vector<command::Invocation> steps;
    steps.reserve(list->as_array().size());
    for (const core::Json& item : list->as_array()) {
        const std::string at = "Betik satırı " + std::to_string(steps.size() + 1);
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
        steps.push_back(std::move(inv));
    }

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
    if (sandbox_ == Sandbox::Safe)
        return core::err(ErrorCode::Unsupported,
                         "Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. "
                         "Gerekli seviye: 'proje' veya 'tam'.");

    const std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return core::err(ErrorCode::IoFailure, "Betik dosyası açılamadı: " + path);

    std::ostringstream buf;
    buf << in.rdbuf();
    return run_text(buf.str(), path);
}

void install(command::Bus& bus, JsonRunner& runner)
{
    bus.on_run_script = [&runner](const std::string& path) -> core::Result<std::string> {
        auto r = runner.run_file(path);
        if (!r) return r.error();
        return r.value().said;
    };
}

} // namespace kentos::script
