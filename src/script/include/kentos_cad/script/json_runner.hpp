// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: the Phase-0 script host.
//
// kentoscad.md §4 describes the eventual two-layer design: Lua (sol2) for the hot
// path and Python (pybind11) as an optional ecosystem module. Neither dependency
// is present in Phase 0, so the shipped host is a JSON command-array runner.
//
// It is not a placeholder for the architecture, only for the language: it already
// obeys every rule the Lua and Python hosts will obey —
//   * every mutation goes through the command bus (§4.3)
//   * the whole script is ONE undo step (§2.5)
//   * a failure rolls the whole batch back, never half-applies (§2.5)
//   * the script never receives a pointer into the document (§4.3)
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/script/host.hpp"

#include <string>
#include <string_view>

namespace kentos::script {

class JsonRunner
{
public:
    /// Builds a runner over a bus. The sandbox defaults to the SAFE level: a
    /// script from an unknown source gets no filesystem, and widening that is a
    /// deliberate act at the call site rather than a default nobody noticed.
    JsonRunner(command::Bus& bus, Sandbox sandbox = Sandbox::Safe);

    /// Runs a JSON document of the form
    ///   { "ad": "...", "komutlar": [ {"cmd": "core.line", "args": {...}}, ... ] }
    /// or a bare array of command objects.
    core::Result<RunReport> run_text(std::string_view json, std::string label = "Betik");

    /// Reads and runs a script file. Rejected unless the sandbox permits file access.
    core::Result<RunReport> run_file(const std::string& path);

    Sandbox sandbox() const noexcept { return sandbox_; }

private:
    command::Bus& bus_;
    Sandbox sandbox_;
};

/// Installs the runner as the bus's script backend, so BETİK / core.script works.
/// Keeps the dependency direction intact: script depends on command, never the
/// reverse (Constitution Article 3).
void install(command::Bus& bus, JsonRunner& runner);

} // namespace kentos::script
