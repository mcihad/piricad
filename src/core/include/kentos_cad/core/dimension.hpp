// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: dimensions and leaders.
//
// A DIMENSION shows a measurement: two definition points, a dimension line, two
// extension lines, two arrowheads and the measured text. What is STORED is the
// definition — the points and the style figures — never the picture; the
// picture is rebuilt every time from integer differences, unit vectors and
// `arc_outline`, so it is the same on every machine (§7.3). Ring 0 is the text
// baseline (the caption lives in the text table, centred on it); ring 1 holds
// the definition points in the order the type says; the payload (model.md R9a)
// holds the type, the style figures in GROUND millimetres (already scaled from
// the paper figures a style prescribes) and the measured value.
//
// A LEADER is an arrowed line pointing at something; its text, when it has
// one, is a separate text entity, as it is in every CAD format.
#pragma once

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::core {

/// What a dimension measures, numbered as DXF group 70's low bits so a file
/// round-trips its type without a table.
enum class DimensionType : std::uint8_t {
    Linear  = 0, ///< the distance between two points along a fixed direction; defs: p1, p2, dimline
    Aligned = 1, ///< the distance between two points along their own line; defs: p1, p2, dimline
    Angular = 2, ///< the angle between two lines; defs: l1a, l1b, l2a, l2b, arc
    Diametric = 3, ///< a circle's diameter; defs: far, near
    Radial    = 4, ///< a circle's radius; defs: centre, on_circle
    Angular3P = 5, ///< the angle at a vertex between two points; defs: vertex, p1, p2, arc
    Ordinate  = 6, ///< a point's x or y from an origin; defs: origin, feature, leader_end

    /// ARCLENGTH — the length ALONG an arc, not the chord across it.
    ///
    /// A transition curve, a kerb return and a pipe bend are all dimensioned by
    /// the distance a wheel travels, and that is not the straight line between
    /// the ends: a 100 grad arc of radius 50 m is 78,540 m along and 70,711 m
    /// across. A drawing that printed the chord would have somebody order the
    /// wrong length of kerbstone.
    ///
    /// ADDED AT THE END, which is what makes it additive: a file written before
    /// this existed never contains the value, so an older drawing reads exactly
    /// as it did (model.md — a shape may gain a case, it may not change one).
    /// defs: centre, start, end, arc_point.
    ArcLength = 7,
};

/// Stable machine name, for a file, a message or a test.
const char* dimension_type_name(DimensionType t) noexcept;

/// How the ends of the dimension line are marked.
enum class ArrowStyle : std::uint8_t {
    Closed = 0, ///< a filled triangle
    Open   = 1, ///< two strokes
    Tick   = 2, ///< an oblique stroke, the architectural mark
};

/// How a dimension's tolerance is written beside its figure.
enum class DimTolerance : std::uint8_t {
    None      = 0, ///< no tolerance
    Symmetric = 1, ///< `12,50±0,05`: `tolerance_plus` either way
    Deviation = 2, ///< `12,50+0,05/-0,02`: up by `tolerance_plus`, down by `tolerance_minus`
    Limits    = 3, ///< `12,55/12,48`: the two limits written in place of the figure
};

/// The payload of a `core.dimension` slot.
///
/// WHAT IS MEASURED AND WHAT IS WRITTEN ARE KEPT APART (TODOS C-10). The
/// `measurement` is always the geometry's own figure; the unit, the precision,
/// the tolerance, the prefix and the suffix are how it is PRESENTED; and an
/// `override_text` with no `<>` in it is a figure somebody TYPED — which the
/// program never mistakes for, and never lets pass as, the measured one. A
/// `<>` in it stands for the measured figure, as in every CAD format.
struct DimensionDef
{
    DimensionType type{DimensionType::Aligned}; ///< what is measured
    ArrowStyle arrow{ArrowStyle::Closed};       ///< how the dimension line ends
    bool user_text_position{false}; ///< the text was placed by hand, not centred on the line
    bool ordinate_x{false};         ///< an ordinate dimension measures x (else y)
    std::int64_t rotation_udeg{0};  ///< a linear dimension's direction
    std::int64_t measurement{0};    ///< what it says: millimetres, or micro-degrees for an angle
    Mm arrow_size{2500};            ///< arrowhead length, ground millimetres
    Mm extension_beyond{1250};      ///< how far an extension line passes the dimension line
    Mm extension_offset{625};       ///< the gap between a definition point and its extension line
    Mm text_gap{625};               ///< between the dimension line and the text
    std::uint8_t precision{2};      ///< decimals in the text
    char decimal_separator{','};    ///< `,` in Turkish, `.` elsewhere
    std::string style{"ISO-25"};    ///< the style the figures came from, at most 255 bytes
    std::string override_text;      ///< text the user typed instead of the measurement, or empty

    // ---- layout 2: written only when one of these is set ------------------
    std::string prefix; ///< before the figure (`R`, `Ø`, `≈`), at most 255 bytes
    std::string suffix; ///< after it (` m`, ` (eski)`), at most 255 bytes
    DimTolerance tolerance{DimTolerance::None}; ///< how the tolerance is written
    std::int64_t tolerance_plus{0}; ///< the upper deviation, or the ± value: mm, or µ° for an angle
    std::int64_t tolerance_minus{0}; ///< the lower deviation, a magnitude: mm, or µ° for an angle
    std::uint8_t unit{
        0}; ///< the unit a length is written in: 0 the drawing's, else DrawingUnit + 1

    /// THE SHEET SCALE THE GROUND SIZES WERE LAID OUT FOR, as its denominator
    /// (`1000` for 1/1000); 0 when not known (a file written before, a DXF).
    /// A style prescribes PAPER sizes — a 2,5 mm arrow — and this is what lets
    /// `ÖLÇÜYENİLE` keep them 2,5 mm on another sheet.
    std::int64_t scale_basis{0};

    friend bool operator==(const DimensionDef&, const DimensionDef&) = default;
};

/// The payload layout version `encode_dimension` writes for a dimension with
/// none of the layout-2 fields set: every such dimension keeps the bytes, and
/// every drawing of them its fingerprint, that it had before those fields.
inline constexpr std::uint16_t kDimensionLayout = 1;

/// The layout with the presentation fields and the sheet scale appended.
inline constexpr std::uint16_t kDimensionLayout2 = 2;

/// What ÖLÇÜ derives from the user's picks: the definition points in the order
/// the kind reads them, and where the caption is centred and which way it reads.
struct DimensionLayout
{
    std::vector<Point2> defs; ///< ring 1 of the entity
    Point2 text_centre{};     ///< the caption's centre
    double text_dir_x{1.0};   ///< the caption's reading direction, unit length: x
    double text_dir_y{0.0};   ///< and y
};

/// Lays a dimension out from what was picked — `picks` is `{p1, p2}` for a
/// linear, aligned, radial or diametric dimension and `{p1, p2, apex}` for a
/// three-point angular one; `where` is the dimension line's location, or the
/// caption's place for a radial or diametric one. Sets `def.rotation_udeg` for a
/// linear dimension (horizontal when the line lies above or below the points,
/// vertical beside them) and `def.measurement`. False when the picks cannot
/// make one: coincident points, an apex on a pick, or a type this cannot lay
/// out (ordinate, four-point angular). ONE function, so the command that
/// creates a dimension, the grip that moves its point and the preview under
/// the cursor agree about where its line goes.
bool dimension_layout(DimensionDef& def, std::span<const Point2> picks, Point2 where,
                      Mm text_height, DimensionLayout& out);

/// The inverse of `dimension_layout` for a stored dimension: the picks and the
/// location its definition points (ring 1) and caption baseline (ring 0) came
/// from. False for a type `dimension_layout` cannot lay out.
bool dimension_picks(DimensionType type, std::span<const Point2> defs, Point2 baseline_start,
                     std::vector<Point2>& picks, Point2& where);

/// A caption baseline from its centre along `(dx, dy)`, long enough for the
/// text the way METİN measures one, readable left to right: the two vertices
/// of ring 0. The caption is anchored MiddleCentre at the first.
std::array<Point2, 2> dimension_baseline(Point2 centre, double dx, double dy, Mm height,
                                         std::string_view text);

/// The document a dimension lives in; `dimension_rebuild` reads its geometry.
class Document;

/// A dimension rebuilt around moved definition points: its two rings, its
/// payload and its caption.
struct DimensionRebuild
{
    std::array<Point2, 2> baseline{};  ///< ring 0, the caption baseline
    std::vector<Point2> defs;          ///< ring 1, the definition points
    std::vector<std::uint8_t> payload; ///< the dimension's payload, re-measured
    std::string text;                  ///< the caption, re-worded (the user's own text is kept)
    Mm text_height{0};                 ///< the caption's height
    DimensionDef def;                  ///< the payload, decoded: what `payload` holds
};

/// What to rebuild a stored dimension with (`dimension_rebuild`): every field
/// left at its default keeps what the dimension has.
struct DimensionEdit
{
    const DimensionDef* def{nullptr}; ///< the payload to lay out, or null for the stored one
    std::span<const std::pair<std::size_t, Point2>> moves{}; ///< definition points moved
    Mm text_height{0};              ///< the caption's height, or 0 for the stored one
    const Point2* caption{nullptr}; ///< a caption placed by hand at this point, or null
};

/// Rebuilds dimension `e` as `edit` says, laid out as ÖLÇÜ lays one out: moved
/// definition points (the others stay), a new payload (a prefix, a tolerance,
/// a unit, a text typed over the figure, figures rescaled for another sheet), a
/// new caption height, a caption placed by hand. The figure is re-measured from
/// the points and re-worded in `unit`; a caption placed by hand
/// (`user_text_position`) keeps its place, carried by the moved points' mean
/// displacement. What ÖLÇÜDÜZENLE, ÖLÇÜYENİLE, YAZIDÜZENLE on a dimension and a
/// linked dimension's commit-time update all build with.
Result<DimensionRebuild> dimension_rebuild(const Document& doc, EntityId e,
                                           const DimensionEdit& edit, DrawingUnit unit);

/// Rebuilds dimension `e` with the definition points `moves` names at their new
/// places and the others where they are, laid out as ÖLÇÜ lays one out: the
/// dimension line and the caption carried by the moved points' mean
/// displacement (an aligned one keeps its offset from the side it measures and
/// turns with it), the figure re-measured and re-worded in `unit` unless the
/// caption is the user's own text. A type the layout cannot draw (an ordinate,
/// a four-point angular one from a file) moves its points, is re-measured and
/// keeps its caption's reading. What a linked dimension does at commit when
/// what it measures moved (core/dimension_link.hpp).
Result<DimensionRebuild> dimension_follow(const Document& doc, EntityId e,
                                          std::span<const std::pair<std::size_t, Point2>> moves,
                                          DrawingUnit unit);

/// The payload bytes.
std::vector<std::uint8_t> encode_dimension(const DimensionDef& def);

/// The payload back, refused when it is not what `encode_dimension` writes.
Result<DimensionDef> decode_dimension(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<DimensionDef> dimension_of(const RingGeometry& geom, std::uint32_t slot);

/// How many definition points a type wants in ring 1.
std::size_t dimension_point_count(DimensionType t) noexcept;

/// The value the definition points measure: a length in millimetres, or an
/// angle in micro-degrees for the angular types. From integer differences and
/// `atan2_udeg`, never libm (§7.3).
/// `ordinate_x` reads an ordinate's axis: true for the easting, false for the
/// northing. Ignored by every other type.
std::int64_t dimension_measure(DimensionType t, std::span<const Point2> defs,
                               std::int64_t rotation_udeg, bool ordinate_x = false) noexcept;

/// The unit a dimension writes a length in: its own, or the drawing's `unit`.
DrawingUnit dimension_unit(const DimensionDef& def, DrawingUnit unit) noexcept;

/// The MEASURED figure alone, whatever the caption says: `12500` mm in metres
/// with 2 decimals and `,` is `12,50`; an angle of 90 000 000 µ° with 2
/// decimals is `90,00°`. Integer arithmetic only, so the string is the same on
/// every platform.
std::string dimension_value_text(const DimensionDef& def, DrawingUnit unit);

/// Whether the caption is a figure somebody typed rather than the measured one:
/// an `override_text` with no `<>` in it.
bool dimension_text_is_manual(const DimensionDef& def) noexcept;

/// The caption as written: the prefix, the measured figure (or the two limits),
/// the tolerance and the suffix — put where the `override_text` has `<>`, or
/// the `override_text` itself when it has none.
std::string dimension_text(const DimensionDef& def, DrawingUnit unit);

/// The tolerance as written after the figure — `±0,05`, `+0,05/-0,02` — or an
/// empty string for none and for limits (which replace the figure instead).
std::string dimension_tolerance_text(const DimensionDef& def, DrawingUnit unit);

/// Formats a length in `unit` with `precision` decimals and `separator`.
std::string format_dimension_length(Mm value, DrawingUnit unit, unsigned precision, char separator);

/// Formats an angle in degrees with `precision` decimals and `separator`, with
/// the degree sign.
std::string format_dimension_angle(std::int64_t udeg, unsigned precision, char separator);

/// Appends the DRAWN form of a dimension: extension lines, the dimension line
/// (or arc), the arrowheads. The text is the entity's caption on ring 0 and is
/// not emitted here.
void dimension_outline(const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into);

/// The payload of a `core.leader` slot.
struct LeaderDef
{
    bool arrow{true};   ///< an arrowhead at the first vertex
    bool spline{false}; ///< the source drew it as a spline; kept for the round trip, drawn straight
    Mm arrow_size{2500}; ///< arrowhead length, ground millimetres

    friend constexpr bool operator==(const LeaderDef&, const LeaderDef&) noexcept = default;
};

/// The payload layout version `encode_leader` writes.
inline constexpr std::uint16_t kLeaderLayout = 1;

/// The payload bytes.
std::vector<std::uint8_t> encode_leader(const LeaderDef& def);

/// The payload back, refused when it is not what `encode_leader` writes.
Result<LeaderDef> decode_leader(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<LeaderDef> leader_of(const RingGeometry& geom, std::uint32_t slot);

/// Appends the DRAWN form of a leader: its vertices and the arrowhead.
void leader_outline(const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into);

/// Appends an arrowhead of `size` with its tip at `tip`, pointing along the
/// direction from `from` to `tip`: a closed triangle for `Closed`, two strokes
/// for `Open`, one oblique stroke for `Tick`. Shared by dimensions and leaders.
void arrowhead_outline(Point2 tip, Point2 from, Mm size, ArrowStyle style, EmitBuffer& into);

} // namespace kentos::core
