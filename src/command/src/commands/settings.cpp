// SPDX-License-Identifier: GPL-3.0-or-later
// core.setting — AYAR, and core.preference — TERCİH.
//
// .claude/model.md R41: ONE command per SCOPE, never one command per setting.
// Fourteen settings do not become fourteen commands; they become two commands and
// a catalogue, so adding a setting adds a SettingSpec and nothing else — no menu
// entry to hand-write, no CLI table to sync, no docs table to forget
// (CLAUDE.md 5.10, model.md R38).
#include "kentos_cad/command/bus.hpp"

#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/crs.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"

#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/text.hpp"

#include <string>

namespace kentos::command {
namespace {

using core::Settings;
using core::SettingScope;
using core::SettingScopeMask;
using core::SettingSpec;
using core::SettingValue;

// ---------------------------------------------------------------------------
// PHASE-0 SEAM — the one thing in this file that is not its final shape.
//
// model.md R39 puts the PROJECT store inside the document: it travels with the
// file, it is undoable, and it is part of content_hash(). That requires three
// things this file does not own — `Document::settings()`, an `Op::SetSetting`
// variant, and `Transaction::set_setting()`. Until they land, the project store
// lives on the BUS, which owns exactly one document, and the write is therefore
// still not undoable and still outside content_hash().
//
// It is deliberately NOT a process-wide static any more. As a static it was one
// store shared by every bus in the process, so a project CRS set in one drawing
// leaked into the next File > New, and every test that read a project setting was
// coupled to whichever case had run before it.
//
// Everything else is already final: the scope masks are the real ones, so a
// project setting cannot be written through TERCİH and an application setting
// cannot be written through AYAR, and that boundary is tested. When the document
// gains its store, `bus.project_settings()` becomes `ctx.transaction().settings()`
// and nothing else in this file changes; the APP store stays on the bus, which is
// where R39 puts a per-user, per-machine value.
// ---------------------------------------------------------------------------

std::string with_unit(const SettingSpec& spec, const SettingValue& v)
{
    std::string out = core::format_setting(spec, v);
    if (!spec.unit.empty()) out += " " + spec.unit;
    return out;
}

void list_scope(Context& ctx, const Settings& store, SettingScope scope)
{
    ctx.echo(std::string(core::setting_scope_label(scope)) + " ayarları:");

    // Generated from the catalogue, in declaration order. There is no second list.
    for (const auto& spec : store.catalogue().all()) {
        if (spec.scope != scope) continue;
        ctx.echo("    " + spec.names.front() + " = " + with_unit(spec, store.get(spec.id)) +
                 (store.is_explicit(spec.id) ? "   (ayarlanmış)" : "   (varsayılan)"));
    }
}

/// "Why is this like this?" is the most common support question, so a bare query
/// answers it: the value, where it came from, and what the declaration allows.
void report_one(Context& ctx, const Settings& store, const SettingSpec& spec)
{
    ctx.echo(spec.names.front() + " = " + with_unit(spec, store.get(spec.id)));
    ctx.echo("    kaynak      : " +
             std::string(store.is_explicit(spec.id) ? "açıkça ayarlandı" : "varsayılan"));
    ctx.echo("    kimlik      : " + spec.id);
    ctx.echo("    kapsam      : " + std::string(core::setting_scope_label(spec.scope)));
    ctx.echo("    tür         : " + std::string(core::setting_type_label(spec.type)));
    ctx.echo("    varsayılan  : " + with_unit(spec, spec.fallback));

    if (spec.type == core::SettingType::Enum) {
        std::string values;
        for (std::size_t i = 0; i < spec.values.size(); ++i) {
            if (i) values += ", ";
            values += spec.values[i];
        }
        ctx.echo("    seçenekler  : " + values);
    } else if (spec.type != core::SettingType::Text && spec.range.bounded()) {
        ctx.echo("    aralık      : " + std::to_string(spec.range.min) + " .. " +
                 std::to_string(spec.range.max));
    }

    ctx.echo("    " + spec.summary);
}

/// A SHEET SCALE IS WHAT A DIMENSION'S SIZES ARE FOR, and a drawing unit what
/// its figures are written in (TODOS C-10). Neither setting rewrites a drawing
/// on its own — a dimension is document state and this store is not undoable
/// yet (the seam above) — so the change says what it left behind and the one
/// command that brings it up to date.
void hint_dimensions(const Context& ctx, const std::string& id, std::int64_t now)
{
    const core::Document& doc = ctx.document();
    std::size_t stale         = 0;
    std::int64_t basis        = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().kind[e] != core::kDimensionKind) continue;
        auto def = core::dimension_of(doc.geometry(), doc.entities().slot[e]);
        if (!def) continue;
        if (id == "core.plan.olcek" && def.value().scale_basis > 0 &&
            def.value().scale_basis != now) {
            ++stale;
            basis = def.value().scale_basis;
        }
        if (id == "core.cizim.birim" && def.value().unit == 0) ++stale;
    }
    if (stale == 0) return;
    if (id == "core.plan.olcek")
        ctx.echo("Bu çizimdeki " + std::to_string(stale) + " ölçü 1/" + std::to_string(basis) +
                 " paftası için boyutlandırılmış; 1/" + std::to_string(now) +
                 " paftasında kâğıtta aynı boyda kalmaları için: ÖLÇÜYENİLE");
    else
        ctx.echo("Bu çizimdeki " + std::to_string(stale) +
                 " ölçünün yazısı önceki birimle duruyor; yeni birimle yazmak için: ÖLÇÜYENİLE");
}

