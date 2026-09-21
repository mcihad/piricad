// SPDX-License-Identifier: GPL-3.0-or-later
// core.ai_provider (YAPAYZEKAMODELİ)
//
// WHICH MODEL RUNS IS A COMMAND, not a dialog. `.claude/ai.md` R13 requires the
// institution to choose its provider BY CONFIGURATION — llama.cpp, an in-house
// vLLM or Ollama server, a cloud API — and CLAUDE.md 5.15 forbids a feature
// reachable only by mouse. So the settings page's table, the chat dock's chooser
// and a deployment script all say the same sentence — this command's `ekle` verb
// with a name, an address and a model.
//
// THE API KEY IS NOT A PARAMETER OF IT, and this is the decision the file exists
// to hold. `anahtar_ref` carries the NAME of the entry in the operating system's
// key store (or of an environment variable); the secret itself is never a command
// argument, so it can never reach `Args`, the journal's `{cmd, args}` line, a
// transcript, an audit record or an error message — CLAUDE.md 5.21 and ai.md P11
// enforced by the shape of the command rather than by care at each call site.
// `ai::looks_like_secret` then refuses a `key_ref` that looks like a key, because
// the mistake to guard against is a user pasting `sk-…` into the box labelled
// "anahtar adı" (`ProviderProfiles::upsert`).
//
// THE WORK IS THE APPLICATION'S. This module may not touch a file, a socket or a
// keychain (ai.md P10), so the store lives behind `Bus::on_ai_provider_request`
// (bus.hpp) exactly as printing lives behind `Bus::on_print_request`. This file
// owns the command, its parameters and what the journal records: every profile
// field as it was resolved, and no credential.
#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/provider.hpp"
#include "kentos_cad/ai/redact.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
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
using command::Flags;
using command::Param;
using command::Task;
using command::UndoPolicy;
using command::Value;
using Request = command::AiProviderRequest;

/// One `islem` word and what it asks for.
struct Operation
{
    const char* name;
    Request::Verb verb;
    bool needs_name;
};

constexpr Operation kOperations[] = {
    {"listele", Request::Verb::Profiles, false}, {"ekle", Request::Verb::SetProfile, true},
    {"sil", Request::Verb::RemoveProfile, true}, {"varsayilan", Request::Verb::SetDefault, true},
    {"dene", Request::Verb::Test, true},
};

std::string operation_list()
{
    std::string out;
    for (const Operation& op : kOperations)
        out += (out.empty() ? "" : " / ") + std::string(op.name);
    return out;
}

const Operation* resolve_operation(std::string_view typed)
{
    for (const Operation& op : kOperations)
        if (core::turkish_key_equals(typed, op.name)) return &op;
    return nullptr;
}

/// The four dialect words, FROM `ai::dialects()` rather than written out here.
///
/// A second list of the dialects would be a second answer to "which wire
/// languages does this program speak" (CLAUDE.md 5.10 applied to data): the
/// enumerator, the wire id and the `Param::choice` list all come from
/// `provider.hpp`, so a fifth dialect arrives in the command's declared word list
/// with no edit to this file.
std::vector<std::string> dialect_words()
{
    std::vector<std::string> words;
    words.reserve(dialects().size());
    for (Dialect dialect : dialects())
        words.emplace_back(dialect_id(dialect));
    return words;
}

/// The canonical wire id for a word the user typed, Turkish-folded, or nothing.
///
/// FOLDED RATHER THAN COMPARED, because the bus already accepted the word by
/// folding it (`validation.cpp`): `OPENAI_CHAT` reaches the body as typed, and
/// the store and the JSON file must hold the canonical spelling.
const char* canonical_dialect(std::string_view typed)
{
    for (Dialect dialect : dialects())
        if (core::turkish_key_equals(typed, dialect_id(dialect))) return dialect_id(dialect);
    return nullptr;
}

/// Both of this command's failure modes when nothing is attached: honestly said.
bool engine_missing(Context& ctx)
{
    if (ctx.session().bus().on_ai_provider_request) return false;
    ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                 "Model sağlayıcı deposu bu ortamda bağlı değil; komutu uygulama "
                                 "içinden çalıştırın."));
    return true;
}

