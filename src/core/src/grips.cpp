// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/grips.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/identity.hpp"

#include <cmath>
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

Result<GripEdit> circle_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
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

Result<GripEdit> arc_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
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
    const Mm r = mm_round(length(c, to));
    if (r <= 0)
        return err(ErrorCode::ValidationFailed,
                   "Yarıçap sıfır: tutamak merkezin üstünde. Merkezden uzak bir yer seçin.");
    // An end goes where it was put and the radius follows it; the other end
    // keeps its direction. The midpoint handle sets the radius alone.
    if (index == 1) v[2] = to;
    if (index == 2) v[3] = to;
    v[1] = Point2{c.x + r, c.y};
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

Result<GripEdit> ellipse_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
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

Result<GripEdit> arc_polyline_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
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

Result<GripEdit> dimension_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
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

Result<GripEdit> block_reference_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit = read_edit(doc, e);
    if (index != 0 || edit.points.empty() || edit.points[0].empty()) return no_such_grip(index, 1);
    auto decoded = decode_block_reference(edit.payload);
    if (!decoded) return decoded.error();
    BlockReference ref = std::move(decoded.value());
    edit.points[0][0]  = to;
    ref.bounds         = block_reference_bounds(doc, to, ref);
    edit.payload       = encode_block_reference(ref);
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

Result<GripEdit> vertex_move(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    GripEdit edit    = read_edit(doc, e);
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
    case kBlockReferenceKind: {
        const auto v = vertex_grips(geom, slot);
        return v.empty() ? v : std::vector<GripPoint>{GripPoint{v[0].at, GripRole::Insertion}};
    }
    default: return vertex_grips(geom, slot);
    }
}

Result<GripEdit> move_grip(const Document& doc, EntityId e, std::size_t index, Point2 to)
{
    const EntityTable& ents = doc.entities();
    if (e >= ents.size() || !ents.alive(e))
        return err(ErrorCode::NotFound, "Nesne bulunamadı veya silinmiş.");
    if (auto st = doc.editable(e); !st) return st.error();
    switch (ents.kind[e]) {
    case kCircleKind: return circle_move(doc, e, index, to);
    case kArcKind: return arc_move(doc, e, index, to);
    case kEllipseKind: return ellipse_move(doc, e, index, to);
    case kArcPolylineKind: return arc_polyline_move(doc, e, index, to);
    case kDimensionKind: return dimension_move(doc, e, index, to);
    case kBlockReferenceKind: return block_reference_move(doc, e, index, to);
    default: return vertex_move(doc, e, index, to);
    }
}

bool grip_preview(const Document& doc, EntityId e, std::size_t index, Point2 to, EmitBuffer& into)
{
    auto edit = move_grip(doc, e, index, to);
    if (!edit) return false;
    const GripEdit& g = edit.value();

    // The edited shape in a scratch arena, drawn by the kind's own outline: the
    // preview IS the future drawing, computed once and the same way.
    RingGeometry scratch;
    const auto inputs = g.inputs();
    auto slot         = scratch.append(inputs, g.payload);
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

} // namespace kentos::core
