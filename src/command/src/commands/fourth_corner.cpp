// SPDX-License-Identifier: GPL-3.0-or-later
// core.fourth_corner — DÖRDÜNCÜKÖŞE. The fourth corner of a building from three
// measured ones (netcad_plan.md N-13; Netcad's `4.Köşeyi Oluştur`, wiki 217385354).
//
// A FIELD SKETCH OF A BUILDING GIVES THREE CORNERS MORE OFTEN THAN FOUR: the
// fourth is behind a fence, under a tree, inside a neighbour's yard, or nobody
// walked round to it. Three corners fix a parallelogram, so the fourth is the
// third's offset from the middle one applied to the first — `a + c − b` — and
// what the command draws is the CLOSED FACE through all four, the way `ALAN`
// would have, with the fourth corner's coordinates said in the transcript
// because that is the figure the surveyor writes down.
//
// `dik=evet` IS FOR A BUILDING THAT IS SQUARE. Three measured corners are never
// exactly at a right angle, and drawing the parallelogram they make gives a
// footprint whose far wall leans by the measuring error. With `dik` the FIRST
// edge is trusted and the third corner is brought onto the perpendicular through
// the middle one — the same projection `DİKDÖRTGEN yontem=3n` makes of its
// third point (`core::edge_rectangle_corners`), so the guide, that method and
// this one agree — and the command SAYS how far the corner had to move and how
// far from a right angle the measurement was: a 3 cm misclosure is a note, a 30
// cm one is a wrong reading, and only the engineer can tell which.
//
// THE THREE CLICKED CORNERS ARE RECORDED, not the four that come out. Replaying
// the three through this command derives the same fourth, and a journal that
// held four could later have one of them moved and describe a face this command
// never draws (Article 1.4).
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/angle.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/polygon.hpp"
#include "piricad/core/trig.hpp"
#include "piricad/core/units.hpp"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// The angle between two arms at their vertex as whole micro-degrees in
/// [0, 180°], from the integer directions — exact, and the same on every
/// platform (`core::atan2_udeg`).
std::int64_t interior_udeg(core::Point2 vertex, core::Point2 first, core::Point2 second)
{
    const std::int64_t to_first  = core::atan2_udeg(first.y - vertex.y, first.x - vertex.x);
    const std::int64_t to_second = core::atan2_udeg(second.y - vertex.y, second.x - vertex.x);
    std::int64_t between         = (to_second - to_first) % core::kUDegFullCircle;
    if (between < 0) between += core::kUDegFullCircle;
    return between > core::kUDegFullCircle / 2 ? core::kUDegFullCircle - between : between;
}