/// Reads the profile fields `ekle` may carry and records each one as resolved.
///
/// RECORDED AS RESOLVED so that a replay produces the same profile: the journal
/// line is the profile (Article 1.4). `anahtar_ref` is recorded too — it is a
/// NAME, and a replay that lost it would produce a profile that cannot
/// authenticate — while the key it names is never here to record.
bool read_profile(Context& ctx, Request& request)
{
    if (const Value v = ctx.argument("lehce"); !v.empty()) {
        const char* id = canonical_dialect(v.as_text());
        if (id == nullptr) {
            std::string list;
            for (const std::string& word : dialect_words())
                list += (list.empty() ? "" : " / ") + word;
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument,
                          "Tanınmayan lehçe: '" + v.as_text() + "'. Lehçeler: " + list));
            return false;
        }
        request.dialect = id;
        ctx.record("lehce", Value::text(request.dialect));
    }
    if (const Value v = ctx.argument("adres"); !v.empty()) {
        request.base_url = v.as_text();
        ctx.record("adres", v);
    }
    if (const Value v = ctx.argument("yol"); !v.empty()) {
        request.path = v.as_text();
        ctx.record("yol", v);
    }
    if (const Value v = ctx.argument("model"); !v.empty()) {
        request.model = v.as_text();
        ctx.record("model", v);
    }
    if (const Value v = ctx.argument("anahtar_ref"); !v.empty()) {
        // THE DOOR, AND IT IS SHUT HERE TOO. The store refuses a `key_ref` that
        // looks like a key (`ProviderProfiles::upsert`), but by then the value
        // has already been through `Args` — and the journal is written from
        // `resolved()` at the end of a SUCCESSFUL run, so recording it first and
        // asking later would be safe only by accident. The refusal is therefore
        // before the value is recorded at all, and the message says what the box
        // is for.
        if (looks_like_secret(v.as_text())) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "'anahtar_ref' bir API anahtarına benziyor. Buraya "
                                         "anahtarın kendisi değil, anahtar zincirindeki kaydın "
                                         "(ya da ortam değişkeninin) adı yazılır; anahtarı "
                                         "Seçenekler ▸ Yapay Zeka Modelleri sayfasından "
                                         "girersiniz."));
            return false;
        }
        request.key_ref = v.as_text();
        ctx.record("anahtar_ref", v);
    }
    if (const Value v = ctx.argument("baglam"); !v.empty()) {
        request.context = v.as_int();
        ctx.record("baglam", v);
    }
    // `azami` HERE IS A MODEL'S OUTPUT CAP, not a regulation's upper bound. The
    // threshold gate's word list reads that word as a zoning limit — a maximum
    // building height, a maximum built-area ratio — and such a number belongs in
    // `/data` and never in C++ (5.13). This one is the endpoint's `max_tokens`
    // and is an affordance the user types, so it carries the annotation the gate
    // provides for exactly that claim.
    if (const Value v = ctx.argument("azami"); !v.empty()) { // ui-label
        request.max_tokens = v.as_int();
        ctx.record("azami", v); // ui-label
    }
    if (const Value v = ctx.argument("sicaklik"); !v.empty()) {
        request.temperature = v.as_number();
        ctx.record("sicaklik", v);
    }
    const auto flag = [&ctx](const char* name, std::int8_t& into) {
        const Value v = ctx.argument(name);
        if (v.empty()) return;
        into = v.as_bool(true) ? std::int8_t{1} : std::int8_t{0};
        ctx.record(name, v);
    };
    flag("akis", request.stream);
    flag("dusunme", request.reasoning_text);
    flag("araclar", request.tools);
    return true;
}

Task<void> run_provider(Context& ctx)
{
    command::Bus& bus = ctx.session().bus();

    auto typed = co_await ctx.text("islem", "İşlem: " + operation_list());
    if (!typed) co_return;

    const Operation* op = resolve_operation(*typed);
    if (op == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *typed + "'. İşlemler: " + operation_list()));
        co_return;
    }
    ctx.record("islem", Value::text(op->name));

    Request request;
    request.verb = op->verb;
    if (op->needs_name) {
        auto name = co_await ctx.text("ad", "Model profilinin adı");
        if (!name || name->empty()) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         std::string("'") + op->name +
                                             "' için profil adı gerekir: ad=<ad>. Tanımlı "
                                             "profilleri YAPAYZEKAMODELİ islem=listele ile "
                                             "görün."));
            co_return;
        }
        request.name = *name;
    }
    // ONLY `ekle` CARRIES THE FIELDS. `sil`, `varsayilan` and `dene` name an
    // existing profile, and reading an address into a request that removes one
    // would journal a field the operation never used.
    if (op->verb == Request::Verb::SetProfile && !read_profile(ctx, request)) co_return;

    if (engine_missing(ctx)) co_return;

    auto answered = co_await bus.on_ai_provider_request(request);
    if (!answered) {
        ctx.session().fail(answered.error());
        co_return;
    }
    ctx.echo(answered.value());
}

} // namespace

