// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io (internal): the drawing of a Netcad 8 SmartObject.
//
// Netcad calls these "akıllı nesne": the settlement, construction, road-width,
// plan-note and function-name symbols its Planet module places on a zoning
// plan. A file keeps only their properties — `nizam=AYRIK`, `kat=3`,
// `taks=0.4` — and Netcad draws the symbol when it shows them; its own DXF
// export leaves most of them out. So nothing but Netcad could show them, and
// this is what lets this program do it.
//
// WHAT THEY LOOK LIKE was taken from Netcad's own documentation of the tools
// that make them ("2.4 Sembol İşlemleri", wiki.netcad.com.tr), measured against
// the symbol's radius — ten metres at an object size of one, which is what the
// record's own bounding box says (20 × size across, on all 15 722 objects of the
// Sivas UİP):
//
//   `Yerleşim`  a circle; the front garden above a short dash, the side garden
//               below it, the order (A, B, BL…) left of it and the storeys right
//   `Yapılaşma` a circle split by a line, TAKS over KAKS (`0.30-0.40` when a
//               minimum is set) — or, for `Emsal`, `E=1.00` and its lines, bare
//   `Yol`       a circle; the width's whole metres, and its two decimals raised,
//               small and underlined: `17⁰⁰`
//   `Plan Notu` its RTF as text, wrapped to the note's box, and the box
//   `Fonksiyon` the function's name
//
// Built in the symbol's own space — metres, anchor at the origin, size one — so
// one definition serves every object that says the same thing and the object's
// size and turn are the reference's scale and rotation.
#pragma once

#include "ncz_format.hpp"

#include "piricad/core/text_store.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace piricad::io::ncz {

/// One stroke of a symbol, in the symbol's own space (metres, anchor at 0, 0).
struct Stroke
{
    enum class Type : std::uint8_t {
        Circle,   ///< `centre`, `radius`
        Polyline, ///< `points`, closed when `closed`
        Text,     ///< `text` at `centre`, `height` tall, placed by `anchor`
    };

    Type type{Type::Polyline};
    std::pair<double, double> centre{0.0, 0.0};
    double radius{0.0};
    std::vector<std::pair<double, double>> points;
    bool closed{false};
    std::string text;
    double height{0.0};
    core::TextAnchor anchor{core::TextAnchor::MiddleCentre};
    double wrap_width{0.0}; ///< a text that breaks to this width; 0 does not wrap
};

/// A drawn symbol and what makes two of them the same drawing.
struct Symbol
{
    std::string key;     ///< the block's name: the class and what the symbol reads
    std::string summary; ///< one line for the block's description
    std::vector<Stroke> strokes;
};

/// The drawing of a Netcad 8 SmartObject, or nothing for a class this reader
/// does not draw — which is then read as a point with its properties.
std::optional<Symbol> planet_symbol(const Entity& e);

/// The plain text of an RTF document, paragraphs on their own lines: what a
/// plan note says, without its fonts. Group destinations (font and colour
/// tables, pictures, `\*` groups) are skipped; `\uN` is decoded, and `\'hh`
/// as Windows-1254, which is what a Turkish Netcad writes.
std::string rtf_text(std::string_view rtf);

/// Base64, as `rtfData` is kept; empty for text that is not.
std::string base64_decode(std::string_view in);

} // namespace piricad::io::ncz
