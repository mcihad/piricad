// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/parallel.hpp"
#include "piricad/core/precision.hpp"

#include "piricad/core/arc.hpp"
#include "piricad/core/arc_polyline.hpp"
#include "piricad/core/circle.hpp"
#include "piricad/core/ellipse.hpp"
#include "piricad/core/entity_kind.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/spline.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace piricad::core {
namespace {

/// What an object IS, for the purpose of a parallel.
struct Shape
{
    enum class Class : std::uint8_t { Runs, Faces, Circle, Arc };

    Class cls{Class::Runs};
    std::vector<std::vector<Point2>> runs; ///< Runs: the open lines, one per part
    std::vector<Polygon> faces;            ///< Faces: exteriors with their holes
    Point2 centre{};                       ///< Circle, Arc
    Mm radius{0};                          ///< Circle, Arc
    Point2 start{};                        ///< Arc
    Point2 end{};                          ///< Arc
    bool approximate{false};               ///< drawn, not defined: the curve kinds
    Mm deviation{0};                       ///< approximate: drawn vs defined
    std::size_t drawn_vertices{0};         ///< approximate: how many vertices were drawn
    std::optional<CurvePath> path;         ///< an arc polyline's own path, arcs as arcs
};

/// A BEVEL IS THE ROUND CORNER CUT STRAIGHT ACROSS. The kernel rounds a corner
/// it moves away from about the source vertex it turns at, with the offset
/// distance for its radius; that arc's chord joins the two offset edges' ends,
/// which is the bevel the polygon road cuts (Clipper2's `Bevel`). An arc of
/// the source itself stays an arc: its centre is not a vertex of the source.
void bevel_corners(CurvePath& path, const CurvePath& source, Mm distance)
{
    std::vector<Point2> turns;
    turns.reserve(source.pieces.size() + 1);
    for (const PathPiece& piece : source.pieces)
        turns.push_back(piece.from);
    if (!source.closed && !source.pieces.empty()) turns.push_back(source.pieces.back().to);
    for (PathPiece& piece : path.pieces)
        if (piece.kind == PathPiece::Kind::Arc && piece.radius == distance &&
            std::ranges::find(turns, piece.centre) != turns.end())
            piece = PathPiece{.from = piece.from, .to = piece.to};
}

/// Whether two computed points are more than the millimetre apart that makes
/// them one point (`kSamePointMm`).
bool apart(Point2 a, Point2 b)
{
    const double dx = static_cast<double>(a.x - b.x);
    const double dy = static_cast<double>(a.y - b.y);
    return dx * dx + dy * dy > kSamePointMm * kSamePointMm;
}

/// THE PARALLEL THAT KEEPS EVERY EDGE'S LENGTH (`JoinStyle::Link`, Netcad's
/// "ucuna bağla"): each edge of the run or ring moved `signed_distance`
/// sideways ON ITS OWN — to the left for a positive one, as `parallel_run` — and
/// the end of one moved edge joined to the start of the next by a straight link.
///
/// NO OFFSET ARITHMETIC OF ITS OWN. An edge alone has no corner to solve, so
/// what moves it is `parallel_run` over its two points — the function every
/// other parallel of a straight line is made by — and all that is done here is
/// to walk the edges and chain the answers. Nothing is carried to a meeting
/// point and nothing is trimmed; that is the method, and it is why the result
/// may cross itself where the source turns toward the side it is moved to.
///
/// Points closer than a millimetre are one point, so two collinear edges give
/// one straight edge and a rounding does not leave a link a millimetre long.
Result<std::vector<Point2>> linked_ring(const std::vector<Point2>& ring, bool closed,
                                        Mm signed_distance)
{
    std::vector<Point2> src;
    src.reserve(ring.size());
    for (const Point2& p : ring)
        if (src.empty() || src.back() != p) src.push_back(p);
    if (closed && src.size() > 1 && src.front() == src.back()) src.pop_back();
    if (src.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Paralel için çizginin en az iki ayrı noktası olmalı. Verilen: " +
                       std::to_string(src.size()));

    const std::size_t edges = closed ? src.size() : src.size() - 1;
    std::vector<Point2> out;
    out.reserve(2 * edges);
    for (std::size_t i = 0; i < edges; ++i) {
        auto moved =
            parallel_run({src[i], src[(i + 1) % src.size()]}, signed_distance, JoinStyle::Miter);
        if (!moved) return moved.error();
        if (moved.value().size() != 1 || moved.value().front().points.size() != 2)
            return err(ErrorCode::Internal,
                       "Bir kenarın paraleli tek bir kenar çıkmadı; kenar uzunlukları "
                       "korunamıyor.");
        for (const Point2& p : moved.value().front().points)
            if (out.empty() || apart(out.back(), p)) out.push_back(p);
    }
    if (closed && out.size() > 2 && !apart(out.front(), out.back())) out.pop_back();
    return out;
}

/// The path through `points`, segment by segment; closed back to the first.
CurvePath path_through(const std::vector<Point2>& points, bool closed)
{
    CurvePath out;
    out.closed = closed;
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        if (points[i] != points[i + 1])
            out.pieces.push_back(PathPiece{.from = points[i], .to = points[i + 1]});
    if (closed && points.size() > 2 && points.back() != points.front())
        out.pieces.push_back(PathPiece{.from = points.back(), .to = points.front()});
    return out;
}

std::vector<Point2> ring_points(const RingGeometry& geom, std::uint32_t r)
{
    const auto xs = geom.ring_xs(r);
    const auto ys = geom.ring_ys(r);
    std::vector<Point2> out;
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(Point2{xs[v], ys[v]});
    return out;
}

/// The distance from `p` to the chord `a`→`b`, in millimetres.
double chord_gap(Point2 a, Point2 b, double px, double py) noexcept
{
    const double ax = static_cast<double>(a.x);
    const double ay = static_cast<double>(a.y);
    const double dx = static_cast<double>(b.x) - ax;
    const double dy = static_cast<double>(b.y) - ay;
    const double l2 = dx * dx + dy * dy;
    double t        = 0.0;
    if (l2 > 0.0) t = std::clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0.0, 1.0);
    const double qx = ax + t * dx - px;
    const double qy = ay + t * dy - py;
    return std::sqrt(qx * qx + qy * qy);
}

