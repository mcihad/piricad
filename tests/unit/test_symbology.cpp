// SPDX-License-Identifier: GPL-3.0-or-later
// The pattern generators, bounded to what can be seen.
//
// WHY THIS FILE EXISTS. A styled imar plan became unusable to pan and zoom, and
// every test in this suite stayed green while it did: the document was right, the
// scene was right, and the picture was right — the program was simply building
// hundreds of thousands of marks nobody could see. `İÇEAKTAR` a plan, style it,
// zoom to 1:1, and one parcel edge carried tens of thousands of glyphs and
// millions of vertices, 19.8 ms a frame against a 16 ms budget (§10.1).
//
// The bug had no wrong answer to assert against, which is why it survived. What
// CAN be asserted is the shape of the work: a face a hundred screens wide must
// not cost a hundred times a face one screen wide, and the marks that do land
// must land exactly where they landed before — a clip that shifted the pattern
// would make it crawl across the parcel as the user pans.
#include "kentos_cad/render/symbology.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <vector>

using namespace kentos;

namespace {

/// A screen-sized clip, the shape every backend hands the generators.
render::PixelBox screen()
{
    return render::PixelBox{-8.0f, -8.0f, 1288.0f, 728.0f};
}

/// A face `times` screens wide and tall, centred on the screen — a parcel at 1:1.
render::PixelBox face(float times)
{
    const float w = 1280.0f * times;
    const float h = 720.0f * times;
    return render::PixelBox{640.0f - w * 0.5f, 360.0f - h * 0.5f, 640.0f + w * 0.5f,
                            360.0f + h * 0.5f};
}

} // namespace

TEST_CASE("çizgi deseni görünür alanla sınırlı kalır")
{
    std::vector<float> one;
    std::vector<float> hundred;

    render::hatch_lines(face(1.0f), screen(), 6.0, 45.0, one);
    render::hatch_lines(face(100.0f), screen(), 6.0, 45.0, hundred);

    CHECK(!one.empty());
    CHECK(!hundred.empty());

    // A hundred times the face is not a hundred times the work. The bound is
    // generous on purpose — the point is the ORDER, not a pixel-exact count.
    CHECK(hundred.size() < one.size() * 3);
}

TEST_CASE("kırpma desenin fazını kaydırmaz")
{
    // The same face, clipped and unclipped. Every line the clipped set produces
    // must be one the unclipped set also produced, at the same place: the phase
    // belongs to the FACE, and a clip that moved it would make the hatch crawl
    // across the parcel while the user pans.
    const render::PixelBox f = face(3.0f);

    std::vector<float> all;
    std::vector<float> some;
    render::hatch_lines(f, render::PixelBox{}, 12.0, 30.0, all);
    render::hatch_lines(f, screen(), 12.0, 30.0, some);

    REQUIRE(all.size() % 4 == 0);
    REQUIRE(some.size() % 4 == 0);
    CHECK(some.size() < all.size());

    // Each line is stated by its perpendicular offset from the face centre, which
    // is what the phase IS. The endpoints differ by design — that is the clip.
    const double cx = (static_cast<double>(f.min_x) + static_cast<double>(f.max_x)) * 0.5;
    const double cy = (static_cast<double>(f.min_y) + static_cast<double>(f.max_y)) * 0.5;

    const double radians = -30.0 * 3.14159265358979323846 / 180.0;
    const double ca      = std::cos(radians);
    const double sa      = std::sin(radians);

    const auto offset_of = [&](const std::vector<float>& v, std::size_t i) {
        const double dx = static_cast<double>(v[i]) - cx;
        const double dy = static_cast<double>(v[i + 1]) - cy;
        return -dx * sa + dy * ca;
    };

    // A hundredth of a pixel. The endpoints are stored as `float` and the face is
    // thousands of pixels across, so an exact comparison would be testing the
    // width of a float and not the phase of a hatch.
    constexpr double kSlack = 0.01;

    for (std::size_t i = 0; i + 3 < some.size(); i += 4) {
        const double want = offset_of(some, i);
        bool found        = false;
        for (std::size_t j = 0; j + 3 < all.size() && !found; j += 4)
            found = std::abs(offset_of(all, j) - want) < kSlack;
        CHECK(found);
    }
}

TEST_CASE("nokta deseni ekran dışına ızgara kurmaz")
{
    std::vector<float> one;
    std::vector<float> hundred;

    render::pattern_points(face(1.0f), screen(), 20.0, 20.0, one);
    render::pattern_points(face(100.0f), screen(), 20.0, 20.0, hundred);

    CHECK(!one.empty());

    // A GRID IS TWO DIMENSIONAL, which is why this one used to be the worst of
    // the three: a hundred screens of face is ten thousand screens of grid. It
    // also used to VANISH rather than merely be slow — the generator gave up
    // above forty thousand stamps and drew nothing at all, so a deep zoom lost
    // the forest symbol instead of stalling on it.
    CHECK(!hundred.empty());
    CHECK(hundred.size() < one.size() * 3);
}

TEST_CASE("nokta deseni ızgara fazını korur")
{
    // `floor(edge / step) * step` anchors the grid to the pixel lattice, so every
    // clipped point must sit on a multiple of the step — the same lattice the
    // unclipped call would have used.
    std::vector<float> spots;
    render::pattern_points(face(40.0f), screen(), 25.0, 15.0, spots);

    REQUIRE(!spots.empty());
    for (std::size_t i = 0; i + 1 < spots.size(); i += 2) {
        CHECK(std::abs(std::remainder(static_cast<double>(spots[i]), 25.0)) < 0.01);
        CHECK(std::abs(std::remainder(static_cast<double>(spots[i + 1]), 15.0)) < 0.01);
    }
}

TEST_CASE("çizgi üzerindeki işaretçiler görünür alanla sınırlı kalır")
{
    // A parcel boundary at 1:1 — a straight run a hundred screens long. Every
    // marker on it used to be built, and all but a handful were off screen.
    const std::array<float, 4> xs{-64000.0f, 64000.0f, 64000.0f, -64000.0f};
    const std::array<float, 4> ys{360.0f, 360.0f, 40000.0f, 40000.0f};

    std::vector<render::Stamp> all;
    std::vector<render::Stamp> some;
    render::place_along_run(xs.data(), ys.data(), 4, core::MarkerPlacement::Interval, 15.0, 0.0,
                            render::PixelBox{}, all);
    render::place_along_run(xs.data(), ys.data(), 4, core::MarkerPlacement::Interval, 15.0, 0.0,
                            screen(), some);

    CHECK(all.size() > 10000);
    CHECK(!some.empty());
    CHECK(some.size() < 200);

    // AND THE PHASE IS THE SAME ONE. Every kept stamp must be a stamp the
    // unclipped walk also produced, at the same point — the walk still crosses
    // the whole run and only the emission stops.
    for (const render::Stamp& s : some) {
        bool found = false;
        for (const render::Stamp& t : all)
            if (std::abs(s.x - t.x) < 0.01f && std::abs(s.y - t.y) < 0.01f) {
                found = true;
                break;
            }
        CHECK(found);
    }
}
