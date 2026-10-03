// SPDX-License-Identifier: GPL-3.0-or-later
//
// PROPERTIES, NOT EXAMPLES (TODOS Q-03). Every other test in this directory is a case somebody
// thought of: a square, a rounded corner, a parcel with a hole. A cadastral output is a legal
// document, and the failures that reach one are the ones nobody thought of — a cut that lands
// exactly on a vertex, a courtyard that touches the boundary, an arc the kernel returns as two.
// So this file states what must hold FOR ANY input of a kind and tries a few hundred of them.
//
// THE INPUTS ARE DETERMINISTIC: each case is a seed, drawn from `std::mt19937_64` — whose output
// the standard fixes — and reduced by modulo, because the standard's DISTRIBUTIONS are not the
// same on libstdc++, libc++ and MSVC. A failure names its seed, so it replays on any machine.
// Coordinates are TUREF-sized (a 485 km easting, a 4310 km northing), the magnitude at which the
// kernel's local frame earns its keep.
//
// WHAT IS HELD, and to what tolerance. An area is compared as areas are in the product: in whole
// square millimetres, after every vertex has been rounded once. One millimetre of error in a
// vertex moves an area by that millimetre times the length of the edge, so the tolerance is the
// operands' perimeters times one millimetre — a rounding, never a lost piece. A bug that drops a
// sliver or a hole is a thousand times that.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/curve_path.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/core/offset.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/core/text.hpp"
#include "piricad/io/service.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace piricad;
using command::Origin;
using core::BooleanOp;
using core::CurvePath;
using core::KernelFace;
using core::Mm;
using core::PathPiece;
using core::Point2;

namespace {

constexpr Mm kX = 485'300'000;
constexpr Mm kY = 4'310'200'000;

Point2 at(Mm x, Mm y)
{
    return Point2{kX + x, kY + y};
}

/// A seeded source of integers. The engine's output is specified by the standard; the reduction
/// is a modulo, so a seed means the same inputs on every platform.
class Rng
{
public:
    explicit Rng(std::uint64_t seed) : engine_(seed) {}

