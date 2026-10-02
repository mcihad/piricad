// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/kernel.hpp"

#include "piricad/core/arc.hpp"
#include "piricad/core/pick.hpp"
#include "piricad/core/precision.hpp"
#include "piricad/core/spline.hpp"
#include "piricad/core/trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#endif

#if PIRICAD_HAVE_OCCT
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Wire.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dInt_GInter.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <IntRes2d_IntersectionPoint.hxx>
#include <IntRes2d_IntersectionSegment.hxx>
#include <Message_ProgressIndicator.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <StdFail_NotDone.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>
#endif

namespace piricad::core {

std::vector<KernelFace> kernel_faces_of(std::vector<CurvePath> rings)
{
    // Each ring with its drawn outline, for the containment test, and its size,
    // for the order: a ring can only be held by a larger one.
    struct Ring
    {
        CurvePath path;
        std::vector<Mm> xs;
        std::vector<Mm> ys;
        Mm2 size{0};
        std::size_t depth{0};
        std::size_t holder{0};
    };

    std::vector<Ring> all;
    all.reserve(rings.size());
    for (CurvePath& path : rings) {
        if (!path.closed || path.pieces.empty()) continue;
        Ring r;
        path_outline(path, r.xs, r.ys);
        const Mm2 a = path_area(path);
        r.size      = a < 0 ? -a : a;
        r.path      = std::move(path);
        all.push_back(std::move(r));
    }
    std::ranges::stable_sort(all, [](const Ring& x, const Ring& y) { return x.size > y.size; });

    constexpr auto kNone = static_cast<std::size_t>(-1);
    std::vector<KernelFace> out;
    std::vector<std::size_t> face_of(all.size(), kNone);
    for (std::size_t i = 0; i < all.size(); ++i) {
        // The smallest ring round this one: the last of the larger that holds
        // one of its vertices, since rings that do not cross nest.
        const Point2 probe = all[i].path.pieces.front().from;
        for (std::size_t j = 0; j < i; ++j)
            if (ring_contains(all[j].xs, all[j].ys, probe)) {
                all[i].depth  = all[j].depth + 1;
                all[i].holder = j;
            }
        CurvePath& path = all[i].path;
        if (all[i].depth % 2 == 0) {
            if (path_area(path) < 0) path = reversed(path);
            face_of[i] = out.size();
            out.push_back(KernelFace{.outer = std::move(path), .holes = {}});
        } else {
            if (path_area(path) > 0) path = reversed(path);
            out[face_of[all[i].holder]].holes.push_back(std::move(path));
        }
    }
    return out;
}

#if !PIRICAD_HAVE_OCCT

bool kernel_available() noexcept
{
    return false;
}

std::string kernel_version()
{
    return "Bu yapıda geometri çekirdeği (OpenCASCADE) yok; PIRICAD_WITH_OCCT=ON ile derleyin.";
}

Result<std::vector<KernelFace>>
kernel_boolean(std::span<const KernelFace>, std::span<const KernelFace>, BooleanOp, std::stop_token)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<std::vector<CurvePath>> kernel_offset(const CurvePath&, Mm, OffsetCorner, bool)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<PathMeets> kernel_meets(const PathPiece&, const PathPiece&)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<KernelFaceIssue> kernel_face_issue(const KernelFace&)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<KernelOverlap> kernel_overlap(const KernelFace&, const KernelFace&, Mm, std::stop_token)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<Box2> kernel_bounds(const CurvePath&)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<std::vector<KernelCoverageGap>> kernel_coverage_gaps(std::span<const KernelFace>, Mm,
                                                            std::stop_token)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

#else

namespace {

/// OCCT observes the command worker's stop during the boolean itself.
class StopProgress final : public Message_ProgressIndicator
{
public:
    explicit StopProgress(std::stop_token stop) : stop_(stop) {}

    Standard_Boolean UserBreak() override { return stop_.stop_requested(); }

    void Show(const Message_ProgressScope&, Standard_Boolean) override {}

private:
    std::stop_token stop_;
};

/// The frame every shape is built in: millimetres, about an origin the input
/// gives, so a TUREF coordinate of four thousand kilometres does not spend the
/// double's mantissa on the part every vertex shares. An integer millimetre
/// within ten kilometres of the origin is exact in a double, and so is the way
/// back (`back`), which rounds once.
struct Frame
{
    Point2 origin{};

    gp_Pnt pnt(Point2 p) const
    {
        return gp_Pnt(static_cast<double>(p.x - origin.x), static_cast<double>(p.y - origin.y),
                      0.0);
    }

    Point2 back(const gp_Pnt& p) const
    {
        return Point2{origin.x + mm_round(p.X()), origin.y + mm_round(p.Y())};
    }
};

/// A bounded OCCT curve and the parameters of the document's walk. The
/// intersector walks increasing parameters; the document may walk backwards.
struct Curve2d
{
    Handle(Geom2d_Curve) curve;
    double start{0.0};
    double end{1.0};

