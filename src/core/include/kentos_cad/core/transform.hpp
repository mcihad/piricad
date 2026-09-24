// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: moving, turning, mirroring and scaling coordinates.
//
// Every function here is DETERMINISTIC ACROSS PLATFORMS, and that is a
// requirement rather than a nicety: a rotated parcel's corners are stored
// coordinates, and §7.3 promises the same drawing comes out bit-identical on
// Linux, Windows and macOS. A transform that disagreed in the last bit would
// produce a different tapu on a different machine.
//
// That is why `sin_cos_udeg` exists instead of a call to libm. `std::sin` and
// `std::cos` are NOT correctly rounded and are not required to agree between
// platforms or even between versions of the same platform's libm. The
// implementation here uses argument reduction that is exact (a quadrant is an
// integer count of 90°) followed by a polynomial in +, * and / only — every one
// of which IEEE-754 pins exactly.
#pragma once

#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace kentos::core {

/// Angles are whole MICRO-DEGREES — `kUDegPerDegree` in units.hpp, the unit the
/// polar snap already uses. An integer angle keeps a right angle exactly a right
/// angle: 90° is 90 000 000, not a float that is nearly it.
using UDeg = std::int64_t;

// `SinCos` and `sin_cos_udeg` live in core/trig.hpp: ONE trigonometry, shared
// with the snap engine, the arc's perimeter and every kind that turns a point.
// This header once carried a second copy with a different series, and two
// deterministic functions that disagree in the last bit are one bug more than
// none.

/// `p` moved by `dx`, `dy`. Exact: integers added to integers.
constexpr Point2 translated(Point2 p, Mm dx, Mm dy) noexcept
{
    return Point2{p.x + dx, p.y + dy};
}

/// `p` turned about `base` by the angle whose sine and cosine are `t`.
///
/// The offset from the base is taken FIRST and the rotation applied to it, so the
/// magnitudes multiplied are the size of the object rather than the size of a
/// TUREF coordinate — the same reason every distance in this program is measured
/// after a translation to a local origin (core.md R3).
Point2 rotated_about(Point2 p, Point2 base, SinCos t);

/// `p` scaled about `base` by `factor`, which must be greater than zero.
///
/// A negative factor is refused by the commands rather than silently turning into
/// a rotation by half a turn: "scale by minus one" and "mirror" are different
/// intentions, and a user who typed the wrong sign should be told.
Point2 scaled_about(Point2 p, Point2 base, double factor);

/// `p` reflected in the line through `a` and `b`.
///
/// Exact when the axis is horizontal or vertical — which is the axis a surveyor
/// picks most of the time — because those two cases are integer negation and are
/// taken before any arithmetic that could round.
Point2 mirrored_in_line(Point2 p, Point2 a, Point2 b);

// ---------------------------------------------------------------------------
// One transform, described rather than applied, and the ghost that previews it.
//
// WHY THE DESCRIPTION AND NOT A `Point2 -> Point2`. A curve has to ask questions
// a bare map cannot answer: how its radius changes, and whether its sweep is
// reversed. So the edit verbs carry a DESCRIPTION of what they are about to do
// and hand it to whoever needs it.
//
// WHY IT IS IN `core`. Two callers need it: the verb, which rewrites the
// document, and the canvas, which draws the ghost that promises what the click
// will do. The ghost used to be a TRANSLATION whatever the verb was — so
// DÖNDÜR, ÖLÇEKLE and AYNALA either had none at all or would have shown the
// objects sliding sideways while the command turned them. A preview that
// promises the wrong transform is worse than no preview: the user aims with it.
// Now both sides call `transformed`, and the ghost cannot disagree with the
// result because it is the same function.
// ---------------------------------------------------------------------------

/// What is being done to the coordinates.
struct Xform
{
    /// Which transform this is. `Align` is the one HİZALA applies: turned and
    /// scaled about `base`, then carried so `base` lands on `axis_b` — and, with
    /// `flip`, reflected first. `Stretch` is a scale about `base` by `factor`
    /// across and `factor_y` up: ÖLÇEKLE with two factors (TODOS C-08). `Place`
    /// is what a block reference does to its definition's members, one copy of
    /// its grid: scaled about `base` by the exact ratios `place_sx`/`place_sy`
    /// (negative mirrors), stepped by (`dx`, `dy`), turned by `place_udeg` and
    /// set down on `axis_b` — `place_block_point`'s arithmetic, so a member
    /// PATLAT takes out lands on the millimetre the reference drew it on (TODOS
    /// C-13). Made by `block_placement`.
    enum class Kind : std::uint8_t { Translate, Rotate, Scale, Mirror, Align, Stretch, Place };

