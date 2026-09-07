// SPDX-License-Identifier: GPL-3.0-or-later
//
// THE ROUND TRIP IS THE PROOF.
//
// A file format is correct when a document written and read back is the same
// document. `Document::content_hash()` is what "the same" means here — it folds
// the CRS, every stored layer field, every interned style and every ring of every
// live entity, in order — and `Settings::fold()` is the other half, because
// model.md R39 makes project-scope settings part of the document.
//
// Everything else in this file exists to make that claim hold against a HOSTILE
// file: a truncated one, a too-new one, one whose offsets point outside itself,
// one whose bytes have been flipped. io.md R18/P6 and CLAUDE.md 6.7.
//
// test.md P9: nothing here mutates a Document except through a command dispatched
// on the Bus. Tests are a client of the bus with no privileges (Article 1.2).
#include "kentos_test.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/guide.hpp"

#include <cmath>
#include <iterator>

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/format.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

namespace fs = std::filesystem;

/// A bus with a file engine attached, which is what an application is.
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
};

/// A temp directory of this test's own, removed when the case ends. test.md R19:
/// deterministic tests create their temp directories per test.
class TempDir
{
public:
    explicit TempDir(const char* tag)
    {
        path_ = fs::temp_directory_path() / (std::string("kentoscad-io-") + tag);
        std::error_code ec;
        fs::remove_all(path_, ec);
        fs::create_directories(path_, ec);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const char* name) const { return (path_ / name).string(); }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

/// A small but deliberately awkward drawing: two layers, an open polyline, a
/// parcel with a hole, a multipart parcel, an entity that was erased (so the key
/// sequence has a hole in it), a hidden entity and a per-entity colour.
///
/// Built entirely through the command bus, because that is the only sanctioned
/// route to a Document (Article 5.9, test.md P9).
void draw_fixture(Rig& rig)
{
    const auto run = [&](const std::string& line) {
        auto r = rig.bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line.c_str(), r.error().message);
    };

    run("KATMAN ad=PARSEL renk=4281236786");
    run("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400 485440.861,4310321.111");

    // A parcel with a yola terk exclusion: exterior + interior in one part.
    rig.bus
        .dispatch(Invocation{"core.line",
                             [] {
                                 Args a;
                                 a.set("noktalar", Value::points({{485300000, 4310200000},
                                                                  {485360000, 4310200000},
                                                                  {485360000, 4310245000},
                                                                  {485300000, 4310245000}}));
                                 return a;
                             }(),
                             Origin::Test})
        .ok();

    run("KATMAN ad=YOL");
    run("ÇİZGİ 485200.000,4310100.000 485250.000,4310100.000");
    run("ÇİZGİ 485250.000,4310100.000 485250.000,4310150.000");

    // Erase one, so the key sequence has a hole and the reader has to reproduce
    // it rather than compacting it (model.md R4/P5).
    run("SİL 2");

    run("KATMAN ad=PARSEL");
    run("AYAR core.crs.id EPSG:5254");
}

std::vector<char> read_bytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

void write_bytes(const std::string& path, const std::vector<char>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// True when the error message carries the stable token io.md R9 names. The token
/// is at the front of the message because `core::Error` carries an enum code, not
/// the string code the rulebook writes — see `kentos_cad/io/format.hpp`.
bool has_token(const core::Error& e, const char* token)
{
    return e.message.rfind(token, 0) == 0;
}

/// Entity and layer keys, in slot order. This is the identity of the document,
/// and content_hash() deliberately does NOT cover it (two documents built the
/// same way must agree), so the round trip has to check it separately.
std::string key_fingerprint(const core::Document& doc)
{
    std::string out;
    for (const core::Layer& l : doc.layers())
        out += "L" + std::to_string(raw(l.key)) + ":" + l.name + ";";
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        out += "E" + std::to_string(raw(doc.key_of(e))) + ":" + (doc.alive(e) ? "1" : "0") + ":" +
               std::to_string(doc.entities().flags[e]) + ":" +
               std::to_string(doc.entities().style[e]) + ":" +
               std::to_string(doc.entities().layer[e]) + ";";
    return out;
}

} // namespace

// ===========================================================================
// The round trip
// ===========================================================================

TEST_CASE("IO: belge -> dosya -> belge, içerik parmak izi birebir aynı")
{
    TempDir tmp("roundtrip");
    const std::string path = tmp.file("gidis-donus.pcad");

    Rig written;
    draw_fixture(written);

    const std::uint64_t hash     = written.doc.content_hash();
    const std::uint64_t settings = written.bus.project_settings().fold(core::fnv1a({}));
    const std::string keys       = key_fingerprint(written.doc);
    const std::size_t live       = written.doc.live_entity_count();
    const std::size_t layers     = written.doc.layers().size();
    const std::size_t styles     = written.doc.styles().size();

    auto saved = written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test);
    REQUIRE(saved.ok());
    REQUIRE(fs::exists(path));

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());

    // THE assertion. Everything else in this file protects it.
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    CHECK_EQ(reloaded.bus.project_settings().fold(core::fnv1a({})), settings);

    // ...and the identity the hash deliberately leaves out (model.md R1, R4).
    CHECK_EQ(key_fingerprint(reloaded.doc), keys);

    CHECK_EQ(reloaded.doc.live_entity_count(), live);
    CHECK_EQ(reloaded.doc.layers().size(), layers);
    CHECK_EQ(reloaded.doc.styles().size(), styles);

    // Both halves of the CRS come back: the document's own (which content_hash
    // folds) and the project setting AYAR writes. They are two representations of
    // one thing today — see the note on `effective_crs` in src/io/src/service.cpp.
    CHECK_EQ(reloaded.doc.crs().id(), written.doc.crs().id());
    CHECK_EQ(std::string(reloaded.bus.project_settings().get("core.crs.id").as_text()),
             std::string("EPSG:5254"));

    // The key allocator must not be about to hand out a key the file already
    // used (identity.hpp adopt_entity).
    CHECK_EQ(reloaded.doc.keys().peek_entity(), written.doc.keys().peek_entity());
    CHECK_EQ(reloaded.doc.keys().peek_layer(), written.doc.keys().peek_layer());

    // An open is not undoable, and the stack it inherited pointed at a document
    // that no longer exists.
    CHECK_EQ(reloaded.undo.undo_depth(), std::size_t{0});
}

// =============================================================================
// Nokta listeleri — the first file a Turkish surveyor opens
// =============================================================================

TEST_CASE("NOKTALAR: Y sağa, X yukarı okunur")
{
    // THE ASSERTION THIS FILE EXISTS FOR. Turkish practice writes `no, Y, X` with
    // Y across and X up — the opposite of the mathematical convention — and a
    // reader that took them the other way round would put every point in the
    // wrong place plausibly enough that nobody would notice.
    TempDir tmp("noktalar");
    const std::string path = tmp.file("olcu.txt");
    {
        std::ofstream out(path);
        out << "# Ada 1284 poligon olcusu\n";
        out << "1;485320.543;4310220.250;845.120;NIRENGI\n";
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=NIRENGI", Origin::Test).ok());
    auto read = r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test);
    if (!read) FAIL_WITH("NOKTALAR", read.error().message);

    REQUIRE(r.doc.live_entity_count() == 1);
    const core::Box2 box = r.doc.extent();
    CHECK(box.min_x == 485320543);  // Y went to the easting
    CHECK(box.min_y == 4310220250); // X went to the northing
}

TEST_CASE("NOKTALAR: milimetre tam okunur, çift duyarlıktan geçmez")
{
    TempDir tmp("nokta-hassas");
    const std::string path = tmp.file("hassas.txt");
    {
        std::ofstream out(path);
        // A nine-figure easting with three decimals is not exactly representable
        // as a double; reading it through one loses the last millimetre.
        out << "1;485320.543;4310220.251\n";
        out << "2;485320.5435;4310220.2554\n"; // and the fourth digit rounds
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() == 2);

    const core::RingSpan a = r.doc.geometry().rings_of(r.doc.entities().slot[0]);
    CHECK(r.doc.geometry().ring_xs(a.first)[0] == 485320543);
    CHECK(r.doc.geometry().ring_ys(a.first)[0] == 4310220251);

    const core::RingSpan b = r.doc.geometry().rings_of(r.doc.entities().slot[1]);
    CHECK(r.doc.geometry().ring_xs(b.first)[0] == 485320544); // .5435 -> 544
    CHECK(r.doc.geometry().ring_ys(b.first)[0] == 4310220255);
}

TEST_CASE("NOKTALAR: virgül, noktalı virgül, sekme ve boşluk ayraçları")
{
    TempDir tmp("nokta-ayrac");

    struct Case
    {
        const char* name;
        const char* line;
    };

    const Case cases[] = {
        {"virgul.txt", "1,485320.000,4310220.000\n"},
        {"noktali.txt", "1;485320.000;4310220.000\n"},
        {"sekme.txt", "1\t485320.000\t4310220.000\n"},
        {"bosluk.txt", "1  485320.000   4310220.000\n"},
    };

    for (const Case& c : cases) {
        const std::string path = tmp.file(c.name);
        {
            std::ofstream out(path);
            out << c.line;
        }

        Rig r;
        REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
        auto read = r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test);
        if (!read) FAIL_WITH(c.name, read.error().message);
        REQUIRE(r.doc.live_entity_count() == 1);
        CHECK(r.doc.extent().min_x == 485320000);
    }
}

TEST_CASE("NOKTALAR: Türkçe ondalık virgülü noktalı virgüllü dosyada okunur")
{
    TempDir tmp("nokta-tr");
    const std::string path = tmp.file("tr.txt");
    {
        std::ofstream out(path);
        out << "1;485320,543;4310220,250\n"; // a Turkish-locale export
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test).ok());
    CHECK(r.doc.extent().min_x == 485320543);
}

TEST_CASE("NOKTALAR: eksen=XY sütunları ters okur")
{
    TempDir tmp("nokta-xy");
    const std::string path = tmp.file("xy.txt");
    {
        std::ofstream out(path);
        out << "1;4310220.000;485320.000\n"; // northing first
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTALAR dosya=\"" + path + "\" eksen=XY", Origin::Test).ok());
    CHECK(r.doc.extent().min_x == 485320000);
    CHECK(r.doc.extent().min_y == 4310220000);
}