    [[nodiscard]] double fraction(double u) const
    {
        return std::clamp((u - start) / (end - start), 0.0, 1.0);
    }
};

Result<Curve2d> curve2d(const PathPiece& p, const Frame& frame)
{
    const auto point = [&frame](Point2 at) {
        return gp_Pnt2d(static_cast<double>(at.x - frame.origin.x),
                        static_cast<double>(at.y - frame.origin.y));
    };
    constexpr double rad = std::numbers::pi / (180.0 * 1'000'000.0);
    Curve2d out;
    switch (p.kind) {
    case PathPiece::Kind::Segment: {
        const gp_Vec2d direction(point(p.from), point(p.to));
        out.end = direction.Magnitude();
        if (out.end == 0.0)
            return err(ErrorCode::InvalidArgument,
                       "Sıfır uzunluklu kenarın kesişimi hesaplanamaz.");
        out.curve = new Geom2d_Line(point(p.from), gp_Dir2d(direction));
        break;
    }
    case PathPiece::Kind::Arc:
        out.curve = new Geom2d_Circle(gp_Ax2d(point(p.centre), gp_Dir2d(1.0, 0.0)),
                                      static_cast<double>(p.radius));
        out.start =
            static_cast<double>(atan2_udeg(p.from.y - p.centre.y, p.from.x - p.centre.x)) * rad;
        out.end = out.start + (static_cast<double>(p.sweep_udeg) * rad);
        break;
    case PathPiece::Kind::Ellipse: {
        const gp_Vec2d u(point(p.centre), point(p.major));
        const gp_Vec2d v(point(p.centre), point(p.minor));
        if (u.Magnitude() == 0.0 || v.Magnitude() == 0.0)
            return err(ErrorCode::InvalidArgument, "Elipsin eksenleri sıfır uzunluklu olamaz.");
        if (u.Crossed(v) == 0.0)
            return err(ErrorCode::InvalidArgument, "Elipsin eksenleri aynı doğru üzerinde olamaz.");
        // The stored axes can be conjugate (and integer rounding alone can
        // make them non-perpendicular). Rotate the parameter frame to the
        // principal axes before giving it to OCCT: P(t) is unchanged, while
        // its new parameter is t − phase. gp_Ax22d would otherwise silently
        // square the second axis and change the document's actual ellipse.
        const double phase =
            0.5 * atan2_rad(2.0 * u.Dot(v), u.SquareMagnitude() - v.SquareMagnitude());
        const SinCos turn    = sin_cos_rad(phase);
        const gp_Vec2d major = (u * turn.cos) + (v * turn.sin);
        const gp_Vec2d minor = (v * turn.cos) - (u * turn.sin);
        out.curve = new Geom2d_Ellipse(gp_Ax22d(point(p.centre), gp_Dir2d(major), gp_Dir2d(minor)),
                                       major.Magnitude(), minor.Magnitude());
        out.start = (static_cast<double>(p.start_udeg) * rad) - phase;
        out.end   = out.start + (static_cast<double>(p.sweep_udeg) * rad);
        break;
    }
    case PathPiece::Kind::Spline: {
        const auto degree = static_cast<std::size_t>(p.spline.degree);
        const auto knots  = p.spline.knots_nano.empty()
                                ? uniform_clamped_knots(p.controls.size(), p.spline.degree)
                                : p.spline.knots_nano;
        if (degree == 0 || p.controls.size() < degree + 1 ||
            knots.size() != p.controls.size() + degree + 1 ||
            (!p.spline.weights_nano.empty() && p.spline.weights_nano.size() != p.controls.size()))
            return err(ErrorCode::InvalidArgument,
                       "Spline kontrol noktaları, derece ve düğümler uyuşmuyor.");
        std::vector<std::int64_t> distinct;
        std::vector<int> counts;
        for (const auto knot : knots) {
            if (!distinct.empty() && knot < distinct.back())
                return err(ErrorCode::InvalidArgument, "Spline düğümleri artan sırada olmalıdır.");
            if (!distinct.empty() && knot == distinct.back()) {
                ++counts.back();
            } else {
                distinct.push_back(knot);
                counts.push_back(1);
            }
        }
        TColgp_Array1OfPnt2d poles(1, static_cast<int>(p.controls.size()));
        TColStd_Array1OfReal weights(1, poles.Length());
        for (int i = 1; i <= poles.Length(); ++i) {
            const auto j = static_cast<std::size_t>(i - 1);
            poles.SetValue(i, point(p.controls[j]));
            weights.SetValue(i, p.spline.weights_nano.empty()
                                    ? 1.0
                                    : static_cast<double>(p.spline.weights_nano[j]) /
                                          static_cast<double>(kNano));
        }
        TColStd_Array1OfReal values(1, static_cast<int>(distinct.size()));
        TColStd_Array1OfInteger mults(1, values.Length());
        // Subtract the knot origin before converting: large nano-fixed-point
        // values must not hide a small domain in the double's mantissa.
        const auto parameter = [&knots](std::int64_t k) {
            return static_cast<double>(static_cast<Int128>(k) - knots.front()) /
                   static_cast<double>(kNano);
        };
        for (int i = 1; i <= values.Length(); ++i) {
            values.SetValue(i, parameter(distinct[static_cast<std::size_t>(i - 1)]));
            mults.SetValue(i, counts[static_cast<std::size_t>(i - 1)]);
        }
        // `periodic` is import provenance; the model evaluates the written knot
        // vector as a non-periodic B-spline, so the adapter must do the same.
        out.curve = new Geom2d_BSplineCurve(poles, weights, values, mults, p.spline.degree, false);
        out.start = parameter(knots[degree]);
        out.end   = parameter(knots[p.controls.size()]);
        break;
    }
    }
    if (!(std::abs(out.end - out.start) > 0.0))
        return err(ErrorCode::InvalidArgument, "Eğrinin parametre aralığı boş olamaz.");
    out.curve = new Geom2d_TrimmedCurve(out.curve, std::min(out.start, out.end),
                                        std::max(out.start, out.end), true, false);
    return out;
}

/// Two straight pieces in a row that lie on one line, made one — EXACTLY: the
/// cross product of their directions zero in integers, the turn between them
/// none. What a lone segment split at its middle for the kernel comes back with.
void join_collinear(CurvePath& path)
{
    std::vector<PathPiece> out;
    out.reserve(path.pieces.size());
    for (const PathPiece& p : path.pieces) {
        if (!out.empty()) {
            PathPiece& last = out.back();
            if (last.kind == PathPiece::Kind::Segment && p.kind == PathPiece::Kind::Segment) {
                const Int128 ax = last.to.x - last.from.x;
                const Int128 ay = last.to.y - last.from.y;
                const Int128 bx = p.to.x - p.from.x;
                const Int128 by = p.to.y - p.from.y;
                if ((ax * by) - (ay * bx) == 0 && (ax * bx) + (ay * by) > 0) {
                    last.to = p.to;
                    continue;
                }
            }
        }
        out.push_back(p);
    }
    // ACROSS THE SEAM of a closed path, where the kernel may have started it.
    if (path.closed && out.size() > 2) {
        const PathPiece& last = out.back();
        PathPiece& first      = out.front();
        if (last.kind == PathPiece::Kind::Segment && first.kind == PathPiece::Kind::Segment) {
            const Int128 ax = last.to.x - last.from.x;
            const Int128 ay = last.to.y - last.from.y;
            const Int128 bx = first.to.x - first.from.x;
            const Int128 by = first.to.y - first.from.y;
            if ((ax * by) - (ay * bx) == 0 && (ax * bx) + (ay * by) > 0) {
                first.from = last.from;
                out.pop_back();
            }
        }
    }
    path.pieces = std::move(out);
}

/// Why `path` cannot go into the kernel, or nothing when it can.
std::optional<std::string> unfit(const CurvePath& path, bool must_close)
{
    if (path.pieces.empty()) return std::string("Boş bir yol geometri çekirdeğine verilemez.");
    if (must_close && !path.closed)
        return std::string("Alan işlemi kapalı bir sınır ister; açık bir yol verildi.");
    for (const PathPiece& p : path.pieces)
        if (p.kind != PathPiece::Kind::Segment && p.kind != PathPiece::Kind::Arc)
            return std::string("Elips ve spline kenarları geometri çekirdeğine bu aşamada "
                               "verilmiyor; yalnız doğru ve yay kenarları.");
    return std::nullopt;
}

/// A path as a wire, one vertex per point so consecutive edges meet in the
/// topology and not merely in their numbers. A zero-length segment is left out.
TopoDS_Wire wire_of(const CurvePath& path, const Frame& f)
{
    const std::size_t n = path.pieces.size();
    BRepBuilderAPI_MakeWire wire;

    // A WHOLE CIRCLE is one closed edge with no second vertex.
    if (n == 1 && path.closed && path.pieces.front().kind == PathPiece::Kind::Arc) {
        const PathPiece& p = path.pieces.front();
        const gp_Circ circ(gp_Ax2(f.pnt(p.centre), gp::DZ()), static_cast<double>(p.radius));
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ).Edge();
        if (p.sweep_udeg < 0) edge.Reverse();
        wire.Add(edge);
        return wire.Wire();
    }

