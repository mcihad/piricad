// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/pick.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/wire.hpp"

#include "kind_common.hpp"

#include <algorithm>
#include <cmath>

namespace kentos::core {

// ---------------------------------------------------------- shared helpers ---
//
// The pieces every kind file uses (kind_common.hpp). Defined here rather than in
// a file of their own so the polyline — the kind the frame budget is written
// against — keeps its helpers in the translation unit that calls them.
namespace kind {

double segment_distance2_m(Point2 a, Point2 b, Point2 p)
{
    const double vx = mm_to_metres(b.x - a.x);
    const double vy = mm_to_metres(b.y - a.y);
    const double wx = mm_to_metres(p.x - a.x);
    const double wy = mm_to_metres(p.y - a.y);

    const double vv = vx * vx + vy * vy;
    double t        = 0.0;
    if (vv > 0.0) {
        t = (wx * vx + wy * vy) / vv;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }

    const double dx = wx - t * vx;
    const double dy = wy - t * vy;
    return dx * dx + dy * dy;
}

Status refuse_payload(const char* what, std::span<const std::uint8_t> payload)
{
    if (payload.empty()) return ok();
    return err(ErrorCode::ValidationFailed, std::string(what) + " tür yükü taşımaz; verilen " +
                                                std::to_string(payload.size()) + " bayt.");
}

Status one_open_ring(const char* what, std::span<const RingGeometry::RingInput> rings,
                     std::size_t vertices)
{
    if (rings.size() != 1 || rings[0].role != RingRole::Open || rings[0].points.size() != vertices)
        return err(ErrorCode::ValidationFailed,
                   std::string(what) + " tek bir açık halkada tam " + std::to_string(vertices) +
                       " tepe noktası ister; verilen " + std::to_string(rings.size()) + " halka" +
                       (rings.empty() ? std::string()
                                      : ", ilkinde " + std::to_string(rings[0].points.size()) +
                                            " tepe noktası") +
                       ".");
    return ok();
}

Box2 box_of_points(std::span<const Mm> xs, std::span<const Mm> ys)
{
    Box2 b{};
    for (std::size_t v = 0; v < xs.size(); ++v)
        b.extend(Point2{xs[v], ys[v]});
    return b;
}

Box2 box_of_runs(const EmitBuffer& runs)
{
    return box_of_points(runs.xs, runs.ys);
}

bool runs_hit(const EmitBuffer& runs, Point2 probe, Mm tolerance, bool inside_counts)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;
    for (std::size_t r = 0; r < runs.run_total(); ++r) {
        const auto xs = runs.run_xs(r);
        const auto ys = runs.run_ys(r);
        if (xs.empty()) continue;
        if (xs.size() == 1) {
            const Point2 v{xs[0], ys[0]};
            if (segment_distance2_m(v, v, probe) <= limit) return true;
            continue;
        }
        const bool closed      = runs.run_closed[r] != 0;
        const std::size_t last = xs.size() - 1;
        const std::size_t segs = closed ? xs.size() : last;
        for (std::size_t sgm = 0; sgm < segs; ++sgm) {
            const std::size_t j = (sgm == last) ? 0 : sgm + 1;
            if (segment_distance2_m(Point2{xs[sgm], ys[sgm]}, Point2{xs[j], ys[j]}, probe) <= limit)
                return true;
        }
    }
    if (!inside_counts) return false;
    // A click inside the shape picks the shape, and a hole vetoes it — the
    // polyline rule, over the drawn runs.
    bool in_shape = false;
    bool in_hole  = false;
    for (std::size_t r = 0; r < runs.run_total(); ++r) {
        if (runs.run_closed[r] == 0) continue;
        if (!ring_contains(runs.run_xs(r), runs.run_ys(r), probe)) continue;
        if (runs.run_hole[r] != 0)
            in_hole = true;
        else
            in_shape = true;
    }
    return in_shape && !in_hole;
}

Mm run_length(std::span<const Mm> xs, std::span<const Mm> ys, bool closed)
{
    if (xs.size() < 2) return 0;
    double total           = 0.0;
    const std::size_t segs = closed ? xs.size() : xs.size() - 1;
    for (std::size_t i = 0; i < segs; ++i) {
        const std::size_t j = (i + 1) % xs.size();
        const auto dx       = static_cast<double>(xs[j] - xs[i]);
        const auto dy       = static_cast<double>(ys[j] - ys[i]);
        total += std::sqrt(dx * dx + dy * dy);
    }
    return static_cast<Mm>(std::llround(total));
}

double run_area2(std::span<const Mm> xs, std::span<const Mm> ys)
{
    if (xs.size() < 3) return 0.0;
    double a2 = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const std::size_t j = (i + 1) % xs.size();
        a2 += static_cast<double>(xs[i] - xs[0]) * static_cast<double>(ys[j] - ys[0]) -
              static_cast<double>(xs[j] - xs[0]) * static_cast<double>(ys[i] - ys[0]);
    }
    return a2;
}

} // namespace kind

using kind::kKeyCenter;
using kind::kKeyEndpoint;
using kind::kKeyMidpoint;
using kind::offer;
using kind::one_open_ring;
using kind::refuse_payload;
using kind::segment_distance2_m;

namespace {

// ---------------------------------------------------------- core.polyline ----
//
// The one built-in kind. Its six functions are free functions over spans: they
// take the geometry store and a batch of slots, and they close over nothing.

void polyline_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.bounds_of(slots[i]);
}

void polyline_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.area_of(slots[i]);
}

