// SPDX-License-Identifier: GPL-3.0-or-later
// core.split_area — ALANİFRAZ. Cut a parcel to a TARGET AREA.
//
// "Bu parselden yola paralel 400 m² ayır" is the ifraz a surveyor is actually
// asked for. `İFRAZ` cuts where it is told and reports what came out; this one is
// given the answer and finds the cut.
//
// THE CUT IS PARALLEL TO A DIRECTION THE USER GIVES — a road frontage, an
// existing boundary, a plan line. That is what makes the search reliable: slide a
// line of fixed direction across the parcel and the area behind it grows
// monotonically, so bisection converges on the one offset that gives the target
// and cannot land on a second answer. A cut that rotated about a point would have
// no such guarantee, and a solver that sometimes finds a different valid answer is
// not something to compute a land record with.
//
// THE TOLERANCE IS A PARAMETER AND IT IS REPORTED. A target area is met to within
// something, never exactly — the boundary lands on a millimetre grid — and a
// command that printed the target instead of what it achieved would be lying
// about a number that goes on a tapu. What it reports is measured from the
// geometry it actually produced.
//
// PENDING SIGN-OFF (CLAUDE.md 6.11). What tolerance an ifraz may be accepted at,
// and which side keeps the parent's ada/parsel numbers, are regulatory questions
// that belong in /data and need a harita mühendisi. This command does the
// geometry and states what it did; it decides neither.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// How many bisection steps. A parcel is at most a few kilometres across, and 60
/// halvings of that is far below the millimetre the geometry stores — so the loop
/// is bounded by a constant rather than by a convergence test that could spin.
constexpr int kSteps = 60;

/// Default acceptance: a hundredth of a square metre. Stated so the report can
/// say what it was, and overridable because what an ifraz may be accepted at is
/// a regulatory question rather than this file's.
constexpr core::Mm2 kDefaultTolerance = 10000; // 0,01 m² in mm²

core::Mm2 abs_area(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

std::string square_metres(core::Mm2 v)
{
    const auto cm2   = static_cast<std::uint64_t>((abs_area(v) + 5000) / 10000);
    std::string frac = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

bool polygon_of(const core::Document& doc, core::EntityId slot, core::Polygon& out)
{
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

    out.exterior.clear();
    out.holes.clear();

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;

        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);

        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});

        if (geom.ring_role[r] == core::RingRole::Exterior && out.exterior.empty())
            out.exterior = std::move(ring);
        else if (ring.size() >= 3)
            out.holes.push_back(std::move(ring));
    }
    return out.exterior.size() >= 3;
}

/// A half-plane covering everything on the near side of the line that runs in
/// direction `(ux, uy)` and sits `offset` along the normal from `origin`.
core::Polygon half_plane(core::Point2 origin, double ux, double uy, double offset, double reach)
{
    const double nx = -uy; // the normal the offset is measured along
    const double ny = ux;

    // The rectangle's corners are pushed `reach` beyond the parcel in every
    // direction, so none of them can land inside it and every vertex of the
    // intersection below is either a parcel vertex or a point on the cut.
    const auto at = [&](double along, double across) {
        return core::Point2{origin.x + core::mm_round(ux * along + nx * across),
                            origin.y + core::mm_round(uy * along + ny * across)};
    };

    core::Polygon poly;
    poly.exterior = {at(-reach, offset - reach), at(reach, offset - reach), at(reach, offset),
                     at(-reach, offset)};
    return poly;
}

core::Mm2 area_of(const std::vector<core::Polygon>& pieces)
{
    core::Mm2 total = 0;
    for (const core::Polygon& p : pieces)
        total += abs_area(core::ring_area(p.exterior));
    return total;
}