    // Topology diagnostics take ellipses and NURBS as their true curves. The
    // existing boolean writer still restricts its output to segments/arcs.
    // Let OCCT create the curve's own vertices: rounded ellipse endpoints need
    // not lie on the unrounded ellipse to the kernel's confusion tolerance.
    if (n == 1 && (path.pieces.front().kind == PathPiece::Kind::Ellipse ||
                   path.pieces.front().kind == PathPiece::Kind::Spline)) {
        auto curve = curve2d(path.pieces.front(), f);
        if (!curve) throw Standard_Failure(curve.error().message.c_str());
        const auto& c      = curve.value();
        const auto spatial = GeomAPI::To3d(c.curve, gp_Pln(gp::XOY()));
        TopoDS_Edge edge;
        if (path.closed) {
            // A closed NURBS can return to the same stored millimetre with
            // endpoints a fraction of a millimetre apart on the true curve.
            // Use its single declared corner with the rounded-fit allowance,
            // just as mixed curve/segment wires share their rounded corners.
            const TopoDS_Vertex corner = BRepBuilderAPI_MakeVertex(f.pnt(path.pieces.front().from));
            BRep_Builder vertices;
            vertices.UpdateVertex(corner, kRoundedFitMm);
            edge = BRepBuilderAPI_MakeEdge(spatial, corner, corner, std::min(c.start, c.end),
                                           std::max(c.start, c.end))
                       .Edge();
        } else {
            edge =
                BRepBuilderAPI_MakeEdge(spatial, std::min(c.start, c.end), std::max(c.start, c.end))
                    .Edge();
        }
        if (c.start > c.end) edge.Reverse();
        wire.Add(edge);
        return wire.Wire();
    }

    std::vector<TopoDS_Vertex> at;
    at.reserve(n + 1);
    for (const PathPiece& p : path.pieces)
        at.push_back(BRepBuilderAPI_MakeVertex(f.pnt(p.from)).Vertex());
    if (!path.closed)
        at.push_back(BRepBuilderAPI_MakeVertex(f.pnt(path.pieces.back().to)).Vertex());

