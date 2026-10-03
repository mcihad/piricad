// SPDX-License-Identifier: GPL-3.0-or-later
//
// Feature classes — the "pens" a digitiser picks (TODOS G-04): a package of definitions is data and
// is refused HERE when it cannot work; KALEM builds the layer, the columns and the binding; what is
// drawn on a class layer starts with the class's defaults and a shape that is not the class's is
// refused whole; KALEMBAĞLA brings existing objects under a class with a preview and a field
// mapping; KALEMDENETİM asks every object whether it still is what its class says; and the typed
// line, a script and a journal replay leave one drawing.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/feature_classes.hpp"
#include "piricad/command/journal.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/document.hpp"
#include "piricad/io/service.hpp"
#include "piricad/script/json_runner.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace piricad;
using namespace piricad::command;

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

    bool run(const std::string& line, Origin origin = Origin::Test)
    {
        said.clear();
        auto r = bus.execute_line(line, origin);
        if (!r) said += "[hata] " + r.error().message + "\n";
        return r.ok();
    }

    /// The cell as the table shows it, or "boş".
    std::string cell(const char* column, std::int64_t key) const
    {
        const core::AttrId col    = doc.attributes().find(column);
        const core::EntityId slot = doc.slot_of(static_cast<core::EntityKey>(key));
        if (col == core::kNoAttr) return "sütun yok";
        auto got = doc.attribute(col, slot);
        return got && got.value().present
                   ? core::attr_display(got.value(), core::DecimalMark::Point)
                   : std::string("boş");
    }

    const core::Layer* layer(const char* name) const
    {
        const core::LayerId id = doc.find_layer(name);
        return id == core::kNoLayer ? nullptr : doc.layer(id);
    }
};

/// A temp directory of this test's own, removed when the case ends (test.md R19).
class TempDir
{
public:
    explicit TempDir(const char* tag)
    {
        path_ = fs::temp_directory_path() / (std::string("piricad-kalem-") + tag);
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

private:
    fs::path path_;
};

/// Writes `text` to a file of the directory and returns its path.
std::string write_package(const TempDir& dir, const char* name, const std::string& text)
{
    const std::string path = dir.file(name);
    std::ofstream(path, std::ios::binary) << text;
    return path;
}

/// A one-class package whose only variable part is the class body.
std::string package_with(const std::string& classes)
{
    return R"json({"schema_version": 1, "package_version": "1.2.3", "id": "deneme",
        "source": "test", "published": "2026-10-03", "licence": "GPL-3.0-or-later",
        "siniflar": [)json" +
           classes + "]}";
}

} // namespace

// ---------------------------------------------------------------------------------------------
// THE PACKAGE
// ---------------------------------------------------------------------------------------------

TEST_CASE("KALEM paketi: gönderilen örnek paket yüklenir ve her sınıfı kendi kendine tutarlı")
{
    auto loaded =
        load_feature_classes(std::string(PIRICAD_DATA_DIR) + "/catalogs/cad/kalem-katalogu.json");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error().message));
    const FeatureClassCatalog& catalog = loaded.value();
    CHECK_EQ(catalog.package_id, "piricad-ornek-kalemler");
    CHECK_GE(catalog.classes.size(), std::size_t{7});

    // THE WORDS THAT FIND A CLASS: its id, its name and its aliases, in the Turkish fold.
    for (const char* word : {"bina", "Bina", "BINA", "BUILDING", "BNA"}) {
        const FeatureClass* c = catalog.find(word);
        REQUIRE_MESSAGE(c != nullptr, word);
        CHECK_EQ(c->id, "bina");
    }
    CHECK(catalog.find("yol ekseni") != nullptr);
    CHECK(catalog.find("hicbir_sinif") == nullptr);

    // A class the sample holds is a thing a geometry check can use.
    const FeatureClass* bina = catalog.find("bina");
    REQUIRE(bina != nullptr);
    CHECK_EQ(bina->geometry, ClassGeometry::Area);
    CHECK_EQ(bina->layer, "BINA");
    CHECK(bina->has_colour);
    CHECK_EQ(bina->colour_rgba, 0xFF7A4A1Cu);
    CHECK(bina->has_fill);
    CHECK_EQ(bina->fill_rgba, 0x307A4A1Cu);
    CHECK_EQ(bina->width_um, 350);
    CHECK_GT(bina->min_area_m2, 0.0);

    // THE REFERENCE A LAYER STORES finds the class again, whatever version it was bound under.
    const std::string ref = catalog.reference_to(*bina);
    CHECK_EQ(ref, "piricad-ornek-kalemler@0.1.0/bina");
    CHECK_EQ(catalog.of_reference(ref), bina);
    CHECK_EQ(catalog.of_reference("piricad-ornek-kalemler@9.9.9/bina"), bina);
    CHECK(catalog.of_reference("baska-paket@0.1.0/bina") == nullptr);
    CHECK(catalog.of_reference("piricad-ornek-kalemler@0.1.0/yok") == nullptr);
    CHECK(catalog.of_reference("") == nullptr);
}

