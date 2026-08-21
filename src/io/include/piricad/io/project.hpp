// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — io: reading and writing the native project file.
//
// .claude/io.md R1: a public `piricad/io/` header exposes only core and command
// types. There is no GDAL, no LibreDWG and no operating-system type below; the
// memory mapping, the bounds checking and the column decoding all live in the
// module's .cpp files (R2).
//
// THE ONE THING TO KNOW ABOUT THIS FILE
//
// Neither entry point mutates a `Document` directly. The writer takes it by
// const reference; the reader takes a `command::Transaction` and every edit it
// makes goes through that. So a read that fails half-way rolls the document back
// to exactly what it was (io.md R17, P11), and Constitution Article 5.9 holds:
// nothing in /src/io reaches around a transaction.
#pragma once

#include "piricad/command/task.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/settings.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

namespace piricad::io {

/// Something the reader accepted but not exactly as the file recorded it.
///
/// model.md R42 is the rule this exists for: a value written by another version
/// is clamped and REPORTED, never silently accepted and never a hard failure that
/// makes the file unopenable. Silence would be io.md P13 — a silent repair.
struct Warning
{
    std::string code;    ///< stable token, e.g. "io.field_unsupported"
    std::string message; ///< Turkish, actionable
};

/// What a read or a write actually did. Returned rather than logged, because the
/// command that asked has to be able to tell the user.
struct ProjectReport
{
    std::uint64_t entities{0};
    std::uint64_t layers{0};
    std::uint64_t vertices{0};
    std::uint64_t bytes{0};
    std::uint32_t format_version{0};

    /// The fingerprint the WRITER recorded. The reader recomputes it after the
    /// load and compares; a mismatch is reported as a warning rather than a
    /// refusal, because a document that differs from its own stamp is still the
    /// user's work and refusing to open it destroys more than it protects.
    std::uint64_t stored_content_hash{0};
    std::uint64_t stored_settings_hash{0};

    std::vector<Warning> warnings;
};

/// True when `path` ends in the native project extension, Turkish-folded.
bool is_project_path(const std::string& path);

/// Writes `doc` plus the PROJECT-scope entries of `settings` to `path`.
///
/// The write is atomic: the bytes go to a sibling temporary and are renamed into
/// place only once they are on disk, so a crash or a full disk can never leave a
/// half-written project where the previous one was.
///
/// model.md R39 decides what travels: project-scope settings are part of the
/// document and go into the file; application and session scopes are per user and
/// per run and never do.
core::Result<ProjectReport> save_project(const core::Document& doc, const core::Settings& settings,
                                         const std::string& path);

/// Reads `path` into `tx.document()`, which MUST be a fresh, empty document.
///
/// io.md R15: the reader streams, honours `stop`, and returns within 100 ms of a
/// cancellation request. Every count, offset and length in the file is treated as
/// an untrusted hint and bounds-checked against the real file size before it is
/// used for an allocation or a seek (R18, P6).
///
/// Errors carry a stable token from `piricad/io/format.hpp` at the front of the
/// message: `io.format_too_new` for a file this build would misread (R9),
/// `io.truncated`, `io.bad_block`, `io.inconsistent`, `io.key_mismatch`.
///
/// `path` is taken BY VALUE, and every coroutine in this module is: a coroutine's
/// reference parameters are not copied into its frame, so a caller that holds the
/// task past the full-expression that made it would be reading a dangling string.
/// Taking it by value costs one allocation per open and closes that hole for good.
command::Task<core::Result<ProjectReport>> read_project(command::Transaction& tx, std::string path,
                                                        core::Settings& settings,
                                                        std::stop_token stop);

} // namespace piricad::io
