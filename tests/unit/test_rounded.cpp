// SPDX-License-Identifier: GPL-3.0-or-later
//
// A ROUNDED PARCEL IS STILL A PARCEL (TODOS O-2). YUVARLA writes the corner as
// an arc into the same object, which makes a polyline an arc polyline — and
// every tool that reads a parcel must go on reading it as one: the corner
// numbering numbers it, the length writer writes the arc's own length at the
// arc, a caption that follows an edge that bends says the arc's length, the
// buffer takes the arc as drawn, and the area editor, which moves straight
// edges, says it passes the parcel over rather than carrying an arc's ends away
// from its centre. The figures are closed forms: π · r / 2 for a quarter arc.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/processing/tool.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        processing::register_processing_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

/// Every caption's words, in slot order.
std::vector<std::string> captions(const core::Document& doc)
{
    std::vector<std::string> out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        const std::uint32_t slot = doc.entities().slot[e];
        if (doc.texts().has(slot)) out.emplace_back(doc.texts().text(slot));
    }
    return out;
}

bool has(const std::vector<std::string>& all, const std::string& one)
{
    return std::find(all.begin(), all.end(), one) != all.end();
}

} // namespace

TEST_CASE("YUVARLANAN: köşesi yuvarlanan parsel Araçlar'a hâlâ alan; köşeleri numaralanıyor")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    const core::EntityId e = r.doc.slot_of(static_cast<core::EntityKey>(1));
    REQUIRE(e != core::kNoEntity);
    REQUIRE(r.doc.entities().kind[e] == core::kArcPolylineKind);
    CHECK(processing::classify(r.doc, e) == processing::Applies::Faces);

    // Its five ring vertices — three corners and the arc's two ends — each
    // numbered; nothing passed over.
    r.said.clear();
    r.run("KÖŞENUMARALA nesneler=1");
    CHECK_EQ(captions(r.doc).size(), 5u);
    CHECK(r.said.find("atlandı") == std::string::npos);
}

TEST_CASE("YUVARLANAN: Uzunluk Yaz yay kenarına kirişi değil yayın boyunu, yayın ortasına yazıyor")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");
    const std::vector<std::string> said = captions(r.doc);
    REQUIRE_EQ(said.size(), 5u);
    // A quarter arc of 2 m is π m long: 3,14 m. Its chord would say 2,83 m.
    CHECK(has(said, "3,14 m"));
    CHECK_FALSE(has(said, "2,83 m"));
    CHECK(has(said, "18,00 m"));
    CHECK(has(said, "8,00 m"));
    // AT THE ARC'S MIDDLE, outside the parcel: the rule places the caption's
    // middle on the arc's radius at 45°, a gap and half its height beyond the
    // arc, and reads it along the arc's tangent there.
    core::EntityId arc_caption = core::kNoEntity;
    for (core::EntityId c = 0; c < r.doc.entities().size(); ++c)
        if (r.doc.alive(c) && r.doc.texts().has(r.doc.entities().slot[c]) &&
            r.doc.texts().text(r.doc.entities().slot[c]) == "3,14 m")
            arc_caption = c;
    REQUIRE(arc_caption != core::kNoEntity);
    const core::Attachment* rule = r.doc.attachments().get(arc_caption);
    REQUIRE(rule != nullptr);
    CHECK(rule->anchor == core::AttachAnchor::Edge);
    const core::EntityId parcel = r.doc.slot_of(static_cast<core::EntityKey>(1));
    const auto def = core::arc_polyline_of(r.doc.geometry(), r.doc.entities().slot[parcel]);
    REQUIRE(def.ok());
    const core::RingSpan span = r.doc.geometry().rings_of(r.doc.entities().slot[parcel]);
    std::vector<core::Point2> ring;
    for (std::size_t v = 0; v < r.doc.geometry().ring_xs(span.first).size(); ++v)
        ring.push_back(r.doc.geometry().vertex(span.first, static_cast<std::uint32_t>(v)));
    const core::Mm height = r.doc.texts().height(r.doc.entities().slot[arc_caption]);
    const auto place      = core::attach_place(ring, true, *rule, height, false, def.value().arcs);
    REQUIRE(place.has_value());
    const double dx = static_cast<double>(place->centre.x - 18'000);
    const double dy = static_cast<double>(place->centre.y - 8'000);
    CHECK(std::abs(std::sqrt(dx * dx + dy * dy) - (2'000.0 + static_cast<double>(rule->gap) +
                                                   static_cast<double>(height) / 2.0)) <= 2.0);
    CHECK(std::abs(dx - dy) <= 2.0); // on the 45° radius
    // Read along the tangent there: perpendicular to the radius.
    CHECK(std::abs(place->dir_x * dx + place->dir_y * dy) <= 1.0);
}

TEST_CASE("YUVARLANAN: yaya çevrilen kenarın bağlı yazısı yayın boyunu söylüyor")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");
    REQUIRE(has(captions(r.doc), "20,00 m"));
    // The south edge bent out through (10, −3): the circle through (0, 0),
    // (10, −3) and (20, 0) has radius 18,1667 m and the arc between them is
    // 21,18 m long — the caption follows it and SAYS it.
    const auto count = [](const std::vector<std::string>& all, const char* one) {
        return std::count(all.begin(), all.end(), std::string(one));
    };
    CHECK_EQ(count(captions(r.doc), "20,00 m"), 2); // the south edge and the north
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3");
    const std::vector<std::string> said = captions(r.doc);
    CHECK_EQ(count(said, "20,00 m"), 1); // the north edge alone
    CHECK(has(said, "21,18 m"));
    // Straight again, the chord again.
    r.run("KENARTÜRÜ nesne=1 kenar=1 tur=duz");
    CHECK_EQ(count(captions(r.doc), "20,00 m"), 2);
}

