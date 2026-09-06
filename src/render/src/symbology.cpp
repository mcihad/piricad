// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/symbology.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace kentos::render {
namespace {

constexpr double kPi = 3.14159265358979323846;

/// How many segments a circular arc is cut into.
///
/// 24 for a full turn, which is 15 degrees a segment. A marker is a handful of
/// pixels across on a plan sheet and the error at 24 sides is under a thousandth
/// of the radius — below what a printer resolves, let alone a screen.
constexpr int kArcSteps = 24;

void push(MarkerOutline& out, double x, double y)
{
    out.xs.push_back(static_cast<float>(x));
    out.ys.push_back(static_cast<float>(y));
}

void close_run(MarkerOutline& out, std::uint32_t count, bool closed)
{
    out.runs.push_back(count);
    out.closed.push_back(closed ? 1u : 0u);
}

} // namespace

void MarkerOutline::clear()
{
    xs.clear();
    ys.clear();
    runs.clear();
    closed.clear();
}

// -----------------------------------------------------------------------------

void place_along_run(const float* xs, const float* ys, std::uint32_t count,
                     core::MarkerPlacement placement, double interval, double phase,
                     const PixelBox& clip, std::vector<Stamp>& out)
{
    if (count < 1 || xs == nullptr || ys == nullptr) return;

    // Outside this, a stamp cannot appear on screen. The caller has already grown
    // the box by the glyph's own reach, so a mark whose centre is just off the
    // edge but whose arm crosses it is still kept.
    const bool bounded = !clip.empty();
    const auto seen    = [&](double x, double y) {
        return !bounded ||
               (x >= static_cast<double>(clip.min_x) && x <= static_cast<double>(clip.max_x) &&
                y >= static_cast<double>(clip.min_y) && y <= static_cast<double>(clip.max_y));
    };

    const auto at = [&](std::uint32_t v) {
        return std::pair<double, double>{static_cast<double>(xs[v]), static_cast<double>(ys[v])};
    };

    const auto direction = [&](std::uint32_t a, std::uint32_t b) {
        const double dx  = static_cast<double>(xs[b]) - static_cast<double>(xs[a]);
        const double dy  = static_cast<double>(ys[b]) - static_cast<double>(ys[a]);
        const double len = std::hypot(dx, dy);
        return len > 0.0 ? std::pair<double, double>{dx / len, dy / len}
                         : std::pair<double, double>{1.0, 0.0};
    };

    const auto stamp = [&](std::pair<double, double> p, std::pair<double, double> d) {
        if (!seen(p.first, p.second)) return;
        out.push_back(Stamp{static_cast<float>(p.first), static_cast<float>(p.second),
                            static_cast<float>(d.first), static_cast<float>(d.second)});
    };

    using core::MarkerPlacement;

    // A LONE VERTEX IS A POINT, and it was getting nothing at all.
    //
    // `count < 2` used to leave here empty-handed, which is right for anything
    // measured ALONG a run — there is no length and no direction to space marks
    // out over. But a NOKTA outlines to exactly one vertex (`point_outline_fn`),
    // and a marker on it is not a mark along a line: it IS the object. Refusing
    // it meant every surveyed point was invisible on the canvas whatever style
    // it carried.
    //
    // Upright, because there is no direction to turn to and inventing one would
    // rotate every röper by whatever the last segment happened to be.
    if (count == 1) {
        stamp(at(0), {1.0, 0.0});
        return;
    }

    if (placement == MarkerPlacement::Vertex) {
        // Upright at each vertex, not turned to the corner. A vertex marker says
        // "a point is here"; rotating it to the average of two segments makes it
        // say something about the corner's shape instead.
        for (std::uint32_t v = 0; v < count; ++v)
            stamp(at(v), {1.0, 0.0});
        return;
    }
    if (placement == MarkerPlacement::FirstVertex) {
        stamp(at(0), direction(0, 1));
        return;
    }
    if (placement == MarkerPlacement::LastVertex) {
        stamp(at(count - 1), direction(count - 2, count - 1));
        return;
    }

    // Interval and Centre both need the run's length, so they share the walk.
    double total = 0.0;
    for (std::uint32_t v = 1; v < count; ++v)
        total += std::hypot(static_cast<double>(xs[v]) - static_cast<double>(xs[v - 1]),
                            static_cast<double>(ys[v]) - static_cast<double>(ys[v - 1]));
    if (total <= 0.0) return;

    const bool centre = placement == MarkerPlacement::Centre;
    if (!centre && interval <= 0.0) return;

    double walked = 0.0;
    double next   = centre ? total * 0.5 : (phase > 0.0 ? phase : interval * 0.5);

    for (std::uint32_t v = 1; v < count; ++v) {
        const auto a     = at(v - 1);
        const auto b     = at(v);
        const double len = std::hypot(b.first - a.first, b.second - a.second);
        if (len <= 0.0) continue;

        // A SEGMENT WHOLLY OFF SCREEN IS STEPPED OVER, not walked stamp by stamp.
        // Testing each mark and discarding it is already cheap, but at 1:1 a
        // parcel edge carries tens of thousands of them and the arithmetic alone
        // shows up in the frame. Advancing `next` by whole intervals lands on the
        // same phase the walk would have reached, so nothing shifts.
        const bool visible_segment =
            !bounded || (std::max(a.first, b.first) >= static_cast<double>(clip.min_x) &&
                         std::min(a.first, b.first) <= static_cast<double>(clip.max_x) &&
                         std::max(a.second, b.second) >= static_cast<double>(clip.min_y) &&
                         std::min(a.second, b.second) <= static_cast<double>(clip.max_y));

        if (!visible_segment && !centre && interval > 0.0) {
            if (next <= walked + len) {
                const double skipped = std::floor((walked + len - next) / interval) + 1.0;
                next += skipped * interval;
            }
            walked += len;
            continue;
        }

        const auto d = direction(v - 1, v);
        while (next <= walked + len) {
            const double t = (next - walked) / len;
            stamp({a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t}, d);
            if (centre) return;
            next += interval;
        }
        walked += len;
    }
}

