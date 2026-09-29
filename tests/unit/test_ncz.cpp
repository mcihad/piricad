// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: the Netcad NCZ reader (io/ncz.hpp).
//
// The seeds are synthetic drawings written by scripts/ncz-tohum.py; a real plan
// is read only when KENTOS_TEST_NCZ names one, because a user's NCZ is their
// municipality's data and never enters the repository.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/io/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace kentos;
using namespace kentos::command;
namespace fs = std::filesystem;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    std::string transcript;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { transcript.append(s).append("\n"); };
    }

    core::Result<DispatchResult> import(const std::string& path)
    {
        return bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    }
};

std::string seed(const char* name)
{
    return (fs::path(KENTOS_FUZZ_DIR) / "tohum" / "ncz" / name).string();
}

using Corner = std::pair<core::Mm, core::Mm>; ///< easting, northing

/// The first ring of every area on `layer`, its corners as stored.
std::vector<std::vector<Corner>> rings_on(const core::Document& doc, const std::string& layer)
{
    std::vector<std::vector<Corner>> out;
    const core::LayerId slot = doc.layer_table().find(layer);
    if (slot == core::kNoLayer) return out;
    const core::EntityTable& ents = doc.entities();
    for (core::EntityId e = 0; e < ents.size(); ++e) {
        if (!doc.alive(e) || ents.layer[e] != slot) continue;
        const core::RingSpan span = doc.geometry().rings_of(ents.slot[e]);
        if (span.count == 0) continue;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        std::vector<Corner> ring;
        for (std::size_t i = 0; i < xs.size(); ++i)
            ring.emplace_back(xs[i], ys[i]);
        out.push_back(std::move(ring));
    }
    return out;
}

/// Corners as a sorted set, so a ring compares whatever vertex it starts on.
std::vector<Corner> sorted(std::vector<Corner> ring)
{
    std::sort(ring.begin(), ring.end());
    return ring;
}

/// The area two convex rings share, square metres: one clipped by the other,
/// edge by edge (Sutherland–Hodgman). A test oracle, independent of the
/// reader's own geometry.
double overlap_m2(const std::vector<Corner>& a, const std::vector<Corner>& b)
{
    using P = std::pair<double, double>;
    std::vector<P> poly;
    for (auto [x, y] : a)
        poly.emplace_back(double(x) / 1000.0, double(y) / 1000.0);
    // Clip against each edge of b, taken counter-clockwise (left is inside).
    double signed2 = 0.0;
    for (std::size_t i = 0; i < b.size(); ++i) {
        const auto& p = b[i];
        const auto& q = b[(i + 1) % b.size()];
        signed2 += double(p.first) * double(q.second) - double(q.first) * double(p.second);
    }
    std::vector<P> clip;
    for (auto [x, y] : b)
        clip.emplace_back(double(x) / 1000.0, double(y) / 1000.0);
    if (signed2 < 0) std::reverse(clip.begin(), clip.end());
    for (std::size_t i = 0; i < clip.size() && !poly.empty(); ++i) {
        const P e0      = clip[i];
        const P e1      = clip[(i + 1) % clip.size()];
        const auto side = [&](const P& p) {
            return (e1.first - e0.first) * (p.second - e0.second) -
                   (e1.second - e0.second) * (p.first - e0.first);
        };
        std::vector<P> next;
        for (std::size_t k = 0; k < poly.size(); ++k) {
            const P cur     = poly[k];
            const P prev    = poly[(k + poly.size() - 1) % poly.size()];
            const double sc = side(cur), sp = side(prev);
            if ((sc >= 0) != (sp >= 0)) {
                const double t = sp / (sp - sc);
                next.emplace_back(prev.first + t * (cur.first - prev.first),
                                  prev.second + t * (cur.second - prev.second));
            }
            if (sc >= 0) next.push_back(cur);
        }
        poly = std::move(next);
    }
    double area2 = 0.0;
    for (std::size_t k = 0; k < poly.size(); ++k) {
        const P& p = poly[k];
        const P& q = poly[(k + 1) % poly.size()];
        area2 += p.first * q.second - q.first * p.second;
    }
    return std::fabs(area2) / 2.0;
}

/// The four 1:1000 sheets of seed 08 (scripts/ncz-tohum.py `SHEETS`): the box
/// the file stores, north and east of its south-west and north-east.
struct SeedSheet
{
    double min_n, min_e, max_n, max_e;
};

constexpr SeedSheet kSheets[] = {
    {4450747.6869809423, 421758.8335489006, 4451447.1806906024, 422298.2273740889},
    {4450742.2352196742, 422291.0940323713, 4451441.6912473785, 422830.4388321189},
    {4451441.6912473785, 421766.0157596478, 4452141.1858941708, 422305.3616443881},
    {4451436.2392827179, 422298.2273740889, 4452135.6962464191, 422837.5242272436},
};

core::Mm mm(double metres)
{
    return static_cast<core::Mm>(std::llround(metres * 1000.0));
}

} // namespace

