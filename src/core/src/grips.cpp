// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/grips.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace kentos::core {
namespace {

/// The stored rings of `e`, unpacked so they can be edited and handed back.
GripEdit read_edit(const Document& doc, EntityId e)
{
    GripEdit out;
    const RingGeometry& geom  = doc.geometry();
    const std::uint32_t gslot = doc.entities().slot[e];
    const RingSpan span       = geom.rings_of(gslot);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        std::vector<Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(Point2{xs[v], ys[v]});
        out.points.push_back(std::move(pts));
        out.roles.push_back(geom.ring_role[r]);
        out.parts.push_back(geom.ring_part[r]);
    }
    const auto bytes = geom.payload_of(gslot);
    out.payload.assign(bytes.begin(), bytes.end());
    return out;
}

/// The vertex `index` names, counted across the rings in R11 order.
bool locate(const GripEdit& edit, std::size_t index, std::size_t& ring, std::size_t& at)
{
    std::size_t remaining = index;
    for (std::size_t r = 0; r < edit.points.size(); ++r) {
        if (remaining < edit.points[r].size()) {
            ring = r;
            at   = remaining;
            return true;
        }
        remaining -= edit.points[r].size();
    }
    return false;
}

double length(Point2 a, Point2 b)
{
    const auto dx = static_cast<double>(b.x - a.x);
    const auto dy = static_cast<double>(b.y - a.y);
    return std::sqrt(dx * dx + dy * dy);
}

Point2 shifted(Point2 p, Mm dx, Mm dy)
{
    return Point2{p.x + dx, p.y + dy};
}

Error no_such_grip(std::size_t index, std::size_t have)
{
    return err(ErrorCode::ValidationFailed, "Bu nesnenin " + std::to_string(index + 1) +
                                                ". tutamağı yok; " + std::to_string(have) +
                                                " tutamağı var.");
}

// ------------------------------------------------------------------ circle ----

std::vector<GripPoint> circle_grips(const RingGeometry& geom, std::uint32_t slot)
{
    const Point2 c = circle_centre_of(geom, slot);
    const Mm r     = circle_radius_of(geom, slot);
    return {GripPoint{c, GripRole::Centre}, GripPoint{Point2{c.x + r, c.y}, GripRole::Radius},
            GripPoint{Point2{c.x, c.y + r}, GripRole::Radius},
            GripPoint{Point2{c.x - r, c.y}, GripRole::Radius},
            GripPoint{Point2{c.x, c.y - r}, GripRole::Radius}};
}

Result<GripEdit> circle_move(GripEdit edit, std::size_t index, Point2 to)
{
    if (edit.points.empty() || edit.points[0].size() < 2) return no_such_grip(index, 0);
    const Point2 c = edit.points[0][0];
    if (index == 0) {
        const Mm dx = to.x - c.x;
        const Mm dy = to.y - c.y;
        for (Point2& p : edit.points[0])
            p = shifted(p, dx, dy);
        return edit;
    }
    if (index > 4) return no_such_grip(index, 5);
    const Mm r = mm_round(length(c, to));
    if (r <= 0)
        return err(ErrorCode::ValidationFailed,
                   "Yarıçap sıfır: tutamak merkezin üstünde. Merkezden uzak bir yer seçin.");
    edit.points[0][1] = Point2{c.x + r, c.y};
    return edit;
}

// --------------------------------------------------------------------- arc ----

std::vector<GripPoint> arc_grips(const RingGeometry& geom, std::uint32_t slot)
{
    const Point2 c   = arc_centre_of(geom, slot);
    const Mm r       = arc_radius_of(geom, slot);
    const Point2 s   = arc_start_of(geom, slot);
    const Point2 f   = arc_end_of(geom, slot);
    const Point2 mid = arc_midpoint(c, r, s, f);
    return {GripPoint{c, GripRole::Centre}, GripPoint{s, GripRole::Endpoint},
            GripPoint{f, GripRole::Endpoint}, GripPoint{mid, GripRole::Radius}};
}

