// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: linework as a planar network, and the faces it encloses.
//
// ONE ANSWER TO "WHICH GROUND DOES THIS LINEWORK CLOSE?" (TODOS C-09). A click
// inside a parcel drawn as loose lines, a network of boundary lines turned into
// parcels, a topology check looking for the end that does not meet: all three
// ask the same question of the same drawing, and they must not disagree about
// where a face is, which island is a hole in it, or which end is open.
//
// THE ARRANGEMENT IS CGAL'S (Article 2.7, kentoscad.md §9.2 names it for
// exactly this). Noding a set of segments and arcs — every crossing, every
// T-junction, every overlap — and walking the faces that result, with the
// islands inside each face as its holes, is the textbook case of a problem with
// well-known degeneracies and a mature exact answer. `Arrangement_2` over the
// circle-segment traits is that answer: exact arithmetic, so a crossing of two
// nearly parallel lines is found where it is and not where a rounding put it,
// and arcs stay arcs. `curve_path.hpp` weighed CGAL for three closed forms and
// declined it; an arrangement is one of CGAL's real strengths, which is where
// that header said a library earns its place. The library lives in
// `planar.cpp` alone; nothing outside that translation unit sees it.
//
// THE TOLERANCE IS THE PROJECT'S, AND IT IS THE ONLY ONE. Two ends within the
// node tolerance (`core.topoloji.dugum_toleransi`) are one node, and an end
// within it of another line's interior is ON that line — the same rule ALANAÇEVİR
// and İFRAZ read. Nothing farther apart is ever joined silently: an end that
// meets nothing is reported as OPEN, with the nearest linework and how far away
// it is, so the user sees the gap instead of a boundary that quietly closed
// across it. Bridging a gap is a separate, explicit request (`bridge`), and
// every bridge it lays is recorded and reported.
//
// INTEGER OUT. Every vertex of a face is rounded to the millimetre once, at the
// end, from the exact value; two faces that share an edge share its rounded
// vertices, so a boundary between two parcels stays one boundary (Article 2.4,
// §7.3).
#pragma once

#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// Whether this build can build networks at all: false when it was configured
/// without CGAL (`KENTOS_WITH_CGAL=OFF`), in which case `Network::build` says so.
bool network_available() noexcept;

/// One piece of linework and where it came from.
struct NetworkPiece
{
    PathPiece piece;         ///< a segment, an arc, or a whole circle (sweep ±360°)
    std::uint32_t source{0}; ///< the caller's index: an entity slot, a bridge number
    /// Drawn, not defined: a chord of a curve that is not a path — an ellipse, a
    /// spline — standing in for it. A face bounded by one says so.
    bool approximate{false};
};

/// One closed ring of a face: the pieces in order, and who drew each.
struct FaceRing
{
    CurvePath path;                                  ///< closed; arcs stay arcs
    std::vector<std::vector<std::uint32_t>> sources; ///< per piece, the sources that drew it
    bool approximate{false};                         ///< some piece is a chord of a curve
    std::vector<std::size_t> bridges;                ///< the bridges (`Network::bridges`) in it
};

/// One face of the network: its outer ring and the islands inside it.
struct NetworkFace
{
    FaceRing outer;              ///< counter-clockwise
    std::vector<FaceRing> holes; ///< clockwise; empty when islands were not asked for
    Mm2 area{0};                 ///< the outer ring's area less its holes'
    Mm2 outer_area{0};           ///< the outer ring's own area
};

/// An end of linework that meets nothing, and the nearest linework to it.
struct OpenEnd
{
    Point2 at{};             ///< the end itself
    std::uint32_t source{0}; ///< the source whose piece ends here
    bool has_nearest{false}; ///< false when there is no other linework at all
    Point2 nearest{};        ///< the closest point of any other linework
    Mm distance{0};          ///< how far that is: the gap a bridge would close
};

/// What the node tolerance did before the network was built.
struct NodeSnaps
{
    std::size_t moved{0}; ///< ends moved onto a node or onto a line
    Mm largest{0};        ///< the farthest any of them moved
};

/// A gap that was bridged because the caller asked: an open end and the
/// linework it was joined to.
struct Bridge
{
    Point2 from{}; ///< the open end
    Point2 to{};   ///< where on the nearest linework it was joined
    Mm width{0};   ///< how wide the gap was
};

/// Linework noded into a planar network: every crossing found, every face known.
class Network
{
public:
    /// Builds the network of `pieces`. Ends within `node_tolerance` of each
    /// other become one node; an end within it of another piece's interior
    /// splits that piece and meets it. With `bridge` above zero, every open end
    /// whose nearest linework is at most that far is joined to it by a segment
    /// (`bridges`). Refused when the build has no CGAL.
    static Result<Network> build(std::span<const NetworkPiece> pieces, Mm node_tolerance,
                                 Mm bridge = 0);

    Network(Network&&) noexcept;
    Network& operator=(Network&&) noexcept;
    Network(const Network&)            = delete;
    Network& operator=(const Network&) = delete;
    ~Network();

    /// The bounded face `p` lies in, with its islands as holes when `islands`;
    /// nothing when `p` is outside every face or exactly on the linework.
    std::optional<NetworkFace> face_at(Point2 p, bool islands) const;

    /// Whether `p` lies exactly on the linework — a click on a line, not in a
    /// face.
    bool on_linework(Point2 p) const;

