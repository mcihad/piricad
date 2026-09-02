// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: the sandbox level, shared by every script host.
//
// kentoscad.md §4.3 gives a script exactly three levels and no fourth:
//
//   güvenli  no filesystem, no network. THE DEFAULT, and the level a script from
//            an unknown source runs at.
//   proje    the project directory and nothing outside it.
//   tam      everything, and only after the user has said so about THIS script.
//
// It lives in its own header because it belongs to the CONCEPT of running a
// script and not to any one host: the JSON runner obeys it today and the Lua
// runner obeys it now, and a second copy of the enum would be a second thing to
// keep in step with §4.3 (CLAUDE.md 5.10 makes that objection about lists, and it
// is the same objection).
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace kentos::command {
/// The command bus; declared rather than included, because this header is about
/// the sandbox and only `journal_run` below needs the type.
class Bus;
} // namespace kentos::command

namespace kentos::script {

/// What a script is permitted to touch (`.claude/script.md` R11).
enum class Sandbox : std::uint8_t {
    Safe,    ///< güvenli — no filesystem, no network. Default.
    Project, ///< proje   — project directory only
    Full,    ///< tam     — requires explicit user consent, never granted implicitly
};

/// The Turkish name, which is what the user types and what the journal records.
std::string_view sandbox_name(Sandbox level) noexcept;

/// The level a Turkish name asks for. Folded with the shared Turkish table, so
/// `guvenli` finds `güvenli` exactly as a command name folds (CLAUDE.md 5.6).
core::Result<Sandbox> sandbox_from_name(std::string_view name);

/// The identity of a script, for the consent record of `.claude/script.md` R12.
///
/// A hash of the script TEXT, not of its path: consent is given to a script, and
/// a file that changed after the user approved it is a different script. FNV-1a
/// from `core::text.hpp` rather than a new one — CLAUDE.md 5.16.
std::uint64_t script_identity(std::string_view text) noexcept;

/// Writes the `{kind:"meta"}` journal line R11 requires for a script run.
///
/// Every run, at every level, including the ones that fail: the record says what
/// was PERMITTED, and a run that was allowed the filesystem and then errored is
/// exactly the run an audit wants to see. `consented` is meaningful only at `tam`
/// and is what R12 asks to be recorded with the identity hash.
void journal_run(command::Bus& bus, std::string_view host, std::string_view label, Sandbox level,
                 std::uint64_t identity, bool consented);

} // namespace kentos::script
