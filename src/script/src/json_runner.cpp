// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/script/json_runner.hpp"

#include "piricad/core/json.hpp"

#include <fstream>
#include <sstream>

namespace piricad::script {
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
        if (!list) list = doc.find("commands");
    }

    if (!list || !list->is_array())
        return core::err(
            ErrorCode::ParseError,
            "Betik ya bir komut dizisi ya da \"komutlar\" alanı olan bir nesne olmalı");

    // One script block is ONE undo step (§2.5) and ONE validation pass (§10.4).
    if (auto st = bus_.begin_batch(label); !st) return st.error();

    RunReport report;
    report.label = label;

    for (const core::Json& item : list->as_array()) {
        if (!item.is_object()) {
            (void)bus_.end_batch();
            return core::err(ErrorCode::ParseError,
                             "Betik satırı bir nesne olmalı: " + item.dump());
        }

        const core::Json* cmd = item.find("cmd");
        if (!cmd) cmd = item.find("komut");
        if (!cmd || !cmd->is_string()) {
            (void)bus_.end_batch();
            return core::err(ErrorCode::ParseError,
                             "Betik satırında \"cmd\" alanı yok: " + item.dump());
        }

        command::Invocation inv;
        inv.name   = cmd->as_string();
        inv.origin = command::Origin::Script;

        if (const core::Json* a = item.find("args")) {
            auto args = command::Args::from_json(*a);
            if (!args) {
                (void)bus_.end_batch();
                return args.error();
            }
            inv.args = std::move(args.value());
        }

        auto result = bus_.dispatch(inv);
        if (!result) {
            // A failing script leaves nothing behind. Half-applied cadastral or
            // zoning edits are never acceptable (§2.5).
            auto closed = bus_.end_batch();
            if (closed && closed.value().mutated) {
                std::string discarded;
                (void)bus_.undo_stack().undo(bus_.document(), &discarded);
            }
            return core::err(result.error().code, "Betik satırı " +
                                                      std::to_string(report.commands + 1) + " (" +
                                                      inv.name + "): " + result.error().message);
        }

        ++report.commands;
    }

    auto closed = bus_.end_batch();
    if (!closed) return closed.error();

    report.ops = closed.value().ops;
    return report;
}

core::Result<RunReport> JsonRunner::run_file(const std::string& path)
{
    if (sandbox_ == Sandbox::Safe)
        return core::err(ErrorCode::Unsupported,
                         "Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. "
                         "Gerekli seviye: 'proje' veya 'tam'.");

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return core::err(ErrorCode::IoFailure, "Betik dosyası açılamadı: " + path);

    std::ostringstream buf;
    buf << in.rdbuf();
    return run_text(buf.str(), path);
}

void install(command::Bus& bus, JsonRunner& runner)
{
    bus.on_run_script = [&runner](const std::string& path) -> core::Status {
        auto r = runner.run_file(path);
        if (!r) return r.error();
        return core::ok();
    };
}

} // namespace piricad::script
