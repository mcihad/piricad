// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/cadastre/topology.hpp"

#include "kentos_cad/core/cleanup.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/planar.hpp"
#include "kentos_cad/core/spatial_index.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <span>
#include <string>

namespace kentos::domain::cadastre {
namespace {

/// Reads one entity's rings out as a polygon, or returns false when it has none
/// that close. A line has no area and no topology to be wrong about.
bool polygon_of(const core::Document& doc, core::EntityId slot, core::Polygon& out)
{
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

    out.exterior.clear();
    out.holes.clear();

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;

        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);

        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});

        if (geom.ring_role[r] == core::RingRole::Exterior && out.exterior.empty())
            out.exterior = std::move(ring);
        else if (ring.size() >= 3)
            out.holes.push_back(std::move(ring));
    }
    return out.exterior.size() >= 3;
}

core::Mm2 abs_area(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

/// Whether the boundary crosses itself.
///
/// Asked of the LIBRARY rather than answered here: a union of a ring with nothing
/// resolves every self-crossing into separate simple rings, so a ring that comes
/// back as more than one piece — or whose area changes — was not simple. That is
/// the same engine the boolean uses, so a ring this accepts is a ring TEVHİT and
/// İFRAZ can work with, which is the property that actually matters.
bool self_intersecting(const core::Polygon& poly)
{
    auto resolved = core::polygon_boolean({poly}, {}, core::BooleanOp::Union);
    if (!resolved) return true; // a shape the boolean refuses is not usable either
    if (resolved.value().size() != 1) return true;

    const core::Mm2 before = abs_area(core::ring_area(poly.exterior));
    const core::Mm2 after  = abs_area(core::ring_area(resolved.value().front().exterior));

    // A self-crossing ring loses the lobe it folds back over, so its resolved area
    // is smaller. A millimetre of slack keeps a legitimately collinear vertex from
    // reading as a defect.
    return before > after + 1000;
}

} // namespace

