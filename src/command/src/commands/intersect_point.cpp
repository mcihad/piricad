// SPDX-License-Identifier: GPL-3.0-or-later
// core.intersect_point — KESİŞİMNOKTA, and core.point_along — ARANOKTA.
//
// Two constructions a surveyor does by hand more often than any other, in the
// interactive form: point at the inputs, get the point.
//
// KESİŞİMNOKTA re-establishes a corner three ways, which are the three ways a
// corner is recoverable when its monument is gone:
//   dogrultu — two bearings from two known points (a resection);
//   mesafe   — two tape measurements off two known monuments (a trilateration);
//   dogru    — where two boundaries WOULD meet, as infinite lines.
// The two-distance form has TWO answers and choosing one silently is what puts a
// boundary on the wrong side of a road, so `yon` is asked for and never guessed.
//
// ARANOKTA divides: a point at a ratio along a line, at a distance along it, or
// `sayi=k` points that cut it into k equal parts — the station pegs along a road
// centreline.
//
// THE ARITHMETIC IS SHARED with the `kes(...)`, `ara(...)` and `uzanti(...)`
// point functions of the one grammar (`construct.hpp`): the same answer, the
// same choice of the two solutions, and the same Turkish refusal whether the
// work was typed or clicked (CLAUDE.md 5.10, Article 1.2).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

// ------------------------------------------------------ KESİŞİMNOKTA ----

Task<void> run_intersect(Context& ctx)
{
    // The method decides what is asked for next, so it is read first. A word the
    // bus already validated against the declared list (R27).
    std::string how = "dogrultu";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };

    auto a = co_await ctx.point("birinci", is("dogru") ? "Birinci doğrunun ilk noktası"
                                                       : "Birinci bilinen nokta");
    if (!a) co_return;

    core::Result<core::Point2> found = core::err(core::ErrorCode::InvalidArgument, "");

    if (is("dogru")) {
        auto b = co_await ctx.point("ikinci", "Birinci doğrunun ikinci noktası",
                                    PointOptions{.rubber_band = true, .rubber_origin = *a});
        if (!b) co_return;
        auto c = co_await ctx.point("ucuncu", "İkinci doğrunun ilk noktası");
        if (!c) co_return;
        auto d = co_await ctx.point("dorduncu", "İkinci doğrunun ikinci noktası",
                                    PointOptions{.rubber_band = true, .rubber_origin = *c});
        if (!d) co_return;
        found = line_crossing(*a, *b, *c, *d);
    } else if (is("mesafe")) {
        auto r1 = co_await ctx.number("birinci_mesafe", "Birinci noktadan ölçülen uzaklık (m)");
        if (!r1) co_return;
        auto b = co_await ctx.point("ikinci", "İkinci bilinen nokta");
        if (!b) co_return;
        auto r2 = co_await ctx.number("ikinci_mesafe", "İkinci noktadan ölçülen uzaklık (m)");
        if (!r2) co_return;

        // TWO ANSWERS, AND THE USER SAYS WHICH. `sol` and `sağ` are read as the
        // side of the direction first→second, which is the same side
        // `core::circle_intersection` calls left.
        std::string side = "sol";
        if (const Value v = ctx.argument("yon"); !v.empty()) side = v.as_text();
        found = distance_crossing(*a, *r1, *b, *r2,
                                  core::turkish_key_equals(side, "sag") ? Side::Right : Side::Left);
    } else {
        auto angle_a = co_await ctx.number("birinci_aci", "Birinci noktadan okunan doğrultu");
        if (!angle_a) co_return;
        auto b = co_await ctx.point("ikinci", "İkinci bilinen nokta");
        if (!b) co_return;
        auto angle_b = co_await ctx.number("ikinci_aci", "İkinci noktadan okunan doğrultu");
        if (!angle_b) co_return;

        const core::AngleConvention convention = ctx.session().bus().angle_convention();
        found = direction_crossing(*a, *angle_a, convention, *b, *angle_b, convention);
    }

    if (!found) {
        ctx.session().fail(found.error());
        co_return;
    }

    auto created = ctx.transaction().add_point(ctx.active_layer(), found.value());
    if (!created) {
        ctx.session().fail(created.error());
        co_return;
    }
    ctx.record("kesisim", Value::point(found.value()));
    ctx.echo("Kesişim noktası yerleştirildi.");
}

// --------------------------------------------------------- ARANOKTA ----

