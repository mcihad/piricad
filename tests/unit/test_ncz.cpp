// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — tests: the Netcad NCZ reader (io/ncz.hpp).
//
// The seeds are synthetic drawings written by scripts/ncz-tohum.py; a real plan
// is read only when PIRICAD_TEST_NCZ names one, because a user's NCZ is their
// municipality's data and never enters the repository.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/domain/geodesy/crs_catalog.hpp"
#include "piricad/domain/geodesy/crs_service.hpp"
#include "piricad/io/service.hpp"
#include "piricad/io/vector.hpp"
#include "piricad/script/json_runner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace piricad;
using namespace piricad::command;
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
    return (fs::path(PIRICAD_FUZZ_DIR) / "tohum" / "ncz" / name).string();
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

/// Live entities on `layer`, or -1 when the drawing has no such layer.
long long on_layer(const core::Document& doc, const std::string& layer)
{
    const core::LayerId slot = doc.layer_table().find(layer);
    if (slot == core::kNoLayer) return -1;
    return static_cast<long long>(doc.layer_entity_count(slot));
}

/// Live entities of `kind`, block members included.
std::size_t of_kind(const core::Document& doc, core::KindId kind)
{
    std::size_t n = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.alive(e) && doc.entities().kind[e] == kind) ++n;
    return n;
}

/// The text a column holds for each live entity that has one, in slot order.
std::vector<std::string> column_texts(const core::Document& doc, const char* id)
{
    std::vector<std::string> out;
    const core::AttrId col = doc.attributes().find(id);
    if (col == core::kNoAttr) return out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        if (auto v = doc.attribute(col, e); v && v.value().present) out.push_back(v.value().text);
    }
    return out;
}

bool has(const std::string& transcript, const std::string& words)
{
    return transcript.find(words) != std::string::npos;
}

constexpr const char* kSeeds[] = {
    "01-her-tur.ncz",
    "02-akilli-nesne.ncz",
    "03-gec-tablolar.ncz",
    "04-bozuk-kayitlar.ncz",
    "05-oznitelik-tablolari.ncz",
    "06-kesik.ncz",
    "07-akilli-nesneler.ncz",
    "08-paftalar.ncz",
    "09-paftalar-sistemsiz.ncz",
    "10-cografi.ncz",
    "11-cografi-bildirim-metre.ncz",
};

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
    const char* real = std::getenv("PIRICAD_TEST_NCZ");
    if (real == nullptr) PENDING("PIRICAD_TEST_NCZ bir .ncz dosyası göstermiyor.");
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

TEST_CASE("NCZ: her tohum ya okunur ya da çizime hiç dokunmadan reddedilir")
{
    for (const char* name : kSeeds) {
        Rig rig;
        const std::uint64_t before = rig.doc.content_hash();
        auto r                     = rig.import(seed(name));
        INFO(name);
        if (!r.ok()) {
            // io.md R17/P11: a failed import leaves the drawing byte for byte as
            // it was, and no undo step behind.
            CHECK(rig.doc.content_hash() == before);
            CHECK(!rig.undo.can_undo());
            continue;
        }
        CHECK(rig.doc.live_entity_count() > 0);
        for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e)
            if (rig.doc.alive(e)) CHECK(rig.doc.entities().layer[e] < rig.doc.layers().size());
    }
}

TEST_CASE("NCZ: okunacak geometrisi olmayan dosya adıyla reddedilir")
{
    Rig rig;
    const std::uint64_t before = rig.doc.content_hash();
    auto r                     = rig.import(seed("04-bozuk-kayitlar.ncz"));
    REQUIRE(!r.ok());
    CHECK(has(r.error().message, "okunabilir geometri içermiyor; çizime hiçbir şey eklenmedi"));
    CHECK(rig.doc.content_hash() == before);
    CHECK(!rig.undo.can_undo());
}