Result<GripEdit> arc_move(GripEdit edit, std::size_t index, Point2 to)
{
    if (edit.points.empty() || edit.points[0].size() < 4) return no_such_grip(index, 0);
    std::vector<Point2>& v = edit.points[0];
    const Point2 c         = v[0];
    if (index == 0) {
        const Mm dx = to.x - c.x;
        const Mm dy = to.y - c.y;
        for (Point2& p : v)
            p = shifted(p, dx, dy);
        return edit;
    }
    if (index > 3) return no_such_grip(index, 4);
    // THREE POINTS, AND THE ONES NOT HELD STAY PUT. An end dragged re-fits the
    // arc through it, the midpoint and the other end; the midpoint dragged
    // re-fits it through both ends — which is what an arc does under a grip in
    // every CAD. The old reading kept the centre and let the radius follow the
    // handle, which carried the OTHER end off whatever it met and left the
    // stored end off the circle the record declares.
    const Mm r     = v[1].x - c.x;
    Point2 a       = v[2];
    Point2 b       = v[3];
    Point2 through = arc_midpoint(c, r, a, b);
    // A grip moved to where it already is changes nothing — which is what
    // ESNET asks of the ends and the midpoint once the centre has carried the
    // whole arc, and a re-fit through rounded points would drift it.
    if ((index == 1 && to == a) || (index == 2 && to == b) || (index == 3 && to == through))
        return edit;
    if (index == 1) a = to;
    if (index == 2) b = to;
    if (index == 3) through = to;
    if (a == b)
        return err(ErrorCode::ValidationFailed,
                   "Yayın iki ucu aynı noktaya düşüyor. Tutamağı öbür uçtan uzak bir yere "
                   "götürün.");
    Point2 centre{};
    Mm radius = 0;
    if (!circumcircle(a, through, b, centre, radius) || radius <= 0)
        return err(ErrorCode::ValidationFailed,
                   "Üç nokta aynı doğru üzerinde; yay düzleşir. Tutamağı iki ucu birleştiren "
                   "doğrunun dışına götürün.");
    // Which way round: the arc runs from `a` through `through` to `b`, and is
    // stored counter-clockwise, so a clockwise run is stored from `b` to `a`.
    const double turn = static_cast<double>(through.x - a.x) * static_cast<double>(b.y - a.y) -
                        static_cast<double>(through.y - a.y) * static_cast<double>(b.x - a.x);
    const bool ccw = turn > 0.0;
    v[0]           = centre;
    v[1]           = Point2{centre.x + radius, centre.y};
    v[2]           = ccw ? a : b;
    v[3]           = ccw ? b : a;
    return edit;
}

// ----------------------------------------------------------------- ellipse ----

std::vector<GripPoint> ellipse_grips(const RingGeometry& geom, std::uint32_t slot)
{
    const Point2 c  = ellipse_centre_of(geom, slot);
    const Point2 a  = ellipse_major_of(geom, slot);
    const Point2 b  = ellipse_minor_of(geom, slot);
    const Point2 a2 = Point2{2 * c.x - a.x, 2 * c.y - a.y};
    const Point2 b2 = Point2{2 * c.x - b.x, 2 * c.y - b.y};
    return {GripPoint{c, GripRole::Centre}, GripPoint{a, GripRole::AxisEnd},
            GripPoint{b, GripRole::AxisEnd}, GripPoint{a2, GripRole::AxisEnd},
            GripPoint{b2, GripRole::AxisEnd}};
}

Result<GripEdit> ellipse_move(GripEdit edit, std::size_t index, Point2 to)
{
    if (edit.points.empty() || edit.points[0].size() < 3) return no_such_grip(index, 0);
    std::vector<Point2>& v = edit.points[0];
    const Point2 c         = v[0];
    if (index == 0) {
        const Mm dx = to.x - c.x;
        const Mm dy = to.y - c.y;
        for (Point2& p : v)
            p = shifted(p, dx, dy);
        return edit;
    }
    if (index > 4) return no_such_grip(index, 5);
    // A mirror-image handle is the axis end reflected through the centre.
    const Point2 reach = index >= 3 ? Point2{2 * c.x - to.x, 2 * c.y - to.y} : to;
    const bool major   = index == 1 || index == 3;
    if (major) {
        // The first axis turns and stretches to the handle; the second keeps its
        // length and stays perpendicular, on the side it was.
        const double al = length(c, reach);
        if (al < 1.0)
            return err(
                ErrorCode::ValidationFailed,
                "Birinci eksen sıfır: tutamak merkezin üstünde. Merkezden uzak bir yer seçin.");
        const double bl   = length(c, v[2]);
        const auto old_ax = static_cast<double>(v[1].x - c.x);
        const auto old_ay = static_cast<double>(v[1].y - c.y);
        const auto old_bx = static_cast<double>(v[2].x - c.x);
        const auto old_by = static_cast<double>(v[2].y - c.y);
        const double side = old_ax * old_by - old_ay * old_bx < 0.0 ? -1.0 : 1.0;
        const auto ax     = static_cast<double>(reach.x - c.x) / al;
        const auto ay     = static_cast<double>(reach.y - c.y) / al;
        v[1]              = reach;
        v[2]              = Point2{c.x + mm_round(-ay * bl * side), c.y + mm_round(ax * bl * side)};
        return edit;
    }
    // The second axis: how far the handle reaches ACROSS the first axis, on the
    // side it reached.
    const auto ax      = static_cast<double>(v[1].x - c.x);
    const auto ay      = static_cast<double>(v[1].y - c.y);
    const double al    = std::sqrt(ax * ax + ay * ay);
    const auto rx      = static_cast<double>(reach.x - c.x);
    const auto ry      = static_cast<double>(reach.y - c.y);
    const double cross = al == 0.0 ? 0.0 : (rx * -ay + ry * ax) / al;
    if (std::abs(cross) < 1.0)
        return err(
            ErrorCode::ValidationFailed,
            "İkinci eksen sıfır: tutamak birinci eksenin üzerinde. Eksene dik bir yer seçin.");
    v[2] = Point2{c.x + mm_round(-ay / al * cross), c.y + mm_round(ax / al * cross)};
    return edit;
}