/// The drawn outline's largest distance from an ELLIPSE through `centre`, with
/// axis ends `major` and `minor`: each chord against the point of the ellipse
/// halfway between its ends, in the ellipse's own parameter. Each end's
/// parameter is read back from the point by inverting the axis map, and the
/// halfway direction is the bisector of the two — the same exact bisection the
/// drawing's own direction table is built with, so no sine is taken.
Mm ellipse_deviation(Point2 centre, Point2 major, Point2 minor, const std::vector<Point2>& drawn,
                     bool closed)
{
    const double ux  = static_cast<double>(major.x - centre.x);
    const double uy  = static_cast<double>(major.y - centre.y);
    const double vx  = static_cast<double>(minor.x - centre.x);
    const double vy  = static_cast<double>(minor.y - centre.y);
    const double det = ux * vy - uy * vx;
    if (drawn.size() < 2 || det == 0.0) return 0;

    const auto param = [&](Point2 p, double& c, double& s) {
        const double px = static_cast<double>(p.x - centre.x);
        const double py = static_cast<double>(p.y - centre.y);
        c               = (px * vy - py * vx) / det;
        s               = (ux * py - uy * px) / det;
    };

    double worst            = 0.0;
    const std::size_t edges = closed ? drawn.size() : drawn.size() - 1;
    for (std::size_t i = 0; i < edges; ++i) {
        const Point2 a = drawn[i];
        const Point2 b = drawn[(i + 1) % drawn.size()];
        double ca      = 0.0;
        double sa      = 0.0;
        double cb      = 0.0;
        double sb      = 0.0;
        param(a, ca, sa);
        param(b, cb, sb);
        double mc         = ca + cb;
        double ms         = sa + sb;
        const double norm = std::sqrt(mc * mc + ms * ms);
        if (norm <= 0.0) continue; // opposite ends: not a chord a drawing has
        mc /= norm;
        ms /= norm;
        const double mx = static_cast<double>(centre.x) + mc * ux + ms * vx;
        const double my = static_cast<double>(centre.y) + mc * uy + ms * vy;
        worst           = std::max(worst, chord_gap(a, b, mx, my));
    }
    return mm_round(worst);
}

/// The drawn outline's largest distance from the ARCS it stands for: a chord of
/// an arc of radius r and length c departs from it by r − √(r² − c²/4) at its
/// middle. Chords are matched to arcs by lying on them — both ends at the
/// radius, to the millimetre the drawing was rounded to.
Mm arc_polyline_deviation(const ArcPolyline& def, const std::vector<Point2>& drawn)
{
    double worst = 0.0;
    for (std::size_t i = 0; i + 1 < drawn.size(); ++i) {
        const Point2 a = drawn[i];
        const Point2 b = drawn[i + 1];
        for (const ArcPolyline::Arc& arc : def.arcs) {
            const auto off = [&](Point2 p) {
                const double dx = static_cast<double>(p.x - arc.centre.x);
                const double dy = static_cast<double>(p.y - arc.centre.y);
                return std::fabs(std::sqrt(dx * dx + dy * dy) - static_cast<double>(arc.radius));
            };
            if (off(a) > kRoundedFitMm || off(b) > kRoundedFitMm) continue;
            const double dx = static_cast<double>(b.x - a.x);
            const double dy = static_cast<double>(b.y - a.y);
            const double r  = static_cast<double>(arc.radius);
            const double h  = (dx * dx + dy * dy) / 4.0;
            if (h >= r * r) continue;
            worst = std::max(worst, r - std::sqrt(r * r - h));
            break;
        }
    }
    return mm_round(worst);
}

