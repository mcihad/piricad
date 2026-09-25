// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: reading a drawing file into a scratch document, and loading
// external references from their files (TODOS C-14, model.md R45a).
//
// A private header of /src/io: the file service uses it for DIŞREFERANS and
// for BLOKEKLE's library, and the project reader's caller for the references
// a drawing holds when it opens.
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/crs.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/project.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

namespace kentos::io {

/// `.dxf` by extension, case-folded.
bool looks_like_dxf(const std::string& path);

/// `.dwg` by extension, case-folded.
bool looks_like_dwg(const std::string& path);

/// What a reader should be told coordinates are in: the EPSG code of a
/// resolved system, its id otherwise.
std::string crs_for_reading(const core::Crs& crs);

/// Reads a project, DXF or DWG file into `scratch`, a fresh document the
/// caller owns and nobody sees; a DXF or DWG that declares no system is read
/// in `crs`.
command::Task<core::Status> read_drawing(core::Document& scratch, std::string path, std::string crs,
                                         std::stop_token stop);

/// Where the file of an external reference recorded at `path` is: there, or —
/// a project folder carried to another place or machine — its file name
/// beside the project file `project`. Empty when neither exists.
std::string locate_external(const std::string& path, const std::string& project);

/// What one load of an external reference brought.
struct XrefLoad
{
    std::uint64_t entities{0};      ///< the file's objects, now drawn through it
    std::uint64_t layers{0};        ///< layers made for it
    std::string found_at;           ///< where the file was read from
    bool moved{false};              ///< found beside the project, not where recorded
    std::string reprojected_from;   ///< the system it was drawn in, when not the drawing's
    std::vector<std::string> notes; ///< what could not be carried, in Turkish
};

/// Reads external reference `block`'s file and fills the definition inside
/// `tx`: the file's objects become its members, the blocks it defines its
/// `NAME|BLOCK` dependents, its layers `NAME|LAYER`. The old members go first,
/// and every reference that draws it has its box brought up to date. A file
/// found beside the project rather than where it was recorded has its new
/// place written into the definition. A failure leaves the definition as it
/// was, not half filled.
///
/// A file drawn in ANOTHER coordinate system is carried into the drawing's by
/// `DÖNÜŞTÜR` — the one reprojection this program has — run on the scratch
/// through `host`'s registry and resolver; without a host that has it, such a
/// file is refused. The file's OWN external references are loaded into it
/// first, once it stands in the drawing's system, and arrive as its
/// dependents; `chain` holds the files being read on the way down, so a file
/// that references one of them is refused rather than read forever.
command::Task<core::Result<XrefLoad>> load_external(command::Transaction& tx, core::BlockId block,
                                                    std::string project, std::stop_token stop,
                                                    command::Bus* host             = nullptr,
                                                    std::vector<std::string> chain = {});

/// Loads every external reference the document holds that is neither
/// unloaded nor taken off. One whose file cannot be read becomes a warning,
/// and the drawing still opens: a missing reference draws nothing, and says so.
command::Task<std::vector<Warning>> load_externals(command::Transaction& tx, std::string project,
                                                   std::stop_token stop,
                                                   command::Bus* host             = nullptr,
                                                   std::vector<std::string> chain = {});

} // namespace kentos::io
