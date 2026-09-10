// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: external vector formats, through GDAL/OGR.
//
// .claude/io.md R2/P2: no `gdal*.h`, `ogr*.h` or `cpl_*.h` appears in this header
// or anywhere outside this module's .cpp files. Everything below is core and
// command types.
//
// .claude/io.md R22 and CLAUDE.md Article 8.2: the backend sits behind
// `KENTOS_WITH_GDAL`, which defaults OFF and hard-fails at configure time when
// switched ON without GDAL present. When it is OFF, every entry point here
// returns an `Error` naming the option and the install command — it never
// silently succeeds and never silently returns an empty layer.
//
// .claude/io.md P7: the driver set is an explicit allow-list held in
// /cmake/KentOSCadGdalDrivers.cmake and handed to this module as a compile
// definition. Adding a driver is an edit to that file, in the same change.
#pragma once

#include "kentos_cad/command/task.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/diagnostics.hpp"
#include "kentos_cad/io/options.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace kentos::io {

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

/// True when this build was configured with `KENTOS_WITH_GDAL=ON`.
bool vector_backend_available();

/// One Turkish line saying what the vector backend can do right now, and when it
/// cannot, exactly which option and which package would change that. Never empty:
/// a status nobody can read is not a status.
std::string vector_backend_status();

/// What an import or an export actually did, for the transcript line.
/// One attribute field the file carries, as the reader saw it.
///
/// Reported for EVERY field whether or not it was imported, because the import
/// wizard's third page is this list: what the file has, what type it would
/// become, and a first value so the user can tell `ada_no` from `parsel_no`
/// without opening the file elsewhere.
struct VectorField
{
    std::string layer;                         ///< the OGR layer it belongs to
    std::string name;                          ///< the field's own name, as in the file
    std::string id;                            ///< the column id it becomes: folded, `[a-z0-9_]`
    core::AttrType type{core::AttrType::Text}; ///< the declared type it maps to
    std::uint8_t scale{0};                     ///< decimals kept for a Decimal
    std::string sample;                        ///< the first non-empty value, for recognition
    bool imported{false};                      ///< whether this run wrote it as a column
};

struct VectorReport
{
    std::string driver; ///< the OGR driver that handled it
    /// Every layer NAME the file holds, in the order it holds them, with how many
    /// entities each produced. Filled whether or not `only` filtered anything, so
    /// one read answers both "what is in here" and "what did I just get".
    std::vector<std::pair<std::string, std::uint64_t>> layer_names;
    std::uint64_t features{0}; ///< features read or written
    std::uint64_t entities{0}; ///< entities created in the document
    std::uint64_t layers{0};   ///< layers read or written
    std::uint64_t texts{0};    ///< captions written as point features (export)
    std::uint64_t columns{0};  ///< attribute columns written as fields (export)
    std::string crs;           ///< the CRS the dataset declared (io.md R20)

    /// What an IMPORT wants the user to know: the unit it scaled by, the
    /// features it could not turn into geometry, the types it read or left
    /// behind, and every note a reader emitted.
    ///
    /// A REAL FILE CARRIES RUBBISH. `deneme_suşehri.dxf` is 48 MB of a real
    /// cadastral drawing and holds a LINESTRING whose fifty-five vertices are all
    /// the same point — something a CAD program left behind. Refusing the whole
    /// file over it threw away 18 497 sound entities, which is the same mistake
    /// the missing-`.prj` refusal made: correct by the letter, useless in the
    /// office. The feature is dropped, counted and NAMED (io.md P11, P13).
    ImportDiagnostics diagnostics;

    /// What an EXPORT wants the user to know that is not a failure — a sidecar
    /// written, a driver limitation worked around. Reported, never swallowed: a
    /// silent lossy export is how a wrong pafta gets delivered.
    std::vector<std::string> notes;
    std::vector<VectorField> fields; ///< every attribute field the file carries
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
/// `path` and `options` are taken by value for the coroutine-lifetime reason
/// spelled out in `project.hpp`.
/// `options.only` names the layers to read; an EMPTY list means every layer,
/// which is what a bare `İÇEAKTAR` asks for. Matching is Turkish-folded, so `İMAR`
/// from a checklist finds `imar` in the file (CLAUDE.md 5.6). A name in `only`
/// that the file does not hold is not an error — it is reported as read zero,
/// because the alternative is a wizard that refuses the whole import over one
/// stale tick. `options.drawing_unit` is the unit a DXF is read in (the setting).
command::Task<core::Result<VectorReport>> import_vector(command::Transaction& tx, std::string path,
                                                        ImportOptions options,
                                                        std::stop_token stop);

/// Writes `doc` to `path` through the named allow-listed driver, or through the
/// driver that matches the extension when `options.driver` is empty.
/// `options.crs` is the coordinate system to stamp on the output, and it must be
/// one OGR can resolve (an EPSG code, a PROJ string, a WKT). An export with no
/// usable CRS produces a file whose coordinates mean nothing to whoever receives
/// it, so it is refused rather than written (io.md R20). `options.unit` is the
/// unit a DXF's coordinates are written in; a geodetic format writes metres.
command::Task<core::Result<VectorReport>> export_vector(const core::Document& doc, std::string path,
                                                        ExportOptions options,
                                                        std::stop_token stop);

} // namespace kentos::io