Task<void> run(Context& ctx)
{
    // WHETHER THE RIGHT ANGLE WAS ASKED FOR, read before anything is awaited: it
    // decides what the third prompt previews, and it is a question of what was
    // GIVEN, not of who is asking (command.md P10).
    const Value square_arg = ctx.argument("dik");
    const bool square      = !square_arg.empty() && square_arg.as_bool();

    auto a = co_await ctx.point("noktalar", "Birinci köşe");
    if (!a) co_return; // ESC before anything was drawn

    auto b = co_await ctx.point(
        "noktalar", "İkinci köşe (birinci ile üçüncünün arasındaki)",
        PointOptions{.rubber_band = true, .rubber_origin = *a, .rubber_shape = RubberShape::Line});
    if (!b) co_return;

    // THE FIRST TWO CORNERS STAY ON SCREEN while the third is aimed. With `dik`
    // the ghost IS the rectangle that will be drawn (`edge_rectangle_corners`
    // is what both call); without it the triangle the three corners make, which
    // is half the parallelogram and the honest part of it to show before the
    // fourth corner exists.
    auto c = co_await ctx.point(
        "noktalar", "Üçüncü köşe",
        PointOptions{.rubber_band   = true,
                     .rubber_origin = *b,
                     .rubber_shape  = square ? RubberShape::EdgeRectangle : RubberShape::Ring,
                     .rubber_chain  = {*a, *b}});
    if (!c) co_return;

    if (*a == *b || *b == *c) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Yan yana iki köşe aynı nokta; bir kenar tanımlamıyor.");
        co_return;
    }

    std::array<core::Point2, 4> corners{};
    if (square) {
        if (!core::edge_rectangle_corners(*a, *b, *c, corners)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Üçüncü köşe birinci kenarın doğrultusunda; dik açı bir alan kapatmaz.");
            co_return;
        }
    } else {
        // COLLINEAR, in exact 128-bit arithmetic: a coordinate difference can be
        // tens of kilometres of millimetres and the product of two of them does
        // not fit in 64 bits.
        const core::Int128 cross = static_cast<core::Int128>(b->x - a->x) * (c->y - b->y) -
                                   static_cast<core::Int128>(b->y - a->y) * (c->x - b->x);
        if (cross == 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Üç köşe bir doğru üzerinde; dördüncü köşe bir alan kapatmaz.");
            co_return;
        }
        corners = {*a, *b, *c, core::fourth_corner(*a, *b, *c)};
    }

    std::vector<core::RingGeometry::RingInput> rings{core::RingGeometry::RingInput{
        std::span<const core::Point2>(corners), core::RingRole::Exterior, 0}};
    auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
    if (!created) {
        ctx.refuse(created.error());
        co_return; // the bus rolls the transaction back
    }

    // THE FOURTH CORNER, said in the surveyor's Y and X and at the precision the
    // project writes its coordinate tables with (`core.crs.hassasiyet`) — the
    // figure the engineer copies into the sketch.
    Bus& bus           = ctx.session().bus();
    const int decimals = static_cast<int>(bus.setting("core.crs.hassasiyet").as_int());
    const auto reading = [decimals](core::Mm v) {
        return core::metres_fixed(v, decimals, ',') + " m";
    };
    const core::Point2 fourth = corners[3];
    ctx.echo("Dördüncü köşe: Y " + reading(fourth.x) + "  X " + reading(fourth.y) +
             "; dört köşeli alan çizildi.");

    core::Json shape = core::Json::array({});
    for (const core::Point2 corner : corners)
        shape.push(Value::point(corner).to_json());

    core::Json report;
    report.set("dorduncu", Value::point(fourth).to_json());
    report.set("koseler", std::move(shape));
    report.set("dik", core::Json::boolean(square));

    if (square) {
        // THE DEVIATION, from the corners AS MEASURED: how far the third one had
        // to move to stand on the perpendicular, and how far from a right angle
        // the second one was. Both are worked out from the integer millimetres
        // the command was given, so the sentence is the same for every client.
        const core::Point2 moved_to  = corners[2];
        const core::Mm moved         = core::segment_length(*c, moved_to);
        const std::int64_t measured  = interior_udeg(*b, *a, *c);
        const std::int64_t off_right = measured - core::kUDegFullCircle / 4;
        const core::AngleUnit unit   = bus.angle_convention().unit;
        const auto angle_of          = [unit](std::int64_t udeg) {
            return core::angle_text(core::turns_from_udeg(udeg), unit);
        };
        const std::string sign    = off_right < 0 ? "-" : "+";
        const std::string measure = angle_of(measured);
        const std::string off     = sign + angle_of(off_right < 0 ? -off_right : off_right);

        report.set("sapma_mm", core::Json::integer(moved));
        report.set("kayma_mm",
                   Value::point(core::Point2{moved_to.x - c->x, moved_to.y - c->y}).to_json());
        report.set("olculen_aci_udeg", core::Json::integer(measured));
        report.set("aci_sapmasi_udeg", core::Json::integer(off_right));
        report.set("olculen_aci_metin", core::Json::string(measure));
        report.set("aci_sapmasi_metin", core::Json::string(off));

        if (moved == 0)
            ctx.echo("Dik açı: üçüncü köşe zaten dik açının üzerinde; sapma yok.");
        else
            ctx.echo("Dik açı dayatıldı: üçüncü köşe " + reading(moved) +
                     " kaydırıldı (ikinci köşedeki açı " + measure + " ölçülmüştü, sapma " + off +
                     ").");
    }
    ctx.report(std::move(report));

    // `dik` is in the line only when it was on: `dik=hayır` is the default spelled
    // out, and an empty answer takes the typed word back out of the record, so the
    // same drawing is one journal line whichever way it was asked for.
    ctx.record("noktalar", Value::points(Value::Points{*a, *b, *c}));
    ctx.record("dik", square ? Value::boolean(true) : Value{});
}

} // namespace

PIRICAD_COMMAND(fourth_corner)
{
    return CommandSpec{
        .id       = "core.fourth_corner",
        .names    = {"DÖRDÜNCÜKÖŞE", "DORDUNCUKOSE", "FOURTHCORNER", "DKÖ", "DKO"},
        .title    = "Dördüncü Köşe",
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::exactly(3),
                              "Üç köşe sırayla: birinci, ikinci (birinci ile üçüncünün "
                              "arasındaki) ve üçüncü; dördüncü ikincinin karşısına düşer")
                    .en("points"),
                Param::boolean("dik", Arity::optional(),
                               "evet: ikinci köşedeki açı dik yapılır — üçüncü köşe birinci "
                               "kenarın dikine çekilir ve sapma söylenir; varsayılan hayır: "
                               "üç köşenin paralelkenarı")
                    .en("right_angle"),
                Param::draw_layer(),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Üç köşeden dördüncü köşeyi hesaplar ve dört köşeli kapalı bir alan çizer; "
                   "dik=evet üçüncü köşeyi dik açıya çeker ve sapmayı söyler.",
        .run     = &run,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace piricad::command