TEST_CASE("KALEM paketi: çalışmayacak bir tanım yükleme anında, sınıfı ve alanı adıyla reddedilir")
{
    TempDir tmp("paket");
    const auto refused = [&tmp](const char* name, const std::string& classes) {
        auto got = load_feature_classes(write_package(tmp, name, package_with(classes)));
        return got.ok() ? std::string() : got.error().message;
    };
    const std::string good_head =
        R"json({"id": "a", "ad": "A", "geometri": "alan", "katman": {"ad": "A"})json";

    CHECK(refused("iyi.json", good_head + "}").empty()); // the control: the head alone is a class

    // An unknown field type, a default the type cannot read, a default outside the field's own
    // list.
    CHECK_NE(
        refused("tur.json", good_head + R"json(, "alanlar": [{"kimlik": "x", "tur": "yok"}]})json")
            .find("'x'"),
        std::string::npos);
    const std::string bad_default = refused(
        "varsayilan.json",
        good_head +
            R"json(, "alanlar": [{"kimlik": "kat", "tur": "tam_sayi", "varsayilan": "iki"}]})json");
    CHECK_NE(bad_default.find("'kat'"), std::string::npos);
    CHECK_NE(bad_default.find("'iki'"), std::string::npos);
    const std::string outside =
        refused("secenek.json",
                good_head + R"json(, "alanlar": [{"kimlik": "t", "tur": "metin", "varsayilan": "c",
                              "secenekler": ["a", "b"]}]})json");
    CHECK_NE(outside.find("seçenekleri arasında değil"), std::string::npos);

    // A geometry that is none of the three, a class with no layer, a bad colour, a width out of
    // range.
    CHECK_NE(
        refused("geometri.json",
                R"json({"id": "a", "ad": "A", "geometri": "yuzey", "katman": {"ad": "A"}})json")
            .find("nokta, cizgi ya da alan"),
        std::string::npos);
    CHECK_NE(refused("katman.json", R"json({"id": "a", "ad": "A", "geometri": "alan"})json")
                 .find("katman.ad"),
             std::string::npos);
    CHECK_NE(refused("renk.json", R"json({"id": "a", "ad": "A", "geometri": "alan",
                              "katman": {"ad": "A", "renk": "kırmızı"}})json")
                 .find("#RRGGBB"),
             std::string::npos);
    CHECK_NE(refused("kalinlik.json", R"json({"id": "a", "ad": "A", "geometri": "cizgi",
                              "katman": {"ad": "A", "kalinlik_mm": 99}})json")
                 .find("kalinlik_mm"),
             std::string::npos);

    // Two classes with one id, two classes on one layer (in the Turkish fold: İ and i are one).
    CHECK_NE(
        refused("ayni-id.json", good_head + "}," + good_head + "}").find("aynı kimlik iki kez"),
        std::string::npos);
    const std::string two_on_one =
        refused("ayni-katman.json",
                R"json({"id": "a", "ad": "A", "geometri": "alan", "katman": {"ad": "Dere"}},
              {"id": "b", "ad": "B", "geometri": "alan", "katman": {"ad": "DERE"}})json");
    CHECK_NE(two_on_one.find("katmanını başka bir sınıf da kullanıyor"), std::string::npos);

    // A package with no id or version cannot be told apart from another one.
    auto anonymous = load_feature_classes(write_package(
        tmp, "kimliksiz.json", R"json({"package_version": "1.0.0", "siniflar": []})json"));
    REQUIRE_FALSE(anonymous.ok());
    CHECK_NE(anonymous.error().message.find("paket kimliğini"), std::string::npos);
    // A missing file says which one.
    auto missing = load_feature_classes(tmp.file("yok.json"));
    REQUIRE_FALSE(missing.ok());
    CHECK_NE(missing.error().message.find("yok.json"), std::string::npos);
}

// ---------------------------------------------------------------------------------------------
// KALEM
// ---------------------------------------------------------------------------------------------

