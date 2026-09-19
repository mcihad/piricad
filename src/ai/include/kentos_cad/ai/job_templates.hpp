// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — ai: the shape of a job template. The templates themselves are DATA.
//
// WHAT A TEMPLATE IS AND IS NOT. It is a named, versioned sequence of COMMAND
// LINES with the places a caller fills in marked. It is not a macro, not a
// script host and not a fast path: nothing here runs anything. A client asks for
// a template, substitutes its parameters and sends the resulting lines through
// the ordinary tool surface — where a write still becomes a suggestion a person
// applies (CLAUDE.md 5.7).
//
// WHY IT EXISTS. An atlas is six commands in a particular order, and the order is
// the part nobody can guess: place the sheet, put a map frame on it, aim the
// atlas at a layer, check the sheet, then print. An agent that has to rediscover
// that sequence rediscovers it differently every time, and a surveyor reading the
// third variant cannot tell which one is right (TODOS M-09).
//
// AND THE TEMPLATES ARE `/data`, NOT C++ (CLAUDE.md 3.5). A better way to lay out
// an atlas is a data release, not a rebuild. This header is the SHAPE only; the
// file is `data/catalogs/ai/is-sablonlari.json`, checked against
// `data/catalogs/schema/is-sablonlari.schema.json`, and the application reads it
// and hands the text here — `/src/ai` does no I/O (ai.md P10).
//
// EVERY STEP IS A REAL COMMAND LINE, and that is enforced rather than hoped:
// `tests/unit/test_ai_tools.cpp` parses every step of every shipped template
// against the live registry, so a template that names a command that does not
// exist, or an argument it does not take, breaks the build. A template full of
// plausible-looking lines that do not run would be worse than no template.
#pragma once

#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::ai {

/// One blank a caller fills in.
struct JobParam
{
    std::string name;    ///< the placeholder's name, written `<ad>` in a step
    std::string help;    ///< what it is, in Turkish
    std::string example; ///< a value that would work, so an agent has a shape
    bool required{true}; ///< whether a step is meaningless without it
};

/// One job, as a sequence of command lines.
struct JobTemplate
{
    std::string id;      ///< stable, lowercase, hyphenated: `atlas-pafta`
    std::string title;   ///< the Turkish name a person reads
    std::string summary; ///< one sentence: what this job produces

    /// The template's own version, independent of the package's.
    ///
    /// A CLIENT MAY HAVE CACHED THE STEPS. Saying which version it holds is how
    /// it learns that the recommended order changed — and why the version is per
    /// template rather than only per package: bumping one job should not tell
    /// every client that all three changed.
    std::string version;

    /// What it needs, in the order a caller is asked for them.
    std::vector<JobParam> params;

    /// The command lines, in the order they run. `<ad>` marks a parameter.
    std::vector<std::string> steps;

    /// What the caller should know before running it: which steps write, what
    /// the result is, what it does NOT do.
    std::string notes;

    /// Substitutes `values` into the steps. A parameter with no value is left as
    /// `<ad>` rather than blanked, so a half-filled template reads as unfinished
    /// instead of producing a command line with an empty argument.
    std::vector<std::string> render(const core::Json& values) const;

    /// What a client is told about this template.
    core::Json to_json(bool with_steps) const;
};

/// The shipped templates, as one versioned package.
struct JobTemplateCatalog
{
    std::int64_t schema_version{0};     ///< the file format's version, not the content's
    std::string package_version;        ///< the package's own version, as `data.md` requires
    std::vector<JobTemplate> templates; ///< in file order, which is the order a list shows

    /// The template with this id, or null.
    const JobTemplate* find(std::string_view id) const;

    /// Reads a catalogue from the file's text. The application supplies the
    /// bytes; this module never opens a file.
    static core::Result<JobTemplateCatalog> from_json(std::string_view text);
};

/// The package's path under `/data`, as the settings and the manual print it.
inline constexpr const char* kJobTemplatePath = "catalogs/ai/is-sablonlari.json";

} // namespace kentos::ai
