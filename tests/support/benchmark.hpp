// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — the benchmark harness.
//
// piricad.md §10.1: "CI'da benchmark kapısı kurun; %10'dan fazla regresyon build'i
// kırsın." Two different things are checked here and they must not be confused:
//
//   BÜTÇE     — the absolute §10.1 numbers. A product requirement. Always checked.
//   TEMEL     — the last recorded measurement on THIS machine. A regression guard.
//               Only checked when the recorded machine matches, because an absolute
//               baseline copied between machines measures the machine, not the code.
//
// A scenario with no implementation reports BEKLEMEDE and never PASS. Google
// Benchmark replaces this harness when the dependency set lands (piricad.md §9.11).
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace bench {

using Clock = std::chrono::steady_clock;

/// Milliseconds since `start`, as a double.
inline double since(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Case
{
    std::string id;                ///< "render.pan_zoom_5m" — stable, used as the baseline key
    std::string title;             ///< Turkish, user-facing in the report
    double budget{0};              ///< §10.1 limit in `unit`; 0 = informational only
    std::string unit;              ///< "ms", "µs", "s", "MB"
    int runs{5};                   ///< the median is reported, not the mean
    std::function<double()> run{}; ///< nullptr = not implemented yet, reports BEKLEMEDE
    std::string pending{};         ///< why it is not implemented, shown in the report
};

inline std::vector<Case>& cases()
{
    static std::vector<Case> c;
    return c;
}

struct Registrar
{
    explicit Registrar(Case c) { cases().push_back(std::move(c)); }
};

struct Measurement
{
    double median{0}; ///< reported value; far more stable than the mean
    double spread{0}; ///< max − min across the samples: this run's own noise
};

/// Runs one case `runs` times. The spread is kept because a relative threshold
/// alone is useless: 10% of a sub-microsecond measurement is smaller than the
/// scheduler's jitter, and gating on it reports noise as a regression.
inline Measurement measure(const Case& c)
{
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(c.runs));

    c.run(); // warm-up, discarded: first touch pays for page faults
    for (int i = 0; i < c.runs; ++i)
        samples.push_back(c.run());

    std::sort(samples.begin(), samples.end());
    return Measurement{samples[samples.size() / 2], samples.back() - samples.front()};
}

/// A regression is a slowdown that is BOTH more than 10% and larger than what
/// this run's own samples varied by. Either test alone produces false alarms.
inline bool regressed(double value, double baseline, double spread)
{
    if (baseline <= 0) return false;
    return value > baseline * 1.10 && (value - baseline) > spread;
}

/// Keeps a computed value alive so the optimiser cannot delete the work that
/// produced it. A benchmark measuring a call the compiler removed measures nothing.
inline volatile double keep_sink = 0.0;

inline void keep(double value)
{
    keep_sink = value;
}

/// Resident set size in MB, or -1 where the platform is not supported.
double resident_mb();

/// Identity of the machine a baseline was recorded on. A baseline recorded
/// elsewhere is reported but never used as a gate.
std::string machine_id();

int run_all(int argc, char** argv);

} // namespace bench

#define PIRICAD_BENCH(sym) static const ::bench::Registrar piricad_bench_##sym
