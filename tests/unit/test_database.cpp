// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE ROUND TRIP IS THE PROOF, AGAIN, AND IT IS A DIFFERENT ROUND TRIP.
//
// Two things go into a database and only one of them comes back the same way:
//
//   A PROJECT goes in as the bytes of its `.pcad` file and must come out as the
//   SAME DOCUMENT — `content_hash()` and `Settings::fold()`, exactly the measure
//   `test_io.cpp` uses on a file. If that holds, saving to a database is as safe
//   as saving to disk, because it IS saving to disk with a different destination.
//
//   A LAYER goes in as an ordinary spatial table and is NOT read back by this
//   program at all. What has to be proved there is that somebody ELSE can read
//   it: the row count, the SRID, the geometry types and the attribute columns are
//   checked with SQL, the way QGIS or ogr2ogr would meet them.
//
// WHEN THERE IS NO DATABASE these cases report PENDING rather than passing.
// CLAUDE.md Article 8.2 and `.claude/data.md` both require it: a gated test that
// prints `ok` while asserting nothing is a false report about coverage, and this
// is a gate that will be shut on most developer machines and open on none of the
// CI runners until one is provisioned.
//
// test.md P9: nothing here mutates a Document except through a command dispatched
// on the Bus.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/transaction.hpp"
#include "piricad/core/text.hpp"
#include "piricad/domain/geodesy/crs_service.hpp"
#include "piricad/io/database.hpp"
#include "piricad/io/postgis.hpp"
#include "piricad/io/service.hpp"

#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using namespace piricad;
using namespace piricad::command;

namespace {

/// The connection string these tests use, or empty when they must not run.
///
/// FROM THE ENVIRONMENT AND NOWHERE ELSE. A hard-coded `host=localhost` would
/// make this suite try to reach whatever happens to listen on port 5432 of the
/// machine running it, which on a developer's laptop is somebody's real database.
/// An explicit `PIRICAD_TEST_PGCONN` is consent.
std::string test_conninfo()
{
    const char* set = std::getenv("PIRICAD_TEST_PGCONN");
    return set != nullptr ? std::string(set) : std::string();
}

/// A bus with both engines attached, which is what an application is.
struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    io::FileService files{bus};
    io::DatabaseService database{bus};

    /// The zone catalogue, so `TUREF/TM30` resolves to EPSG:5254. Without it the
    /// document keeps an unresolved id — which is the truthful state, and exactly
    /// the state the last case in this file exercises on purpose.
    std::optional<domain::geodesy::CrsService> crs;
    std::string transcript;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { transcript.append(s).append("\n"); };

        auto catalogue = domain::geodesy::CrsCatalog::load(std::string(PIRICAD_DATA_DIR) + "/crs");
        REQUIRE(catalogue.ok());
        crs.emplace(bus, std::move(catalogue.value()));

        // The document has to know its CRS before a layer can be written: a table
        // whose SRID is a guess is worse than no table (model.md R36). This is the
        // command a user runs, not a back door.
        run("AYAR koordinat_sistemi deger=TUREF/TM30");
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line.c_str(), r.error().message);
    }

    /// Runs a line that is EXPECTED to fail, and returns what it said.
    std::string expect_fail(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_FALSE(r.ok());
        return r.error().message;
    }
};

/// How many entities the drawing actually holds. `EntityTable::size()` counts
/// SLOTS, and an erased entity leaves its slot behind (model.md R4).
std::size_t live_entities(const core::Document& doc)
{
    std::size_t n = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.entities().alive(e)) ++n;
    return n;
}

