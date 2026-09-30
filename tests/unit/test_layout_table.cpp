// SPDX-License-Identifier: GPL-3.0-or-later
//
// A TABLE ON A SHEET: its columns, its rows and the figures it prints.
//
// What a table SAYS is worked out in the core (`core/layout_table.hpp`) and
// only drawn by the app, so every figure here is checked without a painter:
// the coordinate list a new table is, the corner two parcels share listed once,
// a surveyed point's own number, the whole-number rounding a legal sheet needs,
// the four column verbs of `ÇIKTIÖĞE` and the file that keeps all of it.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/layout_table.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
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

    Rig()
    {
        register_builtin_commands(reg);
        // KÖŞENUMARALA is a processing tool, and the corner numbers it writes
        // are what a coordinate table reads back.
        processing::register_processing_commands(reg);
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(!r.ok(), line << " geçmemeliydi");
        return r.error().message;
    }

    const core::LayoutItem& table(const char* id = "tablo") const
    {
        const core::Layout* sheet = doc.layouts().find("Pafta");
        REQUIRE(sheet != nullptr);
        const core::LayoutItem* item = sheet->find(id);
        REQUIRE(item != nullptr);
        return *item;
    }

    core::TableText text(const char* id = "tablo") const
    {
        const core::Result<core::TableText> made = core::table_text(doc, table(id));
        REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error().message));
        return made.value();
    }

    /// Two parcels that share their 100 m edge, and a numbered point on one
    /// of the shared corners.
    void parcels()
    {
        run("SÜTUN nokta_no metin");
        run("SÜTUN ada_no tam_sayi");
        run("KATMAN ad=PARSEL");
        run("ALAN 485300,4310200 485400,4310200 485400,4310260 485300,4310260"); // 1
        run("ALAN 485400,4310200 485480,4310200 485480,4310260 485400,4310260"); // 2
        run("ÖZNİTELİK ada_no 1 1284");
        run("ÖZNİTELİK ada_no 2 1285");
        run("KATMAN ad=NOKTA");
        run("NOKTA 485400,4310260"); // 3, on the shared corner
        run("ÖZNİTELİK nokta_no 3 \"P-17\"");
        run("ÇIKTIYERLEŞİMİ islem=ekle ad=Pafta kagit=A3 yon=yatay");
        run("ÇIKTIÖĞE islem=ekle yerlesim=Pafta tur=tablo ad=tablo metin=PARSEL x=300 y=20 "
            "genislik=100 yukseklik=80");
    }
};

/// A temp directory of this test's own, removed when the case ends.
class TempDir
{
public:
    explicit TempDir(const char* tag)
    {
        path_ = fs::temp_directory_path() / (std::string("kentoscad-tablo-") + tag);
        std::error_code ec;
        fs::remove_all(path_, ec);
        fs::create_directories(path_);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const char* name) const { return (path_ / name).string(); }

private:
    fs::path path_;
};

} // namespace

TEST_CASE("Tablo: sayı tam sayıyla yazılır — yarım sıfırdan uzağa, binlik ve ondalık işareti")
{
    // 485 320,155 m: a double holds it a hair to one side of the half, and
    // which side decides ,15 or ,16. In whole numbers the half is a half.
    CHECK(core::format_fixed(485320155, 3, 2, false, true) == "485320,16");
    CHECK(core::format_fixed(485320154, 3, 2, false, true) == "485320,15");
    CHECK(core::format_fixed(-485320155, 3, 2, false, true) == "-485320,16");
    CHECK(core::format_fixed(485320155, 3, 2, true, true) == "485.320,16");
    CHECK(core::format_fixed(485320155, 3, 2, true, false) == "485,320.16");
    CHECK(core::format_fixed(1500, 3, 0, false, true) == "2"); // 1,5 → 2
    CHECK(core::format_fixed(-1500, 3, 0, false, true) == "-2");
    CHECK(core::format_fixed(-400, 3, 0, false, true) == "0"); // no "-0"
    CHECK(core::format_fixed(12, 0, 3, false, true) == "12,000");
    CHECK(core::format_fixed(123456, 3, -1, false, true) == "123,456");    // as it is
    CHECK(core::format_fixed(6000000000, 6, 2, true, true) == "6.000,00"); // 6 000 m²
}

