// SPDX-License-Identifier: GPL-3.0-or-later
// core.dependency (BAĞIMLILIK) — which dependents still say what their sources
// say (TODOS F-04).
//
// FOUR TIES, ONE QUESTION (core/ties.hpp). A caption follows an edge, a
// dimension measures corners, a hatch fills what its boundary closes — each
// kept up to date at commit, unless it was locked — and a RESULT (a buffer, an
// area generated from lines, a boundary, a contour) records what its sources
// were when it was computed. This command reads every tie against the drawing
// as it is now and says, for all of them or the ones named, whether each is
// CURRENT, OUT OF DATE, BROKEN (a follower that lost a source) or SOURCELESS (a
// result whose source is gone); and it does the things a user decides about
// one that is not current: bring a follower back to its source (`yenile`),
// accept a result as it stands (`kabul`), or cut either loose (`coz`).
//
// NOTHING IS STORED THAT SAYS "OUT OF DATE". The answer is computed from the
// drawing, so an undo is always right, for every client alike.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/lineage.hpp"
#include "kentos_cad/core/ties.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// How many dependents the transcript names one by one; the report carries
/// every one of them.
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
const char* state_words(core::TieState s)
{
    switch (s) {
    case core::TieState::Current: return "güncel";
    case core::TieState::Behind: return "güncel değil";
    case core::TieState::Broken: return "bağı kopuk";
    case core::TieState::Sourceless: return "kaynaksız";
    }
    return "güncel";
}

/// A follower's kind as the transcript names it.
const char* follower_words(core::TieKind k)
{
    switch (k) {
    case core::TieKind::Caption: return "bağlı yazı";
    case core::TieKind::Dimension: return "bağlı ölçü";
    case core::TieKind::Hatch: return "bağlı tarama";
    case core::TieKind::Result: break;
    }
    return "sonuç";
}

bool is_result(const core::Tie& t)
{
    return t.kind == core::TieKind::Result;
}

/// How many ties of each state, results and followers apart.
struct Counts
{
    std::array<std::size_t, 4> results{};   ///< by `TieState`
    std::array<std::size_t, 4> followers{}; ///< by `TieState`
};

/// How many results one origin's recompute left, and which way.
struct Recomputed
{
    std::size_t in_place{0}; ///< one old result given the new shape: its key, values, ties kept
    std::size_t replaced{0}; ///< old results erased for the ones the run made
};