    /// An integer in [lo, hi].
    Mm in(Mm lo, Mm hi)
    {
        return lo + static_cast<Mm>(engine_() % static_cast<std::uint64_t>(hi - lo + 1));
    }

private:
    std::mt19937_64 engine_;
};

PathPiece segment(Point2 a, Point2 b)
{
    return PathPiece{.kind = PathPiece::Kind::Segment, .from = a, .to = b};
}

/// A simple polygon, star-shaped about `centre`: n vertices at increasing angles, each at its own
/// radius. Concave more often than not, never self-crossing, 10 m to `reach` across.
CurvePath star(Rng& rng, Mm cx, Mm cy, Mm reach)
{
    const int n = static_cast<int>(rng.in(4, 9));
    std::vector<Point2> v;
    for (int i = 0; i < n; ++i) {
        // Each vertex stays inside its own sector, so the order of angles is the order of the walk.
        const double sector = 360.0 / n;
        const double angle =
            (i + 0.1 + 0.8 * static_cast<double>(rng.in(0, 1000)) / 1000.0) * sector;
        const double radius = static_cast<double>(rng.in(reach * 45 / 100, reach));
        const double rad    = angle * 3.14159265358979323846 / 180.0;
        v.push_back(at(cx + static_cast<Mm>(std::llround(radius * std::cos(rad))),
                       cy + static_cast<Mm>(std::llround(radius * std::sin(rad)))));
    }
    CurvePath path;
    path.closed = true;
    for (int i = 0; i < n; ++i)
        path.pieces.push_back(
            segment(v[static_cast<std::size_t>(i)], v[static_cast<std::size_t>((i + 1) % n)]));
    return path;
}

/// A rectangle with all four corners rounded by a real arc, counter-clockwise: a parcel after
/// YUVARLA. `r` is under a third of the shorter side.
CurvePath rounded_rect(Mm x0, Mm y0, Mm w, Mm h, Mm r)
{
    const Mm x1 = x0 + w;
    const Mm y1 = y0 + h;
    CurvePath p;
    p.closed = true;
    p.pieces = {
        segment(at(x0 + r, y0), at(x1 - r, y0)),
        core::arc_piece(at(x1 - r, y0 + r), r, at(x1 - r, y0), at(x1, y0 + r), true),
        segment(at(x1, y0 + r), at(x1, y1 - r)),
        core::arc_piece(at(x1 - r, y1 - r), r, at(x1, y1 - r), at(x1 - r, y1), true),
        segment(at(x1 - r, y1), at(x0 + r, y1)),
        core::arc_piece(at(x0 + r, y1 - r), r, at(x0 + r, y1), at(x0, y1 - r), true),
        segment(at(x0, y1 - r), at(x0, y0 + r)),
        core::arc_piece(at(x0 + r, y0 + r), r, at(x0, y0 + r), at(x0 + r, y0), true),
    };
    return p;
}

CurvePath rect(Mm x0, Mm y0, Mm x1, Mm y1)
{
    CurvePath p;
    p.closed = true;
    p.pieces = {segment(at(x0, y0), at(x1, y0)), segment(at(x1, y0), at(x1, y1)),
                segment(at(x1, y1), at(x0, y1)), segment(at(x0, y1), at(x0, y0))};
    return p;
}

/// A random parcel: a star polygon or a rounded rectangle, by the seed's parity of luck.
CurvePath parcel(Rng& rng, Mm cx, Mm cy, Mm reach)
{
    if (rng.in(0, 1) == 0) return star(rng, cx, cy, reach);
    const Mm w = rng.in(reach, reach * 2);
    const Mm h = rng.in(reach, reach * 2);
    const Mm r = rng.in(w < h ? w / 10 : h / 10, (w < h ? w : h) / 3);
    return rounded_rect(cx - w / 2, cy - h / 2, w, h, r);
}

/// The length of a path's pieces, arcs by their chord: a scale for the tolerance, not a measure.
double perimeter(const CurvePath& path)
{
    double sum = 0.0;
    for (const PathPiece& piece : path.pieces)
        sum += std::hypot(static_cast<double>(piece.to.x - piece.from.x),
                          static_cast<double>(piece.to.y - piece.from.y));
    return sum;
}

/// Net area of a face: its boundary less its holes, in square millimetres.
double area_of(const KernelFace& face)
{
    double sum = std::abs(static_cast<double>(core::path_area(face.outer)));
    for (const CurvePath& hole : face.holes)
        sum -= std::abs(static_cast<double>(core::path_area(hole)));
    return sum;
}

double area_of(const std::vector<KernelFace>& faces)
{
    double sum = 0.0;
    for (const KernelFace& face : faces)
        sum += area_of(face);
    return sum;
}

/// Every face the kernel hands back is one it would take back: no crossing, no stray hole.
bool all_valid(const std::vector<KernelFace>& faces)
{
    for (const KernelFace& face : faces) {
        auto issue = core::kernel_face_issue(face);
        if (!issue || issue.value() != core::KernelFaceIssue::None) return false;
    }
    return true;
}

std::vector<KernelFace> run(const KernelFace& a, const KernelFace& b, BooleanOp op)
{
    const std::vector<KernelFace> left{a};
    const std::vector<KernelFace> right{b};
    auto result = core::kernel_boolean(left, right, op);
    REQUIRE_MESSAGE(result.ok(),
                    "boolean refused: " << (result.ok() ? "" : result.error().message));
    return result.value();
}

constexpr int kCases = 120;

} // namespace