TEST_CASE("KALEM: argümansız sınıfları sayar; sınıfı seçince katman, sütunlar ve bağ kurulur")
{
    Rig r;
    REQUIRE(r.run("KALEM"));
    CHECK_NE(r.said.find("bina"), std::string::npos);
    CHECK_NE(r.said.find("kapalı alan"), std::string::npos);
    CHECK_NE(r.said.find("açık çizgi"), std::string::npos);
    CHECK_EQ(r.doc.layers().size(), std::size_t{1}); // listing touches nothing

    REQUIRE_MESSAGE(r.run("KALEM bina"), r.said);
    const core::Layer* layer = r.layer("BINA");
    REQUIRE(layer != nullptr);
    CHECK_EQ(layer->feature_class, "piricad-ornek-kalemler@0.1.0/bina");
    CHECK_EQ(layer->group, "TOPOGRAFYA > YAPI");
    CHECK_EQ(layer->appearance.rgba, 0xFF7A4A1Cu);
    CHECK_EQ(layer->appearance.width_um, 350);
    CHECK_EQ(layer->appearance.fill_rgba, 0x307A4A1Cu);
    CHECK_EQ(layer->description, "Bina oturum alanları");
    CHECK_EQ(r.bus.active_layer(), r.doc.find_layer("BINA"));
    for (const char* column : {"sinif_kodu", "bina_no", "kat_sayisi", "yapi_turu"})
        CHECK_NE(r.doc.attributes().find(column), core::kNoAttr);
    // The class's own columns belong to its layer, so another layer's inspector stays free of them.
    CHECK_EQ(r.doc.attributes().column(r.doc.attributes().find("bina_no"))->spec().layer, "BINA");

    // The same words by every name, and the same class twice is no second layer and no second
    // column.
    const std::size_t layers  = r.doc.layers().size();
    const std::size_t columns = r.doc.attributes().columns();
    REQUIRE(r.run("KALEM BUILDING"));
    CHECK_EQ(r.doc.layers().size(), layers);
    CHECK_EQ(r.doc.attributes().columns(), columns);
    // Listing now marks the one in use.
    REQUIRE(r.run("KALEM"));
    CHECK_NE(r.said.find("etkin"), std::string::npos);

    // An unknown class names what would have worked and writes nothing.
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.run("KALEM gokdelen"));
    CHECK_NE(r.said.find("Tanımlı sınıflar"), std::string::npos);
    CHECK_NE(r.said.find("bina"), std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);
}

TEST_CASE("KALEM: sınıf katmanına çizilen nesne sınıfın varsayılanlarıyla başlar")
{
    Rig r;
    REQUIRE(r.run("KALEM bina"));
    REQUIRE_MESSAGE(r.run("ALAN noktalar=0,0 10,0 10,10 0,10"), r.said);
    CHECK_EQ(r.cell("sinif_kodu", 1), "BNA");
    CHECK_EQ(r.cell("kat_sayisi", 1), "1");
    CHECK_EQ(r.cell("yapi_turu", 1), "Betonarme");
    CHECK_EQ(r.cell("bina_no", 1), "boş"); // no default: left empty, not zero

    // THE DEFAULT FILLS AN EMPTY CELL, never overwrites a typed one.
    REQUIRE(r.run("ÖZNİTELİK kat_sayisi 1 4"));
    REQUIRE(r.run("ALAN noktalar=20,0 30,0 30,10 20,10"));
    CHECK_EQ(r.cell("kat_sayisi", 1), "4");
    CHECK_EQ(r.cell("kat_sayisi", 2), "1");

    // ONE UNDO STEP takes the object and its defaults together.
    const std::size_t steps = r.undo.undo_depth();
    REQUIRE(r.run("ALAN noktalar=40,0 50,0 50,10 40,10"));
    CHECK_EQ(r.undo.undo_depth(), steps + 1);
    CHECK_EQ(r.cell("sinif_kodu", 3), "BNA");
    REQUIRE(r.run("GERİAL"));
    CHECK_FALSE(r.doc.alive(r.doc.slot_of(static_cast<core::EntityKey>(3))));
}

TEST_CASE("KALEM: sınıfın geometrisinde olmayan nesne bütün komutla birlikte reddedilir")
{
    Rig r;
    REQUIRE(r.run("KALEM bina"));
    const auto hash  = r.doc.content_hash();
    const auto steps = r.undo.undo_depth();

    // A building is a closed area: a loose line on its layer is not what was meant.
    CHECK_FALSE(r.run("ÇİZGİ 0,0 10,0"));
    CHECK_NE(r.said.find("'Bina' sınıfı kapalı alan ister"), std::string::npos);
    CHECK_NE(r.said.find("açık çizgi"), std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash); // nothing half-applied (Article 1.6)
    CHECK_EQ(r.undo.undo_depth(), steps); // and no undo step for a refused command

    // A point is not an area either.
    CHECK_FALSE(r.run("NOKTA 5,5"));
    CHECK_EQ(r.doc.content_hash(), hash);

    // The right shape goes in; the other classes take their own.
    REQUIRE(r.run("ALAN noktalar=0,0 10,0 10,10 0,10"));
    REQUIRE(r.run("KALEM yol_ekseni"));
    REQUIRE_MESSAGE(r.run("ÇİZGİ 0,0 50,0"), r.said);
    CHECK_EQ(r.cell("yol_sinifi", 2), "Mahalle yolu");
    CHECK_FALSE(r.run("ALAN noktalar=0,5 10,5 10,15 0,15"));
    REQUIRE(r.run("KALEM direk"));
    REQUIRE_MESSAGE(r.run("NOKTA 3,3"), r.said);
    CHECK_EQ(r.cell("direk_turu", 3), "Aydınlatma");
    CHECK_FALSE(r.run("ÇİZGİ 0,0 1,1"));
}