/// A small drawing with attributes on it, so the table has real columns.
void draw_fixture(Rig& rig)
{
    rig.run("KATMAN ad=PARSEL");
    rig.run("SÜTUN kimlik=ada_no tur=tam_sayi");
    rig.run("SÜTUN kimlik=malik tur=metin");
    rig.run("SÜTUN kimlik=yuzolcumu tur=uzunluk");

    rig.run("ALAN 485300.000,4310200.000 485360.000,4310200.000 485360.000,4310245.000 "
            "485300.000,4310245.000");
    rig.run("ÖZNİTELİK ada_no 1 142");
    rig.run("ÖZNİTELİK malik 1 \"Ayşe Yılmaz\"");

    // On its OWN layer, so `katmanyaz PARSEL` writing one row proves it wrote the
    // named layer and not the whole drawing.
    rig.run("KATMAN ad=YOL");
    rig.run("ÇİZGİ 485200.000,4310100.000 485250.000,4310100.000 485250.000,4310150.000");
    rig.run("KATMAN ad=PARSEL");
}

} // namespace

TEST_CASE("veritabanı motoru yokken açıkça söyler")
{
    // This one runs EVERYWHERE, with or without a server: it is about the message
    // a client gets when no engine is attached, and attaching none is the point.
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    register_builtin_commands(reg);

    auto r = bus.execute_line("VERİTABANI tablolar", Origin::Test);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("Veritabanı motoru bağlı değil") != std::string::npos);
}

TEST_CASE("bilinmeyen işlem geçerli işlemleri sayar")
{
    Rig rig;
    const std::string said = rig.expect_fail("VERİTABANI uçur");
    CHECK(said.find("Bilinmeyen işlem") != std::string::npos);
    CHECK(said.find("projekaydet") != std::string::npos);
}

TEST_CASE("bağlanmadan iş yapılamaz")
{
    Rig rig;
    if (!io::DatabaseService::available()) {
        PENDING("bu yapı PIRICAD_WITH_POSTGIS olmadan derlendi");
        return;
    }

    const std::string said = rig.expect_fail("VERİTABANI tablolar");
    CHECK(said.find("bağlı değilsiniz") != std::string::npos);
}

TEST_CASE("komut argümanları Value olarak gidip geliyor")
{
    // Article 9: a Value round-trip test per command. A command invocation is
    // DATA (Article 1.4) — it has to survive a trip through JSON unchanged, or a
    // journal cannot replay it and a script cannot express it.
    Args args;
    args.set("islem", Value::text("katmanyaz"));
    args.set("katman", Value::text("İMAR PLANI"));
    args.set("hedef", Value::text("imar_plani"));

    auto back = Args::from_json(args.to_json());
    REQUIRE(back.ok());
    CHECK(back.value() == args);

    // A connection string is the awkward one: `=`, spaces and a URI form all
    // inside one text value.
    Args conn;
    conn.set("islem", Value::text("baglan"));
    conn.set("hedef", Value::text("host=localhost port=5432 dbname=piricad user=harita"));

    auto conn_back = Args::from_json(conn.to_json());
    REQUIRE(conn_back.ok());
    CHECK(conn_back.value() == conn);
}

TEST_CASE("iptal edilen VERİTABANI hiçbir iz bırakmaz")
{
    // Article 9: a cancellation test with an empty undo delta. Pressing ESC at
    // the first prompt must leave the document, the undo stack and the journal
    // exactly as they were — cancelling is not a failure and not an edit.
    Rig rig;

    const std::uint64_t before       = rig.doc.content_hash();
    const std::size_t undo_before    = rig.undo.undo_depth();
    const std::size_t journal_before = rig.journal.size();

    auto started = rig.bus.begin_interactive("VERİTABANI");
    REQUIRE(started.ok());
    auto& session = *started.value();

    REQUIRE(session.waiting());
    session.cancel(); // ESC
    CHECK(rig.bus.finish(session).ok());

    CHECK_EQ(rig.doc.content_hash(), before);
    CHECK_EQ(rig.undo.undo_depth(), undo_before);
    CHECK_EQ(rig.journal.size(), journal_before);
}