    for (std::size_t i = 0; i < n; ++i) {
        const PathPiece& p     = path.pieces[i];
        const TopoDS_Vertex& a = at[i];
        const TopoDS_Vertex& b = path.closed ? at[(i + 1) % n] : at[i + 1];
        if (p.kind == PathPiece::Kind::Segment) {
            if (p.from == p.to) continue;
            wire.Add(BRepBuilderAPI_MakeEdge(a, b).Edge());
            continue;
        }
        if (p.kind == PathPiece::Kind::Ellipse || p.kind == PathPiece::Kind::Spline) {
            auto curve = curve2d(p, f);
            if (!curve) throw Standard_Failure(curve.error().message.c_str());
            const auto& c = curve.value();
            // Stored curve endpoints are rounded millimetres. OCCT retains
            // the exact curve and shares the document's corner vertices with
            // adjoining segments, within the existing rounded-fit allowance.
            BRep_Builder vertices;
            vertices.UpdateVertex(a, kRoundedFitMm);
            vertices.UpdateVertex(b, kRoundedFitMm);
            const bool forward = c.start < c.end;
            TopoDS_Edge edge =
                BRepBuilderAPI_MakeEdge(GeomAPI::To3d(c.curve, gp_Pln(gp::XOY())), forward ? a : b,
                                        forward ? b : a, std::min(c.start, c.end),
                                        std::max(c.start, c.end))
                    .Edge();
            if (!forward) edge.Reverse();
            wire.Add(edge);
            continue;
        }
        // THE CIRCLE'S OWN DIRECTION IS COUNTER-CLOCKWISE about +Z; an arc
        // walked clockwise is the counter-clockwise edge from its end, reversed.
        const gp_Circ circ(gp_Ax2(f.pnt(p.centre), gp::DZ()), static_cast<double>(p.radius));
        const bool ccw = p.sweep_udeg >= 0;
        BRepBuilderAPI_MakeEdge on_circle(circ, ccw ? a : b, ccw ? b : a);
        if (on_circle.IsDone()) {
            TopoDS_Edge edge = on_circle.Edge();
            if (!ccw) edge.Reverse();
            wire.Add(edge);
            continue;
        }
        // THE ENDS ARE THE NEIGHBOURS' VERTICES. A stored arc keeps its centre
        // and radius to the millimetre, so an arc through three points in
        // general position — KENARTÜRÜ's, a DXF bulge's — has its ends a
        // fraction of a millimetre off its own circle: more than the kernel's
        // confusion, which refused the edge, the face and the whole operation
        // ("command not done"). Such an arc is the circle through its two ends
        // and its midpoint instead: the arc the document holds, to the
        // millimetre it holds it, and one whose ends ARE the vertices beside it.
        const GC_MakeArcOfCircle through(
            f.pnt(p.from), f.pnt(point_at(path, PathPlace{.piece = i, .t = 0.5})), f.pnt(p.to));
        if (!through.IsDone()) {
            // Neither road holds it: the kernel's own "not done", which the
            // caller catches and words, like every other failure here.
            wire.Add(on_circle.Edge());
            continue;
        }
        wire.Add(BRepBuilderAPI_MakeEdge(Handle(Geom_Curve)(through.Value()), a, b).Edge());
    }
    return wire.Wire();
}

/// The path walked the way that makes its area's sign `positive`, starting
/// at its lowest, then leftmost, vertex: the one shape a closed path is handed
/// back in, whichever vertex and direction the kernel happened to start from.
CurvePath normalised(CurvePath path, bool positive)
{
    if (!path.closed || path.pieces.empty()) return path;
    if ((path_area(path) > 0) != positive) path = reversed(path);
    std::size_t start = 0;
    for (std::size_t i = 1; i < path.pieces.size(); ++i) {
        const Point2 p = path.pieces[i].from;
        const Point2 q = path.pieces[start].from;
        if (p.y < q.y || (p.y == q.y && p.x < q.x)) start = i;
    }
    std::ranges::rotate(path.pieces, path.pieces.begin() + static_cast<std::ptrdiff_t>(start));
    return path;
}

/// One edge back as a piece, its ends rounded to the millimetre and an arc's
/// sweep worked out again from them (`arc_piece`). Nothing for a piece the
/// rounding made nothing of, or a curve this stage does not take back.
std::optional<PathPiece> piece_of(const TopoDS_Edge& edge, const Frame& f, bool& unknown)
{
    const Point2 a = f.back(BRep_Tool::Pnt(TopExp::FirstVertex(edge, Standard_True)));
    const Point2 b = f.back(BRep_Tool::Pnt(TopExp::LastVertex(edge, Standard_True)));
    const BRepAdaptor_Curve curve(edge);
    switch (curve.GetType()) {
    case GeomAbs_Line:
        if (a == b) return std::nullopt;
        return PathPiece{.kind = PathPiece::Kind::Segment, .from = a, .to = b};
    case GeomAbs_Circle: {
        const gp_Circ c = curve.Circle();
        // A SHORT ARC THE MILLIMETRE SWALLOWED would come back as a whole
        // circle — coincident ends are a full turn to `arc_sweep_udeg`.
        constexpr double kTurn = 2.0 * std::numbers::pi;
        if (a == b && curve.LastParameter() - curve.FirstParameter() < kTurn - 1e-9)
            return std::nullopt;
        const bool ccw =
            (c.Axis().Direction().Z() > 0.0) != (edge.Orientation() == TopAbs_REVERSED);
        return arc_piece(f.back(c.Location()), mm_round(c.Radius()), a, b, ccw);
    }
    default: unknown = true; return std::nullopt;
    }
}

/// A wire back as a path, in the order the explorer walks it.
CurvePath path_of_wire(const TopoDS_Wire& wire, const TopoDS_Face* face, const Frame& f,
                       bool& unknown)
{
    CurvePath out;
    BRepTools_WireExplorer walk =
        face != nullptr ? BRepTools_WireExplorer(wire, *face) : BRepTools_WireExplorer(wire);
    for (; walk.More(); walk.Next())
        if (auto piece = piece_of(walk.Current(), f, unknown)) out.pieces.push_back(*piece);
    out.closed = !out.pieces.empty() && out.pieces.front().from == out.pieces.back().to;
    return out;
}

/// The key the faces are ordered by: the boundary's first vertex — its
/// lowest, then leftmost — and then its area.
std::tuple<Mm, Mm, Mm2> order_key(const KernelFace& face)
{
    const Point2 p = face.outer.pieces.empty() ? Point2{} : face.outer.pieces.front().from;
    return {p.y, p.x, path_area(face.outer)};
}

/// Every face of `shape`, back as `KernelFace`s in the one order.
Result<std::vector<KernelFace>> faces_of(const TopoDS_Shape& shape, const Frame& f)
{
    std::vector<KernelFace> out;
    bool unknown = false;
    for (TopExp_Explorer at(shape, TopAbs_FACE); at.More(); at.Next()) {
        const TopoDS_Face face  = TopoDS::Face(at.Current());
        const TopoDS_Wire outer = BRepTools::OuterWire(face);
        KernelFace one;
        for (TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
            const TopoDS_Wire wire = TopoDS::Wire(w.Current());
            CurvePath path         = path_of_wire(wire, &face, f, unknown);
            if (!path.closed || path.pieces.empty()) continue;
            if (wire.IsSame(outer))
                one.outer = normalised(std::move(path), true);
            else
                one.holes.push_back(normalised(std::move(path), false));
        }
        if (one.outer.pieces.empty()) continue;
        std::ranges::sort(one.holes, [](const CurvePath& x, const CurvePath& y) {
            const Point2 p = x.pieces.front().from;
            const Point2 q = y.pieces.front().from;
            return std::tie(p.y, p.x) < std::tie(q.y, q.x);
        });
        out.push_back(std::move(one));
    }
    if (unknown)
        return err(ErrorCode::Unsupported,
                   "Geometri çekirdeği doğru ve yay olmayan bir kenar üretti; bu aşamada geri "
                   "alınamıyor.");
    std::ranges::sort(
        out, [](const KernelFace& x, const KernelFace& y) { return order_key(x) < order_key(y); });
    return out;
}

/// A face built from `face`, its boundary counter-clockwise and its holes
/// clockwise, which is what the kernel reads a hole by.
TopoDS_Face face_of(const KernelFace& face, const Frame& f)
{
    const CurvePath outer = path_area(face.outer) < 0 ? reversed(face.outer) : face.outer;
    BRepBuilderAPI_MakeFace make(wire_of(outer, f), Standard_True);
    for (const CurvePath& hole : face.holes)
        make.Add(wire_of(path_area(hole) > 0 ? reversed(hole) : hole, f));
    return make.Face();
}

/// The first point of the input: the frame's origin (`Frame`).
Point2 origin_of(std::span<const KernelFace> a, std::span<const KernelFace> b)
{
    for (const auto set : {a, b})
        for (const KernelFace& face : set)
            if (!face.outer.pieces.empty()) return face.outer.pieces.front().from;
    return Point2{};
}

std::string failure(const Standard_Failure& e)
{
    const char* said = e.GetMessageString();
    return std::string("Geometri çekirdeği işlemi tamamlayamadı") +
           (said != nullptr && *said != '\0' ? std::string(": ") + said : std::string()) + ".";
}

/// A display contour, not a source for measurements or stored geometry.
Result<std::vector<Point2>> display_wire(const TopoDS_Wire& wire, const TopoDS_Face& face,
                                         const Frame& frame, std::stop_token stop)
{
    std::vector<Point2> out;
    for (BRepTools_WireExplorer w(wire, face); w.More(); w.Next()) {
        if (stop.stop_requested())
            return err(ErrorCode::Cancelled, "Topoloji denetimi durduruldu; sonuç verilmedi.");
        const TopoDS_Edge edge = w.Current();
        const BRepAdaptor_Curve curve(edge);
        GCPnts_QuasiUniformDeflection points(curve, 1.0);
        if (!points.IsDone())
            return err(ErrorCode::Internal, "OpenCASCADE topoloji işaretini üretemedi.");
        const bool reverse = edge.Orientation() == TopAbs_REVERSED;
        for (int n = 1; n <= points.NbPoints(); ++n) {
            const Point2 p = frame.back(points.Value(reverse ? points.NbPoints() + 1 - n : n));
            if (out.empty() || out.back() != p) out.push_back(p);
        }
    }
    if (out.size() > 1 && out.front() == out.back()) out.pop_back();
    // Canonical seam for deterministic reports, independent of native traversal.
    if (!out.empty()) {
        const auto first = std::ranges::min_element(
            out, [](Point2 a, Point2 b) { return std::tie(a.y, a.x) < std::tie(b.y, b.x); });
        std::rotate(out.begin(), first, out.end());
    }
    return out;
}

} // namespace