TEST_CASE("KALEM: birkaç sınıfın ortak alanı baştan projenin sütunudur; uymayan sütun reddedilir")
{
    Rig r;
    REQUIRE(r.run("KALEM bina"));
    // `sinif_kodu` is carried by every class of the package, so it is the project's from the start;
    // `bina_no` only by Bina, so it stays on its layer.
    CHECK_EQ(r.doc.attributes().column(r.doc.attributes().find("sinif_kodu"))->spec().layer, "");
    CHECK_EQ(r.doc.attributes().column(r.doc.attributes().find("bina_no"))->spec().layer, "BINA");
    REQUIRE(r.run("KALEM yol_ekseni"));
    CHECK_EQ(r.doc.attributes().column(r.doc.attributes().find("sinif_kodu"))->spec().layer, "");
    // ... and each class still starts its objects with its own value.
    REQUIRE(r.run("ÇİZGİ 0,0 20,0"));
    CHECK_EQ(r.cell("sinif_kodu", 1), "YOL");

    // A column the drawing already has, of a type the class does not want, is not touched.
    Rig other;
    REQUIRE(other.run("SÜTUN kimlik=kat_sayisi tur=metin"));
    const auto hash = other.doc.content_hash();
    CHECK_FALSE(other.run("KALEM bina"));
    CHECK_NE(other.said.find("'kat_sayisi' sütunu belgede metin"), std::string::npos);
    CHECK_NE(other.said.find("tam_sayi"), std::string::npos);
    CHECK_EQ(other.doc.content_hash(), hash);
    CHECK(other.layer("BINA") == nullptr); // refused BEFORE the first change

    // A column the drawing scoped to ANOTHER layer is widened to the project's, one undoable-less
    // edit that a batch is refused (it could not give it back).
    Rig scoped;
    REQUIRE(scoped.run("KATMAN ad=BASKA"));
    REQUIRE(scoped.run("SÜTUN kimlik=bina_no tur=tam_sayi katman=BASKA"));
    REQUIRE_MESSAGE(scoped.run("KALEM bina"), scoped.said);
    CHECK_EQ(scoped.doc.attributes().column(scoped.doc.attributes().find("bina_no"))->spec().layer,
             "");
    Rig batched;
    REQUIRE(batched.run("KATMAN ad=BASKA"));
    REQUIRE(batched.run("SÜTUN kimlik=bina_no tur=tam_sayi katman=BASKA"));
    script::JsonRunner runner(batched.bus, script::Sandbox::Project);
    auto ran = runner.run_text(
        R"json({"ad": "x", "komutlar": [{"cmd": "core.feature_class", "args": {"ad": "bina"}}]})json");
    REQUIRE_FALSE(ran.ok());
    CHECK_NE(ran.error().message.find("geri alınamaz"), std::string::npos);
    CHECK(batched.layer("BINA") == nullptr);
}

TEST_CASE("KALEM: katman başka bir sınıfı izliyorsa o sınıf reddedilir; paket değişince yeni sürüm "
          "okunur")
{
    TempDir tmp("catisma");
    Rig r;
    const std::string first = write_package(
        tmp, "kurum.json",
        R"json({"schema_version": 1, "package_version": "1.0.0", "id": "kurum", "source": "t",
            "published": "2026-10-03", "licence": "x", "siniflar": [
              {"id": "a", "ad": "A", "geometri": "alan", "katman": {"ad": "ORTAK"}}]})json");
    REQUIRE(r.bus.app_settings()
                .set("core.kalem.katalog", core::SettingValue::text(first).value())
                .ok());
    REQUIRE_MESSAGE(r.run("KALEM a"), r.said);
    CHECK_EQ(r.layer("ORTAK")->feature_class, "kurum@1.0.0/a");

    // ANOTHER PACKAGE'S CLASS ON THE SAME LAYER is refused, with the layer and the class named.
    const std::string second = write_package(
        tmp, "baska.json",
        R"json({"schema_version": 1, "package_version": "1.0.0", "id": "baska", "source": "t",
            "published": "2026-10-03", "licence": "x", "siniflar": [
              {"id": "z", "ad": "Z", "geometri": "alan", "katman": {"ad": "ORTAK"}}]})json");
    REQUIRE(r.bus.app_settings()
                .set("core.kalem.katalog", core::SettingValue::text(second).value())
                .ok());
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.run("KALEM z"));
    CHECK_NE(r.said.find("'ORTAK' katmanı zaten 'a' sınıfını izliyor"), std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);

    // THE FILE IS READ AGAIN WHEN IT CHANGES: a corrected package reaches the next command.
    REQUIRE(r.bus.app_settings()
                .set("core.kalem.katalog", core::SettingValue::text(first).value())
                .ok());
    write_package(tmp, "kurum.json",
                  R"json({"schema_version": 1, "package_version": "1.0.1", "id": "kurum",
            "source": "t", "published": "2026-10-03", "licence": "x", "siniflar": [
              {"id": "a", "ad": "A", "geometri": "alan", "katman": {"ad": "ORTAK"},
               "alanlar": [{"kimlik": "ek", "tur": "metin", "varsayilan": "yeni"}]}]})json");
    REQUIRE_MESSAGE(r.run("KALEM a"), r.said);
    CHECK_EQ(r.layer("ORTAK")->feature_class, "kurum@1.0.1/a");
    REQUIRE(r.run("ALAN noktalar=0,0 5,0 5,5 0,5"));
    CHECK_EQ(r.cell("ek", 1), "yeni");

    // A package that cannot be read says so, and the layer keeps all its objects and is not
    // checked.
    write_package(tmp, "kurum.json", "{ bu json değil");
    CHECK_FALSE(r.run("KALEM a"));
    CHECK_NE(r.said.find("geçerli JSON değil"), std::string::npos);
    REQUIRE(r.run(
        "ÇİZGİ 0,0 9,9")); // the class layer is active, the package is unreadable: drawn, unchecked
    CHECK_EQ(r.cell("ek", 2), "boş");
}

