// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): the Netcad NCZ drawing format, parsed.
//
// Copyright (C) 2026 Erdinç Örsan ÜNAL
//     The NCZ parser this file and ncz_format.cpp port: `ncz_pure.py` of his QGIS
//     plugin "NCZ Reader", version 1.4.3,
//     https://github.com/erdincunal/Jeomatik-NCZ-Reader — licensed GPL-2.0-or-later.
// Copyright (C) 2026 KentOSCad contributors
//     The C++ port, 28 September 2026 (GPLv3 §5a: this is a modified version).
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. The original is "version 2 of the License, or (at your option) any
// later version"; this port takes that option, as the rest of this program is
// GPL-3.0-or-later. See /NOTICE for the attribution and the trademark note.
//
// WHAT "PORT" MEANS HERE. Every block rule, every offset, every validity test and
// every heuristic of the reference parser is kept, so a file reads into the same
// entities with the same values in the same order. Four things differ, and each
// is a place where the reference could not be copied without breaking a rule of
// this program:
//
//   * TWO PASSES OVER THE MAPPED FILE instead of a list of every entity. The
//     reference collects everything and then "finalises": it drops the `S0`
//     symbols a SmartObject leaves behind and fills in layer names and colours
//     from tables that may come after the geometry. The first pass here reads
//     those tables and notes whether a SmartObject exists; the second hands each
//     entity over, already final, and forgets it (io.md R15: bounded working set).
//   * THE SINE, COSINE AND ARCTANGENT ARE THIS PROGRAM'S (`core/trig.hpp`), not
//     the platform's: a corner must land on the same millimetre on three
//     operating systems (kentoscad.md §7.3). They agree with the reference to a
//     few ulps.
//   * THE COLLINEAR-VERTEX PASS a rectangle test runs is the same removal, in the
//     same order, found in O(n log n) instead of rescanning the ring after every
//     removal — a 3 700-vertex boundary is not a box, and the reference spent a
//     quadratic scan finding that out.
//   * WHAT THE REFERENCE DROPS IN SILENCE IS COUNTED (io.md P11): a record too
//     short for its type, a coordinate outside the world, a text with no height.
//
// And three things it does not do at all, each found on a real drawing and each
// leaving a Netcad 5 file — the reference's own ground — exactly as it read:
//
//   * A BLOCK OF A TYPE IT DOES NOT KNOW IS SEARCHED FOR GEOMETRY, as a container
//     is. Netcad 8 writes its settings as Delphi-style `name value` strings
//     between the geometry, the reference's walk reads a letter of them as a block
//     type, and whatever that "block" spans is skipped: on a 72 MB Sivas UİP that
//     was 190 582 objects — a third of the file, and 0,1 % of them duplicates of
//     what it did read. On the 12 MB `Suşehri` plan it finds nothing more.
//   * A SMARTOBJECT WITH NO RECTANGLE IS KEPT AS A POINT. The reference needs
//     a rectangle of at least a millimetre and dropped the rest; Netcad 8's plan
//     notations are anchored at a point — 4 088 of them had no size, and 5 689
//     more held 10^222 where the reference reads a width.
//   * THE LINE WIDTH IS READ: the float at +28 of every geometry record, in
//     tenths of a millimetre. The reference never looks at it; Netcad's own DXF
//     export of the same drawing agrees with it on 99,6 % of 32 000 matched
//     objects, and on the rest it rounds to its lineweight table.
//
// Not a public header (io.md R1): nothing outside /src/io sees a double here.
// The one conversion to millimetres happens in ncz_reader.cpp.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace kentos::io::ncz {

/// The entity kinds, in the reference parser's words — which are also the words
/// the import report and the `entity_type` column use, so a user comparing this
/// import with the plugin's layers reads the same names.
enum class Kind : std::uint8_t {
    Point,
    Line,
    Polyline,
    Polygon,
    Circle,
    Arc,
    Text,
    Symbol,
    Block,
    MapSheet,
    Triangle,
    SmartObject,
};

inline constexpr std::size_t kKindCount = 12;

/// `Point`, `Line`… as the reference spells them.
const char* kind_name(Kind k) noexcept;