/// Computes the results of the origin `at` again (TODOS F-04): the operation
/// that made them runs INSIDE this command (`Bus::run_nested`) with the
/// arguments it was run with and its sources as they are now. One result made
/// one again takes the new shape IN PLACE — its key, its values, its style and
/// what follows it stay, so a label on a buffer follows the new buffer; any
/// other count replaces the old results with the new ones.
core::Status recompute(Context& ctx, std::uint32_t at, const std::vector<core::EntityId>& olds,
                       Recomputed& done)
{
    const core::Document& doc  = ctx.document();
    const core::Lineage origin = doc.lineage().origin(at); // a copy: the table grows below
    Bus& bus                   = ctx.session().bus();
    const CommandSpec* spec    = bus.registry().by_id(origin.operation);
    const std::string named_as = spec != nullptr && !spec->names.empty()
                                     ? std::string(spec->names.front())
                                     : origin.operation;
    const auto takes           = [spec](const char* param) {
        return spec != nullptr && std::ranges::any_of(spec->params, [param](const Param& p) {
                   return p.name == param;
               });
    };
    if (spec == nullptr || origin.arguments.empty() || !takes("nesneler"))
        return core::err(core::ErrorCode::InvalidArgument,
                         named_as + " sonucu (nesne " +
                             std::to_string(core::raw(doc.key_of(olds.front()))) +
                             ") yeniden hesaplanamaz: nasıl hesaplandığı kayıtlı değil. Kabul "
                             "edin (islem=kabul), çözün (islem=coz) ya da silip yeniden üretin.");
    auto json = core::Json::parse(origin.arguments);
    if (!json) return json.error();
    auto parsed = Args::from_json(json.value());
    if (!parsed) return parsed.error();
    Args args = std::move(parsed.value());

    // FROM ITS SOURCES AS THEY ARE NOW — the ones still there.
    Value::Ints live;
    for (const core::EntityKey k : origin.sources) {
        const core::EntityId e = doc.slot_of(k);
        if (e != core::kNoEntity && doc.alive(e))
            live.push_back(static_cast<std::int64_t>(core::raw(k)));
    }
    if (live.empty())
        return core::err(core::ErrorCode::NotFound,
                         named_as + " sonucunun (nesne " +
                             std::to_string(core::raw(doc.key_of(olds.front()))) +
                             ") hiçbir kaynağı kalmadı; yeniden hesaplanamaz.");
    args.set("nesneler", Value::ids(std::move(live)));
    // The new ones land where the old ones stood, whatever layer is active now.
    if (olds.size() != 1 && takes("katman") && !args.has("katman")) {
        const core::LayerId l = doc.entities().layer[olds.front()];
        if (l < doc.layers().size()) args.set("katman", Value::text(doc.layers()[l].name));
    }

    Transaction& tx        = ctx.transaction();
    const std::size_t mark = tx.size();
    auto ran = bus.run_nested(Invocation{spec->id, std::move(args), Origin::Batch}, tx);
    if (!ran)
        return core::err(ran.error().code, named_as + " sonucu (nesne " +
                                               std::to_string(core::raw(doc.key_of(olds.front()))) +
                                               ") yeniden hesaplanamadı: " + ran.error().message);
    std::vector<core::EntityId> news;
    for (const core::EntityId e : tx.created_since(mark)) {
        const core::Lineage* made = doc.alive(e) ? doc.lineage().get(e) : nullptr;
        if (made != nullptr && made->result() && made->operation == origin.operation)
            news.push_back(e);
    }

    const core::RingGeometry& geom = doc.geometry();
    if (olds.size() == 1 && news.size() == 1) {
        // IN PLACE: the old result takes the new one's shape, words, values and
        // origin, and the new one goes.
        const core::EntityId old  = olds.front();
        const core::EntityId made = news.front();
        const std::uint32_t slot  = doc.entities().slot[made];
        const core::RingSpan span = geom.rings_of(slot);
        std::vector<std::vector<core::Point2>> points(span.count);
        std::vector<core::RingGeometry::RingInput> rings;
        rings.reserve(span.count);
        for (std::uint32_t r = 0; r < span.count; ++r) {
            const auto xs = geom.ring_xs(span.first + r);
            const auto ys = geom.ring_ys(span.first + r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                points[r].push_back(core::Point2{xs[v], ys[v]});
            rings.push_back(core::RingGeometry::RingInput{points[r], geom.ring_role[span.first + r],
                                                          geom.ring_part[span.first + r]});
        }
        const auto payload_view = geom.payload_of(slot);
        const std::vector<std::uint8_t> payload(payload_view.begin(), payload_view.end());
        if (auto st = tx.set_kind_geometry(old, doc.entities().kind[made], rings, payload); !st)
            return st;
        const std::uint32_t made_slot = doc.entities().slot[made];
        if (doc.texts().has(made_slot))
            if (auto st = tx.set_text(old, std::string(doc.texts().text(made_slot)),
                                      doc.texts().height(made_slot), doc.texts().anchor(made_slot));
                !st)
                return st;
        const core::AttrTable& attrs = doc.attributes();
        for (std::size_t column = 0; column < attrs.columns(); ++column) {
            const auto c = static_cast<core::AttrId>(column);
            auto cell    = doc.attribute(c, made);
            if (cell && cell.value().present)
                if (auto st = tx.set_attribute(c, old, cell.value()); !st) return st;
        }
        const core::Lineage renewed = *doc.lineage().get(made);
        if (auto st = tx.set_lineage(old, renewed); !st) return st;
        if (auto st = tx.erase_entity(made); !st) return st;
        ++done.in_place;
        return core::ok();
    }
    // ANY OTHER COUNT: the old results go, the new ones stay.
    for (const core::EntityId old : olds)
        if (doc.alive(old))
            if (auto st = tx.erase_entity(old); !st) return st;
    done.replaced += olds.size();
    return core::ok();
}

Task<void> run_dependency(Context& ctx)
{
    const core::Document& doc = ctx.document();
    const Value asked         = ctx.argument("islem");
    const std::string verb    = asked.empty() ? std::string("durum") : asked.as_text();
    const auto named_as       = [&ctx](const std::string& id) {
        const CommandSpec* spec = ctx.session().bus().registry().by_id(id);
        return spec != nullptr && !spec->names.empty() ? std::string(spec->names.front()) : id;
    };

    // ---- which ties: the named objects', or every one the drawing holds ----
    const Value named = ctx.argument("nesneler");
    std::vector<core::Tie> ties;
    std::size_t untied = 0;
    if (!named.empty()) {
        std::vector<core::EntityId> chosen;
        for (const std::int64_t id : named.as_ids()) {
            const auto key         = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
            const core::EntityId e = doc.slot_of(key);
            if (e == core::kNoEntity || !doc.alive(e)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
                co_return;
            }
            chosen.push_back(e);
        }
        std::ranges::sort(chosen);
        chosen.erase(std::ranges::unique(chosen).begin(), chosen.end());
        for (const core::EntityId e : chosen) {
            std::vector<core::Tie> its = core::ties_of(doc, e);
            if (its.empty()) ++untied;
            for (core::Tie& t : its)
                ties.push_back(std::move(t));
        }
    } else {
        ties = core::every_tie(doc);
    }
    const auto key_of = [&doc](const core::Tie& t) {
        return static_cast<std::int64_t>(core::raw(doc.key_of(t.dependent)));
    };
    const auto say_untied = [&] {
        if (untied != 0)
            ctx.echo(std::to_string(untied) + " nesne kaynağına bağlı değil; atlandı.");
    };

    // ---- a decision about the ones that are not current ----
    if (verb == "kabul" || verb == "coz" || verb == "yenile") {
        // Named: those. Not named: every tie that is not current — a current one
        // has nothing to accept or bring back, and cutting one nobody asked about
        // would drop a tie that still holds.
        std::vector<const core::Tie*> acting;
        for (const core::Tie& t : ties)
            if (!named.empty() || t.state != core::TieState::Current) acting.push_back(&t);

        std::size_t results   = 0;
        std::size_t followers = 0;
        std::size_t locked    = 0;
        std::size_t refused   = 0; // a result asked to follow, a follower asked to be accepted
        std::vector<std::int64_t> keys;
        Transaction::SettleReport quiet;
        // THE RESULTS TO COMPUTE AGAIN, by the run that made them: thirty
        // contours are traced again once.
        std::map<std::uint32_t, std::vector<core::EntityId>> runs;
        std::size_t sourceless = 0;
        for (const core::Tie* t : acting) {
            const core::EntityId e = t->dependent;
            if (verb == "yenile") {
                if (is_result(*t)) {
                    if (t->state == core::TieState::Behind)
                        runs[doc.lineage().origin_of(e)].push_back(e);
                    else if (t->state == core::TieState::Sourceless)
                        ++sourceless;
                    continue;
                }
                if (t->state != core::TieState::Behind) continue;
                if (!doc.editable(e)) {
                    ++locked;
                    continue;
                }
                core::Status st = core::ok();
                if (t->kind == core::TieKind::Caption)
                    st = ctx.transaction().follow_caption(e, quiet);
                else if (t->kind == core::TieKind::Dimension)
                    st = ctx.transaction().follow_dimension(e, ctx.session().bus().drawing_unit(),
                                                            quiet);
                else
                    st = ctx.transaction().follow_hatch(e, quiet);
                if (!st) {
                    ctx.refuse(st.error());
                    co_return;
                }
                ++followers;
            } else if (verb == "kabul") {
                if (!is_result(*t)) {
                    ++refused;
                    continue;
                }
                // AS THEY ARE NOW: a source still there is recorded afresh; one
                // that is gone keeps what it was, and the result stays sourceless.
                core::Lineage next = *doc.lineage().get(e);
                for (std::size_t i = 0; i < next.sources.size(); ++i) {
                    const core::EntityId src = doc.slot_of(next.sources[i]);
                    if (src != core::kNoEntity && doc.alive(src))
                        next.revisions[i] = doc.content_revision(src);
                }
                if (auto st = ctx.transaction().set_lineage(e, std::move(next)); !st) {
                    ctx.refuse(st.error());
                    co_return;
                }
                ++results;
            } else { // coz
                core::Status st = core::ok();
                if (is_result(*t)) {
                    core::Lineage next = *doc.lineage().get(e);
                    next.revisions.clear(); // history now: kept, and never asked again
                    st = ctx.transaction().set_lineage(e, std::move(next));
                    ++results;
                } else {
                    if (t->kind == core::TieKind::Caption)
                        st = ctx.transaction().clear_attachment(e);
                    else if (t->kind == core::TieKind::Dimension)
                        st = ctx.transaction().set_dimension_links(e, {});
                    else
                        st = ctx.transaction().set_hatch_links(e, {});
                    ++followers;
                }
                if (!st) {
                    ctx.refuse(st.error());
                    co_return;
                }
            }
            keys.push_back(key_of(*t));
        }
        Recomputed recomputed;
        std::vector<std::string> recomputed_by;
        for (const auto& [at, olds] : runs) {
            for (const core::EntityId e : olds)
                keys.push_back(static_cast<std::int64_t>(core::raw(doc.key_of(e))));
            const std::string operation = doc.lineage().origin(at).operation;
            if (auto st = recompute(ctx, at, olds, recomputed); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            recomputed_by.push_back(named_as(operation));
        }
        std::ranges::sort(recomputed_by);
        recomputed_by.erase(std::ranges::unique(recomputed_by).begin(), recomputed_by.end());
        refused += sourceless;
        std::ranges::sort(keys);
        keys.erase(std::ranges::unique(keys).begin(), keys.end());

        if (keys.empty() && refused == 0 && locked == 0) {
            if (ties.empty())
                ctx.echo(untied != 0 ? "Seçilen nesnelerin hiçbiri kaynağına bağlı değil."
                                     : "Çizimde kaynağına bağlı bir nesne yok.");
            else if (verb == "yenile")
                ctx.echo("Kaynağının gerisinde kalmış bir bağlı nesne ya da sonuç yok.");
            else
                ctx.echo("Güncel olmayan bir sonuç yok; değişen bir şey olmadı.");
            co_return;
        }
        if (!keys.empty()) {
            ctx.record("islem", Value::text(verb));
            ctx.record("nesneler", Value::ids(keys));
        }
        if (verb == "kabul" && results != 0)
            ctx.echo(std::to_string(results) +
                     " sonuç, kaynakları şimdiki hâliyle güncel kabul edildi.");
        if (verb == "coz" && results != 0)
            ctx.echo(std::to_string(results) +
                     " sonuç kaynağından çözüldü: kökeni geçmiş olarak duruyor, güncel olup "
                     "olmadığı artık sorulmuyor.");
        if (verb == "coz" && followers != 0)
            ctx.echo(std::to_string(followers) +
                     " bağlı nesne bağından çözüldü: yerinde duruyor, kaynağını artık izlemiyor.");
        if (verb == "yenile" && followers != 0)
            ctx.echo(std::to_string(followers) + " bağlı nesne kaynağına yetiştirildi.");
        if (verb == "yenile" && (recomputed.in_place != 0 || recomputed.replaced != 0)) {
            std::string by;
            for (const std::string& n : recomputed_by)
                by += (by.empty() ? "" : ", ") + n;
            ctx.echo(std::to_string(recomputed.in_place + recomputed.replaced) +
                     " sonuç kaynaklarının şimdiki hâlinden yeniden hesaplandı (" + by + ").");
        }
        if (locked != 0)
            ctx.echo(std::to_string(locked) +
                     " bağlı nesne kilitli katmanda; katmanın kilidi açılınca kaynağına yetişir.");
        if (refused != 0)
            ctx.echo(verb == "yenile"
                         ? std::to_string(refused) +
                               " sonucun kaynağı silinmiş; yeniden hesaplanmaz. Kabul etmek için "
                               "islem=kabul, kaynağından çözmek için islem=coz."
                         : std::to_string(refused) +
                               " bağlı nesne kabul edilmez: kaynağına yetiştirmek için "
                               "islem=yenile, bağından çözmek için islem=coz.");
        say_untied();
        core::Json report = core::Json::object({});
        report.set("islem", core::Json::string(verb));
        core::Json done = core::Json::array({});
        for (const std::int64_t k : keys)
            done.push(core::Json::integer(k));
        report.set("nesneler", std::move(done));
        ctx.report(std::move(report));
        co_return;
    }

    // ---- durum: what each dependent is ----
    Counts counts;
    core::Json result_rows   = core::Json::array({});
    core::Json follower_rows = core::Json::array({});
    std::vector<std::string> lines;
    for (const core::Tie& t : ties) {
        const bool result = is_result(t);
        ++(result ? counts.results : counts.followers)[static_cast<std::size_t>(t.state)];
        core::Json row = core::Json::object({});
        row.set("nesne", core::Json::integer(key_of(t)));
        std::string what;
        if (result) {
            const core::Lineage& origin = *doc.lineage().get(t.dependent);
            row.set("islem", core::Json::string(origin.operation));
            row.set("ad", core::Json::string(named_as(origin.operation)));
            what = named_as(origin.operation);
        } else {
            row.set("tur", core::Json::string(core::tie_kind_id(t.kind)));
            row.set("kaynaklar", keys_json(t.sources));
            what = follower_words(t.kind);
        }
        row.set("durum", core::Json::string(core::tie_state_id(t.state)));
        row.set("degisen", keys_json(t.changed));
        row.set("silinen", keys_json(t.gone));
        (result ? result_rows : follower_rows).push(std::move(row));
        if (t.state == core::TieState::Current) continue;
        std::string line = "  " + std::string(state_words(t.state)) + ": nesne " +
                           std::to_string(key_of(t)) + " (" + what + ")";
        if (!t.changed.empty())
            line += std::string(result ? " — değişen kaynak: nesne " : " — kaynağı: nesne ") +
                    keys_listed(t.changed);
        if (!t.gone.empty()) line += " — silinen kaynak: nesne " + keys_listed(t.gone);
        if (t.state == core::TieState::Broken && t.gone.empty())
            line += " — bağlı olduğu köşe ya da sınır artık yok";
        lines.push_back(std::move(line));
    }

    // ---- and the sheets: a name a layout reads that points at nothing ----
    //
    // A sheet is read from the drawing each time it is drawn, so it is never
    // out of date — but a table or a map can name a layer or a column the
    // drawing does not have (TODOS F-04). Asked for the whole drawing only: a
    // sheet item is not an object `nesneler` can name.
    core::Json sheet_rows    = core::Json::array({});
    std::size_t sheet_count  = 0;
    std::size_t sheet_broken = 0;
    std::vector<std::string> sheet_lines;
    if (named.empty())
        for (const core::SheetTie& t : core::sheet_ties(doc)) {
            ++sheet_count;
            core::Json row = core::Json::object({});
            row.set("yerlesim", core::Json::string(t.layout));
            row.set("oge", core::Json::string(t.item));
            row.set("tur",
                    core::Json::string(t.kind == core::SheetTieKind::Layer ? "katman" : "sutun"));
            row.set("ad", core::Json::string(t.name));
            row.set("durum", core::Json::string(t.broken ? "kopuk" : "guncel"));
            sheet_rows.push(std::move(row));
            if (!t.broken) continue;
            ++sheet_broken;
            sheet_lines.push_back("  bağı kopuk: '" + t.layout + "' ▸ " +
                                  (t.item.empty() ? std::string("atlas") : "'" + t.item + "'") +
                                  " — '" + t.name + "' " +
                                  (t.kind == core::SheetTieKind::Layer ? "katmanı" : "sütunu") +
                                  " çizimde yok");
        }

    const std::size_t results   = result_rows.as_array().size();
    const std::size_t followers = follower_rows.as_array().size();
    const auto& r               = counts.results;
    const auto& f               = counts.followers;
    constexpr auto kCurrent     = static_cast<std::size_t>(core::TieState::Current);
    constexpr auto kBehind      = static_cast<std::size_t>(core::TieState::Behind);
    constexpr auto kBroken      = static_cast<std::size_t>(core::TieState::Broken);
    constexpr auto kSourceless  = static_cast<std::size_t>(core::TieState::Sourceless);
    if (ties.empty() && sheet_count == 0) {
        ctx.echo(untied != 0 ? "Seçilen nesnelerin hiçbiri kaynağına bağlı değil."
                             : "Çizimde kaynağına bağlı bir nesne yok.");
    } else {
        if (results != 0)
            ctx.echo(std::to_string(results) + " sonuç: " + std::to_string(r[kCurrent]) +
                     " güncel, " + std::to_string(r[kBehind]) + " güncel değil, " +
                     std::to_string(r[kSourceless]) + " kaynaksız.");
        if (followers != 0)
            ctx.echo(std::to_string(followers) + " bağlı nesne: " + std::to_string(f[kCurrent]) +
                     " güncel, " + std::to_string(f[kBehind]) + " güncel değil, " +
                     std::to_string(f[kBroken]) + " bağı kopuk.");
        if (sheet_count != 0)
            ctx.echo(std::to_string(sheet_count) +
                     " pafta bağı: " + std::to_string(sheet_count - sheet_broken) + " güncel, " +
                     std::to_string(sheet_broken) + " bağı kopuk.");
        lines.insert(lines.end(), sheet_lines.begin(), sheet_lines.end());
        for (std::size_t i = 0; i < lines.size() && i < kListed; ++i)
            ctx.echo(lines[i]);
        if (lines.size() > kListed)
            ctx.echo("  … ve " + std::to_string(lines.size() - kListed) + " nesne daha.");
        if (r[kBehind] != 0 || r[kSourceless] != 0)
            ctx.echo(
                std::string(r[kBehind] != 0
                                ? "Sonuçları yeniden hesaplamak için: BAĞIMLILIK islem=yenile — "
                                : "") +
                "şimdiki hâliyle kabul etmek için: BAĞIMLILIK islem=kabul — kaynağından "
                "çözmek için: BAĞIMLILIK islem=coz");
        if (f[kBehind] != 0)
            ctx.echo("Bağlı nesneleri kaynağına yetiştirmek için: BAĞIMLILIK islem=yenile");
        if (sheet_broken != 0)
            ctx.echo("Kopuk pafta bağını düzeltmek için öğeye var olan bir katman ya da sütun "
                     "verin: ÇIKTIÖĞE");
        say_untied();
    }

    core::Json report = core::Json::object({});
    report.set("sonuclar", std::move(result_rows));
    report.set("guncel", core::Json::integer(static_cast<std::int64_t>(r[kCurrent])));
    report.set("guncel_degil", core::Json::integer(static_cast<std::int64_t>(r[kBehind])));
    report.set("kaynaksiz", core::Json::integer(static_cast<std::int64_t>(r[kSourceless])));
    report.set("baglilar", std::move(follower_rows));
    report.set("paftalar", std::move(sheet_rows));
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
                Param::choice("islem", Arity::optional(), {"durum", "yenile", "kabul", "coz"},
                              "durum: bağlı nesnelerin ve sonuçların güncel olup olmadığı; "
                              "yenile: bağlı nesneyi kaynağına yetiştir; kabul: sonucun "
                              "kaynaklarını şimdiki hâliyle kabul et; coz: bağından çöz")
                    .en("action"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Sorulacak nesneler; verilmezse çizimdeki bütün bağlı nesneler ve sonuçlar"}
                    .en("objects"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bağlı yazı, ölçü, tarama ve türetilmiş sonuçların kaynaklarına göre güncel "
                   "olup olmadığını söyler; güncel olmayanı yetiştirir, kabul eder ya da "
                   "bağından çözer.",
        .run    = &run_dependency,
        .effect = Effect::Query | Effect::DocumentEdit,
    };
}

} // namespace kentos::command
