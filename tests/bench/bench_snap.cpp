// SPDX-License-Identifier: GPL-3.0-or-later
// Snap and selection budgets.
//
// These run on EVERY MOUSE MOVE, next to the frame that piricad.md §10.1 budgets
// at 16 ms over five million parcels. A snap search that took the whole frame
// budget would halve the frame rate while the user is drawing — which is exactly
// when the frame rate matters — so each scenario is gated at the frame budget on
// its own, and the last scenario measures what a mouse move actually costs:
// the scene rebuild AND the snap, together.
#include "benchmark.hpp"
#include "fixtures.hpp"

#include "piricad/core/pick.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/render/scene.hpp"

namespace {

using namespace piricad;

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
double snap_all_modes()
{
    Fixture& f              = fixture();
    const core::SnapQuery q = working_query();

    const auto start         = bench::Clock::now();
    const core::SnapResult r = core::snap(f.doc, q);
    const double ms          = bench::since(start);
    bench::keep(static_cast<double>(r.point.x));
    return ms;
}

/// A single click, resolved with the declared seçim toleransı.
double pick_single()
{
    Fixture& f = fixture();

    const auto start = bench::Clock::now();
    const core::EntityId hit =
        core::pick_nearest(f.doc, f.aim, static_cast<core::Mm>(6 * kWorkingScale));
    const double ms = bench::since(start);
    bench::keep(static_cast<double>(hit));
    return ms;
}

/// A crossing box over the whole viewport: the worst box a user can drag without
/// zooming out, and the one that touches the most geometry.
double pick_viewport_box()
{
    Fixture& f = fixture();
    static std::vector<core::EntityId> hits;
    hits.clear();

    const auto start = bench::Clock::now();
    core::pick_in_box(f.doc, f.view.visible_box(), core::PickMode::Crossing, hits);
    const double ms = bench::since(start);
    bench::keep(static_cast<double>(hits.size()));
    return ms;
}

/// What one mouse move really costs while a command is asking for a point: the
/// scene is rebuilt and the snap marker is resolved. This is the scenario that
/// must fit inside the §10.1 frame budget, and the reason the two above are
/// measured separately is so a regression says which half moved.
double frame_with_snap()
{
    Fixture& f              = fixture();
    const core::SnapQuery q = working_query();

    const auto start = bench::Clock::now();
    render::build_scene(f.doc, f.view, f.options, f.draw);
    const core::SnapResult r = core::snap(f.doc, q);
    const double ms          = bench::since(start);
    bench::keep(static_cast<double>(r.point.y));
    return ms;
}

} // namespace

PIRICAD_BENCH(snap_all){bench::Case{
    .id     = "yakalama.imlec_5m",
    .title  = "5M parselde bütün yakalama modlarıyla tek imleç sorgusu",
    .budget = 16.0,
    .unit   = "ms",
    .runs   = 9,
    .run    = &snap_all_modes,
}};

PIRICAD_BENCH(pick_click){bench::Case{
    .id     = "secim.tek_tik_5m",
    .title  = "5M parselde tek tıklamayla seçim",
    .budget = 16.0,
    .unit   = "ms",
    .runs   = 9,
    .run    = &pick_single,
}};

PIRICAD_BENCH(pick_box){bench::Case{
    .id     = "secim.pencere_5m",
    .title  = "5M parselde ekran boyu kesen kutu (bilgilendirme)",
    .budget = 0.0,
    .unit   = "ms",
    .runs   = 5,
    .run    = &pick_viewport_box,
}};

PIRICAD_BENCH(frame_and_snap){bench::Case{
    .id     = "render.kare_ve_yakalama_5m",
    .title  = "Bir fare hareketi: sahne kurulumu + yakalama",
    .budget = 16.0,
    .unit   = "ms",
    .runs   = 7,
    .run    = &frame_with_snap,
}};
