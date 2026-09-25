// SPDX-License-Identifier: GPL-3.0-or-later
//
// A RESULT KNOWS WHEN IT IS OUT OF DATE (TODOS F-04).
//
// A buffer, an area generated from lines, a boundary found in linework and a
// contour traced through levelled points are statements about their sources.
// What these cases lock is that such a result says it is out of date the moment
// a source changes — and not a moment before, and not about any other result;
// that an undo which puts the source back makes it current again with nothing
// stored to find; that an erased source leaves it standing on its own and an
// undo gives the source back; that a result reshaped with its sources is still
// current and one reshaped on its own is released; and that all of it goes
// through a save and an open, and through every client alike.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/lineage.hpp"
#include "kentos_cad/core/planar.hpp"
#include "kentos_cad/core/ties.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/domain/surface/contour.hpp"
#include "kentos_cad/io/format.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    io::FileService files{bus};
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        domain::surface::register_surface_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line, Origin origin = Origin::Test)
    {
        auto r = bus.execute_line(line, origin);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    core::EntityId entity(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    core::ResultState state(std::int64_t key) const
    {
        return core::check_result(doc, entity(key)).state;
    }

    /// The state of object `key`'s tie of `kind`; `Current` when it has none.
    core::TieState tie(std::int64_t key, core::TieKind kind) const
    {
        for (const core::Tie& t : core::ties_of(doc, entity(key)))
            if (t.kind == kind) return t.state;
        return core::TieState::Current;
    }

    /// How many followers of `kind` in the drawing are in `state`.
    std::size_t counted(core::TieKind kind, core::TieState state) const
    {
        std::size_t n = 0;
        for (const core::Tie& t : core::every_tie(doc))
            n += t.kind == kind && t.state == state ? 1 : 0;
        return n;
    }

    /// What dimension `key` measures, as it says.
    std::int64_t measured(std::int64_t key) const
    {
        auto def = core::dimension_of(doc.geometry(), doc.entities().slot[entity(key)]);
        REQUIRE(def.ok());
        return def.value().measurement;
    }

    /// The words of every caption on `layer`, sorted: what a sheet reads there.
    std::vector<std::string> captions_on(const std::string& layer) const
    {
        std::vector<std::string> out;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
            if (!doc.alive(e)) continue;
            const core::LayerId l = doc.entities().layer[e];
            if (l >= doc.layers().size() || doc.layers()[l].name != layer) continue;
            const std::uint32_t slot = doc.entities().slot[e];
            if (doc.texts().has(slot)) out.emplace_back(doc.texts().text(slot));
        }
        std::ranges::sort(out);
        return out;
    }

    /// The newest live object's key.
    std::int64_t newest() const
    {
        for (auto e = static_cast<core::EntityId>(doc.entities().size()); e-- > 0;)
            if (doc.alive(e)) return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
        return 0;
    }
};

std::vector<core::EntityKey> keys(std::initializer_list<std::uint64_t> raw)
{
    std::vector<core::EntityKey> out;
    for (const std::uint64_t k : raw)
        out.push_back(static_cast<core::EntityKey>(k));
    return out;
}

struct TempDir
{
    std::filesystem::path path;

    explicit TempDir(const char* name)
        : path(std::filesystem::temp_directory_path() / (std::string("kentoscad-") + name))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::string file(const char* name) const { return (path / name).string(); }
};

/// A 3 × 3 levelling survey sloping from 100 m to 102 m, on `N`, heights in `kot`.
void levelled_points(Rig& r)
{
    r.run("KATMAN ad=N");
    r.run("SÜTUN kimlik=kot tur=uzunluk");
    int key = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            r.run("NOKTA noktalar=" + std::to_string(i * 10) + "," + std::to_string(j * 10));
            ++key;
            r.run("ÖZNİTELİK ad=kot nesne=" + std::to_string(key) +
                  " deger=" + std::to_string(100000 + i * 1000));
        }
}

} // namespace

// ================================================================ revision ===

TEST_CASE("SÜRÜM: bir nesnenin içerik sürümü şekli, yazısı ve değerleriyle değişir; katmanı, "
          "rengi ve kimliğiyle değişmez")
{
    Rig r;
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 20,0 20,10 0,10"); // 1
    const std::uint64_t drawn = r.doc.content_revision(r.entity(1));

    // Not what it is: how it is drawn, and where it is filed.
    r.run("KATMAN ad=BASKA");
    r.run("KATMANAT nesneler=1 katman=BASKA");
    r.run("RENK nesneler=1 renk=#C0392B");
    CHECK_EQ(r.doc.content_revision(r.entity(1)), drawn);

    // A column declared later, empty on every row, changes nothing either.
    r.run("SÜTUN kimlik=not tur=metin");
    CHECK_EQ(r.doc.content_revision(r.entity(1)), drawn);

    // What it is: its value, its shape.
    r.run("ÖZNİTELİK ad=ada nesne=1 deger=101");
    const std::uint64_t valued = r.doc.content_revision(r.entity(1));
    CHECK_NE(valued, drawn);
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    CHECK_NE(r.doc.content_revision(r.entity(1)), valued);

    // And an undo brings the number back with the content.
    r.run("GERİAL");
    CHECK_EQ(r.doc.content_revision(r.entity(1)), valued);
    r.run("GERİAL");
    CHECK_EQ(r.doc.content_revision(r.entity(1)), drawn);

    // Two objects drawn alike, anywhere in the list, say the same.
    r.run("ALAN 0,0 20,0 20,10 0,10"); // 2
    CHECK_EQ(r.doc.content_revision(r.entity(2)), drawn);
}