TEST_CASE("NCZ: her tür kendi karşılığına düşer; katmanlar, renkler, nokta adları ve kayıplar "
          "söylenir")
{
    Rig rig;
    REQUIRE(rig.import(seed("01-her-tur.ncz")).ok());
    const core::Document& d = rig.doc;
    CHECK(d.live_entity_count() == 18);

    // A circle is a circle and an arc an arc, not the plugin's 72 and 48 chords.
    CHECK(of_kind(d, core::kCircleKind) == 1);
    CHECK(of_kind(d, core::kArcKind) == 2);
    CHECK(of_kind(d, core::kPointKind) == 5);

    // The file's layers with the file's colours (LEX.ST2). Layer code 4 names a
    // blank layer, which the plugin leaves out of its list, so the codes after
    // it shift — read exactly as the plugin reads them; code 6 names none.
    CHECK(on_layer(d, "PARSEL") == 5);
    CHECK(on_layer(d, "YAZI") == 5);
    CHECK(on_layer(d, "İMAR_ŞERİT") == 4);
    CHECK(on_layer(d, "PAFTA") == 3);
    CHECK(on_layer(d, "KATMAN_6") == 1);
    const core::LayerId parsel = d.layer_table().find("PARSEL");
    REQUIRE(parsel != core::kNoLayer);
    CHECK(d.layers()[parsel].appearance.rgba == 0xFFFF0000u);
    const core::LayerId imar = d.layer_table().find("İMAR_ŞERİT");
    REQUIRE(imar != core::kNoLayer);
    CHECK(d.layers()[imar].appearance.rgba == 0xFF008000u);

    // A point's name lands where NOKTALAR writes it, a nested one too.
    CHECK(column_texts(d, "nokta_no") == std::vector<std::string>{"1284", "P2", "İÇ"});

    // What is not read is counted and named (io.md P11).
    CHECK(has(rig.transcript, "2 kayıt bu okuyucunun tanımadığı NCZ geometri türlerinde (8, 14)"));
    CHECK(has(rig.transcript, "1 nesnenin çizgi kalınlığı okundu (0,20–0,20 mm)"));
    // A point's height is not written as `kot`, and that is said.
    CHECK(rig.doc.attributes().find("kot") == core::kNoAttr);
    CHECK(has(rig.transcript, "1 noktada dosyanın yükseklik alanı dolu (1088,50–1088,50 m); bu "
                              "okuyucu onu kot olarak yazmaz."));
}

TEST_CASE("NCZ: katmanlar= yalnız adı verilen katmanları okur; alanlar=* on yedi sütunu açar")
{
    Rig only;
    REQUIRE(only.bus
                .execute_line("İÇEAKTAR \"" + seed("01-her-tur.ncz") + "\" katmanlar=PARSEL",
                              Origin::Test)
                .ok());
    CHECK(only.doc.live_entity_count() == 5);
    CHECK(on_layer(only.doc, "PARSEL") == 5);
    CHECK(on_layer(only.doc, "YAZI") == -1);

    Rig plain;
    REQUIRE(plain.import(seed("01-her-tur.ncz")).ok());
    CHECK(plain.doc.attributes().find("source_file") == core::kNoAttr);

    Rig all;
    REQUIRE(
        all.bus.execute_line("İÇEAKTAR \"" + seed("01-her-tur.ncz") + "\" alanlar=*", Origin::Test)
            .ok());
    for (const char* id : {"source_file", "layer_code", "layer_name", "entity_type", "name",
                           "label", "color_argb", "radius", "start_ang", "end_ang", "text_h",
                           "rotation", "box_width", "box_height", "scale", "grid_x", "grid_y"})
        CHECK_MESSAGE(all.doc.attributes().find(id) != core::kNoAttr, id);
    CHECK(all.doc.attributes().find("nokta_no") != core::kNoAttr);
}

TEST_CASE("NCZ: katman tabloları geometriden sonra gelse de nesneler doğru katmana düşer")
{
    Rig rig;
    REQUIRE(rig.import(seed("03-gec-tablolar.ncz")).ok());
    CHECK(on_layer(rig.doc, "İKİ") == 2);
    CHECK(on_layer(rig.doc, "KATMAN_7") == 1);
    const core::LayerId iki = rig.doc.layer_table().find("İKİ");
    REQUIRE(iki != core::kNoLayer);
    CHECK(rig.doc.layers()[iki].appearance.rgba == 0xFF070809u);
}

TEST_CASE("NCZ: öznitelik tabloları sayılır ve neden aktarılmadıkları söylenir")
{
    Rig rig;
    REQUIRE(rig.import(seed("05-oznitelik-tablolari.ncz")).ok());
    CHECK(has(rig.transcript, "Dosyada 2 öznitelik tablosu var (@TAB1: 2 satır, @TAB23: 2 satır); "
                              "satırları bir nesneye bağlanmadığı için çizime aktarılmadı."));
}

TEST_CASE("NCZ: Netcad 8 akıllı nesneleri sembol bloğu olarak çizilir, değerleri sütunlarda")
{
    Rig rig;
    REQUIRE(rig.import(seed("07-akilli-nesneler.ncz")).ok());
    CHECK(of_kind(rig.doc, core::kBlockReferenceKind) == 10);
    for (const char* id :
         {"akilli_nesne", "nizam", "kat", "taks", "kaks", "genislik", "fonksiyon_adi"})
        CHECK_MESSAGE(rig.doc.attributes().find(id) != core::kNoAttr, id);
    CHECK(has(rig.transcript, "10 Netcad akıllı nesnesi (Yerleşim 3, Yapılaşma 3, Yol 2, Plan "
                              "Notu 1, Fonksiyon Adı 1) sembol olarak çizildi"));
    // One the reader cannot draw is kept as a point with its values, and said.
    CHECK(has(rig.transcript, "1 akıllı nesnenin (Akıllı Nesne 1) sembolü çizilemedi"));

    // The grid marks a SmartObject draws for itself are not read twice.
    Rig marks;
    REQUIRE(marks.import(seed("02-akilli-nesne.ncz")).ok());
    CHECK(has(marks.transcript, "1 ızgara işareti (katman 0'daki S0 sembolü) akıllı nesnenin "
                                "kendisi çizdiği için ayrıca okunmadı."));
}

