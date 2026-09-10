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
};

/// Stable machine name, for a file, a message or a test.
const char* dimension_type_name(DimensionType t) noexcept;

/// How the ends of the dimension line are marked.
enum class ArrowStyle : std::uint8_t {
    Closed = 0, ///< a filled triangle
    Open   = 1, ///< two strokes
    Tick   = 2, ///< an oblique stroke, the architectural mark
};

/// The payload of a `core.dimension` slot.
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

    friend bool operator==(const DimensionDef&, const DimensionDef&) = default;
};

/// The payload layout version `encode_dimension` writes.
inline constexpr std::uint16_t kDimensionLayout = 1;

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
std::int64_t dimension_measure(DimensionType t, std::span<const Point2> defs,
                               std::int64_t rotation_udeg) noexcept;

/// The measured text: `12500` mm in metres with 2 decimals and `,` is `12,50`;
/// an angle of 90 000 000 µ° with 2 decimals is `90,00°`. Integer arithmetic
/// only, so the string is the same on every platform.
std::string dimension_text(const DimensionDef& def, DrawingUnit unit);

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
