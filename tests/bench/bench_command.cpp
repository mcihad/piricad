// SPDX-License-Identifier: GPL-3.0-or-later
// Command-path budgets from piricad.md §10.1 and §10.4.
#include "benchmark.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/parser.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/script/json_runner.hpp"

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
/// Measured over a batch so the timer resolution does not dominate.
double dispatch_from_script()
{
    Rig rig;

    Args args;
    args.set("noktalar", Value::points({{0, 0}, {10000, 0}}));

    constexpr int kCalls = 20000;

    if (auto st = rig.bus.begin_batch("bench"); !st) return 1e9;
    const auto start = bench::Clock::now();
    for (int i = 0; i < kCalls; ++i)
        (void)rig.bus.dispatch(Invocation{"core.line", args, Origin::Script});
    const double elapsed_ms = bench::since(start);
    (void)rig.bus.end_batch();

    return elapsed_ms * 1000.0 / kCalls; // µs per dispatch
}

/// The command line's own share of the §10.1 30 ms keystroke budget: parsing plus
/// dispatch, without the Qt paint that completes the path.
double parse_and_dispatch()
{
    Rig rig;
    constexpr int kCalls = 20000;

    if (auto st = rig.bus.begin_batch("bench"); !st) return 1e9;
    const auto start = bench::Clock::now();
    for (int i = 0; i < kCalls; ++i)
        (void)rig.bus.execute_line("ÇİZGİ 485320.150,4310220.400 @50,30 @100<45",
                                   Origin::CommandLine);
    const double elapsed_ms = bench::since(start);
    (void)rig.bus.end_batch();

    return elapsed_ms * 1000.0 / kCalls;
}

/// §10.4: a script creating a hundred thousand objects performs exactly one
/// validation pass and writes exactly one undo record.
double batch_100k()
{
    Rig rig;

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

    script::JsonRunner runner(rig.bus, script::Sandbox::Project);

    const auto start = bench::Clock::now();
    (void)runner.run_text(script);
    return bench::since(start);
}

} // namespace

PIRICAD_BENCH(script_dispatch){bench::Case{
    .id     = "komut.betikten_gonderim",
    .title  = "Betikten komut gönderim maliyeti",
    .budget = 10.0,
    .unit   = "µs",
    .runs   = 5,
    .run    = &dispatch_from_script,
}};

PIRICAD_BENCH(cli_parse_dispatch){bench::Case{
    .id     = "komut.ayristir_ve_gonder",
    .title  = "Komut satırı: ayrıştırma + gönderim (30 ms tuş bütçesinin payı)",
    .budget = 0.0,
    .unit   = "µs",
    .runs   = 5,
    .run    = &parse_and_dispatch,
}};

PIRICAD_BENCH(batch_hundred_k){bench::Case{
    .id     = "komut.toplu_is_100k",
    .title  = "100k nesne yaratan betik — tek doğrulama, tek geri alma kaydı",
    .budget = 0.0,
    .unit   = "ms",
    .runs   = 3,
    .run    = &batch_100k,
}};
