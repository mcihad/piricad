// SPDX-License-Identifier: GPL-3.0-or-later
// core.dependency (BAĞIMLILIK) — which results still say what their sources
// say (TODOS F-04).
//
// A RESULT IS A STATEMENT ABOUT ITS SOURCES. A buffer is about a well, a
// generated area about the lines that close it, a boundary about the linework
// it was found in, a contour about the points it was traced through; each
// records its sources' content when it was computed (core/lineage.hpp). This
// command reads that record against the drawing as it is now and says, for
// every result or the ones named, whether it is CURRENT, OUT OF DATE (a source
// still there has changed) or SOURCELESS (a source is gone, nothing else has
// changed) — and does the two things a user decides about one that is not
// current: ACCEPT it as it stands, recording its sources as they are now, or
// DETACH it, keeping its history and dropping the claim.
//
// NOTHING IS STORED THAT SAYS "OUT OF DATE". The answer is computed from the
// drawing, so an undo that puts a well back makes its buffer current again,
// for every client alike and with no flag an undo would have to find.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/lineage.hpp"

#include <algorithm>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// How many out-of-date results the transcript names one by one; the report
/// carries every one of them.
constexpr std::size_t kListed = 20;

/// "12, 13 ve 14": keys as a sentence lists them.
std::string keys_listed(std::span<const core::EntityKey> keys)
{
    std::string out;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i != 0) out += i + 1 == keys.size() ? " ve " : ", ";
        out += std::to_string(core::raw(keys[i]));
    }
    return out;
}

/// The keys as the report writes them.
core::Json keys_json(std::span<const core::EntityKey> keys)
{
    core::Json out = core::Json::array({});
    for (const core::EntityKey k : keys)
        out.push(core::Json::integer(static_cast<std::int64_t>(core::raw(k))));
    return out;
}

/// A state as the transcript says it.
const char* state_words(core::ResultState s)
{
    switch (s) {
    case core::ResultState::Current: return "güncel";
    case core::ResultState::Stale: return "güncel değil";
    case core::ResultState::Sourceless: return "kaynaksız";
    case core::ResultState::History: break;
    }
    return "geçmiş";
}