TEST_CASE("YUVARLANAN: Alanı Düzenle yaylı kenarlı alanı atlıyor ve söylüyor")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    const std::uint64_t before = r.doc.content_hash();
    r.said.clear();
    auto edited = r.bus.execute_line("ALANDÜZENLE nesneler=1 alan=190", Origin::Test);
    CHECK(r.doc.content_hash() == before);
    const std::string all = r.said + (edited ? std::string() : edited.error().message);
    CHECK(all.find("yaylı kenarlı alanı bu sürümde düzenlemez") != std::string::npos);
}

TEST_CASE("YUVARLANAN: Tampon yaylı kenarı çizildiği gibi alıyor")
{
    // The buffer of a rounded parcel is the buffer of what is drawn: its area
    // is the rounded parcel's, grown by 1 m, to within the chords both are
    // drawn with — and it is NOT the buffer of the corner that was cut off.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=4");
    r.run("TAMPON nesneler=1 mesafe=1 katman=TAMPON");
    double grown = 0.0;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        if (r.doc.alive(e) && r.doc.slot_of(static_cast<core::EntityKey>(1)) != e)
            grown = std::max(grown, std::abs(static_cast<double>(r.doc.entity_area(e))) / 1e6);
    // Rounded: 200 − (16 − 4π) = 188,566 m². Grown by 1 m with round
    // corners: + perimeter · 1 + π · 1², the perimeter 60 − 8 + 2π (the
    // rounding takes 4 m off each of the corner's two edges and adds the arc).
    const double pi       = std::acos(-1.0);
    const double rounded  = 200.0 - (16.0 - 4.0 * pi);
    const double expected = rounded + (60.0 - 8.0 + 2.0 * pi) + pi;
    CHECK(std::abs(grown - expected) < 0.2);
}

namespace {

/// Every live area's path, in slot order.
std::vector<core::CurvePath> areas(const core::Document& doc)
{
    std::vector<core::CurvePath> out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.texts().has(doc.entities().slot[e])) continue;
        if (auto p = core::path_of(doc, e); p && p->closed) out.push_back(*p);
    }
    return out;
}

/// The arcs of a path.
std::vector<core::PathPiece> arcs_of(const core::CurvePath& path)
{
    std::vector<core::PathPiece> out;
    for (const core::PathPiece& p : path.pieces)
        if (p.kind == core::PathPiece::Kind::Arc) out.push_back(p);
    return out;
}

double total_area(const core::Document& doc)
{
    double total = 0.0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.alive(e) && !doc.texts().has(doc.entities().slot[e]))
            total += std::abs(static_cast<double>(doc.entity_area(e)));
    return total;
}

} // namespace

