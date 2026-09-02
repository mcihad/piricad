// SPDX-License-Identifier: GPL-3.0-or-later
#include "benchmark.hpp"

#include "kentos_cad/core/json.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace bench {
namespace {

using kentos::core::Json;

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::string cpu_model()
{
#if defined(__linux__)
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        if (line.compare(0, 10, "model name") != 0) continue;
        std::string value = line.substr(colon + 1);
        const auto b      = value.find_first_not_of(" \t");
        return b == std::string::npos ? std::string{} : value.substr(b);
    }
#endif
    return "bilinmiyor";
}

const char* compiler()
{
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(_MSC_VER)
    return "msvc";
#else
    return "?";
#endif
}

const char* build_type()
{
#if defined(NDEBUG)
    return "optimize";
#else
    return "debug";
#endif
}

/// The Google Benchmark time unit a scenario's `unit` string asks for.
///
/// The scenario declares its unit in the terms the budget is written in, so the
/// number in the report and the number in Article 7 are the same number and no
/// reader has to convert one to compare them.
benchmark::TimeUnit time_unit_for(const std::string& unit)
{
    if (unit == "µs") return benchmark::kMicrosecond;
    if (unit == "s") return benchmark::kSecond;
    if (unit == "ns") return benchmark::kNanosecond;
    return benchmark::kMillisecond;
}

/// What one repetition produced, in the scenario's own unit.
double value_of(const benchmark::BenchmarkReporter::Run& run, Metric metric)
{
    if (metric == Metric::Gauge) {
        const auto it = run.counters.find(kGaugeCounter);
        return it == run.counters.end() ? 0.0 : it->second.value;
    }
    return run.GetAdjustedRealTime();
}

/// Collects every repetition Google Benchmark runs, and prints nothing.
///
/// A reporter rather than the JSON output file: the gate has to compare against a
/// baseline and an Article 7 budget in the same process that measured, and going
/// through a temporary file would add a parse step and a failure mode for no gain.
class Collector : public benchmark::BenchmarkReporter
{
public:
    bool ReportContext(const Context&) override { return true; }

    void ReportRuns(const std::vector<Run>& runs) override
    {
        for (const auto& run : runs) {
            // Google Benchmark also emits mean/median/stddev rows. They are
            // skipped: the gate's own definition of spread is max minus min, and
            // that is computed from the raw repetitions below.
            if (run.run_type != Run::RT_Iteration) continue;
            if (run.skipped) continue;
            samples_[run.run_name.function_name].push_back(run);
        }
    }

    /// The raw repetitions of one scenario, or an empty span when it did not run.
    const std::vector<Run>& runs_of(const std::string& id) const
    {
        static const std::vector<Run> none;
        const auto it = samples_.find(id);
        return it == samples_.end() ? none : it->second;
    }

private:
    // Keyed by `function_name` rather than `benchmark_name()`: the latter carries
    // the /repeats: and /iterations: suffixes Google Benchmark appends, and the
    // baseline file is keyed by the scenario id alone.
    std::map<std::string, std::vector<Run>> samples_;
};

/// A regression is only meaningful against a run on the same hardware, the same
/// compiler and the same optimisation level.
struct Report
{
    std::string id;
    std::string title;
    std::string unit;
    double budget{0};
    double value{0};
    double spread{0};
    double baseline{-1};
    bool pending{false};
    std::string pending_why;
};

} // namespace

std::vector<Case>& cases()
{
    static std::vector<Case> c;
    return c;
}

Registrar::Registrar(Case c)
{
    cases().push_back(std::move(c));
}

bool regressed(double value, double baseline, double spread)
{
    if (baseline <= 0) return false;
    return value > baseline * 1.10 && (value - baseline) > spread;
}

double resident_mb()
{
#if defined(__linux__)
    std::ifstream in("/proc/self/status");
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, 6, "VmRSS:") != 0) continue;
        return std::strtod(line.c_str() + 6, nullptr) / 1024.0;
    }
#endif
    return -1.0;
}

std::string machine_id()
{
    return cpu_model() + " | " + compiler() + " " + std::to_string(__cplusplus) + " | " +
           build_type();
}

