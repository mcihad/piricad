// SPDX-License-Identifier: GPL-3.0-or-later
// core.cleanup (TEMİZLE) — what the drawing holds twice or holds for nothing.
//
// FOUND FIRST, REPAIRED ON REQUEST (TODOS C-09). `islem=bul`, the default,
// changes nothing: it selects every object a repair would take or change, marks
// where each one is, and says what it found. `islem=onar` repairs in one
// transaction — one undo step — and says, object by object, what went and what
// changed, with each changed face's area before and after: a repair that moved
// a boundary by a millimetre is a repair the surveyor must be able to see.
//
// NOTHING THAT CARRIES DATA IS DELETED. A copy that holds attributes its twin
// lacks, or that a caption follows, is not a copy a repair may throw away; it is
// kept and named, and the user decides.
//
// The finding is `core::find_redundant` — the same finder TOPOLOJİ reports
// through, so the check and the repair can never disagree.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/cleanup.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// An area in square metres to two decimals, divided in integers (Article 2.4).
std::string square_metres(core::Mm2 v)
{
    const bool negative     = v < 0;
    const auto abs_mm2      = static_cast<std::uint64_t>(negative ? -v : v);
    const std::uint64_t cm2 = (abs_mm2 + 5000) / 10000;
    std::string frac        = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac.insert(frac.begin(), '0');
    return (negative ? "-" : "") + std::to_string(cm2 / 100) + "," + frac + " m²";
}

std::string key_of(const core::Document& doc, core::EntityId e)
{
    return std::to_string(core::raw(doc.entities().key[e]));
}

const char* label_of(core::RedundancyKind k)
{
    switch (k) {
    case core::RedundancyKind::Duplicate: return "yinelenen";
    case core::RedundancyKind::Empty: return "boş";
    case core::RedundancyKind::RepeatedVertex: return "tekrarlanan köşe";
    }
    return "";
}

/// What one finding says in a line of the report.
std::string line_of(const core::Document& doc, const core::Redundancy& r)
{
    switch (r.kind) {
    case core::RedundancyKind::Duplicate:
        return "Nesne " + key_of(doc, r.entity) + ", nesne " + key_of(doc, r.kept) +
               "'in aynısı (aynı tür, aynı katman, aynı köşeler).";
    case core::RedundancyKind::Empty:
        return "Nesne " + key_of(doc, r.entity) +
               " hiçbir şey çizmiyor (uzunluğu ya da alanı yok).";
    case core::RedundancyKind::RepeatedVertex:
        return "Nesne " + key_of(doc, r.entity) + ": " + std::to_string(r.vertices) +
               " köşe bir öncekiyle aynı yerde.";
    }
    return {};
}

bool followed(const core::Document& doc, core::EntityId e)
{
    std::vector<core::EntityId> deps;
    doc.attachments().dependents_of(doc.entities().key[e], deps);
    return !deps.empty();
}