TEST_CASE("YUVARLANAN: İFRAZ yuvarlak köşeyi düştüğü parçada aynı yay olarak bırakıyor (O-3)")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    const double before = total_area(r.doc);
    r.run("İFRAZ nesneler=1 noktalar=10,-5 10,15");
    const std::vector<core::CurvePath> pieces = areas(r.doc);
    REQUIRE_EQ(pieces.size(), 2u);
    std::size_t with_arc = 0;
    for (const core::CurvePath& piece : pieces) {
        const auto arcs = arcs_of(piece);
        if (arcs.empty()) continue;
        ++with_arc;
        REQUIRE_EQ(arcs.size(), 1u);
        CHECK(arcs.front().centre == core::Point2{18'000, 8'000});
        CHECK_EQ(arcs.front().radius, core::Mm{2'000});
    }
    CHECK_EQ(with_arc, 1u);
    // NOTHING LOST: the two pieces make the rounded parcel, to the square
    // millimetre the kernel's millimetre rounding allows.
    CHECK(std::abs(total_area(r.doc) - before) <= 5.0);
    CHECK(r.said.find("İfraz: 2 parça.") != std::string::npos);
}

TEST_CASE("YUVARLANAN: ALANİFRAZ yaylı parselden istenen alanı ayırıyor; yay yerinde")
{
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=2");
    // 50 m² off the west end, parallel to the west edge.
    r.run("ALANİFRAZ nesneler=1 yon=0,0 0,10 alan=50000000");
    const std::vector<core::CurvePath> pieces = areas(r.doc);
    REQUIRE_EQ(pieces.size(), 2u);
    bool target_met = false;
    for (const core::CurvePath& piece : pieces) {
        const double a = std::abs(static_cast<double>(core::path_area(piece)));
        target_met     = target_met || std::abs(a - 50'000'000.0) <= 10'000.0;
    }
    CHECK(target_met);
    // The arc is on the rest, with its centre and radius.
    std::size_t arcs = 0;
    for (const core::CurvePath& piece : pieces)
        for (const core::PathPiece& arc : arcs_of(piece)) {
            ++arcs;
            CHECK(arc.centre == core::Point2{18'000, 8'000});
            CHECK_EQ(arc.radius, core::Mm{2'000});
        }
    CHECK_EQ(arcs, 1u);
}

TEST_CASE("YUVARLANAN: TEVHİT yuvarlanmış parseli komşusuyla yayını koruyarak birleştiriyor")
{
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    r.run("ALAN 10,0 20,0 20,10 10,10");
    r.run("YUVARLA nesne=2 nokta=20,10 yaricap=2");
    const double before = total_area(r.doc);
    r.run("TEVHİT nesneler=1 nesneler=2");
    const std::vector<core::CurvePath> merged = areas(r.doc);
    REQUIRE_EQ(merged.size(), 1u);
    const auto arcs = arcs_of(merged.front());
    REQUIRE_EQ(arcs.size(), 1u);
    CHECK(arcs.front().centre == core::Point2{18'000, 8'000});
    CHECK_EQ(arcs.front().radius, core::Mm{2'000});
    // The seam between them is gone: the rounded rectangle's five pieces.
    CHECK_EQ(merged.front().pieces.size(), 5u);
    CHECK(std::abs(total_area(r.doc) - before) <= 5.0);
}

TEST_CASE("YUVARLANAN: yay kenarına bağlanan yazının yeri kaynağı taşınınca kaymıyor")
{
    // An attachment's offset used to be measured against the arc edge's CHORD
    // and applied along its ARC (core::attach_bends): the first time the
    // parcel moved, the caption stood metres from where it had been. Its
    // anchor now moves by exactly what the parcel moved.
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("YUVARLA nesne=1 nokta=20,10 yaricap=4"); // the arc (16,10) → (20,6), centre (16,6)
    r.run("METİN 20,11 \"köşe\"");                  // 2, beyond the arc
    r.run("BAĞLA nesneler=2 kaynak=1");
    const core::EntityId caption = r.doc.slot_of(core::EntityKey{2});
    REQUIRE(r.doc.attachments().has(caption));
    const auto anchor_of = [&r, caption] {
        const core::RingSpan rs = r.doc.geometry().rings_of(r.doc.entities().slot[caption]);
        return r.doc.geometry().vertex(rs.first, 0);
    };
    const core::Point2 before = anchor_of();
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=30,5");
    const core::Point2 after = anchor_of();
    CHECK(std::llabs(after.x - before.x - 30'000) <= 1);
    CHECK(std::llabs(after.y - before.y - 5'000) <= 1);
}

namespace {

/// A disc as an arc polyline: a 20 m square whose four corners are rounded to
/// 10 m, which uses every straight edge up — four quarter arcs round (10, 10).
void round_zone(Rig& r)
{
    r.run("ALAN 0,0 20,0 20,20 0,20");
    r.run("YUVARLA nesne=1 hepsi=evet yaricap=10");
    REQUIRE(r.doc.entities().kind[r.doc.slot_of(core::EntityKey{1})] == core::kArcPolylineKind);
}

/// A caption's baseline: its two points.
std::array<core::Point2, 2> baseline_of(const core::Document& doc, std::uint64_t key)
{
    const core::EntityId e  = doc.slot_of(core::EntityKey{key});
    const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
    return {doc.geometry().vertex(rs.first, 0), doc.geometry().vertex(rs.first, 1)};
}

} // namespace

TEST_CASE(
    "YUVARLANAN: yay kenarına bağlı yazı durduğu yerde kalır, oradaki teğet boyunca okunur (R46g)")
{
    // A caption tied to an arc used to jump, the first time its source moved,
    // to the tangent at the arc's MIDDLE — here 25° round from where it stood.
    // Measured round the arc (model.md R46g), its anchor stays where it was and
    // it reads along the tangent at its OWN place, the least turn an edge's
    // caption can make; it keeps that place as the zone moves and turns.
    Rig r;
    round_zone(r);
    r.run("METİN 6,21 \"A bölgesi\""); // 2, above the zone's top-left arc
    r.run("BAĞLA nesneler=2 kaynak=1");
    const core::EntityId caption = r.doc.slot_of(core::EntityKey{2});
    const core::Attachment* a    = r.doc.attachments().get(caption);
    REQUIRE(a != nullptr);
    CHECK(a->along_arc);
    const std::array<core::Point2, 2> before = baseline_of(r.doc, 2);

    // Reading along the circle round `centre` at the caption's anchor: its
    // direction square to the radius there.
    const auto reads_round = [](const std::array<core::Point2, 2>& base, core::Point2 centre) {
        const auto dx = static_cast<double>(base[1].x - base[0].x);
        const auto dy = static_cast<double>(base[1].y - base[0].y);
        const auto rx = static_cast<double>(base[0].x - centre.x);
        const auto ry = static_cast<double>(base[0].y - centre.y);
        return std::fabs((dx * rx) + (dy * ry)) <
               1e-3 * std::sqrt((dx * dx) + (dy * dy)) * std::sqrt((rx * rx) + (ry * ry));
    };

    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=30,5");
    const std::array<core::Point2, 2> moved = baseline_of(r.doc, 2);
    CHECK(std::llabs(moved[0].x - before[0].x - 30'000) <= 1);
    CHECK(std::llabs(moved[0].y - before[0].y - 5'000) <= 1);
    CHECK(reads_round(moved, core::Point2{40'000, 15'000}));

    // Turned with its zone, it stays at its place round it: a quarter turn
    // about the zone's centre carries the anchor a quarter of the way round.
    r.run("DÖNDÜR nesneler=1 merkez=40,15 aci=90"); // degrees, counter-clockwise
    const std::array<core::Point2, 2> turned = baseline_of(r.doc, 2);
    CHECK(std::llabs((turned[0].x - 40'000) + (moved[0].y - 15'000)) <= 2);
    CHECK(std::llabs((turned[0].y - 15'000) - (moved[0].x - 40'000)) <= 2);
    CHECK(reads_round(turned, core::Point2{40'000, 15'000}));
}

TEST_CASE("YUVARLANAN: yay boyunca ölçülen pay, kurala verilince aynı noktayı verir")
{
    // attach_measure_offset and attach_place are one rule read both ways: a
    // point measured round the arc is where the placement puts it back, all
    // round the arc and off both sides (R46g).
    Rig r;
    round_zone(r);
    const core::EntityId zone = r.doc.slot_of(core::EntityKey{1});
    const core::RingSpan rs   = r.doc.geometry().rings_of(r.doc.entities().slot[zone]);
    std::vector<core::Point2> ring;
    for (std::uint32_t v = 0; v < r.doc.geometry().ring_count[rs.first]; ++v)
        ring.push_back(r.doc.geometry().vertex(rs.first, v));
    const auto def = core::arc_polyline_of(r.doc.geometry(), r.doc.entities().slot[zone]);
    REQUIRE(def.ok());
    core::Attachment a;
    a.source        = core::EntityKey{1};
    a.anchor        = core::AttachAnchor::Edge;
    a.index         = 1;
    a.gap           = 1'000;
    const auto rule = core::attach_place(ring, true, a, 2'500, false, def.value().arcs);
    REQUIRE(rule.has_value());
    REQUIRE(rule->turn_centre.has_value());
    for (const core::Point2 at : {core::Point2{9'000, 22'500}, core::Point2{-2'000, 14'000},
                                  core::Point2{4'000, 18'000}, core::Point2{19'000, 21'000}}) {
        core::Attachment measured = a;
        core::attach_measure_offset(*rule, at, measured);
        CHECK(measured.along_arc);
        const auto placed = core::attach_place(ring, true, measured, 2'500, true, def.value().arcs);
        REQUIRE(placed.has_value());
        CHECK(std::llabs(placed->centre.x - at.x) <= 1);
        CHECK(std::llabs(placed->centre.y - at.y) <= 1);
    }
}