CommandSpec detail::provider_command_spec()
{
    return CommandSpec{
        .id       = "core.ai_provider",
        .names    = {"YAPAYZEKAMODELİ", "YAPAYZEKAMODELI", "AIMODEL", "YZM"},
        .title    = "Yapay Zeka Modeli",
        .category = command::Category::System,
        .params =
            {
                Param::choice("islem", Arity::exactly(1),
                              {"listele", "ekle", "sil", "varsayilan", "dene"},
                              "Ne yapılacağı: listele, ekle, sil, varsayilan ya da dene "
                              "(bağlantıyı dener)"),
                Param::text("ad", Arity::optional(),
                            "Profilin adı; ekle, sil, varsayilan ve dene için gerekir"),
                Param::choice("lehce", Arity::optional(), dialect_words(),
                              "Uç noktanın konuştuğu telli dil; ekle için, varsayılan "
                              "openai_chat"),
                Param::text("adres", Arity::optional(),
                            "Uç noktanın adresi: http:// ya da https:// ile başlar, satıcının "
                            "ön eki dahil"),
                Param::text("yol", Arity::optional(),
                            "Adresin altındaki uç nokta; '/' ile başlar: /chat/completions, "
                            "/messages, /api/chat"),
                Param::text("model", Arity::optional(), "Model kimliği, uç noktanın yazdığı gibi"),
                // THE NAME OF THE ENTRY, NEVER THE KEY (CLAUDE.md 5.21, ai.md
                // P11). The help says so, because the box's label is the only
                // warning a user reads before pasting.
                Param::text("anahtar_ref", Arity::optional(),
                            "Anahtarı tutan kaydın adı — anahtar zincirindeki kayıt ya da bir "
                            "ortam değişkeni (örnek: DEEPSEEK_API_KEY). Anahtarın kendisi "
                            "buraya yazılmaz"),
                Param::integer_range("baglam", Arity::optional(), 0, 100000000,
                                     "Bağlam penceresi, jeton; 0 bilinmiyor demektir"),
                Param::integer_range("azami", Arity::optional(), 0, 10000000, // ui-label
                                     "Çıktı jeton sınırı; 0 demek 'bu alanı hiç gönderme'"),
                // NO DECLARED RANGE, and that is deliberate: the bus checks a
                // range with `as_int()` (validation.cpp), which truncates 2.5 to
                // 2 and -0.5 to 0 — so a declared 0–2 on a real number would
                // accept both of those. The store checks it properly and names
                // the profile in the refusal (`ProviderProfiles::upsert`).
                Param::number("sicaklik", Arity::optional(),
                              "Örnekleme sıcaklığı, 0 ile 2 arasında; verilmezse hiç "
                              "gönderilmez"),
                Param::boolean("akis", Arity::optional(),
                               "Cevap parça parça mı istensin; varsayılan evet"),
                Param::boolean("dusunme", Arity::optional(),
                               "Modelin düşünme metni gösterilsin mi"),
                Param::boolean("araclar", Arity::optional(),
                               "Uç noktaya araç kataloğu gönderilsin mi; varsayılan evet"),
            },
        // Touches no entity: the profiles are application state kept in the
        // user's configuration directory (model.md R39), so there is nothing to
        // undo. NOT `ReadOnly`, though — that flag suppresses the journal entry
        // (`Bus::finish`), and which model an office pointed at, and when, is
        // exactly the kind of change a journal is for.
        .undo = UndoPolicy::None,
        // NOT `AiAccessible`, and not by omission. A model that could rewrite
        // which model runs — or point this one at an endpoint of its own
        // choosing — is a model editing the boundary it is confined by. The
        // provider choice belongs to the person at the workstation
        // (CLAUDE.md 2.8, ai.md R13).
        .flags = Flags::Interactive | Flags::Scriptable,
        .summary = "Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan "
                   "yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar "
                   "adını taşır.",
        .run = &run_provider,
    };
}

} // namespace kentos::ai