Task<void> run(Context& ctx)
{
    Bus& bus                  = ctx.session().bus();
    const core::Document& doc = ctx.document();
    const core::Mm tolerance =
        bus.project_settings().get("core.topoloji.dugum_toleransi").as_length();

    // THE SCOPE: named, else the selection, else the whole drawing — as
    // TOPOLOJİ reads it, so the two are asked about the same objects.
    std::vector<std::int64_t> ids;
    if (const Value named = ctx.argument("nesneler"); !named.empty()) ids = named.as_ids();
    if (ids.empty())
        for (const core::EntityKey k : bus.selection().keys())
            ids.push_back(static_cast<std::int64_t>(core::raw(k)));
    std::vector<core::EntityId> slots;
    for (const std::int64_t raw : ids) {
        const core::EntityId e =
            raw > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)))
                    : core::kNoEntity;
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        slots.push_back(e);
    }
    const std::string scope =
        slots.empty() ? "bütün çizim" : std::to_string(slots.size()) + " nesne";
    const Value way   = ctx.argument("islem");
    const bool repair = !way.empty() && core::turkish_key_equals(way.as_text(), "onar");

    const std::vector<core::Redundancy> found = core::find_redundant(doc, slots, tolerance);
    if (!ids.empty()) ctx.record("nesneler", Value::ids(ids));
    ctx.record("islem", Value::text(repair ? "onar" : "bul"));
    if (found.empty()) {
        ctx.echo("Temizlik (" + scope + "): yinelenen, boş ya da tekrarlanan köşeli nesne yok.");
        co_return;
    }

    std::size_t duplicates = 0;
    std::size_t empties    = 0;
    std::size_t repeated   = 0;
    std::size_t corners    = 0;
    for (const core::Redundancy& r : found) {
        if (r.kind == core::RedundancyKind::Duplicate) ++duplicates;
        if (r.kind == core::RedundancyKind::Empty) ++empties;
        if (r.kind == core::RedundancyKind::RepeatedVertex) {
            ++repeated;
            corners += r.vertices;
        }
    }
    const std::string counts = std::to_string(duplicates) + " yinelenen, " +
                               std::to_string(empties) + " boş nesne, " + std::to_string(repeated) +
                               " nesnede " + std::to_string(corners) + " tekrarlanan köşe";
    constexpr std::size_t kListed = 12; // what a person reads; the rest are counted

    if (!repair) {
        // FIND: the objects a repair would take or change become the
        // selection, and each is marked where it is.
        bus.selection().clear();
        for (const core::Redundancy& r : found)
            bus.selection().add(doc.entities().key[r.entity]);
        if (bus.on_selection_changed) bus.on_selection_changed();
        for (const core::Redundancy& r : found)
            ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Point,
                                 .points = {r.at},
                                 .labels = {label_of(r.kind)}});
        std::string said = "Temizlik (" + scope + "): " + counts + ". Seçildi ve işaretlendi.";
        for (std::size_t i = 0; i < found.size() && i < kListed; ++i)
            said += "\n  " + line_of(doc, found[i]);
        if (found.size() > kListed)
            said += "\n  … ve " + std::to_string(found.size() - kListed) + " nesne daha.";
        said += "\n  Onarmak için: TEMİZLE islem=onar";
        ctx.echo(said);
        co_return;
    }

    // ---- REPAIR, in the command's one transaction ----
    std::vector<std::string> lines;
    std::vector<std::string> kept;
    std::vector<std::int64_t> erased;
    core::Json changed     = core::Json::array({});
    core::Mm2 before_total = 0;
    core::Mm2 after_total  = 0;
    for (const core::Redundancy& r : found) {
        const std::string key = key_of(doc, r.entity);
        if (r.kind == core::RedundancyKind::RepeatedVertex) {
            auto fixed = core::rings_without_repeats(doc, r.entity, tolerance);
            if (!fixed) continue;
            const core::Mm2 before = doc.entity_area(r.entity);
            std::vector<core::RingGeometry::RingInput> rings;
            rings.reserve(fixed->size());
            for (const core::RepairedRing& ring : *fixed)
                rings.push_back(core::RingGeometry::RingInput{ring.points, ring.role, ring.part});
            if (auto st = ctx.transaction().set_geometry(r.entity, rings); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            const core::Mm2 after = doc.entity_area(r.entity);
            before_total += before;
            after_total += after;
            std::string line =
                "Nesne " + key + ": " + std::to_string(r.vertices) + " köşe çıkarıldı";
            if (before != 0 || after != 0)
                line += "; alan önce " + square_metres(before) + ", sonra " + square_metres(after);
            lines.push_back(line + ".");
            core::Json row = core::Json::object({});
            row.set("nesne", core::Json::integer(std::stoll(key)));
            row.set("cikan_kose", core::Json::integer(static_cast<std::int64_t>(r.vertices)));
            row.set("onceki_alan_mm2", core::Json::integer(before));
            row.set("sonraki_alan_mm2", core::Json::integer(after));
            changed.push(std::move(row));
            continue;
        }
        // A copy or an empty object that carries data is kept and named.
        const bool data =
            r.kind == core::RedundancyKind::Duplicate
                ? !core::same_attributes(doc, r.entity, r.kept) || followed(doc, r.entity)
                : core::has_attributes(doc, r.entity) || followed(doc, r.entity);
        if (data) {
            kept.push_back("Nesne " + key +
                           " silinmedi: öznitelik taşıyor ya da ona bağlı bir "
                           "yazı var; karar sizin.");
            continue;
        }
        const core::Mm2 area = doc.entity_area(r.entity);
        if (auto st = ctx.transaction().erase_entity(r.entity); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        erased.push_back(std::stoll(key));
        std::string line = "Nesne " + key + " silindi";
        if (r.kind == core::RedundancyKind::Duplicate)
            line += " (aynısı nesne " + key_of(doc, r.kept) + " duruyor";
        else
            line += " (hiçbir şey çizmiyordu";
        if (area != 0) line += ", alanı " + square_metres(area);
        lines.push_back(line + ").");
    }

    // A retired key never comes back (R4): a selection still holding one would
    // point at nothing for the rest of the session.
    bool touched = false;
    for (const std::int64_t raw : erased)
        touched |=
            bus.selection().remove(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
    if (touched && bus.on_selection_changed) bus.on_selection_changed();

    std::string said = "Temizlik onarıldı (" + scope + "): " + std::to_string(erased.size()) +
                       " nesne silindi, " + std::to_string(changed.as_array().size()) +
                       " nesneden " + std::to_string(corners) + " köşe çıkarıldı";
    if (before_total != 0 || after_total != 0)
        said += "; değişen alanlar önce " + square_metres(before_total) + ", sonra " +
                square_metres(after_total);
    said += '.';
    for (std::size_t i = 0; i < lines.size() && i < kListed; ++i)
        said += "\n  " + lines[i];
    if (lines.size() > kListed)
        said += "\n  … ve " + std::to_string(lines.size() - kListed) + " nesne daha.";
    for (const std::string& k : kept)
        said += "\n  " + k;
    ctx.echo(said);

    core::Json report = core::Json::object({});
    core::Json gone   = core::Json::array({});
    for (const std::int64_t k : erased)
        gone.push(core::Json::integer(k));
    report.set("silinen", std::move(gone));
    report.set("degisen", std::move(changed));
    report.set("korunan", core::Json::integer(static_cast<std::int64_t>(kept.size())));
    report.set("onceki_alan_mm2", core::Json::integer(before_total));
    report.set("sonraki_alan_mm2", core::Json::integer(after_total));
    ctx.report(std::move(report));
}

} // namespace

KENTOS_COMMAND(cleanup)
{
    return CommandSpec{
        .id       = "core.cleanup",
        .names    = {"TEMİZLE", "TEMIZLE", "OVERKILL", "TMZ"},
        .title    = "Temizle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Bakılacak nesneler; yoksa seçim, o da boşsa bütün çizim"}
                    .en("objects"),
                Param::choice("islem", Arity::optional(), {"bul", "onar"},
                              "bul: bulur, seçer ve işaretler, hiçbir şeyi değiştirmez · onar: "
                              "yinelenenleri ve boş nesneleri siler, tekrarlanan köşeleri çıkarır")
                    .en("action"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Yinelenen, boş ve tekrarlanan köşeli nesneleri bulur; istenirse tek adımda "
                   "onarır ve değişen alanları önce/sonra raporlar.",
        .run     = &run,
    };
}

} // namespace kentos::command
