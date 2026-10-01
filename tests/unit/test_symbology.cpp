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
#include "piricad/render/scene.hpp"
#include "piricad/render/symbology.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace piricad;

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

TEST_CASE("sembol birimleri görünüm boyutunu koruyarak dönüşür")
{
    using core::Measure;
    using core::Unit;
    CHECK(render::measure_in_unit({3000, Unit::Paper}, Unit::Pixel, 250, 4) ==
          Measure{12, Unit::Pixel});
    CHECK(render::measure_in_unit({12, Unit::Pixel}, Unit::Ground, 250, 4) ==
          Measure{3000, Unit::Ground});
    CHECK(render::measure_in_unit({3000, Unit::Ground}, Unit::Paper, 250, 4) ==
          Measure{3000, Unit::Paper});
    CHECK(render::measure_in_unit({3000, Unit::Paper}, Unit::Ground, 1250, 4) ==
          Measure{15000, Unit::Ground});
    CHECK(render::measure_in_unit({-1500, Unit::Ground}, Unit::Pixel, 250, 4) ==
          Measure{-6, Unit::Pixel});
    CHECK(render::measure_in_unit({1, Unit::Paper}, Unit::Pixel, 250, 4) ==
          Measure{1, Unit::Pixel});
    CHECK(render::measure_in_unit({0, Unit::Ground}, Unit::Pixel, 250, 4) ==
          Measure{0, Unit::Pixel});
    CHECK(render::measure_in_unit({std::numeric_limits<std::int32_t>::max(), Unit::Pixel},
                                  Unit::Ground, 1000, 4)
              .value == std::numeric_limits<std::int32_t>::max());
}

TEST_CASE("ölçek yalnız zemin sembol ölçülerini değiştirir")
{
    core::SymbolLayer hatch;
    hatch.type          = core::SymbolLayerType::LinePatternFill;
    hatch.look.width_um = 250;
    hatch.interval      = {3000, core::Unit::Paper};
    hatch.offset        = {2, core::Unit::Pixel};
    hatch.phase         = {1000, core::Unit::Ground};
    const core::ImageStore images;
    const core::DashStore dashes;
    const auto far  = render::pass_of(hatch, images, dashes, 1250, 4);
    const auto near = render::pass_of(hatch, images, dashes, 125, 4);
    CHECK(far.interval_px == doctest::Approx(12));
    CHECK(near.interval_px == doctest::Approx(12));
    CHECK(far.offset_px == doctest::Approx(2));
    CHECK(near.offset_px == doctest::Approx(2));
    CHECK(far.line_width_px == doctest::Approx(1));
    CHECK(near.line_width_px == doctest::Approx(1));
    CHECK(far.phase_px == doctest::Approx(0.8));
    CHECK(near.phase_px == doctest::Approx(8));
    hatch.interval.unit = core::Unit::Ground;
    CHECK(render::pass_of(hatch, images, dashes, 1250, 4).interval_px == doctest::Approx(2.4));
    CHECK(render::pass_of(hatch, images, dashes, 125, 4).interval_px == doctest::Approx(24));
}

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

TEST_CASE("çizgi deseni verilen çapadan geçer; çapa yüzün dışında olsa da (TODOS C-11)")
{
    // Anchored at a point of the pattern's lattice on the ground — brought to
    // the screen by the scene — every line lies a whole number of spacings from
    // it, wherever the face is: the pattern belongs to the ground.
    const render::PixelBox f = face(1.0f);
    std::vector<float> lines;
    const double ax = -377.25;
    const double ay = 1019.5;
    render::hatch_lines(f, screen(), 10.0, 0.0, lines, ax, ay);
    REQUIRE(!lines.empty());
    for (std::size_t i = 0; i + 3 < lines.size(); i += 4) {
        const double across = static_cast<double>(lines[i + 1]) - ay;
        const double k      = across / 10.0;
        CHECK(std::abs(k - std::round(k)) < 0.001);
    }
}