/// The drawn outline's largest distance from a SPLINE: the curve is drawn with
/// `kDrawn` points to a knot span, and sampling it at twice that puts a point of
/// the curve at the middle of every drawn chord.
Mm spline_deviation(std::span<const Point2> controls, const SplineDef& def)
{
    constexpr int kDrawn = 16; // spline_outline's density
    std::vector<Mm> xs;
    std::vector<Mm> ys;
    spline_points(controls, def, kDrawn, xs, ys);
    std::vector<Mm> fx;
    std::vector<Mm> fy;
    spline_points(controls, def, 2 * kDrawn, fx, fy);
    if (xs.size() < 2 || fx.size() != 2 * xs.size() - 1) return 0;

    double worst = 0.0;
    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
        const Point2 a{xs[i], ys[i]};
        const Point2 b{xs[i + 1], ys[i + 1]};
        worst = std::max(worst, chord_gap(a, b, static_cast<double>(fx[2 * i + 1]),
                                          static_cast<double>(fy[2 * i + 1])));
    }
    return mm_round(worst);
}

/// The object's shape, or why it has none worth a parallel.
Result<Shape> shape_of(const Document& doc, EntityId e)
{
    if (e == kNoEntity || !doc.alive(e))
        return err(ErrorCode::NotFound, "Nesne bulunamadı veya silinmiş.");
    if (auto refused = parallel_refusal(doc, e))
        return err(ErrorCode::Unsupported, std::move(*refused));

    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = doc.entities().slot[e];
    const KindId kind        = doc.entities().kind[e];

    Shape shape;
    if (kind == kCircleKind) {
        shape.cls    = Shape::Class::Circle;
        shape.centre = circle_centre_of(geom, slot);
        shape.radius = circle_radius_of(geom, slot);
        return shape;
    }
    if (kind == kArcKind) {
        shape.cls    = Shape::Class::Arc;
        shape.centre = arc_centre_of(geom, slot);
        shape.radius = arc_radius_of(geom, slot);
        shape.start  = arc_start_of(geom, slot);
        shape.end    = arc_end_of(geom, slot);
        return shape;
    }

    if (kind == kPolylineKind) {
        // The rings ARE the shape: open runs are lines, an exterior with the
        // interiors after it is a face (R11 order).
        const RingSpan span = geom.rings_of(slot);
        bool open           = false;
        bool faces          = false;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            std::vector<Point2> pts = ring_points(geom, r);
            switch (geom.ring_role[r]) {
            case RingRole::Open:
                open = true;
                shape.runs.push_back(std::move(pts));
                break;
            case RingRole::Exterior:
                faces = true;
                shape.faces.push_back(Polygon{std::move(pts), {}});
                break;
            case RingRole::Interior:
                if (shape.faces.empty())
                    return err(ErrorCode::InvalidArgument,
                               "Bu alanın bir deliği dış halkasından önce geliyor; paraleli "
                               "alınamaz.");
                shape.faces.back().holes.push_back(std::move(pts));
                break;
            }
        }
        if (open && faces)
            return err(ErrorCode::Unsupported,
                       "Bu nesnede hem açık çizgi hem alan var; paralelin hangi anlamda "
                       "alınacağı belirsiz.");
        if (!open && !faces) return err(ErrorCode::InvalidArgument, "Nesnenin geometrisi boş.");
        shape.cls = faces ? Shape::Class::Faces : Shape::Class::Runs;
        return shape;
    }

    // THE CURVE KINDS, by their drawing. What is measured next is how far that
    // drawing is from the curve, because it is the drawing that is offset.
    EmitBuffer drawn;
    if (!curve_outline(kind, geom, slot, drawn) || drawn.run_total() == 0)
        return err(ErrorCode::Unsupported, "Bu nesnenin çizilmiş bir hâli yok; paraleli alınamaz.");
    for (std::size_t r = 0; r < drawn.run_total(); ++r) {
        std::vector<Point2> pts;
        pts.reserve(drawn.run_count[r]);
        for (std::uint32_t v = 0; v < drawn.run_count[r]; ++v)
            pts.push_back(
                Point2{drawn.xs[drawn.run_start[r] + v], drawn.ys[drawn.run_start[r] + v]});
        shape.drawn_vertices += pts.size();
        if (drawn.run_closed[r] != 0)
            shape.faces.push_back(Polygon{std::move(pts), {}});
        else
            shape.runs.push_back(std::move(pts));
    }
    shape.cls         = shape.faces.empty() ? Shape::Class::Runs : Shape::Class::Faces;
    shape.approximate = true;

    if (kind == kEllipseKind) {
        const Point2 c = ellipse_centre_of(geom, slot);
        const Point2 a = ellipse_major_of(geom, slot);
        const Point2 b = ellipse_minor_of(geom, slot);
        for (const Polygon& f : shape.faces)
            shape.deviation =
                std::max(shape.deviation, ellipse_deviation(c, a, b, f.exterior, true));
        for (const std::vector<Point2>& run : shape.runs)
            shape.deviation = std::max(shape.deviation, ellipse_deviation(c, a, b, run, false));
    } else if (kind == kArcPolylineKind) {
        // ITS PATH, for the kernel's road; the drawing above stays for the side
        // a point is on and for the corner the kernel does not cut.
        shape.path = path_of_slot(kind, geom, slot, PathScope::Circular);
        if (auto def = arc_polyline_of(geom, slot)) {
            for (const Polygon& f : shape.faces) {
                std::vector<Point2> loop = f.exterior;
                loop.push_back(loop.front()); // the closing chord is a chord too
                shape.deviation =
                    std::max(shape.deviation, arc_polyline_deviation(def.value(), loop));
            }
            for (const std::vector<Point2>& run : shape.runs)
                shape.deviation =
                    std::max(shape.deviation, arc_polyline_deviation(def.value(), run));
        }
    } else if (kind == kSplineKind) {
        if (auto def = spline_of(geom, slot)) {
            const RingSpan span = geom.rings_of(slot);
            if (span.count > 0)
                shape.deviation = spline_deviation(ring_points(geom, span.first), def.value());
        }
    }
    return shape;
}