TEST_CASE("NOKTALAR: numara, kot ve kod öznitelik olur")
{
    TempDir tmp("nokta-oznitelik");
    const std::string path = tmp.file("kodlu.txt");
    {
        std::ofstream out(path);
        out << "NIR-3;485320.000;4310220.000;845.120;ROPER\n";
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test).ok());

    const core::AttrTable& t = r.doc.attributes();
    auto no                  = r.doc.attribute(t.find("nokta_no"), 0);
    auto kot                 = r.doc.attribute(t.find("kot"), 0);
    auto kod                 = r.doc.attribute(t.find("kod"), 0);

    REQUIRE(no.ok());
    CHECK(no.value().text == "NIR-3");
    REQUIRE(kot.ok());
    CHECK(kot.value().number == 845120);
    REQUIRE(kod.ok());
    CHECK(kod.value().text == "ROPER");
}

TEST_CASE("NOKTALAR: okunan liste yazılıp aynen geri okunur")
{
    TempDir tmp("nokta-gidis-donus");
    const std::string in_path  = tmp.file("giris.txt");
    const std::string out_path = tmp.file("cikis.txt");
    {
        std::ofstream out(in_path);
        out << "1;485320.543;4310220.250;845.120;NIRENGI\n";
        out << "2;485360.000;4310265.000;845.300;PARSEL\n";
    }

    Rig a;
    REQUIRE(a.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(a.bus.execute_line("NOKTALAR dosya=\"" + in_path + "\"", Origin::Test).ok());
    auto written = a.bus.execute_line("NOKTALAR dosya=\"" + out_path + "\" yon=yaz", Origin::Test);
    if (!written) FAIL_WITH("NOKTALAR yaz", written.error().message);

    Rig b;
    REQUIRE(b.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    REQUIRE(b.bus.execute_line("NOKTALAR dosya=\"" + out_path + "\"", Origin::Test).ok());

    REQUIRE(b.doc.live_entity_count() == 2);
    CHECK(b.doc.extent().min_x == a.doc.extent().min_x);
    CHECK(b.doc.extent().min_y == a.doc.extent().min_y);
    CHECK(b.doc.extent().max_x == a.doc.extent().max_x);
    CHECK(b.doc.extent().max_y == a.doc.extent().max_y);

    const core::AttrTable& t = b.doc.attributes();
    auto no                  = b.doc.attribute(t.find("nokta_no"), 0);
    REQUIRE(no.ok());
    CHECK(no.value().text == "1");
}

TEST_CASE("NOKTALAR tek geri alma adımıdır")
{
    TempDir tmp("nokta-geri");
    const std::string path = tmp.file("cok.txt");
    {
        std::ofstream out(path);
        for (int i = 1; i <= 20; ++i)
            out << i << ";48532" << i << ".000;4310220.000\n";
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());
    const std::size_t before = r.doc.live_entity_count();

    REQUIRE(r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test).ok());
    REQUIRE(r.doc.live_entity_count() == before + 20);

    // Twenty points, ONE undo: an import is one step or the user presses Ctrl+Z
    // twenty times to get back where they were (io.md R17).
    REQUIRE(r.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK(r.doc.live_entity_count() == before);

    // THE COLUMNS STAY, and that is the design rather than a leak: declaring one
    // is a SCHEMA change and `SÜTUN` is `UndoPolicy::None` for the same reason a
    // layer is — rows are addressed against the schema, and undoing a declaration
    // would invalidate every row written against it. Re-importing therefore adds
    // no second `nokta_no`.
    CHECK(r.doc.attributes().find("nokta_no") != core::kNoAttr);
}

TEST_CASE("NOKTALAR: bozuk satır sessizce atlanmaz, numarasıyla bildirilir")
{
    TempDir tmp("nokta-bozuk");
    const std::string path = tmp.file("bozuk.txt");
    {
        std::ofstream out(path);
        out << "1;485320.000;4310220.000\n";
        out << "2;485330.000;BURASI-SAYI-DEGIL\n";
        out << "3;485340.000;4310220.000\n";
    }

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=N", Origin::Test).ok());

    // A point list quietly one row short is a boundary quietly missing a corner.
    auto read = r.bus.execute_line("NOKTALAR dosya=\"" + path + "\"", Origin::Test);

    const std::string reported = read ? r.transcript : r.transcript + read.error().message;
    CHECK(reported.find("koordinat okunamadı") != std::string::npos);
    CHECK(r.doc.live_entity_count() == 0); // and nothing was half-imported
}

TEST_CASE("IO: kılavuzlar dosyayla gider ve sırasıyla geri gelir")
{
    // K4: guides are saved with the drawing. Their block is OPTIONAL, so this
    // also pins the other half of that contract — a drawing with no guides must
    // write no guide block at all, or every golden fixture changes size.
    TempDir tmp("kilavuz");
    const std::string path = tmp.file("kilavuzlu.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(written.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(written.bus.execute_line("KILAVUZ yon=yatay deger=4310220500", Origin::Test).ok());
    REQUIRE(written.bus.execute_line("KILAVUZ yon=düşey deger=485320000", Origin::Test).ok());

    auto saved = written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test);
    if (!saved) FAIL_WITH("FARKLIKAYDET", saved.error().message);

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);

    REQUIRE(reloaded.doc.guides().size() == 2);
    CHECK(reloaded.doc.guides().axis(0) == core::GuideAxis::Horizontal);
    CHECK(reloaded.doc.guides().coordinate(0) == 4310220500);
    CHECK(reloaded.doc.guides().axis(1) == core::GuideAxis::Vertical);
    CHECK(reloaded.doc.guides().coordinate(1) == 485320000);
}

TEST_CASE("IO: kılavuzu olmayan bir çizim kılavuz bloğu yazmaz")
{
    TempDir tmp("kilavuzsuz");
    const std::string with    = tmp.file("kilavuzlu.pcad");
    const std::string without = tmp.file("kilavuzsuz.pcad");

    Rig a;
    REQUIRE(a.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(a.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(a.bus.execute_line("FARKLIKAYDET \"" + without + "\"", Origin::Test).ok());

    Rig b;
    REQUIRE(b.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(b.bus.execute_line("ALAN noktalar=0,0 10,0 10,10 0,10", Origin::Test).ok());
    REQUIRE(b.bus.execute_line("KILAVUZ yon=yatay deger=1000", Origin::Test).ok());
    REQUIRE(b.bus.execute_line("FARKLIKAYDET \"" + with + "\"", Origin::Test).ok());

    // The one with guides is strictly bigger; the one without pays nothing for a
    // feature it does not use, which is what keeps an old file readable and a
    // golden fixture stable (io.md R10).
    CHECK(fs::file_size(with) > fs::file_size(without));
}

TEST_CASE("IO: boş belge de gidip geliyor")
{
    TempDir tmp("empty");
    const std::string path = tmp.file("bos.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    CHECK_EQ(reloaded.doc.content_hash(), written.doc.content_hash());
    CHECK_EQ(reloaded.doc.live_entity_count(), std::size_t{0});
    CHECK_EQ(reloaded.doc.layers().size(), std::size_t{1}); // katman "0"
}

TEST_CASE("IO: iki kez kaydetmek bayt bayt aynı dosyayı üretir")
{
    // Determinism (§7.3): the same document must produce the same bytes, or a
    // golden fixture means nothing and every save churns version control.
    TempDir tmp("deterministic");
    Rig rig;
    draw_fixture(rig);

    const std::string a = tmp.file("a.pcad");
    const std::string b = tmp.file("b.pcad");
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + a + "\"", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + b + "\"", Origin::Test).ok());

    CHECK(read_bytes(a) == read_bytes(b));
}

TEST_CASE("IO: proje ayarları dosyayla gider, uygulama ve oturum ayarları gitmez")
{
    // model.md R39/R40, stated as a test: anything that can change a byte of an
    // exported legal document travels with the document; a theme does not.
    TempDir tmp("settings");
    const std::string path = tmp.file("ayarlar.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("AYAR core.crs.id TUREF/TM33", Origin::Test).ok());
    REQUIRE(written.bus.execute_line("TERCİH core.arayuz.tema koyu", Origin::Test).ok());
    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    REQUIRE(reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());

    CHECK_EQ(std::string(reloaded.bus.project_settings().get("core.crs.id").as_text()),
             std::string("TUREF/TM33"));
    CHECK(reloaded.bus.project_settings().is_explicit("core.crs.id"));

    // The application preference stayed on the machine that set it.
    CHECK(!reloaded.bus.app_settings().is_explicit("core.arayuz.tema"));
}

// ===========================================================================
// Hostile files. io.md R9, R10, R18, P6, P13.
// ===========================================================================

TEST_CASE("IO: bu yapının okuyamayacağı bir sürüm açıklayarak reddedilir")
{
    TempDir tmp("toonew");
    const std::string path = tmp.file("gelecek.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    // Say the file needs a reader from the future. io.md R9: name the version,
    // never crash, never a partial load.
    std::vector<char> bytes = read_bytes(path);
    REQUIRE(bytes.size() > sizeof(io::FileHeader));
    const std::uint32_t future = io::kFormatVersion + 7;
    std::memcpy(bytes.data() + offsetof(io::FileHeader, min_reader_version), &future,
                sizeof(future));
    write_bytes(path, bytes);

    Rig victim;
    auto opened = victim.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    REQUIRE(!opened.ok());
    CHECK(has_token(opened.error(), io::kErrTooNew));
    CHECK(opened.error().message.find(std::to_string(future)) != std::string::npos);
    CHECK_EQ(victim.doc.live_entity_count(), std::size_t{0}); // nothing half-loaded
}

TEST_CASE("IO: tanınmayan blok atlanır, dosya yine açılır")
{
    // io.md R10 is the whole forward-compatibility story: a block this build does
    // not know is skipped by its declared length, reported, and never fatal.
    TempDir tmp("unknown");
    const std::string path = tmp.file("ileri.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());
    const std::uint64_t hash = rig.doc.content_hash();

    // Splice in a block with an id from the future: payload appended after the
    // directory, one more directory entry, and the header updated to match.
    std::vector<char> bytes = read_bytes(path);
    io::FileHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));

    const std::uint64_t payload_offset = io::align_up(bytes.size());
    bytes.resize(static_cast<std::size_t>(payload_offset), '\0');
    const char payload[16] = "gelecek verisi";
    bytes.insert(bytes.end(), payload, payload + sizeof(payload));

    // The directory moves to the end so the new payload does not overlap it.
    std::vector<io::BlockEntry> directory(header.block_count);
    if (header.block_count != 0)
        std::memcpy(directory.data(), read_bytes(path).data() + header.directory_offset,
                    directory.size() * sizeof(io::BlockEntry));
    directory.push_back(
        io::BlockEntry{0x7FFF, 1, payload_offset, sizeof(payload), sizeof(payload)});

    const std::uint64_t new_directory = io::align_up(bytes.size());
    bytes.resize(static_cast<std::size_t>(new_directory), '\0');
    const auto* raw_dir = reinterpret_cast<const char*>(directory.data());
    bytes.insert(bytes.end(), raw_dir, raw_dir + directory.size() * sizeof(io::BlockEntry));

    header.block_count      = static_cast<std::uint32_t>(directory.size());
    header.directory_offset = new_directory;
    header.file_bytes       = bytes.size();
    std::memcpy(bytes.data(), &header, sizeof(header));
    write_bytes(path, bytes);

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    // ...and the user is told there was more in the file than they can see.
    CHECK(reloaded.transcript.find("tanımadığı") != std::string::npos);
}

TEST_CASE("IO: KentOSCad dosyası olmayan bir dosya adıyla birlikte reddedilir")
{
    TempDir tmp("notpiri");
    const std::string path = tmp.file("baska.pcad");
    write_bytes(path, std::vector<char>(512, 'x'));

    Rig rig;
    auto opened = rig.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    REQUIRE(!opened.ok());
    CHECK(has_token(opened.error(), io::kErrNotPiri));
    CHECK(opened.error().message.find(path) != std::string::npos);
}

TEST_CASE("IO: kesilmiş dosya her uzunlukta düzgün reddedilir")
{
    TempDir tmp("truncate");
    const std::string full = tmp.file("tam.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + full + "\"", Origin::Test).ok());

    const std::vector<char> bytes = read_bytes(full);
    REQUIRE(bytes.size() > 128);

    // Every truncation point, not a hand-picked one: a bounds check that holds at
    // 64 bytes and not at 65 is not a bounds check.
    std::size_t rejected = 0;
    for (std::size_t cut = 1; cut < bytes.size(); cut += 7) {
        const std::string path = tmp.file("kesik.pcad");
        write_bytes(path, std::vector<char>(bytes.begin(), bytes.begin() + static_cast<long>(cut)));

        Rig victim;
        auto opened = victim.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
        if (!opened) ++rejected;
        // Whether it was rejected or (impossibly) accepted, nothing may be left
        // half-loaded and nothing may crash — reaching this line is the assertion.
        CHECK((opened.ok() || victim.doc.live_entity_count() == 0));
    }
    CHECK(rejected > 0);
}

TEST_CASE("IO: bozulmuş baytlar çökme değil hata üretir")
{
    // A miniature of the libFuzzer harness in /tests/fuzz, run on every build so
    // the property is defended even where a fuzzer is not.
    TempDir tmp("corrupt");
    const std::string source = tmp.file("saglam.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + source + "\"", Origin::Test).ok());

    const std::vector<char> clean = read_bytes(source);
    std::size_t opened_anyway     = 0;

    for (std::size_t offset = 0; offset < clean.size(); offset += 3) {
        std::vector<char> bytes = clean;
        bytes[offset]           = static_cast<char>(bytes[offset] ^ 0xFF);

        const std::string path = tmp.file("bozuk.pcad");
        write_bytes(path, bytes);

        Rig victim;
        auto opened = victim.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
        if (opened) ++opened_anyway;
    }

    // Most flips land in a bounds-checked field and are refused; a few land in a
    // coordinate byte and produce a valid but different drawing. Both are correct
    // outcomes. What must never happen is the run not reaching this line.
    CHECK(opened_anyway < clean.size());
}

TEST_CASE("IO: başarısız açma açık belgeyi bozmaz")
{
    // io.md P11: no partial import, no orphan layer, and the drawing on screen
    // survives a failed open untouched.
    TempDir tmp("failsafe");
    Rig rig;
    draw_fixture(rig);

    const std::uint64_t hash = rig.doc.content_hash();
    const std::size_t live   = rig.doc.live_entity_count();

    auto opened = rig.bus.execute_line("AÇ \"" + tmp.file("yok.pcad") + "\"", Origin::Test);
    CHECK(!opened.ok());
    CHECK_EQ(rig.doc.content_hash(), hash);
    CHECK_EQ(rig.doc.live_entity_count(), live);
}

TEST_CASE("IO: dosya motoru bağlı değilken komutlar bunu söyler")
{
    // A headless client that never installed a FileService gets the same answer
    // BETİK gives without a script engine — never a crash, never a silent no-op.
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    register_builtin_commands(reg);
    bus.on_echo = [](std::string_view) {};

    auto saved = bus.execute_line("FARKLIKAYDET \"/tmp/olmayacak.pcad\"", Origin::Test);
    REQUIRE(!saved.ok());
    CHECK(saved.error().message.find("Dosya motoru bağlı değil") != std::string::npos);
}

TEST_CASE("IO: adı olmayan çizim KAYDET ile sessizce kaybolmaz")
{
    Rig rig;
    draw_fixture(rig);

    auto saved = rig.bus.execute_line("KAYDET", Origin::Test);
    REQUIRE(!saved.ok());
    CHECK(saved.error().message.find("FARKLIKAYDET") != std::string::npos);
}

TEST_CASE("IO: KAYDET, FARKLIKAYDET'in bağladığı dosyaya yazar")
{
    TempDir tmp("rebind");
    const std::string path = tmp.file("bagli.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());
    auto saved = rig.bus.execute_line("KAYDET", Origin::Test);
    REQUIRE(saved.ok());

    Rig reloaded;
    REQUIRE(reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());
    CHECK_EQ(reloaded.doc.content_hash(), rig.doc.content_hash());
}

// ===========================================================================
// The equality proof, for the file commands (test.md R3)
// ===========================================================================

TEST_CASE("IO: arayüz, komut satırı ve betik aynı dosyayı ve aynı günlüğü üretir")
{
    TempDir tmp("proof");

    const auto journal_of = [](const Journal& j) {
        std::string out;
        for (const auto& e : j.entries())
            out += e.command_id + " " + e.args.to_json().dump() + "\n";
        return out;
    };

    // ---- client 1: the GUI. A menu item starts the command and the file dialog
    //      supplies the one argument it needs. Nothing else. ----
    Rig gui;
    draw_fixture(gui);
    const std::string gui_path = tmp.file("gui.pcad");
    {
        auto started = gui.bus.begin_interactive("FARKLIKAYDET");
        REQUIRE(started.ok());
        auto& session = *started.value();
        REQUIRE(session.waiting());
        CHECK(session.prompt().message == "Kaydedilecek proje dosyası");
        CHECK(session.supply(Value::text(gui_path)).ok());
        CHECK(gui.bus.finish(session).ok());
    }

    // ---- client 2: the command line ----
    Rig cli;
    draw_fixture(cli);
    const std::string cli_path = tmp.file("cli.pcad");
    REQUIRE(cli.bus.execute_line("FARKLIKAYDET \"" + cli_path + "\"", Origin::Test).ok());

    // ---- client 3: a JSON script ----
    Rig scr;
    draw_fixture(scr);
    const std::string scr_path = tmp.file("betik.pcad");
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r =
            runner.run_text(R"([{"cmd":"core.saveas","args":{"dosya":")" + scr_path + R"("}}])");
        if (!r) FAIL_WITH("betik", r.error().message);
        REQUIRE(r.ok());
    }

    // The proof: three clients, one byte-identical file.
    const std::vector<char> from_gui = read_bytes(gui_path);
    CHECK(!from_gui.empty());
    CHECK(from_gui == read_bytes(cli_path));
    CHECK(from_gui == read_bytes(scr_path));

    // And the same journal, which for a ReadOnly command means the same three
    // drawings and no save line in any of them (a replay must not re-save).
    CHECK_EQ(journal_of(gui.journal), journal_of(cli.journal));
    CHECK_EQ(journal_of(cli.journal), journal_of(scr.journal));

    // ---- and the same again for AÇ, which IS journalled ----
    Rig open_cli;
    REQUIRE(open_cli.bus.execute_line("AÇ \"" + cli_path + "\"", Origin::Test).ok());

    Rig open_scr;
    {
        script::JsonRunner runner(open_scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"([{"cmd":"core.open","args":{"dosya":")" + cli_path + R"("}}])");
        REQUIRE(r.ok());
    }

    Rig open_gui;
    {
        auto started = open_gui.bus.begin_interactive("AÇ");
        REQUIRE(started.ok());
        auto& session = *started.value();
        CHECK(session.supply(Value::text(cli_path)).ok());
        CHECK(open_gui.bus.finish(session).ok());
    }

    CHECK_EQ(open_gui.doc.content_hash(), open_cli.doc.content_hash());
    CHECK_EQ(open_cli.doc.content_hash(), open_scr.doc.content_hash());
    CHECK_EQ(journal_of(open_gui.journal), journal_of(open_cli.journal));
    CHECK_EQ(journal_of(open_cli.journal), journal_of(open_scr.journal));
}

TEST_CASE("IO: iptal edilen dosya komutu hiçbir iz bırakmaz")
{
    // Article 9: every command lands a cancellation test with an empty undo delta.
    Rig rig;
    draw_fixture(rig);
    const std::uint64_t hash      = rig.doc.content_hash();
    const std::size_t undo_before = rig.undo.undo_depth();

    auto started = rig.bus.begin_interactive("FARKLIKAYDET");
    REQUIRE(started.ok());
    started.value()->cancel(); // ESC at the file dialog
    CHECK(rig.bus.finish(*started.value()).ok());

    CHECK_EQ(rig.doc.content_hash(), hash);
    CHECK_EQ(rig.undo.undo_depth(), undo_before);
}

TEST_CASE("IO: komut argümanları Value olarak gidip geliyor")
{
    // Article 9: a Value round-trip test per command. A command invocation is
    // DATA (Article 1.4), and a file command is no exception.
    Args args;
    args.set("dosya", Value::text("/veri/parsel.pcad"));
    args.set("bicim", Value::text("DXF"));

    auto back = Args::from_json(args.to_json());
    REQUIRE(back.ok());
    CHECK(back.value() == args);
}

// ===========================================================================
// External formats. data.md Enforcement: gated capability reports PENDING.
// ===========================================================================

TEST_CASE("IO: dış biçim arka ucu durumunu her hâlükârda bildirir")
{
    // Never silently absent. The status string names the option and the install
    // command when the backend is off, so a user is never left guessing whether
    // the format is unsupported or merely uninstalled.
    const std::string status = io::vector_backend_status();
    CHECK(!status.empty());

    // The allow-list is compiled in and is never empty (io.md P7).
    REQUIRE(!io::vector_formats().empty());
    CHECK(io::vector_format_by_id("DXF") != nullptr);
    CHECK(io::vector_format_by_id("GPKG") != nullptr);
    CHECK(io::vector_format_for_path("/veri/pafta.dxf") != nullptr);
    CHECK(io::vector_format_for_path("/veri/pafta.dwg") == nullptr); // io.md R13/R14

    if (!io::vector_backend_available()) {
        // The status has to name the option, or a user cannot tell an unsupported
        // format from an uninstalled one.
        CHECK(status.find("KENTOS_WITH_GDAL") != std::string::npos);
    }
}

TEST_CASE("IO: GDAL kapalıyken İÇEAKTAR sessizce başarılı olmaz")
{
    if (io::vector_backend_available())
        PENDING("GDAL açık; kapalı hâlin davranışı bu yapıda sınanamıyor.");
    Rig rig;
    auto r = rig.bus.execute_line("İÇEAKTAR \"/veri/pafta.dxf\"", Origin::Test);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("KENTOS_WITH_GDAL") != std::string::npos);
}

TEST_CASE("IO: DXF dışa aktar -> içe aktar gidiş dönüşü")
{
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; DXF gidiş-dönüşü sınanamıyor.");
    TempDir tmp("dxf");
    const std::string path = tmp.file("cizim.dxf");

    Rig source;
    REQUIRE(source.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        source.bus.execute_line("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400", Origin::Test)
            .ok());

    auto exported = source.bus.execute_line("DIŞAAKTAR \"" + path + "\"", Origin::Test);
    if (!exported) FAIL_WITH("DIŞAAKTAR", exported.error().message);
    REQUIRE(exported.ok());
    REQUIRE(fs::exists(path));

    Rig target;
    REQUIRE(target.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());

    // Measured as a DELTA, not as an absolute depth. Setting the CRS is itself an
    // undoable document change — it moves `Document::crs()`, which folds into
    // content_hash() — so counting from zero here would be counting the setup.
    // R17's claim is about the import: one import, ONE step.
    const std::size_t before = target.undo.undo_depth();

    auto imported = target.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);
    REQUIRE(imported.ok());

    CHECK(target.doc.live_entity_count() >= 1);

    // io.md R17: one import, one undo step, and undoing it leaves nothing.
    CHECK_EQ(target.undo.undo_depth() - before, std::size_t{1});
    REQUIRE(target.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(target.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("IO: DWG her sürümden okunur ve neyi atladığını söyler")
{
#ifndef KENTOS_DWG_SAMPLES
    PENDING("KENTOS_WITH_DWG=OFF; DWG okuma sınanamıyor.");
#else
    // io.md R13 names LibreDWG and R14 wants coverage MEASURED rather than
    // asserted. LibreDWG ships ten of its own drawings spanning r13 to 2018 —
    // GPLv3 like the library, so readable here without a licence question — and
    // this walks all of them.
    //
    // They are NOT R14's corpus. R14 asks for fifty licence-cleared real files
    // held outside the repository and referenced by URL and SHA-256 in
    // `/data/MANIFEST.json`, and a cadastral drawing from a Turkish office looks
    // nothing like an AutoCAD sample. This is what makes the reader testable
    // before that corpus exists, and the coverage numbers it prints are the shape
    // the real report will take.
    const fs::path dir = KENTOS_DWG_SAMPLES;
    if (!fs::exists(dir)) PENDING("LibreDWG örnek dosyaları bulunamadı: " + dir.string());

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.is_regular_file() && entry.path().extension() == ".dwg")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end()); // test.md R19: sorted iteration

    REQUIRE(files.size() >= 5);

    std::size_t opened = 0;
    for (const fs::path& file : files) {
        Rig rig;
        (void)rig.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test);
        const std::uint64_t before = rig.doc.content_hash();

        auto imported = rig.bus.execute_line("İÇEAKTAR \"" + file.string() + "\"", Origin::Test);

        if (imported) {
            ++opened;
            CHECK(rig.doc.live_entity_count() > 0);
        } else {
            // io.md R17: a refusal leaves the document exactly as it was.
            CHECK_EQ(rig.doc.content_hash(), before);
        }
    }

    // EVERY ONE OF THEM, and the number is the point: a drop here is a coverage
    // regression, which is what R14 asks the build to break on.
    CHECK_EQ(opened, files.size());
#endif
}