// ------------------------------------------------------------ arc polyline ----

std::vector<GripPoint> arc_polyline_grips(const RingGeometry& geom, std::uint32_t slot)
{
    std::vector<GripPoint> out;
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return out;
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(GripPoint{Point2{xs[v], ys[v]}, GripRole::Vertex});
    auto def = arc_polyline_of(geom, slot);
    if (!def) return out;
    const std::size_t n = xs.size();
    for (const ArcPolyline::Arc& arc : def.value().arcs) {
        if (arc.segment >= n) continue;
        const Point2 a{xs[arc.segment], ys[arc.segment]};
        const Point2 b{xs[(arc.segment + 1) % n], ys[(arc.segment + 1) % n]};
        const Point2 mid = arc.ccw ? arc_midpoint(arc.centre, arc.radius, a, b)
                                   : arc_midpoint(arc.centre, arc.radius, b, a);
        out.push_back(GripPoint{mid, GripRole::ArcMid});
    }
    return out;
}

Result<GripEdit> arc_polyline_move(GripEdit edit, std::size_t index, Point2 to)
{
    if (edit.points.empty()) return no_such_grip(index, 0);
    std::vector<Point2>& v = edit.points[0];
    const std::size_t n    = v.size();
    const bool closed      = edit.roles[0] != RingRole::Open;
    auto decoded           = decode_arc_polyline(edit.payload);
    if (!decoded) return decoded.error();
    ArcPolyline def = std::move(decoded.value());

    if (index < n) {
        // A corner moves; every bend that meets it keeps its BULGE — the shape
        // of the bend relative to its chord — and is re-fitted to the new chord,
        // which is what a CAD user sees an arc segment do under a dragged
        // corner. A bend that cannot be re-fitted (its chord collapsed) is
        // dropped: a straight edge, not a refusal.
        const std::size_t segs = closed ? n : n - 1;
        std::vector<ArcPolyline::Arc> kept;
        for (ArcPolyline::Arc arc : def.arcs) {
            const std::size_t from = arc.segment;
            const std::size_t upto = (arc.segment + 1) % n;
            if (from != index && upto != index) {
                kept.push_back(arc);
                continue;
            }
            if (arc.segment >= segs) continue;
            const Point2 a     = v[from];
            const Point2 b     = v[upto];
            const double bulge = bulge_from_arc(a, b, arc.centre, arc.radius, arc.ccw);
            const Point2 a2    = from == index ? to : a;
            const Point2 b2    = upto == index ? to : b;
            Point2 centre{};
            Mm radius = 0;
            bool ccw  = true;
            if (arc_from_bulge(a2, b2, bulge, centre, radius, ccw) && radius > 0) {
                arc.centre = centre;
                arc.radius = radius;
                arc.ccw    = ccw;
                kept.push_back(arc);
            }
        }
        v[index]     = to;
        def.arcs     = std::move(kept);
        edit.payload = encode_arc_polyline(def);
        return edit;
    }

    // The midpoint of a bend: the bend becomes the circle through its two ends
    // and the handle. On the chord it becomes straight.
    const std::size_t which = index - n;
    if (which >= def.arcs.size()) return no_such_grip(index, n + def.arcs.size());
    ArcPolyline::Arc& arc = def.arcs[which];
    const Point2 a        = v[arc.segment];
    const Point2 b        = v[(arc.segment + 1) % n];
    const auto ax = static_cast<double>(a.x - to.x), ay = static_cast<double>(a.y - to.y);
    const auto bx = static_cast<double>(b.x - to.x), by = static_cast<double>(b.y - to.y);
    const double d = 2.0 * (ax * by - ay * bx);
    if (std::abs(d) < 1.0) {
        def.arcs.erase(def.arcs.begin() + static_cast<std::ptrdiff_t>(which));
        edit.payload = encode_arc_polyline(def);
        return edit;
    }
    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double ux = (by * a2 - ay * b2) / d;
    const double uy = (ax * b2 - bx * a2) / d;
    arc.centre      = Point2{to.x + mm_round(ux), to.y + mm_round(uy)};
    arc.radius      = mm_round(std::sqrt(ux * ux + uy * uy));
    // A counter-clockwise bend bulges to the RIGHT of its chord.
    const auto cx = static_cast<double>(b.x - a.x), cy = static_cast<double>(b.y - a.y);
    const auto tx = static_cast<double>(to.x - a.x), ty = static_cast<double>(to.y - a.y);
    arc.ccw = cx * ty - cy * tx < 0.0;
    if (arc.radius <= 0) {
        def.arcs.erase(def.arcs.begin() + static_cast<std::ptrdiff_t>(which));
    }
    edit.payload = encode_arc_polyline(def);
    return edit;
}