TEST_CASE("ÖZELLİK: iki parselin birleşim ve kesişimi alanları korur (U + I = A + B)")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    for (int seed = 1; seed <= kCases; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(seed));
        const KernelFace a{parcel(rng, 0, 0, 12'000), {}};
        // The second parcel's centre is anywhere from on top of the first to just clear of it, so
        // the cases range over containment, overlap, a touch and a near miss.
        const KernelFace b{parcel(rng, rng.in(-14'000, 14'000), rng.in(-14'000, 14'000), 10'000),
                           {}};

        const double tol    = (perimeter(a.outer) + perimeter(b.outer)) * 1.0 + 100.0;
        const double area_a = area_of(a);
        const double area_b = area_of(b);

        const auto u = run(a, b, BooleanOp::Union);
        const auto i = run(a, b, BooleanOp::Intersection);
        const auto d = run(a, b, BooleanOp::Difference);
        const auto s = run(a, b, BooleanOp::SymmetricDifference);

        const bool valid = all_valid(u) && all_valid(i) && all_valid(d) && all_valid(s);
        CHECK_MESSAGE(valid, "bir sonuç yüz kendi denetiminden geçmiyor");
        CHECK_MESSAGE(std::abs(area_of(u) + area_of(i) - area_a - area_b) <= tol,
                      "U + I = A + B bozuldu: " << area_of(u) << " + " << area_of(i) << " ≠ "
                                                << area_a << " + " << area_b);
        CHECK_MESSAGE(std::abs(area_of(d) + area_of(i) - area_a) <= tol,
                      "(A − B) + (A ∩ B) = A bozuldu: " << area_of(d) << " + " << area_of(i)
                                                        << " ≠ " << area_a);
        CHECK_MESSAGE(std::abs(area_of(s) - (area_of(u) - area_of(i))) <= tol,
                      "simetrik fark = birleşim − kesişim bozuldu");
        CHECK_MESSAGE(area_of(i) <= std::min(area_a, area_b) + tol, "kesişim küçük olandan büyük");
        CHECK_MESSAGE(area_of(u) + tol >= std::max(area_a, area_b), "birleşim büyük olandan küçük");
    }
}

TEST_CASE("ÖZELLİK: bir parseli bir doğruyla bölüp birleştirmek parseli geri verir")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    // THE İFRAZ/TEVHİT PAIR. A parcel cut by a straight line into the part on one side and the
    // part on the other, and the two parts fused, is the parcel: the same area, and one face.
    for (int seed = 1; seed <= kCases; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(1000 + seed));
        const KernelFace parcel_face{parcel(rng, 0, 0, 12'000), {}};
        const double area = area_of(parcel_face);
        const double tol  = perimeter(parcel_face.outer) * 2.0 + 100.0;

        // The half-plane to the right of x = cut, as a box wider than any parcel here.
        const Mm cut = rng.in(-6'000, 6'000);
        const KernelFace right{rect(cut, -60'000, cut + 120'000, 60'000), {}};

        const auto on_right = run(parcel_face, right, BooleanOp::Intersection);
        const auto on_left  = run(parcel_face, right, BooleanOp::Difference);
        CHECK_MESSAGE(std::abs(area_of(on_right) + area_of(on_left) - area) <= tol,
                      "iki yarının alan toplamı parselin alanı değil: "
                          << area_of(on_right) << " + " << area_of(on_left) << " ≠ " << area);
        CHECK(all_valid(on_right));
        CHECK(all_valid(on_left));

        // Fused back. An empty side is a parcel the cut missed; then there is nothing to fuse.
        if (on_right.empty() || on_left.empty()) continue;
        auto fused = core::kernel_boolean(on_right, on_left, BooleanOp::Union);
        REQUIRE_MESSAGE(fused.ok(), "birleştirme reddedildi");
        CHECK_MESSAGE(fused.value().size() == 1, "birleşen parçalar tek yüz değil");
        CHECK_MESSAGE(std::abs(area_of(fused.value()) - area) <= tol,
                      "birleşen alan parselin alanından farklı: " << area_of(fused.value()) << " ≠ "
                                                                  << area);
    }
}

TEST_CASE("ÖZELLİK: parselin içine düşen bir adanın farkı tek yüz ve tek deliktir")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    for (int seed = 1; seed <= kCases; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(2000 + seed));
        // A courtyard: the same shape drawn much smaller about the same centre, so it is strictly
        // inside whatever the parcel is. A star shrunk about its centre stays inside a star.
        const KernelFace outer{rounded_rect(-15'000, -12'000, 30'000, 24'000, rng.in(2'000, 6'000)),
                               {}};
        const KernelFace court{star(rng, rng.in(-4'000, 4'000), rng.in(-3'000, 3'000), 3'500), {}};

        const double tol = (perimeter(outer.outer) + perimeter(court.outer)) * 1.0 + 100.0;
        const auto d     = run(outer, court, BooleanOp::Difference);

        REQUIRE_MESSAGE(d.size() == 1, "bir parselden bir ada çıkarılınca tek yüz kalmalı");
        CHECK_MESSAGE(d.front().holes.size() == 1, "ve onun tek deliği olmalı");
        CHECK(all_valid(d));
        CHECK_MESSAGE(std::abs(area_of(d) - (area_of(outer) - area_of(court))) <= tol,
                      "alan = parsel − ada bozuldu");
        // The hole is clockwise, the boundary counter-clockwise: the contract of `KernelFace`.
        CHECK(core::path_area(d.front().outer) > 0);
        CHECK(core::path_area(d.front().holes.front()) < 0);
    }
}

TEST_CASE("ÖZELLİK: aynı girdi aynı sonucu verir; işlenenlerin sırası alanı değiştirmez")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    for (int seed = 1; seed <= 40; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(3000 + seed));
        const KernelFace a{parcel(rng, 0, 0, 12'000), {}};
        const KernelFace b{parcel(rng, rng.in(-12'000, 12'000), rng.in(-12'000, 12'000), 10'000),
                           {}};

        // The same operation twice: the same faces, in the same order, to the vertex.
        for (const auto op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference,
                              BooleanOp::SymmetricDifference})
            CHECK_MESSAGE(run(a, b, op) == run(a, b, op), "aynı girdi iki sonuç verdi");

        // Union and intersection commute; the result is one set of faces however it is asked.
        CHECK(run(a, b, BooleanOp::Union) == run(b, a, BooleanOp::Union));
        CHECK(run(a, b, BooleanOp::Intersection) == run(b, a, BooleanOp::Intersection));

        // A parcel with itself: union and intersection are the parcel, the difference nothing.
        const double tol = perimeter(a.outer) + 100.0;
        CHECK(std::abs(area_of(run(a, a, BooleanOp::Union)) - area_of(a)) <= tol);
        CHECK(std::abs(area_of(run(a, a, BooleanOp::Intersection)) - area_of(a)) <= tol);
        CHECK(run(a, a, BooleanOp::Difference).empty());
    }
}

namespace {

/// A drawing and the means to run commands on it: what a person has open.
struct Drawing
{
    core::Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};

    Drawing()
    {
        command::register_builtin_commands(reg);
        bus.on_echo = [](std::string_view) {};
        REQUIRE(bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    }

    bool run(const std::string& line)
    {
        auto done = bus.execute_line(line, Origin::Test);
        INFO(line << " -> " << (done.ok() ? "ok" : done.error().message));
        return done.ok();
    }
};

/// A coordinate in metres to the millimetre, written the way a person types one.
std::string metres(Rng& rng, Mm base_m, Mm span_m)
{
    const Mm mm = rng.in(0, span_m * 1000);
    char text[48];
    (void)std::snprintf(text, sizeof text, "%lld.%03lld",
                        static_cast<long long>(base_m + mm / 1000),
                        static_cast<long long>(mm % 1000));
    return text;
}

std::string point(Rng& rng, Mm x0, Mm y0, Mm span)
{
    return metres(rng, x0, span) + "," + metres(rng, y0, span);
}

/// What a DXF round trip must keep of a drawing, read off the document.
struct Fingerprint
{
    std::size_t entities{0};
    std::set<std::string> layers;
    double area{0.0};
    core::Box2 extent{};
};

Fingerprint fingerprint(const core::Document& doc)
{
    Fingerprint f;
    f.entities = doc.live_entity_count();
    f.extent   = doc.extent();
    for (const core::Layer& layer : doc.layers())
        f.layers.insert(core::turkish_fold_key(layer.name));
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        f.area += std::abs(static_cast<double>(doc.entity_area(e)));
    return f;
}

/// A random drawing: one to three layers and three to ten lines, polylines, four-cornered parcels,
/// circles and arcs, at millimetre-precise TUREF coordinates.
///
/// Returns the sum of the perimeters drawn, in millimetres: the scale an area's rounding error
/// is measured against (a vertex moved half a millimetre moves an area by that times its edge).
double populate(Drawing& source, Rng& rng)
{
    double perimeters = 0.0;
    constexpr Mm x0   = 485'300;
    constexpr Mm y0   = 4'310'200;
    const int layers  = static_cast<int>(rng.in(1, 3));
    const int things  = static_cast<int>(rng.in(3, 10));
    for (int i = 0; i < things; ++i) {
        if (i < layers || rng.in(0, 3) == 0)
            REQUIRE(source.run("KATMAN ad=KAT" + std::to_string(rng.in(1, layers))));
        switch (rng.in(0, 4)) {
        case 0:
            REQUIRE(source.run("ÇİZGİ " + point(rng, x0, y0, 400) + " " + point(rng, x0, y0, 400)));
            perimeters += 400'000.0;
            break;
        case 1: {
            std::string line = "ÇOKLUÇİZGİ";
            for (Mm k = 0, n = rng.in(3, 6); k < n; ++k)
                line += " " + point(rng, x0, y0, 400);
            REQUIRE(source.run(line));
            perimeters += 5 * 400'000.0;
            break;
        }
        case 2: {
            // A four-cornered parcel, walked around a centre so it never crosses itself.
            const Mm cx = x0 + rng.in(50, 350);
            const Mm cy = y0 + rng.in(50, 350);
            const Mm w  = rng.in(8, 40);
            const Mm h  = rng.in(8, 40);
            REQUIRE(source.run("ALAN noktalar=" + std::to_string(cx - w) + "," +
                               std::to_string(cy - h) + " " + std::to_string(cx + w) + "," +
                               std::to_string(cy - h) + " " + std::to_string(cx + w) + "," +
                               std::to_string(cy + h) + " " + std::to_string(cx - w) + "," +
                               std::to_string(cy + h)));
            break;
        }
        case 3: {
            const Mm cx = x0 + rng.in(30, 370);
            const Mm cy = y0 + rng.in(30, 370);
            REQUIRE(source.run("DAİRE merkez=" + std::to_string(cx) + "," + std::to_string(cy) +
                               " cevre=" + std::to_string(cx + rng.in(2, 25)) + "," +
                               std::to_string(cy)));
            perimeters += 2.0 * 3.14159265358979 * 25'000.0;
            break;
        }
        default: {
            const Mm cx = x0 + rng.in(30, 370);
            const Mm cy = y0 + rng.in(30, 370);
            const Mm r  = rng.in(3, 25);
            REQUIRE(source.run("YAY merkez=" + std::to_string(cx) + "," + std::to_string(cy) +
                               " baslangic=" + std::to_string(cx + r) + "," + std::to_string(cy) +
                               " bitis=" + std::to_string(cx) + "," + std::to_string(cy + r)));
            perimeters += 2.0 * 3.14159265358979 * static_cast<double>(r) * 1000.0;
            break;
        }
        }
    }
    return perimeters;
}

} // namespace

TEST_CASE("ÖZELLİK: DXF'e yazılıp geri okunan çizim sayısını, kapsamını ve alanını korur")
{
    if (!io::vector_backend_available())
        PENDING("PIRICAD_WITH_GDAL=OFF; DXF gidiş-dönüşü sınanamıyor.");

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "piricad-ozellik-dxf";
    std::filesystem::create_directories(dir);

    for (int seed = 1; seed <= 40; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(4000 + seed));
        Drawing source;

        (void)populate(source, rng);
        const Fingerprint before = fingerprint(source.doc);
        REQUIRE(before.entities >= 1);

        const std::string path = (dir / ("tohum-" + std::to_string(seed) + ".dxf")).string();
        auto written = source.bus.execute_line("DIŞAAKTAR \"" + path + "\"", Origin::Test);
        REQUIRE_MESSAGE(written.ok(),
                        "DXF yazılamadı: " << (written.ok() ? "" : written.error().message));

        Drawing target;
        auto read = target.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
        REQUIRE_MESSAGE(read.ok(), "DXF okunamadı: " << (read.ok() ? "" : read.error().message));
        const Fingerprint after = fingerprint(target.doc);

        CHECK_MESSAGE(after.entities == before.entities,
                      "nesne sayısı değişti: " << before.entities << " -> " << after.entities);
        // Layers survive by name (a drawing always has the default one besides).
        for (const std::string& name : before.layers)
            CHECK_MESSAGE(after.layers.contains(name), "katman yitti: " << name);
        // The box to the millimetre — and, for arcs and circles, to a millimetre of the
        // bulge, which is what the format stores.
        const auto close = [](Mm a, Mm b) { return std::abs(static_cast<double>(a - b)) <= 2.0; };
        const bool same_box = close(after.extent.min_x, before.extent.min_x) &&
                              close(after.extent.max_x, before.extent.max_x) &&
                              close(after.extent.min_y, before.extent.min_y) &&
                              close(after.extent.max_y, before.extent.max_y);
        CHECK_MESSAGE(same_box, "kapsam kutusu kaydı");
        CHECK_MESSAGE(std::abs(after.area - before.area) <=
                          1e-6 * std::max(1.0, before.area) + 1000.0,
                      "alan toplamı değişti: " << before.area << " -> " << after.area);
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("ÖZELLİK: yuvarlak köşeli parselin ofseti Steiner formülünü verir (A + P·d + π·d²)")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    constexpr double kPi = 3.14159265358979323846;
    for (int seed = 1; seed <= kCases; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(5000 + seed));
        const Mm w           = rng.in(12'000, 60'000);
        const Mm h           = rng.in(12'000, 60'000);
        const Mm r           = rng.in(1'500, (w < h ? w : h) / 3);
        const CurvePath path = rounded_rect(-w / 2, -h / 2, w, h, r);

        // A CONVEX SHAPE WITH ROUND CORNERS has exact figures: its perimeter is the straight
        // sides and the four quarter-arcs, its area the rectangle less the four corners' lost
        // squares-less-quarter-discs. Grown by d, a convex shape gains P·d + π·d² (Steiner); a
        // smooth one shrunk by d no deeper than its smallest radius loses P·d and gains π·d².
        const double wd = static_cast<double>(w);
        const double hd = static_cast<double>(h);
        const double rd = static_cast<double>(r);
        const double perimeter_exact =
            2.0 * (wd - 2.0 * rd) + 2.0 * (hd - 2.0 * rd) + 2.0 * kPi * rd;
        const double area_exact = wd * hd - (4.0 - kPi) * rd * rd;
        REQUIRE_MESSAGE(std::abs(static_cast<double>(core::path_area(path)) - area_exact) <=
                            perimeter_exact * 1.0,
                        "parselin kendi alanı formülle uyuşmuyor");

        // Outward.
        const Mm out = rng.in(500, 8'000);
        auto grown   = core::kernel_offset(path, out, core::OffsetCorner::Round, false);
        REQUIRE_MESSAGE(grown.ok(), "dış ofset reddedildi");
        REQUIRE(grown.value().size() == 1);
        const double d_out    = static_cast<double>(out);
        const double want_out = area_exact + perimeter_exact * d_out + kPi * d_out * d_out;
        const double got_out  = static_cast<double>(core::path_area(grown.value().front()));
        const double tol_out  = (perimeter_exact + 2.0 * kPi * d_out) * 1.0 + 100.0;
        CHECK_MESSAGE(std::abs(got_out - want_out) <= tol_out,
                      "dış ofsetin alanı Steiner'den uzak: " << got_out << " ≠ " << want_out);

        // Inward, no deeper than the corner radius.
        const Mm in = rng.in(300, r - 200);
        auto shrunk = core::kernel_offset(path, -in, core::OffsetCorner::Round, false);
        REQUIRE_MESSAGE(shrunk.ok(), "iç ofset reddedildi");
        REQUIRE(shrunk.value().size() == 1);
        const double d_in    = static_cast<double>(in);
        const double want_in = area_exact - perimeter_exact * d_in + kPi * d_in * d_in;
        const double got_in  = static_cast<double>(core::path_area(shrunk.value().front()));
        CHECK_MESSAGE(std::abs(got_in - want_in) <= perimeter_exact * 1.0 + 100.0,
                      "iç ofsetin alanı formülden uzak: " << got_in << " ≠ " << want_in);

        // And back: out by d and in by d is the parcel, in area (a convex shape's parallel
        // body is undone exactly by the inverse offset).
        auto back =
            core::kernel_offset(grown.value().front(), -out, core::OffsetCorner::Round, false);
        const bool one_path = back.ok() && back.value().size() == 1;
        REQUIRE_MESSAGE(one_path, "geri ofset tek yol vermedi");
        CHECK_MESSAGE(std::abs(static_cast<double>(core::path_area(back.value().front())) -
                               area_exact) <= (perimeter_exact + 2.0 * kPi * d_out) * 2.0 + 100.0,
                      "dışa sonra içe ofset parseli geri vermedi");
    }
}

TEST_CASE("ÖZELLİK: tersi olan dönüşüm uygulanıp geri alınınca çizim aynen kalır")
{
    // Moved and moved back, turned four quarter-turns, mirrored twice over the same upright axis,
    // doubled and halved: each is an exact integer operation on whole millimetres, so the drawing
    // must come back EXACTLY — the same content hash, not "close". A single millimetre of drift is
    // a parcel that is no longer where its owner's deed puts it.
    for (int seed = 1; seed <= 60; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(6000 + seed));
        Drawing d;
        (void)populate(d, rng);
        const std::uint64_t original = d.doc.content_hash();
        const std::size_t count      = d.doc.live_entity_count();
        const std::string centre     = point(rng, 485'300, 4'310'200, 400);

        const auto everything = [&d] { return d.run("SEÇ mod=TÜMÜ"); };

        // Move there and back.
        const std::string from = point(rng, 485'300, 4'310'200, 400);
        const std::string to   = point(rng, 485'300, 4'310'200, 400);
        REQUIRE(everything());
        REQUIRE(d.run("TAŞI baslangic=" + from + " bitis=" + to));
        REQUIRE(everything());
        REQUIRE(d.run("TAŞI baslangic=" + to + " bitis=" + from));
        CHECK_MESSAGE(d.doc.content_hash() == original, "taşı + geri taşı çizimi bozdu");

        // Four quarter-turns about one centre.
        for (int turn = 0; turn < 4; ++turn) {
            REQUIRE(everything());
            REQUIRE(d.run("DÖNDÜR merkez=" + centre + " aci=90"));
        }
        CHECK_MESSAGE(d.doc.content_hash() == original, "dört çeyrek dönüş çizimi bozdu");

        // Mirrored twice over the same vertical axis (an arbitrary slanted axis rounds, so is
        // held to a millimetre below instead).
        const std::string axis_x = metres(rng, 485'300, 400);
        for (int pass = 0; pass < 2; ++pass) {
            REQUIRE(everything());
            REQUIRE(d.run("AYNALA baslangic=" + axis_x + ",4310100 bitis=" + axis_x + ",4310700"));
        }
        CHECK_MESSAGE(d.doc.content_hash() == original, "iki kez aynalama çizimi bozdu");

        // Doubled, then halved, about one centre.
        REQUIRE(everything());
        REQUIRE(d.run("ÖLÇEKLE merkez=" + centre + " carpan=2"));
        REQUIRE(everything());
        REQUIRE(d.run("ÖLÇEKLE merkez=" + centre + " carpan=0.5"));
        CHECK_MESSAGE(d.doc.content_hash() == original, "iki kat ve yarısı çizimi bozdu");
        CHECK(d.doc.live_entity_count() == count);
    }
}

TEST_CASE(
    "ÖZELLİK: herhangi bir açıyla dönen çizim alanını ve kapsamını korur, ters açıyla geri gelir")
{
    for (int seed = 1; seed <= 60; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(7000 + seed));
        Drawing d;
        const double perimeters  = populate(d, rng);
        const Fingerprint before = fingerprint(d.doc);
        const std::string centre = point(rng, 485'300, 4'310'200, 400);
        const std::int64_t angle = rng.in(1, 359);

        REQUIRE(d.run("SEÇ mod=TÜMÜ"));
        REQUIRE(d.run("DÖNDÜR merkez=" + centre + " aci=" + std::to_string(angle)));
        const Fingerprint turned = fingerprint(d.doc);

        // A rotation keeps every area, to the rounding of its vertices: a millimetre of each
        // vertex times the edge it ends, which the drawn perimeters bound.
        CHECK_MESSAGE(std::abs(turned.area - before.area) <= perimeters * 1.0 + 2000.0,
                      "dönme alanı değiştirdi (" << angle << "°): " << before.area << " -> "
                                                 << turned.area);

        // And the opposite angle brings every vertex back within a couple of millimetres.
        REQUIRE(d.run("SEÇ mod=TÜMÜ"));
        REQUIRE(d.run("DÖNDÜR merkez=" + centre + " aci=" + std::to_string(-angle)));
        const Fingerprint back = fingerprint(d.doc);
        const auto close = [](Mm a, Mm b) { return std::abs(static_cast<double>(a - b)) <= 3.0; };
        const bool same_box = close(back.extent.min_x, before.extent.min_x) &&
                              close(back.extent.max_x, before.extent.max_x) &&
                              close(back.extent.min_y, before.extent.min_y) &&
                              close(back.extent.max_y, before.extent.max_y);
        CHECK_MESSAGE(same_box, "ters açı çizimi yerine getirmedi (" << angle << "°)");
        CHECK(back.entities == before.entities);
    }
}

namespace {

/// A coordinate in whole millimetres, as the metres a person types: 485320150 -> "485320.150".
std::string mm_text(Mm mm)
{
    char text[48];
    (void)std::snprintf(text, sizeof text, "%lld.%03lld", static_cast<long long>(mm / 1000),
                        static_cast<long long>(mm % 1000));
    return text;
}

std::string mm_point(Point2 p)
{
    return mm_text(p.x) + "," + mm_text(p.y);
}

double distance(Point2 a, Point2 b)
{
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

/// Distance of `p` from the infinite line through a and b.
double off_line(Point2 p, Point2 a, Point2 b)
{
    const double bx = static_cast<double>(b.x - a.x);
    const double by = static_cast<double>(b.y - a.y);
    const double px = static_cast<double>(p.x - a.x);
    const double py = static_cast<double>(p.y - a.y);
    return std::abs(bx * py - by * px) / std::hypot(bx, by);
}

} // namespace

TEST_CASE(
    "ÖZELLİK: yakalama uca birebir, ortaya bir milimetre içinde, çizgi üstüne çizgi üstünde iner")
{
    constexpr Mm radius = 400;
    for (int seed = 1; seed <= 80; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(8000 + seed));
        Drawing d;
        const Point2 a = at(rng.in(0, 300'000), rng.in(0, 300'000));
        const Point2 b = at(rng.in(0, 300'000), rng.in(0, 300'000));
        if (distance(a, b) < 8'000.0) continue; // too short for the aims below to be told apart
        REQUIRE(d.run("ÇİZGİ " + mm_point(a) + " " + mm_point(b)));

        const auto shake = [&rng](Point2 p, Mm reach) {
            return Point2{p.x + rng.in(-reach, reach), p.y + rng.in(-reach, reach)};
        };

        // THE END: an aim within the radius lands exactly on the vertex, whatever the shake.
        for (const Point2 end : {a, b}) {
            core::SnapQuery q;
            q.aim                    = shake(end, 150);
            q.radius                 = radius;
            q.modes                  = core::SnapEndpoint;
            const core::SnapResult r = core::snap(d.doc, q);
            const bool on_end        = r.mode == core::SnapEndpoint && r.point == end;
            CHECK_MESSAGE(on_end, "uç noktaya yapışmadı: " << r.point.x - end.x << ", "
                                                           << r.point.y - end.y);
        }

        // THE MIDDLE: within a millimetre of the arithmetic middle.
        const Point2 mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
        core::SnapQuery m;
        m.aim                     = shake(mid, 150);
        m.radius                  = radius;
        m.modes                   = core::SnapMidpoint;
        const core::SnapResult rm = core::snap(d.doc, m);
        const bool on_mid         = rm.mode == core::SnapMidpoint && distance(rm.point, mid) <= 1.5;
        CHECK_MESSAGE(on_mid, "orta noktaya yapışmadı: " << distance(rm.point, mid) << " mm");

        // THE NEAREST POINT: on the line, and never farther from the aim than the aim was.
        const Point2 along{a.x + (b.x - a.x) * 3 / 10, a.y + (b.y - a.y) * 3 / 10};
        core::SnapQuery n;
        n.aim                     = shake(along, 250);
        n.radius                  = radius;
        n.modes                   = core::SnapNearest;
        const core::SnapResult rn = core::snap(d.doc, n);
        CHECK_MESSAGE(rn.mode == core::SnapNearest, "en yakına yapışmadı");
        CHECK_MESSAGE(off_line(rn.point, a, b) <= 1.5, "en yakın nokta çizgi üstünde değil");
        CHECK_MESSAGE(distance(rn.point, n.aim) <= off_line(n.aim, a, b) + 1.5,
                      "en yakın nokta, imlecin çizgiye uzaklığından uzakta");

        // THE FOOT OF A PERPENDICULAR from a point not on the line: on the line, and the way from
        // the base to it runs at a right angle to the line.
        const Point2 base = at(rng.in(0, 300'000), rng.in(0, 300'000));
        if (off_line(base, a, b) < 3'000.0) continue;
        const double ux   = static_cast<double>(b.x - a.x);
        const double uy   = static_cast<double>(b.y - a.y);
        const double len2 = ux * ux + uy * uy;
        const double t =
            ((static_cast<double>(base.x - a.x)) * ux + (static_cast<double>(base.y - a.y)) * uy) /
            len2;
        if (t < 0.05 || t > 0.95) continue; // the foot must fall on the segment itself
        const Point2 foot{a.x + std::llround(t * ux), a.y + std::llround(t * uy)};
        core::SnapQuery p;
        p.aim                     = shake(foot, 150);
        p.radius                  = radius;
        p.modes                   = core::SnapPerpendicular;
        p.has_base                = true;
        p.base                    = base;
        const core::SnapResult rp = core::snap(d.doc, p);
        CHECK_MESSAGE(rp.mode == core::SnapPerpendicular, "dik ayağa yapışmadı");
        const double to_x   = static_cast<double>(rp.point.x - base.x);
        const double to_y   = static_cast<double>(rp.point.y - base.y);
        const double cosine = (to_x * ux + to_y * uy) / (std::hypot(to_x, to_y) * std::sqrt(len2));
        CHECK_MESSAGE(off_line(rp.point, a, b) <= 1.5, "dik ayak çizgi üstünde değil");
        CHECK_MESSAGE(std::abs(cosine) <= 1e-3, "dik ayak dik değil: cos = " << cosine);
    }
}

TEST_CASE("ÖZELLİK: yakalama dairenin merkezine ve dört çeyrek noktasına birebir iner")
{
    for (int seed = 1; seed <= 60; ++seed) {
        INFO("tohum " << seed);
        Rng rng(static_cast<std::uint64_t>(9000 + seed));
        Drawing d;
        const Point2 centre = at(rng.in(0, 300'000), rng.in(0, 300'000));
        const Mm r          = rng.in(3'000, 40'000);
        REQUIRE(d.run("DAİRE merkez=" + mm_point(centre) +
                      " cevre=" + mm_point(Point2{centre.x + r, centre.y})));

        core::SnapQuery q;
        q.aim    = Point2{centre.x + rng.in(-120, 120), centre.y + rng.in(-120, 120)};
        q.radius = 400;
        q.modes  = core::SnapCenter;
        const core::SnapResult rc = core::snap(d.doc, q);
        const bool on_centre      = rc.mode == core::SnapCenter && rc.point == centre;
        CHECK_MESSAGE(on_centre, "merkeze birebir yapışmadı");

        // A quadrant of the circle, by the same kind of aim: on the circle to a millimetre.
        const Point2 east{centre.x + r, centre.y};
        core::SnapQuery e;
        e.aim                     = Point2{east.x + rng.in(-120, 120), east.y + rng.in(-120, 120)};
        e.radius                  = 400;
        e.modes                   = core::SnapEndpoint | core::SnapNearest;
        const core::SnapResult re = core::snap(d.doc, e);
        CHECK_MESSAGE(std::abs(distance(re.point, centre) - static_cast<double>(r)) <= 1.5,
                      "yakalanan nokta çemberin üstünde değil");
    }
}