TEST_CASE("IO: DWG eski usul POLYLINE'i ve katman listesini getiriyor")
{
#ifndef KENTOS_DWG_SAMPLES
    PENDING("KENTOS_WITH_DWG=OFF; DWG POLYLINE okuma sınanamıyor.");
#else
    // TWO REGRESSIONS, both of which a real cadastral DWG walks straight into.
    //
    //   * THE OLD-STYLE POLYLINE. AutoCAD wrote `POLYLINE` for a decade before
    //     `LWPOLYLINE` existed, and its vertices are separate objects chained by
    //     handle and closed by a `SEQEND`. The reader knew only `LWPOLYLINE`, so
    //     every one of them fell through to "unsupported type" — a drawing came
    //     in without its parcel boundaries and the report blamed the file.
    //   * THE LAYER CHECKLIST. The reader counted entities per layer into a
    //     census and then never put it in the report, so the wizard's second page
    //     offered a DWG no layers to tick.
    //
    // Neither needs a fixture of our own: LibreDWG ships its own drawings, GPLv3
    // like the library, and `KENTOS_DWG_SAMPLES` points at them.
    const fs::path dir = KENTOS_DWG_SAMPLES;
    if (!fs::exists(dir)) PENDING("LibreDWG örnek dosyaları bulunamadı: " + dir.string());

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.is_regular_file() && entry.path().extension() == ".dwg")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end()); // test.md R19: sorted iteration
    REQUIRE(files.size() >= 5);

    std::size_t with_layers = 0;
    for (const fs::path& file : files) {
        core::Document scratch;
        auto probe = io::probe_import(scratch, file.string(), "EPSG:5254", std::stop_token{});
        if (!probe) continue;

        if (!probe.value().layers.empty()) ++with_layers;

        // A POLYLINE, ITS VERTICES AND ITS SEQEND ARE NOT LOSSES. The report
        // names every type it could not read; none of these three may be in it.
        for (const std::string& note : probe.value().notes) {
            INFO(file.filename().string() << ": " << note);
            CHECK(note.find("POLYLINE") == std::string::npos);
            CHECK(note.find("VERTEX") == std::string::npos);
            CHECK(note.find("SEQEND") == std::string::npos);
        }
    }

    // A drawing that read has layers, because everything in a DWG sits on one —
    // `0` at the very least. An empty list is the defect, not a quiet file.
    CHECK_EQ(with_layers, files.size());