/// The whole body of both commands. They differ in the store they reach and in
/// whether the change is document state — never in what a user can do (Article 1.2).
Task<void> run_scope(Context& ctx, Settings& store, SettingScope scope)
{
    const Value name = ctx.argument("ad");
    if (name.empty()) {
        list_scope(ctx, store, scope);
        co_return;
    }

    const std::uint32_t index = store.catalogue().find(name.as_text());
    if (index == core::kNoSetting) {
        // One unknown-setting message, written once, in core.
        auto probe = store.lookup(name.as_text());
        ctx.refuse(probe.error());
        co_return;
    }

    const SettingSpec& spec = store.catalogue().at(index);
    if (spec.scope != scope) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + spec.id + "' ayarı " + core::setting_scope_label(spec.scope) +
                       " kapsamındadır; bu komut " + core::setting_scope_label(scope) +
                       " ayarlarını yönetir.");
        co_return;
    }

    // The canonical id, not the alias that was typed, so a replay of this journal
    // line resolves the same setting whatever the user's keyboard did (§2.2).
    ctx.record("ad", Value::text(spec.id));

    const Value value = ctx.argument("deger");
    if (value.empty()) {
        report_one(ctx, store, spec);
        co_return;
    }

    // "varsayılan" is a value word, not a setting value: it drops the explicit entry
    // so the declared default is in force again. It is how a user undoes a setting
    // in a script, where Ctrl+Z is not available.
    if (core::turkish_iequals(value.as_text(), "varsayılan") ||
        core::turkish_iequals(value.as_text(), "varsayilan") ||
        core::turkish_iequals(value.as_text(), "default")) {
        const SettingValue before = store.get(spec.id);
        if (auto st = store.reset(spec.id); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        // A reset is a change like any other, so whoever owns persistence has to
        // hear about it — otherwise the old value comes back on the next start.
        if (Bus& bus = ctx.session().bus(); bus.on_settings_changed)
            bus.on_settings_changed(spec.scope);

        ctx.record("deger", Value::text("varsayılan"));
        ctx.echo(spec.names.front() + " = " + with_unit(spec, store.get(spec.id)) +
                 "   (varsayılana döndü, önceki: " + with_unit(spec, before) + ")");
        co_return;
    }

    auto parsed = core::parse_setting(spec, value.as_text());
    if (!parsed) {
        ctx.refuse(parsed.error());
        co_return;
    }

    store.clear_warnings();

    // Through the BUS, not straight into the store. The bus resolves the scope
    // from the declaration and fires `on_settings_changed`, which is what
    // persists an App-scope value. Writing the store directly is what made
    // `TERCİH` from a script change a preference for that run and lose it, while
    // the same line from the menu survived — an asymmetry Article 1.2 forbids.
    auto change = ctx.session().bus().set_setting(spec.id, parsed.value());

    // The CRS is DOCUMENT state (model.md R36, R37), and the setting is the
    // interface to it rather than a second copy of it. Before this, writing
    // `AYAR koordinat_sistemi` moved the setting and left `Document::crs()` at its
    // constructed default forever — `Transaction::set_crs` existed and no command
    // called it — so a drawing reported one CRS to the exporter and another to its
    // own file. Two stores that disagree about what a coordinate means is exactly
    // the field blunder R36 is written against.
    //
    // Resolution goes through the bus hook because the zone catalogue lives in
    // /src/domain/geodesy and /src/command may not reach it (Article 3.2). With no
    // geodesy module the CRS keeps its id and stays unresolved, which is the
    // truthful state rather than a guess.
    if (change && spec.id == "core.crs.id") {
        Bus& bus           = ctx.session().bus();
        core::Crs resolved = bus.on_crs_resolve ? bus.on_crs_resolve(parsed.value().as_text())
                                                : core::Crs(std::string(parsed.value().as_text()));
        if (auto st = ctx.transaction().set_crs(resolved); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }

    if (change) {
        // Tell the shell before reporting to the user, so the change is on screen
        // by the time the transcript line appears.
        if (Bus& bus = ctx.session().bus(); bus.on_setting_changed)
            bus.on_setting_changed(spec.id, scope);
    }
    if (!change) {
        ctx.refuse(change.error());
        co_return;
    }

    // R42: an out-of-range value is clamped and the user is told, never silently
    // accepted and never a failure that would leave a file unopenable.
    for (const auto& w : store.warnings())
        ctx.echo(w.message);

    ctx.record("deger", Value::text(core::format_setting(spec, change.value().after)));
    ctx.echo(spec.names.front() + " = " + with_unit(spec, change.value().after) +
             "   (önceki: " + with_unit(spec, change.value().before) + ")");
    if (spec.id == "core.plan.olcek" || spec.id == "core.cizim.birim")
        hint_dimensions(ctx, spec.id, change.value().after.as_int());
}

Task<void> run_setting(Context& ctx)
{
    // When the document owns the store, the write inside run_scope becomes
    // ctx.transaction().set_setting(...) — one undo step, one entry in
    // content_hash(). See the PHASE-0 SEAM note above.
    co_await run_scope(ctx, ctx.session().bus().project_settings(), SettingScope::Project);
}

Task<void> run_preference(Context& ctx)
{
    co_await run_scope(ctx, ctx.session().bus().app_settings(), SettingScope::App);
}

Task<void> run_mode(Context& ctx)
{
    co_await run_scope(ctx, ctx.session().bus().session_settings(), SettingScope::Session);
}

} // namespace

