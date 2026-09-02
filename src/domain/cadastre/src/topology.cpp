// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/cadastre/topology.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

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

std::vector<Defect> check_topology(const core::Document& doc,
                                   const std::vector<core::EntityKey>& keys)
{
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

    std::vector<core::Polygon> polys;
    std::vector<core::EntityKey> owners;
    polys.reserve(slots.size());
    owners.reserve(slots.size());

    for (core::EntityId slot : slots) {
        core::Polygon poly;
        if (!polygon_of(doc, slot, poly)) continue; // not an area; nothing to check

        const core::EntityKey key = doc.entities().key[slot];

        if (abs_area(core::ring_area(poly.exterior)) == 0) {
            found.push_back(Defect{DefectKind::ZeroArea, key, core::EntityKey::None, 0});
            continue;
        }
        if (self_intersecting(poly)) {
            found.push_back(Defect{DefectKind::SelfIntersecting, key, core::EntityKey::None, 0});
            continue;
        }

        polys.push_back(std::move(poly));
        owners.push_back(key);
    }

    // ---- pairwise overlap ----
    //
    // Bounded by the bounding boxes first. Two parcels whose boxes do not touch
    // cannot share ground, and the box test is four comparisons against a boolean
    // that is orders of magnitude dearer — which is what keeps a sheet with
    // thousands of parcels on it finishing.
    for (std::size_t i = 0; i < polys.size(); ++i) {
        core::Box2 bi;
        for (const core::Point2& p : polys[i].exterior) bi.extend(p);

        for (std::size_t j = i + 1; j < polys.size(); ++j) {
            core::Box2 bj;
            for (const core::Point2& p : polys[j].exterior) bj.extend(p);

            if (bi.max_x < bj.min_x || bj.max_x < bi.min_x) continue;
            if (bi.max_y < bj.min_y || bj.max_y < bi.min_y) continue;

            auto shared = core::polygon_boolean({polys[i]}, {polys[j]},
                                                core::BooleanOp::Intersection);
            if (!shared || shared.value().empty()) continue;

            core::Mm2 total = 0;
            for (const core::Polygon& piece : shared.value())
                total += abs_area(core::ring_area(piece.exterior));

            // A SQUARE MILLIMETRE OF SLACK. Two parcels that share a boundary
            // meet along it, and rounding a shared vertex to the millimetre can
            // leave a sliver a few square millimetres wide. Reporting that as an
            // overlap would bury the real ones.
            if (total > 1000) {
                found.push_back(Defect{DefectKind::Overlap, owners[i], owners[j], total});
            }
        }
    }

    return found;
}

std::string describe(const core::Document& doc, const Defect& d)
{
    (void)doc;
    const auto first  = static_cast<unsigned long long>(core::raw(d.first));
    const auto second = static_cast<unsigned long long>(core::raw(d.second));

    switch (d.kind) {
    case DefectKind::SelfIntersecting:
        return "Nesne " + std::to_string(first) + ": sınır kendini kesiyor.";
    case DefectKind::ZeroArea:
        return "Nesne " + std::to_string(first) + ": alanı sıfır.";
    case DefectKind::Overlap: {
        // Square metres to two decimals, in integers, the way ALANÖLÇ prints one.
        const auto cm2   = static_cast<std::uint64_t>((d.area + 5000) / 10000);
        std::string frac = std::to_string(cm2 % 100);
        if (frac.size() < 2) frac = "0" + frac;
        return "Nesne " + std::to_string(first) + " ile " + std::to_string(second) +
               " örtüşüyor: " + std::to_string(cm2 / 100) + "," + frac + " m².";
    }
    }
    return "Bilinmeyen kusur.";
}

} // namespace kentos::domain::cadastre

// ---------------------------------------------------------------- TOPOLOJİ ---

namespace kentos::command {
namespace {

Task<void> run_topology(Context& ctx)
{
    const Selection& selection = ctx.session().bus().selection();

    std::vector<core::EntityKey> keys;
    for (std::int64_t raw : ctx.argument("nesneler").as_ids())
        keys.push_back(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
    if (keys.empty())
        for (core::EntityKey k : selection.keys())
            keys.push_back(k);

    const core::Document& doc = ctx.document();
    const auto found          = domain::cadastre::check_topology(doc, keys);

    // WHAT WAS CHECKED, always. "No defects" is only reassuring if the user knows
    // how much was looked at; a report that says nothing about scope is a report
    // that could have checked one parcel.
    const std::string scope =
        keys.empty() ? "bütün çizim" : (std::to_string(keys.size()) + " nesne");

    if (found.empty()) {
        ctx.echo("Topoloji denetimi (" + scope + "): kusur bulunamadı.");
        co_return;
    }

    std::string said =
        "Topoloji denetimi (" + scope + "): " + std::to_string(found.size()) + " kusur.";
    for (const domain::cadastre::Defect& d : found)
        said += "\n  " + domain::cadastre::describe(doc, d);

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
        .category = Category::Query,
        .params   = {Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim"}},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary  = "Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar.",
        .run      = &run_topology,
    };
}

} // namespace kentos::command
