// SPDX-License-Identifier: GPL-3.0-or-later
// Snap and selection budgets.
//
// These run on EVERY MOUSE MOVE, next to the frame that kentoscad.md §10.1 budgets
// at 16 ms over five million parcels. A snap search that took the whole frame
// budget would halve the frame rate while the user is drawing — which is exactly
// when the frame rate matters — so each scenario is gated at the frame budget on
// its own, and the last scenario measures what a mouse move actually costs:
// the scene rebuild AND the snap, together.
#include "benchmark.hpp"
#include "fixtures.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/render/scene.hpp"

#include <vector>

namespace {

using namespace kentos;

/// The working zoom the frame budget is written against: an operator inspecting a
/// handful of parcels, about 190 m across at 1920x1080.
constexpr double kWorkingScale = 100.0;

/// Twelve pixels of aperture — the declared default of `core.yakalama.tolerans` —
/// converted at the working scale.
constexpr core::Mm kAperture = static_cast<core::Mm>(12 * kWorkingScale);

struct Fixture
{
    core::Document& doc;
    render::ViewTransform view;
    render::DrawList draw;
    render::SceneOptions options;
    core::Point2 aim{};

    Fixture() : doc(bench::cadastral_5m())
    {
        view.set_viewport(1920, 1080);
        const core::Box2 extent = doc.extent();
        view.set_centre(extent.centre(), kWorkingScale);

        // Aim just off a parcel corner, which is the expensive case: every mode
        // finds a candidate and the priority ladder is walked to the end.
        aim = core::Point2{extent.centre().x + 300, extent.centre().y + 200};
    }
};

Fixture& fixture()
{
    static Fixture f;
    return f;
}

core::SnapQuery working_query()
{
    Fixture& f = fixture();

    core::SnapQuery q;
    q.aim       = f.aim;
    q.radius    = kAperture;
    q.modes     = core::SnapAllMask;
    q.grid_step = 10000;
    q.has_base  = true;
    q.base      = core::Point2{f.aim.x - 50000, f.aim.y - 30000};
    return q;
}

/// Every object-snap mode on at once. A user with a full aperture pays this on
/// every mouse move, so it is the figure that has to stay small.
void snap_all_modes(benchmark::State& state)
{
    Fixture& f              = fixture();
    const core::SnapQuery q = working_query();

    for (auto _ : state) {
        core::SnapResult r = core::snap(f.doc, q);
        benchmark::DoNotOptimize(r);
    }
}

/// A single click, resolved with the declared `seçim toleransı`.
void pick_single(benchmark::State& state)
{
    Fixture& f = fixture();

    for (auto _ : state) {
        core::EntityId hit =
            core::pick_nearest(f.doc, f.aim, static_cast<core::Mm>(6 * kWorkingScale));
        benchmark::DoNotOptimize(hit);
    }
}

/// A crossing box over the whole viewport: the worst box a user can drag without
/// zooming out, and the one that touches the most geometry.
void pick_viewport_box(benchmark::State& state)
{
    Fixture& f = fixture();
    std::vector<core::EntityId> hits;

    for (auto _ : state) {
        hits.clear();
        core::pick_in_box(f.doc, f.view.visible_box(), core::PickMode::Crossing, hits);
        benchmark::DoNotOptimize(hits);
    }
}

/// What one mouse move really costs while a command is asking for a point: the
/// scene is rebuilt and the snap marker is resolved. This is the scenario that
/// must fit inside the §10.1 frame budget, and the reason the two above are
/// measured separately is so a regression says which half moved.
void frame_with_snap(benchmark::State& state)
{
    Fixture& f              = fixture();
    const core::SnapQuery q = working_query();

    for (auto _ : state) {
        render::build_scene(f.doc, f.view, f.options, f.draw);
        core::SnapResult r = core::snap(f.doc, q);
        benchmark::DoNotOptimize(r);
    }
}

} // namespace

KENTOS_BENCH(snap_all){bench::Case{
    .id          = "yakalama.imlec_5m",
    .title       = "5M parselde bütün yakalama modlarıyla tek imleç sorgusu",
    .budget      = 16.0,
    .unit        = "ms",
    .repetitions = 9,
    .body        = &snap_all_modes,
}};

KENTOS_BENCH(pick_click){bench::Case{
    .id          = "secim.tek_tik_5m",
    .title       = "5M parselde tek tıklamayla seçim",
    .budget      = 16.0,
    .unit        = "ms",
    .repetitions = 9,
    .body        = &pick_single,
}};

KENTOS_BENCH(pick_box){bench::Case{
    .id          = "secim.pencere_5m",
    .title       = "5M parselde ekran boyu kesen kutu (bilgilendirme)",
    .budget      = 0.0,
    .unit        = "ms",
    .repetitions = 5,
    .body        = &pick_viewport_box,
}};

KENTOS_BENCH(frame_and_snap){bench::Case{
    .id          = "render.kare_ve_yakalama_5m",
    .title       = "Bir fare hareketi: sahne kurulumu + yakalama",
    .budget      = 16.0,
    .unit        = "ms",
    .repetitions = 7,
    .body        = &frame_with_snap,
}};
