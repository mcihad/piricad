// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — command: the sample projects under /data/ornekler (TODOS U-06).
//
// A new user's first question is "where do I start", and the honest answer is a
// drawing that is already half done: five small jobs — a map from survey points,
// a parcel layout, a zoning plan, a GIS analysis and a stakeout — each one a JSON
// command script (docs/betik) plus a line or two of what to try next. The scripts
// are DATA (CLAUDE.md 3.5): the program has no drawing of its own in it, and the
// index below is read at the moment it is asked for, the way a catalogue is.
//
// This header is Qt-free so `ÖRNEKPROJE` and the application menu read the same
// list: a menu entry that named a project the command could not find would be two
// answers to one question (CLAUDE.md 5.10).
#pragma once

#include "piricad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::command {

/// One thing to try once the project is open: a command line, exactly as it is
/// typed, and the sentence that says why.
struct SampleStep
{
    std::string command;     ///< a line for the command line; runs as printed
    std::string description; ///< Turkish, one sentence
};

/// One sample project.
struct Sample
{
    std::string id;                ///< `olcuden-harita`; ASCII, stable
    std::string title;             ///< `Ölçüden harita`
    std::string tagline;           ///< one short line (about 45 characters) for a menu row
    std::string summary;           ///< what the project is, in a sentence or two
    std::string script;            ///< the JSON script's path, resolved against the data directory
    std::string layout;            ///< the sheet the project ends in; empty when it has none
    std::int64_t scale{0};         ///< that sheet's 1:N; 0 when it has no sheet
    std::vector<SampleStep> steps; ///< what to try next, in order
};

/// The index: the projects in file order.
struct SampleCatalog
{
    std::string package_version; ///< the index file's own version
    std::vector<Sample> samples; ///< in file order

    /// The project whose id or title is `name`, matched with the Turkish folding
    /// every command name uses; null when none is.
    const Sample* find(std::string_view name) const noexcept;
};

/// Reads `data/ornekler/ornekler.json` from wherever the data directory is
/// (`resolve_catalog_path`). Fails with a message that names the file and the way
/// out — `PIRICAD_DATA` — when the package is missing or malformed.
core::Result<SampleCatalog> load_samples();

} // namespace piricad::command