void polyline_outline(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            into.begin_run(geom.ring_role[r] != RingRole::Open);
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            for (std::size_t v = 0; v < xs.size(); ++v)
                into.push_vertex(xs[v], ys[v]);
        }
    }
}

void polyline_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
                  std::span<std::uint8_t> out)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i]            = 0;
        const RingSpan rs = geom.rings_of(slots[i]);

        for (std::uint32_t r = rs.first; r < rs.first + rs.count && out[i] == 0; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            if (xs.empty()) continue;

            // A one-vertex ring has no segment; measure to the vertex itself so a
            // degenerate record is still selectable rather than invisible.
            if (xs.size() == 1) {
                const Point2 v{xs[0], ys[0]};
                if (segment_distance2_m(v, v, probe) <= limit) out[i] = 1;
                continue;
            }

            const bool closed      = geom.ring_role[r] != RingRole::Open;
            const std::size_t last = xs.size() - 1;
            const std::size_t segs = closed ? xs.size() : last;
            for (std::size_t s = 0; s < segs; ++s) {
                const std::size_t j = (s == last) ? 0 : s + 1;
                const Point2 a{xs[s], ys[s]};
                const Point2 b{xs[j], ys[j]};
                if (segment_distance2_m(a, b, probe) <= limit) {
                    out[i] = 1;
                    break;
                }
            }
        }

        // A CLICK INSIDE A FACE PICKS THE FACE. Measuring only to the edges is
        // the CAD answer and the wrong one for a map: a user reaching for a
        // parcel points AT the parcel, not at the hairline around it, and on a
        // sheet of adjoining parcels the interior is nearly all of what there is
        // to point at. Every tool that starts by asking which objects — move,
        // split, offset — was unusable because of it: the click found nothing and
        // the command waited for a selection that could not be made.
        //
        // AFTER the edge walk and over ALL the rings, because a hole has to be
        // able to veto the exterior: a click in the court cut out of a building
        // is a click in the court, and the walk above stops at the first ring
        // that answers.
        if (out[i] == 0) {
            bool in_exterior = false;
            bool in_hole     = false;
            for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
                if (geom.ring_role[r] == RingRole::Open) continue;
                const auto xs = geom.ring_xs(r);
                const auto ys = geom.ring_ys(r);
                if (!ring_contains(xs, ys, probe)) continue;
                if (geom.ring_role[r] == RingRole::Exterior)
                    in_exterior = true;
                else
                    in_hole = true;
            }
            if (in_exterior && !in_hole) out[i] = 1;
        }
    }
}

// -------------------------------------------------------------- payload ------
//
// The byte order lives in core/wire.hpp, shared with every kind a later phase
// adds and with the document's payload columns; this file only names the
// reader the way its read functions always have.
using Reader = WireReader;

void polyline_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
                    std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const RingSpan rs = geom.rings_of(slot);
        put_u32(bytes, rs.count);
        for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            bytes.push_back(static_cast<std::uint8_t>(geom.ring_role[r]));
            put_u16(bytes, geom.ring_part[r]);
            put_u32(bytes, static_cast<std::uint32_t>(xs.size()));
            for (std::size_t v = 0; v < xs.size(); ++v) {
                put_mm(bytes, xs[v]);
                put_mm(bytes, ys[v]);
            }
        }
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> polyline_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(4))
        return err(ErrorCode::ParseError,
                   "Çoklu çizgi yükü halka sayısını taşıyacak kadar uzun değil.");

    const std::uint32_t ring_count = in.u32();
    // Cheapest possible ring is 7 header bytes; refuse a count the file cannot
    // hold before reserving anything for it.
    if (!in.remaining_records(ring_count, 7))
        return err(ErrorCode::ParseError, "Çoklu çizgi yükü " + std::to_string(ring_count) +
                                              " halka bildiriyor, bu kadar veri yok.");

    std::vector<std::vector<Point2>> store(ring_count);
    std::vector<RingGeometry::RingInput> rings(ring_count);

    for (std::uint32_t r = 0; r < ring_count; ++r) {
        if (!in.remaining(7))
            return err(ErrorCode::ParseError, "Halka " + std::to_string(r) + " başlığı eksik.");

        const std::uint8_t role_byte = in.u8();
        const std::uint16_t part     = in.u16();
        const std::uint32_t verts    = in.u32();

        if (role_byte > static_cast<std::uint8_t>(RingRole::Interior))
            return err(ErrorCode::ParseError,
                       "Bilinmeyen halka rolü: " + std::to_string(role_byte));
        if (!in.remaining_records(verts, 16))
            return err(ErrorCode::ParseError, "Halka " + std::to_string(r) + " için " +
                                                  std::to_string(verts) +
                                                  " tepe noktası bildirilmiş, bu kadar veri yok.");

        store[r].resize(verts);
        for (std::uint32_t v = 0; v < verts; ++v) {
            store[r][v].x = in.mm();
            store[r][v].y = in.mm();
        }

        rings[r].points = std::span<const Point2>(store[r]);
        rings[r].role   = static_cast<RingRole>(role_byte);
        rings[r].part   = part;
    }

    return geom.append(std::span<const RingGeometry::RingInput>(rings));
}

// ------------------------------------------------------------ core.circle ----
//
// A CIRCLE IS ITS DEFINITION, not its picture. The store holds one Open ring of
// exactly two vertices:
//
//     vertex 0 = the centre
//     vertex 1 = {centre.x + radius, centre.y}
//
// so the radius is `xs[1] - xs[0]` — an exact integer, read back with no square
// root and no rounding. A circle stored as the polygon it looks like would have a
// circumference that is not 2*pi*r and an area that is not pi*r^2, and on a
// cadastral sheet those are the numbers that go on the tapu (§12).
//
// The two vertices are geometry the arena already understands, which is what lets
// a circle be saved, loaded and bounded by the same columns a line uses. What
// tells the two apart is `entities.kind` — that column is exactly why it exists.