#endif
}

TEST_CASE("IO: okunabilir her biçim dosya diyaloğunda görünür")
{
    // A format the file dialog does not offer is a format the user has no way to
    // know exists. DWG is the one that nearly went out that way: it is read by
    // LibreDWG and routed by extension, so it is absent from the GDAL allow-list
    // the dialog filter is built from — the reader worked and the shell's own
    // import action could not reach it.
    //
    // Checked here rather than in the shell because /tests links no Qt: what is
    // testable is that the two lists agree about what is readable.
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");

    bool dxf = false;
    bool shp = false;
    for (const io::VectorFormat& f : io::vector_formats()) {
        if (f.extension == ".dxf" && f.read) dxf = true;
        if (f.extension == ".shp" && f.read) shp = true;
    }
    CHECK(dxf);
    CHECK(shp);

    // And DWG is readable exactly when the build linked LibreDWG — the condition
    // `MainWindow::externalFormatFilter` asks before adding its entry.
    CHECK_EQ(io::dwg_backend_available(), io::dwg_backend_status().empty());
}

TEST_CASE("IO: DWG yazma yok, ve kütüphane düzeyinde yok")
{
    // io.md P8 forbids a native DWG writer while R14's coverage report stands,
    // and the dependency is compiled with `LIBREDWG_DISABLE_WRITE=ON` so the
    // encoder is not even linked. The command layer says the same thing: a `.dwg`
    // target is refused by name rather than by a failure halfway through writing.
    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 10,0", Origin::Test).ok());

    TempDir tmp("dwg-yaz");
    auto wrote = rig.bus.execute_line("DIŞAAKTAR \"" + tmp.file("cizim.dwg") + "\"", Origin::Test);
    CHECK(!wrote);
}

TEST_CASE("IO: .prj'siz bir DXF reddedilmez — çizimin sistemi varsayılır ve SÖYLENİR")
{
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; DXF içe aktarımı sınanamıyor.");

    // WHAT REAL FILES LOOK LIKE. A DXF from a surveying office is one file: no
    // `.prj` beside it, because DXF has nowhere to put a coordinate system in the
    // first place. Refusing every such drawing was not io.md R20's rule — which
    // is that an unlabelled coordinate must never be read SILENTLY — it was a
    // refusal of the format, and it made real cadastral drawings unopenable.
    //
    // The drawing's own system stands and the report SAYS it did. Nothing is
    // inferred from the numbers: an easting of 583 000 fits several Turkish TM
    // zones and guessing between them is the blunder R20 exists for.
    TempDir tmp("dxf-prjsiz");
    const std::string path = tmp.file("pafta.dxf");
    {
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << "0\nSECTION\n2\nENTITIES\n"
            << "0\nLWPOLYLINE\n8\nPARSEL\n90\n4\n70\n1\n"
            << "10\n583646.0\n20\n4401083.0\n"
            << "10\n583696.0\n20\n4401083.0\n"
            << "10\n583696.0\n20\n4401133.0\n"
            << "10\n583646.0\n20\n4401133.0\n"
            << "0\nENDSEC\n0\nEOF\n";
    }
    REQUIRE(!fs::exists(tmp.file("pafta.prj")));

    std::string said;
    Rig rig;
    rig.bus.on_echo = [&said](std::string_view t) { said += std::string(t); };
    REQUIRE(rig.bus.execute_line("AYAR core.crs.id EPSG:5256", Origin::Test).ok());

    auto imported = rig.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);

    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{1});

    // AND IT SAID SO. The danger R20 guards against lives in the silence.
    CHECK(said.find("koordinat sistemi bildirmiyor") != std::string::npos);
    CHECK(said.find("EPSG:5256") != std::string::npos);
}