bool open_side(ParallelSide s) noexcept
{
    return s == ParallelSide::Left || s == ParallelSide::Right;
}

/// The two sides `Both` stands for, in the order they are drawn.
std::vector<ParallelSide> sides_of(ParallelSide asked, bool closed)
{
    if (asked != ParallelSide::Both) return {asked};
    return closed ? std::vector<ParallelSide>{ParallelSide::Outside, ParallelSide::Inside}
                  : std::vector<ParallelSide>{ParallelSide::Left, ParallelSide::Right};
}

/// `p` scaled out from `centre` to `radius`, which is where a concentric arc's
/// end is.
Point2 along_ray(Point2 centre, Point2 p, Mm from, Mm to) noexcept
{
    if (from <= 0) return p;
    const double k = static_cast<double>(to) / static_cast<double>(from);
    return Point2{centre.x + mm_round(static_cast<double>(p.x - centre.x) * k),
                  centre.y + mm_round(static_cast<double>(p.y - centre.y) * k)};
}

} // namespace

std::optional<ParallelSide> parallel_side_from_name(std::string_view word)
{
    const std::string w(word);
    if (turkish_key_equals(w, "sol") || turkish_key_equals(w, "left")) return ParallelSide::Left;
    if (turkish_key_equals(w, "sag") || turkish_key_equals(w, "right")) return ParallelSide::Right;
    if (turkish_key_equals(w, "dis") || turkish_key_equals(w, "outside"))
        return ParallelSide::Outside;
    if (turkish_key_equals(w, "ic") || turkish_key_equals(w, "inside")) return ParallelSide::Inside;
    if (turkish_key_equals(w, "iki") || turkish_key_equals(w, "both")) return ParallelSide::Both;
    return std::nullopt;
}

std::string_view parallel_side_name(ParallelSide side) noexcept
{
    switch (side) {
    case ParallelSide::Left: return "sol";
    case ParallelSide::Right: return "sağ";
    case ParallelSide::Outside: return "dış";
    case ParallelSide::Inside: return "iç";
    case ParallelSide::Both: return "iki";
    }
    return "?";
}

Mm drawn_deviation(const Document& doc, EntityId e)
{
    auto shape = shape_of(doc, e);
    return shape && shape.value().approximate ? shape.value().deviation : 0;
}

std::optional<std::string> parallel_refusal(const Document& doc, EntityId e)
{
    if (e == kNoEntity || !doc.alive(e)) return std::string("Nesne bulunamadı veya silinmiş.");
    const KindId kind        = doc.entities().kind[e];
    const std::uint32_t slot = doc.entities().slot[e];

    if (kind == kPointKind)
        return std::string("Bir noktanın paraleli olmaz. Çevresinde bir alan için TAMPON "
                           "kullanın.");
    if (kind == kPolylineKind && doc.texts().has(slot))
        return std::string("Bir yazının paraleli olmaz. Yazıyı taşımak için TAŞI kullanın.");
    if (kind == kHatchKind)
        return std::string("Bir taramanın paraleli olmaz; sınırının paralelini alın.");
    if (kind == kDimensionKind || kind == kLeaderKind)
        return std::string("Bir açıklamanın (ölçü, kılavuz çizgi) paraleli olmaz; ölçtüğü çizginin "
                           "paralelini alın.");
    if (kind == kBlockReferenceKind)
        return std::string("Bir blok yerleşiminin paraleli olmaz; bloğu PATLAT ile açıp "
                           "çizgilerinin paralelini alın.");
    if (kind != kPolylineKind && kind != kCircleKind && kind != kArcKind && kind != kEllipseKind &&
        kind != kSplineKind && kind != kArcPolylineKind)
        return std::string("Bu türün paraleli tanımlı değil.");
    return std::nullopt;
}

bool parallel_encloses(const Document& doc, EntityId e)
{
    auto shape = shape_of(doc, e);
    return shape && shape.value().cls != Shape::Class::Runs;
}