// --------------------------------------------------------------- dimension ----

std::vector<GripPoint> dimension_grips(const RingGeometry& geom, std::uint32_t slot)
{
    std::vector<GripPoint> out;
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count < 2) return out;
    const auto xs = geom.ring_xs(rs.first + 1);
    const auto ys = geom.ring_ys(rs.first + 1);
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(GripPoint{Point2{xs[v], ys[v]}, GripRole::Definition});
    const auto bx = geom.ring_xs(rs.first);
    const auto by = geom.ring_ys(rs.first);
    if (!bx.empty()) out.push_back(GripPoint{Point2{bx[0], by[0]}, GripRole::Caption});
    return out;
}

Result<GripEdit> dimension_move(const Document& doc, EntityId e, GripEdit edit, std::size_t index,
                                Point2 to)
{
    if (edit.points.size() < 2 || edit.points[0].size() < 2) return no_such_grip(index, 0);
    std::vector<Point2>& base = edit.points[0];
    std::vector<Point2>& defs = edit.points[1];
    auto decoded              = decode_dimension(edit.payload);
    if (!decoded) return decoded.error();
    DimensionDef def = std::move(decoded.value());

    if (index == defs.size()) {
        // The caption slides; nothing else moves.
        const Mm dx = to.x - base[0].x;
        const Mm dy = to.y - base[0].y;
        for (Point2& p : base)
            p = shifted(p, dx, dy);
        return edit;
    }
    if (index > defs.size()) return no_such_grip(index, defs.size() + 1);

    std::vector<Point2> picks;
    Point2 where{};
    const bool relaid = dimension_picks(def.type, defs, base[0], picks, where);
    defs[index]       = to;
    if (!relaid) {
        // An ordinate or a four-point angular dimension (from a file): the point
        // moves and the figure is re-measured; the caption stays where it was.
        def.measurement = dimension_measure(def.type, defs, def.rotation_udeg);
        edit.payload    = encode_dimension(def);
        return edit;
    }
    // Re-lay the dimension out from the edited picks, exactly as ÖLÇÜ laid it
    // out from the clicks — so the line, the extension lines and the caption
    // follow the point rather than only the point moving.
    if (!dimension_picks(def.type, defs, base[0], picks, where)) return no_such_grip(index, 0);
    const Mm height = doc.texts().height(doc.entities().slot[e]);
    DimensionLayout layout;
    if (!dimension_layout(def, picks, where, height, layout))
        return err(ErrorCode::ValidationFailed,
                   "Ölçü bu noktayla kurulamıyor: iki nokta çakıştı ya da tepe kolun ucuna geldi.");
    defs                = layout.defs;
    edit.caption_centre = layout.text_centre;
    edit.caption_dir_x  = layout.text_dir_x;
    edit.caption_dir_y  = layout.text_dir_y;
    const auto bl = dimension_baseline(layout.text_centre, layout.text_dir_x, layout.text_dir_y,
                                       height, doc.texts().text(doc.entities().slot[e]));
    base          = {bl[0], bl[1]};
    edit.payload  = encode_dimension(def);
    return edit;
}

// --------------------------------------------------------- block reference ----