    Kind kind{Kind::Translate}; ///< which transform
    Mm dx{0};                   ///< Translate: east component · Place: the grid step across
    Mm dy{0};                   ///< Translate: north component · Place: the grid step up
    Point2 base{};   ///< Rotate/Scale: the centre · Mirror: the axis's first point · Align: the
                     ///< source · Place: the definition's base point
    Point2 axis_b{}; ///< Mirror: the axis's second point · Align: where the source goes · Place:
                     ///< the insertion point
    SinCos turn{};   ///< Rotate, Align: the turn
    double factor{1.0};         ///< Scale, Align: the multiplier · Stretch: the one across (east)
    double factor_y{1.0};       ///< Stretch: the multiplier up (north)
    bool flip{false};           ///< Align: reflected in the line through `base` along east, first
    Ratio place_sx{1, 1};       ///< Place: the scale along the definition's x; negative mirrors
    Ratio place_sy{1, 1};       ///< Place: the scale along its y; negative mirrors
    std::int64_t place_udeg{0}; ///< Place: the turn, micro-degrees counter-clockwise

    friend bool operator==(const Xform&, const Xform&) = default;
};

/// `p` under `x`.
Point2 transformed(const Xform& x, Point2 p);

/// The transform that places copy (`column`, `row`) of `ref`'s grid, the
/// reference standing at `insertion` over a definition whose base is `base`:
/// what `place_block_point` does to one point, as an `Xform` every kind knows
/// how to follow.
Xform block_placement(const BlockReference& ref, Point2 insertion, Point2 base, int column,
                      int row) noexcept;

/// Whether a `Place` keeps shapes — the same magnitude across and up, so a
/// circle stays a circle and a caption its proportions. Every other kind
/// answers for itself (`Stretch` by its two factors).
bool place_uniform(const Xform& x) noexcept;

/// `v` scaled by a uniform `Place`'s magnitude, exactly: `mul_div_round` over
/// the ratio, so a radius, a text height and a spacing agree with the vertices
/// placed beside them. `v` unchanged for any other transform.
Mm place_length(const Xform& x, Mm v) noexcept;

/// How the cursor completes a ghost: which transform is being previewed.
enum class GhostKind : std::uint8_t {
    Translate, ///< the cursor is where the base point goes: TAŞI, KOPYALA
    Rotate,    ///< the cursor's direction from the base is the turn: DÖNDÜR
    Scale,     ///< the cursor's distance from the base, in METRES, is the factor: ÖLÇEKLE
    Mirror,    ///< the cursor is the axis's second point: AYNALA
    Align      ///< the cursor is where the second source goes: HİZALA (`GhostSpec::from1`…)
};

/// What a ghost needs beyond the points it is handed.
struct GhostSpec
{
    /// Which transform the cursor is completing.
    GhostKind kind{GhostKind::Translate};

    /// How many copies the ghost draws, the first being the original moved once.
    /// More than one only for a command that repeats a step.
    std::int64_t copies{1};

    /// The objects the ghost carries, by persistent key. Empty is the live
    /// selection, which is what a verb started from the canvas has in hand; a
    /// verb told its objects by name (`TAŞI nesneler=5`) names them here, so the
    /// ghost is of what will move rather than of whatever is highlighted.
    std::vector<std::int64_t> keys;

    /// `Align` only: the first source, where it goes, and the second source —
    /// the cursor being where the second source goes — and whether the second
    /// pair's distance scales the objects (`HİZALA olcekle=evet`).
    Point2 from1{};
    Point2 to1{};
    Point2 from2{};
    bool scale{false};

    /// `Rotate` BY REFERENCE: the direction that turns onto the cursor's, in
    /// micro-degrees — the turn drawn is the cursor's direction less this
    /// (DÖNDÜR yontem=referans, TODOS C-08). Zero is the plain turn.
    std::int64_t reference_udeg{0};

    /// `Scale` BY REFERENCE: the length that becomes the cursor's distance from
    /// the base, in millimetres — the factor drawn is that distance over this
    /// (ÖLÇEKLE yontem=referans). Zero is the plain factor, metres out.
    Mm reference_length{0};

    friend bool operator==(const GhostSpec&, const GhostSpec&) = default;
};