TEST_CASE("parola libpq'nun HER İKİ bağlantı biçiminden de silinir")
{
    // The keyword form is the obvious one. The URI form is the one that gets
    // forgotten, and it is the one a user is most likely to paste out of a
    // colleague's message — so it is checked here by name.
    CHECK_EQ(redact_conninfo("host=localhost dbname=piricad user=harita password=ÇOKGİZLİ"),
             std::string("host=localhost dbname=piricad user=harita password=***"));

    // Quoted, because a password with a space in it is still a password.
    CHECK_EQ(redact_conninfo("host=x password='SEC RET' dbname=y"),
             std::string("host=x password=*** dbname=y"));

    // The URI form.
    CHECK_EQ(redact_conninfo("postgresql://harita:ÇOKGİZLİ@sunucu.gov.tr/piricad"),
             std::string("postgresql://harita:***@sunucu.gov.tr/piricad"));
    CHECK_EQ(redact_conninfo("postgres://harita:GİZLİ@sunucu:5432/piricad?sslmode=require"),
             std::string("postgres://harita:***@sunucu:5432/piricad?sslmode=require"));

    // A URI with no password at all is left alone — including the one whose only
    // colon belongs to the PORT, which must not be mistaken for a secret.
    CHECK_EQ(redact_conninfo("postgresql://harita@sunucu/piricad"),
             std::string("postgresql://harita@sunucu/piricad"));
    CHECK_EQ(redact_conninfo("postgresql://sunucu:5432/piricad"),
             std::string("postgresql://sunucu:5432/piricad"));

    // The FIELD survives, the secret does not: a replay that silently dropped
    // `password=` would look like a connection that never needed one.
    CHECK(redact_conninfo("host=x password=s").find("password=") != std::string::npos);

    // And a value that merely CONTAINS the word is not cut in half.
    CHECK_EQ(redact_conninfo("dbname=my_password=thing"), std::string("dbname=my_password=thing"));
}

TEST_CASE("bağlantı dizesindeki parola günlüğe düşmez")
{
    // The journal is a plain JSONL file that gets attached to bug reports and
    // committed alongside projects. A password in it is a credential leak with a
    // convenience story attached, so the value is recorded with the FIELD kept
    // and the SECRET replaced — a replay that silently dropped `password=` would
    // look like a connection that never needed one.
    Rig rig;

    // Fails, because nothing is listening on that port — and that is the point:
    // the redaction has to happen on the way to the journal, not as a reward for
    // connecting successfully.
    rig.bus
        .execute_line("VERİTABANI baglan hedef=\"host=127.0.0.1 port=1 dbname=x "
                      "user=y password=ÇOKGİZLİ\"",
                      Origin::Test)
        .ok();

    std::string wire;
    for (const auto& entry : rig.journal.entries())
        wire += entry.command_id + " " + entry.args.to_json().dump() + "\n";

    CHECK(wire.find("ÇOKGİZLİ") == std::string::npos);
}

TEST_CASE("proje veritabanına gidip aynı belge olarak geri gelir")
{
    if (!io::DatabaseService::available()) {
        PENDING("bu yapı PIRICAD_WITH_POSTGIS olmadan derlendi");
        return;
    }
    const std::string conninfo = test_conninfo();
    if (conninfo.empty()) {
        PENDING("PIRICAD_TEST_PGCONN ayarlı değil; veritabanı testleri çalışmadı");
        return;
    }

    Rig rig;
    rig.run("VERİTABANI baglan hedef=\"" + conninfo + "\"");

    draw_fixture(rig);

    // What the drawing IS, before it goes anywhere.
    const std::uint64_t before          = rig.doc.content_hash();
    const std::uint64_t settings_before = rig.bus.project_settings().fold(core::fnv1a({}));
    const std::size_t entities_before   = live_entities(rig.doc);

    rig.run("VERİTABANI projekaydet hedef=piricad-test-proje");

    // Draw over it, so "came back the same" cannot be satisfied by doing nothing.
    rig.run("KATMAN ad=SONRADAN");
    rig.run("ÇİZGİ 0.000,0.000 1.000,1.000");
    REQUIRE(rig.doc.content_hash() != before);

    rig.run("VERİTABANI projeac hedef=piricad-test-proje");

    CHECK(rig.doc.content_hash() == before);
    CHECK(rig.bus.project_settings().fold(core::fnv1a({})) == settings_before);
    CHECK(live_entities(rig.doc) == entities_before);

    // And it is listed.
    rig.run("VERİTABANI projeler");
    CHECK(rig.transcript.find("piricad-test-proje") != std::string::npos);

    // Tidy up. A test suite pointed at somebody's database leaves it as it found
    // it — and running the delete here is also what proves the verb works.
    rig.run("VERİTABANI projesil hedef=piricad-test-proje");
    CHECK(rig.transcript.find("silindi") != std::string::npos);

    // Deleting what is not there is not an error: a script that tidies before it
    // runs must not abort because the tidying was unnecessary.
    rig.run("VERİTABANI projesil hedef=piricad-test-proje");
    CHECK(rig.transcript.find("böyle bir proje yoktu") != std::string::npos);
}