/// How far out a block reference's turning handle sits: to the far corner of
/// what it draws, so the handle is on the symbol's own scale — and at least a
/// metre, so a point-sized symbol's handle is not on top of its insertion.
Mm turning_reach(Point2 insertion, const Box2& bounds)
{
    double far = 1000.0;
    for (const Point2 corner :
         {Point2{bounds.min_x, bounds.min_y}, Point2{bounds.max_x, bounds.min_y},
          Point2{bounds.max_x, bounds.max_y}, Point2{bounds.min_x, bounds.max_y}})
        far = std::max(far, length(insertion, corner));
    return mm_round(far);
}

/// Where the turning handle of a reference at `insertion` sits.
Point2 turning_handle(Point2 insertion, const BlockReference& ref)
{
    const Mm reach    = turning_reach(insertion, ref.bounds);
    const SinCos turn = sin_cos_udeg(ref.rotation_udeg);
    return Point2{insertion.x + mm_round(static_cast<double>(reach) * turn.cos),
                  insertion.y + mm_round(static_cast<double>(reach) * turn.sin)};
}

std::vector<GripPoint> block_reference_grips(const RingGeometry& geom, std::uint32_t slot)
{
    const Point2 at = block_reference_insertion(geom, slot);
    std::vector<GripPoint> out{GripPoint{at, GripRole::Insertion}};
    if (auto ref = decode_block_reference(geom.payload_of(slot)))
        out.push_back(GripPoint{turning_handle(at, ref.value()), GripRole::Rotation});
    return out;
}

Result<GripEdit> block_reference_move(const Document& doc, GripEdit edit, std::size_t index,
                                      Point2 to)
{
    if (index > 1 || edit.points.empty() || edit.points[0].empty()) return no_such_grip(index, 2);
    auto decoded = decode_block_reference(edit.payload);
    if (!decoded) return decoded.error();
    BlockReference ref = std::move(decoded.value());
    if (index == 1) {
        // THE TURNING HANDLE: the reference turns about its insertion point to
        // face the handle, its scale and its copies as they were.
        const Point2 at = edit.points[0][0];
        if (to == at)
            return err(ErrorCode::ValidationFailed,
                       "Döndürme tutamağı ekleme noktasının üstünde; açıyı gösteren bir yer "
                       "seçin.");
        ref.rotation_udeg = atan2_udeg(to.y - at.y, to.x - at.x);
        ref.bounds        = block_reference_bounds(doc, at, ref);
        edit.payload      = encode_block_reference(ref);
        return edit;
    }
    edit.points[0][0] = to;
    ref.bounds        = block_reference_bounds(doc, to, ref);
    edit.payload      = encode_block_reference(ref);
    return edit;
}

// ------------------------------------------------------------------ spline ----

/// A spline's grips are its CONTROL points, ring 0: the points the curve is
/// drawn from. The fit points of ring 1 are a source file's record of where
/// the curve was meant to pass, and moving one would move nothing on screen.
std::vector<GripPoint> spline_grips(const RingGeometry& geom, std::uint32_t slot)
{
    std::vector<GripPoint> out;
    const RingSpan rs = geom.rings_of(slot);
    if (rs.count == 0) return out;
    const auto xs = geom.ring_xs(rs.first);
    const auto ys = geom.ring_ys(rs.first);
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(GripPoint{Point2{xs[v], ys[v]}, GripRole::Control});
    return out;
}

Result<GripEdit> spline_move(GripEdit edit, std::size_t index, Point2 to)
{
    if (edit.points.empty() || index >= edit.points[0].size())
        return no_such_grip(index, edit.points.empty() ? 0 : edit.points[0].size());
    edit.points[0][index] = to;
    // THE FIT POINTS NO LONGER DESCRIBE THE CURVE once a control point moves:
    // kept, a file written from this record would say the curve passes where it
    // no longer does. So they go, and the record says it has none.
    if (edit.points.size() > 1) {
        auto def = decode_spline(edit.payload);
        if (!def) return def.error();
        def.value().has_fit = false;
        edit.points.resize(1);
        edit.roles.resize(1);
        edit.parts.resize(1);
        edit.payload = encode_spline(def.value());
    }
    return edit;
}

// ----------------------------------------------------------------- generic ----

std::vector<GripPoint> vertex_grips(const RingGeometry& geom, std::uint32_t slot)
{
    std::vector<GripPoint> out;
    const RingSpan rs = geom.rings_of(slot);
    for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        for (std::size_t v = 0; v < xs.size(); ++v)
            out.push_back(GripPoint{Point2{xs[v], ys[v]}, GripRole::Vertex});
    }
    return out;
}