// -----------------------------------------------------------------------------

void marker_outline(core::MarkerShape shape, double size, MarkerOutline& out)
{
    out.clear();
    if (size <= 0.0) return;

    const double h = size * 0.5;

    switch (shape) {
    case core::MarkerShape::Circle: {
        for (int i = 0; i < kArcSteps; ++i) {
            const double a = 2.0 * kPi * i / kArcSteps;
            push(out, h * std::cos(a), h * std::sin(a));
        }
        close_run(out, kArcSteps, true);
        break;
    }
    case core::MarkerShape::Square:
        push(out, -h, -h);
        push(out, h, -h);
        push(out, h, h);
        push(out, -h, h);
        close_run(out, 4, true);
        break;

    case core::MarkerShape::Triangle:
        push(out, 0.0, -h);
        push(out, h, h);
        push(out, -h, h);
        close_run(out, 3, true);
        break;

    case core::MarkerShape::Diamond:
        push(out, 0.0, -h);
        push(out, h, 0.0);
        push(out, 0.0, h);
        push(out, -h, 0.0);
        close_run(out, 4, true);
        break;

    case core::MarkerShape::Star:
        for (int i = 0; i < 10; ++i) {
            const double a = -kPi / 2.0 + i * kPi / 5.0;
            const double r = (i % 2 == 0) ? h : h * 0.42;
            push(out, r * std::cos(a), r * std::sin(a));
        }
        close_run(out, 10, true);
        break;

    case core::MarkerShape::Cross:
        push(out, -h, 0.0);
        push(out, h, 0.0);
        close_run(out, 2, false);
        push(out, 0.0, -h);
        push(out, 0.0, h);
        close_run(out, 2, false);
        break;

    case core::MarkerShape::XCross:
        push(out, -h, -h);
        push(out, h, h);
        close_run(out, 2, false);
        push(out, -h, h);
        push(out, h, -h);
        close_run(out, 2, false);
        break;

    case core::MarkerShape::Arrow:
        push(out, h, 0.0);
        push(out, -h, h * 0.7);
        push(out, -h * 0.4, 0.0);
        push(out, -h, -h * 0.7);
        close_run(out, 4, true);
        break;

    case core::MarkerShape::HalfCircle: {
        // The flat side sits ON the line, which is what an escarpment and a
        // shoreline symbol both draw.
        const int steps = kArcSteps / 2;
        for (int i = 0; i <= steps; ++i) {
            const double a = kPi + kPi * i / steps;
            push(out, h * std::cos(a), h * std::sin(a));
        }
        close_run(out, static_cast<std::uint32_t>(steps + 1), true);
        break;
    }

    case core::MarkerShape::Pentagon:
    case core::MarkerShape::Hexagon: {
        const int sides = shape == core::MarkerShape::Pentagon ? 5 : 6;
        for (int i = 0; i < sides; ++i) {
            const double a = -kPi / 2.0 + 2.0 * kPi * i / sides;
            push(out, h * std::cos(a), h * std::sin(a));
        }
        close_run(out, static_cast<std::uint32_t>(sides), true);
        break;
    }

    case core::MarkerShape::Tick:
        // A bare stroke ACROSS the line — local y, because local +x runs along it.
        push(out, 0.0, -h);
        push(out, 0.0, h);
        close_run(out, 2, false);
        break;
    }
}

