// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/kernel.hpp"

#include "kentos_cad/core/arc.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#if KENTOS_HAVE_OCCT
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
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
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#endif

namespace kentos::core {

#if !KENTOS_HAVE_OCCT

bool kernel_available() noexcept
{
    return false;
}

std::string kernel_version()
{
    return "Bu yapıda geometri çekirdeği (OpenCASCADE) yok; KENTOS_WITH_OCCT=ON ile derleyin.";
}

Result<std::vector<KernelFace>> kernel_boolean(std::span<const KernelFace>,
                                               std::span<const KernelFace>, BooleanOp)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

Result<std::vector<CurvePath>> kernel_offset(const CurvePath&, Mm, OffsetCorner, bool)
{
    return err(ErrorCode::Unsupported, kernel_version());
}

#else

namespace {

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

} // namespace

bool kernel_available() noexcept
{
    return true;
}

std::string kernel_version()
{
    return std::string("OpenCASCADE ") + OCC_VERSION_COMPLETE;
}

Result<std::vector<KernelFace>> kernel_boolean(std::span<const KernelFace> a,
                                               std::span<const KernelFace> b, BooleanOp op)
{
    for (const auto set : {a, b})
        for (const KernelFace& face : set) {
            if (auto why = unfit(face.outer, true)) return err(ErrorCode::InvalidArgument, *why);
            for (const CurvePath& hole : face.holes)
                if (auto why = unfit(hole, true)) return err(ErrorCode::InvalidArgument, *why);
        }
    if (a.empty()) return std::vector<KernelFace>{};

    const Frame f{origin_of(a, b)};
    try {
        // EVERY FACE ITS OWN ARGUMENT, so faces of one set that overlap are
        // cut against each other too: a compound argument is taken whole.
        TopTools_ListOfShape arguments;
        TopTools_ListOfShape tools;
        for (const KernelFace& face : a)
            arguments.Append(face_of(face, f));
        for (const KernelFace& face : b)
            tools.Append(face_of(face, f));
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
                return faces_of(alone.Shape(), f);
            }
            tools = arguments;
            tools.RemoveFirst();
            const TopoDS_Shape first = arguments.First();
            arguments.Clear();
            arguments.Append(first);
            if (op == BooleanOp::Difference) op = BooleanOp::Union;
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
        run->SetRunParallel(Standard_False);
        run->Build();
        if (!run->IsDone() || run->HasErrors())
            return err(ErrorCode::Internal,
                       "Geometri çekirdeği bu alan işlemini tamamlayamadı; sınırlardan biri "
                       "kendini kesiyor ya da açık olabilir.");

        // THE SEAMS THE OPERATION LEFT go: two collinear edges, or two arcs of
        // one circle, that it cut and put back side by side are one again.
        ShapeUpgrade_UnifySameDomain unify(run->Shape(), Standard_True, Standard_True,
                                           Standard_False);
        unify.Build();
        return faces_of(unify.Shape(), f);
    } catch (const Standard_Failure& e) {
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
    try {
        BRepOffsetAPI_MakeOffset make;
        if (path.closed) {
            // A CLOSED PATH AS THE FACE IT BOUNDS, so a positive distance is
            // outward whichever way the path was walked.
            const TopoDS_Face face = face_of(KernelFace{path, {}}, f);
            make.Init(face, join, Standard_False);
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

} // namespace kentos::core
