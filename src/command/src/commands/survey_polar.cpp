// SPDX-License-Identifier: GPL-3.0-or-later
// core.survey_polar — ALIM. A field book reduced into a drawing.
//
// A total station standing on a known point reads, for every detail, an ANGLE
// and a DISTANCE. That pair is one line of the field book, and reducing a
// station's worth of them into coordinates by hand is where a day's work gets
// lost. This command is the reduction: station, optional backsight, then the
// readings.
//
// THE OTHER END OF `APLİKASYON`. Setting out produces the same two columns from
// coordinates; this produces coordinates from the two columns. Both go through
// `core/angle.hpp` — `polar_offset_turns` and `direction_turns` — so a point set
// out and the same point read back land on the same millimetre rather than on
// two roundings (CLAUDE.md 5.10).
//
// WITH A BACKSIGHT THE ANGLE IS RELATIVE, which is what an instrument zeroed on
// a known direction actually reads: the absolute direction is the backsight's
// plus the reading's. Without one the reading IS the azimuth. Which of the two
// it is changes every coordinate, so the command says which it used.
//
// GRAD AND CLOCKWISE FROM NORTH by default (`core.aci.birim`, `core.aci.kural`),
// because that is what a Turkish field book is written in and what `@mesafe<açı`
// reads too (TODOS-CAD P0).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    auto station = co_await ctx.point("istasyon", "Aletin durduğu nokta");
    if (!station) co_return; // ESC before anything was read

    // The unit and the rule the whole field book is written in, read once.
    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // THE BACKSIGHT IS OPTIONAL AND CHANGES WHAT EVERY ANGLE MEANS, so it is an
    // argument rather than a prompt: a run that asked for it would make a
    // station with no backsight two keystrokes longer, and a run that assumed one
    // would silently rotate the whole station.
    double zero_turns = 0.0;
    bool relative     = false;
    if (const Value v = ctx.argument("baglama"); !v.empty() && !v.as_points().empty()) {
        zero_turns = core::direction_turns(*station, v.as_points().front(), convention.rule);
        relative   = true;
    }

    bool join = false;
    if (const Value v = ctx.argument("cizgi"); !v.empty()) join = v.as_bool();

    // THE TWO RUNS MUST BE THE SAME LENGTH, and this is checked BEFORE a single
    // point is placed. Half a field book is worse than none: three `aci` and
    // two `kenar` is a line somebody mis-transcribed, and placing the two
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
        const std::size_t firsts  = ctx.argument("aci").as_numbers().size();
        const std::size_t seconds = ctx.argument("kenar").as_numbers().size();
        if (firsts != seconds)
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "`aci` ve `kenar` sayıca eşit olmalı ve sırayla eşleşir: " +
                    std::to_string(firsts) + " `aci`, " + std::to_string(seconds) +
                    " `kenar` geldi. Her okumayı adıyla verin: ALIM 0,0 aci=50 kenar=42.315"));
        if (firsts != seconds) co_return;
    }

    // THE STATION STAYS ON SCREEN while the field book is typed, and the
    // backsight with it when there is one. Neither is a document object — they
    // are what the run remembers — so the instrument's own position left the
    // screen the moment it was given, and every angle after that was read
    // against something invisible. `RubberShape::Fixed` is the preview for a
    // question the mouse is not answering.
    std::vector<core::Point2> setup{*station};
    if (const Value v = ctx.argument("baglama"); !v.empty() && !v.as_points().empty())
        setup.push_back(v.as_points().front());
    const auto standing = [&setup, &station] {
        return PointOptions{.rubber_band   = true,
                            .rubber_origin = *station,
                            .rubber_shape  = RubberShape::Fixed,
                            .rubber_chain  = setup};
    };

    std::vector<core::Point2> shot;
    while (true) {
        auto angle = co_await ctx.number(
            "aci", relative ? "Açı: bağlamadan itibaren okunan açı" : "Semt açısı", standing());
        if (!angle) break;
        auto distance = co_await ctx.number("kenar", "Kenar: alete olan uzaklık (m)", standing());
        if (!distance) break;
        if (*distance < 0.0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Kenar eksi olamaz; bir uzaklığın işareti yoktur."));
            co_return;
        }

        // The reading, in the session's unit, turned into a fraction of a turn
        // and added to the backsight's — turns add, angles in a unit do not.
        const double read_turns =
            core::turns_from_udeg(core::udeg_from_angle(*angle, convention.unit));
        const core::Point2 at = *station + core::polar_offset_turns(
                                               *distance, zero_turns + read_turns, convention.rule);

        auto created = ctx.transaction().add_point(ctx.active_layer(), at);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
        shot.push_back(at);
    }

    if (shot.empty()) co_return; // ESC before a single reading was given

    // JOINED IN THE ORDER THEY WERE READ, when asked. A boundary walked round
    // from one station is a run of readings and the order IS the ring.
    if (join && shot.size() >= 2) {
        auto line = ctx.transaction().add_polyline(ctx.active_layer(), shot);
        if (!line) {
            ctx.session().fail(line.error());
            co_return;
        }
    }

    ctx.echo(std::to_string(shot.size()) + " nokta alımdan hesaplandı (" +
             (relative ? "bağlamaya göre açı" : "semt açısı") + ", " +
             core::angle_rule_label(convention.rule) + ")" +
             (join && shot.size() >= 2 ? ", çizgiyle birleştirildi." : "."));
}

} // namespace

KENTOS_COMMAND(survey_polar)
{
    return CommandSpec{
        .id = "core.survey_polar",
        // `ALM`, NOT `AL`: `AL` is ALAN's abbreviation and has been since that
        // command shipped. Claiming it did not shadow ALAN — it DROPPED it. A
        // failed registration only writes a log line and carries on, so the
        // build was clean, the suite was green, and the command that draws a
        // parcel was gone. TODOS-CAD P1b-2 names `AL`; this supersedes it, for
        // the reason the plan's own §2 gives: one command list, one name.
        .names    = {"ALIM", "SURVEY", "ALM"},
        .title    = "Alım",
        .category = Category::Draw,
        .params =
            {
                Param::point("istasyon", "Aletin durduğu bilinen nokta").en("station"),
                // OPTIONAL, so a station with no backsight is not two keystrokes
                // longer. `Param::point` is exactly one; a point that may be
                // absent is declared the way `APLİKASYON` declares its own.
                Param::points("baglama", Arity::optional(),
                              "Bağlama noktası: verilirse açılar ondan itibaren okunmuş sayılır")
                    .en("backsight"),
                Param::number("aci", Arity::at_least(0), "Okunan açı; kenar ile sırayla eşleşir")
                    .measured_in("oturumun açı birimi")
                    .en("angle"),
                Param::number("kenar", Arity::at_least(0), "Alete olan uzaklık (m)")
                    .measured_in("m")
                    .en("distance"),
                Param::boolean("cizgi", Arity::optional(),
                               "Hesaplanan noktaları okundukları sırayla çizgiyle birleştirir")
                    .en("connect"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir.",
        .run     = &run,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