Result<GripEdit> vertex_move(GripEdit edit, std::size_t index, Point2 to)
{
    std::size_t ring = 0;
    std::size_t at   = 0;
    std::size_t have = 0;
    for (const auto& r : edit.points)
        have += r.size();
    if (!locate(edit, index, ring, at)) return no_such_grip(index, have);
    edit.points[ring][at] = to;
    return edit;
}

} // namespace

std::vector<RingGeometry::RingInput> GripEdit::inputs() const
{
    std::vector<RingGeometry::RingInput> out;
    out.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
        out.push_back(RingGeometry::RingInput{points[i], roles[i], parts[i]});
    return out;
}

std::vector<GripPoint> entity_grips(const Document& doc, EntityId e)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e) || !doc.editable(e)) return {};
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];
    switch (ents.kind[e]) {
    case kCircleKind: return circle_grips(geom, slot);
    case kArcKind: return arc_grips(geom, slot);
    case kEllipseKind: return ellipse_grips(geom, slot);
    case kArcPolylineKind: return arc_polyline_grips(geom, slot);
    case kDimensionKind: return dimension_grips(geom, slot);
    case kBlockReferenceKind: return block_reference_grips(geom, slot);
    case kSplineKind: return spline_grips(geom, slot);
    default: return vertex_grips(geom, slot);
    }
}

namespace {

/// One grip move applied to `edit`, the entity's state so far — the step
/// `move_grip` takes once and `move_grips` takes in turn.
Result<GripEdit> apply_move(const Document& doc, EntityId e, GripEdit edit, std::size_t index,
                            Point2 to)
{
    switch (doc.entities().kind[e]) {
    case kCircleKind: return circle_move(std::move(edit), index, to);
    case kArcKind: return arc_move(std::move(edit), index, to);
    case kEllipseKind: return ellipse_move(std::move(edit), index, to);
    case kArcPolylineKind: return arc_polyline_move(std::move(edit), index, to);
    case kDimensionKind: return dimension_move(doc, e, std::move(edit), index, to);
    case kBlockReferenceKind: return block_reference_move(doc, std::move(edit), index, to);
    case kSplineKind: return spline_move(std::move(edit), index, to);
    default: return vertex_move(std::move(edit), index, to);
    }
}

/// Why `e` cannot be edited at all, or nothing.
std::optional<Error> not_editable(const Document& doc, EntityId e)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e))
        return err(ErrorCode::NotFound, "Nesne bulunamadı veya silinmiş.");
    if (auto st = doc.editable(e); !st) return st.error();
    return std::nullopt;
}

} // namespace

Result<GripEdit> move_grip(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    if (auto why = not_editable(doc, e)) return *why;
    return apply_move(doc, e, read_edit(doc, e), index, to);
}

Result<GripEdit> move_grips(const Document& doc, EntityId e, std::span<const GripMove> moves)
{
    if (auto why = not_editable(doc, e)) return *why;
    GripEdit state = read_edit(doc, e);
    for (const GripMove& m : moves) {
        auto next = apply_move(doc, e, std::move(state), m.index, m.to);
        if (!next) return next.error();
        state = std::move(next.value());
    }
    return state;
}

bool edit_preview(const Document& doc, EntityId e, const GripEdit& edit, EmitBuffer& into)
{
    // The edited shape in a scratch arena, drawn by the kind's own outline: the
    // preview IS the future drawing, computed once and the same way.
    RingGeometry scratch;
    const auto inputs = edit.inputs();
    auto slot         = scratch.append(inputs, edit.payload);
    if (!slot) return false;
    const KindId kind = doc.entities().kind[e];
    if (curve_outline(kind, scratch, slot.value(), into)) return true;
    const RingSpan rs = scratch.rings_of(slot.value());
    for (std::uint32_t r = rs.first; r < rs.first + rs.count; ++r) {
        into.begin_run(scratch.ring_role[r] != RingRole::Open,
                       scratch.ring_role[r] == RingRole::Interior);
        const auto xs = scratch.ring_xs(r);
        const auto ys = scratch.ring_ys(r);
        for (std::size_t v = 0; v < xs.size(); ++v)
            into.push_vertex(xs[v], ys[v]);
    }
    return true;
}

bool grip_preview(const Document& doc, EntityId e, std::size_t index, Point2 to, EmitBuffer& into)
{
    auto edit = move_grip(doc, e, index, to);
    return edit && edit_preview(doc, e, edit.value(), into);
}