void circle_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 c = circle_centre_of(geom, slots[i]);
        const Mm r     = circle_radius_of(geom, slots[i]);
        out[i]         = Box2{c.x - r, c.y - r, c.x + r, c.y + r};
    }
}

void circle_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    // pi as a literal: no libm call, so every platform multiplies the same three
    // doubles in the same order and rounds the same way (§7.3, Article 2.5).
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto r   = static_cast<double>(circle_radius_of(geom, slots[i]));
        const double a = kPi * r * r;

        // A circle 1000 km across is 3.1e18 mm^2, still inside int64; anything
        // beyond that is not a drawing and is clamped rather than wrapped.
        constexpr double kMax = 9.0e18;
        // `llround`, not `+ 0.5` and a cast: the idiom rounds the WRONG WAY for a
        // negative value and loses the half exactly where the double runs out of
        // mantissa. This area cannot be negative, so the two agree here today —
        // which is the reason to write the one that stays right.
        out[i] = a >= kMax ? static_cast<Mm2>(kMax) : static_cast<Mm2>(std::llround(a));
    }
}

void circle_outline_fn(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (const std::uint32_t slot : slots) {
        xs.clear();
        ys.clear();
        circle_outline(circle_centre_of(geom, slot), circle_radius_of(geom, slot), xs, ys);

        into.begin_run(true); // closed: the segment back to the first point is implied
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
}

void circle_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
                std::span<std::uint8_t> out)
{
    const double tol = mm_to_metres(tolerance < 0 ? 0 : tolerance);

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 c = circle_centre_of(geom, slots[i]);
        const double r = mm_to_metres(circle_radius_of(geom, slots[i]));

        const double dx = mm_to_metres(probe.x - c.x);
        const double dy = mm_to_metres(probe.y - c.y);
        const double d  = std::sqrt(dx * dx + dy * dy);

        // The RIM, not the disc: a circle drawn as an outline is picked where it
        // is drawn, the same way a polyline is picked on its segments and not
        // inside the shape they enclose.
        const double off = d > r ? d - r : r - d;
        out[i]           = off <= tol ? std::uint8_t{1} : std::uint8_t{0};
    }
}

void circle_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
                  std::vector<std::uint32_t>& ends)
{
    // Centre and radius, which is what a circle IS. Writing the two stored
    // vertices instead would be writing a representation rather than a fact, and
    // a later build free to store the handle vertex differently could not read it.
    for (const std::uint32_t slot : slots) {
        const Point2 c = circle_centre_of(geom, slot);
        put_mm(bytes, c.x);
        put_mm(bytes, c.y);
        put_mm(bytes, circle_radius_of(geom, slot));
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> circle_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(24))
        return err(ErrorCode::ParseError,
                   "Daire yükü merkez ve yarıçapı taşıyacak kadar uzun değil.");

    const Mm cx = in.mm();
    const Mm cy = in.mm();
    const Mm r  = in.mm();

    if (r <= 0)
        return err(ErrorCode::ParseError,
                   "Daire yarıçapı sıfır ya da negatif: " + std::to_string(r));

    const Point2 pts[2]{Point2{cx, cy}, Point2{cx + r, cy}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 2), RingRole::Open, 0};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

// --------------------------------------------------------------- core.arc ----
//
// Centre, exact radius, and the two measured ends; the sweep runs
// counter-clockwise from the first to the second (core/arc.hpp).

void arc_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        xs.clear();
        ys.clear();
        arc_outline(arc_centre_of(geom, slots[i]), arc_radius_of(geom, slots[i]),
                    arc_start_of(geom, slots[i]), arc_end_of(geom, slots[i]), xs, ys);

        if (xs.empty()) {
            out[i] = Box2{};
            continue;
        }

        // FROM THE DRAWN FORM, because an arc's box is not its ends' box: a sweep
        // that crosses due north reaches further up than either end does. Walking
        // the outline gets that right without a case for each quadrant.
        Box2 b{xs[0], ys[0], xs[0], ys[0]};
        for (std::size_t v = 1; v < xs.size(); ++v) {
            b.min_x = xs[v] < b.min_x ? xs[v] : b.min_x;
            b.min_y = ys[v] < b.min_y ? ys[v] : b.min_y;
            b.max_x = xs[v] > b.max_x ? xs[v] : b.max_x;
            b.max_y = ys[v] > b.max_y ? ys[v] : b.max_y;
        }
        out[i] = b;
    }
}

void circle_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    // The same literal pi `circle_area` uses, and for the same reason: no libm
    // call, so every platform multiplies the same doubles in the same order
    // (§7.3, Article 2.5).
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto r = static_cast<double>(circle_radius_of(geom, slots[i]));
        out[i]       = static_cast<Mm>(std::llround(2.0 * kPi * r));
    }
}