Task<void> run_dependency(Context& ctx)
{
    const core::Document& doc       = ctx.document();
    const core::LineageTable& table = doc.lineage();
    const Value asked               = ctx.argument("islem");
    const std::string verb          = asked.empty() ? std::string("durum") : asked.as_text();

    // ---- which objects: the ones named, or every result the drawing holds ----
    const Value named = ctx.argument("nesneler");
    std::vector<core::EntityId> scope;
    std::size_t not_results = 0;
    if (!named.empty()) {
        for (const std::int64_t id : named.as_ids()) {
            const auto key         = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
            const core::EntityId e = doc.slot_of(key);
            if (e == core::kNoEntity || !doc.alive(e)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
                co_return;
            }
            const core::Lineage* origin = table.get(e);
            if (origin == nullptr || !origin->result()) {
                ++not_results;
                continue;
            }
            scope.push_back(e);
        }
    } else {
        for (const core::EntityId e : table.derived())
            if (doc.alive(e) && table.get(e)->result()) scope.push_back(e);
    }
    std::ranges::sort(scope);
    scope.erase(std::ranges::unique(scope).begin(), scope.end());

    // ---- each result against its sources, one answer per shared origin ----
    std::map<std::uint32_t, core::ResultCheck> checked;
    const auto check = [&](core::EntityId e) -> const core::ResultCheck& {
        const std::uint32_t at = table.origin_of(e);
        auto it                = checked.find(at);
        if (it == checked.end())
            it = checked.emplace(at, core::check_origin(doc, table.origin(at))).first;
        return it->second;
    };
    const auto named_as = [&ctx](const std::string& id) {
        const CommandSpec* spec = ctx.session().bus().registry().by_id(id);
        return spec != nullptr && !spec->names.empty() ? std::string(spec->names.front()) : id;
    };

    if (verb == "kabul" || verb == "coz") {
        // ---- a decision about the ones that are not current ----
        //
        // Named: those. Not named: every result that is not current — a
        // current one has nothing to accept, and detaching one nobody asked
        // about would drop a claim that still holds.
        std::vector<core::EntityId> acting;
        for (const core::EntityId e : scope)
            if (!named.empty() || check(e).state != core::ResultState::Current) acting.push_back(e);
        if (acting.empty()) {
            ctx.echo(scope.empty() ? "Çizimde kaynağına bağlı bir sonuç yok."
                                   : "Güncel olmayan bir sonuç yok; değişen bir şey olmadı.");
            co_return;
        }
        std::vector<std::int64_t> keys;
        for (const core::EntityId e : acting) {
            core::Lineage next = *table.get(e);
            if (verb == "coz") {
                next.revisions.clear(); // history now: kept, and never asked again
            } else {
                // AS THEY ARE NOW: a source still there is recorded afresh; one
                // that is gone keeps what it was, and the result stays sourceless.
                for (std::size_t i = 0; i < next.sources.size(); ++i) {
                    const core::EntityId src = doc.slot_of(next.sources[i]);
                    if (src != core::kNoEntity && doc.alive(src))
                        next.revisions[i] = doc.content_revision(src);
                }
            }
            if (auto st = ctx.transaction().set_lineage(e, std::move(next)); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            keys.push_back(static_cast<std::int64_t>(core::raw(doc.key_of(e))));
        }
        ctx.record("islem", Value::text(verb));
        ctx.record("nesneler", Value::ids(keys));
        ctx.echo(verb == "coz"
                     ? std::to_string(acting.size()) +
                           " sonuç kaynağından çözüldü: kökeni geçmiş olarak duruyor, güncel olup "
                           "olmadığı artık sorulmuyor."
                     : std::to_string(acting.size()) +
                           " sonuç, kaynakları şimdiki hâliyle güncel kabul edildi.");
        if (not_results != 0)
            ctx.echo(std::to_string(not_results) +
                     " nesne kaynağına bağlı bir sonuç değil; atlandı.");
        core::Json report = core::Json::object({});
        report.set("islem", core::Json::string(verb));
        core::Json done = core::Json::array({});
        for (const std::int64_t k : keys)
            done.push(core::Json::integer(k));
        report.set("nesneler", std::move(done));
        ctx.report(std::move(report));
        co_return;
    }

    // ---- durum: what each result is ----
    std::size_t current    = 0;
    std::size_t stale      = 0;
    std::size_t sourceless = 0;
    core::Json rows        = core::Json::array({});
    std::vector<std::string> lines;
    for (const core::EntityId e : scope) {
        const core::ResultCheck& c  = check(e);
        const core::Lineage& origin = *table.get(e);
        const auto key              = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
        core::Json row              = core::Json::object({});
        row.set("nesne", core::Json::integer(key));
        row.set("islem", core::Json::string(origin.operation));
        row.set("ad", core::Json::string(named_as(origin.operation)));
        row.set("durum", core::Json::string(core::result_state_id(c.state)));
        row.set("degisen", keys_json(c.changed));
        row.set("silinen", keys_json(c.gone));
        rows.push(std::move(row));
        if (c.state == core::ResultState::Current) {
            ++current;
            continue;
        }
        (c.state == core::ResultState::Stale ? stale : sourceless) += 1;
        std::string line = "  " + std::string(state_words(c.state)) + ": nesne " +
                           std::to_string(key) + " (" + named_as(origin.operation) + ")";
        if (!c.changed.empty()) line += " — değişen kaynak: nesne " + keys_listed(c.changed);
        if (!c.gone.empty()) line += " — silinen kaynak: nesne " + keys_listed(c.gone);
        lines.push_back(std::move(line));
    }

    if (scope.empty()) {
        ctx.echo(not_results != 0 ? "Seçilen nesnelerin hiçbiri kaynağına bağlı bir sonuç değil."
                                  : "Çizimde kaynağına bağlı bir sonuç yok.");
    } else {
        ctx.echo(std::to_string(scope.size()) + " sonuç: " + std::to_string(current) + " güncel, " +
                 std::to_string(stale) + " güncel değil, " + std::to_string(sourceless) +
                 " kaynaksız.");
        for (std::size_t i = 0; i < lines.size() && i < kListed; ++i)
            ctx.echo(lines[i]);
        if (lines.size() > kListed)
            ctx.echo("  … ve " + std::to_string(lines.size() - kListed) + " sonuç daha.");
        if (stale != 0 || sourceless != 0)
            ctx.echo("Kaynakların şimdiki hâlini kabul etmek için: BAĞIMLILIK islem=kabul — "
                     "sonucu kaynağından çözmek için: BAĞIMLILIK islem=coz");
    }
    if (not_results != 0 && !scope.empty())
        ctx.echo(std::to_string(not_results) + " nesne kaynağına bağlı bir sonuç değil; atlandı.");

    core::Json report = core::Json::object({});
    report.set("sonuclar", std::move(rows));
    report.set("guncel", core::Json::integer(static_cast<std::int64_t>(current)));
    report.set("guncel_degil", core::Json::integer(static_cast<std::int64_t>(stale)));
    report.set("kaynaksiz", core::Json::integer(static_cast<std::int64_t>(sourceless)));
    ctx.report(std::move(report));
}

} // namespace

KENTOS_COMMAND(dependency)
{
    return CommandSpec{
        .id       = "core.dependency",
        .names    = {"BAĞIMLILIK", "BAGIMLILIK", "DEPENDENCY", "BĞM"},
        .title    = "Bağımlılıklar",
        .category = Category::Query,
        .params =
            {
                Param::choice("islem", Arity::optional(), {"durum", "kabul", "coz"},
                              "durum: sonuçların güncel olup olmadığı; kabul: kaynakların "
                              "şimdiki hâlini kabul et; coz: sonucu kaynağından çöz")
                    .en("action"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Sorulacak sonuçlar; verilmezse çizimdeki bütün sonuçlar"}
                    .en("objects"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Türetilmiş sonuçların kaynaklarına göre güncel olup olmadığını söyler; "
                   "güncel olmayanı kabul eder ya da kaynağından çözer.",
        .run    = &run_dependency,
        .effect = Effect::Query | Effect::DocumentEdit,
    };
}

} // namespace kentos::command