// ---------------------------------------------------------------------------------------------
// KALEMBAĞLA
// ---------------------------------------------------------------------------------------------

TEST_CASE(
    "KALEMBAĞLA: önizleme hiçbir şey yazmaz; bağlayınca uygun nesne katmana gelir, uymayan atlanır")
{
    Rig r;
    REQUIRE(r.run("KATMAN ad=ESKI"));
    REQUIRE(r.run("ALAN noktalar=0,0 10,0 10,10 0,10"));   // 1: an area
    REQUIRE(r.run("ALAN noktalar=20,0 30,0 30,10 20,10")); // 2: an area
    REQUIRE(r.run("ÇİZGİ 0,20 10,20"));                    // 3: a line, not a building
    REQUIRE(r.run("SÜTUN kimlik=kat tur=tam_sayi"));
    REQUIRE(r.run("ÖZNİTELİK kat 1 3"));

    const auto hash = r.doc.content_hash();
    REQUIRE_MESSAGE(
        r.run(
            "KALEMBAĞLA ad=bina nesneler=1 nesneler=2 nesneler=3 esle=kat:kat_sayisi onizle=evet"),
        r.said);
    CHECK_EQ(r.doc.content_hash(), hash); // a preview writes NOTHING: no layer, no column, no move
    CHECK(r.layer("BINA") == nullptr);
    CHECK_EQ(r.doc.attributes().find("kat_sayisi"), core::kNoAttr);
    CHECK_NE(r.said.find("2 nesne 'Bina' sınıfına bağlanacak"), std::string::npos);
    CHECK_NE(r.said.find("Uymayan, atlandı: 1 açık çizgi"), std::string::npos);
    CHECK_NE(r.said.find("kat → kat_sayisi"), std::string::npos);

    const std::size_t steps = r.undo.undo_depth();
    REQUIRE_MESSAGE(
        r.run("KALEMBAĞLA ad=bina nesneler=1 nesneler=2 nesneler=3 esle=kat:kat_sayisi"), r.said);
    CHECK_EQ(r.undo.undo_depth(), steps + 1); // one step
    const core::LayerId bina = r.doc.find_layer("BINA");
    const core::LayerId eski = r.doc.find_layer("ESKI");
    CHECK_EQ(r.doc.entities().layer[r.doc.slot_of(static_cast<core::EntityKey>(1))], bina);
    CHECK_EQ(r.doc.entities().layer[r.doc.slot_of(static_cast<core::EntityKey>(3))], eski);
    CHECK_EQ(r.cell("kat_sayisi", 1), "3"); // carried from the old column
    CHECK_EQ(r.cell("kat_sayisi", 2), "1"); // nothing to carry: the class default
    CHECK_EQ(r.cell("sinif_kodu", 2), "BNA");
    CHECK_EQ(r.layer("BINA")->feature_class, "piricad-ornek-kalemler@0.1.0/bina");

    // ONE UNDO takes the objects back to their layer and the cells away.
    REQUIRE(r.run("GERİAL"));
    CHECK_EQ(r.doc.entities().layer[r.doc.slot_of(static_cast<core::EntityKey>(1))], eski);
    CHECK_EQ(r.cell("kat", 1), "3"); // the old column was never touched
    CHECK_EQ(r.cell("kat_sayisi", 1), "boş");
}

