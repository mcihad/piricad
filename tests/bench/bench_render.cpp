// SPDX-License-Identifier: GPL-3.0-or-later
// Render budgets from kentoscad.md §10.1.
#include "benchmark.hpp"
#include "fixtures.hpp"

#include "kentos_cad/core/spatial_index.hpp"
#include "kentos_cad/render/scene.hpp"

#include <array>
#include <memory>

namespace {

using namespace kentos;

/// The 5M fixture costs half a second to build, so it is built once and shared by
/// every render scenario — and, through `bench::cadastral_5m()`, by the snap and
/// selection scenarios too.
struct Cadastral5M
{
    core::Document& doc;
    render::ViewTransform view;
    render::DrawList draw;
    render::SceneOptions options;
    core::Box2 extent;

    Cadastral5M() : doc(bench::cadastral_5m())
    {
        extent = doc.extent();
        view.set_viewport(1920, 1080);
    }
};

Cadastral5M& cadastral()
{
    static Cadastral5M fixture;
    return fixture;
}

/// Working zoom: an operator inspecting a handful of parcels. This is the case
/// §10.1 budgets at 16 ms, and the one culling has to make cheap.
void pan_zoom_working(benchmark::State& state)
{
    auto& f = cadastral();
    f.view.set_centre(f.extent.centre(), 100.0); // ~100 mm/px, about 190 m across

    for (auto _ : state) {
        render::build_scene(f.doc, f.view, f.options, f.draw);
        benchmark::DoNotOptimize(f.draw);
    }
}

/// Whole layer on screen. Informational: at this zoom precomputed LOD tiles carry
/// the load, and those are a Phase-1 deliverable (kentoscad.md §10.3).
void pan_zoom_full(benchmark::State& state)
{
    auto& f = cadastral();
    f.view.fit(f.extent, 0.02);

    for (auto _ : state) {
        render::build_scene(f.doc, f.view, f.options, f.draw);
        benchmark::DoNotOptimize(f.draw);
    }
}

/// Loading a million parcels into the document, i.e. the SoA append path.
///
/// The document is reset with the timer PAUSED. Freeing a million parcels costs
/// real time and is not what this scenario claims to measure; leaving it in the
/// timed region would have made the append path look slower than it is.
void bulk_load(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        auto doc = std::make_unique<core::Document>();
        state.ResumeTiming();

        bench::build_cadastral_grid(*doc, 1'000'000);
        benchmark::DoNotOptimize(*doc);

        state.PauseTiming();
        doc.reset();
        state.ResumeTiming();
    }
}

/// Packing the tree is a one-time cost after a bulk load; the native mmap format
/// will ship it prebuilt (kentoscad.md §9.3).
void index_build(benchmark::State& state)
{
    core::Document doc;
    bench::build_cadastral_grid(doc, 1'000'000);

    core::SpatialIndex index;
    for (auto _ : state) {
        index.build(doc.entities());
        benchmark::DoNotOptimize(index);
    }
}

/// Drawing one line must not repack the layer. The document keeps new entities in
/// a short unindexed tail until they are worth repacking (§10.5).
///
/// One iteration per repetition: the body ADDS a line, so repeating it inside a
/// repetition would measure a document that is growing under the measurement.
void frame_after_edit(benchmark::State& state)
{
    auto& f = cadastral();
    f.view.set_centre(f.extent.centre(), 100.0);

    for (auto _ : state) {
        state.PauseTiming();
        core::Op op;
        const std::array<core::Point2, 2> seg{core::Point2{485000000, 4310000000},
                                              core::Point2{485010000, 4310010000}};
        (void)f.doc.add_polyline(0, seg, op);
        state.ResumeTiming();

        render::build_scene(f.doc, f.view, f.options, f.draw);
        benchmark::DoNotOptimize(f.draw);
    }
}

/// Resident memory with the whole fixture loaded. A reading, not a duration.
void memory_5m(benchmark::State& state)
{
    (void)cadastral();
    for (auto _ : state) {
    }
    state.counters[bench::kGaugeCounter] = bench::resident_mb();
}

} // namespace

KENTOS_BENCH(render_pan_zoom){bench::Case{
    .id          = "render.pan_zoom_5m",
    .title       = "5M poligonlu kadastro katmanında pan/zoom",
    .budget      = 16.0,
    .unit        = "ms",
    .repetitions = 7,
    .body        = &pan_zoom_working,
}};

KENTOS_BENCH(render_pan_zoom_full){bench::Case{
    .id          = "render.pan_zoom_5m_tam_kapsam",
    .title       = "5M poligonun tamamı ekranda (LOD gelene kadar bilgilendirme)",
    .budget      = 0.0,
    .unit        = "ms",
    .repetitions = 3,
    .body        = &pan_zoom_full,
}};

KENTOS_BENCH(core_bulk_load){bench::Case{
    .id     = "core.toplu_yukleme_1m",
    .title  = "1M parselin belgeye yüklenmesi",
    .budget = 0.0,
    .unit   = "ms",
    // Allocation dominates and is noisy, so many repetitions: the spread has to
    // be visible or the regression gate fires on the allocator's mood.
    .repetitions = 9,
    .iterations  = 1, // a second of work; one pass per repetition is the measurement
    .body        = &bulk_load,
}};

KENTOS_BENCH(index_build_1m){bench::Case{
    .id          = "core.indeks_kurulumu_1m",
    .title       = "1M parsel için STR R-tree paketleme",
    .budget      = 0.0,
    .unit        = "ms",
    .repetitions = 5,
    .iterations  = 1,
    .body        = &index_build,
}};

KENTOS_BENCH(frame_after_edit_case){bench::Case{
    .id          = "render.duzenleme_sonrasi_kare",
    .title       = "5M katmana çizgi eklendikten sonraki kare",
    .budget      = 16.0,
    .unit        = "ms",
    .repetitions = 7,
    .iterations  = 1, // the body mutates the shared fixture; see frame_after_edit
    .body        = &frame_after_edit,
}};

KENTOS_BENCH(memory_cadastral){bench::Case{
    .id          = "bellek.5m_parsel",
    .title       = "5M parsel yüklüyken bellek",
    .budget      = 0.0,
    .unit        = "MB",
    .repetitions = 1,
    .metric      = bench::Metric::Gauge,
    .body        = &memory_5m,
}};
