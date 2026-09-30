// SPDX-License-Identifier: GPL-3.0-or-later
//
// The geometry kernel (CLAUDE.md 2.11, core/kernel.hpp): OpenCASCADE behind a
// millimetre face. What is held here is what the adapter promises — an arc goes
// in and an arc comes back, with the centre and radius it had; a face comes back
// as ONE set of pieces whatever order the kernel walked it in; and every figure
// is a whole millimetre, so the same input gives the same numbers on every
// platform. The expected values are integers worked out by hand: a platform
// that disagreed would show a wrong number here, not a last-bit difference.
#include "piricad_test.hpp"

#include "piricad/core/curve_path.hpp"
#include "piricad/core/kernel.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

using namespace piricad;
using core::CurvePath;
using core::KernelFace;
using core::PathPiece;
using core::Point2;

namespace {

// TUREF magnitudes, so the adapter's local frame is exercised: every figure
// below is relative to this corner.
constexpr core::Mm kX = 485'300'000;
constexpr core::Mm kY = 4'310'200'000;

Point2 at(core::Mm x, core::Mm y)
{
    return Point2{kX + x, kY + y};
}

PathPiece segment(Point2 a, Point2 b)
{
    return PathPiece{.kind = PathPiece::Kind::Segment, .from = a, .to = b};
}

/// A 10 m square, counter-clockwise, its top-right corner rounded with a 2 m
/// arc — a parcel after YUVARLA.
CurvePath rounded_square()
{
    CurvePath p;
    p.closed = true;
    p.pieces = {
        segment(at(0, 0), at(10'000, 0)),
        segment(at(10'000, 0), at(10'000, 8'000)),
        core::arc_piece(at(8'000, 8'000), 2'000, at(10'000, 8'000), at(8'000, 10'000), true),
        segment(at(8'000, 10'000), at(0, 10'000)),
        segment(at(0, 10'000), at(0, 0)),
    };
    return p;
}

CurvePath box(core::Mm x0, core::Mm y0, core::Mm x1, core::Mm y1)
{
    CurvePath p;
    p.closed = true;
    p.pieces = {segment(at(x0, y0), at(x1, y0)), segment(at(x1, y0), at(x1, y1)),
                segment(at(x1, y1), at(x0, y1)), segment(at(x0, y1), at(x0, y0))};
    return p;
}

std::size_t arcs_in(const CurvePath& p)
{
    std::size_t n = 0;
    for (const PathPiece& piece : p.pieces)
        if (piece.kind == PathPiece::Kind::Arc) ++n;
    return n;
}

} // namespace

TEST_CASE("ÇEKİRDEK: OpenCASCADE bu yapıda var ve sürümünü söylüyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    CHECK(core::kernel_version().starts_with("OpenCASCADE 7."));
}

TEST_CASE("ÇEKİRDEK: yuvarlatılmış köşeli parsel kesilince yay, merkezi ve yarıçapıyla kalıyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const std::vector<KernelFace> parcel{KernelFace{rounded_square(), {}}};
    const std::vector<KernelFace> right{KernelFace{box(5'000, -1'000, 12'000, 12'000), {}}};

    auto kept = core::kernel_boolean(parcel, right, core::BooleanOp::Intersection);
    REQUIRE(kept.ok());
    REQUIRE_EQ(kept.value().size(), 1u);
    const CurvePath& outer = kept.value().front().outer;
    REQUIRE_EQ(arcs_in(outer), 1u);
    for (const PathPiece& piece : outer.pieces)
        if (piece.kind == PathPiece::Kind::Arc) {
            CHECK_EQ(piece.centre, at(8'000, 8'000));
            CHECK_EQ(piece.radius, 2'000);
            CHECK_EQ(piece.from, at(10'000, 8'000));
            CHECK_EQ(piece.to, at(8'000, 10'000));
            CHECK_EQ(piece.sweep_udeg, 90'000'000); // a quarter turn, counter-clockwise
        }
    // THE AREA WITH ITS ARC: the half square less the corner the arc rounds off,
    // 5 m × 10 m − (2 m × 2 m − π · 2 m² / 4) = 49 141 592,65 mm².
    const double expected = 5'000.0 * 10'000.0 - (4'000'000.0 - std::acos(-1.0) * 1'000'000.0);
    CHECK(std::abs(static_cast<double>(core::path_area(outer)) - expected) <= 1.0);
    // Handed back counter-clockwise, from its lowest, then leftmost, vertex.
    CHECK(core::path_area(outer) > 0);
    CHECK_EQ(outer.pieces.front().from, at(5'000, 0));

    // AND THE REST: the left half has no arc and four corners.
    auto rest = core::kernel_boolean(parcel, right, core::BooleanOp::Difference);
    REQUIRE(rest.ok());
    REQUIRE_EQ(rest.value().size(), 1u);
    CHECK_EQ(arcs_in(rest.value().front().outer), 0u);
    CHECK_EQ(rest.value().front().outer.pieces.size(), 4u);
}

TEST_CASE("ÇEKİRDEK: kesilen iki parça birleşince parsel eski hâline dönüyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const std::vector<KernelFace> parcel{KernelFace{rounded_square(), {}}};
    const std::vector<KernelFace> right{KernelFace{box(5'000, -1'000, 12'000, 12'000), {}}};
    auto a = core::kernel_boolean(parcel, right, core::BooleanOp::Intersection);
    auto b = core::kernel_boolean(parcel, right, core::BooleanOp::Difference);
    REQUIRE((a.ok() && b.ok()));

    std::vector<KernelFace> both = a.value();
    both.insert(both.end(), b.value().begin(), b.value().end());
    auto merged = core::kernel_boolean(both, {}, core::BooleanOp::Union);
    REQUIRE(merged.ok());
    REQUIRE_EQ(merged.value().size(), 1u);

    // THE SEAM IS GONE: the same five pieces the parcel had, from the same
    // vertex — the kernel's own bookkeeping never shows.
    auto again = core::kernel_boolean(parcel, {}, core::BooleanOp::Union);
    REQUIRE(again.ok());
    CHECK_EQ(merged.value().front().outer.pieces.size(), 5u);
    CHECK(merged.value() == again.value());
}

TEST_CASE("ÇEKİRDEK: delikli alan deliğiyle, delik saat yönünde geliyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const std::vector<KernelFace> big{KernelFace{box(0, 0, 20'000, 20'000), {}}};
    const std::vector<KernelFace> yard{KernelFace{box(5'000, 5'000, 15'000, 15'000), {}}};
    auto holed = core::kernel_boolean(big, yard, core::BooleanOp::Difference);
    REQUIRE(holed.ok());
    REQUIRE_EQ(holed.value().size(), 1u);
    const KernelFace& face = holed.value().front();
    REQUIRE_EQ(face.holes.size(), 1u);
    CHECK(core::path_area(face.outer) == 400'000'000);
    CHECK(core::path_area(face.holes.front()) == -100'000'000);
    CHECK_EQ(face.holes.front().pieces.front().from, at(5'000, 5'000));
}

TEST_CASE("ÇEKİRDEK: dışa ofsetin köşeleri gerçek yay, yuvarlak köşe büyüyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    auto grown = core::kernel_offset(rounded_square(), 1'000, core::OffsetCorner::Round,
                                     /*both_sides=*/false);
    REQUIRE(grown.ok());
    REQUIRE_EQ(grown.value().size(), 1u);
    const CurvePath& ring = grown.value().front();
    CHECK(ring.closed);
    std::size_t corners = 0;
    bool fillet         = false;
    for (const PathPiece& piece : ring.pieces) {
        if (piece.kind != PathPiece::Kind::Arc) continue;
        if (piece.radius == 1'000) ++corners;
        if (piece.radius == 3'000 && piece.centre == at(8'000, 8'000)) fillet = true;
    }
    CHECK_EQ(corners, 3u); // the three square corners, rounded by the offset
    CHECK(fillet);         // the parcel's own 2 m arc, 1 m further out
    CHECK_EQ(ring.pieces.front().from, at(0, -1'000));
}

TEST_CASE("ÇEKİRDEK: açık çizginin tamponu yuvarlak uçlu kapalı bant, tek yanı açık çizgi")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    CurvePath ell;
    ell.pieces = {segment(at(0, 0), at(10'000, 0)), segment(at(10'000, 0), at(10'000, 10'000))};

    auto band = core::kernel_offset(ell, 1'000, core::OffsetCorner::Round, /*both_sides=*/true);
    REQUIRE(band.ok());
    REQUIRE_EQ(band.value().size(), 1u);
    CHECK(band.value().front().closed);
    CHECK_EQ(arcs_in(band.value().front()), 3u); // two round ends and the outer corner

    auto side = core::kernel_offset(ell, 1'000, core::OffsetCorner::Round, /*both_sides=*/false);
    REQUIRE(side.ok());
    REQUIRE_EQ(side.value().size(), 1u);
    const CurvePath& right = side.value().front();
    CHECK_FALSE(right.closed);
    // ON THE RIGHT OF TRAVEL: below the first leg, beyond the second.
    CHECK_EQ(right.pieces.front().from, at(0, -1'000));
    CHECK_EQ(right.pieces.back().to, at(11'000, 10'000));
}

TEST_CASE("ÇEKİRDEK: aynı giriş her seferinde aynı parçalar")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    const std::vector<KernelFace> parcel{KernelFace{rounded_square(), {}}};
    const std::vector<KernelFace> cutter{KernelFace{box(-1'000, 3'000, 30'000, 7'000), {}}};
    auto first  = core::kernel_boolean(parcel, cutter, core::BooleanOp::Difference);
    auto second = core::kernel_boolean(parcel, cutter, core::BooleanOp::Difference);
    REQUIRE((first.ok() && second.ok()));
    CHECK_EQ(first.value().size(), 2u); // the band cuts the square in two
    CHECK(first.value() == second.value());
    // IN THE ONE ORDER: by their lowest, then leftmost, vertex.
    CHECK_EQ(first.value().front().outer.pieces.front().from, at(0, 0));
    CHECK_EQ(first.value().back().outer.pieces.front().from, at(0, 7'000));
}

TEST_CASE("ÇEKİRDEK: spline kenarlı yol bu aşamada reddediliyor, sebebi söyleniyor")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    CurvePath odd      = box(0, 0, 10'000, 10'000);
    odd.pieces[1].kind = PathPiece::Kind::Spline;
    auto refused       = core::kernel_boolean(std::vector<KernelFace>{KernelFace{odd, {}}}, {},
                                              core::BooleanOp::Union);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("spline") != std::string::npos);

    CurvePath open  = box(0, 0, 10'000, 10'000);
    open.closed     = false;
    auto not_closed = core::kernel_boolean(std::vector<KernelFace>{KernelFace{open, {}}}, {},
                                           core::BooleanOp::Union);
    REQUIRE_FALSE(not_closed.ok());
    CHECK(not_closed.error().message.find("kapalı") != std::string::npos);
}

TEST_CASE("ÇEKİRDEK: uçları kendi çemberinden milimetre kesri sapan yay da kenar oluyor")
{
    // An arc through three points in general position — what KENARTÜRÜ and a
    // DXF bulge make — keeps its centre and radius to the millimetre, so its
    // ends sit a fraction of a millimetre off its own circle. The kernel's
    // edge from centre and radius refused them, and with them the face and the
    // whole boolean ("command not done"): TEVHİT and BİRLEŞTİR both failed on
    // such a parcel. The edge is the circle through its ends and midpoint now.
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    CurvePath bent;
    bent.closed = true;
    // (0,0) → (20,0) bowed to (10,−3): centre (10, 15.1667), radius 18.1667 —
    // stored as (10000, 15167) and 18167.
    bent.pieces = {core::arc_piece(at(10'000, 15'167), 18'167, at(0, 0), at(20'000, 0), true),
                   segment(at(20'000, 0), at(20'000, 10'000)),
                   segment(at(20'000, 10'000), at(0, 10'000)), segment(at(0, 10'000), at(0, 0))};
    const std::vector<KernelFace> a{KernelFace{bent, {}}};
    const std::vector<KernelFace> b{KernelFace{box(20'000, 0, 40'000, 10'000), {}}};
    auto fused = core::kernel_boolean(a, b, core::BooleanOp::Union);
    if (!fused) MESSAGE(fused.error().message);
    REQUIRE(fused.ok());
    REQUIRE_EQ(fused.value().size(), 1u);
    std::size_t arcs = 0;
    for (const PathPiece& piece : fused.value().front().outer.pieces)
        if (piece.kind == PathPiece::Kind::Arc) {
            ++arcs;
            CHECK_EQ(piece.centre, at(10'000, 15'167));
            CHECK_EQ(piece.radius, core::Mm{18'167});
        }
    CHECK_EQ(arcs, 1u);
}
