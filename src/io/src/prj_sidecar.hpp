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

#include "kentos_cad/core/result.hpp"

#include <string>

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

} // namespace kentos::io