Result<ParallelSide> parallel_side_at(const Document& doc, EntityId e, Point2 p)
{
    auto shape = shape_of(doc, e);
    if (!shape) return shape.error();
    const Shape& s = shape.value();

    switch (s.cls) {
    case Shape::Class::Circle:
    case Shape::Class::Arc: {
        const Int128 dx = p.x - s.centre.x;
        const Int128 dy = p.y - s.centre.y;
        const Int128 r  = s.radius;
        return dx * dx + dy * dy < r * r ? ParallelSide::Inside : ParallelSide::Outside;
    }
    case Shape::Class::Faces: {
        // EVEN-ODD over every ring, so a point in a courtyard is outside the face.
        bool inside = false;
        for (const Polygon& f : s.faces) {
            const auto in = [&p](const std::vector<Point2>& ring) {
                std::vector<Mm> xs;
                std::vector<Mm> ys;
                for (const Point2& q : ring) {
                    xs.push_back(q.x);
                    ys.push_back(q.y);
                }
                return ring_contains(xs, ys, p);
            };
            if (in(f.exterior)) inside = !inside;
            for (const std::vector<Point2>& hole : f.holes)
                if (in(hole)) inside = !inside;
        }
        return inside ? ParallelSide::Inside : ParallelSide::Outside;
    }
    case Shape::Class::Runs: break;
    }

    // The side of the nearest edge, by the exact sign of a cross product.
    double best  = std::numeric_limits<double>::max();
    Int128 which = 0;
    for (const std::vector<Point2>& run : s.runs)
        for (std::size_t i = 0; i + 1 < run.size(); ++i) {
            const Point2 a = run[i];
            const Point2 b = run[i + 1];
            if (a == b) continue;
            const double gap = chord_gap(a, b, static_cast<double>(p.x), static_cast<double>(p.y));
            if (gap < best) {
                best  = gap;
                which = static_cast<Int128>(b.x - a.x) * (p.y - a.y) -
                        static_cast<Int128>(b.y - a.y) * (p.x - a.x);
            }
        }
    if (which == 0)
        return err(ErrorCode::InvalidArgument,
                   "Nokta çizginin tam üzerinde; paralelin gideceği tarafı çizginin bir yanında "
                   "gösterin.");
    return which > 0 ? ParallelSide::Left : ParallelSide::Right;
}

