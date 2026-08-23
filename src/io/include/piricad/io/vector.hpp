// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: external vector formats, through GDAL/OGR.
//
// .claude/io.md R2/P2: no `gdal*.h`, `ogr*.h` or `cpl_*.h` appears in this header
// or anywhere outside this module's .cpp files. Everything below is core and
// command types.
//
// .claude/io.md R22 and CLAUDE.md Article 8.2: the backend sits behind
// `PIRICAD_WITH_GDAL`, which defaults OFF and hard-fails at configure time when
// switched ON without GDAL present. When it is OFF, every entry point here
// returns an `Error` naming the option and the install command — it never
// silently succeeds and never silently returns an empty layer.
//
// .claude/io.md P7: the driver set is an explicit allow-list held in
// /cmake/PiriCADGdalDrivers.cmake and handed to this module as a compile
// definition. Adding a driver is an edit to that file, in the same change.
#pragma once

#include "piricad/command/task.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/result.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

namespace piricad::io {

/// One allow-listed external format.
struct VectorFormat
{
    std::string driver;    ///< the OGR driver name, e.g. "DXF", "GPKG"
    std::string label;     ///< Turkish label for a file dialog
    std::string extension; ///< ".dxf", lower case, with the dot
    bool read{false};      ///< the allow-list permits reading it
    bool write{false};     ///< the allow-list permits writing it
};

/// The allow-list, always populated, in the order /cmake declares it. Whether
/// this build can actually reach a driver is `vector_backend_available()`.
const std::vector<VectorFormat>& vector_formats();

/// The allow-listed format whose extension matches `path`, or nullptr.
const VectorFormat* vector_format_for_path(const std::string& path);

/// The allow-listed format whose driver name matches `id`, Turkish-folded, or
/// nullptr.
const VectorFormat* vector_format_by_id(const std::string& id);

/// True when this build was configured with `PIRICAD_WITH_GDAL=ON`.
bool vector_backend_available();

/// One Turkish line saying what the vector backend can do right now, and when it
/// cannot, exactly which option and which package would change that. Never empty:
/// a status nobody can read is not a status.
std::string vector_backend_status();

/// What an import or an export actually did, for the transcript line.
struct VectorReport
{
    std::string driver;        ///< the OGR driver that handled it
    std::uint64_t features{0}; ///< features read or written
    std::uint64_t entities{0}; ///< entities created in the document
    std::uint64_t layers{0};   ///< layers read or written
    std::string crs;           ///< the CRS the dataset declared (io.md R20)
    /// Anything the user should know that is not a failure — a dropped field, a
    /// sidecar written, a driver limitation worked around. Reported, never
    /// swallowed: a silent lossy export is how a wrong pafta gets delivered.
    std::vector<std::string> notes;
};

/// Reads `path` into `tx.document()`, merging into whatever is already there.
///
/// One transaction, one undo entry; any failure rolls the document back to
/// exactly its pre-import state (io.md R17, P11). The dataset MUST declare a CRS
/// — a missing one is an error, never a silent assumption of TUREF/TM30 (R20).
///
/// `project_crs` is the drawing's own coordinate system, passed in rather than
/// read off the `Document`. Today the CRS a user can actually set is the
/// project-scope setting `core.crs.id` (model.md R39/R40), while
/// `core::Document::crs()` has no command that writes it — see the note in
/// `service.cpp`. Taking it as an argument keeps this module out of that
/// argument: it compares what it is given and never guesses.
///
/// `path` and `driver` are taken by value for the coroutine-lifetime reason
/// spelled out in `project.hpp`.
command::Task<core::Result<VectorReport>> import_vector(command::Transaction& tx, std::string path,
                                                        std::string driver, std::string project_crs,
                                                        std::stop_token stop);

/// Writes `doc` to `path` through the named allow-listed driver, or through the
/// driver that matches the extension when `driver` is empty.
/// `crs` is the coordinate system to stamp on the output, and it must be one OGR
/// can resolve (an EPSG code, a PROJ string, a WKT). An export with no usable CRS
/// produces a file whose coordinates mean nothing to whoever receives it, so it
/// is refused rather than written (io.md R20).
command::Task<core::Result<VectorReport>> export_vector(const core::Document& doc, std::string path,
                                                        std::string driver, std::string crs,
                                                        std::stop_token stop);

} // namespace piricad::io