TEST_CASE("KALEMBAĞLA: eşleme yanlış yazılırsa ya da türü uymuyorsa hiçbir şey olmaz")
{
    Rig r;
    REQUIRE(r.run("ALAN noktalar=0,0 10,0 10,10 0,10"));
    REQUIRE(r.run("SÜTUN kimlik=kat tur=tam_sayi"));
    REQUIRE(r.run("SÜTUN kimlik=yazi tur=metin"));
    const auto hash = r.doc.content_hash();

    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina nesneler=1 esle=kat"));
    CHECK_NE(r.said.find("'eski_sutun:sinif_alani' biçiminde"), std::string::npos);
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina nesneler=1 esle=olmayan:kat_sayisi"));
    CHECK_NE(r.said.find("'olmayan' sütunu yok"), std::string::npos);
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina nesneler=1 esle=kat:olmayan_alan"));
    CHECK_NE(r.said.find("'olmayan_alan' alanı yok"), std::string::npos);
    // A text into a whole-number field is not a conversion the mapping makes up.
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina nesneler=1 esle=yazi:kat_sayisi"));
    CHECK_NE(r.said.find("eşlenemez"), std::string::npos);
    CHECK_FALSE(r.run("KALEMBAĞLA ad=gokdelen nesneler=1"));
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina nesneler=99"));
    CHECK_NE(r.said.find("Bilinmeyen nesne"), std::string::npos);
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina")); // nothing selected, nothing named
    CHECK_EQ(r.doc.content_hash(), hash);

    // A number may be carried into a TEXT field, as the text the table shows.
    REQUIRE(r.run("ÖZNİTELİK kat 1 7"));
    REQUIRE_MESSAGE(r.run("KALEMBAĞLA ad=bina nesneler=1 esle=kat:yapi_turu"), r.said);
    CHECK_EQ(r.cell("yapi_turu", 1), "7");
}

TEST_CASE("KALEMBAĞLA: bütün bir katmanı ve seçimi bağlar; kilitli katmandaki nesne atlanır")
{
    Rig r;
    REQUIRE(r.run("KATMAN ad=KILITLI"));
    REQUIRE(r.run("ALAN noktalar=0,0 10,0 10,10 0,10")); // 1, on a layer about to be locked
    REQUIRE(r.run("KATMAN ad=KILITLI kilitli=evet"));
    REQUIRE(r.run("KATMAN ad=ACIK"));
    REQUIRE(r.run("ALAN noktalar=20,0 30,0 30,10 20,10")); // 2
    REQUIRE(r.run("ALAN noktalar=40,0 50,0 50,10 40,10")); // 3
    REQUIRE(r.run("ÇİZGİ 0,20 10,20"));                    // 4

    // A locked layer's objects cannot move: nothing is bound, and the sentence says why.
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.run("KALEMBAĞLA ad=bina katman=KILITLI"));
    CHECK_NE(r.said.find("kilitli"), std::string::npos);
    CHECK_EQ(r.doc.content_hash(), hash);

    // The whole open layer: the two areas come, the line stays.
    REQUIRE_MESSAGE(r.run("KALEMBAĞLA ad=bina katman=ACIK"), r.said);
    CHECK_NE(r.said.find("2 nesne 'Bina' sınıfına bağlandı"), std::string::npos);
    CHECK_NE(r.said.find("Uymayan, atlandı: 1 açık çizgi"), std::string::npos);
    CHECK_EQ(r.doc.entities().layer[r.doc.slot_of(static_cast<core::EntityKey>(2))],
             r.doc.find_layer("BINA"));
    CHECK_EQ(r.doc.entities().layer[r.doc.slot_of(static_cast<core::EntityKey>(4))],
             r.doc.find_layer("ACIK"));

    // The selection, when neither objects nor a layer is named.
    Rig s;
    REQUIRE(s.run("ÇİZGİ 0,0 10,0"));
    REQUIRE(s.run("ÇİZGİ 0,5 10,5"));
    REQUIRE(s.run("SEÇ NESNE nesneler=1 nesneler=2"));
    REQUIRE_MESSAGE(s.run("KALEMBAĞLA ad=yol_ekseni"), s.said);
    CHECK_NE(s.said.find("seçim (2 nesne)"), std::string::npos);
    CHECK_EQ(s.cell("yol_sinifi", 2), "Mahalle yolu");
}

// ---------------------------------------------------------------------------------------------
// KALEMDENETİM
// ---------------------------------------------------------------------------------------------