Result<std::optional<Stretched>> stretch_entity(const Document& doc, EntityId e, const Box2& window,
                                                Mm dx, Mm dy)
{
    if (auto why = not_editable(doc, e)) return *why;
    const auto inside = [&window](Point2 p) {
        return p.x >= window.min_x && p.x <= window.max_x && p.y >= window.min_y &&
               p.y <= window.max_y;
    };

    // A POLYLINE'S CORNERS ARE ITS GRIPS and each moves on its own, so they are
    // moved in one pass over the rings rather than one search per corner.
    if (doc.entities().kind[e] == kPolylineKind) {
        Stretched out{e, read_edit(doc, e), 0};
        for (std::vector<Point2>& ring : out.edit.points)
            for (Point2& p : ring)
                if (inside(p)) {
                    p = shifted(p, dx, dy);
                    ++out.moved;
                }
        if (out.moved == 0) return std::optional<Stretched>{};
        return std::optional<Stretched>{std::move(out)};
    }

    // EVERY OTHER KIND GRIP BY GRIP, each to where it WAS plus the offset —
    // positions taken once, before the first move. A circle's centre already
    // carries its radius handle, so the handle's own move then lands where the
    // handle is and changes nothing: windowing a whole circle translates it.
    const std::vector<GripPoint> grips = entity_grips(doc, e);
    std::vector<GripMove> moves;
    // A block's turning handle is not a place on the drawing: a window over it
    // does not turn the block, which moves with its insertion point or not at all.
    for (std::size_t i = 0; i < grips.size(); ++i)
        if (grips[i].role != GripRole::Rotation && inside(grips[i].at))
            moves.push_back(GripMove{i, shifted(grips[i].at, dx, dy)});
    if (moves.empty()) return std::optional<Stretched>{};

    auto edit = move_grips(doc, e, moves);
    if (!edit) return edit.error();
    return std::optional<Stretched>{Stretched{e, std::move(edit.value()), moves.size()}};
}

std::vector<std::uint8_t> encode_stretch_guide(const StretchGuide& guide)
{
    // version, the window, the key count, the keys — little-endian as the
    // machine writes them, because the bytes never leave the process.
    std::vector<std::uint8_t> bytes(1 + sizeof(Box2) + sizeof(std::uint32_t) +
                                    guide.keys.size() * sizeof(std::int64_t));
    bytes[0]       = 1;
    std::size_t at = 1;
    const auto put = [&bytes, &at](const void* from, std::size_t n) {
        std::memcpy(bytes.data() + at, from, n);
        at += n;
    };
    const std::array<Mm, 4> box{guide.window.min_x, guide.window.min_y, guide.window.max_x,
                                guide.window.max_y};
    put(box.data(), sizeof(box));
    const auto count = static_cast<std::uint32_t>(guide.keys.size());
    put(&count, sizeof(count));
    if (count != 0) put(guide.keys.data(), guide.keys.size() * sizeof(std::int64_t));
    return bytes;
}

Result<StretchGuide> decode_stretch_guide(std::span<const std::uint8_t> bytes)
{
    const std::size_t head = 1 + 4 * sizeof(Mm) + sizeof(std::uint32_t);
    if (bytes.size() < head || bytes[0] != 1)
        return err(ErrorCode::InvalidArgument, "Esnetme önizlemesinin baytları tanınmıyor.");
    StretchGuide guide;
    std::array<Mm, 4> box{};
    std::memcpy(box.data(), bytes.data() + 1, sizeof(box));
    guide.window        = Box2{box[0], box[1], box[2], box[3]};
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 1 + sizeof(box), sizeof(count));
    if (bytes.size() != head + count * sizeof(std::int64_t))
        return err(ErrorCode::InvalidArgument, "Esnetme önizlemesinin baytları tanınmıyor.");
    guide.keys.resize(count);
    if (count != 0)
        std::memcpy(guide.keys.data(), bytes.data() + head, count * sizeof(std::int64_t));
    return guide;
}

