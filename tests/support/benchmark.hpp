// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the Article 7 budget harness, on Google Benchmark.
//
// TWO JOBS, and keeping them apart is the whole design.
//
//   GOOGLE BENCHMARK MEASURES. It picks the iteration count so a sub-microsecond
//   operation is not timed against the clock's own resolution, keeps the timer
//   out of the setup, runs repetitions, and knows what this machine's clock
//   costs. That is a solved problem, and CLAUDE.md Article 2.7 says a solved
//   problem is not solved again. What it replaces is a hand-rolled `measure()`
//   that did its own warm-up, its own loop and its own median — and, in the
//   command benchmarks, a literal `kCalls = 20000` written into the body because
//   the author had to guess how many repetitions the timer needed.
//
//   THE GATE IS OURS, because no library knows that 16 ms is a product
//   requirement rather than a measurement:
//
//     BÜTÇE — the absolute Article 7 numbers. Always checked, on every machine.
//     TEMEL — the last measurement on THIS machine. Checked only when the
//             recorded machine matches, because an absolute baseline copied
//             between machines measures the machine and not the code.
//
// A scenario with no implementation reports BEKLEMEDE and never PASS: a budget
// that quietly vanishes from the report is a budget nobody is accountable for.
#pragma once

#include <benchmark/benchmark.h>

#include <functional>
#include <string>
#include <vector>

namespace bench {

/// What a scenario produces.
enum class Metric {
    /// A DURATION, timed by Google Benchmark around the body's loop. Almost
    /// everything here is this.
    Time,
    /// A READING taken at one moment — resident memory, a file size. There is no
    /// loop to time; the body takes the reading and writes it to the counter
    /// named by `kGaugeCounter`.
    Gauge,
};

/// The Google Benchmark counter a Gauge scenario reports through.
///
/// One fixed name rather than the unit string, so the reporter can find the value
/// without knowing what the scenario measures.
inline constexpr const char* kGaugeCounter = "deger";

/// One scenario: what it measures, what it is allowed to cost, and the body.
struct Case
{
    /// Stable id, also the Google Benchmark name and the baseline file's key.
    /// Renaming one silently discards its recorded baseline.
    std::string id;

    /// Turkish, shown in the report. The id is for machines; this is for people.
    std::string title;

    /// The Article 7 limit in `unit`; 0 means informational — measured, reported
    /// and guarded against regression, but with no absolute number to fail.
    ///
    /// Zero is used where piricad.md §10.1 states no figure. Inventing one would
    /// produce a threshold nobody derived, and the first time it failed somebody
    /// would raise it — which test.md P7 forbids outright.
    double budget{0};

    /// `ms`, `µs`, `s` for a Time case; anything for a Gauge (`MB`).
    std::string unit;

    /// How many independent repetitions to run. The median is reported.
    int repetitions{5};

    /// Iterations inside one repetition, or 0 to let Google Benchmark choose.
    ///
    /// Choose 1 when a repetition CANNOT be repeated without changing what is
    /// measured: a body that mutates its fixture, that touches the filesystem, or
    /// that costs a second on its own. Everything else is left to the library,
    /// which is the point of using it.
    int iterations{0};

    /// Time or gauge.
    Metric metric{Metric::Time};

    /// The body, in Google Benchmark's own form. Null means the scenario is not
    /// implemented and reports BEKLEMEDE.
    std::function<void(benchmark::State&)> body{};

    /// Why it is not implemented, printed beside it. Only for a null body.
    std::string pending{};
};

/// Every registered scenario, in registration order.
std::vector<Case>& cases();

/// Registers one scenario at static-initialisation time.
struct Registrar
{
    explicit Registrar(Case c);
};

/// A regression is a slowdown that is BOTH more than 10% and larger than what
/// this run's own repetitions varied by. Either test alone produces false alarms:
/// 10% of a sub-microsecond measurement is smaller than the scheduler's jitter,
/// and gating on it reports noise as a regression.
bool regressed(double value, double baseline, double spread);

/// Resident set size in MB, or -1 where the platform is not supported.
double resident_mb();

/// Identity of the machine a baseline was recorded on. A baseline recorded
/// elsewhere is reported but never used as a gate.
std::string machine_id();

/// Runs every scenario, prints the Turkish report, and returns the process exit
/// code: non-zero when a budget is exceeded or a regression is confirmed.
int run_all(int argc, char** argv);

} // namespace bench

/// Declares one scenario. The symbol only has to be unique within its file.
#define PIRICAD_BENCH(sym) static const ::bench::Registrar piricad_bench_##sym