void arc_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 centre = arc_centre_of(geom, slots[i]);
        const auto r        = static_cast<double>(arc_radius_of(geom, slots[i]));
        const Point2 start  = arc_start_of(geom, slots[i]);
        const Point2 end    = arc_end_of(geom, slots[i]);

        // The sweep from the two ends, counter-clockwise, which is the direction
        // `add_arc` documents — in whole micro-degrees from `atan2_udeg`, never
        // from `std::atan2`, so the same arc measures the same on every platform
        // (§7.3). Ends that coincide are a full turn, not a zero-length arc.
        const std::int64_t a0 = atan2_udeg(start.y - centre.y, start.x - centre.x);
        const std::int64_t a1 = atan2_udeg(end.y - centre.y, end.x - centre.x);
        std::int64_t sweep    = a1 - a0;
        if (sweep <= 0) sweep += kUDegFullCircle;

        const double radians = static_cast<double>(sweep) * (kPi / (180.0 * 1000000.0));
        out[i]               = static_cast<Mm>(std::llround(r * radians));
    }
}

void polyline_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    // What the stored run measures IS what a polyline measures, so this is the
    // one kind for which the geometry's own answer was already right.
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = geom.perimeter_of(slots[i]);
}

void point_perimeter(const RingGeometry&, SlotSpan slots, std::span<Mm> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm{0};
}

void arc_area(const RingGeometry&, SlotSpan slots, std::span<Mm2> out)
{
    // An arc encloses nothing, exactly as an open ring encloses nothing (R10).
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm2{0};
}

void arc_outline_fn(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (const std::uint32_t slot : slots) {
        xs.clear();
        ys.clear();
        arc_outline(arc_centre_of(geom, slot), arc_radius_of(geom, slot), arc_start_of(geom, slot),
                    arc_end_of(geom, slot), xs, ys);

        into.begin_run(false); // an arc does not close
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
}

void arc_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
             std::span<std::uint8_t> out)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;

    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        out[i] = 0;
        xs.clear();
        ys.clear();
        arc_outline(arc_centre_of(geom, slots[i]), arc_radius_of(geom, slots[i]),
                    arc_start_of(geom, slots[i]), arc_end_of(geom, slots[i]), xs, ys);

        // MEASURED AGAINST THE DRAWN FORM, so the sweep is respected for free: the
        // part of the circle the arc does not cover has no segments to be near.
        for (std::size_t v = 0; v + 1 < xs.size(); ++v) {
            if (segment_distance2_m(Point2{xs[v], ys[v]}, Point2{xs[v + 1], ys[v + 1]}, probe) <=
                limit) {
                out[i] = 1;
                break;
            }
        }
    }
}

void arc_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
               std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const Point2 c = arc_centre_of(geom, slot);
        const Point2 s = arc_start_of(geom, slot);
        const Point2 e = arc_end_of(geom, slot);
        put_mm(bytes, c.x);
        put_mm(bytes, c.y);
        put_mm(bytes, arc_radius_of(geom, slot));
        put_mm(bytes, s.x);
        put_mm(bytes, s.y);
        put_mm(bytes, e.x);
        put_mm(bytes, e.y);
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> arc_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(56))
        return err(ErrorCode::ParseError, "Yay yükü merkez, yarıçap ve iki ucu taşımıyor.");

    const Mm cx = in.mm();
    const Mm cy = in.mm();
    const Mm r  = in.mm();
    const Mm sx = in.mm();
    const Mm sy = in.mm();
    const Mm ex = in.mm();
    const Mm ey = in.mm();

    if (r <= 0)
        return err(ErrorCode::ParseError, "Yay yarıçapı sıfır ya da negatif: " + std::to_string(r));

    const Point2 pts[4]{Point2{cx, cy}, Point2{cx + r, cy}, Point2{sx, sy}, Point2{ex, ey}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 4), RingRole::Open, 0};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

// ------------------------------------------------------------- core.point ----
//
// A SURVEYED POINT: a control point, a traverse station, a benchmark. One Open
// ring of exactly one vertex, which is the smallest thing the arena can hold and
// exactly what this is — a place, with nothing between it and anywhere else.
//
// `snap.hpp` reserved the DÜĞÜM snap bit for this and left it unused, because a
// snap mode for a thing that cannot exist is a promise the program does not keep.
// It can exist now.

Point2 point_position(const RingGeometry& geom, std::uint32_t slot)
{
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return Point2{};
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    if (xs.empty()) return Point2{};
    return Point2{xs[0], ys[0]};
}

void point_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 p = point_position(geom, slots[i]);
        out[i]         = Box2{p.x, p.y, p.x, p.y};
    }
}

void point_area(const RingGeometry&, SlotSpan slots, std::span<Mm2> out)
{
    // A point encloses nothing and never will.
    for (std::size_t i = 0; i < slots.size(); ++i)
        out[i] = Mm2{0};
}

void point_outline_fn(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    // ONE VERTEX, and the marker is the style's business rather than the kind's.
    // A kind that emitted a cross here would be deciding what a röper looks like,
    // and that is what the symbology catalogue is for (model.md R14).
    for (const std::uint32_t slot : slots) {
        const Point2 p = point_position(geom, slot);
        into.begin_run(false);
        into.push_vertex(p.x, p.y);
    }
}

void point_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
               std::span<std::uint8_t> out)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const Point2 p  = point_position(geom, slots[i]);
        const double dx = mm_to_metres(probe.x - p.x);
        const double dy = mm_to_metres(probe.y - p.y);
        out[i]          = (dx * dx + dy * dy) <= limit ? std::uint8_t{1} : std::uint8_t{0};
    }
}

void point_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
                 std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const Point2 p = point_position(geom, slot);
        put_mm(bytes, p.x);
        put_mm(bytes, p.y);
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> point_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(16)) return err(ErrorCode::ParseError, "Nokta yükü bir koordinat taşımıyor.");

    const Mm x = in.mm();
    const Mm y = in.mm();

    const Point2 pts[1]{Point2{x, y}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 1), RingRole::Open, 0};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

} // namespace