core::Result<std::vector<Defect>> check_topology(const core::Document& doc,
                                                 const std::vector<core::EntityKey>& keys,
                                                 core::Mm tolerance,
                                                 const command::JobControl& control)
{
    // HOW OFTEN THE STOP IS LOOKED AT: a few thousand polygons, or a few
    // hundred of the pairwise pass, are well inside the 100 ms a stop has.
    constexpr std::size_t kStride     = 2048;
    constexpr std::size_t kPairStride = 256;
    const auto stopped                = [] {
        return core::err(core::ErrorCode::Cancelled,
                                        "Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.");
    };

    std::vector<Defect> found;

    // The slots to check: what was asked for, or every live area in the drawing.
    std::vector<core::EntityId> slots;
    if (keys.empty()) {
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e)) slots.push_back(e);
    } else {
        for (core::EntityKey k : keys) {
            const core::EntityId slot = doc.slot_of(k);
            if (slot != core::kNoEntity && doc.alive(slot)) slots.push_back(slot);
        }
    }

    // ---- pass one, 0..10 %: the areas, and what is wrong with one alone ----
    std::vector<core::Polygon> polys;
    std::vector<core::EntityKey> owners;
    polys.reserve(slots.size());
    owners.reserve(slots.size());

    for (std::size_t n = 0; n < slots.size(); ++n) {
        if (n % kStride == 0) {
            if (control.cancelled()) return stopped();
            control.at(n, slots.size(), 0, 100);
        }
        const core::EntityId slot = slots[n];
        core::Polygon poly;
        if (!polygon_of(doc, slot, poly)) continue; // not an area; nothing to check

        const core::EntityKey key = doc.entities().key[slot];

        // WHERE TO LOOK: the first corner, which is where the canvas marks it.
        const core::Point2 corner = poly.exterior.empty() ? core::Point2{} : poly.exterior.front();
        if (abs_area(core::ring_area(poly.exterior)) == 0) {
            found.push_back(Defect{DefectKind::ZeroArea, key, core::EntityKey::None, 0, 0, corner});
            continue;
        }
        if (self_intersecting(poly)) {
            found.push_back(
                Defect{DefectKind::SelfIntersecting, key, core::EntityKey::None, 0, 0, corner});
            continue;
        }

        polys.push_back(std::move(poly));
        owners.push_back(key);
    }

    // ---- pass two, 10..80 %: pairwise overlap, between neighbours only ----
    //
    // Each parcel's box is computed once, and the R-tree over them hands every
    // parcel the ones whose boxes may touch its own; the exact box test and
    // then the boolean — orders of magnitude dearer — run on those alone. The
    // neighbours are taken in ascending order, so the findings come out in the
    // order the every-pair loop gave them.
    std::vector<core::Box2> boxes(polys.size());
    for (std::size_t i = 0; i < polys.size(); ++i)
        for (const core::Point2& p : polys[i].exterior)
            boxes[i].extend(p);
    core::SpatialIndex index;
    index.build(std::span<const core::Box2>(boxes));

    std::vector<core::EntityId> near;
    for (std::size_t i = 0; i < polys.size(); ++i) {
        if (i % kPairStride == 0) {
            if (control.cancelled()) return stopped();
            control.at(i, polys.size(), 100, 800);
        }
        const core::Box2& bi = boxes[i];
        near.clear();
        index.query(bi, near);
        std::ranges::sort(near);

        for (const core::EntityId j : near) {
            if (j <= i) continue;
            const core::Box2& bj = boxes[j];
            if (bi.max_x < bj.min_x || bj.max_x < bi.min_x) continue;
            if (bi.max_y < bj.min_y || bj.max_y < bi.min_y) continue;

            auto shared =
                core::polygon_boolean({polys[i]}, {polys[j]}, core::BooleanOp::Intersection);
            if (!shared || shared.value().empty()) continue;

            core::Mm2 total              = 0;
            core::Mm2 largest_area       = 0;
            const core::Polygon* largest = nullptr;
            for (const core::Polygon& piece : shared.value()) {
                const core::Mm2 a = abs_area(core::ring_area(piece.exterior));
                total += a;
                if (largest == nullptr || a > largest_area) {
                    largest      = &piece;
                    largest_area = a;
                }
            }

            // A SQUARE MILLIMETRE OF SLACK. Two parcels that share a boundary
            // meet along it, and rounding a shared vertex to the millimetre can
            // leave a sliver a few square millimetres wide. Reporting that as an
            // overlap would bury the real ones.
            if (total > 1000) {
                // THE GROUND THEY BOTH CLAIM, kept so the canvas can outline it:
                // "12 ile 13 örtüşüyor" on a sheet of four thousand parcels says
                // which, and the outline says where.
                Defect d{DefectKind::Overlap, owners[i], owners[j], total};
                if (largest != nullptr) {
                    d.region = largest->exterior;
                    core::Box2 around;
                    for (const core::Point2& p : d.region)
                        around.extend(p);
                    d.at = core::Point2{
                        (around.min_x / 2) + (around.max_x / 2),
                        (around.min_y / 2) + (around.max_y / 2),
                    };
                }
                found.push_back(std::move(d));
            }
        }
    }

    // ---- pass three, 80..90 %: what TEMİZLE repairs, found by the finder it
    //      repairs with ----
    if (control.cancelled()) return stopped();
    control.at(0, 1, 800, 900);
    const auto key_of = [&doc](core::EntityId e) { return doc.entities().key[e]; };
    for (const core::Redundancy& r : core::find_redundant(doc, slots, tolerance)) {
        switch (r.kind) {
        case core::RedundancyKind::Duplicate:
            found.push_back(
                Defect{DefectKind::Duplicate, key_of(r.entity), key_of(r.kept), 0, 0, r.at, {}, 0});
            break;
        case core::RedundancyKind::Empty:
            // A face of no area is already a ZeroArea defect above.
            if (core::Polygon poly; polygon_of(doc, r.entity, poly)) break;
            found.push_back(Defect{DefectKind::ZeroLength,
                                   key_of(r.entity),
                                   core::EntityKey::None,
                                   0,
                                   0,
                                   r.at,
                                   {},
                                   0});
            break;
        case core::RedundancyKind::RepeatedVertex:
            found.push_back(Defect{DefectKind::RepeatedVertex,
                                   key_of(r.entity),
                                   core::EntityKey::None,
                                   0,
                                   r.vertices,
                                   r.at,
                                   {},
                                   0});
            break;
        }
    }

    // ---- pass four, 90..100 %: the gaps of a line network, found by the
    //      network SINIR builds ----
    //
    // LINES ONLY. An open run that should meet another and stops short is the
    // defect a boundary-line layer has; a face is closed by definition and its
    // neighbours are checked pairwise above.
    if (control.cancelled()) return stopped();
    control.at(0, 1, 900, 1000);
    if (core::network_available()) {
        std::vector<core::NetworkPiece> pieces;
        for (const core::EntityId e : slots) {
            const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
            bool open                 = span.count > 0;
            for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
                open = open && doc.geometry().ring_role[r] == core::RingRole::Open;
            if (!open) continue;
            std::vector<core::NetworkPiece> own = core::network_pieces(doc, e, e);
            pieces.insert(pieces.end(), own.begin(), own.end());
        }
        if (!pieces.empty()) {
            if (auto net = core::Network::build(pieces, tolerance)) {
                std::vector<core::OpenEnd> ends = net.value().open_ends();
                std::ranges::stable_sort(ends, [](const core::OpenEnd& a, const core::OpenEnd& b) {
                    return a.distance < b.distance;
                });
                std::set<core::Point2> used; // an end in one gap only
                for (const core::OpenEnd& end : ends) {
                    if (!end.has_nearest || used.contains(end.at) || used.contains(end.nearest))
                        continue;
                    used.insert(end.at);
                    used.insert(end.nearest);
                    found.push_back(Defect{DefectKind::Gap, key_of(end.source),
                                           core::EntityKey::None, 0, 0, end.at, end.nearest,
                                           end.distance});
                }
            }
        }
    }
    if (control.cancelled()) return stopped();
    control.at(1, 1);
    return found;
}

