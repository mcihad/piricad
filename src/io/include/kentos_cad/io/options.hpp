// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: what a caller tells a reader or a writer, in one record.
//
// Every reader used to take the same four things as positional arguments —
// driver, project CRS, layer filter, field filter — and adding a fifth (the
// drawing unit to assume when a file names none) would have meant a sixth
// parameter on three signatures and every call site. A record grows without
// that, and a call site that does not care about a member leaves it defaulted.
#pragma once

#include "kentos_cad/core/units.hpp"

#include <string>
#include <vector>

namespace kentos::io {

/// What an import is asked to do beyond "read this path".
struct ImportOptions
{
    /// The format to read it as, or empty to decide from the extension.
    std::string driver;

    /// The drawing's own coordinate system, for comparison with what the file
    /// declares and as the fallback for a format that cannot declare one (DXF,
    /// DWG). Never guessed from the coordinates (io.md R20).
    std::string project_crs;

    /// The layers to read; empty means every layer. Matched Turkish-folded.
    std::vector<std::string> only;

    /// The attribute fields to read as columns; `*` alone means all of them,
    /// empty means none.
    std::vector<std::string> fields;

    /// The unit a drawing-format file (DXF, DWG) is read in: the project setting
    /// `core.cizim.birim`, which the command reads and passes here. The file's
    /// own `$INSUNITS` header is compared to it and reported, never obeyed —
    /// see `ImportDiagnostics::declared_unit` for why. A geodetic format ignores
    /// it, because its unit is its coordinate system's metre.
    core::DrawingUnit drawing_unit{core::DrawingUnit::Metre};
};

/// What an export is asked to do beyond "write this path".
struct ExportOptions
{
    /// The format to write, or empty to decide from the extension.
    std::string driver;

    /// The coordinate system to stamp on the output; must resolve in the
    /// writer's library. An export with none is refused (io.md R20).
    std::string crs;

    /// The unit a drawing-format file's coordinates are written in, and what
    /// its header declares. Ignored by geodetic formats, which write metres.
    core::DrawingUnit unit{core::DrawingUnit::Metre};
};

} // namespace kentos::io
