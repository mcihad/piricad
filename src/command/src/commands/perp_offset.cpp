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

    // THE TWO RUNS MUST BE THE SAME LENGTH, and this is checked BEFORE a single
    // point is placed. Half a field book is worse than none: three `ayak` and
    // two `boy` is a line somebody mis-transcribed, and placing the two
    // whole pairs and dropping the third silently is how a detail disappears
    // from a survey (Article 1.6 — a validation failure rolls the whole thing
    // back).
    //
    // It also catches the commonest positional mistake. The two runs are declared
    // one after the other, so bare numbers all bind to the FIRST of them and the
    // second is left empty; the command used to return in SILENCE — no point, no
    // reason — which is the worst answer a command has, because the caller has
    // nothing to correct. The message names the pairing rule and the counts.
    {
        const std::size_t firsts  = ctx.argument("ayak").as_numbers().size();
        const std::size_t seconds = ctx.argument("boy").as_numbers().size();
        if (firsts != seconds)
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "`ayak` ve `boy` sayıca eşit olmalı ve sırayla eşleşir: " + std::to_string(firsts) +
                    " `ayak`, " + std::to_string(seconds) +
                    " `boy` geldi. Her okumayı adıyla verin: DİKAYAK 0,0 100,0 ayak=30 boy=-5"));
        if (firsts != seconds) co_return;
    }

    // THE BASELINE STAYS ON SCREEN while the readings are typed. It is not a
    // document object — it is two points the run remembers — so once the second
    // one was given it left the screen entirely, and the user was typing `ayak`
    // and `boy` against a baseline they could no longer see. `RubberShape::Fixed`
    // is the preview for a question the mouse is not answering.
    const std::vector<core::Point2> baseline{*a, *b};

    std::vector<core::Point2> placed;
    while (true) {
        // THE PAIR IS TWO ANSWERS, not one string to be split. A number is a
        // number in this program — an expression, a unit, a sign — and asking for
        // "30 -5" as text would have put a second parser in this file (5.11).
        auto foot = co_await ctx.number("ayak", "Ayak: taban üzerinde ilk noktadan uzaklık (m)",
                                        PointOptions{.rubber_band   = true,
                                                     .rubber_origin = *a,
                                                     .rubber_shape  = RubberShape::Fixed,
                                                     .rubber_chain  = baseline});
        if (!foot) break;

        // AND ONCE THE FOOT IS KNOWN it joins the picture, because the offset is
        // measured from THERE and that is the thing the next number is about.
        std::vector<core::Point2> with_foot = baseline;
        if (core::Point2 on_base{};
            core::perpendicular_offset(*a, *b, core::mm_from_metres(*foot), 0, on_base))
            with_foot.push_back(on_base);

        auto offset = co_await ctx.number("boy", "Boy: dik uzaklık (m, sol pozitif)",
                                          PointOptions{.rubber_band   = true,
                                                       .rubber_origin = *a,
                                                       .rubber_shape  = RubberShape::Fixed,
                                                       .rubber_chain  = std::move(with_foot)});
        if (!offset) break;

        core::Point2 at{};
        if (!core::perpendicular_offset(*a, *b, core::mm_from_metres(*foot),
                                        core::mm_from_metres(*offset), at)) {
            // Unreachable while the two base points differ, which was checked
            // above; failing rather than asserting keeps a bad call a refusal.
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Taban doğrultusu hesaplanamadı."));
            co_return;
        }

        auto created = ctx.transaction().add_point(ctx.active_layer(), at);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
        placed.push_back(at);
    }

    if (placed.empty()) co_return; // ESC before a single reading was given

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
