// SPDX-License-Identifier: GPL-3.0-or-later
// Render budgets from piricad.md §10.1.
#include "benchmark.hpp"
#include "fixtures.hpp"

#include "piricad/core/spatial_index.hpp"
#include "piricad/render/scene.hpp"

#include <array>

#include <memory>

namespace {

using namespace piricad;

/// The 5M fixture costs half a second to build, so it is built once and shared by
/// every render scenario.
struct Cadastral5M
{
    core::Document doc;
    render::ViewTransform view;
    render::DrawList draw;
    render::SceneOptions options;
    core::Box2 extent;

    Cadastral5M()
    {
        bench::build_cadastral_grid(doc, 5'000'000);
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
double pan_zoom_working()
{
    auto& f = cadastral();
    f.view.set_centre(f.extent.centre(), 100.0); // ~100 mm/px, about 190 m across

    const auto start = bench::Clock::now();
    render::build_scene(f.doc, f.view, f.options, f.draw);
    return bench::since(start);
}

/// Whole layer on screen. Informational: at this zoom precomputed LOD tiles carry
/// the load, and those are a Phase-1 deliverable (piricad.md §10.3).
double pan_zoom_full()
{
    auto& f = cadastral();
    f.view.fit(f.extent, 0.02);

    const auto start = bench::Clock::now();
    render::build_scene(f.doc, f.view, f.options, f.draw);
    return bench::since(start);
}

/// Loading five million parcels into the document, i.e. the SoA append path.
double bulk_load()
{
    core::Document doc;
    const auto start = bench::Clock::now();
    bench::build_cadastral_grid(doc, 1'000'000);
    return bench::since(start);
}

/// Packing the tree is a one-time cost after a bulk load; the native mmap format
/// will ship it prebuilt (piricad.md §9.3).
double index_build()
{
    core::Document doc;
    bench::build_cadastral_grid(doc, 1'000'000);

    core::SpatialIndex index;
    const auto start = bench::Clock::now();
    index.build(doc.entities());
    return bench::since(start);
}

/// Drawing one line must not repack the layer. The document keeps new entities in
/// a short unindexed tail until they are worth repacking (§10.5).
double frame_after_edit()
{
    auto& f = cadastral();
    f.view.set_centre(f.extent.centre(), 100.0);

    core::Op op;
    const std::array<core::Point2, 2> seg{core::Point2{485000000, 4310000000},
                                          core::Point2{485010000, 4310010000}};
    (void)f.doc.add_polyline(0, seg, op);

    const auto start = bench::Clock::now();
    render::build_scene(f.doc, f.view, f.options, f.draw);
    return bench::since(start);
}

double memory_5m()
{
    (void)cadastral();
    return bench::resident_mb();
}

} // namespace

PIRICAD_BENCH(render_pan_zoom){bench::Case{
    .id     = "render.pan_zoom_5m",
    .title  = "5M poligonlu kadastro katmanında pan/zoom",
    .budget = 16.0,
    .unit   = "ms",
    .runs   = 7,
    .run    = &pan_zoom_working,
}};

PIRICAD_BENCH(render_pan_zoom_full){bench::Case{
    .id     = "render.pan_zoom_5m_tam_kapsam",
    .title  = "5M poligonun tamamı ekranda (LOD gelene kadar bilgilendirme)",
    .budget = 0.0,
    .unit   = "ms",
    .runs   = 3,
    .run    = &pan_zoom_full,
}};

PIRICAD_BENCH(core_bulk_load){bench::Case{
    .id     = "core.toplu_yukleme_1m",
    .title  = "1M parselin belgeye yüklenmesi",
    .budget = 0.0,
    .unit   = "ms",
    .runs   = 9, // ayırma baskın, gürültülü: yayılımın görünmesi için çok örnek
    .run    = &bulk_load,
}};

PIRICAD_BENCH(index_build_1m){bench::Case{
    .id     = "core.indeks_kurulumu_1m",
    .title  = "1M parsel için STR R-tree paketleme",
    .budget = 0.0,
    .unit   = "ms",
    .runs   = 5,
    .run    = &index_build,
}};

PIRICAD_BENCH(frame_after_edit_case){bench::Case{
    .id     = "render.duzenleme_sonrasi_kare",
    .title  = "5M katmana çizgi eklendikten sonraki kare",
    .budget = 16.0,
    .unit   = "ms",
    .runs   = 7,
    .run    = &frame_after_edit,
}};

PIRICAD_BENCH(memory_cadastral){bench::Case{
    .id     = "bellek.5m_parsel",
    .title  = "5M parsel yüklüyken bellek",
    .budget = 0.0,
    .unit   = "MB",
    .runs   = 1,
    .run    = &memory_5m,
}};