TEST_CASE("IO: Shapefile içe aktarımı — parseller alan olarak gelir")
{
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; Shapefile içe aktarımı sınanamıyor.");

    // WHAT INSTITUTIONS ACTUALLY SEND. The land registry, the municipality and
    // the provincial directorate all hand over `.shp`; a program that cannot open
    // one is outside the workflow whatever else it can do.
    //
    // Written here with GPKG rather than with a checked-in binary fixture: the
    // point of the case is the READ path, and a fixture nobody can regenerate is
    // a fixture nobody can fix. GDAL writes the shapefile set from the same
    // document the assertions are then made against.
    TempDir tmp("shp");

    Rig source;
    REQUIRE(source.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(source.bus
                .execute_line("ALAN 485300000,4310200000 485360000,4310200000 "
                              "485360000,4310245000 485300000,4310245000",
                              Origin::Test)
                .ok());

    // Out through GPKG, which holds a face as a face, then across to shapefile
    // with GDAL's own writer — the file a user receives.
    const std::string gpkg = tmp.file("kaynak.gpkg");
    auto exported          = source.bus.execute_line("DIŞAAKTAR \"" + gpkg + "\"", Origin::Test);
    if (!exported) FAIL_WITH("DIŞAAKTAR", exported.error().message);
    REQUIRE(fs::exists(gpkg));

    const std::string shp = tmp.file("parsel.shp");
    const std::string cmd = "ogr2ogr -f \"ESRI Shapefile\" \"" + shp + "\" \"" + gpkg + "\"";
    if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0 || !fs::exists(shp))
        PENDING("ogr2ogr yok; Shapefile örneği üretilemedi.");

    Rig target;
    REQUIRE(target.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    const std::size_t before = target.undo.undo_depth();

    auto imported = target.bus.execute_line("İÇEAKTAR \"" + shp + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);

    CHECK_EQ(target.doc.live_entity_count(), std::size_t{1});

    // A FACE, not a line. The whole reason to read the format.
    int faces = 0;
    for (core::EntityId e = 0; e < target.doc.entities().size(); ++e) {
        if (!target.doc.alive(e)) continue;
        const core::RingSpan span = target.doc.geometry().rings_of(target.doc.entities().slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            if (target.doc.geometry().ring_role[r] == core::RingRole::Exterior) ++faces;
    }
    CHECK_EQ(faces, 1);

    // io.md R17: one import, one undo step, and undoing it leaves nothing.
    CHECK_EQ(target.undo.undo_depth() - before, std::size_t{1});
    REQUIRE(target.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(target.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("IO: eksik .shx Türkçe açıklanır, GDAL'ın config önerisiyle değil")
{
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");

    // What a user actually meets. Someone e-mails the `.shp` alone, or a zip
    // loses a member, and GDAL answers
    //
    //     Unable to open parsel.shx ... Set SHAPE_RESTORE_SHX config option to YES
    //
    // which tells a surveyor to set an environment variable they have never heard
    // of, in English, about a file they did not know existed. What happened is
    // that the set arrived incomplete, and that is what the message should say.
    TempDir tmp("shp-eksik");
    const std::string path = tmp.file("parsel.shp");
    {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.is_open());
        out << "not a shapefile";
    }

    Rig rig;
    auto imported = rig.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    REQUIRE(!imported);
    CHECK(imported.error().message.find(".shx") != std::string::npos);
    CHECK(imported.error().message.find("TEK dosya değildir") != std::string::npos);
}

TEST_CASE("IO: Shapefile YAZMA için açık değil, ve sebebini söylüyor")
{
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");

    // A shapefile holds exactly ONE geometry type. A drawing with parcels,
    // boundaries, monuments and parcel numbers in it cannot be written to one
    // file at all — GDAL refuses every feature after the first — so the driver is
    // read-only until somebody decides how one drawing becomes several files.
    // Refused by NAME rather than by a GDAL error nobody can act on.
    const io::VectorFormat* shp = io::vector_format_for_path("/veri/parsel.shp");
    REQUIRE(shp != nullptr);
    CHECK(shp->read);
    CHECK(!shp->write);
}

TEST_CASE("IO: DXF içe aktarımı noktayı, yazıyı ve KAPALI çizgiyi kaybetmiyor")
{
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; DXF içe aktarımı sınanamıyor.");

    // THE THREE THINGS A CADASTRAL DXF IS MADE OF, and all three used to be lost.
    //
    //   * DXF has no polygon. A parcel is an LWPOLYLINE with its closed flag set,
    //     and OGR hands that over as a LINESTRING whose last vertex repeats its
    //     first. Read as an open run — which is what happened — every parcel in
    //     the file came in as a LINE: no fill, no area, nothing for İFRAZ or
    //     TEVHİT to work on.
    //   * A DXF POINT is a surveyed control point or benchmark. It fell through
    //     to "unsupported geometry" and was skipped.
    //   * A DXF TEXT arrives as a POINT carrying its string in a `Text` field —
    //     every parcel and ada number on the sheet. Also skipped.
    TempDir tmp("dxf-tam");
    const std::string path = tmp.file("karisik.dxf");

    {
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << "0\nSECTION\n2\nENTITIES\n"
            // a closed square: 50 x 50 m, so 2500 m^2
            << "0\nLWPOLYLINE\n8\nPARSEL\n90\n4\n70\n1\n"
            << "10\n0.0\n20\n0.0\n"
            << "10\n50.0\n20\n0.0\n"
            << "10\n50.0\n20\n50.0\n"
            << "10\n0.0\n20\n50.0\n"
            // a surveyed point
            << "0\nPOINT\n8\nNIRENGI\n10\n100.0\n20\n200.0\n30\n0.0\n"
            // and a caption
            << "0\nTEXT\n8\nNUMARA\n10\n150.0\n20\n250.0\n40\n2.5\n1\nParsel 12\n"
            << "0\nENDSEC\n0\nEOF\n";
    }
    {
        std::ofstream prj(tmp.file("karisik.prj"));
        REQUIRE(prj.is_open());
        prj << "PROJCS[\"TUREF / TM36\",GEOGCS[\"TUREF\",DATUM[\"Turkish_National_Reference_"
               "Frame\",SPHEROID[\"GRS 1980\",6378137,298.257222101]],PRIMEM[\"Greenwich\",0],"
               "UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Transverse_Mercator\"],"
               "PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",36],"
               "PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",500000],"
               "PARAMETER[\"false_northing\",0],UNIT[\"metre\",1]]";
    }

    Rig rig;
    auto imported = rig.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);

    CHECK_EQ(rig.doc.live_entity_count(), std::size_t{3});

    int faces    = 0;
    int points   = 0;
    int captions = 0;
    for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e) {
        if (!rig.doc.alive(e)) continue;

        if (rig.doc.entities().kind[e] == core::kPointKind) {
            ++points;
            continue;
        }
        if (rig.doc.texts().has(rig.doc.entities().slot[e])) {
            ++captions;
            continue;
        }

        const core::RingSpan span = rig.doc.geometry().rings_of(rig.doc.entities().slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            if (rig.doc.geometry().ring_role[r] == core::RingRole::Exterior) ++faces;
    }

    CHECK_EQ(faces, 1);    // the parcel is a FACE, not a line
    CHECK_EQ(points, 1);   // the nirengi survived
    CHECK_EQ(captions, 1); // and so did the number
}

TEST_CASE("IO: DXF yazısı dosyadaki açıyla geliyor")
{
    // THE REGRESSION. A DXF caption carries a rotation (group code 50) and a
    // planner uses it: street names run along the street, parcel and ada numbers
    // along the parcel. GDAL puts it in the feature's STYLE STRING and nowhere
    // else — `LABEL(f:"Arial",t:"Yol",p:1,a:30,s:2g,c:#000000)` — and the reader
    // read only the size out of that string. Every caption in an imported plan
    // therefore came in horizontal, and a label laid along a road crossed it.
    //
    // A caption is a baseline plus a string, and `render/src/scene.cpp` reads its
    // facing from the first vertex to the last. So the angle is checked where it
    // actually lives: the direction of the two vertices the import produced.
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; DXF yazı açısı sınanamıyor.");

    TempDir tmp("dxf-yazi-aci");
    const std::string path = tmp.file("yazili.dxf");

    {
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << "0\nSECTION\n2\nENTITIES\n"
            // Rotated 30 degrees counter-clockwise.
            << "0\nTEXT\n8\nYOL\n10\n0.0\n20\n0.0\n40\n2.0\n50\n30.0\n1\nYol\n"
            // NOT rotated, and its own text contains `a:`. The angle used to be
            // looked for with a plain search, which finds that one and reads a
            // parcel's own label as its rotation.
            << "0\nTEXT\n8\nADA\n10\n100.0\n20\n100.0\n40\n2.0\n1\nAda: 12\n"
            << "0\nENDSEC\n0\nEOF\n";
    }

    Rig rig;
    auto imported = rig.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);

    // Both captions, by the string they carry.
    double turned = 1e9;
    double flat   = 1e9;
    for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e) {
        if (!rig.doc.alive(e)) continue;
        const auto slot = rig.doc.entities().slot[e];
        if (!rig.doc.texts().has(slot)) continue;

        const core::RingSpan span = rig.doc.geometry().rings_of(slot);
        REQUIRE(span.count >= 1);
        const auto xs = rig.doc.geometry().ring_xs(span.first);
        const auto ys = rig.doc.geometry().ring_ys(span.first);
        REQUIRE(xs.size() >= 2);

        const double degrees = std::atan2(static_cast<double>(ys.back() - ys.front()),
                                          static_cast<double>(xs.back() - xs.front())) *
                               180.0 / 3.14159265358979323846;

        if (rig.doc.texts().text(slot) == "Yol")
            turned = degrees;
        else if (rig.doc.texts().text(slot) == "Ada: 12")
            flat = degrees;
    }

    // A degree of slack: the baseline's ends are ground MILLIMETRES, so the
    // direction is quantised by the rounding and not exact.
    CHECK(std::abs(turned - 30.0) < 1.0);
    CHECK(std::abs(flat) < 1.0);
}