namespace {

/// The parallel of a straight-edged line or face whose edges keep their
/// lengths (`linked_ring`). A face is wound first — its boundary counter-
/// clockwise, its holes clockwise — so that the material is on the left of
/// every ring: growing the face moves each ring to the right, which pushes a
/// hole's edges into the hole, and shrinking moves them left.
Result<Parallel> linked_parallel(const Shape& s, Mm distance, ParallelSide side)
{
    Parallel out;
    const bool faces = s.cls == Shape::Class::Faces;
    for (const ParallelSide one : sides_of(side, faces)) {
        if (!faces) {
            for (const std::vector<Point2>& run : s.runs) {
                auto moved =
                    linked_ring(run, false, one == ParallelSide::Left ? distance : -distance);
                if (!moved) return moved.error();
                ParallelPiece piece;
                piece.shape = ParallelPiece::Shape::Run;
                piece.side  = one;
                piece.run   = std::move(moved.value());
                out.pieces.push_back(std::move(piece));
            }
            continue;
        }

        const Mm signed_distance = one == ParallelSide::Outside ? -distance : distance;
        ParallelPiece piece;
        piece.shape = ParallelPiece::Shape::Face;
        piece.side  = one;
        for (const Polygon& face : s.faces) {
            const auto wound = [](std::vector<Point2> ring, bool counter_clockwise) {
                if ((ring_area(ring) > 0) != counter_clockwise) std::ranges::reverse(ring);
                return ring;
            };
            Polygon moved;
            auto boundary = linked_ring(wound(face.exterior, true), true, signed_distance);
            if (!boundary) return boundary.error();
            moved.exterior = std::move(boundary.value());
            for (const std::vector<Point2>& hole : face.holes) {
                auto inner = linked_ring(wound(hole, false), true, signed_distance);
                if (!inner) return inner.error();
                moved.holes.push_back(std::move(inner.value()));
            }
            piece.faces.push_back(std::move(moved));
        }
        out.pieces.push_back(std::move(piece));
    }
    return out;
}

/// The parallel of a shape already read: the ONE body an object and a run that
/// is not an object yet (`run_parallel`) are moved sideways by, so the canvas's
/// preview under the cursor and the result a command writes cannot differ.
Result<Parallel> parallel_of(const Shape& s, Mm distance, ParallelSide side, JoinStyle join)
{
    Parallel out;
    out.approximate    = s.approximate;
    out.deviation      = s.deviation;
    out.drawn_vertices = s.drawn_vertices;

    const bool closed = s.cls == Shape::Class::Faces || s.cls == Shape::Class::Circle;
    if (s.cls != Shape::Class::Arc && side != ParallelSide::Both) {
        if (closed && open_side(side))
            return err(ErrorCode::InvalidArgument,
                       "Kapalı bir şeklin solu ya da sağı yoktur; `taraf=dis` ya da `taraf=ic` "
                       "verin.");
        if (!closed && !open_side(side))
            return err(ErrorCode::InvalidArgument,
                       "Açık bir çizginin içi ya da dışı yoktur; `taraf=sol` ya da `taraf=sag` "
                       "verin.");
    }

    // ---- "UCUNA BAĞLA": THE EDGES KEEP THEIR LENGTH (`JoinStyle::Link`) ----
    //
    // Only a straight edge keeps its length when it moves sideways: an arc's
    // concentric parallel is longer or shorter than the arc, and a curve drawn
    // as chords has a chord at every vertex. So the method is for lines and faces
    // of straight edges, and for a shape with an arc or a drawn curve in it the
    // answer is a sentence, not a corner the shape quietly did not get. A circle
    // and an arc have no corner at all, and their parallel is what it always was.
    if (join == JoinStyle::Link && (s.cls == Shape::Class::Runs || s.cls == Shape::Class::Faces)) {
        if (s.approximate || s.path.has_value())
            return err(ErrorCode::Unsupported,
                       "kose=uc kenar uzunluklarını korur ve bunu yalnız düz kenar yapabilir; bu "
                       "nesnenin bir kenarı yay ya da eğri. Düz kenarlı bir çizgi ya da alan "
                       "verin, ya da kose=KÖŞE, YUVARLAK ya da PAH kullanın.");
        return linked_parallel(s, distance, side);
    }

    // ---- THE KERNEL'S ROAD (TODOS O-4, CLAUDE.md 2.11) ----
    //
    // An arc polyline's parallel as what it is — its arcs concentric arcs, its
    // segments offset segments — and a ROUND corner asked of any line or face as
    // a true arc about the corner, not the fan of short edges the polygon road
    // leaves. The polygon road stays for the sharp and bevelled corners of a
    // straight shape — exact, fast, and what the golden fixtures hold — and for
    // a face with holes: an object whose edges bend keeps one ring (R9b).
    const bool holes =
        std::ranges::any_of(s.faces, [](const Polygon& f) { return !f.holes.empty(); });
    const bool bends = s.path.has_value();
    const bool round_straight =
        !s.approximate && join == JoinStyle::Round &&
        (s.cls == Shape::Class::Runs || (s.cls == Shape::Class::Faces && !holes));
    if (!bends && !s.approximate && join == JoinStyle::Round && holes) out.round_as_chords = true;
    if (kernel_available() && (bends || round_straight)) {
        std::vector<CurvePath> sources;
        if (bends) {
            sources.push_back(*s.path);
        } else if (s.cls == Shape::Class::Runs) {
            for (const std::vector<Point2>& run : s.runs)
                sources.push_back(path_through(run, false));
        } else {
            for (const Polygon& f : s.faces)
                sources.push_back(path_through(f.exterior, true));
        }
        // A bevel is asked of the kernel round and cut straight afterwards.
        const OffsetCorner corner =
            join == JoinStyle::Miter ? OffsetCorner::Sharp : OffsetCorner::Round;
        for (const ParallelSide one : sides_of(side, closed)) {
            // The kernel's sign: a closed path grows outward, an open one goes
            // to the right of travel, for a positive distance.
            const bool plus = one == ParallelSide::Outside || one == ParallelSide::Right;
            for (const CurvePath& source : sources) {
                auto moved = kernel_offset(source, plus ? distance : -distance, corner, false);
                if (!moved) return moved.error();
                for (CurvePath& path : moved.value()) {
                    if (join == JoinStyle::Bevel) bevel_corners(path, source, distance);
                    ParallelPiece piece;
                    piece.shape = ParallelPiece::Shape::Path;
                    piece.side  = one;
                    piece.path  = std::move(path);
                    out.pieces.push_back(std::move(piece));
                }
            }
        }
        out.approximate    = false;
        out.deviation      = 0;
        out.drawn_vertices = 0;
        return out;
    }

    for (ParallelSide one : sides_of(side, closed)) {
        switch (s.cls) {
        case Shape::Class::Circle: {
            const Mm radius =
                one == ParallelSide::Outside ? s.radius + distance : s.radius - distance;
            if (radius <= 0) continue; // collapsed: said by the caller
            ParallelPiece piece;
            piece.shape  = ParallelPiece::Shape::Circle;
            piece.side   = one;
            piece.centre = s.centre;
            piece.radius = radius;
            out.pieces.push_back(std::move(piece));
            break;
        }
        case Shape::Class::Arc: {
            // Drawn counter-clockwise, so its left is toward the centre.
            const bool grow = one == ParallelSide::Outside || one == ParallelSide::Right;
            const Mm radius = grow ? s.radius + distance : s.radius - distance;
            if (radius <= 0) continue;
            ParallelPiece piece;
            piece.shape  = ParallelPiece::Shape::Arc;
            piece.side   = one;
            piece.centre = s.centre;
            piece.radius = radius;
            piece.start  = along_ray(s.centre, s.start, s.radius, radius);
            piece.end    = along_ray(s.centre, s.end, s.radius, radius);
            out.pieces.push_back(std::move(piece));
            break;
        }
        case Shape::Class::Faces: {
            auto moved =
                offset_faces(s.faces, one == ParallelSide::Outside ? distance : -distance, join);
            if (!moved) return moved.error();
            if (moved.value().empty()) continue;
            ParallelPiece piece;
            piece.shape = ParallelPiece::Shape::Face;
            piece.side  = one;
            piece.faces = std::move(moved.value());
            out.pieces.push_back(std::move(piece));
            break;
        }
        case Shape::Class::Runs:
            for (const std::vector<Point2>& run : s.runs) {
                auto beside =
                    parallel_run(run, one == ParallelSide::Left ? distance : -distance, join);
                if (!beside) return beside.error();
                for (OffsetRing& ring : beside.value()) {
                    ParallelPiece piece;
                    piece.shape  = ParallelPiece::Shape::Run;
                    piece.side   = one;
                    piece.run    = std::move(ring.points);
                    piece.closed = ring.closed;
                    out.pieces.push_back(std::move(piece));
                }
            }
            break;
        }
    }
    return out;
}

/// The two ends of a side's parallel — where it starts and where it stops, the
/// start being the end nearer the axis's first point — or nothing when the side
/// did not come back as ONE open line. The nearer-end rule is what keeps a cap
/// from crossing itself if a backend hands a side back running the other way.
std::optional<std::array<Point2, 2>> ends_of(const std::vector<ParallelPiece>& pieces,
                                             Point2 axis_start, Point2 axis_end)
{
    if (pieces.size() != 1) return std::nullopt;
    const ParallelPiece& only = pieces.front();
    Point2 a{};
    Point2 b{};
    switch (only.shape) {
    case ParallelPiece::Shape::Run:
        if (only.closed || only.run.size() < 2) return std::nullopt;
        a = only.run.front();
        b = only.run.back();
        break;
    case ParallelPiece::Shape::Path:
        if (only.path.closed || only.path.pieces.empty()) return std::nullopt;
        a = only.path.pieces.front().from;
        b = only.path.pieces.back().to;
        break;
    default: return std::nullopt;
    }
    const auto gap2 = [](Point2 p, Point2 q) {
        const Int128 dx = p.x - q.x;
        const Int128 dy = p.y - q.y;
        return dx * dx + dy * dy;
    };
    if (gap2(a, axis_start) + gap2(b, axis_end) > gap2(b, axis_start) + gap2(a, axis_end))
        std::swap(a, b);
    return std::array<Point2, 2>{a, b};
}

} // namespace