bool kernel_available() noexcept
{
    return true;
}

Result<KernelFaceIssue> kernel_face_issue(const KernelFace& face)
{
#if defined(TRACY_ENABLE)
    ZoneScopedN("kernel_face_issue");
#endif
    const auto connected = [](const CurvePath& path) {
        if (path.pieces.empty() || !path.closed) return false;
        for (std::size_t i = 0; i < path.pieces.size(); ++i)
            if (path.pieces[i].to != path.pieces[(i + 1) % path.pieces.size()].from) return false;
        return true;
    };
    if (!connected(face.outer)) return KernelFaceIssue::InvalidBoundary;
    for (const auto& hole : face.holes)
        if (!connected(hole)) return KernelFaceIssue::InvalidBoundary;
    const Frame frame{face.outer.pieces.front().from};
    try {
        const TopoDS_Face shape = face_of(face, frame);
        for (TopExp_Explorer w(shape, TopAbs_WIRE); w.More(); w.Next()) {
            BRepCheck_Wire check(TopoDS::Wire(w.Current()));
            TopoDS_Edge first, second;
            if (check.SelfIntersect(shape, first, second) == BRepCheck_SelfIntersectingWire)
                return KernelFaceIssue::SelfIntersection;
        }
        return BRepCheck_Analyzer(shape, Standard_True).IsValid()
                   ? KernelFaceIssue::None
                   : KernelFaceIssue::InvalidBoundary;
    } catch (const StdFail_NotDone&) {
        // A disconnected or degenerate wire cannot be made into a face. That
        // is a finding, not a repaired face nor a successfully empty check.
        return KernelFaceIssue::InvalidBoundary;
    } catch (const Standard_Failure& e) {
        return err(ErrorCode::Internal, failure(e));
    }
}

Result<Box2> kernel_bounds(const CurvePath& path)
{
#if defined(TRACY_ENABLE)
    ZoneScopedN("kernel_bounds");
#endif
    if (path.pieces.empty()) return Box2{};
    const Frame frame{path.pieces.front().from};
    try {
        Bnd_Box box;
        BRepBndLib::AddOptimal(wire_of(path, frame), box, Standard_False, Standard_False);
        if (box.IsVoid()) return Box2{};
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        return Box2{
            frame.origin.x + mm_round(std::floor(x0)), frame.origin.y + mm_round(std::floor(y0)),
            frame.origin.x + mm_round(std::ceil(x1)), frame.origin.y + mm_round(std::ceil(y1))};
    } catch (const Standard_Failure& e) {
        return err(ErrorCode::Internal, failure(e));
    }
}

Result<KernelOverlap> kernel_overlap(const KernelFace& a, const KernelFace& b, Mm tolerance,
                                     std::stop_token stop)
{
#if defined(TRACY_ENABLE)
    ZoneScopedN("kernel_overlap");
#endif
    if (tolerance < 0) return err(ErrorCode::InvalidArgument, "Topoloji toleransı negatif olamaz.");
    const auto stopped = [] {
        return err(ErrorCode::Cancelled, "Topoloji denetimi durduruldu; sonuç verilmedi.");
    };
    if (stop.stop_requested()) return stopped();
    for (const auto* face : {&a, &b}) {
        if (face->outer.pieces.empty() || !face->outer.closed)
            return err(ErrorCode::InvalidArgument, "Örtüşme denetimi kapalı alanlar ister.");
        for (const auto& hole : face->holes)
            if (hole.pieces.empty() || !hole.closed)
                return err(ErrorCode::InvalidArgument, "Örtüşme denetimi kapalı delikler ister.");
    }
    const Frame frame{a.outer.pieces.front().from};
    try {
        BRepAlgoAPI_Common common;
        TopTools_ListOfShape arguments, tools;
        arguments.Append(face_of(a, frame));
        tools.Append(face_of(b, frame));
        common.SetArguments(arguments);
        common.SetTools(tools);
        common.SetRunParallel(Standard_False);
        Handle(Message_ProgressIndicator) progress = new StopProgress(stop);
        common.Build(progress->Start());
        if (stop.stop_requested()) return stopped();
        if (!common.IsDone() || common.HasErrors())
            return err(ErrorCode::Internal, "OpenCASCADE örtüşme denetimini tamamlayamadı; "
                                            "sonuç verilmedi.");

        KernelOverlap out;
        std::vector<double> areas;
        double largest_area = 0.0;
        TopoDS_Face largest;
        for (TopExp_Explorer f(common.Shape(), TopAbs_FACE); f.More(); f.Next()) {
            if (stop.stop_requested()) return stopped();
            const TopoDS_Face piece = TopoDS::Face(f.Current());
            GProp_GProps surface, boundary;
            BRepGProp::SurfaceProperties(piece, surface);
            BRepGProp::LinearProperties(piece, boundary);
            const double area = std::abs(surface.Mass());
            areas.push_back(area);
            if (area > 0.0 && 2.0 * area > static_cast<double>(tolerance) * boundary.Mass())
                out.exceeds_tolerance = true;
            if (area > largest_area) {
                largest_area = area;
                largest      = piece;
            }
        }
        // A named summation order independent of OCCT's face traversal:
        // smaller contributions first, then one square-millimetre rounding.
        std::ranges::sort(areas);
        double total = 0.0;
        for (const double area : areas)
            total += area;
        out.area = mm_round(total);
        if (out.exceeds_tolerance && !largest.IsNull()) {
            // Only the canvas outline is sampled. Area and tolerance were
            // computed on the exact intersection above, without these points.
            const TopoDS_Wire outer = BRepTools::OuterWire(largest);
            for (BRepTools_WireExplorer w(outer, largest); w.More(); w.Next()) {
                if (stop.stop_requested()) return stopped();
                const TopoDS_Edge edge = w.Current();
                const BRepAdaptor_Curve curve(edge);
                GCPnts_QuasiUniformDeflection points(curve, 1.0);
                if (!points.IsDone())
                    return err(ErrorCode::Internal, "OpenCASCADE örtüşme işaretini üretemedi.");
                const bool reverse = edge.Orientation() == TopAbs_REVERSED;
                for (int n = 1; n <= points.NbPoints(); ++n) {
                    const Point2 p =
                        frame.back(points.Value(reverse ? points.NbPoints() + 1 - n : n));
                    if (out.region.empty() || out.region.back() != p) out.region.push_back(p);
                }
            }
            if (out.region.size() > 1 && out.region.front() == out.region.back())
                out.region.pop_back();
        }
        if (stop.stop_requested()) return stopped();
        return out;
    } catch (const Standard_Failure& e) {
        if (stop.stop_requested()) return stopped();
        return err(ErrorCode::Internal, failure(e));
    }
}