Task<void> run(Context& ctx)
{
    const Selection& selection = ctx.session().bus().selection();

    Value::Ints requested = ctx.argument("nesneler").as_ids();
    if (requested.empty())
        for (core::EntityKey k : selection.keys())
            requested.push_back(static_cast<std::int64_t>(core::raw(k)));

    if (requested.size() != 1) {
        ctx.echo("Alana göre ifraz tek parsel üzerinde çalışır. Seçili: " +
                 std::to_string(requested.size()) + ".");
        co_return;
    }

    // The direction the cut runs in, as two points: a road frontage, an existing
    // boundary, a plan line. Two points rather than an angle, because that is what
    // is on the drawing and converting it to a bearing by hand is a step where a
    // quadrant gets lost.
    auto first = co_await ctx.point("yon", "Ayırma çizgisinin yönü: ilk nokta");
    if (!first) co_return;
    auto second = co_await ctx.point("yon", "Ayırma çizgisinin yönü: ikinci nokta",
                                     PointOptions{.rubber_band = true, .rubber_origin = *first});
    if (!second) co_return;

    const auto dx       = static_cast<double>(second->x - first->x);
    const auto dy       = static_cast<double>(second->y - first->y);
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0) {
        ctx.echo("Yön çizgisinin iki ucu aynı yerde; ayırma yönü belirsiz.");
        co_return;
    }
    const double ux = dx / length;
    const double uy = dy / length;

    const Value wanted = ctx.argument("alan");
    if (wanted.empty()) {
        ctx.echo("Ayrılacak alan eksik. Örnek: ALANİFRAZ alan=400000000 (400 m², mm² olarak)");
        co_return;
    }
    const auto target = static_cast<core::Mm2>(wanted.as_int());
    if (target <= 0) {
        ctx.echo("Ayrılacak alan sıfırdan büyük olmalı.");
        co_return;
    }

    core::Mm2 tolerance = kDefaultTolerance;
    if (const Value given = ctx.argument("tolerans"); !given.empty())
        tolerance = static_cast<core::Mm2>(given.as_int());
    if (tolerance <= 0) tolerance = kDefaultTolerance;

    const core::Document& doc = ctx.document();
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(requested.front()));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(requested.front()));
        co_return;
    }

    core::Polygon parcel;
    if (!polygon_of(doc, slot, parcel)) {
        ctx.echo("Nesne " + std::to_string(requested.front()) +
                 " kapalı bir alan değil; ifraz yalnız alanlar üzerinde çalışır.");
        co_return;
    }

    const core::Mm2 whole = abs_area(core::ring_area(parcel.exterior));
    if (target >= whole) {
        ctx.echo("İstenen alan (" + square_metres(target) + ") parselin tamamından (" +
                 square_metres(whole) + ") küçük olmalı.");
        co_return;
    }

    // ---- the search range, from the parcel's own extent along the normal ----
    const double nx = -uy;
    const double ny = ux;

    double low = 0.0, high = 0.0;
    bool first_vertex = true;
    double reach      = 0.0;
    for (const core::Point2& p : parcel.exterior) {
        const double t =
            (static_cast<double>(p.x - first->x)) * nx + (static_cast<double>(p.y - first->y)) * ny;
        const double along =
            (static_cast<double>(p.x - first->x)) * ux + (static_cast<double>(p.y - first->y)) * uy;
        if (first_vertex) {
            low = high   = t;
            first_vertex = false;
        }
        low   = std::min(low, t);
        high  = std::max(high, t);
        reach = std::max(reach, std::abs(along) + std::abs(t));
    }
    reach = reach * 2.0 + 1000.0;

    // ---- bisection ----
    //
    // The area behind the line grows monotonically as the line slides, so this
    // converges on THE offset rather than on one of several. Sixty halvings of a
    // parcel-sized range is far below a millimetre.
    std::vector<core::Polygon> kept;
    double lo = low, hi = high;
    for (int i = 0; i < kSteps; ++i) {
        const double mid = (lo + hi) * 0.5;

        auto side = core::polygon_boolean({parcel}, {half_plane(*first, ux, uy, mid, reach)},
                                          core::BooleanOp::Intersection);
        if (!side) {
            ctx.echo(side.error().message);
            co_return;
        }

        const core::Mm2 got = area_of(side.value());
        kept                = std::move(side.value());
        if (got < target)
            lo = mid;
        else
            hi = mid;
    }

    if (kept.empty()) {
        ctx.echo("Bu yönde istenen alan ayrılamıyor: parsel çizgiyi kesmiyor.");
        co_return;
    }

    const core::Mm2 achieved = area_of(kept);
    if (abs_area(achieved - target) > tolerance) {
        // REFUSED RATHER THAN ROUNDED. A target that cannot be met in this
        // direction is a fact about the parcel, and drawing a boundary that misses
        // it by more than the tolerance would put a wrong number on a tapu.
        ctx.echo("İstenen alana bu yönde ulaşılamadı. İstenen: " + square_metres(target) +
                 ", en yakın: " + square_metres(achieved) + ", tolerans: " +
                 square_metres(tolerance) + ". Yönü değiştirin ya da toleransı büyütün.");
        co_return;
    }

    // ---- the remainder ----
    auto rest = core::polygon_boolean({parcel}, kept, core::BooleanOp::Difference);
    if (!rest) {
        ctx.echo(rest.error().message);
        co_return;
    }

    // ---- write both sides, copy the attributes, remove the parent ----
    const core::AttrTable& table = doc.attributes();
    std::string said             = "Alana göre ifraz:";

    const auto emit = [&](const std::vector<core::Polygon>& pieces,
                          const char* label) -> core::Status {
        for (const core::Polygon& piece : pieces) {
            std::vector<core::RingGeometry::RingInput> rings;
            rings.push_back(
                core::RingGeometry::RingInput{piece.exterior, core::RingRole::Exterior, 0});
            for (const std::vector<core::Point2>& hole : piece.holes)
                rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});

            auto created = ctx.transaction().add_area(ctx.active_layer(), rings);
            if (!created) return created.error();

            for (std::size_t c = 0; c < table.columns(); ++c) {
                const auto col = static_cast<core::AttrId>(c);
                auto had       = doc.attribute(col, slot);
                if (!had || !had.value().present) continue;
                if (auto st = ctx.transaction().set_attribute(col, created.value(), had.value());
                    !st)
                    return st.error();
            }
            said +=
                std::string("\n  ") + label + ": " + square_metres(core::ring_area(piece.exterior));
        }
        return core::ok();
    };

    if (auto st = emit(kept, "ayrılan"); !st) {
        ctx.echo(st.error().message);
        co_return;
    }
    if (auto st = emit(rest.value(), "kalan"); !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    if (auto st = ctx.transaction().erase_entity(slot); !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    // WHAT WAS ACHIEVED, not what was asked for. The boundary lands on a
    // millimetre grid and the difference is what a surveyor checks.
    said += "\n  istenen " + square_metres(target) + ", elde edilen " + square_metres(achieved) +
            "  (fark " + square_metres(achieved - target) + ", tolerans " +
            square_metres(tolerance) + ")";
    said += "\n  toplam " + square_metres(achieved + area_of(rest.value())) +
            "  ·  ifrazdan önce " + square_metres(whole);

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("yon", Value::points({*first, *second}));
    ctx.record("alan", Value::integer(target));
    if (tolerance != kDefaultTolerance) ctx.record("tolerans", Value::integer(tolerance));
    ctx.echo(said);
}

} // namespace

KENTOS_COMMAND(split_area)
{
    return CommandSpec{
        .id       = "core.split_area",
        .names    = {"ALANİFRAZ", "ALANIFRAZ", "SPLITAREA", "ALİF"},
        .category = Category::Modify,
        .params =
            {
                Param::points("yon", Arity{0, 2},
                              "Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır)"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ayrılacak parsel; yoksa etkin seçim"},
                Param::integer("alan", Arity::optional(),
                               "Ayrılacak alan, mm² (400 m² = 400000000)"),
                Param::integer("tolerans", Arity::optional(),
                               "Kabul toleransı, mm²; varsayılan 10000 (0,01 m²)"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Parselden verilen yöne paralel, istenen alanda bir parça ayırır.",
        .run     = &run,
    };
}

} // namespace kentos::command
