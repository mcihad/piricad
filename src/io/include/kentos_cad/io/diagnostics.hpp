// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: what an import wants the user to know, in one shape.
//
// io.md P11/P13: a loss is REPORTED, never silent and never silently repaired.
// Every reader used to say it its own way — a bare count and a first reason on
// the vector path, a type→count map on the DWG path, free text everywhere — and
// the things that were lost without a word (a hatch read as a face, a Z dropped,
// a unit assumed) were lost because there was no place to say them. This is that
// place: one structure every reader fills, one rendering the command prints and
// the wizard lists.
//
// Two kinds of fact live here. NOTES are sentences a reader emits as it goes,
// capped so a broken file cannot print ten thousand lines; the cap says how many
// it dropped. FIELDS are counts the rendering derives lines from AFTER the cap,
// so the unit, the paper-space skips and the per-type census can never be hidden
// by a flood of notes.
#pragma once

#include "kentos_cad/core/units.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::io {

/// How serious one line is. The order is the order the wizard lists them in.
enum class Severity : std::uint8_t {
    Info,     ///< something happened that the user may want to know
    Warning,  ///< the reader had to assume something; the drawing may be wrong
    Degraded, ///< something was read with less fidelity than the file carried
    Skipped,  ///< something in the file was not read at all
    Error,    ///< a failure the reader survived by leaving something out
};

/// The Turkish prefix of a transcript line for the level, without the colon:
/// `not`, `uyarı`, `düşürme`, `atlandı`, `hata`. `not` is what every documented
/// transcript already shows, so an Info line reads exactly as it always did.
const char* severity_prefix(Severity level) noexcept;

/// One sentence with its level.
struct Diagnostic
{
    Severity level{Severity::Info}; ///< how serious it is
    std::string text;               ///< the sentence, Turkish, as the user reads it
};

/// One entity type's census: how many of it were read as the file meant them,
/// how many were left behind, how many were read with something lost.
struct TypeTally
{
    std::string type;          ///< the FILE's name for it: `LWPOLYLINE`, `HATCH`, `INSERT`
    std::uint64_t read{0};     ///< read into the document
    std::uint64_t skipped{0};  ///< not read at all
    std::uint64_t degraded{0}; ///< read, but with something the file carried lost
};

/// Where the drawing unit the coordinates were scaled by came from.
///
/// A drawing format is ALWAYS read in the project's unit. The file's own header
/// (`$INSUNITS`) is compared to it and reported in `declared_unit`, never
/// obeyed: it is set by whoever last saved the file, and Turkish cadastral
/// drawings routinely say "millimetres" over numbers that are plainly metres.
/// Obeying one once shrank a whole town to eight metres across and rounded
/// every circle in it to a blob.
enum class UnitSource : std::uint8_t {
    Unknown, ///< the reader did not decide (nothing read yet)
    Setting, ///< a drawing format: `core.cizim.birim` decided
    Crs,     ///< a geodetic format: the unit is the coordinate system's metre
};

/// Everything an import has to say that is not its result.
struct ImportDiagnostics
{
    /// How many free-text notes are kept. Eight is what a person reads on a
    /// transcript; the rest are counted, not lost.
    static constexpr std::size_t kMaxNotes = 8;

    std::vector<TypeTally> types; ///< sorted by type name, so two runs print alike

    core::DrawingUnit unit{core::DrawingUnit::Metre}; ///< what the coordinates were scaled by
    UnitSource unit_source{UnitSource::Unknown};

    /// What the file's own header claimed, when it claimed a unit this program
    /// knows; nothing for a file that says nothing (`$INSUNITS` absent or 0) or
    /// names a unit outside the table. Reported beside `unit`: a Warning when
    /// the two disagree, so the fix is one line away.
    std::optional<core::DrawingUnit> declared_unit;

    std::uint64_t paper_space_skipped{0}; ///< entities on a layout, not in the model

    /// Features the reader could not turn into geometry, and the first reason.
    /// A field, not a note, so the count survives the cap.
    std::uint64_t skipped{0};
    std::string skipped_reason;

    std::vector<Diagnostic> notes;  ///< in emission order, at most `kMaxNotes`
    std::uint64_t dropped_notes{0}; ///< how many `note()` refused past the cap

    /// Appends a note, or counts it when the cap is reached.
    void note(Severity level, std::string text);

    /// Adds to one type's census, creating the row on first sight.
    void tally(std::string_view type, std::uint64_t read, std::uint64_t skipped,
               std::uint64_t degraded);

    /// Folds another reader's diagnostics into this one (a probe over several
    /// layers, a merge after a worker thread). Notes beyond the cap are counted.
    void merge(const ImportDiagnostics& other);

    /// True when there is nothing at all to say.
    bool empty() const noexcept;

    /// Every line, notes first in emission order, then the lines the fields
    /// produce (unit, paper space, skipped features, the type census), then
    /// nothing about dropped notes — `transcript()` adds that tail itself.
    std::vector<Diagnostic> lines() const;

    /// The same lines, stable-sorted with the most serious first: what the
    /// wizard lists.
    std::vector<Diagnostic> ordered() const;

    /// The transcript tail: one `"\n  <prefix>: <text>"` per line, then
    /// `"\n  … ve N not daha."` when notes were dropped. Empty when `empty()`.
    std::string transcript() const;

    /// The census in one sentence — `Okunan türler: LWPOLYLINE 4021, LINE 1200;
    /// parçalanan: ELLIPSE 3; atlanan: DIMENSION 27` — or empty when no type
    /// was counted. At most eight types per group are named; the rest are a
    /// count.
    std::string type_summary() const;
};

} // namespace kentos::io