TEST_CASE("Tablo: yeni tablo koordinat listesidir; ortak köşe bir kez, noktanın kendi numarası")
{
    Rig r;
    r.parcels();
    const core::LayoutItem& item = r.table();

    // A NEW TABLE LISTS NOTHING IT WAS NOT ASKED FOR: the three columns of a
    // coordinate list, over the corners.
    REQUIRE(item.table_columns.size() == 3);
    CHECK(item.table_columns[0].source == "$no");
    CHECK(item.table_columns[1].source == "$y");
    CHECK(item.table_columns[2].source == "$x");
    CHECK(item.table.rows == core::TableRows::Vertices);

    const core::TableText t = r.text();
    CHECK(t.heads == std::vector<std::string>{"No", "Sağa (Y)", "Yukarı (X)"});

    // SIX CORNERS, not eight: the two on the shared edge are one point each.
    REQUIRE(t.rows.size() == 6);
    CHECK(t.rows[0] == std::vector<std::string>{"1", "485300,00", "4310200,00"});
    CHECK(t.rows[1] == std::vector<std::string>{"2", "485400,00", "4310200,00"});
    // THE SURVEYED POINT'S OWN NUMBER, where it stands.
    CHECK(t.rows[2] == std::vector<std::string>{"P-17", "485400,00", "4310260,00"});
    CHECK(t.rows[4][1] == "485480,00");
}

TEST_CASE("Tablo: nesne satırları, öznitelik ve hesaplanan sütun, biçim komuttan verilir")
{
    Rig r;
    r.parcels();
    r.run("ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=tablo satirlar=nesne "
          "sutunlar=ada_no sutunlar=$alan");
    r.run("ÇIKTIÖĞE islem=sutunayarla yerlesim=Pafta ad=tablo sutun=2 baslik=\"Alan (m²)\" "
          "ondalik=1 binlik=evet sutun_hiza=sag");
    const core::TableText t = r.text();
    REQUIRE(t.heads.size() == 2);
    CHECK(t.heads[1] == "Alan (m²)");
    REQUIRE(t.rows.size() == 2);
    CHECK(t.rows[0][0] == "1284");
    CHECK(t.rows[0][1] == "6.000,0"); // 100 × 60 m
    CHECK(t.rows[1][1] == "4.800,0"); // 80 × 60 m

    // THE DECIMAL MARK IS THE TABLE'S, for every figure in it.
    r.run("ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=tablo ondalik_isaret=nokta");
    CHECK(r.text().rows[0][1] == "6,000.0");
}

TEST_CASE("Tablo: sütun fiilleri ekler, taşır, ayarlar, siler; her biri tek geri alma adımı")
{
    Rig r;
    r.parcels();
    r.run("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=tablo kaynak=$katman baslik=Katman");
    REQUIRE(r.table().table_columns.size() == 4);
    CHECK(r.table().table_columns[3].source == "$katman");

    // TO THE FRONT, and every other column moves up one.
    r.run("ÇIKTIÖĞE islem=sutuntasi yerlesim=Pafta ad=tablo sutun=4 hedef=1");
    CHECK(r.table().table_columns[0].source == "$katman");
    CHECK(r.table().table_columns[1].source == "$no");

    r.run("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=tablo kaynak=ada_no hedef=2");
    CHECK(r.table().table_columns[1].source == "ada_no");

    r.run("ÇIKTIÖĞE islem=sutunayarla yerlesim=Pafta ad=tablo sutun=1 sutun_genislik=18 "
          "esaralik=evet");
    CHECK(r.table().table_columns[0].width == core::um_from_mm(18));
    CHECK(r.table().table_columns[0].mono);

    r.run("ÇIKTIÖĞE islem=sutunsil yerlesim=Pafta ad=tablo sutun=1");
    REQUIRE(r.table().table_columns.size() == 4);
    CHECK(r.table().table_columns[0].source == "ada_no");

    // ONE CTRL+Z, ONE VERB.
    r.run("GERİAL");
    CHECK(r.table().table_columns.size() == 5);
    CHECK(r.table().table_columns[0].source == "$katman");
}