    /// Every bounded face, islands as holes when `islands`; faces whose rounded
    /// rings collapse to nothing are left out and counted in `collapsed()`.
    std::vector<NetworkFace> faces(bool islands) const;

    /// How many faces the last `faces` or `face_at` call left out because their
    /// rings collapsed when rounded to the millimetre.
    std::size_t collapsed() const noexcept;

    /// Every end that meets nothing, with its nearest other linework, ordered by
    /// position so two runs list them alike.
    std::vector<OpenEnd> open_ends() const;

    /// Groups of sources that drew the SAME stretch of linework: an edge two
    /// pieces lie on, top of one another.
    std::vector<std::vector<std::uint32_t>> overlaps() const;

    /// The bridges `build` laid, in the order it laid them.
    const std::vector<Bridge>& bridges() const noexcept;

    /// What the node tolerance moved.
    NodeSnaps snaps() const noexcept;

private:
    struct Impl;
    explicit Network(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

/// One stored ring as a snapshot holds it: its vertices and its role.
struct StoredRing
{
    std::span<const Point2> points; ///< the vertices, no repeated closing point
    RingRole role{RingRole::Open};  ///< open, exterior or interior
};

/// The pieces a stored object contributes to a network, from its kind, rings
/// and payload alone — the answer `network_pieces` gives for a live entity, for
/// a snapshot that has no document (a processing tool, R4). `drawn` is the
/// object's drawn outline, read only for the kinds that are not paths yet: an
/// ellipse and a spline, whose pieces are its chords, marked approximate.
std::vector<NetworkPiece> record_pieces(KindId kind, std::span<const StoredRing> rings,
                                        std::span<const std::uint8_t> payload,
                                        std::span<const StoredRing> drawn, std::uint32_t source);

/// The pieces entity `e` contributes to a network: every ring of a polyline or
/// a face, an arc, a circle, an arc-polyline with its arcs; an ellipse or a
/// spline as the chords it is drawn with, marked approximate. Nothing for a kind
/// that bounds no ground — a caption, a dimension, a hatch, a block, a point —
/// and nothing for a member of a block definition. `source` is written into
/// every piece.
std::vector<NetworkPiece> network_pieces(const Document& doc, EntityId e, std::uint32_t source);

/// How a face is stored. One record of the kind that holds it exactly — an
/// arc-polyline for a face bounded by arcs, a circle for a whole circle — or
/// rings for an area. A face with arcs AND holes, which no single kind holds,
/// is rings with its arcs as the chords `arc_outline` draws, and `chords` says
/// how far those chords stray from the arcs. One rule for every caller, so SINIR
/// and ALANÜRET cannot write the same face two ways.
struct FaceShape
{
    bool whole{false};                      ///< a record (`record`), not rings
    PathRecord record;                      ///< the record, when `whole`
    std::vector<std::vector<Point2>> rings; ///< otherwise the exterior, then the holes
    Mm chords{0};                           ///< how far any chord strays from its arc
};

/// The shape `face` is stored as.
FaceShape face_shape(const NetworkFace& face);

// The signed area of a face's path is `path_area` (curve_path.hpp): the ONE area
// rule for a curve, arcs and all (TODOS F-03).

/// What a click inside a region asks.
struct RegionQuery
{
    Point2 at{};                ///< the point inside the region
    bool islands{true};         ///< closed linework inside the region becomes its holes
    Mm node_tolerance{10};      ///< the project's node tolerance
    Mm bridge{0};               ///< bridge open ends up to this far apart; 0 = never
    std::vector<EntityId> only; ///< the boundary set; empty = every visible entity
    /// How far out from the point linework is gathered; 0 = the whole drawing.
    /// A live preview bounds it by what is on screen; a command never does.
    Mm max_reach{0};
};

/// What a click inside a region found.
struct Region
{
    std::optional<NetworkFace> face; ///< the region, when the point is inside one
    bool on_linework{false};         ///< the point is on a line, not inside anything
    std::vector<OpenEnd> open;       ///< the open ends near it, when it did not close
    std::vector<Bridge> bridges;     ///< the gaps `bridge` closed to find it
    NodeSnaps snaps;                 ///< what the node tolerance joined
    std::vector<EntityId> sources;   ///< the entity slots whose linework bounds the face
    bool approximate{false};         ///< an ellipse or a spline bounds it, as drawn
    Mm deviation{0};                 ///< how far those drawings are from their curves
};

/// What SINIR's live preview needs to find the region a click would find: the
/// same query, less the point, which is the cursor.
struct RegionPreview
{
    bool islands{true};             ///< holes for the islands
    Mm node_tolerance{10};          ///< the project's node tolerance
    Mm bridge{0};                   ///< the gap the command would bridge
    std::vector<std::int64_t> keys; ///< the boundary set, by persistent key; empty = all visible
};

/// The preview as bytes.
std::vector<std::uint8_t> encode_region_preview(const RegionPreview& preview);

/// The preview back, refused when the bytes are not what the encoder writes.
Result<RegionPreview> decode_region_preview(std::span<const std::uint8_t> bytes);

/// The region around `query.at`: the smallest face of the visible linework that
/// contains it, islands inside it as holes. The linework is gathered outward
/// from the point until the face found no longer reaches the edge of what was
/// gathered — so the answer depends on the drawing alone, never on the view.
Result<Region> region_at(const Document& doc, const RegionQuery& query);

} // namespace kentos::core