Result<std::vector<KernelCoverageGap>> kernel_coverage_gaps(std::span<const KernelFace> faces,
                                                            Mm tolerance, std::stop_token stop)
{
#if defined(TRACY_ENABLE)
    ZoneScopedN("kernel_coverage_gaps");
#endif
    if (tolerance < 0) return err(ErrorCode::InvalidArgument, "Topoloji toleransı negatif olamaz.");
    const auto stopped = [] {
        return err(ErrorCode::Cancelled, "Topoloji denetimi durduruldu; sonuç verilmedi.");
    };
    if (stop.stop_requested()) return stopped();
    for (const auto& face : faces) {
        if (face.outer.pieces.empty() || !face.outer.closed)
            return err(ErrorCode::InvalidArgument, "Kapsama denetimi kapalı alanlar ister.");
        for (const auto& hole : face.holes)
            if (hole.pieces.empty() || !hole.closed)
                return err(ErrorCode::InvalidArgument, "Kapsama denetimi kapalı delikler ister.");
    }
    if (faces.size() < 2) return std::vector<KernelCoverageGap>{};
    const Frame frame{origin_of(faces, {})};
    try {
        TopTools_ListOfShape arguments, tools, exclusions;
        for (const auto& face : faces) {
            if (stop.stop_requested()) return stopped();
            const TopoDS_Face shape = face_of(face, frame);
            if (arguments.IsEmpty())
                arguments.Append(shape);
            else
                tools.Append(shape);
            for (const auto& hole : face.holes)
                exclusions.Append(face_of(KernelFace{hole, {}}, frame));
        }
        Handle(Message_ProgressIndicator) progress = new StopProgress(stop);
        BRepAlgoAPI_Fuse unite;
        unite.SetArguments(arguments);
        unite.SetTools(tools);
        unite.SetRunParallel(Standard_False);
        unite.SetNonDestructive(Standard_True);
        unite.Build(progress->Start());
        if (stop.stop_requested()) return stopped();
        if (!unite.IsDone() || unite.HasErrors())
            return err(ErrorCode::Internal, "OpenCASCADE kapsama birleşimini tamamlayamadı; "
                                            "sonuç verilmedi.");
        // Fuse retains adjacent coplanar subfaces. Native unification removes
        // their seams, exposing the actual inner wires of the whole coverage.
        unite.SimplifyResult(Standard_True, Standard_True);
        if (stop.stop_requested()) return stopped();
        TopTools_ListOfShape candidates;
        for (TopExp_Explorer f(unite.Shape(), TopAbs_FACE); f.More(); f.Next()) {
            const TopoDS_Face face  = TopoDS::Face(f.Current());
            const TopoDS_Wire outer = BRepTools::OuterWire(face);
            for (TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
                if (stop.stop_requested()) return stopped();
                if (w.Current().IsSame(outer)) continue;
                auto wire = TopoDS::Wire(w.Current());
                wire.Reverse(); // an inner boundary becomes the candidate's exterior
                candidates.Append(BRepBuilderAPI_MakeFace(wire, Standard_True).Face());
            }
        }
        if (candidates.IsEmpty()) return std::vector<KernelCoverageGap>{};
        // A hole of the union can contain disconnected covered islands. Remove
        // the coverage AND declared exclusions before measuring each gap.
        exclusions.Append(unite.Shape());
        BRepAlgoAPI_Cut uncovered;
        uncovered.SetArguments(candidates);
        uncovered.SetTools(exclusions);
        uncovered.SetRunParallel(Standard_False);
        uncovered.SetNonDestructive(Standard_True);
        uncovered.Build(progress->Start());
        if (stop.stop_requested()) return stopped();
        if (!uncovered.IsDone() || uncovered.HasErrors())
            return err(ErrorCode::Internal, "OpenCASCADE kapsama boşluklarını çıkaramadı; "
                                            "sonuç verilmedi.");
        uncovered.SimplifyResult(Standard_True, Standard_True);
        std::vector<KernelCoverageGap> out;
        for (TopExp_Explorer f(uncovered.Shape(), TopAbs_FACE); f.More(); f.Next()) {
            if (stop.stop_requested()) return stopped();
            const TopoDS_Face face = TopoDS::Face(f.Current());
            GProp_GProps surface, boundary;
            BRepGProp::SurfaceProperties(face, surface);
            BRepGProp::LinearProperties(face, boundary);
            const double area = std::abs(surface.Mass());
            if (!(area > 0.0 && 2.0 * area > static_cast<double>(tolerance) * boundary.Mass()))
                continue;
            KernelCoverageGap gap;
            gap.area = mm_round(area); // each independent finding, rounded once
            if (gap.area <= 0) continue;
            const TopoDS_Wire outer = BRepTools::OuterWire(face);
            auto region             = display_wire(outer, face, frame, stop);
            if (!region) return region.error();
            gap.region = std::move(region.value());
            for (TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
                if (w.Current().IsSame(outer)) continue;
                auto hole = display_wire(TopoDS::Wire(w.Current()), face, frame, stop);
                if (!hole) return hole.error();
                gap.holes.push_back(std::move(hole.value()));
            }
            std::ranges::sort(gap.holes);
            out.push_back(std::move(gap));
        }
        std::ranges::sort(out, [](const auto& a, const auto& b) {
            return std::tie(a.region, a.holes, a.area) < std::tie(b.region, b.holes, b.area);
        });
        if (stop.stop_requested()) return stopped();
        return out;
    } catch (const Standard_Failure& e) {
        if (stop.stop_requested()) return stopped();
        return err(ErrorCode::Internal, failure(e));
    }
}

