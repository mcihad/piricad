// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: a DXF's MULTILEADERs, which libdxfrw does not read (TODOS C-12).
//
// GDAL'S DXF DRIVER READS THEM (CLAUDE.md 5.16): a leader's lines and its
// landing, its arrowhead as a filled outline, its words with their anchor,
// height and turn. The DXF import reads everything else with libdxfrw and asks
// this for the one entity that library has no class for, and only when the file
// holds one — a second pass over a large drawing costs time the import budget
// does not have.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <string>
#include <vector>

namespace kentos::io {

/// A point as the file writes it, in the file's own units.
struct DxfXY
{
    double x{0.0}; ///< as the file writes it
    double y{0.0}; ///< as the file writes it
};

/// One MULTILEADER, its parts gathered by entity handle.
struct DxfMultiLeader
{
    std::string handle;                     ///< its entity handle, the parts' common key
    std::string layer;                      ///< the layer it is on
    std::vector<std::vector<DxfXY>> lines;  ///< its leader lines and its landing, each a run
    std::vector<std::vector<DxfXY>> arrows; ///< its arrowheads' outlines, one per leader line
    std::string text;                       ///< its words, lines split by '\n'; empty without
    DxfXY text_at{};                        ///< where the words' anchor stands
    double text_height{0.0};                ///< their height, in file units
    double text_angle{0.0};                 ///< their turn, degrees counter-clockwise
    int text_anchor{7};                     ///< GDAL's label anchor, 1–9: 7 is top-left
};

/// Whether this build reads MULTILEADERs at all (it needs GDAL).
bool dxf_multileaders_supported() noexcept;

/// Every MULTILEADER on the drawing of the DXF at `path` — not in a block
/// definition, not in paper space. `utf8` reads its words as UTF-8 whatever the
/// file's code page says, as a DXF 2007 or later always is.
core::Result<std::vector<DxfMultiLeader>> read_dxf_multileaders(const std::string& path, bool utf8);

} // namespace kentos::io