TEST_CASE("IO: DXF birden çok katmanı taşır — dışa aktarım ilk katmanda durmaz")
{
    // The regression this locks. DXF holds exactly ONE OGR layer, named
    // `entities`, and a drawing's layers live there as a `Layer` attribute. The
    // export asked OGR for a layer per KentOSCad layer, so the second call failed
    // with "Unable to have more than one OGR entities layer in a DXF file": the
    // first layer was written, the command reported the GDAL message, and the file
    // left on disk held a fraction of the drawing.
    //
    // The case above this one draws on a single layer, which is exactly why the
    // bug survived it. A cadastral drawing is never one layer.
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; çok katmanlı DXF sınanamıyor.");
    TempDir tmp("dxf-katman");
    const std::string path = tmp.file("cok-katman.dxf");

    Rig source;
    REQUIRE(source.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    for (const char* name : {"PARSEL", "YOL", "BINA"}) {
        REQUIRE(source.bus.execute_line(std::string("KATMAN ad=") + name, Origin::Test).ok());
        REQUIRE(
            source.bus
                .execute_line("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400", Origin::Test)
                .ok());
    }
    REQUIRE(source.doc.live_entity_count() == std::size_t{3});

    auto exported = source.bus.execute_line("DIŞAAKTAR \"" + path + "\"", Origin::Test);
    if (!exported) FAIL_WITH("DIŞAAKTAR", exported.error().message);
    REQUIRE(exported.ok());

    // Read it back through our own importer: every entity has to come home, not
    // just the one whose layer happened to be created first.
    Rig target;
    REQUIRE(target.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    REQUIRE(target.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test).ok());
    CHECK_EQ(target.doc.live_entity_count(), std::size_t{3});

    // And the layer names survive the trip, because a DXF that lost them is a
    // drawing a surveyor has to re-sort by hand.
    std::vector<std::string> names;
    for (core::LayerId l = 0; l < target.doc.layers().size(); ++l)
        if (target.doc.layer_entity_count(l) > 0) names.push_back(target.doc.layers()[l].name);
    std::sort(names.begin(), names.end());
    REQUIRE(names.size() == std::size_t{3});
    CHECK_EQ(names[0], std::string("BINA"));
    CHECK_EQ(names[1], std::string("PARSEL"));
    CHECK_EQ(names[2], std::string("YOL"));
}

TEST_CASE("IO: GeoPackage dışa aktar -> içe aktar gidiş dönüşü, koordinat mm cinsinden korunur")
{
    if (!io::vector_backend_available())
        PENDING("KENTOS_WITH_GDAL=OFF; GeoPackage gidiş-dönüşü sınanamıyor.");
    TempDir tmp("gpkg");
    const std::string path = tmp.file("parseller.gpkg");

    Rig source;
    REQUIRE(source.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(source.bus
                .execute_line("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400 "
                              "485440.861,4310321.111",
                              Origin::Test)
                .ok());

    REQUIRE(source.bus.execute_line("DIŞAAKTAR \"" + path + "\"", Origin::Test).ok());
    REQUIRE(fs::exists(path));

    Rig target;
    REQUIRE(target.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    auto imported = target.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) FAIL_WITH("İÇEAKTAR", imported.error().message);
    REQUIRE(imported.ok());
    REQUIRE(target.doc.live_entity_count() >= 1);

    // The whole point of Mm: a coordinate that left as 485320150 mm comes back as
    // 485320150 mm, not 485320149.9999999 (Article 2.4, model.md R21).
    const core::Box2 out = source.doc.extent();
    const core::Box2 in  = target.doc.extent();
    CHECK_EQ(in.min_x, out.min_x);
    CHECK_EQ(in.min_y, out.min_y);
    CHECK_EQ(in.max_x, out.max_x);
    CHECK_EQ(in.max_y, out.max_y);
}

TEST_CASE("IO: sanal dosya sistemi yolları reddedilir")
{
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF; /vsi reddi sınanamıyor.");
    // io.md P14: a dataset path must not become a network fetch or an archive
    // traversal, whoever typed it — a user, a script or the AI.
    Rig rig;
    auto r =
        rig.bus.execute_line("İÇEAKTAR \"/vsicurl/https://example.invalid/a.dxf\"", Origin::Test);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("sanal dosya sistemi") != std::string::npos);
}

TEST_CASE("IO: proje dosyası İÇEAKTAR ile değil AÇ ile açılır")
{
    TempDir tmp("wrongverb");
    const std::string path = tmp.file("proje.pcad");

    Rig rig;
    draw_fixture(rig);
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    auto r = rig.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("AÇ komutunu kullanın") != std::string::npos);
}

// ===========================================================================
// The fuzz corpus, replayed on every build
// ===========================================================================

TEST_CASE("IO: fuzz tohum korpusundaki her dosya çökmeden ele alınır")
{
    // CLAUDE.md 6.7 ships the harness and the corpus with the format. The libFuzzer
    // target in /tests/fuzz needs Clang; this replays the same seeds through the
    // same reader on every build, so the corpus is never dead weight.
    const fs::path corpus = fs::path(KENTOS_FUZZ_DIR) / "tohum" / "proje";
    if (!fs::exists(corpus)) PENDING("Fuzz tohum korpusu bulunamadı: " + corpus.string());

    std::vector<fs::path> seeds;
    for (const auto& entry : fs::directory_iterator(corpus))
        if (entry.is_regular_file()) seeds.push_back(entry.path());
    std::sort(seeds.begin(), seeds.end()); // test.md R19: sorted directory iteration

    std::size_t handled = 0;
    for (const fs::path& seed : seeds) {
        Rig rig;
        auto opened = rig.bus.execute_line("AÇ \"" + seed.string() + "\"", Origin::Test);
        // Accepted or rejected are both fine. Reaching the next line is the test.
        (void)opened;
        ++handled;
    }
    CHECK(handled == seeds.size());
    CHECK(handled >= 4);
}

TEST_CASE("IO: DXF ve Shapefile tohum korpusu da içe aktarımdan geçirilir")
{
    // The same claim for the format corpora, and it was not being kept: the
    // replay above reads `tohum/proje` through `AÇ` and stopped there, so the DXF
    // seeds — and now the shapefile ones — were only ever exercised by a libFuzzer
    // build nobody makes on an ordinary machine. `tests/fuzz/CMakeLists.txt` says
    // the corpora are "NOT dead weight when this is off"; this is what makes that
    // sentence true.
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");

    std::size_t handled = 0;
    for (const char* klasor : {"dxf", "shp"}) {
        const fs::path corpus = fs::path(KENTOS_FUZZ_DIR) / "tohum" / klasor;
        if (!fs::exists(corpus)) continue;

        std::vector<fs::path> seeds;
        for (const auto& entry : fs::directory_iterator(corpus))
            if (entry.is_regular_file()) seeds.push_back(entry.path());
        std::sort(seeds.begin(), seeds.end()); // test.md R19: sorted iteration

        for (const fs::path& seed : seeds) {
            Rig rig;
            (void)rig.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test);
            const std::uint64_t before = rig.doc.content_hash();

            auto imported =
                rig.bus.execute_line("İÇEAKTAR \"" + seed.string() + "\"", Origin::Test);

            // Accepted or refused are both fine; what is NOT fine is a refusal
            // that left something behind (io.md R17).
            if (!imported) CHECK_EQ(rig.doc.content_hash(), before);
            ++handled;
        }
    }
    CHECK(handled >= 3);
}