std::string square_metres(core::Mm2 area)
{
    // Square metres to two decimals, in integers, the way ALANÖLÇ prints one.
    const auto cm2   = static_cast<std::uint64_t>((area + 5000) / 10000);
    std::string frac = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

std::string describe(const core::Document& doc, const Defect& d)
{
    (void)doc;
    const auto first  = static_cast<unsigned long long>(core::raw(d.first));
    const auto second = static_cast<unsigned long long>(core::raw(d.second));

    switch (d.kind) {
    case DefectKind::SelfIntersecting:
        return "Nesne " + std::to_string(first) + ": sınır kendini kesiyor.";
    case DefectKind::ZeroArea: return "Nesne " + std::to_string(first) + ": alanı sıfır.";
    case DefectKind::Overlap:
        return "Nesne " + std::to_string(first) + " ile " + std::to_string(second) +
               " örtüşüyor: " + square_metres(d.area) + ".";
    case DefectKind::Duplicate:
        return "Nesne " + std::to_string(first) + ", nesne " + std::to_string(second) +
               "'in aynısı (yinelenen; TEMİZLE islem=onar siler).";
    case DefectKind::ZeroLength:
        return "Nesne " + std::to_string(first) + ": uzunluğu yok, hiçbir şey çizmiyor.";
    case DefectKind::RepeatedVertex:
        return "Nesne " + std::to_string(first) + ": " + std::to_string(d.count) +
               " köşe bir öncekiyle aynı yerde.";
    case DefectKind::Gap: {
        // Centimetres below a metre, metres to two decimals above it.
        const core::Mm a = d.distance;
        std::string width;
        if (a < 1000) {
            width = a % 10 == 0 ? std::to_string(a / 10) + " cm" : std::to_string(a) + " mm";
        } else {
            const core::Mm cm = (a + 5) / 10;
            std::string frac  = std::to_string(cm % 100);
            if (frac.size() < 2) frac = "0" + frac;
            width = std::to_string(cm / 100) + "," + frac + " m";
        }
        return "Nesne " + std::to_string(first) + ": açık uç, en yakın çizgiye " + width +
               " (boşluk).";
    }
    }
    return "Bilinmeyen kusur.";
}

} // namespace kentos::domain::cadastre

// ---------------------------------------------------------------- TOPOLOJİ ---