Result<Parallel> entity_parallel(const Document& doc, EntityId e, Mm distance, ParallelSide side,
                                 JoinStyle join)
{
    if (distance <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Paralel mesafesi sıfırdan büyük olmalı; tarafı `taraf` söyler.");

    auto shape = shape_of(doc, e);
    if (!shape) return shape.error();
    return parallel_of(shape.value(), distance, side, join);
}

Result<Parallel> run_parallel(std::span<const Point2> run, Mm distance, ParallelSide side,
                              JoinStyle join)
{
    if (distance <= 0)
        return err(ErrorCode::InvalidArgument, "Paralel mesafesi sıfırdan büyük olmalı.");

    Shape shape;
    shape.cls = Shape::Class::Runs;
    shape.runs.emplace_back(run.begin(), run.end());
    return parallel_of(shape, distance, side, join);
}

Status check_double_line(const DoubleLineSpec& spec)
{
    if (spec.left < 0 || spec.right < 0)
        return err(ErrorCode::InvalidArgument,
                   "Yan genişliği eksi olamaz. Bir yanı çizmemek için o yanın genişliğine 0 "
                   "yazın; yön sol ve sağ diye ayrılır, eksi işaretle değil.");
    if (spec.left == 0 && spec.right == 0)
        return err(ErrorCode::InvalidArgument,
                   "Çift çizgi için en az bir yanın genişliği sıfırdan büyük olmalı: sol ya da "
                   "sağ.");
    if (spec.left > kMmCoordinateLimit || spec.right > kMmCoordinateLimit)
        return err(ErrorCode::InvalidArgument,
                   "Yan genişliği çizimin koordinat sınırını aşıyor; metre olarak yazdığınızdan "
                   "emin olun.");
    return ok();
}

Result<DoubleLine> double_line(std::span<const Point2> axis, const DoubleLineSpec& spec)
{
    if (const Status st = check_double_line(spec); !st) return st.error();

    DoubleLine out;
    if (spec.left > 0) {
        auto beside = run_parallel(axis, spec.left, ParallelSide::Left, spec.join);
        if (!beside) return beside.error();
        out.left = std::move(beside.value().pieces);
    }
    if (spec.right > 0) {
        auto beside = run_parallel(axis, spec.right, ParallelSide::Right, spec.join);
        if (!beside) return beside.error();
        out.right = std::move(beside.value().pieces);
    }

    if (spec.close_ends) {
        // THE CAPS JOIN THE ENDS THE PARALLELS ACTUALLY HAVE, not a second
        // offset of the axis's end points: a corner solved at the last vertex, a
        // rounding at the millimetre, all are already in the pieces.
        const Point2 first = axis.front();
        const Point2 last  = axis.back();
        std::optional<std::array<Point2, 2>> l;
        std::optional<std::array<Point2, 2>> r;
        if (spec.left > 0) {
            l = ends_of(out.left, first, last);
            if (!l)
                return err(ErrorCode::Unsupported,
                           "Eksenin sol yanındaki paralel tek parça çıkmadı; uçları kapatılacak "
                           "bir uç yok. Daha dar bir genişlik verin ya da uclar=acik kullanın.");
        }
        if (spec.right > 0) {
            r = ends_of(out.right, first, last);
            if (!r)
                return err(ErrorCode::Unsupported,
                           "Eksenin sağ yanındaki paralel tek parça çıkmadı; uçları kapatılacak "
                           "bir uç yok. Daha dar bir genişlik verin ya da uclar=acik kullanın.");
        }
        // A ONE-SIDED DOUBLE LINE IS CLOSED AGAINST THE AXIS ITSELF.
        out.caps.push_back({l ? (*l)[0] : first, r ? (*r)[0] : first});
        out.caps.push_back({l ? (*l)[1] : last, r ? (*r)[1] : last});
    }
    return out;
}

std::vector<std::uint8_t> encode_double_line_preview(const DoubleLineSpec& spec)
{
    // version, join, closed ends, left, right — little-endian as the machine
    // writes it, because the bytes never leave the process (a prompt to the
    // canvas), like `encode_parallel_preview`.
    std::vector<std::uint8_t> bytes(3 + 2 * sizeof(Mm));
    bytes[0] = 1;
    bytes[1] = static_cast<std::uint8_t>(spec.join);
    bytes[2] = spec.close_ends ? 1 : 0;
    std::memcpy(bytes.data() + 3, &spec.left, sizeof(Mm));
    std::memcpy(bytes.data() + 3 + sizeof(Mm), &spec.right, sizeof(Mm));
    return bytes;
}

Result<DoubleLineSpec> decode_double_line_preview(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 3 + 2 * sizeof(Mm) || bytes[0] != 1 ||
        bytes[1] > static_cast<std::uint8_t>(JoinStyle::Bevel) || bytes[2] > 1)
        return err(ErrorCode::InvalidArgument, "Çift çizgi önizlemesinin baytları tanınmıyor.");
    DoubleLineSpec spec;
    spec.join       = static_cast<JoinStyle>(bytes[1]);
    spec.close_ends = bytes[2] == 1;
    std::memcpy(&spec.left, bytes.data() + 3, sizeof(Mm));
    std::memcpy(&spec.right, bytes.data() + 3 + sizeof(Mm), sizeof(Mm));
    return spec;
}