TEST_CASE("DXF: daire daire, yay yay olarak okunur — çokgen olarak değil")
{
    // WHAT THIS LOCKS. OGR's DXF driver tessellates a CIRCLE and an ARC into a
    // LINESTRING before this module sees them, so a reader that trusted the
    // geometry alone stored a real cadastral file's 3 874 circles and 3 523 arcs
    // as many-cornered polygons: no centre, no radius, an area that was the
    // polygon's rather than pi r squared, and nothing for a centre snap to find.
    // AutoCAD and FreeCAD keep them as curves.
    //
    // Three things have to hold at once, and each of them was wrong at some point
    // while this was written:
    //
    //   * the CLASS is read from the file (`SubClasses`), never guessed from the
    //     shape — a surveyor's hand-drawn 64-gon must stay a polygon;
    //   * the centre is fitted in a frame TRANSLATED to the first vertex, because
    //     a TUREF northing squared is 2e19 and a double stops counting by ones at
    //     9e15 — untranslated, two identical circles in one file came out one as a
    //     circle and one as a polygon;
    //   * the sweep direction is read from the tessellation, because OGR hands
    //     these arcs over CLOCKWISE and taking the ends in arrival order stored
    //     the COMPLEMENT — a 120 degree arc became the 240 degree one.
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");

    const fs::path seed = fs::path(KENTOS_FUZZ_DIR) / "tohum" / "dxf" / "05-daire-yay-cizgi.dxf";
    if (!fs::exists(seed)) PENDING("Fikstür bulunamadı: " + seed.string());

    Rig rig;
    (void)rig.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test);
    auto imported = rig.bus.execute_line("İÇEAKTAR \"" + seed.string() + "\"", Origin::Test);
    REQUIRE(imported.ok());
    REQUIRE_EQ(rig.doc.live_entity_count(), 3u);

    // The measures, in millimetres, from the kind rather than from the stored run.
    const auto measure = [&rig](core::EntityId e, core::Mm2& area, core::Mm& length) {
        const std::uint32_t slot[1]{rig.doc.entities().slot[e]};
        const core::KindSpec* spec = core::builtin_kinds().find(rig.doc.entities().kind[e]);
        REQUIRE(spec != nullptr);
        spec->area(rig.doc.geometry(), core::SlotSpan(slot, 1), std::span<core::Mm2>(&area, 1));
        REQUIRE(spec->perimeter != nullptr);
        spec->perimeter(rig.doc.geometry(), core::SlotSpan(slot, 1),
                        std::span<core::Mm>(&length, 1));
    };

    bool saw_circle = false;
    bool saw_arc    = false;
    bool saw_line   = false;

    for (core::EntityId e = 0; e < rig.doc.entities().size(); ++e) {
        if (!rig.doc.alive(e)) continue;

        core::Mm2 area{0};
        core::Mm length{0};
        measure(e, area, length);

        switch (rig.doc.entities().kind[e]) {
        case core::kCircleKind: {
            saw_circle = true;
            // r = 8 m. pi r^2 = 201 061 930 mm^2, 2 pi r = 50 265 mm. Within a
            // millimetre of the exact figure, which is the storage unit.
            CHECK(std::abs(area - core::Mm2{201'061'930}) <= 2000);
            CHECK(std::abs(length - core::Mm{50'265}) <= 1);
            break;
        }
        case core::kArcKind: {
            saw_arc = true;
            // An arc encloses nothing (R10), and r = 12,5 m over 120 degrees is
            // 26 180 mm — NOT the 52 360 the reversed ends produced.
            CHECK_EQ(area, core::Mm2{0});
            CHECK(std::abs(length - core::Mm{26'180}) <= 2);
            break;
        }
        case core::kPolylineKind: {
            saw_line = true;
            CHECK_EQ(area, core::Mm2{0});
            CHECK_EQ(length, core::Mm{30'000});
            break;
        }
        default: break;
        }
    }

    CHECK(saw_circle);
    CHECK(saw_arc);
    CHECK(saw_line);
}

namespace {

/// Reads a whole file. The io tests otherwise work through the document, so this
/// is the one place that looks at the bytes a writer produced.
std::string slurp(const std::string& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

// ----------------------------------------------------------------- QML ----

TEST_CASE("QML: sayılar yerel ayara değil biçime aittir")
{
    // The bug this locks. width_mm used snprintf("%.3f"), which writes the decimal
    // separator of the CURRENT LOCALE — and on the Turkish system this was built
    // on that is a comma. The first export wrote outline_width="0,700" and QGIS
    // reads that as zero: a hairline where a 0.7 mm cadastral boundary belongs.
    // Verified by loading the file back through QGIS itself.
    TempDir tmp("qml");
    const std::string path = tmp.file("katman.qml");

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=KONUT renk=0xFF8C541A", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());
    REQUIRE(r.bus
                .execute_line("STİL katman=KONUT renk=0xFF5D3A12 kalinlik=700 dolgu=0xFF8C541A",
                              Origin::Test)
                .ok());
    REQUIRE(r.bus.execute_line("STİLAKTAR KONUT \"" + path + "\"", Origin::Test).ok());

    const std::string body = slurp(path);
    CHECK(body.find("outline_width\" v=\"0.700\"") != std::string::npos);
    CHECK(body.find("0,700") == std::string::npos); // never a comma, on any machine

    // Colours reach QGIS as r,g,b,a decimal — written the KentOSCad way they would
    // load as black and the user would blame the export.
    CHECK(body.find("140,84,26,255") != std::string::npos); // fill  #8C541A
    CHECK(body.find("93,58,18,255") != std::string::npos);  // stroke #5D3A12
}

TEST_CASE("QML: ölçek penceresi Rendering kategorisini de bildirir")
{
    // QGIS loads ONLY the style categories the file names, and a scale window is
    // in Rendering, not Symbology. Declaring symbology alone made QGIS read the
    // colours and silently drop the range — the export looked correct and the
    // drawing behaved differently over there.
    TempDir tmp("qml-olcek");
    const std::string windowed = tmp.file("pencereli.qml");
    const std::string plain    = tmp.file("penceresiz.qml");

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=LEKE", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("STİL katman=LEKE renk=0xFF6A1B9A olcek_max=5000", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("STİLAKTAR LEKE \"" + windowed + "\"", Origin::Test).ok());

    const std::string with = slurp(windowed);
    CHECK(with.find("styleCategories=\"Symbology|Rendering\"") != std::string::npos);
    CHECK(with.find("hasScaleBasedVisibilityFlag=\"1\"") != std::string::npos);
    CHECK(with.find("minScale=\"5000\"") != std::string::npos);

    // A layer with no window says Symbology only, so loading it cannot reset a
    // scale range the target layer already had.
    REQUIRE(r.bus.execute_line("KATMAN ad=DUZ", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("ALAN 485400,4310200 485460,4310200 485460,4310245", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("STİLAKTAR DUZ \"" + plain + "\"", Origin::Test).ok());

    const std::string without = slurp(plain);
    CHECK(without.find("styleCategories=\"Symbology\"") != std::string::npos);
    CHECK(without.find("hasScaleBasedVisibilityFlag") == std::string::npos);
}

TEST_CASE("QML: katman adındaki XML karakterleri kaçırılır")
{
    TempDir tmp("qml-xml");
    const std::string path = tmp.file("kacis.qml");

    Rig r;
    REQUIRE(r.bus.execute_line("KATMAN ad=\"A & B\"", Origin::Test).ok());
    REQUIRE(
        r.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test).ok());
    REQUIRE(r.bus.execute_line("STİLAKTAR \"A & B\" \"" + path + "\"", Origin::Test).ok());

    const std::string body = slurp(path);
    CHECK(body.find("A &amp; B") != std::string::npos);
}

TEST_CASE("IO: öznitelik ve metin dosyayla gider ve parmak izi tutar")
{
    // The regression this locks, and it is a data-loss one: attributes and text
    // fold into content_hash(), so a file that dropped them reopened as a
    // DIFFERENT document. The reader's own fingerprint check said so on every
    // load — which is how the gap was found, and is exactly what that check is
    // for.
    TempDir tmp("kalicilik");
    const std::string path = tmp.file("t.pcad");

    std::uint64_t saved_hash = 0;
    {
        Rig r;
        REQUIRE(r.bus.execute_line("SÜTUN ada_no tam_sayi", Origin::Test).ok());
        REQUIRE(r.bus.execute_line("SÜTUN gosterim metin", Origin::Test).ok());
        REQUIRE(r.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
        REQUIRE(
            r.bus.execute_line("ALAN 485300,4310200 485360,4310200 485360,4310245", Origin::Test)
                .ok());
        REQUIRE(r.bus.execute_line("ÖZNİTELİK ada_no 1 1234", Origin::Test).ok());
        REQUIRE(
            r.bus.execute_line("ÖZNİTELİK gosterim 1 \"TOPLU KONUT ALANI\"", Origin::Test).ok());
        REQUIRE(r.bus.execute_line("METİN 485310,4310210 \"1234/7\" 2000", Origin::Test).ok());

        saved_hash = r.doc.content_hash();
        REQUIRE(r.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());
    }

    Rig back;
    REQUIRE(back.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());

    // The schema comes back, in order, with its types.
    REQUIRE(back.doc.attributes().columns() == std::size_t{2});
    const auto ada = back.doc.attributes().find("ada_no");
    const auto gos = back.doc.attributes().find("gosterim");
    REQUIRE(ada != core::kNoAttr);
    REQUIRE(gos != core::kNoAttr);

    auto number = back.doc.attribute(ada, 0);
    REQUIRE(number.ok());
    CHECK(number.value().present);
    CHECK_EQ(number.value().number, std::int64_t{1234});

    auto text = back.doc.attribute(gos, 0);
    REQUIRE(text.ok());
    CHECK_EQ(text.value().text, std::string("TOPLU KONUT ALANI"));

    // The caption comes back with its height, and on the right entity.
    bool found_caption = false;
    for (core::EntityId e = 0; e < back.doc.entities().size(); ++e) {
        if (!back.doc.entities().alive(e)) continue;
        const std::uint32_t slot = back.doc.entities().slot[e];
        if (!back.doc.texts().has(slot)) continue;
        found_caption = true;
        CHECK_EQ(back.doc.texts().text(slot), std::string_view("1234/7"));
        CHECK_EQ(back.doc.texts().height(slot), core::Mm{2000});
    }
    CHECK(found_caption);

    // And the whole document is the same document. This is the assertion that
    // would have caught the loss on its own.
    CHECK_EQ(back.doc.content_hash(), saved_hash);
}

TEST_CASE("IO: yığılmış sembol dosyayla gidip geliyor")
{
    // The regression this locks down. `.pcad` wrote only the resolved appearance
    // of each style, so a drawing whose gösterim was a fill under a boundary under
    // a glyph came back as the boundary alone. Everything else about the document
    // survived, which is what made it hard to see: the colours were right, the
    // pattern was gone, and the only thing that said so was the fingerprint.
    TempDir tmp("sembol");
    const std::string path = tmp.file("sembol.pcad");

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ORMAN", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("ALAN 485300000,4310200000 485370000,4310200000 "
                              "485370000,4310250000 485300000,4310250000",
                              Origin::Test)
                .ok());

    // A stack built one invocation at a time, which is how the designer drives it
    // and how a script writes the same thing.
    REQUIRE(rig.bus.execute_line("STİL katman=ORMAN tip=dolgu dolgu=805568546", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("STİL katman=ORMAN ekle=evet tip=nokta-desen-dolgu sekil=ucgen "
                              "birim=zemin boyut=3000 aralik=9000 aci=15000000 renk=4280645666",
                              Origin::Test)
                .ok());

    const core::StyleId sid = rig.doc.entities().style[0];
    REQUIRE(sid != core::kByLayerStyle);

    const core::Symbol before = rig.doc.styles().symbol_at(sid);
    REQUIRE(before.layers.size() == std::size_t{2});
    CHECK(before.layers[1].type == core::SymbolLayerType::PointPatternFill);
    CHECK(before.layers[1].shape == core::MarkerShape::Triangle);
    CHECK(before.layers[1].interval.unit == core::Unit::Ground);
    CHECK_EQ(before.layers[1].interval.value, 9000);
    CHECK_EQ(before.layers[1].angle_udeg, 15000000);

    const std::uint64_t hash = rig.doc.content_hash();
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());

    // The fingerprint is the whole assertion: it folds every symbol layer, so a
    // document that hashes the same cannot have lost one.
    CHECK_EQ(reloaded.doc.content_hash(), hash);

    const core::StyleId reopened = reloaded.doc.entities().style[0];
    CHECK_EQ(reopened, sid);
    const core::Symbol after = reloaded.doc.styles().symbol_at(reopened);
    REQUIRE(after.layers.size() == std::size_t{2});
    CHECK(after == before);
}

TEST_CASE("IO: boş katmanın varsayılan sembolü dosyayla gidip geliyor")
{
    TempDir tmp("katman-sembolu");
    const std::string path = tmp.file("katman-sembolu.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("KATMAN ORMAN", Origin::Test).ok());
    REQUIRE(
        written.bus.execute_line("STİL katman=ORMAN tip=dolgu dolgu=805568546", Origin::Test).ok());
    REQUIRE(written.bus
                .execute_line("STİL katman=ORMAN ekle=evet tip=cizgi renk=4280645666", Origin::Test)
                .ok());

    const core::LayerId layer = written.doc.find_layer("ORMAN");
    REQUIRE(layer != core::kNoLayer);
    const core::StyleId style = written.doc.layer(layer)->style;
    REQUIRE(style != core::kByLayerStyle);
    const core::Symbol before = written.doc.styles().symbol_at(style);
    REQUIRE(before.layers.size() == std::size_t{2});

    const std::uint64_t hash = written.doc.content_hash();
    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    REQUIRE(reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());
    CHECK_EQ(reloaded.doc.content_hash(), hash);

    const core::LayerId reopened_layer = reloaded.doc.find_layer("ORMAN");
    REQUIRE(reopened_layer != core::kNoLayer);
    const core::StyleId reopened_style = reloaded.doc.layer(reopened_layer)->style;
    CHECK_EQ(reopened_style, style);
    CHECK(reloaded.doc.styles().symbol_at(reopened_style) == before);
}

TEST_CASE("IO: sembolsüz eski dosya parmak izini koruyor")
{
    // A file written before symbols were persisted carries neither block, and
    // reads back as a plain colour and width per style — which is exactly what it
    // meant. Simulated by saving a drawing that declares no stack: the writer
    // still emits the blocks, and every symbol in them is one plain layer, so the
    // ids and the fingerprint have to come back unchanged.
    TempDir tmp("duz-sembol");
    const std::string path = tmp.file("duz.pcad");

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN YOL", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 10000,0 10000,10000", Origin::Test).ok());
    REQUIRE(
        rig.bus.execute_line("STİL katman=YOL renk=4278190335 kalinlik=700", Origin::Test).ok());

    const std::uint64_t hash = rig.doc.content_hash();
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    REQUIRE(reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());
    CHECK_EQ(reloaded.doc.content_hash(), hash);
}

TEST_CASE("IO: çizgi tipi dosyayla gidip geliyor")
{
    // A line type is four numbers and the drawing carries them, for exactly the
    // reason it carries the bytes of a raster symbol: a table that lived only in
    // the catalogue package would mean a plan sheet renders differently on a
    // machine where the package is not installed, and a plan sheet is a legal
    // document.
    TempDir tmp("cizgitipi");
    const std::string path = tmp.file("cizgitipi.pcad");

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN SINIR", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 60,0 60,45", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("STİL katman=SINIR tip=cizgi kalinlik=500 desen=\"8 1 1 1\"",
                              Origin::Test)
                .ok());

    REQUIRE(rig.doc.dashes().size() == std::size_t{2});
    CHECK(rig.doc.dashes().origin(1).find("STİL") != std::string_view::npos);

    const std::uint64_t hash = rig.doc.content_hash();
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig back;
    REQUIRE(back.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());

    // R38/§16: the fingerprint folds the line types, so an equal hash is already
    // the proof that the pattern survived. The fields are checked too, because a
    // hash says "the same" and a reader wants to know "the same WHAT".
    CHECK_EQ(back.doc.content_hash(), hash);
    REQUIRE(back.doc.dashes().size() == std::size_t{2});

    const core::DashPattern& p = back.doc.dashes().at(1);
    CHECK_EQ(static_cast<int>(p.count), 4);
    CHECK_EQ(static_cast<int>(p.lengths[0]), 800);
    CHECK_EQ(static_cast<int>(p.lengths[3]), 100);
    CHECK(back.doc.dashes().origin(1).find("STİL") != std::string_view::npos);

    const core::Symbol sym = back.doc.styles().symbol_at(back.doc.entities().style[0]);
    REQUIRE(!sym.layers.empty());
    CHECK_EQ(static_cast<int>(sym.layers.front().look.dash), 1);
}

TEST_CASE("IO: çizgi tipi olmayan dosya eskisi gibi okunuyor")
{
    // The block is OPTIONAL and its absence is not a defect: every file written
    // before line types existed has none, every stroke in it was solid, and that
    // is exactly what an empty store draws. This pins io.md R10 — which is also
    // why adding the block was not a format version bump.
    TempDir tmp("desensiz");
    const std::string path = tmp.file("desensiz.pcad");

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN SINIR", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());
    CHECK_EQ(rig.doc.dashes().size(), std::size_t{1}); // the sentinel alone

    const std::uint64_t hash = rig.doc.content_hash();
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig back;
    REQUIRE(back.bus.execute_line("AÇ \"" + path + "\"", Origin::Test).ok());
    CHECK_EQ(back.doc.dashes().size(), std::size_t{1});
    CHECK_EQ(back.doc.content_hash(), hash);
}

TEST_CASE("IO: gömülü görsel dosyayla gidip geliyor")
{
    // MPYY publishes its symbology as pictures, so a drawing that uses a gösterim
    // carries the picture. A path would break the moment the drawing is emailed to
    // the belediye that has to check it, which is the case this format exists for.
    TempDir tmp("gorsel");
    const std::string path = tmp.file("gorsel.pcad");
    const std::string pack = std::string(KENTOS_DATA_DIR) + "/catalogs/mpyy/plan-gosterim.json";

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN OSB", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("ALAN 485300000,4310200000 485385000,4310200000 "
                              "485385000,4310260000 485300000,4310260000",
                              Origin::Test)
                .ok());

    auto styled = rig.bus.execute_line(
        "STİL katman=OSB paket=\"" + pack + "\" kod=ortak-organize-sanayi-bolgesi", Origin::Test);
    if (!styled) FAIL_WITH("STİL", styled.error().message);
    REQUIRE(styled.ok());

    // The row is published with a hatch, a line type and a glyph, so the symbol is
    // a stack of raster layers rather than a colour standing in for them.
    const core::StyleId sid = rig.doc.entities().style[0];
    REQUIRE(sid != core::kByLayerStyle);
    const core::Symbol before = rig.doc.styles().symbol_at(sid);
    CHECK(before.layers.size() >= std::size_t{2});

    bool has_raster = false;
    for (const core::SymbolLayer& l : before.layers)
        if (l.image != core::kNoImage) has_raster = true;
    CHECK(has_raster);

    REQUIRE(rig.doc.images().size() >= std::size_t{2}); // the sentinel plus at least one picture
    CHECK(rig.doc.images().total_bytes() > std::size_t{0});

    // Provenance travels with the bytes: a plan sheet that cannot say which annex
    // its symbols came from cannot be checked (model.md R35, CLAUDE.md 11.7).
    CHECK(rig.doc.images().origin(1).find("ortak-organize-sanayi-bolgesi") !=
          std::string_view::npos);

    const std::uint64_t hash = rig.doc.content_hash();
    REQUIRE(rig.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());

    CHECK_EQ(reloaded.doc.content_hash(), hash);
    CHECK_EQ(reloaded.doc.images().size(), rig.doc.images().size());
    CHECK_EQ(reloaded.doc.images().total_bytes(), rig.doc.images().total_bytes());

    const core::Symbol after = reloaded.doc.styles().symbol_at(reloaded.doc.entities().style[0]);
    CHECK(after == before);
}

TEST_CASE("IO: aynı görsel iki kez eklenince tek kopya saklanıyor")
{
    // A hatch shared by nine plan types is stored once. Without content
    // deduplication a sheet using twenty gösterim from one annex would carry
    // twenty copies of the same scan.
    Rig rig;
    const std::string pack = std::string(KENTOS_DATA_DIR) + "/catalogs/mpyy/plan-gosterim.json";

    REQUIRE(rig.bus.execute_line("KATMAN A", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("KATMAN B", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ÇİZGİ 0,0 1000,0 1000,1000 0,1000 0,0", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("KATMAN A", Origin::Test).ok());

    const std::string style =
        "STİL katman=A paket=\"" + pack + "\" kod=ortak-organize-sanayi-bolgesi";
    REQUIRE(rig.bus.execute_line(style, Origin::Test).ok());
    const std::size_t after_first = rig.doc.images().size();

    // The same row again: every picture it names is already there.
    REQUIRE(rig.bus.execute_line(style, Origin::Test).ok());
    CHECK_EQ(rig.doc.images().size(), after_first);
}

TEST_CASE("IO: daire dosyaya gidip daire olarak geri geliyor")
{
    // The kind column is what tells a circle from a two-point line: both are one
    // Open ring of two vertices, and the geometry alone cannot separate them. A
    // reader that guessed from the shape would turn every saved circle into a
    // short line pointing east.
    TempDir tmp("circle-roundtrip");
    const std::string path = tmp.file("daire.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("KATMAN ad=YAPI", Origin::Test).ok());
    REQUIRE(
        written.bus.execute_line("DAİRE merkez=485300,4310200 cevre=485325,4310200", Origin::Test)
            .ok());
    REQUIRE(written.bus.execute_line("ÇİZGİ 485300,4310200 485325,4310200", Origin::Test).ok());

    const std::uint64_t hash = written.doc.content_hash();

    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());

    REQUIRE_EQ(reloaded.doc.live_entity_count(), std::size_t{2});
    CHECK_EQ(reloaded.doc.content_hash(), hash);

    // The circle came back a circle, and the line beside it — the same two
    // vertices, the same ring — came back a line.
    CHECK(reloaded.doc.entities().kind[0] == core::kCircleKind);
    CHECK(reloaded.doc.entities().kind[1] == core::kPolylineKind);

    const std::uint32_t slot = reloaded.doc.entities().slot[0];
    CHECK_EQ(core::circle_centre_of(reloaded.doc.geometry(), slot).x, core::Mm{485300000});
    CHECK_EQ(core::circle_radius_of(reloaded.doc.geometry(), slot), core::Mm{25000});

    // And its box is still the circle's, not the box of its two stored vertices.
    const core::Box2 box = reloaded.doc.entity_extent(0);
    CHECK_EQ(box.min_y, core::Mm{4310175000});
    CHECK_EQ(box.max_y, core::Mm{4310225000});
}

TEST_CASE("IO: yay dosyaya gidip yay olarak geri geliyor")
{
    TempDir tmp("arc-roundtrip");
    const std::string path = tmp.file("yay.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("KATMAN ad=YOL", Origin::Test).ok());
    REQUIRE(written.bus
                .execute_line("YAY merkez=485300,4310200 baslangic=485330,4310200 "
                              "bitis=485300,4310230",
                              Origin::Test)
                .ok());

    const std::uint64_t hash = written.doc.content_hash();
    const core::Box2 box     = written.doc.entity_extent(0);

    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) FAIL_WITH("AÇ", opened.error().message);
    REQUIRE(opened.ok());

    REQUIRE_EQ(reloaded.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    CHECK(reloaded.doc.entities().kind[0] == core::kArcKind);

    const std::uint32_t slot = reloaded.doc.entities().slot[0];
    CHECK_EQ(core::arc_radius_of(reloaded.doc.geometry(), slot), core::Mm{30000});
    CHECK_EQ(core::arc_start_of(reloaded.doc.geometry(), slot).x, core::Mm{485330000});

    // The arc's own box, rebuilt on load rather than taken from the four stored
    // vertices — which include the centre and a handle due east.
    CHECK_EQ(reloaded.doc.entity_extent(0), box);
}