TEST_CASE(
    "KALEMDENETİM: küçük alan, eksik zorunlu değer, listede olmayan değer ve eksik sütunu söyler")
{
    Rig r;
    REQUIRE(r.run("KALEM parsel"));
    REQUIRE(r.run("ALAN noktalar=0,0 20,0 20,20 0,20"));         // 1: 400 m², no ada/parsel
    REQUIRE(r.run("ALAN noktalar=30,0 30.5,0 30.5,0.5 30,0.5")); // 2: a 0,25 m² sliver
    REQUIRE(r.run("ÖZNİTELİK ada 1 12"));
    REQUIRE(r.run("ÖZNİTELİK parsel 1 3"));
    CHECK(r.run("KALEMDENETİM"));
    CHECK_NE(r.said.find("PARSEL — Parsel: 2 nesne"), std::string::npos);
    CHECK_NE(r.said.find("çok küçük"), std::string::npos);   // the sliver (min 1 m²)
    CHECK_NE(r.said.find("eksik değer"), std::string::npos); // object 2 has no ada
    CHECK_NE(r.said.find("alan 0.25 m², en az 1.00 m² olmalı"), std::string::npos);

    // A value outside a field's own list.
    REQUIRE(r.run("KALEM bina"));
    REQUIRE(r.run("ALAN noktalar=0,30 10,30 10,40 0,40")); // 3
    REQUIRE(r.run("ÖZNİTELİK yapi_turu 3 Cam"));
    CHECK(r.run("KALEMDENETİM ad=bina"));
    CHECK_NE(r.said.find("listede yok"), std::string::npos);
    CHECK_NE(r.said.find("'yapi_turu' = 'Cam'"), std::string::npos);

    // `sec=evet` selects the offenders so the next command can act on them.
    REQUIRE(r.run("KALEMDENETİM ad=bina sec=evet"));
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(static_cast<core::EntityKey>(3)));

    // A clean layer says so. An object moved onto the layer by hand with the wrong shape is found.
    REQUIRE(r.run("ÖZNİTELİK yapi_turu 3 Yığma"));
    REQUIRE(r.run("KALEMDENETİM ad=bina"));
    CHECK_NE(r.said.find("sorun yok"), std::string::npos);
    // A layer that already held a loose line when the class took it over: the line is found, not
    // refused (it was there first), and KALEM said it had not checked.
    Rig old;
    REQUIRE(old.run("KATMAN ad=BINA"));
    REQUIRE(old.run("ÇİZGİ 0,60 10,60")); // 1
    REQUIRE(old.run("KALEM bina"));
    CHECK_NE(old.said.find("katmanında zaten 1 nesne var; denetlenmediler"), std::string::npos);
    REQUIRE(old.run("KALEMDENETİM ad=bina"));
    CHECK_NE(old.said.find("geometri"), std::string::npos);
    CHECK_NE(old.said.find("sınıf kapalı alan ister, nesne açık çizgi"), std::string::npos);
    // ... and a hand move of the same line ONTO a class layer is refused as the draw is.
    Rig moved;
    REQUIRE(moved.run("KALEM bina"));
    REQUIRE(moved.run("KATMAN ad=DIS"));
    REQUIRE(moved.run("ÇİZGİ 0,60 10,60")); // 1
    const auto hash = moved.doc.content_hash();
    REQUIRE(moved.run("SEÇ NESNE nesneler=1"));
    CHECK_FALSE(moved.run("KATMANAT katman=BINA"));
    CHECK_NE(moved.said.find("'Bina' sınıfı kapalı alan ister"), std::string::npos);
    CHECK_EQ(moved.doc.content_hash(), hash);

    // Nothing to check, and a class with no layer yet.
    Rig empty;
    REQUIRE(empty.run("KALEMDENETİM"));
    CHECK_NE(empty.said.find("denetlenecek bir şey yok"), std::string::npos);
    CHECK_FALSE(empty.run("KALEMDENETİM ad=bina"));
    CHECK_NE(empty.said.find("izleyen bir katman yok"), std::string::npos);
}

TEST_CASE("KALEMDENETİM: sınıfın sütunu belgeden silinmişse bir bulgu olarak söyler")
{
    Rig r;
    REQUIRE(r.run("KALEM dere"));
    REQUIRE(r.run("SÜTUN kimlik=akis sil=evet"));
    REQUIRE(r.run("KALEMDENETİM"));
    CHECK_NE(r.said.find("eksik sütun"), std::string::npos);
    CHECK_NE(r.said.find("'akis' sütunu belgede yok"), std::string::npos);
    // The object drawn meanwhile still gets the defaults of the columns that remain.
    REQUIRE(r.run("ÇİZGİ 0,0 30,0"));
    CHECK_EQ(r.cell("sinif_kodu", 1), "DRE");
}

// ---------------------------------------------------------------------------------------------
// KANIT: the same drawing from every client
// ---------------------------------------------------------------------------------------------

TEST_CASE("PROOF: KALEM ve KALEMBAĞLA komut satırından, betikten ve günlük oynatmasından aynı "
          "belgeyi bırakır")
{
    const std::vector<std::string> lines = {
        "KALEM bina",       "ALAN noktalar=0,0 10,0 10,10 0,10", "KALEM yol_ekseni",
        "ÇİZGİ 0,20 30,20", "KALEMBAĞLA ad=dere nesneler=2",
    };
    Rig typed, scripted, replayed;
    for (const std::string& line : lines)
        REQUIRE_MESSAGE(typed.run(line, Origin::CommandLine), line << ": " << typed.said);

    // The same, as a script: one step per command, in this order.
    script::JsonRunner runner(scripted.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"json({"ad": "Kalemler", "komutlar": [
        {"cmd": "core.feature_class", "args": {"ad": "bina"}},
        {"cmd": "core.area", "args": {"noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]]}},
        {"cmd": "core.feature_class", "args": {"ad": "yol_ekseni"}},
        {"cmd": "core.line", "args": {"noktalar": [[0,20000],[30000,20000]]}},
        {"cmd": "core.feature_class_bind", "args": {"ad": "dere", "nesneler": [2]}}]})json");
    REQUIRE_MESSAGE(ran.ok(), (ran.ok() ? std::string() : ran.error().message));
    CHECK_EQ(typed.doc.content_hash(), scripted.doc.content_hash());

    // THE JOURNAL REPLAYS the drawing from empty.
    for (const auto& e : typed.journal.entries()) {
        auto replay = replayed.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        if (!replay) FAIL_WITH(e.command_id, replay.error().message);
    }
    CHECK_EQ(replayed.doc.content_hash(), typed.doc.content_hash());
    // ... and the bound layer and the defaults are in what was rebuilt.
    CHECK_EQ(replayed.cell("sinif_kodu", 1), "BNA");
    CHECK_EQ(replayed.layer("DERE")->feature_class, "piricad-ornek-kalemler@0.1.0/dere");
}