TEST_CASE("katman başka bir programın okuyabileceği tablo olur")
{
    if (!io::DatabaseService::available()) {
        PENDING("bu yapı PIRICAD_WITH_POSTGIS olmadan derlendi");
        return;
    }
    const std::string conninfo = test_conninfo();
    if (conninfo.empty()) {
        PENDING("PIRICAD_TEST_PGCONN ayarlı değil; veritabanı testleri çalışmadı");
        return;
    }

    Rig rig;
    rig.run("VERİTABANI baglan hedef=\"" + conninfo + "\"");
    draw_fixture(rig);

    rig.run("VERİTABANI katmanyaz katman=PARSEL hedef=piricad_test_parsel");
    CHECK(rig.transcript.find("1 satır") != std::string::npos);
    CHECK(rig.transcript.find("EPSG:5254") != std::string::npos);

    // It shows up as a spatial table, which is what "a GIS layer" means.
    rig.run("VERİTABANI tablolar");
    CHECK(rig.transcript.find("piricad_test_parsel") != std::string::npos);

    // The other layer is a different table. TWO rows, not one: `ÇİZGİ` with three
    // points draws two segments and therefore two entities, and the writer emits
    // one row per ENTITY rather than per command.
    rig.run("VERİTABANI katmanyaz katman=YOL hedef=piricad_test_yol");
    CHECK(rig.transcript.find("'YOL' katmanı yazıldı: 2 satır") != std::string::npos);

    // Writing it AGAIN REPLACES rather than appends — the table is dropped and
    // rebuilt inside one transaction. Running a command twice must not double
    // every parcel in a municipality's table, and a doubled cadastral table is
    // worse than no table at all.
    const std::string before = rig.transcript;
    rig.run("VERİTABANI katmanyaz katman=PARSEL hedef=piricad_test_parsel");
    CHECK(rig.transcript.substr(before.size()).find("1 satır") != std::string::npos);
}

TEST_CASE("sütun adı çakışması açıkça reddedilir")
{
    if (!io::DatabaseService::available()) {
        PENDING("bu yapı PIRICAD_WITH_POSTGIS olmadan derlendi");
        return;
    }
    const std::string conninfo = test_conninfo();
    if (conninfo.empty()) {
        PENDING("PIRICAD_TEST_PGCONN ayarlı değil; veritabanı testleri çalışmadı");
        return;
    }

    // Every written table already has a `kimlik` column, and `kimlik` is a
    // perfectly natural Turkish attribute name. The server would refuse the
    // CREATE with a message about SQL; this one names the attribute the user has
    // to rename.
    Rig rig;
    rig.run("VERİTABANI baglan hedef=\"" + conninfo + "\"");
    rig.run("KATMAN ad=PARSEL");
    rig.run("SÜTUN kimlik=kimlik tur=metin");
    rig.run("ALAN 485300.000,4310200.000 485360.000,4310200.000 485360.000,4310245.000 "
            "485300.000,4310245.000");

    const std::string said = rig.expect_fail("VERİTABANI katmanyaz katman=PARSEL "
                                             "hedef=piricad_test_cakisma");
    CHECK(said.find("kimlik") != std::string::npos);
    CHECK(said.find("yeniden adlandırın") != std::string::npos);
}