namespace kentos::command {
namespace {

/// One kind of defect as the summary and the structured answer name it.
struct KindWords
{
    domain::cadastre::DefectKind kind;
    const char* id;    ///< the structured answer's word, ASCII
    const char* words; ///< the summary's
};

/// In the order the summary counts them: what claims ground first.
constexpr std::array<KindWords, 7> kKindWords{{
    {domain::cadastre::DefectKind::Overlap, "ortusme", "örtüşme"},
    {domain::cadastre::DefectKind::SelfIntersecting, "kendini_kesen", "kendini kesen sınır"},
    {domain::cadastre::DefectKind::ZeroArea, "sifir_alan", "sıfır alanlı nesne"},
    {domain::cadastre::DefectKind::Duplicate, "yinelenen", "yinelenen nesne"},
    {domain::cadastre::DefectKind::ZeroLength, "bos", "boş nesne"},
    {domain::cadastre::DefectKind::RepeatedVertex, "tekrarlanan_kose", "tekrarlanan köşeli nesne"},
    {domain::cadastre::DefectKind::Gap, "bosluk", "boşluk"},
}};

/// How many findings the transcript lists one by one; the rest are counted,
/// marked on the canvas and given whole in the structured answer.
constexpr std::size_t kListed = 20;

const KindWords& words_of(domain::cadastre::DefectKind kind)
{
    for (const KindWords& w : kKindWords)
        if (w.kind == kind) return w;
    return kKindWords.front();
}

core::Json point_json(core::Point2 p)
{
    return core::Json::array({core::Json::integer(p.x), core::Json::integer(p.y)});
}

Task<void> run_topology(Context& ctx)
{
    const Selection& selection = ctx.session().bus().selection();

    std::vector<core::EntityKey> keys;

    // Named for the reason `stakeout_command.cpp` gives: `argument` returns by
    // value and `as_ids` refers into it, so the two written together leave the
    // loop reading a destroyed vector.
    const Value picked = ctx.argument("nesneler");
    for (std::int64_t raw : picked.as_ids())
        keys.push_back(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
    if (keys.empty())
        for (core::EntityKey k : selection.keys())
            keys.push_back(k);

    const core::Document& doc = ctx.document();
    const core::Mm tolerance =
        ctx.session().bus().project_settings().get("core.topoloji.dugum_toleransi").as_length();

    // WHAT WAS CHECKED, always. "No defects" is only reassuring if the user knows
    // how much was looked at; a report that says nothing about scope is a report
    // that could have checked one parcel.
    std::size_t looked = 0;
    if (keys.empty()) {
        looked = doc.live_entity_count();
    } else {
        for (const core::EntityKey k : keys)
            if (const core::EntityId e = doc.slot_of(k); e != core::kNoEntity && doc.alive(e))
                ++looked;
    }
    const std::string scope =
        (keys.empty() ? "bütün çizim, " : "seçim, ") + std::to_string(looked) + " nesne";

    // LONG WORK, OFF THE UI THREAD when the session can be resumed
    // (command/job.hpp, TODOS F-05): a hundred thousand parcels is seconds, and
    // the status strip counts them with a Durdur beside it. The check only reads
    // the drawing, and nothing writes it while the job runs (`Bus::writable`).
    core::Result<std::vector<domain::cadastre::Defect>> checked =
        core::err(core::ErrorCode::Internal, "Topoloji denetimi başlamadı.");
    Job job;
    job.label = "Topoloji denetimi";
    job.work  = [&](const JobControl& control) {
        checked = domain::cadastre::check_topology(doc, keys, tolerance, control);
    };
    co_await run_job(ctx.session(), job);

    if (job.stop.stop_requested() ||
        (!checked && checked.error().code == core::ErrorCode::Cancelled)) {
        // STOPPED, NOT CHECKED: half a check is not a smaller check, and its
        // "no defects" would be a lie. Nothing is said about the findings.
        ctx.session().end_stopped();
        ctx.echo("Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.");
        co_return;
    }
    if (!checked) {
        ctx.refuse(checked.error());
        co_return;
    }
    const std::vector<domain::cadastre::Defect>& found = checked.value();

    // ---- the structured answer: every finding, whatever the transcript lists ----
    std::array<std::size_t, kKindWords.size()> counts{};
    core::Json listed = core::Json::array({});
    for (const domain::cadastre::Defect& d : found) {
        for (std::size_t k = 0; k < kKindWords.size(); ++k)
            if (kKindWords[k].kind == d.kind) ++counts[k];
        core::Json one;
        one.set("tur", core::Json::string(words_of(d.kind).id));
        one.set("nesne", core::Json::integer(static_cast<std::int64_t>(core::raw(d.first))));
        if (d.second != core::EntityKey::None)
            one.set("diger", core::Json::integer(static_cast<std::int64_t>(core::raw(d.second))));
        if (d.kind == domain::cadastre::DefectKind::Overlap)
            one.set("alan_mm2", core::Json::integer(d.area));
        if (d.kind == domain::cadastre::DefectKind::RepeatedVertex)
            one.set("kose", core::Json::integer(static_cast<std::int64_t>(d.count)));
        if (d.kind == domain::cadastre::DefectKind::Gap) {
            one.set("uc", point_json(d.to));
            one.set("genislik_mm", core::Json::integer(d.distance));
        }
        one.set("nokta", point_json(d.at));
        one.set("aciklama", core::Json::string(domain::cadastre::describe(doc, d)));
        listed.push(std::move(one));
    }
    core::Json kinds;
    for (std::size_t k = 0; k < kKindWords.size(); ++k)
        if (counts[k] != 0)
            kinds.set(kKindWords[k].id, core::Json::integer(static_cast<std::int64_t>(counts[k])));
    core::Json report;
    report.set("kapsam", core::Json::string(keys.empty() ? "cizim" : "secim"));
    report.set("bakilan", core::Json::integer(static_cast<std::int64_t>(looked)));
    report.set("kusur", core::Json::integer(static_cast<std::int64_t>(found.size())));
    report.set("turler", std::move(kinds));
    report.set("kusurlar", std::move(listed));
    ctx.report(std::move(report));

    if (found.empty()) {
        ctx.echo("Topoloji denetimi (" + scope + "): kusur bulunamadı.");
        co_return;
    }

    // ---- the summary a person reads: the count, what kinds, the first few ----
    //
    // BOUNDED. A sheet with four thousand overlaps used to put four thousand
    // lines on the transcript, which is the same as putting none: the count by
    // kind comes first, the first twenty follow, and every one of them is
    // marked on the canvas and given whole in the structured answer.
    std::string kinds_said;
    for (std::size_t k = 0; k < kKindWords.size(); ++k)
        if (counts[k] != 0)
            kinds_said += (kinds_said.empty() ? "" : ", ") + std::to_string(counts[k]) + " " +
                          kKindWords[k].words;
    std::string said = "Topoloji denetimi (" + scope + "): " + std::to_string(found.size()) +
                       " kusur — " + kinds_said + ".";
    for (std::size_t i = 0; i < found.size() && i < kListed; ++i)
        said += "\n  " + domain::cadastre::describe(doc, found[i]);
    if (found.size() > kListed)
        said += "\n  … ve " + std::to_string(found.size() - kListed) +
                " kusur daha; hepsi tuvalde işaretli.";

    // WHERE A DEFECT HAS A PLACE, the canvas shows it: a gap as the line across
    // it, an overlap as the ground both parcels claim, the rest as a point.
    // View state, never recorded.
    for (const domain::cadastre::Defect& d : found) {
        using Kind = domain::cadastre::DefectKind;
        if (d.kind == Kind::Gap) {
            ctx.mark(MeasureMark{
                .shape = MeasureMark::Shape::Gap, .points = {d.at, d.to}, .labels = {"boşluk"}});
        } else if (d.kind == Kind::Overlap && d.region.size() >= 3) {
            ctx.mark(MeasureMark{.shape  = MeasureMark::Shape::Ring,
                                 .points = d.region,
                                 .labels = {"örtüşme " + domain::cadastre::square_metres(d.area)}});
        } else {
            std::string label = "tekrarlanan köşe";
            if (d.kind == Kind::Duplicate) label = "yinelenen";
            if (d.kind == Kind::ZeroLength) label = "boş";
            if (d.kind == Kind::ZeroArea) label = "sıfır alan";
            if (d.kind == Kind::SelfIntersecting) label = "kendini kesen sınır";
            if (d.kind == Kind::Overlap) label = "örtüşme";
            ctx.mark(MeasureMark{
                .shape = MeasureMark::Shape::Point, .points = {d.at}, .labels = {label}});
        }
    }

    // IT REPORTS, IT NEVER REPAIRS. A boundary is measured data; only the
    // surveyor decides what a defect means and only they can sign the result
    // (CLAUDE.md 6.11).
    said += "\n  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.";
    ctx.echo(said);
}

} // namespace

KENTOS_COMMAND(topology)
{
    return CommandSpec{
        .id       = "core.topology",
        .names    = {"TOPOLOJİ", "TOPOLOJI", "TOPOLOGY", "TPL"},
        .title    = "Topoloji Denetimi",
        .category = Category::Query,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim"}
                         .en("objects")},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly | Flags::LongRunning,
        .summary = "Kendini kesen sınır, sıfır alan ve örtüşen parselleri; yinelenen ve boş "
                   "nesneleri, tekrarlanan köşeleri ve çizgi ağındaki boşlukları raporlar.",
        .run = &run_topology,
    };
}

} // namespace kentos::command