TEST_CASE("KALEM: iptal edilen komut sınıf katmanını ve bağını geride bırakmaz")
{
    Rig r;
    const auto hash = r.doc.content_hash();
    CHECK_FALSE(r.run("KALEM gokdelen"));
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.doc.layers().size(), std::size_t{1});
    // A refused draw on a class layer leaves the layer, the columns and the binding as KALEM made
    // them.
    REQUIRE(r.run("KALEM bina"));
    const auto after_pen = r.doc.content_hash();
    CHECK_FALSE(r.run("ÇİZGİ 0,0 5,5"));
    CHECK_EQ(r.doc.content_hash(), after_pen);
    CHECK_EQ(r.layer("BINA")->feature_class, "piricad-ornek-kalemler@0.1.0/bina");
}

// ---------------------------------------------------------------------------------------------
// THE FILE
// ---------------------------------------------------------------------------------------------

TEST_CASE("KALEM: sınıfı izleyen katman dosyaya yazılıp açılınca izlemeye devam eder, "
          "varsayılanlar uygulanır")
{
    TempDir tmp("dosya");
    const std::string path = tmp.file("kalemli.pcad");

    Rig written;
    REQUIRE(written.run("KALEM bina"));
    REQUIRE(written.run("ALAN noktalar=0,0 10,0 10,10 0,10"));
    REQUIRE(written.run("KATMAN ad=SERBEST"));
    const std::uint64_t hash = written.doc.content_hash();
    REQUIRE_MESSAGE(written.run("FARKLIKAYDET \"" + path + "\""), written.said);

    Rig reloaded;
    REQUIRE_MESSAGE(reloaded.run("AÇ \"" + path + "\""), reloaded.said);
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    REQUIRE(reloaded.layer("BINA") != nullptr);
    CHECK_EQ(reloaded.layer("BINA")->feature_class, "piricad-ornek-kalemler@0.1.0/bina");
    CHECK(reloaded.layer("SERBEST")->feature_class.empty()); // a plain layer stays plain

    // The reopened drawing enforces the class again: the package is read, not the file.
    REQUIRE(reloaded.run("KATMAN ad=BINA"));
    CHECK_FALSE(reloaded.run("ÇİZGİ 0,30 10,30"));
    REQUIRE(reloaded.run("ALAN noktalar=20,0 30,0 30,10 20,10"));
    CHECK_EQ(reloaded.cell("kat_sayisi", 2), "1");

    // A drawing that never used a class writes no class block: it is the file an earlier build
    // wrote.
    Rig plain;
    REQUIRE(plain.run("KATMAN ad=DUZ"));
    REQUIRE(plain.run("ÇİZGİ 0,0 5,0"));
    const std::string plain_path = tmp.file("duz.pcad");
    REQUIRE(plain.run("FARKLIKAYDET \"" + plain_path + "\""));
    std::ifstream in(plain_path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(bytes.find("katman sinifi") == std::string::npos);
    Rig back;
    REQUIRE(back.run("AÇ \"" + plain_path + "\""));
    CHECK(back.layer("DUZ")->feature_class.empty());
}

TEST_CASE("KALEM: tohum dosyaları — geçerli olan sınıfı taşır, bozuk olanlar hiçbir şey bırakmadan "
          "reddedilir")
{
    const fs::path corpus = fs::path(PIRICAD_FUZZ_DIR) / "tohum" / "proje";
    const auto open_seed  = [&corpus](Rig& r, const char* name) {
        return r.run("AÇ \"" + (corpus / name).string() + "\"");
    };

    Rig good;
    REQUIRE_MESSAGE(open_seed(good, "26-katman-sinifi.pcad"), good.said);
    REQUIRE(good.layer("BINA") != nullptr);
    CHECK_EQ(good.layer("BINA")->feature_class, "piricad-ornek-kalemler@0.1.0/bina");
    CHECK_EQ(good.layer("YOL_EKSENI")->feature_class, "piricad-ornek-kalemler@0.1.0/yol_ekseni");

    // A string index past the pool and a column shorter than the layer list are refused, and the
    // drawing is left as it was found (the reader's rollback), never half-classed.
    for (const char* name : {"27-katman-sinifi-bozuk.pcad", "28-katman-sinifi-eksik-satir.pcad"}) {
        Rig bad;
        const auto hash = bad.doc.content_hash();
        CHECK_FALSE(open_seed(bad, name));
        CHECK_EQ(bad.doc.content_hash(), hash);
        CHECK(bad.layer("BINA") == nullptr);
    }
}
