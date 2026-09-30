// SPDX-License-Identifier: GPL-3.0-or-later
// core.station_offset — PRİZMA: where each point stands off a baseline, as dik
// ayak and dik boy (netcad_plan.md N-02; Netcad's Prizma, wiki 217385199).
//
// The question a survey crew asks of every detail it took off a baseline, and
// the reverse of `dik(A,B,ayak,boy)`: that function puts a point down from the
// two numbers, this reads the two numbers back from the point, with the same
// sign — RIGHT POSITIVE, looking from A to B (`core::station_offset`).
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/measure_mark.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/json.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/units.hpp"

#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// A length as a surveyor reads one: metres, three decimals, the Turkish comma.
std::string metres(core::Mm v)
{
    return core::metres_fixed(v, 3, ',') + " m";
}

core::Json point_json(core::Point2 p)
{
    core::Json out = core::Json::array({});
    out.push(core::Json::integer(p.x));
    out.push(core::Json::integer(p.y));
    return out;
}

Task<void> run_station_offset(Context& ctx)
{
    auto a = co_await ctx.point("baslangic", "Tabanın başlangıcı (A)");
    if (!a) co_return;
    auto b = co_await ctx.point("bitis", "Tabanın sonu (B)",
                                PointOptions{.rubber_band = true, .rubber_origin = *a});
    if (!b) co_return;
    if (*a == *b) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tabanın iki ucu aynı nokta; bir doğrultu tanımlamıyor.");
        co_return;
    }

    Bus& bus            = ctx.session().bus();
    const core::Mm base = core::segment_length(*a, *b);
    ctx.echo("PRİZMA: taban " + metres(base) +
             "; ayak A'dan B'ye, boy A'dan B'ye bakarken sağda pozitif, solda negatif.");
    ctx.mark(MeasureMark{MeasureMark::Shape::Run, {*a, *b}, {"taban " + metres(base)}});

    core::Json rows   = core::Json::array({});
    std::size_t asked = 0;
    for (;;) {
        // THE BASELINE STAYS ON SCREEN while the points are picked, as DİKAYAK
        // keeps it while its figures are typed: nothing follows the cursor, and
        // the answer's tick is drawn once the point is given.
        auto p = co_await ctx.point("noktalar", "Ölçülecek nokta (Enter bitirir)",
                                    PointOptions{.rubber_band   = true,
                                                 .rubber_origin = *a,
                                                 .rubber_shape  = RubberShape::Fixed,
                                                 .rubber_chain  = {*a, *b}});
        if (!p) break;
        ++asked;

        core::Mm foot   = 0;
        core::Mm offset = 0;
        (void)core::station_offset(*a, *b, *p, foot, offset);
        core::Point2 at{};
        (void)core::perpendicular_offset(*a, *b, foot, 0, at);

        const char* side = offset > 0 ? "sağda" : offset < 0 ? "solda" : "tabanın üstünde";
        const std::optional<std::string> number = bus.point_number_at(*p);
        ctx.echo("  " + (number ? *number + " numaralı nokta" : std::to_string(asked) + ". nokta") +
                 ": ayak " + metres(foot) + " · boy " + metres(offset) + " (" + side + ")");

        core::Json row;
        row.set("nokta", point_json(*p));
        row.set("ayak_mm", core::Json::integer(foot));
        row.set("boy_mm", core::Json::integer(offset));
        if (number) row.set("nokta_no", core::Json::string(*number));
        rows.push(std::move(row));

        // THE PERPENDICULAR, from its foot on the baseline out to the point,
        // with both figures on it — the tick a kroki draws.
        ctx.mark(MeasureMark{MeasureMark::Shape::Run,
                             {at, *p},
                             {"ayak " + metres(foot) + " · boy " + metres(offset)}});
    }
    if (asked == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "PRİZMA en az bir nokta ister: tabandan sonra ölçülecek noktaları verin.");
        co_return;
    }

    core::Json baseline;
    baseline.set("baslangic", point_json(*a));
    baseline.set("bitis", point_json(*b));
    baseline.set("uzunluk_mm", core::Json::integer(base));
    core::Json report;
    report.set("taban", std::move(baseline));
    report.set("noktalar", std::move(rows));
    ctx.report(std::move(report));

    ctx.record("baslangic", Value::point(*a));
    ctx.record("bitis", Value::point(*b));
}

} // namespace

PIRICAD_COMMAND(station_offset)
{
    return CommandSpec{
        .id       = "core.station_offset",
        .names    = {"PRİZMA", "PRIZMA", "STATIONOFFSET", "PRZ"},
        .title    = "Prizma (Dik Ayak ve Dik Boy)",
        .category = Category::Query,
        .params =
            {
                Param::point("baslangic", "Tabanın başlangıcı (A)").en("start"),
                Param::point("bitis", "Tabanın sonu (B)").en("end"),
                Param::points("noktalar", Arity::at_least(1),
                              "Dik ayağı ve dik boyu okunacak noktalar; boy A'dan B'ye bakarken "
                              "sağda pozitif")
                    .en("points"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly |
                   Flags::NoEffect,
        .summary = "Noktaların iki noktalı bir tabana göre dik ayağını ve dik boyunu okur; boy "
                   "sağda pozitif, solda negatiftir.",
        .run     = &run_station_offset,
        .effect  = Effect::Query,
    };
}

} // namespace piricad::command