// ================================================================== buffer ===

TEST_CASE("SONUÇ: kuyusu taşınan tampon güncel değil olur; geri alınınca yine güncel; "
          "BAĞIMLILIK söyler, kabul eder ve kabul geri alınır")
{
    Rig r;
    r.run("NOKTA 0,0");                           // 1, a well
    r.run("NOKTA 100,0");                         // 2, another, far away
    r.run("TAMPON nesneler=1 mesafe=5 katman=K"); // 3
    r.run("TAMPON nesneler=2 mesafe=5 katman=K"); // 4
    REQUIRE(r.doc.lineage().get(r.entity(3)) != nullptr);
    CHECK(r.doc.lineage().get(r.entity(3))->result());
    CHECK_EQ(r.doc.lineage().get(r.entity(3))->sources, keys({1}));
    CHECK_EQ(r.state(3), core::ResultState::Current);
    CHECK_EQ(r.state(4), core::ResultState::Current);

    // The well moves: its buffer says so, at once and by name; the other does not.
    r.said.clear();
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=2,0");
    CHECK_EQ(r.state(3), core::ResultState::Stale);
    CHECK_EQ(core::check_result(r.doc, r.entity(3)).changed, keys({1}));
    CHECK_EQ(r.state(4), core::ResultState::Current);
    CHECK(r.said.find("Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON). Hangileri "
                      "olduğunu görmek için: BAĞIMLILIK") != std::string::npos);

    // Undone, the well is back and the buffer is current again — nothing stored
    // said otherwise; redone, out of date again.
    r.run("GERİAL");
    CHECK_EQ(r.state(3), core::ResultState::Current);
    r.run("YİNELE");
    CHECK_EQ(r.state(3), core::ResultState::Stale);

    // BAĞIMLILIK says which, and why, and the report carries it.
    r.said.clear();
    auto told = r.bus.execute_line("BAĞIMLILIK", Origin::Test);
    REQUIRE(told.ok());
    CHECK(r.said.find("2 sonuç: 1 güncel, 1 güncel değil, 0 kaynaksız.") != std::string::npos);
    CHECK(r.said.find("güncel değil: nesne 3 (TAMPON) — değişen kaynak: nesne 1") !=
          std::string::npos);
    const core::Json& report = told.value().report;
    REQUIRE(report.find("guncel_degil") != nullptr);
    CHECK_EQ(report.find("guncel_degil")->as_int(), 1);
    const core::Json* rows = report.find("sonuclar");
    REQUIRE(rows != nullptr);
    REQUIRE_EQ(rows->as_array().size(), std::size_t{2});
    CHECK_EQ(rows->as_array()[0].find("durum")->as_string(), std::string("guncel_degil"));
    CHECK_EQ(rows->as_array()[0].find("degisen")->as_array()[0].as_int(), 1);

    // Accepted, it is current, as one undo step; undone, out of date again.
    const std::size_t depth = r.undo.undo_depth();
    r.run("BAĞIMLILIK islem=kabul");
    CHECK_EQ(r.state(3), core::ResultState::Current);
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    r.run("GERİAL");
    CHECK_EQ(r.state(3), core::ResultState::Stale);

    // Nothing out of date: nothing to accept, and no undo step for it.
    r.run("BAĞIMLILIK islem=kabul nesneler=3");
    const std::size_t settled = r.undo.undo_depth();
    r.said.clear();
    r.run("BAĞIMLILIK islem=kabul");
    CHECK(r.said.find("Güncel olmayan bir sonuç yok") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), settled);
}