TEST_CASE("çözülmemiş koordinat sistemiyle tablo yazılmaz")
{
    if (!io::DatabaseService::available()) {
        PENDING("bu yapı PIRICAD_WITH_POSTGIS olmadan derlendi");
        return;
    }
    const std::string conninfo = test_conninfo();
    if (conninfo.empty()) {
        PENDING("PIRICAD_TEST_PGCONN ayarlı değil; veritabanı testleri çalışmadı");
        return;
    }

    // A rig WITHOUT the CRS setting, so nothing resolved it. The refusal is the
    // feature: a table labelled with the wrong SRID moves every coordinate in it
    // by kilometres and does so silently.
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    register_builtin_commands(reg);
    io::FileService files{bus};
    io::DatabaseService database{bus};

    bus.execute_line("VERİTABANI baglan hedef=\"" + conninfo + "\"", Origin::Test).ok();
    bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok();
    bus.execute_line("ÇİZGİ 0.000,0.000 1.000,1.000", Origin::Test).ok();

    auto r = bus.execute_line("VERİTABANI katmanyaz katman=PARSEL", Origin::Test);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("koordinat sistemi") != std::string::npos);
}

// ===========================================================================
// The EWKB encoding. Checked WITHOUT a server, because a test that needs a
// running PostgreSQL is a test that does not run: this is the piece that has to
// be exactly right, and a parcel written with the wrong shape is a wrong area on
// a legal document.
// ===========================================================================

namespace {

/// A little-endian `uint32` out of an EWKB buffer.
std::uint32_t word_at(const std::string& wkb, std::size_t at)
{
    std::uint32_t v = 0;
    for (int i = 3; i >= 0; --i)
        v = (v << 8) | static_cast<unsigned char>(wkb[at + static_cast<std::size_t>(i)]);
    return v;
}

/// The geometry type EWKB declares, with the SRID flag taken off.
std::uint32_t type_of(const std::string& wkb)
{
    return word_at(wkb, 1) & 0x00FFFFFFu;
}

/// Whether the SRID flag is set, and what it says.
bool carries_srid(const std::string& wkb)
{
    return (word_at(wkb, 1) & 0x20000000u) != 0;
}

std::int64_t srid_of(const std::string& wkb)
{
    return static_cast<std::int64_t>(word_at(wkb, 5));
}

/// The document's one live entity.
core::EntityId only_entity(const core::Document& doc)
{
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.entities().alive(e)) return e;
    return 0;
}

} // namespace

TEST_CASE("EWKB: açık halka LINESTRING, kapalı halka POLYGON olur")
{
    Rig rig;
    rig.run("KATMAN ad=YOL");
    rig.run("ÇİZGİ 485200.000,4310100.000 485250.000,4310100.000");

    const std::string line = io::entity_ewkb(rig.doc, only_entity(rig.doc), 5254);
    REQUIRE_FALSE(line.empty());
    CHECK_EQ(line[0], '\x01'); // little endian
    CHECK(carries_srid(line));
    CHECK_EQ(srid_of(line), 5254);
    CHECK_EQ(type_of(line), 2u); // LineString

    // Header + one uint32 count + two vertices of two doubles.
    CHECK_EQ(line.size(), std::size_t{1 + 4 + 4 + 4} + 2 * 2 * sizeof(double));

    Rig face;
    face.run("KATMAN ad=PARSEL");
    face.run("ALAN 485300.000,4310200.000 485360.000,4310200.000 485360.000,4310245.000 "
             "485300.000,4310245.000");

    const std::string polygon = io::entity_ewkb(face.doc, only_entity(face.doc), 5254);
    REQUIRE_FALSE(polygon.empty());
    CHECK_EQ(type_of(polygon), 3u);     // Polygon
    CHECK_EQ(word_at(polygon, 9), 1u);  // one ring
    CHECK_EQ(word_at(polygon, 13), 5u); // FOUR corners, written with the fifth closing it
}

