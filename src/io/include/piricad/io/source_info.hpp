// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: what a data source IS and what it can do, before any of it is read.
//
// A file in a format GDAL opens is not one thing. A GeoPackage has transactions and random reads
// and a row count you can ask for; a Shapefile has ten-character field names and no transactions; a
// GeoJSON file has no constraints at all and a row count only if you read it. The importer used to
// treat them alike and say nothing, so a user learned what a source could not do when something
// went missing (TODOS G-02). This is the report that comes FIRST: the driver, the layers, each
// layer's identity, coordinate system, row estimate, constraints and capabilities, and — beside
// them — what PiriCAD does and does not do with each.
//
// EVERYTHING DRIVER-SIDE IS GDAL'S ANSWER (`GDALDriver` capability metadata, `OGRLayer::
// TestCapability`, `OGRFieldDefn`). Nothing here is a table of what a format "usually" supports:
// the existence of a driver in GDAL does not mean the product supports every capability it reports
// (G-02), so the report separates the two on purpose.
//
// .claude/io.md R2/P2: no GDAL header appears here.
#pragma once

#include "piricad/core/json.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace piricad::io {

/// One attribute field of a layer, with the constraints the source declares for it.
struct FieldFacts
{
    std::string name;          ///< the field's own name
    std::string type;          ///< GDAL's name for its type (`Integer64`, `String`, `Date`…)
    int width{0};              ///< declared width; 0 when none
    int precision{0};          ///< declared decimals; 0 when none
    bool nullable{true};       ///< false: the source forbids NULL here (a NOT NULL constraint)
    bool unique{false};        ///< true: the source forbids duplicates (a UNIQUE constraint)
    std::string default_value; ///< the declared default, empty when none
    std::string domain;        ///< the coded-value or range domain it follows, empty when none
};

/// One layer of a source.
struct LayerFacts
{
    std::string name;       ///< the layer's identifier in the source
    std::string geometry;   ///< GDAL's name for its geometry type (`Polygon`, `Line String`…)
    bool has_z{false};      ///< the geometry type carries heights
    bool has_m{false};      ///< the geometry type carries measures
    bool curved{false};     ///< the geometry type may hold true curves (arcs)
    std::string crs;        ///< the layer's coordinate system (`EPSG:5254`); empty: none declared
    std::string crs_unit;   ///< what one coordinate counts, in GDAL's words; empty when no CRS
    std::int64_t rows{-1};  ///< the feature count; negative: not cheaply known
    bool rows_exact{false}; ///< true when the source can answer the count without reading it all
    bool has_extent{false}; ///< the bounding box below is the layer's
    double extent[4]{0, 0, 0, 0}; ///< min x, min y, max x, max y, in the layer's own units
    std::string
        fid_column; ///< the column that identifies a feature; empty when the source has none
    std::string geometry_column;    ///< the geometry column's name, when the source names one
    bool geometry_nullable{true};   ///< a feature may have no geometry
    std::vector<FieldFacts> fields; ///< every attribute field, with its declared constraints

    // What the layer can do — each flag is one `OGRLayer::TestCapability`. The write capabilities
    // (append, rewrite in place, add or drop a field) are NOT here: the source is opened read-only,
    // and GDAL answers every one of them "no" for a read-only dataset even where the format writes
    // fine. A report that said "yok" would be wrong; the driver-level `driver_can_*` flags carry
    // the honest part of that answer.
    bool random_read{false};         ///< a feature can be fetched by its id
    bool fast_spatial_filter{false}; ///< a bounding-box filter is answered from an index
    bool fast_count{false};          ///< the count is cheap
    bool fast_extent{false};         ///< the extent is cheap
    bool transactions{false};        ///< edits can be grouped and rolled back
    bool curve_geometries{false};    ///< the layer can store true curves
    bool measured_geometries{false}; ///< the layer can store measures
    bool z_geometries{false};        ///< the layer can store heights
};

/// A source as a whole.
struct SourceInfo
{
    std::string path;                    ///< as asked
    std::string driver;                  ///< the GDAL driver that opened it (`GPKG`)
    std::string driver_name;             ///< the driver's long name
    bool driver_can_create{false};       ///< the driver can write a new dataset
    bool driver_can_create_layer{false}; ///< a layer can be added to an existing dataset
    bool driver_can_delete_layer{false}; ///< a layer can be removed from one

    /// WHAT PIRICAD ITSELF PERMITS, from the allow-list (io.md P7): distinct from what the driver
    /// can do. A driver that writes is not a format this product writes until the list says so.
    bool piricad_reads{false};
    bool piricad_writes{false};

    std::vector<LayerFacts> layers;
};

/// Opens `path` read-only through GDAL and reports it. Cheap: nothing is read but metadata and,
/// where the source answers it without a scan, the row count. Fails with the reason when GDAL
/// cannot open the file or it is not in an allow-listed format.
core::Result<SourceInfo> inspect_source(const std::string& path);

/// The report as the Turkish text a transcript or a tooltip shows: for each layer its identity,
/// system, row estimate, constraints and capabilities, then what PiriCAD does with each. `only`
/// restricts it to one layer (Turkish-folded match) when non-empty.
std::string describe(const SourceInfo& info, const std::string& only = {});

/// The same facts as a structured object, for a client that reads structure (a script, an agent).
core::Json to_json(const SourceInfo& info);

} // namespace piricad::io