// -----------------------------------------------------------------------------

void hatch_lines(const PixelBox& face, const PixelBox& clip, double spacing, double angle_degrees,
                 std::vector<float>& out)
{
    if (spacing <= 0.0 || face.empty()) return;

    // THE PHASE ANCHOR IS THE FACE, ALWAYS. Line i sits at `i * spacing` from the
    // face's own centre, so the pattern belongs to the parcel: pan the view and
    // it travels with the parcel instead of crawling across it. Everything below
    // narrows WHICH of those lines are emitted; none of it moves them.
    const double cx = (static_cast<double>(face.min_x) + static_cast<double>(face.max_x)) * 0.5;
    const double cy = (static_cast<double>(face.min_y) + static_cast<double>(face.max_y)) * 0.5;

    const double w = static_cast<double>(face.max_x) - static_cast<double>(face.min_x);
    const double h = static_cast<double>(face.max_y) - static_cast<double>(face.min_y);

    // The half-diagonal, so a set at any angle still reaches every corner, plus
    // one spacing so the outermost line is not cut off by the rotation.
    const double reach = std::hypot(w, h) * 0.5 + spacing;

    const double radians = -angle_degrees * kPi / 180.0;
    const double ca      = std::cos(radians);
    const double sa      = std::sin(radians);

    const auto put = [&](double x, double y) {
        out.push_back(static_cast<float>(cx + x * ca - y * sa));
        out.push_back(static_cast<float>(cy + x * sa + y * ca));
    };

    // What actually has to be covered: the part of the face that is on screen.
    // An empty clip means "no viewport was given", and then the face itself is
    // the region — which is the old behaviour, kept so a caller that cannot say
    // where the screen is still gets a correct picture.
    const bool bounded = !clip.empty();
    const double rx0 =
        bounded ? std::max(static_cast<double>(face.min_x), static_cast<double>(clip.min_x))
                : static_cast<double>(face.min_x);
    const double ry0 =
        bounded ? std::max(static_cast<double>(face.min_y), static_cast<double>(clip.min_y))
                : static_cast<double>(face.min_y);
    const double rx1 =
        bounded ? std::min(static_cast<double>(face.max_x), static_cast<double>(clip.max_x))
                : static_cast<double>(face.max_x);
    const double ry1 =
        bounded ? std::min(static_cast<double>(face.max_y), static_cast<double>(clip.max_y))
                : static_cast<double>(face.max_y);
    if (rx1 <= rx0 || ry1 <= ry0) return; // the face is off screen entirely

    // That region's four corners, turned into the hatch's own frame. The extent
    // along the local y axis says WHICH lines can reach it; the extent along the
    // local x axis says how long each of them has to be.
    double ly_min = 1e300;
    double ly_max = -1e300;
    double lx_min = 1e300;
    double lx_max = -1e300;
    for (const auto& [px, py] :
         {std::pair{rx0, ry0}, std::pair{rx1, ry0}, std::pair{rx1, ry1}, std::pair{rx0, ry1}}) {
        const double dx = px - cx;
        const double dy = py - cy;
        const double lx = dx * ca + dy * sa;
        const double ly = -dx * sa + dy * ca;
        lx_min          = std::min(lx_min, lx);
        lx_max          = std::max(lx_max, lx);
        ly_min          = std::min(ly_min, ly);
        ly_max          = std::max(ly_max, ly);
    }

    // A COUNT, not an accumulated position: adding a step a thousand times drifts,
    // and a hatch that drifts is a hatch whose spacing is not the one the annex
    // published. The count is now taken from the visible band rather than from the
    // whole face, which is the difference between a few dozen lines and a few
    // hundred thousand at 1:1.
    const auto steps = static_cast<int>(std::floor(reach / spacing));
    const int first  = std::max(-steps, static_cast<int>(std::floor(ly_min / spacing)) - 1);
    const int last   = std::min(steps, static_cast<int>(std::ceil(ly_max / spacing)) + 1);

    // One spacing of overshoot at each end so a line's cap and its dash phase do
    // not stop exactly at the screen edge and shimmer as the view moves.
    const double x_from = std::max(-reach, lx_min - spacing);
    const double x_to   = std::min(reach, lx_max + spacing);
    if (x_to <= x_from) return;

    for (int i = first; i <= last; ++i) {
        const double y = i * spacing;
        put(x_from, y);
        put(x_to, y);
    }
}

