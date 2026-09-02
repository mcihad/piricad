// SPDX-License-Identifier: GPL-3.0-or-later
//
// kentoscad.md §11 Faz 0: "`double` jitter testi: 30. `dilim` TM3 koordinatlarıyla zoom".
//
// §10.3 states the trap: TUREF/TM3 coordinates are seven digits, so writing one
// straight into a float vertex attribute shimmers by metres. This test proves
// three things — that the hazard is real, that ViewTransform's origin offset
// removes it, and that the screen mapping round-trips exactly.
#include "kentos_test.hpp"

#include "kentos_cad/render/view.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

using namespace kentos;
using kentos::core::Mm;
using kentos::core::Point2;

namespace {

// A real parcel corner in the 30th 3-degree zone, TUREF/TM30.
constexpr Point2 kCorner{485320150, 4310220400};

/// How many distinct float values a run of consecutive millimetres collapses to.
std::size_t distinct_floats(Mm from, Mm count, const std::function<float(Mm)>& to_float)
{
    std::size_t distinct = 1;
    float previous       = to_float(from);

    for (Mm d = 1; d < count; ++d) {
        const float here = to_float(from + d);
        if (here != previous) {
            ++distinct;
            previous = here;
        }
    }
    return distinct;
}

} // namespace

TEST_CASE("JITTER: absolute TM3 coordinates collapse in a float")
{
    // The hazard of §10.3, stated as a measurement rather than a warning: float
    // has a 24-bit mantissa, so at 4.85e8 the gap between representable values is
    // 32 mm. A thousand consecutive millimetres land on about thirty of them.
    const std::size_t naive =
        distinct_floats(kCorner.x, 1000, [](Mm v) { return static_cast<float>(v); });

    CHECK(naive < 64); // ~32 in practice: three centimetres of shimmer

    const auto step = static_cast<double>(std::nextafter(static_cast<float>(kCorner.x), 1e30f)) -
                      static_cast<double>(static_cast<float>(kCorner.x));
    CHECK(step >= 16.0); // the size of one jump, in millimetres on the ground
}

TEST_CASE("JITTER: the origin offset keeps every millimetre distinct")
{
    render::ViewTransform view;
    view.set_viewport(1920, 1080);
    view.set_centre(kCorner, 1.0); // 1 mm per pixel — deep zoom

    // The same thousand millimetres, routed through the offset: all thousand stay
    // apart. This is the whole reason ViewTransform exists.
    const std::size_t offset =
        distinct_floats(kCorner.x, 1000, [&view](Mm v) { return view.offset_x_f(v); });

    CHECK_EQ(offset, std::size_t{1000});
}

TEST_CASE("JITTER: offsets stay ordered and close to the origin")
{
    render::ViewTransform view;
    view.set_viewport(1920, 1080);
    view.set_centre(kCorner, 1.0); // 1 mm per pixel — deep zoom

    // Adjacent millimetres must map to distinct, ordered float offsets.
    float previous = view.offset_x_f(kCorner.x - 5);
    for (Mm d = -4; d <= 5; ++d) {
        const float here = view.offset_x_f(kCorner.x + d);
        CHECK(here > previous);
        previous = here;
    }

    // And the offsets stay near the origin, where float is dense.
    for (Mm d : {Mm{-100000}, Mm{0}, Mm{100000}}) {
        CHECK(std::abs(view.offset_x_f(kCorner.x + d)) < 1.0e6f);
        CHECK(std::abs(view.offset_y_f(kCorner.y + d)) < 1.0e6f);
    }
}

TEST_CASE("JITTER: screen mapping round-trips exactly at working zoom")
{
    render::ViewTransform view;
    view.set_viewport(1920, 1080);

    // From 1 mm/px up to a whole sheet on screen.
    for (double scale : {1.0, 10.0, 100.0, 1000.0}) {
        view.set_centre(kCorner, scale);

        for (Mm dx : {Mm{-500000}, Mm{-1}, Mm{0}, Mm{1}, Mm{500000}}) {
            for (Mm dy : {Mm{-500000}, Mm{0}, Mm{500000}}) {
                const Point2 world{kCorner.x + dx, kCorner.y + dy};
                const Point2 back = view.to_world(view.to_screen(world));

                // Below one pixel the mapping quantises, which is correct; the
                // error must never exceed half a pixel.
                CHECK(std::abs(back.x - world.x) <= static_cast<Mm>(scale / 2 + 1));
                CHECK(std::abs(back.y - world.y) <= static_cast<Mm>(scale / 2 + 1));
            }
        }
    }
}

TEST_CASE("JITTER: panning away and back restores the exact view")
{
    render::ViewTransform view;
    view.set_viewport(1920, 1080);
    view.set_centre(kCorner, 25.0);

    const auto before = view.to_screen(kCorner);

    for (int i = 0; i < 500; ++i)
        view.pan_pixels(13.0, -7.0);
    for (int i = 0; i < 500; ++i)
        view.pan_pixels(-13.0, 7.0);

    const auto after = view.to_screen(kCorner);

    // Panning is integer millimetre arithmetic, so a round trip is exact — a
    // drawing must not drift because the operator scrolled around.
    CHECK_EQ(view.centre(), kCorner);
    CHECK_EQ(before.x, after.x);
    CHECK_EQ(before.y, after.y);
}

TEST_CASE("JITTER: zooming in and out returns to the same scale")
{
    render::ViewTransform view;
    view.set_viewport(1920, 1080);
    view.set_centre(kCorner, 40.0);

    const render::ScreenPoint anchor{960.0, 540.0};

    for (int i = 0; i < 40; ++i)
        view.zoom_at(anchor, 1.2);
    for (int i = 0; i < 40; ++i)
        view.zoom_at(anchor, 1.0 / 1.2);

    // The scale is a double and 1.2 has no exact binary form, so the round trip is
    // only approximate — but it must stay within a part in a thousand, or the
    // scale readout would visibly wander as the operator works.
    CHECK(std::abs(view.mm_per_pixel() - 40.0) < 0.04);
}

TEST_CASE("JITTER: every zone's coordinates survive the offset")
{
    // TM 3° zones 27..45: eastings carry the zone prefix, so the magnitudes differ
    // by a factor of three across the country (data/crs/tm3-dilimleri.json).
    render::ViewTransform view;
    view.set_viewport(1920, 1080);

    for (int zone = 27; zone <= 45; zone += 3) {
        const Mm easting  = static_cast<Mm>(zone / 3) * 1000000000 + 485320150;
        const Mm northing = 4310220400;
        const Point2 p{easting, northing};

        view.set_centre(p, 1.0);
        CHECK(view.offset_x_f(p.x + 1) > view.offset_x_f(p.x));
        CHECK(view.offset_y_f(p.y + 1) > view.offset_y_f(p.y));
        CHECK_EQ(view.to_world(view.to_screen(p)), p);
    }
}