std::vector<std::uint8_t> encode_parallel_preview(const ParallelPreview& preview)
{
    // version, join, distance, count, keys — little-endian as the machine writes
    // it, because the bytes never leave the process (a prompt to the canvas).
    const auto count = static_cast<std::uint32_t>(preview.keys.size());
    std::vector<std::uint8_t> bytes(2 + sizeof(Mm) + sizeof(count) +
                                    preview.keys.size() * sizeof(std::int64_t));
    bytes[0]           = 1;
    bytes[1]           = static_cast<std::uint8_t>(preview.join);
    std::size_t offset = 2;
    std::memcpy(bytes.data() + offset, &preview.distance, sizeof(Mm));
    offset += sizeof(Mm);
    std::memcpy(bytes.data() + offset, &count, sizeof(count));
    offset += sizeof(count);
    if (count != 0)
        std::memcpy(bytes.data() + offset, preview.keys.data(), count * sizeof(std::int64_t));
    return bytes;
}

Result<ParallelPreview> decode_parallel_preview(std::span<const std::uint8_t> bytes)
{
    constexpr std::size_t kHead = 2 + sizeof(Mm) + sizeof(std::uint32_t);
    if (bytes.size() < kHead || bytes[0] != 1 ||
        bytes[1] > static_cast<std::uint8_t>(JoinStyle::Link))
        return err(ErrorCode::InvalidArgument, "Paralel önizlemesinin baytları tanınmıyor.");
    ParallelPreview preview;
    preview.join       = static_cast<JoinStyle>(bytes[1]);
    std::size_t offset = 2;
    std::memcpy(&preview.distance, bytes.data() + offset, sizeof(Mm));
    offset += sizeof(Mm);
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + offset, sizeof(count));
    offset += sizeof(count);
    if (bytes.size() != offset + static_cast<std::size_t>(count) * sizeof(std::int64_t))
        return err(ErrorCode::InvalidArgument, "Paralel önizlemesinin baytları tanınmıyor.");
    preview.keys.resize(count);
    if (count != 0)
        std::memcpy(preview.keys.data(), bytes.data() + offset, count * sizeof(std::int64_t));
    return preview;
}

} // namespace piricad::core