Point2 point_position_of(const RingGeometry& geom, std::uint32_t slot)
{
    return point_position(geom, slot);
}

// ------------------------------------------------------------- validate ------
//
// Each kind's own floor, asked before a single byte is appended (model.md R9a).
// The five built-in kinds carry no payload, so each refuses one: bytes a build
// cannot interpret are bytes a later build would trust.

Status polyline_validate(std::span<const RingGeometry::RingInput> rings,
                         std::span<const std::uint8_t> payload)
{
    if (auto st = refuse_payload("Çoklu çizgi", payload); !st) return st;
    // THE FLOOR LIVES HERE, where the kind is known. `RingGeometry::append`
    // allows a one-vertex open ring because a `core.point` is exactly that; a
    // polyline with one vertex is a line that goes nowhere and is refused. Faces
    // are checked by the arena itself (three vertices, R11 order).
    if (rings.size() == 1 && rings[0].role == RingRole::Open && rings[0].points.size() < 2)
        return err(ErrorCode::ValidationFailed, "Bir çizgi en az iki nokta ister, verilen: " +
                                                    std::to_string(rings[0].points.size()) + ".");
    return ok();
}

Status circle_validate(std::span<const RingGeometry::RingInput> rings,
                       std::span<const std::uint8_t> payload)
{
    if (auto st = refuse_payload("Daire", payload); !st) return st;
    if (auto st = one_open_ring("Daire", rings, 2); !st) return st;
    const Point2 c = rings[0].points[0];
    const Point2 h = rings[0].points[1];
    if (h.y != c.y || h.x <= c.x)
        return err(ErrorCode::ValidationFailed,
                   "Dairenin yarıçap tutamağı merkezin tam doğusunda ve merkezden uzakta olmalı; "
                   "yarıçap sıfırdan büyük olmalı.");
    return ok();
}

Status arc_validate(std::span<const RingGeometry::RingInput> rings,
                    std::span<const std::uint8_t> payload)
{
    if (auto st = refuse_payload("Yay", payload); !st) return st;
    if (auto st = one_open_ring("Yay", rings, 4); !st) return st;
    const Point2 c = rings[0].points[0];
    const Point2 h = rings[0].points[1];
    if (h.y != c.y || h.x <= c.x)
        return err(ErrorCode::ValidationFailed,
                   "Yayın yarıçap tutamağı merkezin tam doğusunda olmalı; yarıçap sıfırdan büyük "
                   "olmalı.");
    return ok();
}

Status point_validate(std::span<const RingGeometry::RingInput> rings,
                      std::span<const std::uint8_t> payload)
{
    if (auto st = refuse_payload("Nokta", payload); !st) return st;
    return one_open_ring("Nokta", rings, 1);
}

Status ellipse_validate(std::span<const RingGeometry::RingInput> rings,
                        std::span<const std::uint8_t> payload)
{
    // A payload is the sweep of a PARTIAL ellipse (core/ellipse.hpp); none is a
    // whole one. Anything else is bytes this kind cannot read.
    if (!payload.empty())
        if (auto arc = decode_ellipse_arc(payload); !arc) return arc.error();
    if (auto st = one_open_ring("Elips", rings, 3); !st) return st;
    const Point2 c = rings[0].points[0];
    if (rings[0].points[1] == c)
        return err(ErrorCode::InvalidArgument, "Elipsin birinci ekseni sıfır uzunlukta olamaz.");
    if (rings[0].points[2] == c)
        return err(ErrorCode::InvalidArgument, "Elipsin ikinci ekseni sıfır uzunlukta olamaz.");
    return ok();
}

// ----------------------------------------------------------- key points ------
//
// What a snap reaches for on a curve beyond its outline. The mode bits are the
// values `core/snap.hpp` declares; they are written as numbers here because this
// header sits below the snap engine, and a kind must not depend on it.

void circle_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    offer(into, circle_centre_of(geom, slot), kKeyCenter);
}

void arc_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    const Point2 centre = arc_centre_of(geom, slot);
    const Point2 from   = arc_start_of(geom, slot);
    const Point2 to     = arc_end_of(geom, slot);
    offer(into, centre, kKeyCenter);
    offer(into, from, kKeyEndpoint);
    offer(into, to, kKeyEndpoint);
    offer(into, arc_midpoint(centre, arc_radius_of(geom, slot), from, to), kKeyMidpoint);
}

void ellipse_key_points(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into)
{
    // The centre, and the four axis ends: the two the command was given and
    // their mirrors. They are what ELİPS was drawn from and what a surveyor
    // measures it by; an ellipse has no corner and no end of its own. A partial
    // ellipse has two ends instead of four axis ends.
    const Point2 c     = ellipse_centre_of(geom, slot);
    const Point2 major = ellipse_major_of(geom, slot);
    const Point2 minor = ellipse_minor_of(geom, slot);
    offer(into, c, kKeyCenter);
    if (const auto arc = ellipse_arc_of(geom, slot); arc.has_value()) {
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        ellipse_arc_outline(c, major, minor, arc->start_udeg, arc->end_udeg, xs, ys);
        if (!xs.empty()) {
            offer(into, Point2{xs.front(), ys.front()}, kKeyEndpoint);
            offer(into, Point2{xs.back(), ys.back()}, kKeyEndpoint);
        }
        return;
    }
    offer(into, major, kKeyEndpoint);
    offer(into, minor, kKeyEndpoint);
    offer(into, Point2{2 * c.x - major.x, 2 * c.y - major.y}, kKeyEndpoint);
    offer(into, Point2{2 * c.x - minor.x, 2 * c.y - minor.y}, kKeyEndpoint);
}

