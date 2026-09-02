// SPDX-License-Identifier: GPL-3.0-or-later
// core.sector — DİLİM, and core.annulus — HALKA.
//
// Two closed shapes a round curve encloses, and the two a plan sheet asks for:
// a pie slice (a junction fillet area, a view cone, a sector of influence) and a
// ring (a protection band around a well, a buffer of fixed width around a point).
//
// STORED AS A FACE, NOT AS A DEFINITION, and that is the difference from DAİRE
// and YAY. A circle and an arc are exact — centre and radius — because their
// whole shape follows from two numbers. A sector's boundary is two straight radii
// AND a curve, and a ring's is two curves; both are ordinary rings once drawn,
// and inventing a stored kind for each would add two entity kinds to the model
// for shapes nothing else in the program needs to recognise (model.md).
//
// The curve itself comes from `core::arc_outline` and `core::circle_outline` —
// the same deterministic bisection the document is drawn with — so a sector's
// arc and a YAY drawn over it land on exactly the same vertices (§7.3).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The radius `centre`->`p` implies, rounded to the millimetre the record stores.
core::Mm radius_between(core::Point2 centre, core::Point2 p)
{
    const double dx = core::mm_to_metres(p.x - centre.x);
    const double dy = core::mm_to_metres(p.y - centre.y);
    return core::mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(core::kMmPerMetre));
}

std::vector<core::Point2> zip(const std::vector<core::Mm>& xs, const std::vector<core::Mm>& ys)
{
    std::vector<core::Point2> out;
    out.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size() && i < ys.size(); ++i)
        out.push_back(core::Point2{xs[i], ys[i]});
    return out;
}

// ------------------------------------------------------------------ DİLİM ---

Task<void> run_sector(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Dilimin merkezi");
    if (!centre) co_return; // ESC before anything was drawn

    auto start = co_await ctx.point("baslangic", "Dilimin ilk kenarı; yarıçapı bu belirler",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Circle});
    if (!start) co_return;

    const core::Mm radius = radius_between(*centre, *start);
    if (radius <= 0) {
        ctx.echo("İlk kenar merkezle aynı yerde; yarıçap sıfır olamaz.");
        co_return;
    }

    auto end = co_await ctx.point("bitis", "Dilimin ikinci kenarı (saat yönünün tersine)",
                                  PointOptions{.rubber_band   = true,
                                               .rubber_origin = *centre,
                                               .rubber_shape  = RubberShape::Arc,
                                               .rubber_chain  = {*start}});
    if (!end) co_return;

    if (end->x == centre->x && end->y == centre->y) {
        ctx.echo("İkinci kenar merkezle aynı yerde; dilimin nereye kadar gideceği belirsiz.");
        co_return;
    }

    std::vector<core::Mm> xs, ys;
    core::arc_outline(*centre, radius, *start, *end, xs, ys);

    // THE CENTRE CLOSES IT. The arc is the crust; the two radii are the straight
    // sides, and they exist because the ring returns to the centre between the
    // arc's last vertex and its first.
    std::vector<core::Point2> ring = zip(xs, ys);
    ring.push_back(*centre);

    if (ring.size() < 3) {
        ctx.echo("Bu iki kenar bir dilim kapatmıyor: süpürme sıfır.");
        co_return;
    }

    const core::RingGeometry::RingInput input{ring, core::RingRole::Exterior, 0};
    auto created = ctx.transaction().add_area(ctx.active_layer(), {&input, 1});
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    ctx.record("merkez", Value::point(*centre));
    ctx.record("baslangic", Value::point(*start));
    ctx.record("bitis", Value::point(*end));
}

// ------------------------------------------------------------------ HALKA ---

Task<void> run_annulus(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Halkanın merkezi");
    if (!centre) co_return;

    auto inner = co_await ctx.point("ic", "İç çember üzerinde bir nokta",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Circle});
    if (!inner) co_return;

    auto outer = co_await ctx.point("dis", "Dış çember üzerinde bir nokta",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Circle});
    if (!outer) co_return;

    core::Mm r_in  = radius_between(*centre, *inner);
    core::Mm r_out = radius_between(*centre, *outer);

    // GIVEN IN EITHER ORDER. A user who clicks the outside first has not made a
    // mistake, and refusing them would be pedantry: the smaller radius is the
    // hole whichever click produced it.
    if (r_in > r_out) std::swap(r_in, r_out);

    if (r_in <= 0) {
        ctx.echo("İç yarıçap sıfır: bu bir halka değil, daire. DAİRE komutunu kullanın.");
        co_return;
    }
    if (r_in == r_out) {
        ctx.echo("İki çember aynı: halkanın genişliği sıfır olamaz.");
        co_return;
    }

    std::vector<core::Mm> xs, ys;
    core::circle_outline(*centre, r_out, xs, ys);
    const std::vector<core::Point2> exterior = zip(xs, ys);

    xs.clear();
    ys.clear();
    core::circle_outline(*centre, r_in, xs, ys);
    const std::vector<core::Point2> hole = zip(xs, ys);

    const core::RingGeometry::RingInput rings[2]{
        {exterior, core::RingRole::Exterior, 0},
        {hole, core::RingRole::Interior, 0},
    };

    auto created = ctx.transaction().add_area(ctx.active_layer(), {rings, 2});
    if (!created) {
        ctx.echo(created.error().message);
        co_return;
    }

    ctx.record("merkez", Value::point(*centre));
    ctx.record("ic", Value::point(*inner));
    ctx.record("dis", Value::point(*outer));
}

} // namespace

KENTOS_COMMAND(sector)
{
    return CommandSpec{
        .id       = "core.sector",
        .names    = {"DİLİM", "DILIM", "SECTOR", "DL"},
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Dilimin merkezi"),
                Param::point("baslangic", "İlk kenarın ucu; yarıçapı bu belirler"),
                Param::point("bitis", "İkinci kenarın yönü; süpürme saat yönünün tersinedir"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.",
        .run     = &run_sector,
    };
}

KENTOS_COMMAND(annulus)
{
    return CommandSpec{
        .id       = "core.annulus",
        .names    = {"HALKA", "ANNULUS", "HLK"},
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Halkanın merkezi"),
                Param::point("ic", "İç çember üzerinde bir nokta"),
                Param::point("dis", "Dış çember üzerinde bir nokta"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez, iç ve dış yarıçaptan delikli halka çizer.",
        .run     = &run_annulus,
    };
}

} // namespace kentos::command
