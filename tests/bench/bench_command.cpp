// SPDX-License-Identifier: GPL-3.0-or-later
// Command-path budgets from piricad.md §10.1 and §10.4.
#include "benchmark.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/script/json_runner.hpp"

#include <memory>

namespace {

using namespace piricad;
using namespace piricad::command;

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [](std::string_view) {};
    }
};

/// piricad.md §10.1: dispatching a command from a script must stay under 10 µs.
///
/// The body dispatches ONCE and the harness repeats it, which is the part that
/// used to be a hand-written `kCalls = 20000` with the per-call cost divided out
/// afterwards. The count is still FIXED rather than left to Google Benchmark, and
/// the reason is specific to this scenario: every dispatch adds an entity, so a
/// dispatch made faster would earn MORE iterations, grow the document further,
/// and report a higher per-call figure — an improvement arriving as a regression.
void dispatch_from_script(benchmark::State& state)
{
    Rig rig;

    Args args;
    args.set("noktalar", Value::points({{0, 0}, {10000, 0}}));

    // One batch around the whole run: the §10.1 figure is the dispatch cost a
    // script pays, not the cost of opening a transaction per line.
    if (auto st = rig.bus.begin_batch("bench"); !st) {
        state.SkipWithError("toplu iş açılamadı");
        return;
    }

    for (auto _ : state) {
        auto result = rig.bus.dispatch(Invocation{"core.line", args, Origin::Script});
        benchmark::DoNotOptimize(result);
    }

    (void)rig.bus.end_batch();
}

/// The command line's own share of the §10.1 30 ms keystroke budget: parsing plus
/// dispatch, without the Qt paint that completes the path.
void parse_and_dispatch(benchmark::State& state)
{
    Rig rig;

    if (auto st = rig.bus.begin_batch("bench"); !st) {
        state.SkipWithError("toplu iş açılamadı");
        return;
    }

    for (auto _ : state) {
        auto result = rig.bus.execute_line("ÇİZGİ 485320.150,4310220.400 @50,30 @100<45",
                                           Origin::CommandLine);
        benchmark::DoNotOptimize(result);
    }

    (void)rig.bus.end_batch();
}

/// §10.4: a script creating a hundred thousand objects performs exactly one
/// validation pass and writes exactly one undo record.
///
/// The script text is built once, outside the timer: composing four megabytes of
/// JSON is this test's fixture, not its subject.
void batch_100k(benchmark::State& state)
{
    std::string script = "[";
    for (int i = 0; i < 100000; ++i) {
        if (i) script += ',';
        script += R"({"cmd":"core.line","args":{"noktalar":[[)";
        script += std::to_string(i * 100);
        script += ",0],[";
        script += std::to_string(i * 100);
        script += R"(,10000]]}})";
    }
    script += "]";

    for (auto _ : state) {
        state.PauseTiming();
        auto rig    = std::make_unique<Rig>();
        auto runner = std::make_unique<script::JsonRunner>(rig->bus, script::Sandbox::Project);
        state.ResumeTiming();

        auto result = runner->run_text(script);
        benchmark::DoNotOptimize(result);

        // Tearing down a document holding a hundred thousand entities is not what
        // the §10.4 figure is about. Held by pointer so the release happens HERE,
        // inside the paused region, and not at the closing brace after it.
        state.PauseTiming();
        runner.reset();
        rig.reset();
        state.ResumeTiming();
    }
}

} // namespace

PIRICAD_BENCH(script_dispatch){bench::Case{
    .id          = "komut.betikten_gonderim",
    .title       = "Betikten komut gönderim maliyeti",
    .budget      = 10.0,
    .unit        = "µs",
    .repetitions = 5,
    .iterations  = 20000, // fixed: see dispatch_from_script
    .body        = &dispatch_from_script,
}};

PIRICAD_BENCH(cli_parse_dispatch){bench::Case{
    .id          = "komut.ayristir_ve_gonder",
    .title       = "Komut satırı: ayrıştırma + gönderim (30 ms tuş bütçesinin payı)",
    .budget      = 0.0,
    .unit        = "µs",
    .repetitions = 5,
    .iterations  = 20000,
    .body        = &parse_and_dispatch,
}};

PIRICAD_BENCH(batch_hundred_k){bench::Case{
    .id          = "komut.toplu_is_100k",
    .title       = "100k nesne yaratan betik — tek doğrulama, tek geri alma kaydı",
    .budget      = 0.0,
    .unit        = "ms",
    .repetitions = 3,
    .iterations  = 1,
    .body        = &batch_100k,
}};