/// The ghost as bytes: kind (uint8), copies (int64), the three `Align` points
/// (six int64), the scale flag (uint8), the reference turn and length (two
/// int64), the key count (uint32) and the keys.
/// Unversioned because a ghost lives for the length of one prompt and is never
/// written to a file.
std::vector<std::uint8_t> encode_ghost_spec(const GhostSpec& spec);
std::optional<GhostSpec> decode_ghost_spec(std::span<const std::uint8_t> bytes);

/// The transform HİZALA applies: `from1` carried to `to1`, turned so `from1`→`from2` lies
/// along `to1`→`to2`, and — with `scale` — stretched by how much longer the
/// second pair is. Nothing when either pair's two points coincide, which leaves
/// no direction to turn to and no length to scale by.
std::optional<Xform> align_xform(Point2 from1, Point2 to1, Point2 from2, Point2 to2, bool scale,
                                 bool flip = false);

/// Whether `x` turns the plane inside out — a mirror, or an alignment that
/// reflects — so a counter-clockwise sweep comes out clockwise.
bool xform_reverses(const Xform& x) noexcept;

/// One object's record carried by a translation: its rings, and its payload
/// with every coordinate the payload holds moved too.
struct TranslatedRecord
{
    std::vector<std::vector<Point2>> rings; ///< one vector per ring, R11 order
    std::vector<RingRole> roles;            ///< each ring's role
    std::vector<std::uint16_t> parts;       ///< each ring's part
    std::vector<std::uint8_t> payload;      ///< the kind's payload, its coordinates moved

    /// The rings as `set_kind_geometry` takes them; the spans borrow from
    /// `rings`, so the record must outlive the call.
    std::vector<RingGeometry::RingInput> inputs() const;
};

/// `e` carried by (`dx`, `dy`) — the record a paste writes (TODOS C-08). The
/// rings alone are not the whole object: an arc polyline's arc centres, a
/// block reference's drawn box and a hatch's pattern origin live in the
/// payload, and a paste that moved only the vertices left an arc bending
/// round its old centre and a symbol's box where it used to be.
Result<TranslatedRecord> translated_record(const Document& doc, EntityId e, Mm dx, Mm dy);

/// An ellipse — or an elliptic arc — by its centre, its two axis ends and its
/// sweep in the ellipse's own parameter (core/ellipse.hpp).
struct EllipseImage
{
    Point2 centre{};            ///< the centre
    Point2 major{};             ///< the end of the first axis
    Point2 minor{};             ///< the end of the second, perpendicular and counter-clockwise
    bool partial{false};        ///< an elliptic arc rather than a whole ellipse
    std::int64_t start_udeg{0}; ///< the arc's start parameter
    std::int64_t end_udeg{0};   ///< its end parameter, counter-clockwise from the start
};

/// What `x` makes of an ellipse: its axes PERPENDICULAR again, the second
/// counter-clockwise of the first, and the sweep re-read in the new axes'
/// parameter. Under a scale that differs across and up the images of two
/// perpendicular axes are conjugate but no longer perpendicular — a record kept
/// that way would draw right and be written wrong to a DXF ELLIPSE, which reads
/// the second axis as the first turned a quarter — so the principal axes are
/// found again, the longer first. Every other transform keeps the axes it was
/// given, turned the right way round after a reflection (TODOS C-08).
EllipseImage transformed_ellipse(const Xform& x, const EllipseImage& e);

/// The transform `cursor` implies for a ghost whose base point is `base`.
///
/// THE ONE PLACE THE CURSOR BECOMES A TRANSFORM, called by the verb when it
/// takes the pointed answer and by the canvas on every mouse move. That is what
/// makes the ghost exact rather than nearly right.
Xform ghost_xform(GhostKind kind, Point2 base, Point2 cursor);

/// The same, for a ghost whose spec carries more than a kind: an `Align`
/// ghost's fixed points (the cursor is the second target), every other kind as
/// the call above. An `Align` whose pairs collapse moves nothing.
Xform ghost_xform(const GhostSpec& spec, Point2 base, Point2 cursor);

/// The turn `base`->`cursor` makes, in whole micro-degrees counter-clockwise
/// from east — the unit `DÖNDÜR`'s own `aci` is written in, times
/// `kUDegPerDegree`. Split out because the verb records the ANGLE while the
/// ghost needs the `SinCos`, and both have to come from the same integer.
UDeg ghost_turn_udeg(Point2 base, Point2 cursor) noexcept;

/// The factor `base`->`cursor` implies: the distance between them in METRES,
/// which is how every CAD reads a dragged scale. Never negative; zero when the
/// cursor is on the base point, which the verb refuses.
double ghost_factor(Point2 base, Point2 cursor) noexcept;

} // namespace kentos::core
