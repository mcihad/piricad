// SPDX-License-Identifier: GPL-3.0-or-later
// core.point_draw — NOKTA. A surveyed point.
//
// A control point, a traverse station, a benchmark: a place, with nothing
// between it and anywhere else. Until `core.point` existed the document could not
// hold one — an open ring needs two vertices to be a line, and this is not a line
// — which is why `snap.hpp` reserved the DÜĞÜM snap bit and left it unused.
//
// WHAT A POINT LOOKS LIKE IS NOT DECIDED HERE. The kind emits one vertex and the
// symbology catalogue decides whether that draws as a cross, a triangle or a
// numbered monument (model.md R14). A command that drew a cross would be putting
// a gösterim in C++, which Article 5.13 forbids.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <string>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    std::size_t placed = 0;

    // A loop, because points arrive in runs: a surveyor loading a station's worth
    // of monuments places twenty without going back to the tool column. ESC ends
    // it, exactly as it ends every other open-ended draw command.
    while (auto at = co_await ctx.point("noktalar", "Nokta")) {
        auto created = ctx.transaction().add_point(ctx.active_layer(), *at);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
        ++placed;
    }

    if (placed == 0) co_return; // ESC before anything was placed

    ctx.echo(std::to_string(placed) + " nokta yerleştirildi.");
}

} // namespace

KENTOS_COMMAND(point_draw)
{
    return CommandSpec{
        .id       = "core.point_draw",
        .names    = {"NOKTA", "POINT", "NK"},
        .category = Category::Draw,
        .params   = {Param::points("noktalar", Arity::at_least(1),
                                   "Yerleştirilecek noktalar")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary  = "Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.",
        .run      = &run,
    };
}

} // namespace kentos::command
