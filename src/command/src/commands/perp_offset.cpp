// SPDX-License-Identifier: GPL-3.0-or-later
// core.perp_offset — DİKAYAK. Detail off a baseline, the way a crew records it.
//
// Two known monuments make a baseline. Every detail between them is then two
// tape readings: how far ALONG the line from the first monument, and how far OUT
// to the side. That pair is the whole entry in a Turkish field book, and it is
// how a wall, a kerb, a pole or a building corner gets into a drawing when the
// only instrument is a tape and a prism pole.
//
// THE MATHS IS `core`'S, NOT THIS FILE'S. `dik(A,B,ayak,boy)` in the one grammar
// (`point_function.cpp`) and this command draw the same points from two
// different clients, so both call `core::perpendicular_offset` and neither
// carries its own copy of the sign convention. LEFT IS POSITIVE — Netcad's sign —
// and that is on the command page because it is the whole content of the
// function for the user.
//
// A LOOP, because a baseline is worth setting up only if a run of details comes
// off it. Each pair places a point; ESC and the right button end the run. With
// `cizgi=evet` the points are joined in the order they were given, which is what
// a kerb line or a building face is.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto a = co_await ctx.point("baslangic", "Taban çizgisinin ilk noktası");
    if (!a) co_return; // ESC before anything was asked

    auto b = co_await ctx.point("bitis", "Taban çizgisinin ikinci noktası",
                                PointOptions{.rubber_band = true, .rubber_origin = *a});
    if (!b) co_return;
    if (a->x == b->x && a->y == b->y) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Taban çizgisinin iki noktası aynı; dik indirilecek bir "
                                     "doğrultu yok."));
        co_return;
    }

    bool join = false;
    if (const Value v = ctx.argument("cizgi"); !v.empty()) join = v.as_bool();

    std::vector<core::Point2> placed;
    while (true) {
        // THE PAIR IS TWO ANSWERS, not one string to be split. A number is a
        // number in this program — an expression, a unit, a sign — and asking for
        // "30 -5" as text would have put a second parser in this file (5.11).
        auto foot = co_await ctx.number("ayak", "Ayak: taban üzerinde ilk noktadan uzaklık (m)");
        if (!foot) break;
        auto offset = co_await ctx.number("boy", "Boy: dik uzaklık (m, sol pozitif)");
        if (!offset) break;

        core::Point2 at{};
        if (!core::perpendicular_offset(*a, *b, core::mm_from_metres(*foot),
                                        core::mm_from_metres(*offset), at)) {
            // Unreachable while the two base points differ, which was checked
            // above; failing rather than asserting keeps a bad call a refusal.
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Taban doğrultusu hesaplanamadı."));
            co_return;
        }

        auto created = ctx.transaction().add_point(ctx.active_layer(), at);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
        placed.push_back(at);
    }

    if (placed.empty()) co_return; // ESC before a single pair was given

    // JOINED IN THE ORDER THEY WERE GIVEN. A kerb, a building face and a fence
    // are a run of details along one baseline, and the order the crew read them
    // in IS the shape. Two points are the fewest a line can have.
    if (join && placed.size() >= 2) {
        auto line = ctx.transaction().add_polyline(ctx.active_layer(), placed);
        if (!line) {
            ctx.session().fail(line.error());
            co_return;
        }
    }

    ctx.echo(std::to_string(placed.size()) + " nokta dik ayak/dik boy ile yerleştirildi" +
             (join && placed.size() >= 2 ? " ve çizgiyle birleştirildi." : "."));
}

} // namespace

KENTOS_COMMAND(perp_offset)
{
    return CommandSpec{
        .id       = "core.perp_offset",
        .names    = {"DİKAYAK", "DIKAYAK", "PERPOFFSET", "DA"},
        .title    = "Dik Ayak",
        .category = Category::Draw,
        .params =
            {
                Param::point("baslangic", "Taban çizgisinin ilk noktası (A)"),
                Param::point("bitis", "Taban çizgisinin ikinci noktası (B)"),
                Param::number("ayak", Arity::at_least(0),
                              "A'dan taban boyunca uzaklık (m); boy ile sırayla eşleşir"),
                Param::number("boy", Arity::at_least(0),
                              "Tabana dik uzaklık (m); A→B yönünde SOL pozitiftir"),
                Param::boolean("cizgi", Arity::optional(),
                               "Yerleştirilen noktaları verildikleri sırayla çizgiyle birleştirir"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir.",
        .run     = &run,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