void pattern_points(const PixelBox& face, const PixelBox& clip, double step_x, double step_y,
                    std::vector<float>& out)
{
    if (step_x <= 0.0 || step_y <= 0.0) return;
    if (face.max_x < face.min_x || face.max_y < face.min_y) return;

    // Bounded to what can be seen BEFORE the grid is counted. `floor(edge / step)`
    // anchors the grid to the pixel lattice and not to the edge it was given, so
    // clipping the edge moves no glyph — it only stops the loop from walking
    // across a parcel that is mostly off screen. A grid is two dimensional, so
    // this is the difference between a hundred stamps and a hundred thousand.
    const bool bounded = !clip.empty();
    const double min_x =
        bounded ? std::max(static_cast<double>(face.min_x), static_cast<double>(clip.min_x))
                : static_cast<double>(face.min_x);
    const double min_y =
        bounded ? std::max(static_cast<double>(face.min_y), static_cast<double>(clip.min_y))
                : static_cast<double>(face.min_y);
    const double max_x =
        bounded ? std::min(static_cast<double>(face.max_x), static_cast<double>(clip.max_x))
                : static_cast<double>(face.max_x);
    const double max_y =
        bounded ? std::min(static_cast<double>(face.max_y), static_cast<double>(clip.max_y))
                : static_cast<double>(face.max_y);
    if (max_x < min_x || max_y < min_y) return;

    const double x0 = std::floor(min_x / step_x) * step_x;
    const double y0 = std::floor(min_y / step_y) * step_y;

    const auto cols = static_cast<int>(std::floor((max_x + step_x - x0) / step_x));
    const auto rows = static_cast<int>(std::floor((max_y + step_y - y0) / step_y));

    // A cheap ceiling on how many glyphs one face can ask for. It survives the
    // clip because a spacing that collapses — a symbol whose interval rounds to a
    // fraction of a pixel — can still ask for millions of stamps inside a single
    // screen. What changed is that it is no longer REACHED by a normal deep zoom,
    // where it used to make the pattern vanish rather than merely be slow.
    constexpr int kMaxStamps = 40000;
    if (cols <= 0 || rows <= 0 || cols > kMaxStamps || rows > kMaxStamps ||
        cols * rows > kMaxStamps)
        return;

    for (int r = 0; r <= rows; ++r) {
        for (int c = 0; c <= cols; ++c) {
            out.push_back(static_cast<float>(x0 + c * step_x));
            out.push_back(static_cast<float>(y0 + r * step_y));
        }
    }
}

} // namespace kentos::render
