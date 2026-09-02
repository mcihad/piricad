// SPDX-License-Identifier: GPL-3.0-or-later
// core.arc_draw — YAY. An arc from its centre and its two ends.
//
// The curve a road bend, a junction turn or a watercourse is actually made of.
// Like the circle it is stored as its DEFINITION — centre, exact radius, and the
// two measured ends — and the many-sided run is only what gets drawn
// (core/arc.hpp).
//
// THE SWEEP IS ALWAYS COUNTER-CLOCKWISE from the first end to the second, so the
// two ends given the other way round are the other arc of the same circle. That
// is the whole direction control: no flag, no "major arc" option, and no two
// records that could mean one picture.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::command {
namespace {

/// The radius `centre`->`p` implies, rounded to the millimetre the record stores.
core::Mm radius_between(core::Point2 centre, core::Point2 p)
{
    // Metres before squaring: the square of a TM3 coordinate difference in
    // millimetres leaves the 53-bit mantissa long before it leaves int64
    // (core.md R3). `double` is transient and never stored.
    const double dx = core::mm_to_metres(p.x - centre.x);
    const double dy = core::mm_to_metres(p.y - centre.y);
    return core::mm_round(std::sqrt(dx * dx + dy * dy) * static_cast<double>(core::kMmPerMetre));
}

Task<void> run(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Yayın merkezi");
    if (!centre) co_return; // ESC before anything was drawn

    // The first end fixes the radius, so the guide is the whole circle it lies on:
    // the user is choosing a radius here, and a radius is a circle.
    auto start = co_await ctx.point("baslangic", "Yayın başlangıç noktası",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *centre,
                                                 .rubber_shape  = RubberShape::Circle});
    if (!start) co_return;

    const core::Mm radius = radius_between(*centre, *start);
    if (radius <= 0) {
        ctx.echo("Başlangıç noktası merkezle aynı yerde; yarıçap sıfır olamaz.");
        co_return;
    }

    // The second end fixes the sweep, and the guide is the ARC that sweep makes.
    auto end = co_await ctx.point("bitis", "Yayın bitiş noktası (saat yönünün tersine)",
                                  PointOptions{.rubber_band   = true,
                                               .rubber_origin = *centre,
                                               .rubber_shape  = RubberShape::Arc,
                                               .rubber_chain  = {*start}});
    if (!end) co_return;

    if (end->x == centre->x && end->y == centre->y) {
        ctx.echo("Bitiş noktası merkezle aynı yerde; yayın nereye kadar gideceği belirsiz.");
        co_return;
    }

    auto created = ctx.transaction().add_arc(ctx.active_layer(), *centre, radius, *start, *end);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // RECORDED AS IT WAS ASKED. The radius is what the centre and the first end
    // MEAN; recording the derived number instead would let a replay disagree with
    // the run that produced it.
    ctx.record("merkez", Value::point(*centre));
    ctx.record("baslangic", Value::point(*start));
    ctx.record("bitis", Value::point(*end));
}

} // namespace

KENTOS_COMMAND(arc_draw)
{
    return CommandSpec{
        .id       = "core.arc_draw",
        .names    = {"YAY", "ARC", "YY"},
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Yayın merkezi"),
                Param::point("baslangic", "Yayın başlangıç noktası; yarıçapı bu belirler"),
                Param::point("bitis", "Yayın bitiş yönü; süpürme saat yönünün tersinedir"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir.",
        .run     = &run,
    };
}

} // namespace kentos::command
