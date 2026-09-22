// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — script: the Python host (kentoscad.md §4.2, `.claude/script.md`).
//
// A LANGUAGE AND NOTHING ELSE. `script.md` R3 says a host "MUST add a language
// and nothing else", and this file is that sentence kept: the same `Bus`, the
// same one grammar for command text, one batch per run, the same `RunReport`,
// the same `Sandbox` enum, the same `journal_run` record. What this host adds
// over the JSON runner beside it is a language — not an architecture.
//
// THIS IS THE ONE EMBEDDED LANGUAGE. An embedded Lua stood here until the
// amendment that removed it; §4.1's split between a Lua hot path and a Python
// ecosystem layer is gone, and the hot path it named — style rules, label
// expressions, area calculators, everything that runs PER FEATURE — is served by
// the built-in expression engine in `command/parser.hpp`, which is compiled,
// deterministic and already the one grammar (CLAUDE.md 5.11). Python is what a
// second language was ever wanted for: plugins, batch jobs and data pipelines.
//
// A PER-OBJECT EVALUATOR WRITTEN IN PYTHON IS STILL A DEFECT, and now for a
// sharper reason than before: there is no faster script language behind it to
// fall back on. A cadastral sheet has millions of features; a Python call per
// feature is the wrong shape whatever the budget, and the expression engine is
// the answer (`script.md` R5, P1).
//
// THE API IS ENGLISH, and that is a deliberate departure from Article 2.6's
// Turkish-first rule for user-facing names. A Python module is read and written
// by programmers in the language every Python library is written in, and a
// half-Turkish API (`cad.line(noktalar=...)`) is the worst of both. The Turkish
// names remain what they always were: what a user types at the command line and
// what the manual teaches.
//
// WHAT A SCRIPT CAN DO, and the write half of it is one function (§4.3):
//
//   cad.run("ÇİZGİ 0,0 10,10")   the ONE write path. Everything that mutates the
//                                document goes through the bus, so undo,
//                                validation and journalling happen whether the
//                                script asked for them or not (R1).
//
// The per-command surface — `cad.line(points=...)` for all of them — is PROJECTED
// FROM `Registry` rather than written here, because a hand-written wrapper per
// command is exactly the "binding table" CLAUDE.md 5.10 forbids.
//
// THE SANDBOX MEANS SOMETHING DIFFERENT HERE, and saying so is the honest thing.
// The host this replaced could implement `güvenli` by never opening whole
// libraries: no `io`, no `os`, no `package`. CPython cannot be treated that way —
// `os`, `socket`, `subprocess`, `ctypes` and `__import__` are all in the one
// interpreter, and CPython's own documentation says a Python sandbox is not
// achievable. So for this host the level governs what the BINDINGS open, what the
// interpreter is configured to see, and whether the user consented — not a jail
// the interpreter is locked inside. A rule that claimed otherwise would be a rule
// offering a guarantee the runtime cannot keep.
//
// PYBIND11 IS NOT IN THIS HEADER. The binding library sits behind a pimpl, so
// nothing that includes this pays for `<pybind11/embed.h>` — which drags in
// `Python.h` and every macro CPython defines — and swapping it would touch one
// .cpp (CLAUDE.md Article 9).
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/script/host.hpp"
#include "kentos_cad/script/json_runner.hpp"
#include "kentos_cad/script/sandbox.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

namespace kentos::script {

class PythonRunner
{
public:
    /// Builds a runner over a bus. The sandbox defaults to SAFE for the reason
    /// the JSON runner's does: a script from an unknown source gets no
    /// filesystem, and widening that is a deliberate act at the call site.
    explicit PythonRunner(command::Bus& bus, Sandbox sandbox = Sandbox::Safe);

    /// Closes the runner. THE INTERPRETER OUTLIVES IT, on purpose.
    ///
    /// CPython is a process-wide singleton with static state in every extension
    /// module ever imported, and finalising it to start another one is documented
    /// as only partly supported. So the interpreter starts on first use and stays
    /// up for the life of the process, and two runners — a test fixture beside
    /// the application's own — share it rather than fighting over it. The
    /// reasoning is written out in full at `ensure_interpreter` in the .cpp.
    ~PythonRunner();

    PythonRunner(const PythonRunner&)            = delete;
    PythonRunner& operator=(const PythonRunner&) = delete;

    /// The project directory, which is the whole of what `proje` permits.
    ///
    /// Empty means "no project", and at `proje` that denies every path rather
    /// than permitting every path — a jail with no walls is not a jail (P8).
    void set_project_root(std::string path);

    /// Records the user's consent for ONE script, by its identity hash.
    ///
    /// `tam` runs only after this, and only for the script whose text hashes to
    /// `identity` (R12). Nothing auto-grants it and nothing escalates into it:
    /// there is no setting, environment variable, CLI flag or script header that
    /// reaches this function — a human answering a question does (P7).
    void grant_full(std::uint64_t identity);

    /// Runs a Python source string. `token` cancels it; the interpreter changes
    /// hands at least every 5 ms, so a requested stop is not advisory (R13).
    core::Result<RunReport> run_text(std::string_view source, std::string label = "Betik",
                                     std::stop_token token = {});

    /// Reads and runs a script file. Refused unless the sandbox permits the path.
    core::Result<RunReport> run_file(const std::string& path, std::stop_token token = {});

    Sandbox sandbox() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Installs the runner as the bus's script backend, so BETİK / core.script runs a
/// `.py` file. Keeps the dependency direction intact: script depends on command,
/// never the reverse (Constitution Article 3).
void install(command::Bus& bus, PythonRunner& runner);

/// Installs BOTH hosts behind one `BETİK`, chosen by file extension: `.py` runs
/// through Python, everything else through the JSON runner.
///
/// The EXTENSION and not a flag, a setting or a second command: a user with two
/// script files should not have to tell the program which language each one is
/// in — the file already says. A second command would also be a second thing to
/// document, to expose to an agent and to keep in step (CLAUDE.md 5.10).
void install(command::Bus& bus, JsonRunner& json, PythonRunner& python);

} // namespace kentos::script