KENTOS_KIND(point)
{
    KindSpec s{};
    s.id         = kPointKind;
    s.stable_id  = "core.point";
    s.summary_tr = "Ölçülmüş tek nokta: nirengi, poligon noktası, röper.";
    s.names[0]   = "NOKTA";
    s.names[1]   = "NOKTA";
    s.names[2]   = "POINT";
    s.names[3]   = "NK";
    s.bbox       = &point_bbox;
    s.outline    = &point_outline_fn;
    s.hit        = &point_hit;
    s.area       = &point_area;
    s.perimeter  = &point_perimeter;
    s.read       = &point_read;
    s.write      = &point_write;
    s.validate   = &point_validate;
    return s;
}

KENTOS_KIND(arc)
{
    KindSpec s{};
    s.id         = kArcKind;
    s.stable_id  = "core.arc";
    s.summary_tr = "Merkez, yarıçap ve iki uçla tanımlı yay.";
    s.names[0]   = "YAY";
    s.names[1]   = "YAY";
    s.names[2]   = "ARC";
    s.names[3]   = "YY";
    s.bbox       = &arc_bbox;
    s.outline    = &arc_outline_fn;
    s.hit        = &arc_hit;
    s.area       = &arc_area;
    s.perimeter  = &arc_perimeter;
    s.read       = &arc_read;
    s.write      = &arc_write;
    s.validate   = &arc_validate;
    s.key_points = &arc_key_points;
    return s;
}

KENTOS_KIND(circle)
{
    KindSpec s{};
    s.id         = kCircleKind;
    s.stable_id  = "core.circle";
    s.summary_tr = "Merkez ve yarıçapla tanımlı daire.";
    s.names[0]   = "DAİRE";
    s.names[1]   = "DAIRE";
    s.names[2]   = "CIRCLE";
    s.names[3]   = "DR";
    s.bbox       = &circle_bbox;
    s.outline    = &circle_outline_fn;
    s.hit        = &circle_hit;
    s.area       = &circle_area;
    s.perimeter  = &circle_perimeter;
    s.read       = &circle_read;
    s.write      = &circle_write;
    s.validate   = &circle_validate;
    s.key_points = &circle_key_points;
    return s;
}

// ------------------------------------------------------------ core.ellipse ----
//
// Three stored vertices: the centre and the two axis ENDPOINTS. The endpoints
// carry the rotation as vectors, so nothing here reads or writes an angle.

/// The drawn form of one ellipse slot: the whole curve, closed, or the partial
/// one's open sweep. Returns whether the run closes.
bool ellipse_run(const RingGeometry& geom, std::uint32_t slot, std::vector<Mm>& xs,
                 std::vector<Mm>& ys)
{
    xs.clear();
    ys.clear();
    const Point2 c     = ellipse_centre_of(geom, slot);
    const Point2 major = ellipse_major_of(geom, slot);
    const Point2 minor = ellipse_minor_of(geom, slot);
    if (const auto arc = ellipse_arc_of(geom, slot); arc.has_value()) {
        ellipse_arc_outline(c, major, minor, arc->start_udeg, arc->end_udeg, xs, ys);
        return false;
    }
    ellipse_outline(c, major, minor, xs, ys);
    return true;
}

void ellipse_bbox(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (ellipse_arc_of(geom, slots[i]).has_value()) {
            // A partial ellipse's box is its drawn sweep's, like an arc's.
            ellipse_run(geom, slots[i], xs, ys);
            out[i] = kind::box_of_points(xs, ys);
            continue;
        }
        // The exact extent of a rotated ellipse is `sqrt(ax² + bx²)` in x and
        // `sqrt(ay² + by²)` in y — the half-widths of its bounding box. Not the
        // axis endpoints: for a rotated ellipse those lie INSIDE the box, and a
        // cull box that small would drop the shape at the edge of the view.
        const Point2 c = ellipse_centre_of(geom, slots[i]);
        const Point2 a = ellipse_major_of(geom, slots[i]);
        const Point2 b = ellipse_minor_of(geom, slots[i]);

        const auto ax = static_cast<double>(a.x - c.x);
        const auto ay = static_cast<double>(a.y - c.y);
        const auto bx = static_cast<double>(b.x - c.x);
        const auto by = static_cast<double>(b.y - c.y);

        const Mm hx = mm_round(std::sqrt(ax * ax + bx * bx));
        const Mm hy = mm_round(std::sqrt(ay * ay + by * by));
        out[i]      = Box2{c.x - hx, c.y - hy, c.x + hx, c.y + hy};
    }
}

void ellipse_perimeter(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (ellipse_arc_of(geom, slots[i]).has_value()) {
            // A partial ellipse's length has no closed form either; the drawn
            // sweep's is what is reported, and the page says so.
            ellipse_run(geom, slots[i], xs, ys);
            out[i] = kind::run_length(xs, ys, false);
            continue;
        }
        const Point2 c = ellipse_centre_of(geom, slots[i]);
        const Point2 a = ellipse_major_of(geom, slots[i]);
        const Point2 b = ellipse_minor_of(geom, slots[i]);

        const double ra =
            std::hypot(static_cast<double>(a.x - c.x), static_cast<double>(a.y - c.y));
        const double rb =
            std::hypot(static_cast<double>(b.x - c.x), static_cast<double>(b.y - c.y));

        // RAMANUJAN'S SECOND APPROXIMATION. An ellipse's circumference has no
        // closed form — it is an elliptic integral — and this one is within a few
        // parts per billion for every eccentricity a drawing produces, which is
        // far below the millimetre this is rounded to. Stated rather than hidden:
        // the number is an approximation, and it is a better one than the
        // tessellation it replaces.
        const double h = (ra - rb) * (ra - rb) / ((ra + rb) * (ra + rb));
        const double p = kPi * (ra + rb) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));

        out[i] = std::isfinite(p) ? static_cast<Mm>(std::llround(p)) : Mm{0};
    }
}

