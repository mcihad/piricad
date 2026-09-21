// SPDX-License-Identifier: GPL-3.0-or-later
// core.suggestion (ÖNERİ), core.mcp (MCPSUNUCU)
//
// THE LISTENER MUST NOT BE MOUSE-ONLY. CLAUDE.md 5.15 forbids a feature reachable
// only by mouse and 1.2 makes the GUI one client among equals, so the menu entry
// and the status cell that start the agent server both run `MCPSUNUCU
// islem=baslat` — typeable, scriptable and journalled like everything else.
//
// THE DECISION IS THE EXCEPTION, AND IT IS THE ONE 5.7 REQUIRES. `ÖNERİ` reads
// freely — `islem=listele` and `islem=durum` answer any client — but
// `islem=uygula` and `islem=reddet` REFUSE, in words, naming the suggestion card
// as the only place a decision can be made. That is not an unfinished command:
// an `ai::Approval` has one private factory, called from one file (gate.hpp,
// ai.md P15), and a command that could stand in for that click would be exactly
// the trust mode P1 forbids. So the two verbs exist in order to give a client
// asking for them a clear refusal rather than a `Bilinmeyen komut`, and
// `docs/komutlar/suggestion.md` documents that behaviour as the behaviour.
#include "kentos_cad/ai/commands.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/log.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <string>
#include <vector>

namespace kentos::ai {
namespace {

using command::Arity;
using command::CommandSpec;
using command::Context;
using command::Effect;
using command::Flags;
using command::Param;
using command::Task;
using command::UndoPolicy;
using command::Value;
using Verb = command::Bus::AiRequest::Verb;

/// One `islem` word and what it asks for.
///
/// `needs_target` is whether the verb needs the command's SECOND argument — the
/// suggestion id for `ÖNERİ`, the client label for `MCPSUNUCU`. One bool for both
/// rosters because the shape is the same: a word, what it asks the application
/// for, and whether it has to name something.
struct Operation
{
    const char* name;
    Verb verb;
    bool needs_target;
};

constexpr Operation kSuggestionOps[] = {
    {"uygula", Verb::SuggestionApply, true},
    {"reddet", Verb::SuggestionReject, true},
    {"durum", Verb::SuggestionState, true},
    {"listele", Verb::SuggestionList, false},
};

constexpr Operation kServerOps[] = {
    {"baslat", Verb::ServerStart, false},       {"durdur", Verb::ServerStop, false},
    {"durum", Verb::ServerState, false},        {"belirtec", Verb::ServerToken, false},
    {"istemciler", Verb::ServerClients, false}, {"iptal", Verb::ServerRevoke, true},
    {"izin", Verb::ServerRestore, true},        {"sina", Verb::ServerProbe, false},
};

template<std::size_t N> std::string word_list(const Operation (&ops)[N])
{
    std::string out;
    for (const Operation& op : ops)
        out += (out.empty() ? "" : " / ") + std::string(op.name);
    return out;
}

template<std::size_t N> const Operation* resolve(const Operation (&ops)[N], std::string_view typed)
{
    for (const Operation& op : ops)
        if (core::turkish_key_equals(typed, op.name)) return &op;
    return nullptr;
}

/// Both commands answer the same way when nothing is attached: honestly.
bool engine_missing(Context& ctx, const char* what)
{
    if (ctx.session().bus().on_ai_request) return false;
    ctx.session().fail(
        core::err(core::ErrorCode::Unsupported,
                  std::string(what) + " bu yapıda bağlı değil; uygulama içinden çalıştırın."));
    return true;
}

Task<void> run_suggestion(Context& ctx)
{
    auto typed = co_await ctx.text("islem", "İşlem: " + word_list(kSuggestionOps));
    if (!typed) co_return;

    const Operation* op = resolve(kSuggestionOps, *typed);
    if (op == nullptr) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Tanınmayan işlem: '" + *typed +
                                         "'. İşlemler: " + word_list(kSuggestionOps)));
        co_return;
    }
    ctx.record("islem", Value::text(op->name));

    command::Bus::AiRequest request;
    request.verb = op->verb;

    if (op->needs_target) {
        auto plan = co_await ctx.text("oneri", "Öneri kimliği");
        if (!plan || plan->empty()) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "'" + std::string(op->name) +
                                             "' için öneri kimliği gerekir: oneri=<kimlik>. "
                                             "Bekleyenleri ÖNERİ islem=listele ile görün."));
            co_return;
        }
        request.plan = *plan;
        ctx.record("oneri", Value::text(request.plan));
    }

    if (engine_missing(ctx, "Öneri defteri")) co_return;

    auto answered = co_await ctx.session().bus().on_ai_request(request);
    if (!answered) {
        ctx.session().fail(answered.error());
        co_return;
    }
    ctx.echo(answered.value());
}