/// One coordinate as the reference returns it: `x` EAST, `y` NORTH. The file
/// holds them the other way round — northing first, the Turkish X — and the
/// reference swaps them in `_coordinate`; so does this parser, in one place.
struct Coord
{
    double x{0.0}; ///< easting, metres (the file's second number)
    double y{0.0}; ///< northing, metres (the file's first number)
    double z{0.0}; ///< what the reference reads as a height; see ncz_reader.cpp
};

/// Which optional fields the reference set for an entity — the keywords its
/// `_append_entity` was called with. A field left out keeps its default and is
/// not a fact from the file, so the reader writes a column only where the bit is
/// set.
enum Field : std::uint16_t {
    kName       = 1u << 0,
    kLabel      = 1u << 1,
    kTextHeight = 1u << 2,
    kRotation   = 1u << 3,
    kBoxWidth   = 1u << 4,
    kBoxHeight  = 1u << 5,
    kScale      = 1u << 6,
    kGridX      = 1u << 7,
    kGridY      = 1u << 8,
    kRadius     = 1u << 9,
    kStartAngle = 1u << 10,
    kEndAngle   = 1u << 11,
    kClosed     = 1u << 12,
};

/// A Netcad 8 SmartObject's class — "akıllı nesne" in Netcad's own words — as
/// the GUID in its record names it. Planet's symbol tools write these: the
/// settlement symbol (nizam, kat, bahçe mesafeleri), the construction symbol
/// (TAKS, KAKS, Emsal, Hmax, Yençok), the road-width symbol, a plan note (RTF)
/// and a function name.
enum class SmartClass : std::uint8_t {
    None,         ///< not a Netcad 8 SmartObject: the reference's rectangle, or nothing
    Settlement,   ///< `Yerleşim`
    Construction, ///< `Yapılaşma`
    Road,         ///< `Yol`
    PlanNote,     ///< `Plan Notu`
    FunctionName, ///< `Fonksiyon Adı`
    Other,        ///< a class this reader does not draw: kept as a point with its properties
};

/// `Yerleşim`, `Yapılaşma`… — the words Netcad's menu uses.
const char* smart_class_name(SmartClass c) noexcept;

/// One property of a Netcad 8 SmartObject, as its record holds it: every value
/// is TEXT there (`12`, `0.4`, `AYRIK`, `True`), whatever the type byte says.
struct SmartProperty
{
    std::string name;    ///< the property's own name: `nizam`, `taks`, `genislik`
    std::string value;   ///< its value, as written
    std::string display; ///< the label Netcad's property grid shows: `Nizam`, `Taks`
    bool user{false};    ///< a value the user entered, not a setting of the symbol
    bool null{false};    ///< switched off by its `chk…IsNull` companion: not drawn
};

/// One entity, already finalised: its layer name and colour are what the
/// reference's `_finalize_entities` leaves in them.
struct Entity
{
    Kind kind{Kind::Point};
    std::uint8_t layer_code{0};
    std::string layer_name;             ///< empty when no table names the code
    std::optional<std::uint32_t> color; ///< 0xAARRGGBB, or none
    std::string name;                   ///< a point's name — its number
    std::string label;                  ///< text, symbol code, block/sheet name
    double text_height{0.0};            ///< metres
    double rotation{0.0};               ///< degrees, as the reference computes it
    double box_width{0.0};              ///< metres
    double box_height{0.0};             ///< metres
    double scale{0.0};
    double grid_x{0.0};
    double grid_y{0.0};
    double radius{0.0};      ///< metres
    double start_angle{0.0}; ///< as stored: radians in every file seen so far
    double end_angle{0.0};
    /// The pen width, in TENTHS of a millimetre as the file holds it (7 is
    /// 0,70 mm); 0 is the thinnest line, and a negative value — which Netcad's
    /// DXF export writes as 0 — is left to the layer.
    double line_width{0.0};
    bool closed{false};
    std::uint16_t set{0}; ///< `Field` bits
    std::vector<Coord> coords;

    /// A Netcad 8 SmartObject's class and properties; `None` and empty for
    /// every other entity. The size and the rotation are `scale` and
    /// `rotation`, where the reference reads them.
    SmartClass smart{SmartClass::None};
    std::vector<SmartProperty> properties;
};

/// One value of an attribute row: absent (the reference's `None`), an integer, a
/// float or a text.
using Cell = std::variant<std::monostate, std::int64_t, double, std::string>;