Result<GripEdit> insert_vertex(const Document& doc, EntityId e, std::size_t after, Point2 at)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e))
        return err(ErrorCode::NotFound, "Nesne bulunamadı veya silinmiş.");
    if (const auto st = doc.editable(e); !st) return st.error();
    // A CURVE HAS NO CORNERS TO ADD. Its stored vertices are its definition —
    // a circle's are a centre and a radius handle — and one more would leave a
    // record that is no longer that kind at all.
    if (ents.kind[e] != kPolylineKind || doc.texts().has(ents.slot[e]))
        return err(ErrorCode::InvalidArgument,
                   "Bu nesne bir eğri ya da yazı; araya köşe eklenemez. Tutamaklarını KÖŞETAŞI "
                   "ile taşıyabilirsiniz.");

    GripEdit edit    = read_edit(doc, e);
    std::size_t ring = 0;
    std::size_t pos  = 0;
    std::size_t have = 0;
    for (const auto& r : edit.points)
        have += r.size();
    if (!locate(edit, after, ring, pos)) return no_such_grip(after, have);

    // `after` names the corner the new one follows, so it names a SEGMENT: the
    // one leaving that corner. On a closed ring the last corner's segment is the
    // closing edge — a parcel's closing edge needs a bend as often as any other.
    if (edit.roles[ring] == RingRole::Open && pos + 1 >= edit.points[ring].size())
        return err(ErrorCode::InvalidArgument,
                   "Son köşeden sonra kenar yok: açık bir çizgide " + std::to_string(after + 1) +
                       ". köşe uçtur. Araya köşe eklemek için ondan önceki bir köşe verin.");

    edit.points[ring].insert(edit.points[ring].begin() + static_cast<std::ptrdiff_t>(pos) + 1, at);
    return edit;
}

bool insert_preview(const Document& doc, EntityId e, std::size_t after, Point2 at, EmitBuffer& into)
{
    auto edit = insert_vertex(doc, e, after, at);
    if (!edit) return false;
    const GripEdit& g = edit.value();
    for (std::size_t r = 0; r < g.points.size(); ++r) {
        into.begin_run(g.roles[r] != RingRole::Open, g.roles[r] == RingRole::Interior);
        for (const Point2& p : g.points[r])
            into.push_vertex(p.x, p.y);
    }
    return true;
}

std::optional<std::size_t> nearest_grip(const Document& doc, EntityId e, Point2 probe)
{
    const std::vector<GripPoint> grips = entity_grips(doc, e);
    std::optional<std::size_t> best;
    double nearest = 0.0;
    for (std::size_t i = 0; i < grips.size(); ++i) {
        const double dx = mm_to_metres(grips[i].at.x - probe.x);
        const double dy = mm_to_metres(grips[i].at.y - probe.y);
        const double d  = dx * dx + dy * dy;
        if (!best || d < nearest) {
            best    = i;
            nearest = d;
        }
    }
    return best;
}

std::optional<std::size_t> nearest_edge(const Document& doc, EntityId e, Point2 probe)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e) || ents.kind[e] != kPolylineKind) return std::nullopt;

    const GripEdit edit = read_edit(doc, e);
    std::optional<std::size_t> best;
    double nearest     = 0.0;
    std::size_t before = 0; ///< vertices in the rings already walked
    for (std::size_t r = 0; r < edit.points.size(); ++r) {
        const std::vector<Point2>& ring = edit.points[r];
        const bool closed               = edit.roles[r] != RingRole::Open;
        const std::size_t edges         = closed ? ring.size() : ring.size() - 1;
        for (std::size_t i = 0; ring.size() >= 2 && i < edges; ++i) {
            const Point2 a     = ring[i];
            const Point2 b     = ring[(i + 1) % ring.size()];
            const Point2 on    = closest_point_on_segment(a, b, probe);
            const double dx    = mm_to_metres(on.x - probe.x);
            const double dy    = mm_to_metres(on.y - probe.y);
            const double score = dx * dx + dy * dy;
            if (!best || score < nearest) {
                best    = before + i;
                nearest = score;
            }
        }
        before += ring.size();
    }
    return best;
}

std::vector<std::uint8_t> encode_grip_guide(const GripGuide& guide)
{
    // version, insert, index, key — little-endian as the machine writes it,
    // because the bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(2 + sizeof(guide.index) + sizeof(guide.key));
    bytes[0] = 1;
    bytes[1] = guide.insert ? 1 : 0;
    std::memcpy(bytes.data() + 2, &guide.index, sizeof(guide.index));
    std::memcpy(bytes.data() + 2 + sizeof(guide.index), &guide.key, sizeof(guide.key));
    return bytes;
}

Result<GripGuide> decode_grip_guide(std::span<const std::uint8_t> bytes)
{
    GripGuide guide;
    if (bytes.size() != 2 + sizeof(guide.index) + sizeof(guide.key) || bytes[0] != 1 ||
        bytes[1] > 1)
        return err(ErrorCode::InvalidArgument, "Tutamak önizlemesinin baytları tanınmıyor.");
    guide.insert = bytes[1] == 1;
    std::memcpy(&guide.index, bytes.data() + 2, sizeof(guide.index));
    std::memcpy(&guide.key, bytes.data() + 2 + sizeof(guide.index), sizeof(guide.key));
    return guide;
}

} // namespace kentos::core