void ellipse_area(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out)
{
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (ellipse_arc_of(geom, slots[i]).has_value()) {
            // An open sweep encloses nothing, exactly as an arc does not.
            out[i] = Mm2{0};
            continue;
        }
        const Point2 c = ellipse_centre_of(geom, slots[i]);
        const Point2 a = ellipse_major_of(geom, slots[i]);
        const Point2 b = ellipse_minor_of(geom, slots[i]);

        // pi·|a×b|: the cross product of the two axis vectors is the area of the
        // parallelogram they span, and that is right for a rotated ellipse where
        // multiplying two axis LENGTHS would only be right for an upright one.
        const auto ax = static_cast<double>(a.x - c.x);
        const auto ay = static_cast<double>(a.y - c.y);
        const auto bx = static_cast<double>(b.x - c.x);
        const auto by = static_cast<double>(b.y - c.y);

        const double cross = ax * by - ay * bx;
        const double area  = kPi * (cross < 0.0 ? -cross : cross);

        constexpr double kMax = 9.0e18;
        out[i] = area >= kMax ? static_cast<Mm2>(kMax) : static_cast<Mm2>(std::llround(area));
    }
}

void ellipse_outline_fn(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into)
{
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (const std::uint32_t slot : slots) {
        const bool closed = ellipse_run(geom, slot, xs, ys);
        into.begin_run(closed);
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
}

void ellipse_hit(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
                 std::span<std::uint8_t> out)
{
    const double tol   = mm_to_metres(tolerance < 0 ? 0 : tolerance);
    const double limit = tol * tol;

    std::vector<Mm> xs;
    std::vector<Mm> ys;

    for (std::size_t i = 0; i < slots.size(); ++i) {
        // THE RIM, tested against the drawn run and with the same measure the
        // polyline uses. A closed-form distance to an ellipse has no elementary
        // solution and every approximation of one is wrong somewhere; the run is
        // what the user sees and what they aimed at.
        const bool closed = ellipse_run(geom, slots[i], xs, ys);

        out[i]                 = 0;
        const std::size_t segs = xs.size() < 2 ? 0 : (closed ? xs.size() : xs.size() - 1);
        for (std::size_t v = 0; v < segs && out[i] == 0; ++v) {
            const Point2 a{xs[v], ys[v]};
            const Point2 b{xs[(v + 1) % xs.size()], ys[(v + 1) % ys.size()]};
            if (segment_distance2_m(a, b, probe) <= limit) out[i] = 1;
        }
    }
}

void ellipse_write(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
                   std::vector<std::uint32_t>& ends)
{
    for (const std::uint32_t slot : slots) {
        const Point2 c = ellipse_centre_of(geom, slot);
        const Point2 a = ellipse_major_of(geom, slot);
        const Point2 b = ellipse_minor_of(geom, slot);
        put_mm(bytes, c.x);
        put_mm(bytes, c.y);
        put_mm(bytes, a.x);
        put_mm(bytes, a.y);
        put_mm(bytes, b.x);
        put_mm(bytes, b.y);
        ends.push_back(static_cast<std::uint32_t>(bytes.size()));
    }
}

Result<std::uint32_t> ellipse_read(RingGeometry& geom, std::span<const std::uint8_t> payload)
{
    Reader in(payload);
    if (!in.remaining(48))
        return err(ErrorCode::ParseError,
                   "Elips yükü merkez ve iki eksen ucunu taşıyacak kadar uzun değil.");

    const Mm cx = in.mm();
    const Mm cy = in.mm();
    const Mm ax = in.mm();
    const Mm ay = in.mm();
    const Mm bx = in.mm();
    const Mm by = in.mm();

    if (cx == ax && cy == ay)
        return err(ErrorCode::ParseError, "Elipsin birinci ekseni sıfır uzunlukta.");
    if (cx == bx && cy == by)
        return err(ErrorCode::ParseError, "Elipsin ikinci ekseni sıfır uzunlukta.");

    const Point2 pts[3]{Point2{cx, cy}, Point2{ax, ay}, Point2{bx, by}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 3), RingRole::Open, 0};
    return geom.append(std::span<const RingGeometry::RingInput>(&ring, 1));
}

KENTOS_KIND(ellipse)
{
    KindSpec s{};
    s.id         = kEllipseKind;
    s.stable_id  = "core.ellipse";
    s.summary_tr = "Merkez ve iki eksen ucuyla tanımlı elips.";
    s.names[0]   = "ELİPS";
    s.names[1]   = "ELIPS";
    s.names[2]   = "ELLIPSE";
    s.names[3]   = "EL";
    s.bbox       = &ellipse_bbox;
    s.outline    = &ellipse_outline_fn;
    s.hit        = &ellipse_hit;
    s.area       = &ellipse_area;
    s.perimeter  = &ellipse_perimeter;
    s.read       = &ellipse_read;
    s.write      = &ellipse_write;
    s.validate   = &ellipse_validate;
    s.key_points = &ellipse_key_points;
    return s;
}

