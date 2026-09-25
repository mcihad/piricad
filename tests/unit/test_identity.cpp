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
#include "kentos_cad/io/service.hpp"

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
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
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