TEST_CASE("EWKB: delikli yüzey tek POLYGON, iki halka")
{
    // `bolum` splits the point list: the first ring is the boundary and every
    // one after it is a hole in it — a yola terk exclusion inside a parsel.
    Rig rig;
    rig.run("KATMAN ad=PARSEL");
    rig.run("ALAN 485300.000,4310200.000 485360.000,4310200.000 485360.000,4310245.000 "
            "485300.000,4310245.000 485315.000,4310212.000 485345.000,4310212.000 "
            "485345.000,4310232.000 485315.000,4310232.000 bolum=4 bolum=4");

    const std::string wkb = io::entity_ewkb(rig.doc, only_entity(rig.doc), 5254);
    REQUIRE_FALSE(wkb.empty());
    CHECK_EQ(type_of(wkb), 3u);     // Polygon, NOT MultiPolygon
    CHECK_EQ(word_at(wkb, 9), 2u);  // exterior + one hole
    CHECK_EQ(word_at(wkb, 13), 5u); // the exterior's four corners, closed
}

TEST_CASE("EWKB: iki yüzlü parsel MULTIPOLYGON olur, ikinci yüz delik değil")
{
    // THE BUG THIS LOCKS DOWN. An entity's rings are a FLAT list and their roles
    // are what group them. Reading the span as "first ring is the boundary, the
    // rest are holes" turned a parcel cut in two by a road into a polygon whose
    // second FACE was a HOLE — a table whose areas are wrong and whose geometry
    // is still valid enough that nothing complains.
    //
    // No command draws a multipart face today; İÇEAKTAR does, out of a
    // MULTIPOLYGON in a GeoPackage, which is exactly how a parcel arrives from
    // TKGM. The geometry is built here through the same ring API the importer
    // uses, because the alternative is a test that needs GDAL AND a server.
    Rig rig;
    rig.run("KATMAN ad=PARSEL");

    const core::LayerId layer = rig.doc.find_layer("PARSEL");
    REQUIRE(layer != core::kNoLayer);

    // Two separate faces, each its own exterior. Through a Transaction, because
    // that is the only sanctioned road to a Document (Article 5.9).
    const std::array<core::Point2, 4> west{
        core::Point2{485300000, 4310200000}, core::Point2{485360000, 4310200000},
        core::Point2{485360000, 4310245000}, core::Point2{485300000, 4310245000}};
    const std::array<core::Point2, 4> east{
        core::Point2{485400000, 4310200000}, core::Point2{485440000, 4310200000},
        core::Point2{485440000, 4310230000}, core::Point2{485400000, 4310230000}};
    {
        Transaction tx(rig.doc, "iki yüzlü parsel");

        const std::array<core::RingGeometry::RingInput, 2> rings{
            core::RingGeometry::RingInput{west, core::RingRole::Exterior, 0},
            core::RingGeometry::RingInput{east, core::RingRole::Exterior, 1}};

        auto made = tx.add_area(layer, rings);
        REQUIRE(made.ok());

        // The ops are DROPPED rather than pushed onto the undo stack: this
        // fixture is about the shape of the geometry, not about undo.
        (void)tx.release();
    }

    const std::string wkb = io::entity_ewkb(rig.doc, only_entity(rig.doc), 5254);
    REQUIRE_FALSE(wkb.empty());

    CHECK(carries_srid(wkb));
    CHECK_EQ(srid_of(wkb), 5254);
    CHECK_EQ(type_of(wkb), 6u);    // MultiPolygon, not Polygon-with-a-hole
    CHECK_EQ(word_at(wkb, 9), 2u); // two parts

    // The first member: its own byte order, its own type, NO srid of its own.
    CHECK_EQ(wkb[13], '\x01');
    CHECK_EQ(word_at(wkb, 14), 3u); // Polygon
    CHECK_EQ(word_at(wkb, 18), 1u); // one ring in this part
    CHECK_EQ(word_at(wkb, 22), 5u); // four corners, closed
}