TEST_CASE("Tablo: yanlış kaynak, olmayan sütun ve tablo olmayan öğe adıyla reddedilir")
{
    Rig r;
    r.parcels();
    CHECK(r.refused("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=tablo kaynak=$kot")
              .find("Tanınmayan hesaplanan sütun: '$kot'") != std::string::npos);
    CHECK(r.refused("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=tablo kaynak=tapu")
              .find("Öznitelik sütunu yok: 'tapu'") != std::string::npos);
    CHECK(
        r.refused("ÇIKTIÖĞE islem=sutunsil yerlesim=Pafta ad=tablo sutun=9").find("3 sütun var") !=
        std::string::npos);
    CHECK(r.refused("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=harita kaynak=$y")
              .find("bir tablo değil") != std::string::npos);
    CHECK(r.refused("ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=harita cizgiler=hayir")
              .find("yalnız bir tablo öğesine verilir") != std::string::npos);
}

TEST_CASE("Tablo: sütunlar ve biçem dosyayla ve şablonla gidip gelir; eski tablo eski anlamında")
{
    TempDir tmp("dosya");
    const std::string path = tmp.file("tablo.pcad");
    Rig written;
    written.parcels();
    for (const char* line :
         {"ÇIKTIÖĞE islem=sutunayarla yerlesim=Pafta ad=tablo sutun=2 baslik=\"Y (m)\" ondalik=3 "
          "binlik=evet sutun_genislik=30",
          "ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=tablo baslik_yazi=3.5 baslik_zemin=#E6E6E6 "
          "baslik_hiza=orta baslik_kalin=hayir cizgi_renk=#808080 cizgi_kalinlik=0.25 "
          "seritli=evet serit_renk=#F7F7F7 ondalik_isaret=nokta"})
        written.run(line);
    const core::LayoutItem before = written.table();
    const std::uint64_t hash      = written.doc.content_hash();
    written.run("FARKLIKAYDET \"" + path + "\"");

    Rig reloaded;
    reloaded.run("AÇ \"" + path + "\"");
    const core::LayoutItem& back = reloaded.table();
    CHECK(back.table_columns == before.table_columns);
    CHECK(back.table == before.table);
    CHECK(back.table.header_fill == 0xFFE6E6E6u);
    CHECK(back.table.header_align == 1);
    CHECK_FALSE(back.table.decimal_comma);
    CHECK(reloaded.doc.content_hash() == hash);

    // THE TEMPLATE TOO: a kurum's coordinate list is a layout it reuses.
    const core::Layout* sheet = written.doc.layouts().find("Pafta");
    REQUIRE(sheet != nullptr);
    const core::Result<core::Layout> templated =
        core::layout_from_json(core::layout_to_json(*sheet, "Kurum"), "Kurum");
    REQUIRE(templated.ok());
    const core::LayoutItem* copied = templated.value().find("tablo");
    REQUIRE(copied != nullptr);
    CHECK(copied->table_columns == before.table_columns);
    CHECK(copied->table == before.table);

    // A TABLE WRITTEN BEFORE COLUMNS COULD BE SET has no list of its own, and
    // prints what it always printed: every attribute its layer has.
    core::LayoutItem older = before;
    older.table_columns.clear();
    older.table = core::LayoutTableStyle{};
    older.columns.clear();
    const std::vector<core::LayoutColumn> legacy = core::table_column_list(written.doc, older);
    REQUIRE(legacy.size() == 2);
    CHECK(legacy[0].source == "nokta_no");
    CHECK(legacy[1].source == "ada_no");
}

TEST_CASE("Tablo: olmayan katman ya da sütun tabloyu reddeder, boş başlık basılmaz")
{
    Rig r;
    r.parcels();
    r.run("ÇIKTIÖĞE islem=sutunekle yerlesim=Pafta ad=tablo kaynak=ada_no");
    r.run("SÜTUN ada_no sil=evet");
    const core::Result<core::TableText> gone = core::table_text(r.doc, r.table());
    REQUIRE_FALSE(gone.ok());
    CHECK(gone.error().message.find("'ada_no' adlı öznitelik sütunu yok") != std::string::npos);
    // AND THE SHEET'S CHECK SAYS WHICH ITEM READS IT.
    bool said = false;
    for (const std::string& one : core::layout_trouble(*r.doc.layouts().find("Pafta"), r.doc))
        if (one.find("ada_no") != std::string::npos && one.find("tablo") != std::string::npos)
            said = true;
    CHECK(said);
}

