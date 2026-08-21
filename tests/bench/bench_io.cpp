// SPDX-License-Identifier: GPL-3.0-or-later
// Native project format budgets.
//
// piricad.md §10.1 gives no explicit number for `.pcad`, so these are recorded as
// informational (`budget = 0`) rather than invented: a gate whose threshold nobody
// derived is a gate that will be relaxed the first time it fails, and test.md P7
// forbids relaxing a budget to make a bench pass.
//
// What they DO defend is the regression guard: the harness compares against this
// machine's recorded baseline, so a change that makes saving twice as slow breaks
// the build even without an absolute number (test.md R7, R8).
//
// The scale is chosen from §10.1's own worst case — the five-million-polygon
// cadastral layer — divided down to something a laptop can run in a second. The
// per-entity cost is what matters and it is linear in this format.
#include "benchmark.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/io/service.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace piricad;
using namespace piricad::command;

namespace fs = std::filesystem;

constexpr int kParcels = 20000;

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [](std::string_view) {};
    }
};

/// A grid of closed four-corner parcels, built through the bus like everything
/// else (test.md P9). One batch, so the fixture cost is not the undo stack's.
void fill(Rig& rig, int parcels)
{
    (void)rig.bus.begin_batch("bench fixture");
    for (int i = 0; i < parcels; ++i) {
        const core::Mm x = 485000000 + (i % 200) * 60000;
        const core::Mm y = 4310000000 + (i / 200) * 45000;

        Args args;
        args.set("noktalar",
                 Value::points(
                     {{x, y}, {x + 50000, y}, {x + 50000, y + 40000}, {x, y + 40000}, {x, y}}));
        (void)rig.bus.dispatch(Invocation{"core.line", args, Origin::Batch});
    }
    (void)rig.bus.end_batch();
}

fs::path scratch()
{
    return fs::temp_directory_path() / "piricad-bench-io.pcad";
}

double save_project()
{
    Rig rig;
    fill(rig, kParcels);

    const auto path  = scratch();
    const auto start = bench::Clock::now();
    (void)rig.bus.execute_line("FARKLIKAYDET \"" + path.string() + "\"", Origin::Batch);
    const double ms = bench::since(start);

    std::error_code ec;
    fs::remove(path, ec);
    return ms;
}

double open_project()
{
    const auto path = scratch();
    {
        Rig source;
        fill(source, kParcels);
        (void)source.bus.execute_line("FARKLIKAYDET \"" + path.string() + "\"", Origin::Batch);
    }

    Rig target;
    const auto start = bench::Clock::now();
    (void)target.bus.execute_line("AÇ \"" + path.string() + "\"", Origin::Batch);
    const double ms = bench::since(start);

    std::error_code ec;
    fs::remove(path, ec);
    return ms;
}

} // namespace

PIRICAD_BENCH(pcad_save){bench::Case{
    .id     = "io.pcad_20k_kaydet",
    .title  = "20k parselli projeyi kaydetme",
    .budget = 0, // informational: §10.1 sets no number for the native format
    .unit   = "ms",
    .runs   = 3,
    .run    = &save_project,
}};

PIRICAD_BENCH(pcad_open){bench::Case{
    .id     = "io.pcad_20k_ac",
    .title  = "20k parselli projeyi açma",
    .budget = 0,
    .unit   = "ms",
    .runs   = 3,
    .run    = &open_project,
}};