KENTOS_COMMAND(setting)
{
    return CommandSpec{
        .id       = "core.setting",
        .names    = {"AYAR", "SETTING", "AY"},
        .title    = "Proje Ayarı",
        .category = Category::System,
        .params =
            {
                Param::text("ad", Arity::optional(), "Ayar adı veya kimliği; yoksa liste")
                    .en("name"),
                Param::text("deger", Arity::optional(), "Yeni değer; yoksa yalnızca okur")
                    .en("value"),
            },
        // One command = one undo step (§2.5). Declared here so the policy is right
        // the day the document owns the store; today the store is the seam above.
        .undo = UndoPolicy::SingleTransaction,
        // Deliberately NOT AiAccessible: changing the project CRS reinterprets every
        // coordinate in the document, and .claude/ai.md keeps that out of reach of a
        // suggestion. A licensed engineer sets it (kentoscad.md §5.1).
        .flags   = Flags::Scriptable,
        .summary = "Proje ayarlarını listeler, okur ve değiştirir.",
        .run     = &run_setting,
        .effect  = Effect::Query | Effect::SettingsChange,
    };
}

// R41 asks for one command per scope, and there are three scopes. Without this
// one the session settings were declared and unreachable: the snap modes, ortho,
// polar step and snap-to-grid had no store and no way in from any client. A
// setting nobody can write is not a setting.
KENTOS_COMMAND(mode)
{
    return CommandSpec{
        .id       = "core.mode",
        .names    = {"MOD", "MODE", "MD"},
        .title    = "Çizim Modları",
        .category = Category::System,
        .params =
            {
                Param::text("ad", Arity::optional(), "Mod adı veya kimliği; yoksa liste")
                    .en("name"),
                Param::text("deger", Arity::optional(), "Yeni değer; yoksa yalnızca okur")
                    .en("value"),
            },
        // Transient by R39: not undoable, not journalled as a document mutation.
        // ReadOnly is the flag that says so to the bus, exactly as TERCİH does.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::ReadOnly,
        .summary = "Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur "
                   "ve değiştirir.",
        .run     = &run_mode,
        // A MODE CHANGES WHAT THE NEXT COMMAND MEANS — which snap fires, which
        // ortho constraint applies — so it is a stored preference, not a view.
        .effect = Effect::Query | Effect::SettingsChange,
    };
}

KENTOS_COMMAND(preference)
{
    return CommandSpec{
        .id       = "core.preference",
        .names    = {"TERCİH", "TERCIH", "PREFERENCE", "PREF"},
        .title    = "Uygulama Tercihi",
        .category = Category::System,
        .params =
            {
                Param::text("ad", Arity::optional(), "Tercih adı veya kimliği; yoksa liste")
                    .en("name"),
                Param::text("deger", Arity::optional(), "Yeni değer; yoksa yalnızca okur")
                    .en("value"),
            },
        // R43: an application preference is not document state, so it is not
        // undoable and not journalled as a document mutation. ReadOnly says exactly
        // that to the bus — the same flag core.zoom carries for view state.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable | Flags::ReadOnly,
        .summary = "Uygulama tercihlerini listeler, okur ve değiştirir.",
        .run     = &run_preference,
        .effect  = Effect::Query | Effect::SettingsChange,
    };
}

} // namespace kentos::command
