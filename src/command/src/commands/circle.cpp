// SPDX-License-Identifier: GPL-3.0-or-later
// core.circle_draw — DAİRE. A circle from its centre and a point on its rim.
//
// The first curve this program can draw, and the reason it is a KIND rather than
// a many-sided polygon: a circle stored as its picture has a circumference that
// is not 2·pi·r and an area that is not pi·r², and on a cadastral sheet those are
// the numbers that reach the tapu (§12). What is stored is the centre and the
// radius; the 128-gon is only what gets drawn (core/circle.hpp).
//
// The rim point is asked for rather than a typed radius because that is how a
// circle is drawn with a mouse, and because every input aid then applies to it
// for free: snapping the rim to a parsel corner puts the circle exactly through
// that corner, which is what a çekme mesafesi or a monument radius needs.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto centre = co_await ctx.point("merkez", "Dairenin merkezi");
    if (!centre) co_return; // ESC before anything was drawn

    // The guide runs from the centre, so the user sees the radius they are about
    // to fix rather than a line from nowhere.
    // THE CIRCLE ITSELF, not a radius line. A guide is the only thing that tells
    // a user what the next click will make before they make it (input.hpp), and a
    // circle previewed as a line says nothing about the circle.
    auto rim = co_await ctx.point("cevre", "Çember üzerinde bir nokta",
                                  PointOptions{.rubber_band   = true,
                                               .rubber_origin = *centre,
                                               .rubber_shape  = RubberShape::Circle});
    if (!rim) co_return;

    // Metres before squaring: the square of a TM3 coordinate difference in
    // millimetres leaves the 53-bit mantissa long before it leaves int64, and
    // translating to the centre first is what keeps the operands small
    // (core.md R3). `double` is transient and never stored.
    const double dx       = core::mm_to_metres(rim->x - centre->x);
    const double dy       = core::mm_to_metres(rim->y - centre->y);
    const core::Mm radius = core::mm_round(std::sqrt(dx * dx + dy * dy) * core::kMmPerMetre);

    if (radius <= 0) {
        ctx.echo("Çember noktası merkezle aynı yerde; yarıçap sıfır olamaz.");
        co_return;
    }

    auto created = ctx.transaction().add_circle(ctx.active_layer(), *centre, radius);
    if (!created) {
        ctx.echo(created.error().message);
        co_return; // the bus rolls the transaction back
    }

    // RECORDED AS IT WAS ASKED, so a replay draws the same circle: the rim point
    // and not the radius, because the radius is what the two points MEAN and
    // recording a derived number would let a replay disagree with the run.
    ctx.record("merkez", Value::point(*centre));
    ctx.record("cevre", Value::point(*rim));
}

} // namespace

KENTOS_COMMAND(circle_draw)
{
    return CommandSpec{
        .id       = "core.circle_draw",
        .names    = {"DAİRE", "DAIRE", "CIRCLE", "DR"},
        .category = Category::Draw,
        .params =
            {
                Param::point("merkez", "Dairenin merkezi"),
                Param::point("cevre", "Çember üzerinde bir nokta; yarıçapı bu belirler"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Merkez ve çember üzerindeki bir noktadan daire çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