int run_all(int argc, char** argv)
{
    // The baseline path comes from the environment, not from argv: argv belongs to
    // Google Benchmark now, so `--benchmark_filter=render.*` works while a
    // scenario is being tuned.
    const char* baseline_env = std::getenv("KENTOS_BENCH_BASELINE");
    const std::string baseline_path =
        baseline_env ? baseline_env : std::string(KENTOS_BENCH_DIR) + "/temel-degerler.json";
    const bool record = std::getenv("KENTOS_BENCH_RECORD") != nullptr;

    // ---- load the baseline, if it belongs to this machine ----
    std::string baseline_machine;
    std::vector<std::pair<std::string, double>> baseline;

    if (auto text = read_file(baseline_path); !text.empty()) {
        if (auto j = Json::parse(text); j) {
            if (const Json* m = j.value().find("makine")) baseline_machine = m->as_string();
            if (const Json* v = j.value().find("olcumler"); v && v->is_object())
                for (const auto& [k, val] : v->as_object())
                    baseline.emplace_back(k, val.as_double(-1));
        }
    }

    const std::string here = machine_id();
    const bool same        = !baseline_machine.empty() && baseline_machine == here;

    std::printf("KentOSCad — performans bütçeleri (kentoscad.md §10.1)\n");
    std::printf("makine: %s\n", here.c_str());
    if (baseline_machine.empty())
        std::printf("temel:  kayıtlı değer yok — regresyon kapısı devre dışı\n");
    else if (!same)
        std::printf("temel:  başka makinede kaydedilmiş (%s) — regresyon kapısı devre dışı\n",
                    baseline_machine.c_str());
    else
        std::printf("temel:  %zu ölçüm, regresyon kapısı açık (>%%10 kırar)\n", baseline.size());
    std::printf("\n");

    // ---- hand every implemented scenario to Google Benchmark ----
    for (const auto& c : cases()) {
        if (!c.body) continue;

        auto* registered = benchmark::RegisterBenchmark(c.id, c.body)
                               ->Unit(time_unit_for(c.unit))
                               ->Repetitions(c.repetitions)
                               // Wall clock, not CPU time: a frame budget is what
                               // the user waits for, and CPU time hides a stall
                               // that blocked on memory or on the disk.
                               ->UseRealTime();

        // A Gauge reads a number rather than timing a loop, so repeating inside
        // one repetition would measure the same reading again.
        if (c.iterations > 0 || c.metric == Metric::Gauge)
            registered->Iterations(c.metric == Metric::Gauge ? 1 : c.iterations);
    }

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 2;

    Collector collector;
    benchmark::RunSpecifiedBenchmarks(&collector);
    benchmark::Shutdown();

    // ---- gate ----
    std::vector<Report> reports;
    int budget_failures     = 0;
    int regression_failures = 0;

    for (const auto& c : cases()) {
        Report r;
        r.id     = c.id;
        r.title  = c.title;
        r.unit   = c.unit;
        r.budget = c.budget;

        const auto& runs = collector.runs_of(c.id);
        if (!c.body || runs.empty()) {
            r.pending = true;
            // An implemented scenario with no runs was filtered out by
            // --benchmark_filter, which is a deliberate act and not a gap.
            r.pending_why = c.body ? "bu koşumda süzgeç dışında kaldı" : c.pending;
            reports.push_back(std::move(r));
            continue;
        }

        std::vector<double> samples;
        samples.reserve(runs.size());
        for (const auto& run : runs)
            samples.push_back(value_of(run, c.metric));
        std::sort(samples.begin(), samples.end());

        // The median, not the mean: the first repetition pays for page faults and
        // a cold cache, and a mean would carry that into every later comparison.
        r.value  = samples[samples.size() / 2];
        r.spread = samples.back() - samples.front();

        for (const auto& [k, v] : baseline)
            if (k == c.id) r.baseline = v;

        if (c.budget > 0 && r.value > c.budget) ++budget_failures;
        if (same && regressed(r.value, r.baseline, r.spread)) ++regression_failures;

        reports.push_back(std::move(r));
    }

    std::printf("%-30s %12s %9s %9s %9s  %s\n", "senaryo", "ölçüm", "yayılım", "bütçe", "temel",
                "durum");
    std::printf("%s\n", std::string(92, '-').c_str());

    for (const auto& r : reports) {
        // Column padding is byte based, so the placeholder is ASCII: a UTF-8 dash
        // is three bytes wide and would shift the whole table.
        char budget[32];
        if (r.budget > 0)
            std::snprintf(budget, sizeof budget, "%.2f", r.budget);
        else
            std::snprintf(budget, sizeof budget, "-");

        if (r.pending) {
            std::printf("%-30s %12s %9s %9s %9s  BEKLEMEDE  %s\n", r.id.c_str(), "-", "-", budget,
                        "-", r.pending_why.c_str());
            continue;
        }

        const bool over_budget = r.budget > 0 && r.value > r.budget;
        const bool slower      = same && regressed(r.value, r.baseline, r.spread);
        const char* status     = over_budget ? "BÜTÇE AŞILDI" : (slower ? "REGRESYON" : "tamam");

        char measured[32], spread[32], base[32];
        std::snprintf(measured, sizeof measured, "%.3f %s", r.value, r.unit.c_str());
        std::snprintf(spread, sizeof spread, "+-%.3f", r.spread);
        if (r.baseline > 0)
            std::snprintf(base, sizeof base, "%.3f", r.baseline);
        else
            std::snprintf(base, sizeof base, "-");

        std::printf("%-30s %12s %9s %9s %9s  %s\n", r.id.c_str(), measured, spread, budget, base,
                    status);
    }

    std::printf("\n");
    for (const auto& r : reports)
        if (!r.pending && r.budget > 0 && r.value > r.budget)
            std::printf("  %s: %s — %.2f %s, bütçe %.2f %s (%.1f×)\n", r.id.c_str(),
                        r.title.c_str(), r.value, r.unit.c_str(), r.budget, r.unit.c_str(),
                        r.value / r.budget);

    // ---- record ----
    if (record) {
        Json measurements;
        for (const auto& r : reports)
            if (!r.pending) measurements.set(r.id, Json::number(r.value));

        Json out;
        out.set("makine", Json::string(here));
        out.set("aciklama", Json::string("KentOSCad performans temel değerleri. Yalnız aynı makinede "
                                         "regresyon kapısı olarak kullanılır (kentoscad.md §10.1)."));
        out.set("olcumler", std::move(measurements));

        std::ofstream file(baseline_path, std::ios::binary);
        file << out.dump_pretty(2) << "\n";
        std::printf("temel değerler kaydedildi: %s\n", baseline_path.c_str());
        return 0;
    }

    if (budget_failures || regression_failures) {
        std::printf("\n%d bütçe aşımı, %d regresyon.\n", budget_failures, regression_failures);
        std::printf("Bir bütçe, benchmark geçsin diye gevşetilmez (CLAUDE.md Article 7).\n");
        return 1;
    }

    std::printf("bütün bütçeler karşılandı.\n");
    return 0;
}

} // namespace bench
