// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): the `.prj` companion of a coordinate-less format.
//
// A DXF has no slot for a coordinate system (io.md R20 would make the format
// unimportable without one), so the convention every GIS in the country follows
// stands in: an ESRI-style `.prj` beside the file, one WKT string. Read on import
// when present, written on export so this program's own output round-trips.
// Shared by the GDAL vector path and the libdxfrw path, which is why it is a
// header of its own and not a corner of either.
#pragma once

#include "kentos_cad/core/crs.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>

#ifdef KENTOS_HAVE_GDAL
class OGRSpatialReference;
#endif

namespace kentos::io {

/// The identifier (`EPSG:5256`, or the system's name when it has no authority
/// code) of the coordinate system the `.prj` beside `path` names. `NotFound` when
/// there is no sidecar; `Unsupported` in a build without GDAL, which is what
/// resolves the WKT.
core::Result<std::string> prj_sidecar_crs(const std::string& path);

/// Writes the `.prj` beside `path` for the system `crs_id` names and returns the
/// sidecar's path. `Unsupported` in a build without GDAL.
core::Result<std::string> write_prj_sidecar(const std::string& path, const std::string& crs_id);

/// `path` with its extension replaced by `.prj`.
std::string prj_sidecar_path(const std::string& path);

#ifdef KENTOS_HAVE_GDAL
/// `srs` as a `core::Crs` named `id` and carrying what its coordinates COUNT —
/// metres, degrees or something else — so the one message
/// (`core::crs_unit_problem`) can speak for a file as it does for a setting.
core::Crs crs_with_unit(const OGRSpatialReference& srs, std::string id);
#endif

/// Why a DXF written in `unit` (not metres) goes out WITHOUT a `.prj`, and how
/// to get one: the sidecar names a system that counts metres, and a GIS program
/// reading millimetres as metres would put the drawing a thousand times too far
/// out. One sentence for both DXF writers.
std::string dxf_prj_withheld(core::DrawingUnit unit);

/// The refusal for a file whose coordinate system does not count metres, or
/// success (TODOS F-03). `where` names the file and, when it has several, the
/// layer: "'yollar.gpkg' dosyasının 'yollar' katmanı".
///
/// A FILE'S system is not the user's to choose, so this does not say "choose
/// another" the way `AYAR` does: it says how to bring the file into metres.
core::Status file_crs_holds_metres(const core::Crs& crs, const std::string& where);

} // namespace kentos::io