KENTOS_KIND(polyline)
{
    KindSpec s{};
    s.id         = 1;
    s.stable_id  = "core.polyline";
    s.summary_tr = "Açık ya da kapalı halkalardan oluşan temel çizgi nesnesi.";
    s.names[0]   = "ÇOKLUÇİZGİ";
    s.names[1]   = "COKLUCIZGI";
    s.names[2]   = "POLYLINE";
    s.names[3]   = "PL";
    s.bbox       = &polyline_bbox;
    s.outline    = &polyline_outline;
    s.hit        = &polyline_hit;
    s.area       = &polyline_area;
    s.perimeter  = &polyline_perimeter;
    s.read       = &polyline_read;
    s.write      = &polyline_write;
    s.validate   = &polyline_validate;
    return s;
}

// ----------------------------------------------------------- KindTable -------

Status KindTable::add(const KindSpec& spec)
{
    if (spec.id == kNoKind) return err(ErrorCode::InvalidArgument, "Nesne türü kimliği atanmamış.");
    if (spec.stable_id == nullptr || *spec.stable_id == '\0')
        return err(ErrorCode::InvalidArgument,
                   "Nesne türü kalıcı kimliği boş olamaz (örnek: \"core.polyline\").");
    if (find(spec.id) != nullptr)
        return err(ErrorCode::ValidationFailed,
                   "Nesne türü kimliği zaten kayıtlı: " + std::to_string(spec.id));

    // A null here would become an indirect call through zero in the middle of a
    // frame; refusing at registration turns a crash into a message.
    if (spec.bbox == nullptr || spec.outline == nullptr || spec.hit == nullptr ||
        spec.area == nullptr || spec.read == nullptr || spec.write == nullptr)
        return err(ErrorCode::InvalidArgument, std::string("'") + spec.stable_id +
                                                   "' nesne türünün eksik işlev işaretçisi var.");

    if (spec.names[0] == nullptr || *spec.names[0] == '\0')
        return err(ErrorCode::InvalidArgument,
                   std::string("'") + spec.stable_id + "' nesne türünün Türkçe adı yok.");

    std::vector<std::pair<std::string, KindId>> pending;
    for (const char* n : spec.names) {
        if (n == nullptr || *n == '\0') continue;
        std::string folded = turkish_fold_key(n);
        if (find_name(folded) != nullptr)
            return err(ErrorCode::ValidationFailed, "Nesne türü adı zaten kullanılıyor: " + folded);

        // A repeat WITHIN one spec is not a defect and must not be one: the names
        // are declared as a Turkish spelling and its ASCII fold (CLAUDE.md 2.6),
        // and a lookup key folds the two alphabets together on purpose, so
        // `ÇOKLUÇİZGİ` and `COKLUCIZGI` arrive as the same key. They name the same
        // kind, so the second is simply already accounted for. Across two specs it
        // is still an error, and that is the check above.
        bool already = false;
        for (const auto& p : pending)
            if (p.first == folded) already = true;
        if (already) continue;

        pending.emplace_back(std::move(folded), spec.id);
    }

    specs_.push_back(spec);
    for (auto& p : pending) {
        const auto at =
            std::lower_bound(folded_.begin(), folded_.end(), p.first,
                             [](const auto& e, const std::string& k) { return e.first < k; });
        folded_.insert(at, std::move(p));
    }
    return ok();
}

const KindSpec* KindTable::find(KindId id) const noexcept
{
    for (const auto& s : specs_)
        if (s.id == id) return &s;
    return nullptr;
}

const KindSpec* KindTable::find_name(std::string_view name) const
{
    const std::string folded = turkish_fold_key(name);
    const auto at =
        std::lower_bound(folded_.begin(), folded_.end(), folded,
                         [](const auto& e, const std::string& k) { return e.first < k; });
    if (at == folded_.end() || at->first != folded) return nullptr;
    return find(at->second);
}

// The one and only list of built-in entity kinds (R25). Adding a kind means one
// factory above and one line here — the same idiom as the command list, and the
// integer id lives with the factory because it reaches the file format.
#define KENTOS_BUILTIN_KINDS(X)                                                                    \
    X(polyline)                                                                                    \
    X(circle)                                                                                      \
    X(arc)                                                                                         \
    X(point)                                                                                       \
    X(ellipse)                                                                                     \
    X(arc_polyline)                                                                                \
    X(spline)                                                                                      \
    X(hatch)                                                                                       \
    X(block_reference)                                                                             \
    X(dimension)                                                                                   \
    X(leader)

bool curve_outline(KindId kind, const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into)
{
    if (kind == kPolylineKind) return false;

    const KindSpec* spec = builtin_kinds().find(kind);
    if (spec == nullptr) return false;

    into.clear();
    const std::uint32_t one[1]{slot};
    spec->outline(geom, SlotSpan(one, 1), into);
    return true;
}

const KindTable& builtin_kinds()
{
    // Immutable from the first read onward: built once, never added to, handed
    // out as const. That is const state, which core.md P8 permits — it is not a
    // registry, and nothing can reach in and change what "core.polyline" means.
    static const KindTable table = [] {
        KindTable t;
#define KENTOS_REGISTER_KIND(sym) (void)t.add(kentos_kind_##sym());
        KENTOS_BUILTIN_KINDS(KENTOS_REGISTER_KIND)
#undef KENTOS_REGISTER_KIND
        return t;
    }();
    return table;
}

#undef KENTOS_BUILTIN_KINDS

} // namespace kentos::core