std::string kernel_version()
{
    return std::string("OpenCASCADE ") + OCC_VERSION_COMPLETE;
}

Result<PathMeets> kernel_meets(const PathPiece& a, const PathPiece& b)
{
    try {
        const Frame frame{a.from};
        auto first = curve2d(a, frame);
        if (!first) return first.error();
        auto second = curve2d(b, frame);
        if (!second) return second.error();
        // OCCT works in local millimetres. A tight computational tolerance
        // keeps a tangency from becoming a long shared stretch; storage still
        // rounds exactly once to whole millimetres below.
        const Geom2dAPI_InterCurveCurve solve(first.value().curve, second.value().curve, 1e-7);
        const auto& inter = solve.Intersector();
        if (!inter.IsDone())
            return err(ErrorCode::ValidationFailed, "OpenCASCADE eğri kesişimini çözemedi.");
        PathMeets out;
        const auto append = [&](const IntRes2d_IntersectionPoint& hit, bool touching) {
            gp_Pnt2d p;
            gp_Pnt2d q;
            gp_Vec2d dp;
            gp_Vec2d dq;
            first.value().curve->D1(hit.ParamOnFirst(), p, dp);
            second.value().curve->D1(hit.ParamOnSecond(), q, dq);
            const double product = dp.Magnitude() * dq.Magnitude();
            touching = touching || (product > 0.0 && std::abs(dp.Crossed(dq)) <= 1e-6 * product);
            out.crossings.push_back(PathCrossing{
                .at = {.piece = 0, .t = first.value().fraction(hit.ParamOnFirst())},
                .point =
                    {
                        .x = frame.origin.x + mm_round(hit.Value().X()),
                        .y = frame.origin.y + mm_round(hit.Value().Y()),
                    },
                .touching = touching,
            });
        };
        for (int i = 1; i <= inter.NbPoints(); ++i)
            append(inter.Point(i), false);
        for (int i = 1; i <= inter.NbSegments(); ++i) {
            const auto& shared = inter.Segment(i);
            if (!shared.HasFirstPoint() || !shared.HasLastPoint()) {
                out.unresolved = true;
                continue;
            }
            const auto& from = shared.FirstPoint();
            const auto& to   = shared.LastPoint();
            // The solver also represents a tangent by a tiny segment. A
            // stretch smaller than the storage resolution is one touch.
            const gp_Pnt2d middle =
                first.value().curve->Value((from.ParamOnFirst() + to.ParamOnFirst()) / 2.0);
            if (from.Value().Distance(to.Value()) <= kSamePointMm &&
                from.Value().Distance(middle) <= kSamePointMm) {
                append(from, true);
                continue;
            }
            const double lo = first.value().fraction(from.ParamOnFirst());
            const double hi = first.value().fraction(to.ParamOnFirst());
            out.overlaps.push_back(PathOverlap{.from = {.piece = 0, .t = std::min(lo, hi)},
                                               .to   = {.piece = 0, .t = std::max(lo, hi)}});
        }
        std::ranges::sort(out.crossings, [](const PathCrossing& p, const PathCrossing& q) {
            return p.at.t < q.at.t;
        });
        std::vector<PathCrossing> unique;
        for (const auto& hit : out.crossings) {
            if (!unique.empty() &&
                distance_squared(unique.back().point, hit.point) <= kSamePointMm * kSamePointMm) {
                unique.back().touching = unique.back().touching || hit.touching;
            } else {
                unique.push_back(hit);
            }
        }
        out.crossings = std::move(unique);
        std::ranges::sort(out.overlaps, [](const PathOverlap& p, const PathOverlap& q) {
            return p.from.t < q.from.t;
        });
        return out;
    } catch (const Standard_Failure& failure) {
        return err(ErrorCode::ValidationFailed,
                   std::string("OpenCASCADE eğri kesişimi başarısız: ") +
                       failure.GetMessageString());
    }
}

