// SPDX-License-Identifier: GPL-3.0-or-later
//
// ONE OBJECT, ONE IDENTITY (TODOS F-02).
//
// A parcel is one object wherever it is looked at: the map, its table row, the
// selection, the label that names it and the dimension that measures it. What
// these cases lock is that a grip dragged across it changes its SHAPE and
// nothing else — its key, its cells, its place in the selection and its ties —
// through a save and a reopen too; that a value undone goes back to the column
// it came from even after another column was dropped; and that a property of
// something the drawing may not edit — a locked parcel, a member of an
// external reference — is refused by name rather than changed.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <filesystem>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::Point2;

namespace {

namespace fs = std::filesystem;

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
        domain::cadastre::register_cadastre_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    /// The origin of object `key`: its operation and its sources' keys, or
    /// an empty operation when it has none.
    core::Lineage origin(std::int64_t key) const
    {
        const core::Lineage* got = doc.lineage().get(entity(key));
        return got == nullptr ? core::Lineage{} : *got;
    }

    /// The newest live object's key.
    std::int64_t newest() const
    {
        for (auto e = static_cast<core::EntityId>(doc.entities().size()); e-- > 0;)
            if (doc.alive(e)) return static_cast<std::int64_t>(core::raw(doc.key_of(e)));
        return 0;
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        return r.ok() ? std::string() : r.error().message;
    }

