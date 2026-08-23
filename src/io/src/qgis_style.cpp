// SPDX-License-Identifier: GPL-3.0-or-later
// QGIS QML style export.
//
// Why write QML at all: a surveyor's work does not stay in one program. A layer
// styled here goes to QGIS for analysis, to a colleague who works there, or into
// a municipal system built on it. Interop is the difference between a symbology
// engine and a symbology engine you can use.
//
// Why write it BY HAND rather than through a library: there is no library. QML is
// QGIS's own format and the only writer of it is QGIS, which CLAUDE.md 5.16 would
// otherwise have us link — and Article 2.7's test refuses that for reasons that
// have nothing to do with licence (QGIS is GPL-2+, which IS GPLv3-compatible):
// QgsGeometry is double-based where PiriCAD stores int64 millimetres, QGIS
// symbology renders into a QPainter where PiriCAD is going to QRhi, and
// QgsExpression is a second parser CLAUDE.md 5.11 forbids. Producing forty lines
// of XML is not reimplementing QGIS; it is speaking to it.
//
// This file writes; it does not read. Reading QML means reading QGIS's expression
// language, which is exactly the parser 5.11 refuses, so import lands with SLD —
// a wire format with a declarative filter — and not with QML.
#include "qgis_style.hpp"

#include "piricad/core/style.hpp"

#include <string>

namespace piricad::io {
namespace {

/// XML text escaping. Five characters, done once, rather than trusting that a
/// layer called `A & B` will never exist.
std::string escape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out += c;
        }
    }
    return out;
}

/// 0xAARRGGBB to the `r,g,b,a` QGIS writes. Alpha last, and decimal, because that
/// is what QGIS reads — a colour written the way PiriCAD stores it would load as
/// black and the user would blame the export.
std::string colour(std::uint32_t rgba)
{
    const unsigned a = (rgba >> 24) & 0xFFu;
    const unsigned r = (rgba >> 16) & 0xFFu;
    const unsigned g = (rgba >> 8) & 0xFFu;
    const unsigned b = rgba & 0xFFu;
    return std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," +
           std::to_string(a);
}

/// Paper micrometres to millimetres, which is QGIS's unit for a stroke width.
/// Written with one decimal: 700 µm is 0.7 mm, and a line weight is a plotted
/// dimension the regulation prescribes, not a screen preference.
/// Built from integers, NOT from snprintf("%f").
///
/// This is not fastidiousness. `%f` writes the decimal separator of the current
/// locale, and on a Turkish system that is a comma: the first version of this
/// function wrote `outline_width="0,700"` and QGIS reads that as zero. A file
/// format's numbers belong to the format, never to the machine that happened to
/// write them — the same rule core/json.cpp already follows for the same reason.
std::string width_mm(std::int32_t width_um)
{
    const bool negative = width_um < 0;
    const auto value    = static_cast<std::int64_t>(negative ? -width_um : width_um);

    std::string frac = std::to_string(value % 1000);
    frac.insert(0, 3 - frac.size(), '0');

    return (negative ? "-" : "") + std::to_string(value / 1000) + "." + frac;
}

void symbol_layer(std::string& out, const core::Appearance& look, bool area)
{
    out += "      <layer class=\"";
    out += area ? "SimpleFill" : "SimpleLine";
    out += "\" enabled=\"1\" pass=\"0\" locked=\"0\">\n";

    if (area) {
        out += "        <prop k=\"color\" v=\"" + colour(look.fill_rgba) + "\"/>\n";
        out += "        <prop k=\"outline_color\" v=\"" + colour(look.rgba) + "\"/>\n";
        out += "        <prop k=\"outline_width\" v=\"" + width_mm(look.width_um) + "\"/>\n";
        out += "        <prop k=\"outline_width_unit\" v=\"MM\"/>\n";
        // A fill colour of zero is "no fill" here and `no` there. Writing a
        // transparent black instead would come back as a black polygon.
        out += "        <prop k=\"style\" v=\"";
        out += look.fill_rgba == 0 ? "no" : "solid";
        out += "\"/>\n";
    } else {
        out += "        <prop k=\"line_color\" v=\"" + colour(look.rgba) + "\"/>\n";
        out += "        <prop k=\"line_width\" v=\"" + width_mm(look.width_um) + "\"/>\n";
        out += "        <prop k=\"line_width_unit\" v=\"MM\"/>\n";
        out += "        <prop k=\"line_style\" v=\"";
        out += look.dash == 0 ? "solid" : "dash";
        out += "\"/>\n";
    }

    out += "      </layer>\n";
}

} // namespace

std::string build_qml(const core::Layer& layer, const core::Symbol& symbol, bool area)
{
    const core::Appearance& look = symbol.primary();

    std::string out;
    out += "<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>\n";
    // The category list is not decoration: QGIS loads ONLY what it names, and a
    // scale window lives in `Rendering`, not in `Symbology`. Declaring symbology
    // alone made QGIS read the colours and silently drop the scale range —
    // verified by loading the file back through QGIS itself, which is the only
    // way to find this out.
    const bool windowed = symbol.min_scale != 0 || symbol.max_scale != 0;
    out += "<qgis version=\"3.34\" styleCategories=\"";
    out += windowed ? "Symbology|Rendering" : "Symbology";
    out += "\"";

    // The scale window travels. QGIS spells it minimumScale / maximumScale and
    // means the same denominators PiriCAD stores, so a leke that hides when you
    // zoom in keeps hiding when you zoom in over there.
    if (windowed) {
        out += " hasScaleBasedVisibilityFlag=\"1\"";
        out += " minScale=\"" +
               std::to_string(symbol.max_scale == 0 ? 1000000000u : symbol.max_scale) + "\"";
        out += " maxScale=\"" + std::to_string(symbol.min_scale) + "\"";
    }
    out += ">\n";

    out += "  <!-- PiriCAD tarafından üretildi. Katman: " + escape(layer.name) + " -->\n";
    out += "  <renderer-v2 type=\"singleSymbol\" forceraster=\"0\" symbollevels=\"0\">\n";
    out += "    <symbols>\n";
    out += "      <symbol name=\"0\" type=\"";
    out += area ? "fill" : "line";
    out += "\" alpha=\"1\" clip_to_extent=\"1\">\n";

    symbol_layer(out, look, area);

    // A stacked symbol writes every layer it has. QGIS draws them in the same
    // back-to-front order, which is the whole reason the stack is ordered.
    for (std::size_t i = 1; i < symbol.layers.size(); ++i)
        symbol_layer(out, symbol.layers[i].look, symbol.layers[i].kind == core::StrokeKind::Fill);

    out += "      </symbol>\n";
    out += "    </symbols>\n";
    out += "  </renderer-v2>\n";
    out += "</qgis>\n";
    return out;
}

} // namespace piricad::io