Result<std::vector<KernelFace>> kernel_boolean(std::span<const KernelFace> a,
                                               std::span<const KernelFace> b, BooleanOp op,
                                               std::stop_token stop)
{
#if defined(TRACY_ENABLE)
    ZoneScopedN("kernel_boolean");
#endif
    const auto stopped = [] {
        return err(ErrorCode::Cancelled, "Alan işlemi durduruldu; sonuç oluşturulmadı.");
    };
    if (stop.stop_requested()) return stopped();
    for (const auto set : {a, b})
        for (const KernelFace& face : set) {
            if (auto why = unfit(face.outer, true)) return err(ErrorCode::InvalidArgument, *why);
            for (const CurvePath& hole : face.holes)
                if (auto why = unfit(hole, true)) return err(ErrorCode::InvalidArgument, *why);
        }
    if (a.empty()) {
        if (b.empty()) return std::vector<KernelFace>{};
        if (op == BooleanOp::Union || op == BooleanOp::SymmetricDifference)
            return kernel_boolean(b, {}, BooleanOp::Union, stop);
        return std::vector<KernelFace>{};
    }

    const Frame f{origin_of(a, b)};
    try {
        // EVERY FACE ITS OWN ARGUMENT, so faces of one set that overlap are
        // cut against each other too: a compound argument is taken whole.
        TopTools_ListOfShape arguments;
        TopTools_ListOfShape tools;
        for (const KernelFace& face : a) {
            if (stop.stop_requested()) return stopped();
            arguments.Append(face_of(face, f));
        }
        for (const KernelFace& face : b) {
            if (stop.stop_requested()) return stopped();
            tools.Append(face_of(face, f));
        }
        // NOTHING ON THE OTHER SIDE: nothing is common with it, taking it away
        // takes nothing, and a union of ONE set is its first face fused with
        // the rest — or, for a single face, the face as the kernel hands a face
        // back, which is what normalising one is.
        if (tools.IsEmpty()) {
            if (op == BooleanOp::Intersection) return std::vector<KernelFace>{};
            if (arguments.Size() == 1) {
                ShapeUpgrade_UnifySameDomain alone(arguments.First(), Standard_True, Standard_True,
                                                   Standard_False);
                alone.Build();
                if (stop.stop_requested()) return stopped();
                return faces_of(alone.Shape(), f);
            }
            tools = arguments;
            tools.RemoveFirst();
            const TopoDS_Shape first = arguments.First();
            arguments.Clear();
            arguments.Append(first);
            if (op == BooleanOp::Difference || op == BooleanOp::SymmetricDifference)
                op = BooleanOp::Union;
        }

        Handle(Message_ProgressIndicator) progress = new StopProgress(stop);
        const auto build = [&](BRepAlgoAPI_BooleanOperation& operation) -> Result<void> {
            operation.SetRunParallel(Standard_False);
            operation.SetNonDestructive(Standard_True);
            operation.Build(progress->Start());
            if (stop.stop_requested()) return stopped();
            if (!operation.IsDone() || operation.HasErrors())
                return err(ErrorCode::Internal,
                           "OpenCASCADE alan işlemini tamamlayamadı; kaynaklar değiştirilmedi.");
            return {};
        };
        if (op == BooleanOp::SymmetricDifference) {
            BRepAlgoAPI_Cut left, right;
            left.SetArguments(arguments);
            left.SetTools(tools);
            if (auto done = build(left); !done) return done.error();
            right.SetArguments(tools);
            right.SetTools(arguments);
            if (auto done = build(right); !done) return done.error();
            const bool has_left  = TopExp_Explorer(left.Shape(), TopAbs_FACE).More();
            const bool has_right = TopExp_Explorer(right.Shape(), TopAbs_FACE).More();
            TopoDS_Shape result;
            if (has_left && has_right) {
                BRepAlgoAPI_Fuse joined;
                TopTools_ListOfShape first, second;
                first.Append(left.Shape());
                second.Append(right.Shape());
                joined.SetArguments(first);
                joined.SetTools(second);
                if (auto done = build(joined); !done) return done.error();
                joined.SimplifyResult(Standard_True, Standard_True);
                result = joined.Shape();
            } else if (has_left)
                result = left.Shape();
            else if (has_right)
                result = right.Shape();
            else
                return std::vector<KernelFace>{};
            if (stop.stop_requested()) return stopped();
            return faces_of(result, f);
        }

        BRepAlgoAPI_Fuse fuse;
        BRepAlgoAPI_Common common;
        BRepAlgoAPI_Cut cut;
        BRepAlgoAPI_BooleanOperation* run = &fuse;
        if (op == BooleanOp::Intersection) run = &common;
        if (op == BooleanOp::Difference) run = &cut;
        run->SetArguments(arguments);
        run->SetTools(tools);
        // ONE THREAD: the kernel's parallel mode may meet the same faces in a
        // different order, and the order is not allowed to show (§7.3).
        if (auto done = build(*run); !done) return done.error();

        // THE SEAMS THE OPERATION LEFT go: two collinear edges, or two arcs of
        // one circle, that it cut and put back side by side are one again.
        ShapeUpgrade_UnifySameDomain unify(run->Shape(), Standard_True, Standard_True,
                                           Standard_False);
        unify.Build();
        if (stop.stop_requested()) return stopped();
        return faces_of(unify.Shape(), f);
    } catch (const Standard_Failure& e) {
        if (stop.stop_requested()) return stopped();
        return err(ErrorCode::Internal, failure(e));
    }
}

Result<std::vector<CurvePath>> kernel_offset(const CurvePath& path, Mm distance,
                                             OffsetCorner corner, bool both_sides)
{
    if (auto why = unfit(path, false)) return err(ErrorCode::InvalidArgument, *why);
    if (distance == 0) return std::vector<CurvePath>{path};

    const Frame f{path.pieces.front().from};
    const GeomAbs_JoinType join =
        corner == OffsetCorner::Round ? GeomAbs_Arc : GeomAbs_Intersection;
    const bool lone = !path.closed && path.pieces.size() == 1 &&
                      path.pieces.front().kind == PathPiece::Kind::Segment;
    try {
        BRepOffsetAPI_MakeOffset make;
        if (path.closed) {
            // A CLOSED PATH AS THE FACE IT BOUNDS, so a positive distance is
            // outward whichever way the path was walked.
            const TopoDS_Face face = face_of(KernelFace{path, {}}, f);
            make.Init(face, join, Standard_False);
        } else if (lone) {
            // ONE STRAIGHT EDGE DEFINES NO PLANE, and the planar offset finds
            // its plane from the wire: a lone segment was refused ("command
            // not done"), and with it a 2-point line's buffer and its round-
            // cornered parallel. Given as two edges meeting at its exact middle
            // — on the line, in the kernel's own doubles — it is the same line.
            const gp_Pnt a         = f.pnt(path.pieces.front().from);
            const gp_Pnt b         = f.pnt(path.pieces.front().to);
            const TopoDS_Vertex va = BRepBuilderAPI_MakeVertex(a).Vertex();
            const TopoDS_Vertex vm =
                BRepBuilderAPI_MakeVertex(gp_Pnt((a.XYZ() + b.XYZ()) / 2.0)).Vertex();
            const TopoDS_Vertex vb = BRepBuilderAPI_MakeVertex(b).Vertex();
            BRepBuilderAPI_MakeWire halves;
            halves.Add(BRepBuilderAPI_MakeEdge(va, vm).Edge());
            halves.Add(BRepBuilderAPI_MakeEdge(vm, vb).Edge());
            make.Init(join, both_sides ? Standard_False : Standard_True);
            make.AddWire(halves.Wire());
        } else {
            make.Init(join, both_sides ? Standard_False : Standard_True);
            make.AddWire(wire_of(path, f));
        }
        make.Perform(static_cast<double>(distance));
        if (!make.IsDone())
            return err(ErrorCode::InvalidArgument,
                       "Bu uzaklıkta ofset çizilemiyor; şekil bu değerle kendi üstüne kapanıyor "
                       "olabilir.");

        std::vector<CurvePath> out;
        bool unknown = false;
        for (TopExp_Explorer w(make.Shape(), TopAbs_WIRE); w.More(); w.Next()) {
            CurvePath one = path_of_wire(TopoDS::Wire(w.Current()), nullptr, f, unknown);
            if (one.pieces.empty()) continue;
            if (lone) join_collinear(one);
            out.push_back(one.closed ? normalised(std::move(one), true) : std::move(one));
        }
        if (unknown)
            return err(ErrorCode::Unsupported,
                       "Geometri çekirdeği doğru ve yay olmayan bir ofset kenarı üretti; bu "
                       "aşamada geri alınamıyor.");
        std::ranges::sort(out, [](const CurvePath& x, const CurvePath& y) {
            const Point2 p = x.pieces.front().from;
            const Point2 q = y.pieces.front().from;
            return std::tie(p.y, p.x) < std::tie(q.y, q.x);
        });
        return out;
    } catch (const Standard_Failure& e) {
        return err(ErrorCode::Internal, failure(e));
    }
}

#endif

} // namespace piricad::core
