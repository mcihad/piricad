// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — script: the Lua host (piricad.md §4.1–§4.3, `.claude/script.md`).
//
// WHY LUA AND NOT PYTHON. §4.1 fixes the layer roles and they are not
// interchangeable: every expression evaluator, style rule, label expression and
// area calculator is Lua, because those run PER FEATURE and a cadastral sheet has
// millions of them. Python is the ecosystem layer — plugins, batch jobs, data
// pipelines — and a hot-path evaluator written in it is a defect
// (`.claude/script.md` R5, P1).
//
// WHAT A SCRIPT CAN DO, and it is a short list on purpose (§4.3):
//
//   h.komut("ÇİZGİ 0,0 10,10")   the ONE write path. Everything that mutates the
//                                document goes through the bus, so undo,
//                                validation and journalling happen whether the
//                                script asked for them or not (R1).
//   h.katmanlar(), h.ayar(id)…   reads, and they return VALUES. No binding hands
//                                back a handle into the document — no reference,
//                                no pointer, no iterator, no non-const span
//                                (R9, R10, P4).
//
// The command text is parsed by `piricad/command/parser.hpp` — the same grammar
// object the command line uses, never a second one (R4, P5). `h.komut` is a thin
// wrapper over `Bus::execute_line`, which is what the CLI widget calls.
//
// ONE SCRIPT IS ONE UNDO STEP (R14). The whole body runs inside one batch: a
// failure anywhere rolls the entire block back, because a half-applied ifraz is a
// build-blocking defect (CLAUDE.md 1.6).
//
// SOL2 IS NOT IN THIS HEADER. The binding library is an implementation detail
// behind a pimpl, so nothing that includes this pays for `<sol/sol.hpp>` and
// swapping it would touch one .cpp (CLAUDE.md Article 9).
#pragma once

#include "piricad/command/bus.hpp"
#include "piricad/core/result.hpp"
#include "piricad/script/host.hpp"
#include "piricad/script/json_runner.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

namespace piricad::script {

class LuaRunner
{
public:
    /// Builds a runner over a bus. The sandbox defaults to SAFE for the reason
    /// the JSON runner's does: a script from an unknown source gets no
    /// filesystem, and widening that is a deliberate act at the call site.
    explicit LuaRunner(command::Bus& bus, Sandbox sandbox = Sandbox::Safe);

    /// Closes the interpreter. Declared here and defined in the .cpp because the
    /// pimpl is incomplete at this point.
    ~LuaRunner();

    LuaRunner(const LuaRunner&)            = delete;
    LuaRunner& operator=(const LuaRunner&) = delete;

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

    /// Runs a Lua chunk. `token` cancels it; the interpreter polls at least every
    /// 50 ms and a requested stop is not advisory (R13, P11).
    core::Result<RunReport> run_text(std::string_view lua, std::string label = "Betik",
                                     std::stop_token token = {});

    /// Reads and runs a script file. Refused unless the sandbox permits the path.
    core::Result<RunReport> run_file(const std::string& path, std::stop_token token = {});

    Sandbox sandbox() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Installs the runner as the bus's script backend, so BETİK / core.script runs a
/// `.lua` file. Keeps the dependency direction intact: script depends on command,
/// never the reverse (Constitution Article 3).
void install(command::Bus& bus, LuaRunner& runner);

/// Installs BOTH hosts behind one `BETİK`, chosen by file extension: `.lua` runs
/// through Lua, everything else through the JSON runner.
///
/// The EXTENSION and not a flag, a setting or a second command, because a user
/// with two script files should not have to tell the program which language each
/// one is in — the file already says. A second command would also be a second
/// thing to document, to expose to the AI and to keep in step (CLAUDE.md 5.10).
void install(command::Bus& bus, JsonRunner& json, LuaRunner& lua);

} // namespace piricad::script