Task<void> run_mcp(Context& ctx)
{
    auto typed = co_await ctx.text("islem", "İşlem: " + word_list(kServerOps));
    if (!typed) co_return;

    const Operation* op = resolve(kServerOps, *typed);
    if (op == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *typed + "'. İşlemler: " + word_list(kServerOps)));
        co_return;
    }
    ctx.record("islem", Value::text(op->name));

    command::Bus::AiRequest request;
    request.verb = op->verb;
    if (const Value port = ctx.argument("port"); !port.empty()) {
        request.port = port.as_int();
        ctx.record("port", port);
    }

    if (op->needs_target) {
        auto who = co_await ctx.text("ad", "İstemcinin adı (MCPSUNUCU islem=istemciler ile görün)");
        if (!who || who->empty()) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "'" + std::string(op->name) +
                              "' için istemcinin adı gerekir: ad=<istemci>. Adları MCPSUNUCU "
                              "islem=istemciler ile görün."));
            co_return;
        }
        // THE LABEL IS A NAME AND NOTHING ELSE. It is the client's declared name
        // plus its token's fingerprint, which is what the audit record already
        // carries; no token, key or header value reaches this argument, the
        // journal line it becomes, or the echo below (CLAUDE.md 5.21).
        request.client = *who;
        ctx.record("ad", Value::text(request.client));
    }

    if (engine_missing(ctx, "Aracı sunucusu")) co_return;

    auto answered = co_await ctx.session().bus().on_ai_request(request);
    if (!answered) {
        ctx.session().fail(answered.error());
        co_return;
    }
    ctx.echo(answered.value());
}

} // namespace

std::vector<CommandSpec> detail::ai_command_specs()
{
    std::vector<CommandSpec> specs;

    specs.push_back(CommandSpec{
        .id       = "core.suggestion",
        .names    = {"ÖNERİ", "ONERI", "SUGGESTION", "ÖN"},
        .title    = "Yapay Zeka Önerisi",
        .category = command::Category::System,
        .params =
            {
                Param::choice("islem", Arity::exactly(1), {"uygula", "reddet", "durum", "listele"},
                              "Ne yapılacağı: uygula, reddet, durum ya da listele"),
                Param::text("oneri", Arity::optional(),
                            "Öneri kimliği; uygula, reddet ve durum için gerekir"),
            },
        // NOT `SingleTransaction`: applying a plan opens a BATCH inside the
        // application, so this command's own transaction would be a second one
        // around it. The undo entry belongs to the batch, which is what makes an
        // approved suggestion one Ctrl+Z (ai.md R4).
        .undo  = UndoPolicy::Custom,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.",
        .run = &run_suggestion,
        // READING A SUGGESTION IS A READ; APPLYING ONE IS NOT.
        //
        // `uygula` and `reddet` are refused from every command path anyway —
        // `AiService` answers them with "a suggestion is applied by the button on
        // its card" — so what an agent gains here is the ability to SEE what it
        // proposed and what came of it, which is the half of TODOS M-04 that does
        // not wait on the approval question.
        .effect      = Effect::Query | Effect::DocumentEdit,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"listele", Effect::Query},
                {"durum", Effect::Query},
                {"uygula", Effect::DocumentEdit},
                {"reddet", Effect::Query},
            },
    });

    specs.push_back(CommandSpec{
        .id       = "core.mcp",
        .names    = {"MCPSUNUCU", "MCPSERVER", "MCP"},
        .title    = "MCP Sunucusu",
        .category = command::Category::System,
        .params =
            {
                Param::choice("islem", Arity::exactly(1),
                              {"baslat", "durdur", "durum", "belirtec", "istemciler", "iptal",
                               "izin", "sina"},
                              "Ne yapılacağı: baslat, durdur, durum, belirtec (yeni belirteç "
                              "üretir), istemciler, iptal (bir istemcinin yetkisini kaldırır), "
                              "izin (geri verir) ya da sina (bağlantıyı sınar)"),
                Param::integer_range("port", Arity::optional(), 1024, 65535,
                                     "Yalnız bu başlatma için port; verilmezse ayardaki port"),
                Param::text("ad", Arity::optional(),
                            "İstemcinin adı; iptal ve izin için gerekir. Adları islem=istemciler "
                            "ile görün"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, "
                   "durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri "
                   "listeler ve tek bir istemcinin yetkisini kaldırır.",
        .run     = &run_mcp,
        // STARTING A LISTENER IS AN OUTWARD ACT. It binds a port on this machine
        // and hands a token to whoever is given it; no undo stack reaches that.
        .effect      = Effect::Query | Effect::ExternalWrite | Effect::SettingsChange,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"durum", Effect::Query},
                {"istemciler", Effect::Query},
                {"baslat", Effect::ExternalWrite},
                {"durdur", Effect::ExternalWrite},
                // WHO MAY REACH THIS MACHINE is a settings change, not a query:
                // it outlives the call and it is the decision an agent must
                // never make for itself (S-04).
                {"iptal", Effect::SettingsChange},
                {"izin", Effect::SettingsChange},
                // A PROBE IS A REAL REQUEST over a real socket — it leaves this
                // process, so it is an external write however harmless it is.
                {"sina", Effect::ExternalWrite},
                {"belirtec", Effect::SettingsChange},
            },
    });

    // AND WHICH MODEL RUNS, which is a third thing that must not be mouse-only
    // (commands/provider_command.cpp owns it).
    specs.push_back(detail::provider_command_spec());

    return specs;
}

void register_ai_commands(command::Registry& registry)
{
    // A FAILED REGISTRATION IS LOGGED AND THE REST STILL LAND, exactly as the
    // builtin roster does it (commands/builtin.cpp): one name clash must not
    // take the other six commands out of the program silently.
    for (CommandSpec& spec : detail::read_tool_specs())
        if (auto st = registry.add(std::move(spec)); !st) command::log_error(st.error().message);
    for (CommandSpec& spec : detail::ai_command_specs())
        if (auto st = registry.add(std::move(spec)); !st) command::log_error(st.error().message);
}

} // namespace kentos::ai