    core::EntityId entity(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    /// Object `key`'s cell in `column`, as the table shows it; `-` when empty.
    std::string cell(const char* column, std::int64_t key) const
    {
        const auto v = doc.attribute(doc.attributes().find(column), entity(key));
        if (!v.ok() || !v.value().present) return "-";
        return core::attr_display(v.value(), core::DecimalMark::Point);
    }

    std::string caption(std::int64_t key) const
    {
        return std::string(doc.texts().text(doc.entities().slot[entity(key)]));
    }
};

/// A scratch folder, gone when the case ends.
class TempDir
{
public:
    explicit TempDir(const char* tag)
    {
        path_ = fs::temp_directory_path() / (std::string("kentoscad-kimlik-") + tag);
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

    std::string file(const std::string& name) const { return (path_ / name).string(); }

private:
    fs::path path_;
};

} // namespace

TEST_CASE("KİMLİK: tutamakla değişen parselin satırı, seçimi, etiketi ve ölçüsü korunur; "
          "kaydedilip açılınca da")
{
    TempDir tmp("tutamak");
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 20,0 20,10 0,10");   // 1
    r.run("ALAN 30,0 40,0 40,10 30,10"); // 2, a neighbour with a value of its own
    r.run("ÖZNİTELİK ada 1 101");
    r.run("ÖZNİTELİK ada 2 202");
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\\n{#alan} m²\"");   // 3, 4
    r.run("ÖLÇÜ tur=hizali birinci=0,0 ikinci=20,0 konum=10,-3"); // 5, tied to 1
    r.run("SEÇ nesneler=1");
    const core::EntityId before = r.entity(1);

    // The grip: the parcel's corner pulled out.
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=25,0");
    const core::EntityId e = r.entity(1);
    CHECK_EQ(e, before);                                               // the same object
    CHECK_NE(r.doc.entities().slot[e], static_cast<std::uint32_t>(e)); // on a new slot
    CHECK_EQ(r.cell("ada", 1), "101");                                 // its own row
    CHECK_EQ(r.cell("ada", 2), "202");                                 // and nobody else's
    CHECK(r.bus.selection().contains(r.doc.key_of(e)));                // still picked
    // Its label says its new area and its dimension measures its new side.
    std::vector<std::string> labels;
    for (const std::int64_t k : {3, 4})
        labels.push_back(r.caption(k));
    CHECK(std::ranges::find(labels, std::string("101\n225,00 m²")) != labels.end());
    auto measured = core::dimension_of(r.doc.geometry(), r.doc.entities().slot[r.entity(5)]);
    REQUIRE(measured.ok());
    CHECK_EQ(measured.value().measurement, 25'000);

    // Saved and opened again: the same key, the same cell, the same ties.
    const std::string path = tmp.file("pafta.pcad");
    r.run("FARKLIKAYDET \"" + path + "\"");
    Rig back;
    back.run("AÇ \"" + path + "\"");
    CHECK_EQ(back.cell("ada", 1), "101");
    CHECK_EQ(back.cell("ada", 2), "202");
    CHECK(back.doc.attachments().get(back.entity(3)) != nullptr);
    CHECK(back.doc.dimension_links().get(back.entity(5)) != nullptr);
    // And the ties still work after the reopen: the next drag moves them all.
    back.run("KÖŞETAŞI nesne=1 kose=2 nokta=30,0");
    CHECK_EQ(back.cell("ada", 1), "101");
    auto again = core::dimension_of(back.doc.geometry(), back.doc.entities().slot[back.entity(5)]);
    REQUIRE(again.ok());
    CHECK_EQ(again.value().measurement, 30'000);
}

TEST_CASE("KİMLİK: sütun silinse de GERİAL değeri kendi sütununa koyar; silinen sütunun değeri "
          "hiçbir yere yazılmaz")
{
    // THE DEFECT: an undo record named its column by NUMBER, and dropping a
    // column renumbers the ones after it — so undoing an edit of `c` after `b`
    // was dropped wrote the old value into `d`.
    Rig r;
    r.run("ALAN 0,0 10,0 10,10 0,10");
    for (const char* c : {"a", "b", "c", "d"})
        r.run(std::string("SÜTUN kimlik=") + c + " tur=metin");
    r.run("ÖZNİTELİK c 1 X");
    r.run("ÖZNİTELİK c 1 Y");
    r.run("ÖZNİTELİK b 1 Z");
    r.run("SÜTUN kimlik=b sil=evet");

    r.run("GERİAL"); // the edit of `b`: its column is gone, nothing to put back
    CHECK_EQ(r.cell("a", 1), "-");
    CHECK_EQ(r.cell("c", 1), "Y");
    CHECK_EQ(r.cell("d", 1), "-");
    r.run("GERİAL"); // Y back to X, in `c`
    CHECK_EQ(r.cell("c", 1), "X");
    CHECK_EQ(r.cell("d", 1), "-");
    r.run("GERİAL");
    CHECK_EQ(r.cell("c", 1), "-");
    CHECK_EQ(r.cell("d", 1), "-");
    r.run("YİNELE");
    CHECK_EQ(r.cell("c", 1), "X");
}

TEST_CASE("KİMLİK: kilitli katmandaki nesnenin değeri, katmanı, stili ve yazısı değişmez; kilit "
          "açılınca değişir")
{
    Rig r;
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 10,0 10,10 0,10");   // 1
    r.run("METİN 2,2 \"Ada 101\" 1000"); // 2
    r.run("KATMAN ad=YOL");
    r.run("ÇİZGİ 0,20 10,20"); // 3, a style to copy
    r.run("KATMAN ad=PARSEL kilitli=evet");

    const auto locked = [](const std::string& why) {
        return why.find("'PARSEL' katmanı kilitli") != std::string::npos;
    };
    CHECK(locked(r.refused("ÖZNİTELİK ada 1 101")));
    CHECK(locked(r.refused("KATMANAT nesneler=1 katman=YOL")));
    CHECK(locked(r.refused("STİLKOPYALA kaynak=3 nesneler=1")));
    CHECK(locked(r.refused("YAZIDÜZENLE nesneler=2 yazi=\"Ada 102\"")));
    CHECK_EQ(r.cell("ada", 1), "-");
    CHECK_EQ(r.caption(2), std::string("Ada 101"));
    CHECK_EQ(r.doc.layer_table().at(r.doc.entities().layer[r.entity(1)])->name,
             std::string("PARSEL"));

    r.run("KATMAN ad=PARSEL kilitli=hayır");
    r.run("ÖZNİTELİK ada 1 101");
    r.run("YAZIDÜZENLE nesneler=2 yazi=\"Ada 102\"");
    CHECK_EQ(r.cell("ada", 1), "101");
    CHECK_EQ(r.caption(2), std::string("Ada 102"));
}

TEST_CASE("KİMLİK: dış referansın parçasına değer yazılmaz; ret dosyayı ve bağlamayı söyler")
{
    TempDir tmp("disref");
    const std::string source = tmp.file("altlik.pcad");
    {
        Rig src;
        src.run("KATMAN ad=YOL");
        src.run("SÜTUN kimlik=ad tur=metin");
        src.run("ÇİZGİ 0,0 100,0");
        src.run("ÖZNİTELİK ad 1 \"Cumhuriyet Caddesi\"");
        src.run("FARKLIKAYDET \"" + source + "\"");
    }
    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\"");
    core::EntityId member = core::kNoEntity;
    for (core::EntityId e = 0; e < host.doc.entities().size(); ++e)
        if (host.doc.alive(e) && (host.doc.entities().flags[e] & core::FlagInBlock) != 0 &&
            host.doc.entities().kind[e] == core::kPolylineKind)
            member = e;
    REQUIRE(member != core::kNoEntity);
    const std::string key = std::to_string(core::raw(host.doc.key_of(member)));

    const std::string why = host.refused("ÖZNİTELİK ad " + key + " \"Atatürk Caddesi\"");
    CHECK(why.find("'altlik' dış referansının parçası ('altlik.pcad')") != std::string::npos);
    CHECK(why.find("DIŞREFERANS islem=bagla ad=altlik") != std::string::npos);
    const auto v = host.doc.attribute(host.doc.attributes().find("ad"), member);
    REQUIRE(v.ok());
    CHECK_EQ(v.value().text, std::string("Cumhuriyet Caddesi"));
}

// ------------------------------------------------------------ lineage ----

namespace {

std::vector<core::EntityKey> keys(std::initializer_list<std::uint64_t> raw)
{
    std::vector<core::EntityKey> out;
    for (const std::uint64_t k : raw)
        out.push_back(static_cast<core::EntityKey>(k));
    return out;
}

} // namespace

TEST_CASE("KÖKEN: analiz çıktısı kökenini bilir — tampon her kuyusunu, birleşik tampon hepsini")
{
    Rig r;
    r.run("NOKTA 0,0");                                                  // 1
    r.run("NOKTA 50,0");                                                 // 2
    r.run("TAMPON nesneler=1 2 mesafe=5 birlestir=hayir katman=KORUMA"); // 3, 4
    const core::Lineage a = r.origin(3);
    const core::Lineage b = r.origin(4);
    CHECK_EQ(a.operation, "islem.tampon");
    CHECK_EQ(b.operation, "islem.tampon");
    CHECK((a.sources == keys({1}) || a.sources == keys({2})));
    CHECK((b.sources == keys({1}) || b.sources == keys({2})));
    CHECK_NE(a.sources, b.sources);

    r.run("TAMPON nesneler=1 2 mesafe=40 katman=BIRLESIK"); // one face round both
    CHECK_EQ(r.origin(r.newest()).sources, keys({1, 2}));

    // Undone, the result goes and its history with it; redone, both come back.
    r.run("GERİAL");
    CHECK(r.doc.lineage().get(r.entity(5)) == nullptr);
    r.run("YİNELE");
    CHECK_EQ(r.origin(5).sources, keys({1, 2}));
}

TEST_CASE(
    "KÖKEN: ifraz parçaları silinen parseli, tevhit parseli ikisini bilir; NESNEBİLGİ söyler; "
    "kaydedilip açılınca da")
{
    TempDir tmp("koken");
    Rig r;
    r.run("ALAN 0,0 20,0 20,10 0,10");              // 1
    r.run("İFRAZ nesneler=1 noktalar=10,-5 10,15"); // 2, 3; 1 erased
    CHECK_FALSE(r.doc.alive(r.entity(1)));
    CHECK_EQ(r.origin(2).operation, "core.split_parcel");
    CHECK_EQ(r.origin(2).sources, keys({1}));
    CHECK_EQ(r.origin(3).sources, keys({1}));
    r.run("TEVHİT nesneler=2 3"); // 4
    CHECK_EQ(r.origin(4).operation, "core.merge");
    CHECK_EQ(r.origin(4).sources, keys({2, 3}));

    // NESNEBİLGİ says it by the name a user types, and that the parents are gone.
    auto told = r.bus.execute_line("NESNEBİLGİ nesneler=4", Origin::Test);
    REQUIRE(told.ok());
    CHECK(r.said.find("kökeni: TEVHİT (kaynak: nesne 2 (artık çizimde değil), 3 (artık çizimde "
                      "değil))") != std::string::npos);
    const core::Json* rows = told.value().report.find("nesneler");
    REQUIRE(rows != nullptr);
    const core::Json* origin = rows->as_array().front().find("koken");
    REQUIRE(origin != nullptr);
    CHECK_EQ(origin->find("islem")->as_string(), std::string("core.merge"));
    CHECK(origin->find("kaynaklar")->as_array().front().find("silinmis")->as_bool());

    // And the parent is asked what was made from it — though it is gone.
    r.said.clear();
    r.run("NESNEBİLGİ nesneler=4");
    CHECK(r.said.find("bundan türetilen") == std::string::npos); // nothing made from 4

    // The history is kept with the drawing.
    const std::string path = tmp.file("ifraz.pcad");
    r.run("FARKLIKAYDET \"" + path + "\"");
    Rig back;
    back.run("AÇ \"" + path + "\"");
    CHECK_EQ(back.origin(4).operation, "core.merge");
    CHECK_EQ(back.origin(4).sources, keys({2, 3}));
    CHECK_EQ(back.origin(2).sources, keys({1})); // a dead row's history too
    CHECK_EQ(back.doc.content_hash(), r.doc.content_hash());
}

TEST_CASE("KÖKEN: BUDA'nın ve BÖL'ün yeni parçası kaynağını bilir; yerinde kalan parça kendisidir")
{
    Rig r;
    r.run("ÇİZGİ 0,0 20,0");   // 1
    r.run("ÇİZGİ 10,-5 10,5"); // 2, the cutting edge
    r.run("BÖL nesne=1 nokta=5,0");
    // The first piece stays in object 1 — the same object, no origin — and the
    // second is new, made from it.
    CHECK(r.doc.lineage().get(r.entity(1)) == nullptr);
    CHECK_EQ(r.origin(3).operation, "core.split");
    CHECK_EQ(r.origin(3).sources, keys({1}));
}

// ----------------------------------------------------------- data sources ----

namespace {

/// A GeoPackage of `count` parcels on `PARSEL`, each with its `ada`, at `path`.
void write_parcels(const std::string& path, int count)
{
    Rig src;
    src.run("AYAR core.crs.id EPSG:5254"); // a system GeoPackage can name
    src.run("KATMAN ad=PARSEL");
    src.run("SÜTUN kimlik=ada tur=tam_sayi");
    for (int i = 0; i < count; ++i) {
        const int x = i * 30;
        src.run("ALAN " + std::to_string(x) + ",0 " + std::to_string(x + 20) + ",0 " +
                std::to_string(x + 20) + ",10 " + std::to_string(x) + ",10");
        src.run("ÖZNİTELİK ada " + std::to_string(i + 1) + " " + std::to_string(101 * (i + 1)));
    }
    std::error_code ec;
    fs::remove(path, ec);
    src.run("DIŞAAKTAR \"" + path + "\"");
}

/// The live members of external reference `name`.
std::vector<core::EntityId> members_of(const core::Document& doc, const std::string& name)
{
    std::vector<core::EntityId> out;
    const core::BlockId b = doc.blocks().find(name);
    if (b == core::kNoBlock) return out;
    for (const core::EntityKey k : doc.blocks().at(b).members)
        if (const core::EntityId m = doc.slot_of(k); m != core::kNoEntity && doc.alive(m))
            out.push_back(m);
    return out;
}

} // namespace

TEST_CASE("VERİ KAYNAĞI: bir CBS dosyası içe alınınca kopyalanır; bağlanınca salt okunur kalır ve "
          "her açılışta dosyasından okunur")
{
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF; CBS dosyası okunamıyor.");
    TempDir tmp("cbs");
    const std::string gpkg = tmp.file("parseller.gpkg");
    write_parcels(gpkg, 2);

    // İÇEAKTAR: the drawing's own objects, editable like anything drawn here.
    {
        Rig copy;
        copy.run("AYAR core.crs.id EPSG:5254");
        copy.run("İÇEAKTAR \"" + gpkg + "\"");
        std::size_t own = 0;
        for (core::EntityId e = 0; e < copy.doc.entities().size(); ++e)
            if (copy.doc.alive(e) && (copy.doc.entities().flags[e] & core::FlagInBlock) == 0 &&
                copy.doc.entities().kind[e] != core::kBlockReferenceKind)
                ++own;
        CHECK_EQ(own, std::size_t{2});
        CHECK(copy.doc.blocks().find("parseller") == core::kNoBlock);
    }

    // DIŞREFERANS: a live link — the file's parcels drawn in place, their
    // values readable, nothing of theirs editable here.
    Rig host;
    host.run("AYAR core.crs.id EPSG:5254"); // the file's own: nothing to carry across
    host.run("DIŞREFERANS dosya=\"" + gpkg + "\"");
    const core::BlockId linked = host.doc.blocks().find("parseller");
    REQUIRE(linked != core::kNoBlock);
    CHECK((host.doc.blocks().at(linked).flags & core::kBlockExternal) != 0);
    std::vector<core::EntityId> parcels = members_of(host.doc, "parseller");
    REQUIRE_EQ(parcels.size(), std::size_t{2});
    CHECK(host.doc.find_layer("parseller|PARSEL") != core::kNoLayer);
    std::vector<std::int64_t> adas;
    for (const core::EntityId m : parcels) {
        const auto v = host.doc.attribute(host.doc.attributes().find("ada"), m);
        REQUIRE(v.ok());
        adas.push_back(v.value().number);
    }
    std::ranges::sort(adas);
    CHECK_EQ(adas, (std::vector<std::int64_t>{101, 202}));
    const std::string member = std::to_string(core::raw(host.doc.key_of(parcels.front())));
    CHECK(host.refused("ÖZNİTELİK ada " + member + " 999")
              .find("'parseller' dış referansının "
                    "parçası ('parseller.gpkg')") != std::string::npos);

    // The file changes; the link sees it on reload, and again on the next open.
    write_parcels(gpkg, 3);
    host.run("DIŞREFERANS islem=yenile ad=parseller");
    CHECK_EQ(members_of(host.doc, "parseller").size(), std::size_t{3});
    const std::string project = tmp.file("pafta.pcad");
    host.run("FARKLIKAYDET \"" + project + "\"");
    write_parcels(gpkg, 4);
    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK_EQ(members_of(back.doc, "parseller").size(), std::size_t{4});
}

// ------------------------------------------------------------- local copy ----

namespace {

/// The error a line is refused with.
core::Error refusal(Rig& r, const std::string& line)
{
    auto got = r.bus.execute_line(line, Origin::Test);
    REQUIRE_FALSE(got.ok());
    return got.error();
}

/// The live objects on layer `name` that are this drawing's own (no member).
std::vector<core::EntityId> own_on(const core::Document& doc, const std::string& name)
{
    std::vector<core::EntityId> out;
    const core::LayerId layer = doc.find_layer(name);
    for (core::EntityId e = 0; e < doc.entities().size(); ++e)
        if (doc.alive(e) && doc.entities().standalone(e) && doc.entities().layer[e] == layer)
            out.push_back(e);
    return out;
}

} // namespace

TEST_CASE("YERELKOPYA: bağlı dosyanın nesnesini düzenleme isteği yerel kopyayı önerir; kopya bu "
          "çizimin, bağlantı yerinde kalır")
{
    TempDir tmp("yerel");
    const std::string source = tmp.file("altlik.pcad");
    {
        Rig src;
        src.run("KATMAN ad=PARSEL");
        src.run("SÜTUN kimlik=ada tur=tam_sayi");
        src.run("ALAN 0,0 20,0 20,10 0,10"); // 1
        src.run("ÖZNİTELİK ada 1 101");
        src.run("KATMAN ad=YOL");
        src.run("ÇİZGİ 0,20 100,20"); // 2
        src.run("FARKLIKAYDET \"" + source + "\"");
    }
    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\" nokta=1000,0");
    const std::vector<core::EntityId> members = members_of(host.doc, "altlik");
    REQUIRE_EQ(members.size(), std::size_t{2});
    core::EntityId parcel = core::kNoEntity;
    for (const core::EntityId m : members)
        if (host.doc.entities().kind[m] == core::kPolylineKind &&
            host.doc.attribute(host.doc.attributes().find("ada"), m).value().present)
            parcel = m;
    REQUIRE(parcel != core::kNoEntity);
    core::EntityId reference = core::kNoEntity;
    for (core::EntityId e = 0; e < host.doc.entities().size(); ++e)
        if (host.doc.entities().standalone(e) &&
            host.doc.entities().kind[e] == core::kBlockReferenceKind)
            reference = e;
    REQUIRE(reference != core::kNoEntity);
    const std::string member_key = std::to_string(core::raw(host.doc.key_of(parcel)));
    const std::string ref_key    = std::to_string(core::raw(host.doc.key_of(reference)));

    // EVERY REFUSAL NAMES THE WAY OUT, as a command line any client can run.
    CHECK_EQ(refusal(host, "ÖZNİTELİK ada " + member_key + " 102").remedy,
             "YERELKOPYA nesneler=" + member_key);
    CHECK_EQ(refusal(host, "PATLAT nesne=" + ref_key).remedy, "YERELKOPYA nesneler=" + ref_key);
    CHECK_EQ(refusal(host, "BLOKDÜZENLE nesne=" + ref_key).remedy,
             "YERELKOPYA nesneler=" + ref_key);

    // The way out taken: both objects, where the reference draws them, on the
    // layers their file calls them by, with their values — and the link intact.
    const std::size_t depth = host.undo.undo_depth();
    host.run("YERELKOPYA nesneler=" + ref_key);
    CHECK_EQ(host.undo.undo_depth(), depth + 1);
    const std::vector<core::EntityId> parcels = own_on(host.doc, "PARSEL");
    const std::vector<core::EntityId> roads   = own_on(host.doc, "YOL");
    REQUIRE_EQ(parcels.size(), std::size_t{1});
    REQUIRE_EQ(roads.size(), std::size_t{1});
    const core::EntityId copy = parcels.front();
    CHECK_EQ(host.doc.entities().box_of(copy).min_x, 1'000'000); // placed as the reference draws it
    CHECK_EQ(host.doc.attribute(host.doc.attributes().find("ada"), copy).value().number, 101);
    const core::Lineage* origin = host.doc.lineage().get(copy);
    REQUIRE(origin != nullptr);
    CHECK_EQ(origin->operation, "core.local_copy");
    CHECK_EQ(origin->sources, std::vector<core::EntityKey>{host.doc.key_of(reference)});
    CHECK_EQ(members_of(host.doc, "altlik").size(), std::size_t{2}); // the link is as it was

    // And the copy is this drawing's own: its value and its corner can change.
    const std::string copy_key = std::to_string(core::raw(host.doc.key_of(copy)));
    host.run("ÖZNİTELİK ada " + copy_key + " 102");
    host.run("KÖŞETAŞI nesne=" + copy_key + " kose=2 nokta=1025,0");
    CHECK_EQ(host.doc.attribute(host.doc.attributes().find("ada"), copy).value().number, 102);

    // One object only, from its member; or only what a window touches.
    host.run("YERELKOPYA nesneler=" + member_key + " katman=TASLAK");
    CHECK_EQ(own_on(host.doc, "TASLAK").size(), std::size_t{1});
    host.run("YERELKOPYA nesneler=" + ref_key + " katman=PENCERE pencere=1050,15 1060,25");
    CHECK_EQ(own_on(host.doc, "PENCERE").size(), std::size_t{1}); // the road alone

    // By the name it was linked under, which is what a script knows it by.
    host.run("YERELKOPYA ad=altlik katman=ADIYLA");
    CHECK_EQ(own_on(host.doc, "ADIYLA").size(), std::size_t{2});
    CHECK(refusal(host, "YERELKOPYA ad=yok").message.find("'yok' adında bir dış referans yok") !=
          std::string::npos);

    // What is not a linked file's is refused by name, pointing at KOPYALA.
    CHECK(refusal(host, "YERELKOPYA nesneler=" + copy_key).message.find("KOPYALA") !=
          std::string::npos);
}

TEST_CASE("YERELKOPYA KANIT: arayüz, komut satırı, betik ve oynatma aynı belgeyi ve günlüğü "
          "bırakır; iptal hiçbir şey bırakmaz")
{
    TempDir tmp("yerel-kanit");
    const std::string source = tmp.file("altlik.pcad");
    {
        Rig src;
        src.run("KATMAN ad=PARSEL");
        src.run("ALAN 0,0 20,0 20,10 0,10");
        src.run("FARKLIKAYDET \"" + source + "\"");
    }
    const std::string linked = "DIŞREFERANS dosya=\"" + source + "\"";
    const auto journal_of    = [](const Journal& j) {
        std::string out;
        for (const auto& e : j.entries())
            out += e.command_id + ' ' + e.args.to_json().dump() + '\n';
        return out;
    };
    const auto reference_of = [](const Rig& r) {
        for (core::EntityId e = 0; e < r.doc.entities().size(); ++e)
            if (r.doc.entities().standalone(e) &&
                r.doc.entities().kind[e] == core::kBlockReferenceKind)
                return static_cast<std::int64_t>(core::raw(r.doc.key_of(e)));
        return std::int64_t{0};
    };

    Rig gui;
    gui.run(linked);
    const std::int64_t ref = reference_of(gui);
    REQUIRE(ref != 0);
    {
        auto started = gui.bus.begin_interactive("YERELKOPYA", Origin::Gui);
        REQUIRE(started.ok());
        CHECK(started.value()->supply(Value::ids({ref})).ok());
        auto done = gui.bus.finish(*started.value());
        REQUIRE_MESSAGE(done.ok(), (done.ok() ? std::string() : done.error().message));
    }
    Rig cli;
    cli.run(linked);
    REQUIRE(cli.bus.execute_line("YERELKOPYA nesneler=" + std::to_string(ref), Origin::CommandLine)
                .ok());
    Rig scr;
    scr.run(linked);
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(
            R"({"ad":"Y","komutlar":[{"cmd":"core.local_copy","args":{"nesneler":[)" +
            std::to_string(ref) + "]}}]}");
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error().message));
    }
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(journal_of(gui.journal), journal_of(cli.journal));
    CHECK_EQ(journal_of(cli.journal), journal_of(scr.journal));

    // The journal replays to the same drawing.
    Rig replay;
    for (const auto& e : cli.journal.entries())
        REQUIRE(replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch}).ok());
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());

    // Asked and not answered, it leaves nothing — not even an undo step.
    Rig asked;
    asked.run(linked);
    const std::uint64_t before = asked.doc.content_hash();
    const std::size_t depth    = asked.undo.undo_depth();
    {
        auto started = asked.bus.begin_interactive("YERELKOPYA", Origin::Gui);
        REQUIRE(started.ok());
        started.value()->cancel();
        (void)asked.bus.finish(*started.value());
    }
    CHECK_EQ(asked.doc.content_hash(), before);
    CHECK_EQ(asked.undo.undo_depth(), depth);
}