TEST_CASE("Tablo: fuzz tohumları — sütunlu tablo okunur, öğesine uymayan tablo kaydı reddedilir")
{
    const fs::path corpus = fs::path(KENTOS_FUZZ_DIR) / "tohum" / "proje";
    if (!fs::exists(corpus)) PENDING("Fuzz tohum korpusu bulunamadı: " + corpus.string());

    Rig good;
    good.run("AÇ \"" + (corpus / "22-tablo-sutunlari.pcad").string() + "\"");
    const core::LayoutItem& table = good.table();
    REQUIRE(table.table_columns.size() == 4);
    CHECK(table.table_columns[3].source == "ada_no");
    CHECK(table.table_columns[3].heading == "Ada");
    CHECK(table.table.stripes);
    CHECK(table.table.header_fill == 0xFFE6E6E6u);

    // A RECORD FOR AN ITEM THE FILE DOES NOT HAVE is a corrupt file, refused
    // by name — never read as some other item's columns (io.md R18).
    Rig bad;
    const std::string said =
        bad.refused("AÇ \"" + (corpus / "23-tablo-kaydi-bozuk.pcad").string() + "\"");
    CHECK(said.find("tablo kaydı") != std::string::npos);
    CHECK(bad.doc.layouts().size() == 0);
}

TEST_CASE("Tablo: köşenin numarası, KÖŞENUMARALA'nın paftaya yazdığıdır; ortak köşede de")
{
    Rig r;
    r.parcels();
    // THE RIGHT PARCEL'S CORNERS, numbered from its lower left: K-1 there,
    // then round counter-clockwise.
    r.run("KÖŞENUMARALA nesneler=2 baslangic=485400,4310200 onek=K-");
    const core::TableText t = r.text();
    REQUIRE(t.rows.size() == 6);
    std::vector<std::string> numbers;
    for (const auto& row : t.rows)
        numbers.push_back(row[0]);
    // THE LEFT PARCEL'S CORNERS FIRST, in its own order: the shared ones carry
    // the number the right parcel's corners were given, and where a surveyed
    // point (P-17) and a written number meet, the sheet's number wins — the
    // coordinate table says what the pafta shows. A corner nobody numbered
    // keeps its row.
    CHECK(numbers == std::vector<std::string>{"1", "K-1", "K-4", "4", "K-2", "K-3"});
}

TEST_CASE("Tablo: noktanın yanındaki numara yazısı listeye ikinci kez girmez (bildirilen hata)")
{
    // A POINT AND ITS NUMBER, on one layer, the way a surveyed list arrives:
    // each point with its number written beside it. The caption is an object
    // of its own standing a little off the point — and the table listed it as
    // a second corner, so every point entered the list twice.
    TempDir tmp("noktalar");
    const std::string list = tmp.file("olcu.txt");
    {
        std::FILE* f = std::fopen(list.c_str(), "w");
        REQUIRE(f != nullptr);
        std::fputs("101 485300.000 4310200.000\n102 485400.000 4310200.000\n"
                   "103 485400.000 4310260.000\n",
                   f);
        std::fclose(f);
    }
    Rig r;
    r.run("KATMAN ad=NOKTA");
    r.run("NOKTALAR dosya=\"" + list + "\"");
    r.run("METİN 485301,4310201 \"101\"");
    r.run("METİN 485401,4310201 \"102\"");
    r.run("METİN 485401,4310261 \"103\"");
    r.run("ÇIKTIYERLEŞİMİ islem=ekle ad=Pafta kagit=A3 yon=yatay");
    r.run("ÇIKTIÖĞE islem=ekle yerlesim=Pafta tur=tablo ad=tablo metin=NOKTA");

    const core::TableText corners = r.text();
    REQUIRE(corners.rows.size() == 3);
    CHECK(corners.rows[0] == std::vector<std::string>{"101", "485300,00", "4310200,00"});
    CHECK(corners.rows[2] == std::vector<std::string>{"103", "485400,00", "4310260,00"});

    // AND AS OBJECTS: an attribute table lists the points, not their labels.
    r.run("ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=tablo satirlar=nesne");
    CHECK(r.text().rows.size() == 3);
}
