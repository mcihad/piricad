// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): what the DXF reader and writer share.
//
// The AutoCAD colour index, the lineweight table, the XDATA byte codec, the MTEXT
// formatting codes, the arithmetic that turns a bulge into an arc and a spline
// into points — each is needed on the way in and on the way out, and two copies
// of a table are two tables that drift apart. Not a public header (io.md R1/R2):
// the DRW_* types appear here, and only dxf_reader.cpp and dxf_writer.cpp
// include it.
#pragma once

#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef KENTOS_HAVE_DXFRW
#include <drw_base.h>
#endif

namespace kentos::io::dxf {

// ------------------------------------------------------------------ colour ----

/// The RGB (0xRRGGBB) of AutoCAD Color Index `index` (1..255), or black for 0
/// and 256 — the ByBlock and ByLayer sentinels, which a caller resolves before
/// asking. The 1–9 primaries, the 250–255 greys and the 240-entry hue table are
/// the ones every DXF consumer agrees on.
std::uint32_t aci_rgb(int index) noexcept;

/// The index (1..255) whose colour is nearest `rgb` (0xRRGGBB), by squared
/// distance. What a DXF that carries no 24-bit colour gets.
int nearest_aci(std::uint32_t rgb) noexcept;

/// The ink an ACI index means on a drawing: index 7 is "white/black", the
/// colour that adapts to the background, and a plan is printed in black on
/// white — so 7 reads as black. Every other index is its palette entry.
std::uint32_t aci_ink(int index) noexcept;

/// The ACI index a drawing colour is written as: black goes as 7, the index
/// every reader shows in its background's opposite; everything else as the
/// nearest palette entry.
int aci_for_ink(std::uint32_t rgb) noexcept;

// -------------------------------------------------------------- lineweight ----

/// Paper micrometres for a DXF lineweight code (group 370: hundredths of a
/// millimetre, 0..211). Negative codes — ByLayer, ByBlock, Default — are 0 here;
/// the caller keeps the Source.
std::int32_t lineweight_um_from_dxf(int code370) noexcept;

/// The nearest legal DXF lineweight code for a width in paper micrometres.
int dxf_lineweight_from_um(std::int32_t um) noexcept;

// ---------------------------------------------------------------- angles ----

/// Whole micro-degrees, half away from zero, for an angle in radians or degrees.
std::int64_t udeg_from_radians(double radians) noexcept;
std::int64_t udeg_from_degrees(double degrees) noexcept;
double radians_from_udeg(std::int64_t udeg) noexcept;
double degrees_from_udeg(std::int64_t udeg) noexcept;

/// The point `radius` from `centre` in the direction `udeg`, through
/// `core::sin_cos_udeg` so every platform lands on the same millimetre.
core::Point2 point_on_circle(core::Point2 centre, core::Mm radius, std::int64_t udeg) noexcept;

// -------------------------------------------------------------- geometry ----

/// The circle a polyline bulge describes between `a` and `b`. `bulge` is
/// tan(θ/4) of the included angle, positive counter-clockwise. False for a bulge
/// of zero or coincident ends. The arc runs from `a` to `b`; `ccw` says which way.
bool arc_from_bulge(core::Point2 a, core::Point2 b, double bulge, core::Point2& centre,
                    core::Mm& radius, bool& ccw) noexcept;

/// The points of an elliptic arc from `start_udeg` to `end_udeg` of the
/// parametric angle, counter-clockwise, `centre + cos·major + sin·minor` where
/// `major` and `minor` are the axis VECTORS. Deterministic (`sin_cos_udeg`).
/// Appends without the closing point; a full turn ends one step short.
void ellipse_arc_points(core::Point2 centre, core::Point2 major_vec, core::Point2 minor_vec,
                        std::int64_t start_udeg, std::int64_t end_udeg,
                        std::vector<core::Point2>& out);

/// A point in drawing units, before the millimetre conversion.
struct Pt
{
    double x{0.0};
    double y{0.0};
};

/// Points along a B-spline / NURBS by de Boor's algorithm — `samples` per knot
/// span, in fixed evaluation order with nothing but +, −, × and ÷, so the same
/// file gives the same points everywhere. Weights may be empty (non-rational).
/// False when the knot vector does not fit the degree and control count.
bool nurbs_points(int degree, const std::vector<double>& knots, const std::vector<Pt>& controls,
                  const std::vector<double>& weights, int samples, std::vector<Pt>& out);

// -------------------------------------------------------------- patterns ----
//
// A HATCH'S PATTERN TRAVELS AS THE LINES IT DRAWS, not as a name alone. A file
// that says "ANSI31" and no more leaves the spacing to whichever pattern file
// the reading program has, in whichever unit it thinks it is in — a metric
// drawing opened with the imperial acad.pat comes out 25,4 times too sparse,
// and a drawing in metres read as millimetres a thousand times too dense. So
// the lines go with it, as DXF lets them (group 78 and what follows), and the
// scale in group 41 is stated against the metric pattern file this program's
// catalogue is: its numbers, read as drawing units, times the scale are the
// spacing on the ground.

/// One line of a pattern as a DXF HATCH carries it after group 78: in drawing
/// units, turned and scaled — the line as drawn, which is what another program
/// draws the pattern from.
struct PatternLine
{
    double angle_deg{0.0};      ///< 53: the lines' direction, the hatch's turn included
    double base_x{0.0};         ///< 43: a point the first line passes through
    double base_y{0.0};         ///< 44
    double offset_x{0.0};       ///< 45: from one line to the next, in the drawing's axes
    double offset_y{0.0};       ///< 46
    std::vector<double> dashes; ///< 49 each, 79 of them: ink positive, gap negative
};

/// Group 41 for `def` in `unit`: the scale against the metric pattern file, or
/// — DXF's own rule for a pattern of the user's own (type 0) — the spacing.
double dxf_pattern_scale(const core::HatchDef& def, core::DrawingUnit unit) noexcept;

/// The definition lines `def` draws with, in `unit`: each family turned by the
/// hatch's angle and scaled, its base point set on the hatch's origin, and again
/// a quarter turn on when the hatch is doubled. Empty for a solid, and for a
/// pattern known only by its name.
std::vector<PatternLine> pattern_lines(const core::HatchDef& def, core::DrawingUnit unit);

/// Group 41 back onto `def`, whose `pattern_type` and `angle_udeg` are read:
/// the scale of a catalogue pattern, or the one family of a user pattern.
void apply_dxf_pattern_scale(core::HatchDef& def, double group41, core::DrawingUnit unit);

/// `def.families` and `def.origin` from the file's own definition lines, at
/// `def.scale` and `def.angle_udeg`. `known` — the catalogue's families for the
/// pattern's name, or a user pattern's one — decides the origin when the file's
/// lines are that pattern, and is kept as it is when they match it to the
/// rounding of a text file. A doubled hatch's second half, the same lines a
/// quarter turn on, is folded back into `double_lines`. False, `def` untouched,
/// when the lines are not a pattern this program can hold.
bool families_from_lines(core::HatchDef& def, std::span<const PatternLine> lines,
                         core::DrawingUnit unit, std::span<const core::HatchDef::Family> known);

// ------------------------------------------------------------------ text ----

/// TEXT with its `%%d`, `%%p`, `%%c`, `%%%` codes expanded to °, ±, Ø, %.
std::string expand_text_codes(std::string_view raw);

/// MTEXT with its formatting stripped: `\P` becomes a line break, `\{ \} \\`
/// their character, `\S a/b;` a fraction and `\S a^b;` its two halves side by
/// side, the switches `\L \l \O \o \K \k` dropped alone, `\A \C \f \F \H \Q \T \W
/// \p ...;` dropped with their argument, `{ }` groups unwrapped, `%%` codes
/// expanded.
std::string strip_mtext(std::string_view raw);

/// A caption as MTEXT content: a line break as `\P`, a backslash and a brace
/// escaped, everything else as it is — what `strip_mtext` reads back to the
/// same words.
std::string escape_mtext(std::string_view text);

#ifdef KENTOS_HAVE_DXFRW

// ----------------------------------------------------------------- XDATA ----

/// The byte form of extended entity data kept in `core::ForeignTable` under
/// `core::kForeignDxfXdata`: per item `u16 code, u8 type, payload` — a string as
/// `u32 length + UTF-8`, an integer as `i64`, a double as its eight IEEE bytes
/// (the FILE's number, model.md R26a), a coordinate as three of those. Little
/// endian, bounds-checked on the way back.
std::vector<std::uint8_t> encode_xdata(const std::vector<std::shared_ptr<DRW_Variant>>& items);
std::vector<std::shared_ptr<DRW_Variant>> decode_xdata(std::span<const std::uint8_t> bytes);

/// The library's version enum for the year a user names.
DRW::Version drw_version_for_year(int year) noexcept;

/// `AC1021` for `DRW::AC1021`; `?` for unknown.
std::string acad_name(DRW::Version v);

#endif

} // namespace kentos::io::dxf