TEST_CASE("NCZ: dosyanın dilimi çizimin diliminden farklıysa dönüştürülmeden okunur ve söylenir")
{
    auto catalogue = domain::geodesy::CrsCatalog::load(std::string(PIRICAD_DATA_DIR) + "/crs");
    REQUIRE(catalogue.ok());
    Rig rig;
    domain::geodesy::CrsService service(rig.bus, std::move(catalogue.value()));
    REQUIRE(rig.bus.execute_line("AYAR koordinat_sistemi EPSG:5256", Origin::Test).ok());
    REQUIRE(rig.doc.crs().central_meridian_deg() == 36);

    // The seed declares ITRF, 3°, meridian 39 — `Suşehri`'s zone — into a TM36
    // drawing: the TM30/TM33 blunder of model.md R36, made visible.
    REQUIRE(rig.import(seed("01-her-tur.ncz")).ok());
    CHECK(has(rig.transcript, "orta meridyen 39°"));
    CHECK(has(rig.transcript, "36° orta meridyenli. Koordinatlar dönüştürülmedi: dilimler "
                              "farklıysa çizim yanlış yere düşer."));
}

TEST_CASE("NCZ: coğrafi koordinatlı dosya reddedilir; bildirimi yanlış olan metre dosyası okunur")
{
    Rig degrees;
    const std::uint64_t before = degrees.doc.content_hash();
    auto refused               = degrees.import(seed("10-cografi.ncz"));
    REQUIRE(!refused.ok());
    CHECK(has(refused.error().message, "Dosya coğrafi koordinatlarda ("));
    CHECK(has(refused.error().message, "bütün koordinatları derece aralığında"));
    CHECK(degrees.doc.content_hash() == before);

    Rig metres;
    REQUIRE(metres.import(seed("11-cografi-bildirim-metre.ncz")).ok());
    CHECK(metres.doc.live_entity_count() == 1);
    CHECK(has(metres.transcript, "bildirim yanlış görünüyor"));
}

TEST_CASE("NCZ: kesik dosya okunabildiği kadar okunur ve nerede kesildiği söylenir")
{
    Rig rig;
    REQUIRE(rig.import(seed("06-kesik.ncz")).ok());
    CHECK(rig.doc.live_entity_count() == 8);
    CHECK(has(rig.transcript, "Dosya kesik ya da bozuk görünüyor: 2755. bayttaki kayıt dosyanın "
                              "bittiği yerden 16 bayt öteye uzanıyor."));

    // A whole file says nothing of the kind.
    Rig whole;
    REQUIRE(whole.import(seed("01-her-tur.ncz")).ok());
    CHECK(!has(whole.transcript, "kesik ya da bozuk"));
    Rig tables;
    REQUIRE(tables.import(seed("05-oznitelik-tablolari.ncz")).ok());
    CHECK(!has(tables.transcript, "kesik ya da bozuk"));
}

TEST_CASE("NCZ: bir içe aktarma tek geri alma adımıdır")
{
    Rig rig;
    REQUIRE(rig.import(seed("07-akilli-nesneler.ncz")).ok());
    const std::uint64_t read = rig.doc.content_hash();
    // Every object, symbol members included, goes in one step; the layers, the
    // symbol blocks and the columns the import made stay, because undo never
    // cuts a table (model.md R4a).
    REQUIRE(rig.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(rig.doc.live_entity_count() == 0);
    CHECK(!rig.undo.can_undo());
    REQUIRE(rig.bus.execute_line("YİNELE", Origin::Test).ok());
    CHECK(rig.doc.content_hash() == read);
}

TEST_CASE("NCZ KANIT: komut satırı ve JSON betik aynı çizimi ve aynı günlüğü bırakır")
{
    // Article 6.4 for a file-reading command: the GUI's `İçe Aktar` sends the line
    // the command line sends, so the two roads to compare are the line and the
    // script.
    Rig line;
    REQUIRE(line.import(seed("01-her-tur.ncz")).ok());

    Rig script;
    script::JsonRunner runner(script.bus, script::Sandbox::Project);
    std::string path = seed("01-her-tur.ncz");
    std::string escaped;
    for (const char c : path) {
        if (c == '\\' || c == '"') escaped += '\\';
        escaped += c;
    }
    REQUIRE(
        runner.run_text(R"([{"cmd": "İÇEAKTAR", "args": {"dosya": ")" + escaped + R"("}}])").ok());

    // The same drawing, and the same record: every journalled command with its
    // resolved arguments. Which client sent it is the one field that differs.
    const auto recorded = [](const Journal& j) {
        std::string out;
        for (const auto& e : j.entries())
            out += e.command_id + " " + e.args.to_json().dump() + "\n";
        return out;
    };
    CHECK(line.doc.content_hash() == script.doc.content_hash());
    CHECK(recorded(line.journal) == recorded(script.journal));
    CHECK(recorded(line.journal).find("core.import") != std::string::npos);
}