TEST_CASE("SONUÇ: kaynağı silinen tampon kaynaksız kalır; geri alınınca kaynağı da bağı da "
          "döner; çözülen sonuç geçmiş olur")
{
    Rig r;
    r.run("NOKTA 0,0");                           // 1
    r.run("TAMPON nesneler=1 mesafe=5 katman=K"); // 2
    r.said.clear();
    r.run("SİL nesneler=1");
    CHECK_EQ(r.state(2), core::ResultState::Sourceless);
    CHECK_EQ(core::check_result(r.doc, r.entity(2)).gone, keys({1}));
    CHECK(r.said.find("Kaynağı silindiği için 1 sonuç artık kaynaksız (TAMPON)") !=
          std::string::npos);
    r.run("GERİAL");
    CHECK_EQ(r.state(2), core::ResultState::Current);

    // Detached: its history stays, the claim goes — and moving the well later
    // says nothing about it.
    r.run("BAĞIMLILIK islem=coz nesneler=2");
    REQUIRE(r.doc.lineage().get(r.entity(2)) != nullptr);
    CHECK_EQ(r.doc.lineage().get(r.entity(2))->operation, "islem.tampon");
    CHECK_EQ(r.doc.lineage().get(r.entity(2))->sources, keys({1}));
    CHECK_EQ(r.state(2), core::ResultState::History);
    r.said.clear();
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=2,0");
    CHECK(r.said.find("güncel değil") == std::string::npos);
    r.run("GERİAL");
    r.run("GERİAL");
    CHECK_EQ(r.state(2), core::ResultState::Current);

    // An object that is not a result is named as such.
    r.said.clear();
    r.run("BAĞIMLILIK nesneler=1");
    CHECK(r.said.find("Seçilen nesnelerin hiçbiri kaynağına bağlı değil.") != std::string::npos);
}

TEST_CASE("SONUÇ: kaynağıyla birlikte taşınan tampon güncel kalır; kendi başına değiştirilen "
          "kaynağından çözülür")
{
    Rig r;
    r.run("NOKTA 0,0");                           // 1
    r.run("TAMPON nesneler=1 mesafe=5 katman=K"); // 2
    r.said.clear();
    r.run("TAŞI nesneler=1 2 baslangic=0,0 bitis=10,0");
    CHECK_EQ(r.state(2), core::ResultState::Current);
    CHECK(r.said.find("güncel değil") == std::string::npos);
    r.run("GERİAL");
    CHECK_EQ(r.state(2), core::ResultState::Current);

    // Moved on its own, it is no longer the buffer that was computed.
    r.said.clear();
    r.run("TAŞI nesneler=2 baslangic=0,0 bitis=10,0");
    CHECK_EQ(r.state(2), core::ResultState::History);
    CHECK_MESSAGE(r.said.find("1 sonuç (TAMPON) kaynağından ayrı değiştirildiği için "
                              "kaynağından çözüldü") != std::string::npos,
                  r.said);
    r.run("GERİAL");
    CHECK_EQ(r.state(2), core::ResultState::Current);
}

// ================================================================= contour ===

TEST_CASE("SONUÇ: bir noktanın kotu değişince bir çalışmanın bütün eş yükselti eğrileri "
          "güncel değil olur; köken bir kez tutulur, dosyada da bir kez yazılır")
{
    if (!domain::surface::available()) PENDING("KENTOS_WITH_CDT=OFF; EŞYÜKSELTİ sınanamıyor.");
    TempDir tmp("sonuc-eslik");
    Rig r;
    levelled_points(r);
    r.run("EŞYÜKSELTİ aralik=500");
    std::vector<core::EntityId> lines;
    for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
        if (r.doc.alive(e) && r.doc.lineage().get(e) != nullptr) lines.push_back(e);
    REQUIRE(lines.size() >= 3);
    // One run, one origin, however many lines: the nine points once.
    const std::uint32_t shared = r.doc.lineage().origin_of(lines.front());
    for (const core::EntityId e : lines)
        CHECK_EQ(r.doc.lineage().origin_of(e), shared);
    CHECK_EQ(r.doc.lineage().origin(shared).operation, "core.contour");
    CHECK_EQ(r.doc.lineage().origin(shared).sources.size(), std::size_t{9});
    for (const core::EntityId e : lines)
        CHECK_EQ(core::check_result(r.doc, e).state, core::ResultState::Current);

    // A height re-read: every line of the run is out of date, and it is said once.
    r.said.clear();
    r.run("ÖZNİTELİK ad=kot nesne=5 deger=101500");
    for (const core::EntityId e : lines)
        CHECK_EQ(core::check_result(r.doc, e).state, core::ResultState::Stale);
    CHECK(r.said.find(std::to_string(lines.size()) + " sonuç artık güncel değil (EŞYÜKSELTİ)") !=
          std::string::npos);

    // Saved and opened: the evidence travels, the states are the same, the
    // fingerprint is the same — and the run's origin is written once.
    const std::string path = tmp.file("eslik.pcad");
    r.run("FARKLIKAYDET \"" + path + "\"");
    Rig back;
    back.run("AÇ \"" + path + "\"");
    CHECK_EQ(back.doc.content_hash(), r.doc.content_hash());
    for (const core::EntityId e : lines)
        CHECK_EQ(core::check_result(back.doc, e).state, core::ResultState::Stale);
    CHECK(back.said.find("tanımadığı") == std::string::npos);
    const std::uint32_t reread = back.doc.lineage().origin_of(lines.front());
    for (const core::EntityId e : lines)
        CHECK_EQ(back.doc.lineage().origin_of(e), reread);

    // Put back, current again — in the reopened drawing too.
    back.run("ÖZNİTELİK ad=kot nesne=5 deger=101000");
    for (const core::EntityId e : lines)
        CHECK_EQ(core::check_result(back.doc, e).state, core::ResultState::Current);
}

