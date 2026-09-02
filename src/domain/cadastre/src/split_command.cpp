// SPDX-License-Identifier: GPL-3.0-or-later
// core.split_parcel — İFRAZ. One parcel becomes two along a cutting line.
//
// THE CUT IS A STRAIGHT LINE THROUGH THE PARCEL, given as two points. That is
// what an ifraz almost always is on a sheet — a boundary agreed between the
// parties or laid down by a plan — and it is the one form whose result is exact:
// the line is extended past the parcel and the two half-planes are intersected
// with it, so no band is subtracted and no area is lost. A cut described by a
// polyline is a different problem and is not attempted here rather than being
// approximated, because a few square centimetres missing from a parsel is a few
// square centimetres somebody owns.
//
// AREAS ARE REPORTED, NEVER TARGETED. "Split this parcel into 400 m² and the
// rest" is an ifraz by area, it needs an iteration and a tolerance, and both of
// those are regulatory decisions (6.11, 5.13). This command cuts where it is told
// and prints what came out, so the surveyor can check it against the plan.
//
// THE ATTRIBUTES ARE COPIED TO BOTH SIDES, unchanged. That is not a rule about
// what an ifraz means — the new ada/parsel numbers come from TKGM — it is the
// only non-destructive thing to do with what was there, and the command says so.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/entity_kind.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/offset.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

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

/// A rectangle covering one side of the line a->b, big enough to contain `box`.
///
/// The line is extended and widened well past the parcel, so the intersection
/// below is exact: every vertex of the result is either a parcel vertex or a
/// point on the cut, and none of them came from the rectangle's own corners.
core::Polygon half_plane(core::Point2 a, core::Point2 b, const core::Box2& box, bool left)
{
    // Reach: the box's diagonal, doubled. Anything at least that long puts the
    // rectangle's own corners outside the parcel whatever angle the cut is at.
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    const double len = std::sqrt(dx * dx + dy * dy);

    const double wx = static_cast<double>(box.max_x - box.min_x);
    const double wy = static_cast<double>(box.max_y - box.min_y);
    const double reach = 2.0 * (std::sqrt(wx * wx + wy * wy) + 1000.0);

    const double ux = dx / len; // along the cut
    const double uy = dy / len;
    const double nx = left ? -uy : uy; // and away from it, to one side
    const double ny = left ? ux : -ux;

    const auto at = [&](double along, double across) {
        return core::Point2{a.x + core::mm_round(ux * along + nx * across),
                            a.y + core::mm_round(uy * along + ny * across)};
    };

    core::Polygon poly;
    poly.exterior = {at(-reach, 0.0), at(len + reach, 0.0), at(len + reach, reach),
                     at(-reach, reach)};
    return poly;
}

core::Mm2 abs_area(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

std::string square_metres(core::Mm2 v)
{
    const auto cm2   = static_cast<std::uint64_t>((abs_area(v) + 5000) / 10000);
    std::string frac = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

Task<void> run(Context& ctx)
{
    const Selection& selection = ctx.session().bus().selection();

    Value::Ints requested = ctx.argument("nesneler").as_ids();
    if (requested.empty())
        for (core::EntityKey k : selection.keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

    if (requested.size() != 1) {
        ctx.echo("İfraz tek parsel üzerinde çalışır. Seçili: " +
                 std::to_string(requested.size()) + ".");
        co_return;
    }

    auto first = co_await ctx.point("noktalar", "Ayırma çizgisinin ilk noktası");
    if (!first) co_return; // ESC before anything was cut

    auto second = co_await ctx.point("noktalar", "Ayırma çizgisinin ikinci noktası",
                                     PointOptions{.rubber_band = true, .rubber_origin = *first});
    if (!second) co_return;

    if (first->x == second->x && first->y == second->y) {
        ctx.echo("Ayırma çizgisinin iki ucu aynı yerde; kesme yönü belirsiz.");
        co_return;
    }

    const core::Document& doc = ctx.document();
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(requested.front()));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(requested.front()));
        co_return;
    }

    core::Polygon parcel;
    if (!polygon_of(doc, slot, parcel)) {
        ctx.echo("Nesne " + std::to_string(requested.front()) +
                 " kapalı bir alan değil; ifraz yalnız alanlar üzerinde çalışır.");
        co_return;
    }

    core::Box2 box;
    for (const core::Point2& p : parcel.exterior) box.extend(p);

    const core::Mm2 before = abs_area(core::ring_area(parcel.exterior));

    std::vector<core::Polygon> pieces;
    for (bool left : {true, false}) {
        const core::Polygon side = half_plane(*first, *second, box, left);
        auto part = core::polygon_boolean({parcel}, {side}, core::BooleanOp::Intersection);
        if (!part) {
            ctx.echo(part.error().message);
            co_return;
        }
        for (core::Polygon& piece : part.value())
            pieces.push_back(std::move(piece));
    }

    if (pieces.size() < 2) {
        ctx.echo("Bu çizgi parseli kesmiyor: ifraz için çizginin parselin içinden geçmesi "
                 "gerekir.");
        co_return;
    }

    // ---- write the pieces, copy the attributes, remove the original ----
    const core::AttrTable& table = doc.attributes();
    std::string said = "İfraz: " + std::to_string(pieces.size()) + " parça.";

    for (const core::Polygon& piece : pieces) {
        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{piece.exterior, core::RingRole::Exterior, 0});
        for (const std::vector<core::Point2>& hole : piece.holes)
            rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});

        auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
        if (!created) {
            ctx.echo(created.error().message);
            co_return;
        }

        for (std::size_t c = 0; c < table.columns(); ++c) {
            const auto col = static_cast<core::AttrId>(c);
            auto had       = doc.attribute(col, slot);
            if (!had || !had.value().present) continue;
            if (auto st = ctx.transaction().set_attribute(col, created.value(), had.value()); !st) {
                ctx.echo(st.error().message);
                co_return;
            }
        }

        said += "\n  " + square_metres(core::ring_area(piece.exterior));
    }

    if (auto st = ctx.transaction().erase_entity(slot); !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    // THE SUM IS PRINTED because it is what a surveyor checks first: an ifraz that
    // loses area has cut something it should not have, and a difference of a few
    // square centimetres is a few square centimetres somebody owns.
    core::Mm2 after = 0;
    for (const core::Polygon& piece : pieces) after += abs_area(core::ring_area(piece.exterior));
    said += "\n  toplam " + square_metres(after) + "  ·  ifrazdan önce " + square_metres(before);

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("noktalar", Value::points({*first, *second}));
    ctx.echo(said);
}

} // namespace

PIRICAD_COMMAND(split_parcel)
{
    return CommandSpec{
        .id       = "core.split_parcel",
        .names    = {"İFRAZ", "IFRAZ", "SUBDIVIDE", "İFR"},
        .category = Category::Modify,
        .params =
            {
                // `noktalar` FIRST, and the order is load bearing. A bare token
                // after a named argument is positional, and the parser hands
                // positionals to the declared params in order — with the
                // unbounded `nesneler` first, the cutting line's second point was
                // swallowed as an object id and İFRAZ waited forever for a point
                // it had already been given.
                Param::points("noktalar", Arity{0, 2}, "Ayırma çizgisinin iki ucu"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ayrılacak parsel; yoksa etkin seçim"},
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).",
        .run     = &run,
    };
}

} // namespace piricad::command
