// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: AutoCAD DWG, read only, through LibreDWG.
//
// `.claude/io.md` R13 names LibreDWG and names it for a reason: it is the only
// GPL-compatible DWG implementation there is. The ODA Drawings SDK is banned
// outright (P1) and GDAL's own CAD driver is libopencad, a different
// implementation than the rulebook chose — enabling that one would quietly
// answer a decision nobody made.
//
// READ ONLY, and enforced twice over: P8 forbids a native DWG writer while the
// R14 coverage report stands, and the dependency is compiled with
// `LIBREDWG_DISABLE_WRITE=ON` so the encoder is not even linked. DWG output is
// produced by exporting DXF.
#pragma once

#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/diagnostics.hpp"
#include "kentos_cad/io/options.hpp"

#include <cstddef>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace kentos::io {

/// What one DWG read produced, and what it could not.
struct DwgReport
{
    std::string version;     ///< the DWG version the file declares, e.g. "AC1032"
    std::size_t objects{0};  ///< objects the file held
    std::size_t entities{0}; ///< entities this reader turned into geometry
    std::size_t layers{0};   ///< drawing layers created

    /// Every layer name the file holds, with how many entities each produced.
    std::vector<std::pair<std::string, std::size_t>> layer_names;

    /// Everything the reader wants the user to know: a file LibreDWG opened with
    /// warnings, the unit it scaled by, and the census of entity types — read,
    /// and left behind because this reader has no translation for them. R14's
    /// coverage report is built from exactly that census: what a real file holds
    /// that this reader still drops.
    ImportDiagnostics diagnostics;
};

/// Whether this build linked LibreDWG.
bool dwg_backend_available();

/// Why DWG is unavailable in this build, or an empty string when it is not.
std::string dwg_backend_status();

/// Reads `path` into `tx`.
///
/// io.md R15: streams, checks `stop` at least every 64 K entities, and returns
/// within 100 ms of a cancellation. R17: everything happens inside the caller's
/// one transaction, so a failure rolls the document back untouched.
/// `options.only` names the layers to read; empty means all of them. See the
/// note on `import_vector` — the two readers answer the same way on purpose.
/// `options.fields` is ignored: a DWG carries no attribute table this reader
/// offers.
command::Task<core::Result<DwgReport>> import_dwg(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop);

} // namespace kentos::io