// ================================================= only what is affected ===

TEST_CASE("SONUÇ: bir kaynağın değişmesi yalnız ondan yapılan sonuçları sorar")
{
    Rig r;
    // Forty wells, forty buffers.
    for (int i = 0; i < 40; ++i)
        r.run("NOKTA " + std::to_string(i * 100) + ",0");
    for (int i = 1; i <= 40; ++i)
        r.run("TAMPON nesneler=" + std::to_string(i) + " mesafe=5 katman=K");

    // One well moves: one origin is compared, one buffer is out of date.
    const std::uint64_t before = r.doc.content_hash();
    r.run("TAŞI nesneler=7 baslangic=0,0 bitis=1,0");
    std::size_t stale = 0;
    for (std::int64_t k = 41; k <= 80; ++k)
        stale += r.state(k) == core::ResultState::Stale ? 1 : 0;
    CHECK_EQ(stale, std::size_t{1});
    CHECK_EQ(r.state(47), core::ResultState::Stale);
    CHECK_NE(r.doc.content_hash(), before);

    // The settle's own count: what it asked, not what the drawing holds.
    Transaction tx(r.doc, "sınama");
    core::Point2 at{600'000, 0};
    const std::vector<core::Point2> ring{at};
    const core::RingGeometry::RingInput one{ring, core::RingRole::Open, 0};
    REQUIRE(tx.set_geometry(r.entity(8), std::span<const core::RingGeometry::RingInput>(&one, 1)));
    const Transaction::SettleReport rep = tx.settle_results();
    CHECK_EQ(rep.results_asked, std::size_t{1});
    CHECK_EQ(rep.results_stale, std::size_t{1});
    tx.rollback();

    // A line drawn far from every well asks nothing at all.
    Transaction far(r.doc, "uzak");
    REQUIRE(far.add_polyline(0, std::vector<core::Point2>{{0, 50'000}, {10'000, 50'000}}));
    CHECK_EQ(far.settle_results().results_asked, std::size_t{0});
    far.rollback();
}

// =============================================================== boundary ===

TEST_CASE("SONUÇ: SINIR'ın bulduğu alan, çizgisi taşınınca güncel değil olur")
{
    if (!core::network_available()) PENDING("KENTOS_WITH_CGAL=OFF; SINIR sınanamıyor.");
    Rig r;
    r.run("ÇİZGİ 0,0 20,0");   // 1
    r.run("ÇİZGİ 20,0 20,10"); // 2
    r.run("ÇİZGİ 20,10 0,10"); // 3
    r.run("ÇİZGİ 0,10 0,0");   // 4
    r.run("SINIR nokta=5,5");  // 5
    CHECK(r.doc.lineage().get(r.entity(5))->result());
    CHECK_EQ(r.state(5), core::ResultState::Current);
    r.run("KÖŞETAŞI nesne=2 kose=1 nokta=25,0");
    CHECK_EQ(r.state(5), core::ResultState::Stale);
    CHECK_EQ(core::check_result(r.doc, r.entity(5)).changed, keys({2}));
}

// ============================================================== followers ===

TEST_CASE("BAĞLI YAZI: kilitli katmanda izleyemeyen uzunluk yazısı güncel değil görünür; kilit "
          "açılınca kaynağına yetişir; geri alınınca yine geride kalır")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 40,0 40,30 0,30");          // 1
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU"); // one caption per edge
    const std::vector<std::string> drawn = r.captions_on("OLCU");
    REQUIRE_EQ(drawn.size(), std::size_t{4});
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Current), std::size_t{4});

    // Locked, the two edges that meet the moved corner leave their captions behind.
    r.run("KATMAN ad=OLCU kilitli=evet");
    r.said.clear();
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=48,30");
    CHECK(r.said.find("Katmanın kilidi açılınca kaynağına yetişir.") != std::string::npos);
    CHECK_EQ(r.captions_on("OLCU"), drawn);
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Behind), std::size_t{2});

    // BAĞIMLILIK says so; asked to bring them back, it says why it cannot yet.
    r.said.clear();
    r.run("BAĞIMLILIK");
    CHECK(r.said.find("4 bağlı nesne: 2 güncel, 2 güncel değil, 0 bağı kopuk.") !=
          std::string::npos);
    CHECK(r.said.find("BAĞIMLILIK islem=yenile") != std::string::npos);
    const std::uint64_t behind = r.doc.content_hash();
    r.said.clear();
    r.run("BAĞIMLILIK islem=yenile");
    CHECK(r.said.find("2 bağlı nesne kilitli katmanda; katmanın kilidi açılınca kaynağına "
                      "yetişir.") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), behind);

    // Unlocked: they catch up in the same step, and say their new lengths.
    r.said.clear();
    r.run("KATMAN ad=OLCU kilitli=hayır");
    CHECK(r.said.find("Kilidi açılan 2 bağlı nesne kaynağına yetişti.") != std::string::npos);
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Current), std::size_t{4});
    CHECK_EQ(r.captions_on("OLCU"),
             (std::vector<std::string>{"30,00 m", "31,05 m", "40,00 m", "48,00 m"}));

    // Undone, the lock and the captions go back together.
    r.run("GERİAL");
    CHECK(r.doc.layers()[r.doc.entities().layer[r.entity(2)]].locked);
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Behind), std::size_t{2});
}

