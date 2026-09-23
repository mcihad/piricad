// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/ghost.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/spline.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace kentos::command {
namespace {

/// A run from a kind's flat outline arrays.
GhostRun zipped(const std::vector<core::Mm>& xs, const std::vector<core::Mm>& ys, bool closed)
{
    GhostRun run;
    run.closed = closed;
    run.points.reserve(xs.size());
    for (std::size_t i = 0; i < xs.size() && i < ys.size(); ++i)
        run.points.push_back(core::Point2{xs[i], ys[i]});
    return run;
}

/// The circle the kind draws for `centre` and `radius`, or nothing for a zero one.
std::vector<GhostRun> circle(core::Point2 centre, core::Mm radius)
{
    if (radius <= 0) return {};
    std::vector<core::Mm> xs;
    std::vector<core::Mm> ys;
    core::circle_outline(centre, radius, xs, ys);
    return {zipped(xs, ys, true)};
}

/// The arc the kind draws for its stored form.
std::vector<GhostRun> arc(core::Point2 centre, core::Mm radius, core::Point2 start,
                          core::Point2 end)
{
    if (radius <= 0) return {};
    std::vector<core::Mm> xs;
    std::vector<core::Mm> ys;
    core::arc_outline(centre, radius, start, end, xs, ys);
    return {zipped(xs, ys, false)};
}

/// The run so far and the point to come: the segments a run of ÇİZGİ writes, the
/// polyline ÇOKLUÇİZGİ writes, the face ALAN writes.
std::vector<GhostRun> chained(const Prompt& prompt, core::Point2 at, bool closed)
{
    GhostRun run;
    run.closed = closed;
    if (prompt.rubber_chain.empty()) {
        run.points = {prompt.rubber_origin, at};
    } else {
        run.points = prompt.rubber_chain;
        run.points.push_back(at);
    }
    return {std::move(run)};
}

} // namespace

std::vector<GhostRun> ghost_outline(const Prompt& prompt, core::Point2 at,
                                    core::AngleConvention convention)
{
    const core::Point2 origin = prompt.rubber_origin;
    const auto& chain         = prompt.rubber_chain;

    switch (prompt.rubber_shape) {
    case RubberShape::Line: return chained(prompt, at, false);
    case RubberShape::Ring: return chained(prompt, at, true);

    case RubberShape::Rectangle: {
        // `DİKDÖRTGEN`'s four corners, in its own order: a corner sharing an x or
        // a y with the first encloses nothing, and the command refuses it.
        if (origin.x == at.x || origin.y == at.y) return {};
        return {GhostRun{{origin, core::Point2{at.x, origin.y}, at, core::Point2{origin.x, at.y}},
                         true}};
    }

    case RubberShape::EdgeRectangle: {
        if (chain.size() < 2) return {};
        std::array<core::Point2, 4> four{};
        if (!core::edge_rectangle_corners(chain[0], chain[1], at, four)) return {};
        return {GhostRun{{four.begin(), four.end()}, true}};
    }

    case RubberShape::Circle:
        // The circle through the cursor — `DAİRE`'s rim, and the radius YAY and a
        // sector are choosing when their first end is asked for.
        return circle(origin, core::radius_through(origin, at));

    case RubberShape::Arc:
        if (chain.empty()) return circle(origin, core::radius_through(origin, at));
        // YAY's second end: the radius is the first end's, and the sweep runs
        // counter-clockwise from it to the cursor, as the arc is stored.
        return arc(origin, core::radius_through(origin, chain.front()), chain.front(), at);

    case RubberShape::CircleBuild: {
        const auto guide = core::decode_circle_guide(prompt.rubber_payload);
        if (!guide) return {};
        core::Point2 centre{};
        core::Mm radius = 0;
        if (!core::circle_from_guide(*guide, chain, at, centre, radius)) return {};
        return circle(centre, radius);
    }

    case RubberShape::ArcBuild: {
        const auto guide = core::decode_arc_guide(prompt.rubber_payload);
        if (!guide) return {};
        core::Point2 centre{};
        core::Mm radius = 0;
        core::Point2 first{};
        core::Point2 last{};
        if (!core::arc_from_guide(*guide, chain, at, centre, radius, first, last)) return {};
        return arc(centre, radius, first, last);
    }

    case RubberShape::ArcSweep: {
        // YAY bma, its sweep SHOWN: the angle the cursor sweeps from the start
        // round the centre, and the arc `core::arc_by_sweep` makes of it — the
        // conversion the click goes through (`Prompt::pick_sweep`) and the arc
        // the command then commits.
        if (chain.empty()) return {};
        const double sweep = core::arc_sweep_toward(origin, chain.front(), at, convention);
        core::Mm radius    = 0;
        core::Point2 first{};
        core::Point2 last{};
        if (!core::arc_by_sweep(origin, chain.front(), sweep, convention, radius, first, last))
            return {};
        return arc(origin, radius, first, last);
    }

    case RubberShape::Ellipse: {
        if (chain.empty()) return {};
        core::Point2 minor{};
        if (!core::ellipse_minor_end(origin, chain.front(), at, minor)) return {};
        std::vector<core::Mm> xs;
        std::vector<core::Mm> ys;
        // A PARTIAL ellipse when the prompt carries the sweep — the piece the
        // click will make, not the whole shape it is cut from.
        if (auto sweep = core::decode_ellipse_arc(prompt.rubber_payload)) {
            core::ellipse_arc_outline(origin, chain.front(), minor, sweep.value().start_udeg,
                                      sweep.value().end_udeg, xs, ys);
            return {zipped(xs, ys, false)};
        }
        core::ellipse_outline(origin, chain.front(), minor, xs, ys);
        return {zipped(xs, ys, true)};
    }

    case RubberShape::Polygon: {
        const auto guide = core::decode_polygon_guide(prompt.rubber_payload);
        if (!guide) return {};
        core::PolygonPick pick;
        if (!core::polygon_from_guide(*guide, origin, at, convention, pick)) return {};
        return {GhostRun{std::move(pick.corners), true}};
    }

    case RubberShape::Curve: {
        // The curve through the control points so far and the cursor, over the
        // degree SPLINE will use — lowered when there are too few points for it,
        // exactly as the command lowers it — at the kind's own density.
        std::vector<core::Point2> controls = chain;
        controls.push_back(at);
        if (controls.size() < 2) return {};
        core::SplineDef def;
        if (auto decoded = core::decode_spline(prompt.rubber_payload)) def = decoded.value();
        def.degree =
            static_cast<std::uint8_t>(std::clamp<std::size_t>(def.degree, 1, controls.size() - 1));
        def.knots_nano = core::uniform_clamped_knots(controls.size(), def.degree);
        std::vector<core::Mm> xs;
        std::vector<core::Mm> ys;
        core::spline_points(controls, def, core::kSplineSamplesPerSpan, xs, ys);
        if (xs.size() < 2) return {};
        return {zipped(xs, ys, def.closed)};
    }

    default: return {};
    }
}

} // namespace kentos::command
