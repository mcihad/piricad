// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the dash patterns one document carries.
//
// A LINE TYPE IS A PATTERN, NOT A PICTURE. MPYY prints `İL SINIRI` as a dash, a
// gap, a dot and a gap, repeated; that is four numbers. Carrying it as a picture
// instead — which is how the annex publishes it and how this program first read
// it — costs everything a pattern gives you for free: it cannot be recoloured,
// it cannot be scaled without resampling, it cannot be written into DWG, DXF or
// GML as a line type, and above all it cannot TURN A CORNER. A stamped picture is
// a rigid rectangle; it rotates to one edge's angle and leaves a wedge of nothing
// on the outside of every bend. A pattern follows the geometry and the join is
// the renderer's own.
//
// WHY THE PATTERNS TRAVEL INSIDE THE DOCUMENT. `Appearance.dash` was declared as
// an index into a table "from /data", and a table that lives only in the
// catalogue package would mean a plan sheet renders differently on a machine
// where the package is not installed. `ImageStore` settled the same question the
// same way for raster symbols and for the same reason (model.md R35): the bytes
// are here. A drawing is a legal document and it has to draw the same everywhere.
//
// THE UNIT IS THE STROKE'S OWN WIDTH. A pattern is stored as multiples of the
// line's width, which is what makes one pattern usable at 0,2 mm and at 1,0 mm
// without a second entry, and what lets it be handed to a renderer that thinks in
// pen widths. It is also what the annex means: the printed sample of a dash-dot
// boundary is drawn at some thickness and the dash is so many times that.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Index into a Document's DashStore. Slot 0 always means "solid".
using DashId = std::uint16_t;

/// A solid stroke. Costs nothing to reference and is what every drawing written
/// before line types existed already holds.
inline constexpr DashId kSolidDash = 0;

/// The longest pattern one line type may declare.
///
/// Eight segments is four marks and four gaps — more than any line type MPYY
/// publishes, and more than DWG's own `LTYPE` carries in practice. A cap rather
/// than a vector per entry keeps the store flat and keeps a malformed catalogue
/// from asking for an unbounded allocation at load time (io.md: untrusted input).
inline constexpr std::size_t kMaxDashSegments = 8;

/// One line type: alternating mark and gap lengths, in multiples of the stroke's
/// own width, starting with a mark.
struct DashPattern
{
    /// `count` entries are meaningful; the rest are zero. An odd count would
    /// leave a mark with no gap after it, so `DashStore::intern` refuses one.
    std::uint16_t lengths[kMaxDashSegments]{}; ///< in hundredths of a stroke width
    std::uint8_t count{0};

    friend bool operator==(const DashPattern&, const DashPattern&) = default;
};

/// The line types one document carries, deduplicated by content.
class DashStore
{
public:
    /// Builds a store whose slot 0 is already the solid sentinel.
    DashStore();

    /// Adds a pattern, or returns the id an identical one already has.
    ///
    /// `origin` records which catalogue row and package the pattern was published
    /// in, for the same reason `ImageStore` records it: a plan sheet that cannot
    /// say where its symbology came from cannot be checked (CLAUDE.md 11.7).
    ///
    /// Refuses a pattern with an odd number of segments, one with a zero-length
    /// mark, or one longer than `kMaxDashSegments` — each of those is a catalogue
    /// defect and drawing something plausible instead would hide it.
    Result<DashId> intern(const DashPattern& pattern, std::string_view origin);

    bool contains(DashId id) const noexcept { return id < patterns_.size(); }

    /// The pattern behind `id`. The solid sentinel and an id this store does not
    /// hold both answer a pattern of count 0, which every caller draws as solid —
    /// the same "answer what the drawing says" rule `ImageStore::bytes` keeps.
    const DashPattern& at(DashId id) const noexcept;

    /// Where this line type was published. Empty for the sentinel.
    std::string_view origin(DashId id) const;

    /// How many entries, including the sentinel at 0.
    std::size_t size() const noexcept { return patterns_.size(); }

    /// Folds every pattern into the document fingerprint, IN ID ORDER.
    ///
    /// An empty store folds to the seed unchanged, so every file and fixture
    /// written before line types existed keeps its fingerprint.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    struct Entry
    {
        DashPattern pattern;
        std::string origin;
    };

    std::vector<Entry> patterns_;
};

} // namespace kentos::core