TEST_CASE("BAĞLI YAZI: kilitliyken köşe eklenen parselin yazıları kilit açılınca aynı kenara "
          "yetişir; komşu kenara sıçramaz")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 40,0 40,30 0,30"); // 1
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");
    r.run("KATMAN ad=OLCU kilitli=evet");
    // A corner on the bottom edge: every edge after it is numbered one further.
    r.run("KÖŞEEKLE nesne=1 kose=1 nokta=20,-5");
    r.run("KATMAN ad=OLCU kilitli=hayır");
    // The bottom edge's caption went to one of its halves; the other three kept
    // their edges — a tie that had not been renumbered behind the lock would
    // have moved each onto its neighbour: two 20,62, one 30 and one 40.
    CHECK_EQ(r.captions_on("OLCU"),
             (std::vector<std::string>{"20,62 m", "30,00 m", "30,00 m", "40,00 m"}));
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Current), std::size_t{4});
}

TEST_CASE("BAĞLI ÖLÇÜ ve TARAMA: kilitliyken geride kalan kilit açılınca yetişir")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0"); // 1
    r.run("KATMAN ad=OLCU");
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    r.run("KATMAN ad=OLCU kilitli=evet");
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=15,0");
    CHECK_EQ(r.measured(2), 12'000);
    CHECK_EQ(r.tie(2, core::TieKind::Dimension), core::TieState::Behind);
    r.run("KATMAN ad=OLCU kilitli=hayır");
    CHECK_EQ(r.measured(2), 15'000);
    CHECK_EQ(r.tie(2, core::TieKind::Dimension), core::TieState::Current);

    Rig h;
    h.run("KATMAN ad=PARSEL");
    h.run("ALAN 0,0 20,0 20,10 0,10"); // 1
    h.run("KATMAN ad=TARAMA");
    h.run("TARAMA nesneler=1 desen=ANSI31"); // 2
    h.run("KATMAN ad=TARAMA kilitli=evet");
    h.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    CHECK_EQ(h.tie(2, core::TieKind::Hatch), core::TieState::Behind);
    h.said.clear();
    h.run("KATMAN ad=TARAMA kilitli=hayır");
    CHECK(h.said.find("Kilidi açılan 1 bağlı nesne kaynağına yetişti.") != std::string::npos);
    CHECK_EQ(h.tie(2, core::TieKind::Hatch), core::TieState::Current);

    // A locked hatch whose boundary is erased is broken behind the lock too: it
    // stays as it was, and says it follows nothing.
    h.run("KATMAN ad=TARAMA kilitli=evet");
    h.run("SİL nesneler=1");
    CHECK_EQ(h.tie(2, core::TieKind::Hatch), core::TieState::Broken);
    REQUIRE(h.doc.hatch_links().get(h.entity(2)) != nullptr);
    CHECK(h.doc.hatch_links().get(h.entity(2))->front().broken);
    h.run("GERİAL");
    CHECK_EQ(h.tie(2, core::TieKind::Hatch), core::TieState::Current);
    CHECK_FALSE(h.doc.hatch_links().get(h.entity(2))->front().broken);
}

TEST_CASE("BAĞIMLILIK yenile: dışarıda değişmiş bir kaynağa bağlı ölçü kaynağına yetişir; tek "
          "adımda geri alınır; coz bağı çözer")
{
    Rig r;
    r.run("ÇİZGİ 0,0 12,0");                                     // 1
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=12,0 konum=6,-2"); // 2
    // The line changed with nothing to follow it — what a drawing written by a
    // program that knows no ties would leave.
    {
        Transaction tx(r.doc, "dışarıda");
        const std::vector<core::Point2> ring{{0, 0}, {15'000, 0}};
        const core::RingGeometry::RingInput one{ring, core::RingRole::Open, 0};
        REQUIRE(
            tx.set_geometry(r.entity(1), std::span<const core::RingGeometry::RingInput>(&one, 1)));
        (void)tx.release();
    }
    CHECK_EQ(r.tie(2, core::TieKind::Dimension), core::TieState::Behind);
    r.said.clear();
    r.run("BAĞIMLILIK islem=yenile");
    CHECK(r.said.find("1 bağlı nesne kaynağına yetiştirildi.") != std::string::npos);
    CHECK_EQ(r.measured(2), 15'000);
    CHECK_EQ(r.tie(2, core::TieKind::Dimension), core::TieState::Current);
    r.run("GERİAL");
    CHECK_EQ(r.measured(2), 12'000);
    CHECK_EQ(r.tie(2, core::TieKind::Dimension), core::TieState::Behind);

    // Nothing behind: nothing to do, and no undo step for it.
    r.run("YİNELE");
    const std::size_t depth = r.undo.undo_depth();
    r.said.clear();
    r.run("BAĞIMLILIK islem=yenile");
    CHECK(r.said.find("Kaynağının gerisinde kalmış bir bağlı nesne yok.") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), depth);

    // Cut loose: the dimension stays and measures nothing more.
    r.run("BAĞIMLILIK islem=coz nesneler=2");
    CHECK(r.doc.dimension_links().get(r.entity(2)) == nullptr);
    CHECK(core::ties_of(r.doc, r.entity(2)).empty());
    r.run("GERİAL");
    CHECK(r.doc.dimension_links().get(r.entity(2)) != nullptr);
}

TEST_CASE("BAĞ: silme ve geri alma her bağ türünü tutarlı getirir")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("ALAN 0,0 20,0 20,10 0,10");                            // 1
    r.run("UZUNLUKYAZ nesneler=1 katman=OLCU");                   // 2..5
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=20,0 konum=10,-3"); // 6
    r.run("TARAMA nesneler=1 desen=ANSI31");                      // 7
    r.run("TAMPON nesneler=1 mesafe=2 katman=K");                 // 8
    const std::uint64_t whole = r.doc.content_hash();
    r.run("SİL nesneler=1");
    // The captions go with it; the dimension and the hatch stay broken; the
    // buffer stands alone.
    CHECK(r.captions_on("OLCU").empty());
    CHECK_EQ(r.tie(6, core::TieKind::Dimension), core::TieState::Broken);
    CHECK_EQ(r.tie(7, core::TieKind::Hatch), core::TieState::Broken);
    CHECK_EQ(r.state(8), core::ResultState::Sourceless);
    // One undo gives everything back, every tie whole.
    r.run("GERİAL");
    CHECK_EQ(r.doc.content_hash(), whole);
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Current), std::size_t{4});
    CHECK_EQ(r.tie(6, core::TieKind::Dimension), core::TieState::Current);
    CHECK_EQ(r.tie(7, core::TieKind::Hatch), core::TieState::Current);
    CHECK_EQ(r.state(8), core::ResultState::Current);
    // And the next change is followed by all of them again.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    CHECK_EQ(r.counted(core::TieKind::Caption, core::TieState::Current), std::size_t{4});
    CHECK_EQ(r.tie(6, core::TieKind::Dimension), core::TieState::Current);
    CHECK_EQ(r.measured(6), 25'000);
    CHECK_EQ(r.tie(7, core::TieKind::Hatch), core::TieState::Current);
    CHECK_EQ(r.state(8), core::ResultState::Stale);
}

TEST_CASE("BAĞ: döngü kurulamaz — yazı kendisini izleyene, tarama kendisinden dolduruluna "
          "bağlanamaz")
{
    Rig r;
    r.run("ÇİZGİ 0,0 10,0");  // 1
    r.run("METİN 5,2 \"a\""); // 2
    r.run("METİN 5,4 \"b\""); // 3
    r.run("BAĞLA nesneler=2 kaynak=1");
    r.run("BAĞLA nesneler=3 kaynak=2"); // a chain is allowed
    auto cycle = r.bus.execute_line("BAĞLA nesneler=2 kaynak=3", Origin::Test);
    REQUIRE_FALSE(cycle.ok());
    CHECK(cycle.error().message.find("Bağ döngüsü") != std::string::npos);

    // Two hatches, the second filled from the first: the first may not then be
    // filled from the second.
    r.run("ALAN 0,20 10,20 10,30 0,30");     // 4
    r.run("TARAMA nesneler=4 desen=ANSI31"); // 5
    r.run("ALAN 20,20 30,20 30,30 20,30");   // 6
    r.run("TARAMA nesneler=6 desen=ANSI31"); // 7
    Transaction tx(r.doc, "döngü");
    const std::array<core::HatchSource, 1> from_first{
        core::HatchSource{r.doc.key_of(r.entity(5)), false}};
    REQUIRE(tx.set_hatch_links(r.entity(7), from_first));
    const std::array<core::HatchSource, 1> from_second{
        core::HatchSource{r.doc.key_of(r.entity(7)), false}};
    auto refused = tx.set_hatch_links(r.entity(5), from_second);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("Tarama bağ döngüsü") != std::string::npos);
    tx.rollback();
}

TEST_CASE("BAĞIMLILIK: kılavuzdaki örnekler kelimesi kelimesine")
{
    // docs/komutlar/dependency.md prints what the command line says; the page
    // and the program must agree to the character.
    Rig r;
    r.run("NOKTA 485320,4310220");
    r.run("TAMPON nesneler=1 mesafe=5 katman=KORUMA");
    r.said.clear();
    r.run("TAŞI nesneler=1 baslangic=485320,4310220 bitis=485322,4310220");
    CHECK(r.said.find("Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON). Hangileri "
                      "olduğunu görmek için: BAĞIMLILIK\n") != std::string::npos);
    r.said.clear();
    r.run("BAĞIMLILIK");
    CHECK_EQ(r.said, std::string("1 sonuç: 0 güncel, 1 güncel değil, 0 kaynaksız.\n"
                                 "  güncel değil: nesne 2 (TAMPON) — değişen kaynak: nesne 1\n"
                                 "Kaynakların şimdiki hâlini kabul etmek için: BAĞIMLILIK "
                                 "islem=kabul — sonucu kaynağından çözmek için: BAĞIMLILIK "
                                 "islem=coz\n"));

    Rig l;
    for (const char* line :
         {"KATMAN ad=PARSEL", "ALAN 485300,4310200 485340,4310200 485340,4310230 485300,4310230",
          "UZUNLUKYAZ nesneler=1 katman=OLCU", "KATMAN ad=OLCU kilitli=evet",
          "KÖŞETAŞI nesne=1 kose=3 nokta=485348,4310230"})
        l.run(line);
    l.said.clear();
    l.run("BAĞIMLILIK");
    CHECK_EQ(l.said, std::string("4 bağlı nesne: 2 güncel, 2 güncel değil, 0 bağı kopuk.\n"
                                 "  güncel değil: nesne 3 (bağlı yazı) — kaynağı: nesne 1\n"
                                 "  güncel değil: nesne 4 (bağlı yazı) — kaynağı: nesne 1\n"
                                 "Bağlı nesneleri kaynağına yetiştirmek için: BAĞIMLILIK "
                                 "islem=yenile\n"));
    l.said.clear();
    l.run("KATMAN ad=OLCU kilitli=hayır");
    CHECK(l.said.find("Kilidi açılan 2 bağlı nesne kaynağına yetişti.\n") != std::string::npos);
}

// ============================================================ the bytes ===

TEST_CASE("SONUÇ: kökenin geri alma baytları sürümlerini taşır; geçmiş kökeni eskisi gibi "
          "kodlanır")
{
    core::Lineage history{"core.copy", keys({3, 7}), {}};
    const std::vector<std::uint8_t> old_bytes = core::encode_lineage(&history);
    REQUIRE_FALSE(old_bytes.empty());
    CHECK_EQ(old_bytes[0], 1); // the layout every drawing before results was written in
    auto back = core::decode_lineage(old_bytes);
    REQUIRE(back.ok());
    CHECK_EQ(back.value(), history);

    core::Lineage result{"islem.tampon", keys({3, 7}), {0x1234567890ABCDEFull, 42}};
    const std::vector<std::uint8_t> bytes = core::encode_lineage(&result);
    CHECK_EQ(bytes[0], 2);
    auto again = core::decode_lineage(bytes);
    REQUIRE(again.ok());
    CHECK_EQ(again.value(), result);

    // Cut short, or with no source, it is refused rather than read.
    std::vector<std::uint8_t> cut(bytes.begin(), bytes.end() - 1);
    CHECK_FALSE(core::decode_lineage(cut).ok());
}

// ================================================================== seeds ===

TEST_CASE("SONUÇ: sonuç kökeni tohumları korpusta; bozuk satırlar uyarıyla atlanır, dışarı "
          "taşan köken adıyla reddedilir")
{
    // CLAUDE.md 6.7, for the three result blocks (core/lineage.hpp, F-04):
    // written under KENTOS_TOHUM_UPDATE — two wells and their buffers, one
    // well moved after — and read back on every build.
    namespace fs          = std::filesystem;
    const fs::path corpus = fs::path(KENTOS_FUZZ_DIR) / "tohum" / "proje";
    const fs::path good   = corpus / "16-sonuc-kokenleri.pcad";
    const fs::path row    = corpus / "17-sonuc-satiri-bozuk.pcad";
    const fs::path run    = corpus / "18-sonuc-kaynagi-tasan.pcad";
    if (std::getenv("KENTOS_TOHUM_UPDATE") != nullptr) {
        Rig w;
        for (const char* line :
             {"NOKTA 0,0", "NOKTA 100,0", "TAMPON nesneler=1 2 mesafe=5 birlestir=hayir katman=K",
              "TAŞI nesneler=1 baslangic=0,0 bitis=2,0"})
            w.run(line);
        w.run("FARKLIKAYDET \"" + good.string() + "\"");

        std::ifstream in(good, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        const auto u32 = [&bytes](std::size_t at) {
            std::uint32_t v = 0;
            std::memcpy(&v, bytes.data() + at, 4);
            return v;
        };
        const auto u64 = [&bytes](std::size_t at) {
            std::uint64_t v = 0;
            std::memcpy(&v, bytes.data() + at, 8);
            return v;
        };
        std::size_t origins = 0;
        std::size_t rows    = 0;
        for (std::uint32_t b = 0; b < u32(20); ++b) {
            const std::size_t entry = u64(24) + std::size_t{b} * 32;
            if (u32(entry) == io::kBlkResultOrigins) origins = u64(entry + 8);
            if (u32(entry) == io::kBlkResultRows) rows = u64(entry + 8);
        }
        REQUIRE(origins != 0);
        REQUIRE(rows != 0);
        // A row naming an object the file does not hold, and one naming an
        // origin past the block.
        std::string broken_rows    = bytes;
        const std::uint64_t nobody = 0xFFFF'FFFFu;
        std::memcpy(broken_rows.data() + rows, &nobody, 8);
        const std::uint32_t nowhere = 0x7FFF'FFFFu;
        std::memcpy(broken_rows.data() + rows + 16 + 8, &nowhere, 4);
        std::ofstream(row, std::ios::binary)
            .write(broken_rows.data(), static_cast<std::streamsize>(broken_rows.size()));
        // An origin whose sources run past the sources block.
        std::string broken_run    = bytes;
        const std::uint32_t count = 0x00FF'FFFFu;
        std::memcpy(broken_run.data() + origins + 8, &count, 4);
        std::ofstream(run, std::ios::binary)
            .write(broken_run.data(), static_cast<std::streamsize>(broken_run.size()));
    }
    if (!fs::exists(good) || !fs::exists(row) || !fs::exists(run))
        PENDING("Sonuç kökeni tohumları yok; KENTOS_TOHUM_UPDATE=1 ile yazılır.");

    Rig a;
    a.run("AÇ \"" + good.string() + "\"");
    CHECK(a.said.find("tanımadığı") == std::string::npos);
    CHECK_EQ(a.state(3) == core::ResultState::Stale || a.state(4) == core::ResultState::Stale,
             true);
    std::size_t stale = 0;
    for (const std::int64_t k : {3, 4})
        stale += a.state(k) == core::ResultState::Stale ? 1 : 0;
    CHECK_EQ(stale, std::size_t{1});

    Rig b;
    b.run("AÇ \"" + row.string() + "\"");
    CHECK(b.said.find("var olmayan bir nesneye ya da kökene") != std::string::npos);

    Rig c;
    auto refused = c.bus.execute_line("AÇ \"" + run.string() + "\"", Origin::Test);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().message.find("sonuç kökeninin kaynakları") != std::string::npos);
}

// ================================================================== proof ===

TEST_CASE("BAĞIMLILIK KANIT: arayüz, komut satırı, betik ve oynatma aynı belgeyi ve günlüğü "
          "bırakır")
{
    const auto journal_of = [](const Journal& j) {
        std::string out;
        for (const auto& e : j.entries())
            out += e.command_id + ' ' + e.args.to_json().dump() + '\n';
        return out;
    };
    const auto prepared = [](Rig& r) {
        r.run("NOKTA 0,0");
        r.run("TAMPON nesneler=1 mesafe=5 katman=K");
        r.run("TAŞI nesneler=1 baslangic=0,0 bitis=2,0");
    };
    Rig gui;
    prepared(gui);
    gui.run("BAĞIMLILIK islem=kabul", Origin::Gui);
    Rig cli;
    prepared(cli);
    cli.run("BAĞIMLILIK islem=kabul", Origin::CommandLine);
    Rig scr;
    prepared(scr);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto done = runner.run_text(
            R"({"ad":"B","komutlar":[{"cmd":"core.dependency","args":{"islem":"kabul"}}]})");
        REQUIRE_MESSAGE(done.ok(), (done.ok() ? std::string() : done.error().message));
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(journal_of(gui.journal), journal_of(cli.journal));
    CHECK_EQ(journal_of(cli.journal), journal_of(scr.journal));
    CHECK_EQ(cli.state(2), core::ResultState::Current);
    // The journal names what was accepted, so a replay accepts the same.
    CHECK(journal_of(cli.journal).find("core.dependency {\"islem\":\"kabul\",\"nesneler\":[2]}") !=
          std::string::npos);

    Rig replay;
    for (const auto& e : cli.journal.entries())
        REQUIRE(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
}