/// One `@TAB` record, decoded the reference's way. The columns keep the order
/// the reference inserts them in.
struct AttributeRow
{
    std::size_t row_index{0};
    std::vector<std::pair<std::string, Cell>> columns;
};

/// Every record of one `@TABn` reference, in file order.
struct AttributeTable
{
    std::string table_ref;
    std::vector<AttributeRow> rows;
};

/// Everything the reference returns besides its entity list, and what this port
/// counts that the reference let fall silently.
struct Header
{
    std::vector<std::string> layer_names;    ///< the layer tables, non-blank names only
    std::vector<std::uint32_t> layer_colors; ///< LEX.ST2, 0xFFRRGGBB
    std::string version_name;                ///< `5.2.0.1035N`
    std::string epsg;                        ///< TILED_XML's `SRS…` as the reference cleans it
    std::string projection_text;             ///< `ITRF / 3 / Zone 39`

    /// The MPROJ bytes the projection text is built from, for the checks the
    /// reader makes against the drawing's system. Meaningful when `mproj`.
    bool mproj{false};
    std::uint8_t projection{0}; ///< 1 geographic, 2 six-degree, 3 three-degree
    std::uint8_t datum{0};      ///< 0 WGS-84, 1 ITRF, 4 ED50, 254 ED50-HGK
    std::uint8_t zone{0};       ///< the zone byte: the central meridian for 3°

    /// Geometry records of a type the reference does not read, by type byte.
    std::array<std::uint64_t, 256> unsupported{};

    /// Geometry records of a type it DOES read that produced nothing: too short
    /// for the type, a coordinate outside ±100 000 km, no text, no height, fewer
    /// than two points, no area. By type byte. The reference drops these without a
    /// word; the reader says how many.
    std::array<std::uint64_t, 256> dropped{};

    /// `S0` symbols on layer 0 left out because the drawing holds a SmartObject:
    /// the grid marks the reference found such an object draws.
    std::uint64_t smart_marks{0};

    /// Blocks of a type the reference does not know, searched for geometry, and
    /// the entities they held (see the file comment).
    std::uint64_t swept_blocks{0};
    std::uint64_t swept_entities{0};

    /// SmartObjects kept as a point because they have no usable rectangle.
    std::uint64_t point_smart_objects{0};

    /// Netcad 8 SmartObjects read with their class and properties — Planet's
    /// settlement, construction, road, plan-note and function-name symbols.
    std::uint64_t planet_symbols{0};

    bool smart_object{false}; ///< the drawing holds at least one SmartObject
};

/// Receives the entities in the reference's order.
class Sink
{
public:
    virtual ~Sink() = default;

    /// The tables as they stand at the END of the file — the first pass's —
    /// before the first entity arrives: what a layer's own colour is, and what
    /// the file says its coordinates are in. Returning false stops the read
    /// before any geometry is handed over.
    virtual bool begin(const Header& final_header)
    {
        (void)final_header;
        return true;
    }

    /// One entity. Returning false stops the read: the reader has failed and
    /// has nothing more to do with the rest of the file.
    virtual bool entity(const Entity& e) = 0;
};

/// A layer's own colour: the reference's `_geometry_color(code, 0)`, the colour
/// an entity with colour code 0 takes. None when no LEX.ST2 table covers it.
std::optional<std::uint32_t> layer_color(const Header& h, std::uint8_t layer_code) noexcept;

/// How a read ended.
enum class Outcome : std::uint8_t {
    Complete,  ///< every block was read
    Cancelled, ///< `stop` was requested
    Stopped,   ///< the sink refused an entity
};

/// Reads `data` the way the reference's `_NCZParser.parse` does — minus the
/// attribute tables, which `attribute_tables` reads — handing every entity to
/// `sink`, and fills `header`. Checks `stop` at least every 4 MB (io.md R15).
Outcome read(std::span<const std::uint8_t> data, Sink& sink, Header& header, std::stop_token stop);

/// Only the header: the first of the two passes, for a caller that wants the
/// tables and the declared system without the geometry.
Outcome read_header(std::span<const std::uint8_t> data, Header& header, std::stop_token stop);

/// The reference's `_extract_attribute_tables`: every `@TABn` record, sorted by
/// reference. Empty when the file has none; `std::nullopt` when stopped.
std::optional<std::vector<AttributeTable>> attribute_tables(std::span<const std::uint8_t> data,
                                                            std::stop_token stop);

} // namespace kentos::io::ncz
