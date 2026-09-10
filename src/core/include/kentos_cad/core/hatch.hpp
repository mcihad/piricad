// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: a pattern-filled face.
//
// What a DXF `HATCH` is: boundary loops and the pattern that fills them. The
// loops are the rings, in the face convention (R11: exterior first, islands
// after, so the arena bounds and measures the face like a parcel); the pattern —
// name, angle, scale, origin and the line families that draw it — is the
// payload (model.md R9a).
//
// THE PICTURE IS THE STYLE'S. Rendering never reads this payload: the command
// that makes a hatch (or the DXF reader) interns a Symbol whose fill layers are
// the pattern's families, and the frame path reads one `u32` as it does for
// every entity (R14). The payload is what lets the hatch be edited, exported and
// re-symbolised without guessing what it was.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kentos::core {

/// The payload of a `core.hatch` slot.
struct HatchDef
{
    /// One family of parallel lines in the pattern, the way acad.pat states it:
    /// an angle, a base point, an offset from one line to the next, and a dash
    /// sequence along the line (positive ink, negative gap, zero a dot). Lengths
    /// are PATTERN micrometres: what the pattern says at scale 1, multiplied by
    /// `scale` when drawn.
    struct Family
    {
        std::int64_t angle_udeg{0};          ///< the lines' direction
        std::int64_t base_x_um{0};           ///< where the first line passes
        std::int64_t base_y_um{0};           ///< where the first line passes
        std::int64_t offset_x_um{0};         ///< from one line to the next, along the lines
        std::int64_t offset_y_um{0};         ///< from one line to the next, across them
        std::vector<std::int64_t> dashes_um; ///< empty = continuous

        friend bool operator==(const Family&, const Family&) = default;
    };

    bool solid{true};              ///< filled with the colour, no pattern
    bool double_lines{false};      ///< the pattern drawn again at 90° (DXF 77)
    bool gradient_dropped{false};  ///< the source was a gradient this program does not draw
    bool associative{false};       ///< DXF 71, kept for the round trip
    std::uint16_t style{0};        ///< DXF 75: 0 normal, 1 outermost, 2 ignore islands
    std::uint16_t pattern_type{1}; ///< DXF 76: 0 user, 1 predefined, 2 custom
    std::string name{"SOLID"};     ///< the pattern's name, at most 255 bytes
    std::int64_t angle_udeg{0};    ///< the pattern's rotation
    Ratio scale{1, 1};             ///< pattern micrometres × scale = ground micrometres
    Point2 origin{};               ///< where the pattern's base point sits, in the drawing
    std::vector<Family> families;  ///< empty for a solid, or a pattern known only by name

    friend bool operator==(const HatchDef&, const HatchDef&) = default;
};

/// The payload layout version `encode_hatch` writes.
inline constexpr std::uint16_t kHatchLayout = 1;

/// The payload bytes.
std::vector<std::uint8_t> encode_hatch(const HatchDef& def);

/// The payload back, refused when the bytes are not what `encode_hatch` writes.
Result<HatchDef> decode_hatch(std::span<const std::uint8_t> payload);

/// The payload of the slot; an error for a slot whose bytes do not decode.
Result<HatchDef> hatch_of(const RingGeometry& geom, std::uint32_t slot);

/// A family's spacing across its lines at the hatch's scale, in ground
/// millimetres: what a `LinePatternFill` layer's interval is. Zero when the
/// family has no offset across the lines.
Mm hatch_family_spacing_mm(const HatchDef& def, const HatchDef::Family& family) noexcept;

} // namespace kentos::core
