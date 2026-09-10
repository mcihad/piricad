// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: DXF, read and written as what it is.
//
// io.md R13: DXF is first-class — full read and write, round-trip tested. The
// GDAL vector path reads a DXF through OGR, which flattens every curve before
// this program sees it and drops what OGR has no feature for: the block
// structure, the extended entity data, the true circle. This path reads the
// file at group-code level through libdxfrw (LibreCAD's reader/writer), so a
// CIRCLE arrives as a centre and a radius, an ARC as its ends, an ELLIPSE as its
// axes, a block as its definition, and XDATA as bytes to keep.
//
// Only core and command types cross this header (io.md R1); the DRW_* types live
// in src/dxf_reader.cpp, src/dxf_writer.cpp and src/dxf_common.cpp.
#pragma once

#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/diagnostics.hpp"
#include "kentos_cad/io/options.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace kentos::io {

/// True when this build carries libdxfrw (`KENTOS_WITH_DXFRW`). False routes DXF
/// through the GDAL vector path, which reads less and says so.
bool dxf_backend_available();

/// One line saying which of the two roads a DXF takes in this build and why.
std::string dxf_backend_status();

/// The AutoCAD release a DXF is written as. The number is the AutoCAD year;
/// 2007 and later are UTF-8 files, earlier ones carry a code page.
enum class DxfVersion : std::uint8_t {
    R2000, ///< AC1015
    R2004, ///< AC1018
    R2007, ///< AC1021 — the default: UTF-8, every program since 2007 reads it
    R2010, ///< AC1024
    R2013, ///< AC1027
    R2018, ///< AC1032
};

/// The version a user names with `surum=2013`, or nothing for a year that is not
/// a DXF release.
std::optional<DxfVersion> dxf_version_from_year(int year) noexcept;

/// The year, for transcripts.
int dxf_version_year(DxfVersion v) noexcept;

/// What a read or a write produced.
struct DxfReport
{
    std::string version;       ///< `AC1018` as the file says it
    std::string crs;           ///< the system the coordinates are in
    std::uint64_t entities{0}; ///< entities created (read) or written
    std::uint64_t layers{0};   ///< layers touched (read) or written
    std::uint64_t blocks{0};   ///< block definitions read (or written)
    std::vector<std::pair<std::string, std::size_t>> layer_names; ///< (name, count), file order
    ImportDiagnostics diagnostics;                                ///< every loss, said
};

/// Reads `path` into `tx`'s document. Streams through libdxfrw's callbacks,
/// honours `stop` every few thousand entities (io.md R15), and reports every
/// entity type it read, degraded or left out (P11/P13). The file's `$INSUNITS`
/// is compared to `options.drawing_unit` and reported, never obeyed.
command::Task<core::Result<DxfReport>> import_dxf(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop);

/// Writes `doc` to `path` as a DXF of `version`, every kind as its own DXF
/// entity, layers with their colours and weights, attributes as `KENTOSCAD`
/// extended data, foreign data as the XDATA it came from, and a `.prj` beside it.
command::Task<core::Result<DxfReport>> export_dxf(const core::Document& doc, std::string path,
                                                  ExportOptions options, DxfVersion version,
                                                  std::stop_token stop);

} // namespace kentos::io
