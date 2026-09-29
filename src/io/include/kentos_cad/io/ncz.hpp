// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: Netcad NCZ drawings, read natively.
//
// Copyright (C) 2026 Erdinç Örsan ÜNAL
//     The NCZ parser this reader ports: `ncz_pure.py` of his QGIS plugin
//     "NCZ Reader", version 1.4.3,
//     https://github.com/erdincunal/Jeomatik-NCZ-Reader — licensed GPL-2.0-or-later.
// Copyright (C) 2026 KentOSCad contributors
//     The C++ port and the reader around it, 28 September 2026.
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. See /NOTICE for the attribution and the trademark note.
//
// WHY NATIVE. NCZ is Netcad's own binary drawing format and no library reads
// it: GDAL has no driver, and the one open implementation is the plugin above,
// in Python, for QGIS. Article 2.7 therefore allows the hand-written reader, and
// the reader is that plugin's parser, ported block for block
// (src/ncz_format.cpp), so a file comes in as the same entities with the same
// values the plugin reads — verified against it — only as the entities this
// program has: a circle is a circle and an arc is an arc, not the 72- and
// 48-segment rings the plugin approximates them with for QGIS.
//
// No library, so no `KENTOS_WITH_*` option: every build reads NCZ.
#pragma once

#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/diagnostics.hpp"
#include "kentos_cad/io/options.hpp"
#include "kentos_cad/io/vector.hpp"

#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace kentos::io {

/// What one NCZ read produced, and what it could not.
struct NczReport
{
    std::string version;       ///< the Netcad build that wrote it: `5.2.0.1035N`
    std::string crs;           ///< the system the coordinates were read in: the drawing's
    std::string declared;      ///< what the file declares, in Turkish; empty when nothing
    std::uint64_t entities{0}; ///< entities created
    std::uint64_t layers{0};   ///< layers created

    /// Every layer an entity of the file lands on, with how many it produced —
    /// the import wizard's tick boxes. A layer the file names but draws nothing
    /// on is not listed: there is nothing to tick it for.
    std::vector<std::pair<std::string, std::size_t>> layer_names;

    /// The seventeen attribute fields the reference plugin writes on every
    /// feature — `source_file`, `layer_code`, `label`, `radius`… — offered as
    /// columns exactly as a GIS file's fields are (`alanlar=`), with a sample
    /// value each. One set for the whole file: its `layer` names no layer.
    std::vector<VectorField> fields;

    std::uint64_t attribute_tables{0}; ///< `@TAB` tables the file holds
    std::uint64_t attribute_rows{0};   ///< their rows, all of them
    ImportDiagnostics diagnostics;     ///< every loss, said
};

/// Reads `path` into `tx`'s document.
///
/// Maps the file and reads it in two passes (io.md R15): the tables first, then
/// the entities one at a time into the transaction, `stop` asked at least every
/// 4 MB and every 4096 entities. The coordinates are metres in the drawing's own
/// system — Netcad's, exactly as the plugin reads them into the QGIS project's
/// — never transformed; what the file declares (its MPROJ datum, projection and
/// zone, its SRS) is compared with `options.project_meridian` and said. A file
/// that declares geographic coordinates and holds degrees is refused (io.md
/// R20a). One transaction: a failure rolls every entity back (R17).
///
/// `options.only` names the layers to read, empty for all; `options.fields`
/// the columns to write, `*` for all seventeen, empty for none. The point
/// number is always written to `nokta_no`, as `NOKTALAR` writes it.
command::Task<core::Result<NczReport>> import_ncz(command::Transaction& tx, std::string path,
                                                  ImportOptions options, std::stop_token stop);

} // namespace kentos::io