TEST_CASE("çok sık desen binlerce çizgi üretmez; ekranda tonuna döner (TODOS C-11)")
{
    // A hatch whose lines are a hundredth of a pixel apart is a tone. The
    // generator refuses to lay it out line by line — the backends draw the tint
    // instead — so the frame stays inside its budget whatever the scale.
    std::vector<float> lines;
    render::hatch_lines(face(1.0f), screen(), 0.01, 45.0, lines);
    CHECK(lines.empty());
    CHECK(render::hatch_tint_below(0.01));
    CHECK_FALSE(render::hatch_tint_below(6.0));
    // One-pixel lines every two pixels cover half the face.
    CHECK_EQ(render::hatch_tint_alpha(1.0, 2.0, 255), 128);
    CHECK_EQ(render::hatch_tint_alpha(1.0, 0.5, 200), 200);
}

TEST_CASE("SVG çizgi işaretçisi sıfır fazı korur ve kenar uzarken yeniden dağılmaz")
{
    const std::array<float, 2> xs{0, 99};
    const std::array<float, 2> ys{0, 0};
    const std::array<float, 2> longer{0, 106};
    std::vector<render::Stamp> a, b, legacy, negative;
    render::place_along_run(xs.data(), ys.data(), 2, core::MarkerPlacement::Interval, 20, 0, {}, a,
                            true);
    render::place_along_run(longer.data(), ys.data(), 2, core::MarkerPlacement::Interval, 20, 0, {},
                            b, true);
    REQUIRE(a.size() == 5);
    REQUIRE(b.size() == 6);
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == b[i].x);
        CHECK(a[i].x == static_cast<float>(i * 20));
    }
    render::place_along_run(xs.data(), ys.data(), 2, core::MarkerPlacement::Interval, 20, 0, {},
                            legacy);
    CHECK(legacy.front().x == 10);
    render::place_along_run(xs.data(), ys.data(), 2, core::MarkerPlacement::Interval, 20, -5, {},
                            negative, true);
    CHECK(negative.front().x == 15);
}

TEST_CASE("Stil kaydırması açık çizgide ve kapalı halkada köşeyi ve kapanışı korur")
{
    render::PolylineBatch source;
    source.xs   = {0, 100, 100, 0, 0, 200, 300};
    source.ys   = {0, 0, 100, 100, 0, 0, 0};
    source.runs = {5, 2};
    render::PolylineBatch result;
    render::offset_polyline(source, 4, result);
    CHECK(result.runs == source.runs);
    CHECK(result.xs[0] == 4);
    CHECK(result.ys[0] == 4);
    CHECK(result.xs[1] == 96);
    CHECK(result.ys[1] == 4);
    CHECK(result.xs[4] == result.xs[0]);
    CHECK(result.ys[4] == result.ys[0]);
    CHECK(result.xs[5] == 200);
    CHECK(result.ys[5] == 4);
    CHECK(result.xs[6] == 300);
    CHECK(result.ys[6] == 4);
    render::offset_polyline(source, -4, result);
    CHECK(result.xs[0] == -4);
    CHECK(result.ys[0] == -4);
    CHECK(result.ys[6] == -4);
    CHECK(source.xs[0] == 0);
    CHECK(source.ys[0] == 0);
}

TEST_CASE("KentOS SVG işaretçisi iç köşeyi ve her segmentin merkezini ayırt eder")
{
    const std::array<float, 3> xs{0, 100, 100};
    const std::array<float, 3> ys{0, 0, 80};
    std::vector<render::Stamp> inner, centres;
    render::place_along_run(xs.data(), ys.data(), 3, core::MarkerPlacement::Vertex, 0, 0, {}, inner,
                            true, render::SvgPlacement::InnerVertex);
    REQUIRE(inner.size() == 1);
    CHECK(inner.front().x == 100);
    CHECK(inner.front().y == 0);
    render::place_along_run(xs.data(), ys.data(), 3, core::MarkerPlacement::Centre, 0, 0, {},
                            centres, true, render::SvgPlacement::SegmentCentre);
    REQUIRE(centres.size() == 2);
    CHECK(centres[0].x == 50);
    CHECK(centres[0].y == 0);
    CHECK(centres[1].x == 100);
    CHECK(centres[1].y == 40);
}