Task<void> run_along(Context& ctx)
{
    auto a = co_await ctx.point("birinci", "Doğrunun ilk noktası");
    if (!a) co_return;
    auto b = co_await ctx.point("ikinci", "Doğrunun ikinci noktası",
                                PointOptions{.rubber_band = true, .rubber_origin = *a});
    if (!b) co_return;
    if (*a == *b) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "İki nokta aynı; üzerinde ara nokta bulunacak doğru yok."));
        co_return;
    }

    std::vector<core::Point2> placed;

    // `sayi=k` CUTS THE LINE INTO k EQUAL PARTS and places the k−1 points
    // BETWEEN them — the station pegs along a centreline. The ends are already
    // there; placing them again would leave two points on one monument.
    if (const Value v = ctx.argument("sayi"); !v.empty()) {
        const std::int64_t parts = v.as_int();
        for (std::int64_t i = 1; i < parts; ++i) {
            auto at = along_ratio(*a, *b, static_cast<double>(i) / static_cast<double>(parts));
            if (!at) {
                ctx.session().fail(at.error());
                co_return;
            }
            auto created = ctx.transaction().add_point(ctx.active_layer(), at.value());
            if (!created) {
                ctx.session().fail(created.error());
                co_return;
            }
            placed.push_back(at.value());
        }
        ctx.echo(std::to_string(placed.size()) + " ara nokta yerleştirildi (" +
                 std::to_string(parts) + " eşit parça).");
        co_return;
    }

    // Otherwise a run of readings, each one a ratio or a distance depending on
    // `yontem`. A loop, because pegging a line means a run of them.
    bool by_metres = false;
    if (const Value v = ctx.argument("yontem"); !v.empty())
        by_metres = core::turkish_key_equals(v.as_text(), "mesafe");

    while (true) {
        auto reading = co_await ctx.number("deger", by_metres ? "İlk noktadan uzaklık (m)"
                                                              : "Oran (0 ile 1 arası)");
        if (!reading) break;

        auto at = by_metres ? along_distance(*a, *b, *reading) : along_ratio(*a, *b, *reading);
        if (!at) {
            ctx.session().fail(at.error());
            co_return;
        }
        auto created = ctx.transaction().add_point(ctx.active_layer(), at.value());
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
        placed.push_back(at.value());
    }

    if (placed.empty()) co_return;
    ctx.echo(std::to_string(placed.size()) + " ara nokta yerleştirildi.");
}

} // namespace

KENTOS_COMMAND(intersect_point)
{
    return CommandSpec{
        .id       = "core.intersect_point",
        .names    = {"KESİŞİMNOKTA", "KESISIMNOKTA", "INTERSECTPT", "KSN"},
        .title    = "Kesişim Noktası",
        .category = Category::Draw,
        .params =
            {
                Param::choice("yontem", Arity::optional(), {"dogrultu", "mesafe", "dogru"},
                              "dogrultu: iki doğrultu · mesafe: iki uzaklık · dogru: iki doğru"),
                Param::point("birinci", "Birinci bilinen nokta"),
                Param::points("ikinci", Arity::optional(), "İkinci bilinen nokta"),
                Param::points("ucuncu", Arity::optional(), "İkinci doğrunun ilk noktası"),
                Param::points("dorduncu", Arity::optional(), "İkinci doğrunun ikinci noktası"),
                Param::number("birinci_aci", Arity::optional(), "Birinci noktadan okunan doğrultu"),
                Param::number("ikinci_aci", Arity::optional(), "İkinci noktadan okunan doğrultu"),
                Param::number("birinci_mesafe", Arity::optional(),
                              "Birinci noktadan ölçülen uzaklık (m)"),
                Param::number("ikinci_mesafe", Arity::optional(),
                              "İkinci noktadan ölçülen uzaklık (m)"),
                Param::choice("yon", Arity::optional(), {"sol", "sag"},
                              "İki uzaklık kesişiminin hangi çözümü; birinci→ikinci yönüne göre"),
                Param::points("kesisim", Arity::optional(), "Bulunan nokta; günlüğe yazılır"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar.",
        .run    = &run_intersect,
        .effect = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(point_along)
{
    return CommandSpec{
        .id       = "core.point_along",
        .names    = {"ARANOKTA", "POINTALONG", "ARN"},
        .title    = "Ara Nokta",
        .category = Category::Draw,
        .params =
            {
                Param::point("birinci", "Doğrunun ilk noktası"),
                Param::point("ikinci", "Doğrunun ikinci noktası"),
                Param::choice("yontem", Arity::optional(), {"oran", "mesafe"},
                              "oran: 0 ile 1 arası · mesafe: ilk noktadan metre"),
                Param::number("deger", Arity::at_least(0),
                              "Oran ya da uzaklık; birden çok verilebilir"),
                Param::integer_range("sayi", Arity::optional(), 2, 1000,
                                     "Doğruyu bu kadar eşit parçaya böler"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta "
                   "koyar.",
        .run    = &run_along,
        .effect = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