TEST_CASE("NCZ: pafta dosyanın bildirdiği dilimde dönük dörtgen olarak çizilir; komşular köşe "
          "köşe buluşur, sınırlayıcı kutular gibi üst üste binmez")
{
    if (!io::vector_backend_available())
        PENDING("GDAL kapalı; paftanın gerçek çerçevesi bu yapıda kurulamaz, kutusu çizilir.");
    Rig rig;
    REQUIRE(rig.import(seed("08-paftalar.ncz")).ok());

    // The corners PROJ gives the four 22,5″ cells in TM39 (GRS80), easting and
    // northing in millimetres, south-west first: worked out once with cs2cs.
    const std::vector<std::vector<Corner>> expected{
        {{421758834, 4450753176},
         {422291094, 4450747687},
         {422298227, 4451441691},
         {421766016, 4451447181}},
        {{422291094, 4450747687},
         {422823354, 4450742235},
         {422830439, 4451436239},
         {422298227, 4451441691}},
        {{421766016, 4451447181},
         {422298227, 4451441691},
         {422305362, 4452135696},
         {421773199, 4452141186}},
        {{422298227, 4451441691},
         {422830439, 4451436239},
         {422837524, 4452130244},
         {422305362, 4452135696}},
    };
    const auto rings = rings_on(rig.doc, "PINDEX_1000");
    REQUIRE(rings.size() == 5);
    for (const auto& want : expected) {
        const bool found = std::any_of(rings.begin(), rings.end(), [&](const auto& ring) {
            return sorted(ring) == sorted(want);
        });
        CHECK(found);
    }

    // The block's centre is a corner of all four sheets, and no two overlap.
    const Corner centre{422298227, 4451441691};
    int sharing = 0;
    for (const auto& ring : rings)
        sharing += static_cast<int>(std::count(ring.begin(), ring.end(), centre));
    CHECK(sharing == 4);
    double overlap = 0.0;
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i + 1; j < 4; ++j)
            overlap += overlap_m2(expected[i], expected[j]);
    CHECK(overlap < 0.01);
    // The boxes the file stores DO overlap: that was the fault.
    const auto box = [](const SeedSheet& s) {
        return std::vector<Corner>{{mm(s.min_e), mm(s.min_n)},
                                   {mm(s.max_e), mm(s.min_n)},
                                   {mm(s.max_e), mm(s.max_n)},
                                   {mm(s.min_e), mm(s.max_n)}};
    };
    CHECK(overlap_m2(box(kSheets[0]), box(kSheets[2])) > 1000.0);

    // A local sheet — a rectangle in the projection, on no graticule — keeps
    // its box, and both outcomes are said.
    const std::vector<Corner> local{{421000000, 4448000000},
                                    {421250000, 4448000000},
                                    {421250000, 4448400000},
                                    {421000000, 4448400000}};
    CHECK(std::any_of(rings.begin(), rings.end(),
                      [&](const auto& ring) { return sorted(ring) == sorted(local); }));
    CHECK(rig.transcript.find("4 pafta çerçevesi, dosyanın bildirdiği ITRF, 3° dilim, orta "
                              "meridyen 39° sisteminde gerçek biçimiyle, dönük dörtgen olarak "
                              "çizildi") != std::string::npos);
    CHECK(rig.transcript.find("1 pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi: "
                              "kutusu bir enlem-boylam paftasına oturmuyor") != std::string::npos);

    // One import, one undo step.
    REQUIRE(rig.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(rig.doc.live_entity_count() == 0);
}

TEST_CASE("NCZ: sistem bildirmeyen dosyada pafta sakladığı kutuyla çizilir ve neden söylenir")
{
    Rig rig;
    REQUIRE(rig.import(seed("09-paftalar-sistemsiz.ncz")).ok());
    const auto rings = rings_on(rig.doc, "PINDEX_1000");
    REQUIRE(rings.size() == 4);
    for (const SeedSheet& s : kSheets) {
        const std::vector<Corner> box{{mm(s.min_e), mm(s.min_n)},
                                      {mm(s.max_e), mm(s.min_n)},
                                      {mm(s.max_e), mm(s.max_n)},
                                      {mm(s.min_e), mm(s.max_n)}};
        CHECK(std::any_of(rings.begin(), rings.end(),
                          [&](const auto& ring) { return sorted(ring) == sorted(box); }));
    }
    CHECK(rig.transcript.find("4 pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi, "
                              "çünkü dosya koordinat sistemi bildirmiyor") != std::string::npos);
}

TEST_CASE("NCZ: gerçek bir planın pafta indeksi boşluksuz ve bindirmesiz döşenir")
{
    const char* real = std::getenv("KENTOS_TEST_NCZ");
    if (real == nullptr) PENDING("KENTOS_TEST_NCZ bir .ncz dosyası göstermiyor.");
    if (!io::vector_backend_available())
        PENDING("GDAL kapalı; paftanın gerçek çerçevesi bu yapıda kurulamaz.");
    Rig rig;
    REQUIRE(rig.import(real).ok());

    std::size_t sheets = 0;
    for (const core::Layer& layer : rig.doc.layers()) {
        if (layer.name.rfind("PINDEX", 0) != 0) continue;
        const auto rings = rings_on(rig.doc, layer.name);
        sheets += rings.size();
        for (const auto& ring : rings)
            CHECK(ring.size() == 4);
        double overlap = 0.0;
        for (std::size_t i = 0; i < rings.size(); ++i)
            for (std::size_t j = i + 1; j < rings.size(); ++j)
                overlap += overlap_m2(rings[i], rings[j]);
        const std::string said = layer.name + ": " + std::to_string(rings.size()) + " pafta, " +
                                 std::to_string(overlap) + " m² bindirme";
        INFO(said);
        CHECK(overlap < 1.0);
    }
    const std::string total = std::to_string(sheets) + " pafta sınandı";
    MESSAGE(total);
}
