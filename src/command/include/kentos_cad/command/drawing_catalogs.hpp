// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: the two drawing catalogues under /data/catalogs/dxf.
//
// A hatch PATTERN (ANSI31, NET, …) and a dimension STYLE (ISO-25, STANDARD, …)
// are data, not code (CLAUDE.md 5.13, data.md R1): the angle of a hatch line and
// the length of an arrowhead live in a JSON package a user can extend, and this
// header is what reads them. Both `TARAMA` and `ÖLÇÜ` load their catalogue at
// the moment they run; the DXF reader loads the pattern catalogue to give a
// `HATCH` named `ANSI31` its families.
//
// Paths are the App settings `core.tarama.desen_katalogu` and
// `core.olcu.stil_katalogu`, written the way the manual prints them
// (`data/catalogs/dxf/...`) and resolved by `resolve_catalog_path`.
#pragma once

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/style.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {

/// One named pattern: its line families at scale 1, in pattern micrometres.
struct HatchPattern
{
    std::string id;                               ///< `ANSI31`, upper case, as DXF names it
    std::string description;                      ///< Turkish, one line
    std::vector<core::HatchDef::Family> families; ///< empty for `SOLID`
};

/// The pattern catalogue.
struct HatchPatternCatalog
{
    std::string package_version;        ///< the package's own version
    std::vector<HatchPattern> patterns; ///< in file order

    /// The pattern named `id`, matched case-insensitively; null when unknown.
    const HatchPattern* find(std::string_view id) const noexcept;
};

/// One named dimension style, in PAPER micrometres (model.md R20).
struct DimensionStyle
{
    std::string id;                                   ///< `ISO-25`
    std::string description;                          ///< Turkish, one line
    core::ArrowStyle arrow{core::ArrowStyle::Closed}; ///< how the dimension line ends
    std::int32_t arrow_um{2500};                      ///< arrowhead length
    std::int32_t extension_beyond_um{1250};           ///< extension line past the dimension line
    std::int32_t extension_offset_um{625}; ///< gap between a point and its extension line
    std::int32_t text_gap_um{625};         ///< between the dimension line and the text
    std::int32_t text_height_um{2500};     ///< the caption's height
    std::uint8_t precision{2};             ///< decimals in the text
    char decimal_separator{','};           ///< `,` or `.`
};

/// The dimension style catalogue.
struct DimensionStyleCatalog
{
    std::string package_version;        ///< the package's own version
    std::vector<DimensionStyle> styles; ///< in file order

    /// The style named `id`, matched case-insensitively; null when unknown.
    const DimensionStyle* find(std::string_view id) const noexcept;
};

/// The default catalogue paths, as the settings' fallbacks print them.
inline constexpr const char* kDefaultHatchPatternPath   = "data/catalogs/dxf/tarama-desenleri.json";
inline constexpr const char* kDefaultDimensionStylePath = "data/catalogs/dxf/olcu-stili.json";

/// Where a catalogue path points on this machine: the path as given when it
/// exists; else under `$KENTOS_DATA` (with the leading `data/` dropped); else
/// under the source tree this build was configured from. Empty when none holds
/// the file, so the caller can say which path it looked for.
std::string resolve_catalog_path(std::string_view configured);

/// Reads and validates a pattern catalogue.
core::Result<HatchPatternCatalog> load_hatch_patterns(const std::string& path);

/// Reads and validates a dimension style catalogue.
core::Result<DimensionStyleCatalog> load_dimension_styles(const std::string& path);

/// The symbol a hatch is drawn with (model.md R14: resolved at commit, one u32
/// per entity at frame time): a boundary stroke in `ink`, then — solid — one
/// filled layer in `ink`, or — patterned — one `LinePatternFill` layer per
/// family at the hatch's angle and scale, doubled at 90° when the hatch says so.
/// Dash sequences are not drawn in this version and the caller says so.
core::Symbol hatch_symbol(const core::HatchDef& def, std::uint32_t ink_rgba);

} // namespace kentos::command
