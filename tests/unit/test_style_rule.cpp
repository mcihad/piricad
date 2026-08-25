// SPDX-License-Identifier: GPL-3.0-or-later
//
// The style catalogue and its rule evaluator — .claude/model.md R13–R19.
//
// Two things are under test and they are not the same thing:
//
//   1. the ENGINE — a closed, declarative rule set that turns a feature and a map
//      scale into one catalogue row, deterministically;
//   2. the COMMAND — `STİL`, which is the only way that row ever reaches
//      `style[e]`, because a GIS renderer is a command (R14).
//
// The catalogue used by the engine tests is a synthetic fixture written inline.
// It is deliberately NOT a regulation: no test in this file may become the place
// a `gösterim` value lives, because that value belongs in /data (CLAUDE.md 5.13).
// The one test that reads /data asserts the shipped package's PROVENANCE, not its
// contents.
#include "piricad_test.hpp"

#include <algorithm>

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/style_library.hpp"
#include "piricad/core/style_rule.hpp"
#include "piricad/render/scene.hpp"
#include "piricad/render/view.hpp"
#include "piricad/script/json_runner.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace piricad;
using namespace piricad::command;

namespace {

namespace fs = std::filesystem;

/// A synthetic package. Every value here is invented for the test and says so.
const char* kFixture = R"({
  "schema_version": 1,
  "package_version": "9.9.9",
  "id": "test-stil-paketi",
  "source": "PiriCAD sınama paketi — mevzuat metni DEĞİLDİR",
  "published": "2026-01-01",
  "licence": "test",

  "cizgi_desenleri": [
    { "id": "surekli", "indeks": 0 },
    { "id": "kesik",   "indeks": 1 }
  ],
  "tarama_desenleri": [
    { "id": "yok",   "indeks": 0 },
    { "id": "capraz", "indeks": 5 }
  ],

  "stiller": [
    { "id": "alan-buyuk",
      "ad": "Büyük alan",
      "kaynak": "sınama",
      "cizgi":  { "renk": "#FF203040", "kalinlik_um": 500, "desen": "surekli" },
      "dolgu":  { "renk": "#80A0B0C0", "tarama": "capraz" },
      "simge": 3,
      "sira": 20,
      "olcek": { "en_kucuk_payda": 0, "en_buyuk_payda": 5000 } },

    { "id": "alan-kucuk",
      "ad": "Küçük alan",
      "cizgi":  { "renk": "#FF112233", "kalinlik_um": 250, "desen": "kesik" },
      "dolgu":  { "renk": "#FF445566" },
      "sira": 10 },

    { "id": "cizgi-genel",
      "ad": "Genel çizgi",
      "cizgi": { "renk": "#FF7F0000" } },

    { "id": "kalan",
      "ad": "Sınıflanmamış",
      "cizgi": { "renk": "#FF808080", "kalinlik_um": 100 } }
  ],

  "kurallar": [
    { "id": "k-alan-buyuk",
      "stil": "alan-buyuk",
      "kosullar": [
        { "alan": "geometri", "esittir": "alan" },
        { "alan": "alan_mm2", "aralik": { "en_az": 1000000000 } }
      ] },

    { "id": "k-alan-kucuk",
      "stil": "alan-kucuk",
      "kosullar": [ { "alan": "geometri", "esittir": "alan" } ] },

    { "id": "k-cizgi",
      "stil": "cizgi-genel",
      "kosullar": [ { "alan": "geometri", "biri": ["cizgi", "nokta"] } ],
      "olcek": { "en_buyuk_payda": 25000 } },

    { "id": "k-kalan", "stil": "kalan" }
  ]
})";

core::StyleCatalog load_fixture()
{
    auto json = core::Json::parse(kFixture);
    if (!json) return core::StyleCatalog{};
    auto catalog = core::StyleCatalog::from_json(json.value());
    if (!catalog) return core::StyleCatalog{};
    return std::move(catalog.value());
}

core::Result<core::StyleCatalog> parse_catalog(const std::string& text)
{
    auto json = core::Json::parse(text);
    if (!json) return json.error();
    return core::StyleCatalog::from_json(json.value());
}

core::FeatureView area_feature(std::int64_t area_mm2)
{
    core::FeatureView f;
    f.set_text("geometri", "alan");
    f.set_number("alan_mm2", area_mm2);
    return f;
}

/// Writes the fixture where the command can read it. A catalogue is loaded from
/// DISK at runtime and is never compiled in (data.md P10), so the command's path
/// has to be exercised through a real file.
fs::path fixture_file()
{
    const fs::path path = fs::temp_directory_path() / "piricad-stil-fixture.json";
    std::ofstream out(path, std::ios::binary);
    out << kFixture;
    return path;
}

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [](std::string_view) {};
    }

    /// One layer, one three-vertex polyline and one square parcel on it.
    void draw_fixture_document()
    {
        CHECK(bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
        CHECK(bus.execute_line("ÇİZGİ 485300.000,4310200.000 485360.000,4310200.000 "
                               "485360.000,4310240.000",
                               Origin::Test)
                  .ok());
    }
};

std::string what_happened(const Journal& j)
{
    std::string out;
    for (const auto& e : j.entries()) {
        out += e.command_id;
        out += ' ';
        out += e.args.to_json().dump();
        out += '\n';
    }
    return out;
}

std::string style_column(const core::Document& doc)
{
    std::string out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.entities().alive(e)) continue;
        out += std::to_string(doc.entities().style[e]);
        out += ' ';
    }
    return out;
}

/// What `style_column` reads on a document nothing has styled: every live entity
/// still inherits from its layer (model.md R13, StyleId{0}).
std::string all_by_layer(const core::Document& doc)
{
    std::string out;
    for (std::size_t i = 0; i < doc.live_entity_count(); ++i)
        out += "0 ";
    return out;
}

} // namespace

// ------------------------------------------------------------------ colour ----

TEST_CASE("STİL: renk metni '#AARRGGBB' ve '#RRGGBB' biçimlerini çözer")
{
    auto opaque = core::parse_rgba("#112233");
    REQUIRE(opaque.ok());
    CHECK_EQ(opaque.value(), 0xFF112233u);

    auto alpha = core::parse_rgba("#80112233");
    REQUIRE(alpha.ok());
    CHECK_EQ(alpha.value(), 0x80112233u);

    // Case is irrelevant for hex digits; the folding table is not involved.
    auto upper = core::parse_rgba("#ABCDEF");
    REQUIRE(upper.ok());
    CHECK_EQ(upper.value(), 0xFFABCDEFu);

    CHECK(!core::parse_rgba("#12345").ok());
    CHECK(!core::parse_rgba("#1122GG").ok());
    CHECK(!core::parse_rgba("").ok());

    // The message names what was expected and what arrived (§3, R19).
    auto bad = core::parse_rgba("kirmizi");
    REQUIRE(!bad.ok());
    CHECK(bad.error().message.find("#AARRGGBB") != std::string::npos);
    CHECK(bad.error().message.find("kirmizi") != std::string::npos);
}

// ------------------------------------------------------------ package load ----

TEST_CASE("STİL: paket künyesi eksiksiz olmadan yüklenmez")
{
    auto complete = parse_catalog(kFixture);
    REQUIRE(complete.ok());
    CHECK_EQ(complete.value().id(), std::string{"test-stil-paketi"});
    CHECK_EQ(complete.value().package_version(), std::string{"9.9.9"});
    CHECK_EQ(complete.value().published(), std::string{"2026-01-01"});
    CHECK_EQ(complete.value().licence(), std::string{"test"});
    CHECK_EQ(complete.value().entries().size(), std::size_t{4});
    CHECK_EQ(complete.value().rules().size(), std::size_t{4});

    // data.md R2: each header field is required, and the message says which one.
    for (const char* missing : {"package_version", "source", "published", "licence"}) {
        std::string text         = R"({"schema_version":1,"package_version":"1.0.0","id":"x",
                               "source":"s","published":"2026-01-01","licence":"l"})";
        const std::string needle = std::string("\"") + missing + "\"";
        const auto at            = text.find(needle);
        REQUIRE(at != std::string::npos);
        text.replace(at, needle.size(), "\"kaldirildi\"");

        auto result = parse_catalog(text);
        REQUIRE(!result.ok());
        CHECK(result.error().message.find(missing) != std::string::npos);
    }
}

TEST_CASE("STİL: bozuk paket sessizce değil, sebebini söyleyerek reddedilir")
{
    const auto rejects = [](const char* text, const char* needle) {
        auto result = parse_catalog(text);
        if (result.ok()) {
            FAIL_WITH(text, "paket kabul edildi, reddedilmeliydi");
            return;
        }
        if (result.error().message.find(needle) == std::string::npos)
            FAIL_WITH(needle, result.error().message);
    };

    const std::string head =
        R"({"schema_version":1,"package_version":"1.0.0","id":"x","source":"s",
            "published":"2026-01-01","licence":"l",)";

    // A rule pointing at a row that is not there fails at LOAD, not years later
    // when the feature that matches it finally turns up.
    rejects((head + R"("stiller":[{"id":"a"}],
                      "kurallar":[{"id":"k","stil":"yok-boyle"}]})")
                .c_str(),
            "yok-boyle");

    // A catalogue id is permanent; the same id twice means one of them is lost.
    rejects((head + R"("stiller":[{"id":"a"},{"id":"a"}]})").c_str(), "iki kez");

    // Two tests on one condition would need an operator precedence — that is the
    // expression evaluator CLAUDE.md 5.11 refuses.
    rejects((head + R"("stiller":[{"id":"a"}],
                      "kurallar":[{"id":"k","stil":"a",
                        "kosullar":[{"alan":"g","esittir":"x","var":true}]}]})")
                .c_str(),
            "tek bir sınama");

    // No test at all is a condition that means nothing.
    rejects((head + R"("stiller":[{"id":"a"}],
                      "kurallar":[{"id":"k","stil":"a","kosullar":[{"alan":"g"}]}]})")
                .c_str(),
            "hiçbir sınama");

    // A float bound would be a stored floating-point value (model.md R21/P8).
    rejects((head + R"("stiller":[{"id":"a"}],
                      "kurallar":[{"id":"k","stil":"a",
                        "kosullar":[{"alan":"g","aralik":{"en_az":1.5}}]}]})")
                .c_str(),
            "tam sayı");

    // An unknown dash name must not quietly become dash 0.
    rejects((head + R"("cizgi_desenleri":[{"id":"surekli","indeks":0}],
                      "stiller":[{"id":"a","cizgi":{"desen":"boyle-bir-desen-yok"}}]})")
                .c_str(),
            "boyle-bir-desen-yok");

    // An inverted scale window is a typo that would silently never match.
    rejects((head + R"("stiller":[{"id":"a",
                        "olcek":{"en_kucuk_payda":25000,"en_buyuk_payda":1000}}]})")
                .c_str(),
            "ters");
}

// ------------------------------------------------------------- conditions ----

TEST_CASE("STİL: kural dili dört sınamadan ibarettir ve dördü de çalışır")
{
    const core::StyleCatalog catalog = load_fixture();
    REQUIRE(catalog.entries().size() == 4);

    // Equality + range: a big face takes the first rule.
    auto big = catalog.classify(area_feature(2'400'000'000), 1000);
    REQUIRE(big.ok());
    CHECK_EQ(big.value()->id, std::string{"alan-buyuk"});

    // Same geometry, below the range: the second rule catches it.
    auto small = catalog.classify(area_feature(12), 1000);
    REQUIRE(small.ok());
    CHECK_EQ(small.value()->id, std::string{"alan-kucuk"});

    // Set membership.
    core::FeatureView line;
    line.set_text("geometri", "cizgi");
    auto as_line = catalog.classify(line, 1000);
    REQUIRE(as_line.ok());
    CHECK_EQ(as_line.value()->id, std::string{"cizgi-genel"});

    core::FeatureView point;
    point.set_text("geometri", "nokta");
    auto as_point = catalog.classify(point, 1000);
    REQUIRE(as_point.ok());
    CHECK_EQ(as_point.value()->id, std::string{"cizgi-genel"});

    // A field the feature does not carry matches nothing, so the catch-all wins.
    core::FeatureView bare;
    bare.set_text("baska", "deger");
    auto rest = catalog.classify(bare, 1000);
    REQUIRE(rest.ok());
    CHECK_EQ(rest.value()->id, std::string{"kalan"});

    // Presence, tested directly.
    core::StyleCondition present;
    present.field = "ada_no";
    present.test  = core::StyleCondition::Test::Present;
    CHECK(!present.matches(bare));
    core::FeatureView with_ada;
    with_ada.set_number("ada_no", 0);
    CHECK(present.matches(with_ada));

    // A numeric cell compares as its exact decimal rendering, so equality on a
    // number is possible without a second syntax for it.
    core::StyleCondition equals_number;
    equals_number.field = "ada_no";
    equals_number.values.push_back("0");
    CHECK(equals_number.matches(with_ada));
}

TEST_CASE("STİL: ölçek penceresi kuralı hem açar hem kapatır")
{
    const core::StyleCatalog catalog = load_fixture();

    core::FeatureView line;
    line.set_text("geometri", "cizgi");

    // 1:25000 is the inclusive far edge of the line rule.
    auto inside = catalog.classify(line, 25000);
    REQUIRE(inside.ok());
    CHECK_EQ(inside.value()->id, std::string{"cizgi-genel"});

    // One step further out and the rule no longer applies, so the catch-all does.
    auto outside = catalog.classify(line, 25001);
    REQUIRE(outside.ok());
    CHECK_EQ(outside.value()->id, std::string{"kalan"});

    const core::ScaleWindow unbounded{};
    CHECK(unbounded.covers(0));
    CHECK(unbounded.covers(4294967295u));

    const core::ScaleWindow band{1000, 5000};
    CHECK(!band.covers(999));
    CHECK(band.covers(1000));
    CHECK(band.covers(5000));
    CHECK(!band.covers(5001));
}

TEST_CASE("STİL: eşleşme dosya sırasına göredir ve ilk uyan kazanır")
{
    const core::StyleCatalog catalog = load_fixture();

    // The big-area rule is declared first, so a feature that satisfies BOTH area
    // rules takes it. Reversing the file order would reverse the answer — which is
    // exactly why the order is part of the package's content hash.
    auto both = catalog.classify(area_feature(1'000'000'000), 1000);
    REQUIRE(both.ok());
    CHECK_EQ(both.value()->id, std::string{"alan-buyuk"});
}

TEST_CASE("STİL: bilinmeyen satır kimliği sessiz varsayılana değil, hataya düşer")
{
    const core::StyleCatalog catalog = load_fixture();

    auto known = catalog.entry("alan-kucuk");
    REQUIRE(known.ok());
    CHECK_EQ(known.value()->appearance.width_um, 250);
    CHECK_EQ(known.value()->appearance.dash, 1);
    CHECK_EQ(known.value()->appearance.rgba, 0xFF112233u);

    auto unknown = catalog.entry("yok-boyle-bir-satir");
    REQUIRE(!unknown.ok());
    CHECK(unknown.error().code == core::ErrorCode::NotFound);
    // model.md R35: the answer names the package version it came from.
    CHECK(unknown.error().message.find("9.9.9") != std::string::npos);

    // A catalogue with rules but no match reports it rather than inventing a grey
    // line: an unclassified feature is a fact the caller must decide about.
    auto rules_only = parse_catalog(R"({"schema_version":1,"package_version":"1.0.0","id":"x",
        "source":"s","published":"2026-01-01","licence":"l",
        "stiller":[{"id":"a"}],
        "kurallar":[{"id":"k","stil":"a","kosullar":[{"alan":"yok","var":true}]}]})");
    REQUIRE(rules_only.ok());
    core::FeatureView empty;
    CHECK(!rules_only.value().classify(empty, 0).ok());
}

// --------------------------------------------------------------- cascade ----

TEST_CASE("STİL: satır yalnız bildirdiği özelliği yazar, gerisi ByLayer kalır")
{
    const core::StyleCatalog catalog = load_fixture();

    core::Appearance base;
    base.rgba      = 0xFF010203u;
    base.width_um  = 1234;
    base.dash      = 7;
    base.fill_rgba = 0xFF040506u;
    base.hatch     = 9;

    auto row = catalog.entry("cizgi-genel"); // declares a colour and nothing else
    REQUIRE(row.ok());

    const core::Appearance out = core::apply_entry(*row.value(), base);

    CHECK_EQ(out.rgba, 0xFF7F0000u);
    CHECK(out.src_colour == core::Source::Explicit);

    // Everything the row is silent about keeps the layer's value AND its source —
    // that is the R19 cascade surviving a catalogue application.
    CHECK_EQ(out.width_um, 1234);
    CHECK(out.src_width == core::Source::ByLayer);
    CHECK_EQ(out.dash, 7);
    CHECK(out.src_dash == core::Source::ByLayer);
    CHECK_EQ(out.fill_rgba, 0xFF040506u);
    CHECK_EQ(out.hatch, 9);
    CHECK(out.src_fill == core::Source::ByLayer);
}

// ---------------------------------------------------------- determinism ----

TEST_CASE("STİL: aynı katalog iki kez yüklenince bit-birebir aynıdır")
{
    const core::StyleCatalog a = load_fixture();
    const core::StyleCatalog b = load_fixture();
    CHECK_EQ(a.content_hash(), b.content_hash());

    // Rule ORDER decides the answer, so two packages differing only in order are
    // two different packages and must hash differently.
    auto reordered = parse_catalog(R"({"schema_version":1,"package_version":"1.0.0","id":"x",
        "source":"s","published":"2026-01-01","licence":"l",
        "stiller":[{"id":"a","cizgi":{"renk":"#FF000000"}},
                   {"id":"b","cizgi":{"renk":"#FFFFFFFF"}}],
        "kurallar":[{"id":"k2","stil":"b"},{"id":"k1","stil":"a"}]})");
    auto straight  = parse_catalog(R"({"schema_version":1,"package_version":"1.0.0","id":"x",
        "source":"s","published":"2026-01-01","licence":"l",
        "stiller":[{"id":"a","cizgi":{"renk":"#FF000000"}},
                   {"id":"b","cizgi":{"renk":"#FFFFFFFF"}}],
        "kurallar":[{"id":"k1","stil":"a"},{"id":"k2","stil":"b"}]})");
    REQUIRE(reordered.ok());
    REQUIRE(straight.ok());
    CHECK(reordered.value().content_hash() != straight.value().content_hash());
}

TEST_CASE("STİL: aynı katalog + aynı belge = aynı StyleId dizisi ve aynı içerik özeti")
{
    const fs::path package = fixture_file();

    const auto run_once = [&](std::string& styles, std::uint64_t& hash) {
        Rig rig;
        rig.draw_fixture_document();
        CHECK(
            rig.bus
                .execute_line("STİL katman=PARSEL paket=\"" + package.string() + "\"", Origin::Test)
                .ok());
        styles = style_column(rig.doc);
        hash   = rig.doc.content_hash();
    };

    std::string styles_a;
    std::string styles_b;
    std::uint64_t hash_a = 0;
    std::uint64_t hash_b = 0;
    run_once(styles_a, hash_a);
    run_once(styles_b, hash_b);

    CHECK_EQ(styles_a, styles_b);
    CHECK_EQ(hash_a, hash_b);
    // Not the empty answer: the entity really did get a resolved style.
    CHECK(styles_a != all_by_layer(core::Document{}));
}

TEST_CASE("STİL: aynı görünüm iki kez istenirse tek StyleId'ye toplanır")
{
    Rig rig;
    rig.draw_fixture_document();

    CHECK(
        rig.bus.execute_line("STİL katman=PARSEL renk=4281236786 kalinlik=350", Origin::Test).ok());
    const std::string first       = style_column(rig.doc);
    const std::size_t after_first = rig.doc.styles().size();

    CHECK(rig.bus.execute_line("STİL katman=PARSEL sifirla=evet", Origin::Test).ok());
    CHECK_EQ(style_column(rig.doc), all_by_layer(rig.doc));

    CHECK(
        rig.bus.execute_line("STİL katman=PARSEL renk=4281236786 kalinlik=350", Origin::Test).ok());

    // Interning is what keeps the batch key (layer, style, kind) bounded: the same
    // appearance asked for twice is one row in the table, not two.
    CHECK_EQ(style_column(rig.doc), first);
    CHECK_EQ(rig.doc.styles().size(), after_first);
}

TEST_CASE("STİL: boş katmanın varsayılan görünümünü günceller")
{
    Rig rig;
    CHECK(rig.bus.execute_line("KATMAN ad=BOS", Origin::Test).ok());

    const core::LayerId layer = rig.doc.find_layer("BOS");
    REQUIRE(layer != core::kNoLayer);
    CHECK_EQ(rig.doc.layer_entity_count(layer), std::size_t{0});

    auto styled = rig.bus.execute_line(
        "STİL katman=BOS renk=4281236786 kalinlik=350 dolgu=2168724224", Origin::Test);
    REQUIRE(styled.ok());
    CHECK(styled.value().mutated);

    const core::Appearance& appearance = rig.doc.layer(layer)->appearance;
    CHECK_EQ(appearance.rgba, 4281236786u);
    CHECK_EQ(appearance.width_um, 350);
    CHECK_EQ(appearance.fill_rgba, 2168724224u);
    CHECK_EQ(appearance.src_colour, core::Source::Explicit);
    CHECK_EQ(appearance.src_width, core::Source::Explicit);
    CHECK_EQ(appearance.src_fill, core::Source::Explicit);
}

TEST_CASE("STİL: boş katmanda sembol yığınını saklar")
{
    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=BOS", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("STİL katman=BOS tip=dolgu dolgu=2168724224", Origin::Test).ok());
    REQUIRE(
        rig.bus.execute_line("STİL katman=BOS ekle=evet tip=cizgi renk=4281236786", Origin::Test)
            .ok());

    const core::LayerId layer = rig.doc.find_layer("BOS");
    REQUIRE(layer != core::kNoLayer);
    const core::StyleId style = rig.doc.layer(layer)->style;
    REQUIRE(style != core::kByLayerStyle);

    const core::Symbol symbol = rig.doc.styles().symbol_at(style);
    REQUIRE(symbol.layers.size() == std::size_t{2});
    CHECK(symbol.layers[0].type == core::SymbolLayerType::SimpleFill);
    CHECK(symbol.layers[1].type == core::SymbolLayerType::SimpleLine);
}

TEST_CASE("STİL: katalog satırı boş katmana tam sembol olarak yazılır")
{
    const fs::path package = fixture_file();

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=BOS", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("STİL katman=BOS paket=\"" + package.string() + "\" kod=alan-buyuk",
                              Origin::Test)
                .ok());

    const core::LayerId layer = rig.doc.find_layer("BOS");
    REQUIRE(layer != core::kNoLayer);
    const core::StyleId style = rig.doc.layer(layer)->style;
    REQUIRE(style != core::kByLayerStyle);

    const auto row = load_fixture().entry("alan-buyuk");
    REQUIRE(row.ok());
    const core::Symbol& symbol = rig.doc.styles().symbol_at(style);
    CHECK(symbol == core::symbol_of_entry(*row.value(), {}));
    CHECK(symbol.primary() == rig.doc.layer(layer)->appearance);
}

TEST_CASE("STİL: seçilen MPYY sembolü boş katmandan tuvale görselleriyle ulaşır")
{
    const std::string package = std::string(PIRICAD_DATA_DIR) + "/catalogs/mpyy/plan-gosterim.json";

    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=OSB", Origin::Test).ok());
    REQUIRE(rig.bus
                .execute_line("STİL katman=OSB paket=\"" + package +
                                  "\" kod=ortak-organize-sanayi-bolgesi",
                              Origin::Test)
                .ok());

    const core::LayerId layer = rig.doc.find_layer("OSB");
    REQUIRE(layer != core::kNoLayer);
    const core::StyleId style = rig.doc.layer(layer)->style;
    REQUIRE(style != core::kByLayerStyle);

    const core::Symbol& symbol = rig.doc.styles().symbol_at(style);
    bool has_document_image    = false;
    for (const core::SymbolLayer& symbol_layer : symbol.layers)
        has_document_image = has_document_image || symbol_layer.image != core::kNoImage;
    CHECK(has_document_image);
    CHECK(rig.doc.images().size() > std::size_t{1});

    REQUIRE(rig.bus.execute_line("ALAN 0,0 10000,0 10000,10000 0,10000", Origin::Test).ok());
    CHECK_EQ(rig.doc.entities().style[0], core::kByLayerStyle);

    render::ViewTransform view;
    view.set_viewport(800, 600);
    view.fit(rig.doc.extent());
    render::DrawList draw;
    render::build_scene(rig.doc, view, {}, draw);

    bool canvas_has_raster = false;
    for (const render::PassStyle& pass : draw.passes)
        if ((pass.type == core::SymbolLayerType::RasterFill ||
             pass.type == core::SymbolLayerType::RasterLine ||
             pass.type == core::SymbolLayerType::RasterMarker) &&
            !pass.image.empty())
            canvas_has_raster = true;
    CHECK(canvas_has_raster);
}

TEST_CASE("layer order decides what covers what, even when two layers share a style")
{
    // The defect this pins: passes were keyed by STYLE alone, so two layers
    // drawing the same gösterim shared one batch. One of them could not be drawn
    // over the other, and the layer list — which is the user's own statement of
    // what is on top — said nothing about the result.
    //
    // Everything here goes through the bus, like every other client: a scene the
    // test built by hand would prove something about a document nobody can make.
    Rig rig;
    REQUIRE(rig.bus.execute_line("KATMAN ad=ALT renk=4282348748", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ALAN 0,0 10000,0 10000,10000 0,10000", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("KATMAN ad=UST renk=4282348748", Origin::Test).ok());
    REQUIRE(rig.bus.execute_line("ALAN 0,0 10000,0 10000,10000 0,10000", Origin::Test).ok());

    const core::LayerId under = rig.doc.find_layer("ALT");
    const core::LayerId over  = rig.doc.find_layer("UST");
    REQUIRE(under != core::kNoLayer);
    REQUIRE(over != core::kNoLayer);
    REQUIRE(under < over);

    render::ViewTransform view;
    view.set_viewport(800, 600);
    view.fit(rig.doc.extent());

    render::DrawList draw;
    render::build_scene(rig.doc, view, {}, draw);

    // TWO batches carry ink, not one. Two layers with the same appearance used to
    // share a pass, and a shared pass cannot be ordered against itself.
    std::size_t with_ink = 0;
    for (std::uint32_t p : draw.order)
        if (p < draw.polylines.size() && !draw.polylines[p].runs.empty()) ++with_ink;
    CHECK_EQ(with_ink, 2);

    // And they are drawn in LAYER order, so a layer further down the list covers
    // the one above it — which is what the panel promises.
    std::vector<std::uint32_t> drawn;
    for (std::uint32_t p : draw.order)
        for (const render::DrawList::ZKey& key : draw.z_keys)
            if (key.pass == p) drawn.push_back(key.layer);

    REQUIRE(drawn.size() >= 2);
    CHECK(std::is_sorted(drawn.begin(), drawn.end()));
    CHECK(drawn.front() <= under);
    CHECK(drawn.back() >= over);
}

// ------------------------------------------------------- the equality proof ----

TEST_CASE("STİL: arayüz, komut satırı ve betik aynı belgeyi ve aynı günlüğü üretir")
{
    // Client 1 — the GUI, as a toolbar button that starts the command and a
    // dialog that answers its one prompt.
    Rig gui;
    gui.draw_fixture_document();
    {
        auto started = gui.bus.begin_interactive("STİL");
        REQUIRE(started.ok());
        auto& session = *started.value();

        CHECK(session.waiting());
        CHECK_EQ(session.prompt().message, std::string{"Katman adı"});
        CHECK(session.supply(Value::text("PARSEL")).ok());
        CHECK(gui.bus.finish(session).ok());
    }

    // Client 2 — the command line.
    Rig cli;
    cli.draw_fixture_document();
    CHECK(cli.bus.execute_line("STİL katman=PARSEL", Origin::CommandLine).ok());

    // Client 3 — a JSON script.
    Rig scr;
    scr.draw_fixture_document();
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        CHECK(runner
                  .run_text(R"({"ad":"Stil","komutlar":[
                                 {"cmd":"core.style","args":{"katman":"PARSEL"}}]})")
                  .ok());
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    CHECK(gui.journal.entries().back().origin == Origin::Gui);
    CHECK(cli.journal.entries().back().origin == Origin::CommandLine);
    CHECK(scr.journal.entries().back().origin == Origin::Script);
}

TEST_CASE("STİL: katalog yolu verilen çağrı da üç istemcide aynı sonucu verir")
{
    const fs::path package = fixture_file();

    Args args;
    args.set("katman", Value::text("PARSEL"));
    args.set("paket", Value::text(package.string()));
    args.set("olcek", Value::integer(1000));

    // The GUI's non-interactive path: a panel or dialog dispatches a full
    // invocation, exactly as the layer panel does for KATMAN.
    Rig gui;
    gui.draw_fixture_document();
    CHECK(gui.bus.dispatch(Invocation{"core.style", args, Origin::Gui}).ok());

    Rig cli;
    cli.draw_fixture_document();
    CHECK(cli.bus
              .execute_line("STİL katman=PARSEL paket=\"" + package.string() + "\" olcek=1000",
                            Origin::CommandLine)
              .ok());

    Rig scr;
    scr.draw_fixture_document();
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        const std::string text = R"({"ad":"Stil","komutlar":[{"cmd":"core.style","args":{
            "katman":"PARSEL","paket":")" +
                                 package.string() + R"(","olcek":1000}}]})";
        auto r = runner.run_text(text);
        CHECK(r.ok());
    }

    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));
}

// ------------------------------------------------------ rollback and undo ----

TEST_CASE("STİL: tek komut tek geri alma adımıdır")
{
    Rig rig;
    rig.draw_fixture_document();
    const std::size_t before = rig.undo.undo_depth();

    CHECK(rig.bus.execute_line("STİL katman=PARSEL renk=4292897792", Origin::Test).ok());
    CHECK_EQ(rig.undo.undo_depth(), before + 1);
    CHECK(style_column(rig.doc) != "0 ");

    CHECK(rig.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(style_column(rig.doc), all_by_layer(rig.doc));
}

TEST_CASE("STİL: iptal edilen komut geri alma yığınını büyütmez")
{
    Rig rig;
    rig.draw_fixture_document();
    const std::size_t before = rig.undo.undo_depth();

    auto started = rig.bus.begin_interactive("STİL");
    REQUIRE(started.ok());
    started.value()->cancel(); // ESC before answering the prompt
    CHECK(rig.bus.finish(*started.value()).ok());

    CHECK_EQ(rig.undo.undo_depth(), before);
    CHECK_EQ(style_column(rig.doc), all_by_layer(rig.doc));
}

TEST_CASE("STİL: hata belgeye hiç dokunmadan döner")
{
    Rig rig;
    rig.draw_fixture_document();
    const std::uint64_t before = rig.doc.content_hash();
    const std::size_t depth    = rig.undo.undo_depth();

    // Unknown layer.
    auto missing = rig.bus.execute_line("STİL katman=YOKBOYLE", Origin::Test);
    REQUIRE(!missing.ok());
    CHECK(missing.error().message.find("YOKBOYLE") != std::string::npos);

    // A code with no package to read it from.
    auto orphan = rig.bus.execute_line("STİL katman=PARSEL kod=K", Origin::Test);
    REQUIRE(!orphan.ok());
    CHECK(orphan.error().message.find("paket") != std::string::npos);

    // A package that is not there.
    auto absent =
        rig.bus.execute_line("STİL katman=PARSEL paket=/yok/boyle/paket.json", Origin::Test);
    REQUIRE(!absent.ok());
    CHECK(absent.error().code == core::ErrorCode::IoFailure);

    // A code that the package does not carry: the failure happens while deciding,
    // before a single style is written.
    const fs::path package = fixture_file();
    auto bad_code          = rig.bus.execute_line(
        "STİL katman=PARSEL paket=\"" + package.string() + "\" kod=YOKBOYLE", Origin::Test);
    REQUIRE(!bad_code.ok());

    CHECK_EQ(rig.doc.content_hash(), before);
    CHECK_EQ(rig.undo.undo_depth(), depth);
    CHECK_EQ(style_column(rig.doc), all_by_layer(rig.doc));
}

// ------------------------------------------------- the shipped data package ----

TEST_CASE("VERİ: sevk edilen plan gösterim paketi künyesiyle birlikte yüklenir")
{
    const fs::path path = fs::path{PIRICAD_DATA_DIR} / "catalogs" / "mpyy" / "plan-gosterim.json";
    REQUIRE(fs::exists(path));

    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();

    auto catalog = parse_catalog(buffer.str());
    REQUIRE(catalog.ok());

    // data.md R2 — the header is what makes a data release auditable.
    CHECK_EQ(catalog.value().id(), std::string{"mpyy-plan-gosterimleri"});
    CHECK(!catalog.value().package_version().empty());
    CHECK(!catalog.value().source().empty());
    CHECK(!catalog.value().published().empty());
    CHECK(!catalog.value().licence().empty());

    // The rows have landed: they are extracted from the official annexes by
    // scripts/mpyy-cikar.py. This assertion is deliberately a THRESHOLD and not a
    // count, and it names no colour, code or width — a regulatory value may not
    // live in C++ (CLAUDE.md 5.13). The exact row count and the reference values
    // are asserted against the shipped package by scripts/ci-gate-mpyy.sh, which
    // reads them from /tests/golden/mpyy, where a data change is reviewed as data.
    CHECK(catalog.value().entries().size() > 0);

    // The mapping rules stay empty on purpose: which feature earns which row is
    // not stated by the annex, and a guessed rule paints a signed plan wrong.
    CHECK_EQ(catalog.value().rules().size(), std::size_t{0});
}

// -------------------------------------------------- categorized rendering ----

TEST_CASE("FeatureView öznitelik satırını sınıflandırmaya taşır")
{
    // The link that was missing. The rule engine could test a field; nothing put
    // a real attribute row in front of it, because Document had no columns. It
    // has them now, and from_row is the bridge the style command uses.
    core::AttrTable table;
    core::AttrSpec spec;
    spec.id   = "gosterim";
    spec.type = core::AttrType::Text;
    REQUIRE(table.add(spec).ok());
    table.resize(2);
    REQUIRE(table.set(0, 0, core::attr_text("TOPLU KONUT ALANI")).ok());

    const core::FeatureView row = core::FeatureView::from_row(table, 0);
    const core::AttrValue* seen = row.find("gosterim");
    REQUIRE(seen != nullptr);
    CHECK_EQ(seen->text, std::string("TOPLU KONUT ALANI"));

    // A condition written against that field now matches a real parcel.
    core::StyleCondition cond;
    cond.field = "gosterim";
    cond.test  = core::StyleCondition::Test::Equals;
    cond.values.push_back("TOPLU KONUT ALANI");
    CHECK(cond.matches(row));

    // And an untagged row matches nothing — no silent default (data.md R6).
    CHECK(!cond.matches(core::FeatureView::from_row(table, 1)));
}

TEST_CASE("Sınıflandırma: aynı katmandaki iki nesne farklı stil alır")
{
    // The whole point of a categorized renderer, stated as an assertion: layer
    // membership does not decide appearance, the entity's own data does.
    core::AttrTable table;
    core::AttrSpec spec;
    spec.id   = "gosterim";
    spec.type = core::AttrType::Text;
    REQUIRE(table.add(spec).ok());
    table.resize(2);
    REQUIRE(table.set(0, 0, core::attr_text("ORGANİZE SANAYİ BÖLGESİ")).ok());
    REQUIRE(table.set(0, 1, core::attr_text("SERBEST BÖLGE")).ok());

    core::StyleCondition sanayi;
    sanayi.field = "gosterim";
    sanayi.test  = core::StyleCondition::Test::Equals;
    sanayi.values.push_back("ORGANİZE SANAYİ BÖLGESİ");

    const core::FeatureView a = core::FeatureView::from_row(table, 0);
    const core::FeatureView b = core::FeatureView::from_row(table, 1);
    CHECK(sanayi.matches(a));
    CHECK(!sanayi.matches(b));
}

TEST_CASE("Sınıflandırma sayısal aralıkla da çalışır — kademeli çizici")
{
    // A graduated renderer is the Range test over a numeric column, and it needs
    // no new machinery: a five-band housing `lekesi` graded by population density is
    // rules with five windows.
    core::AttrTable table;
    core::AttrSpec spec;
    spec.id   = "yogunluk";
    spec.type = core::AttrType::Int64;
    REQUIRE(table.add(spec).ok());
    table.resize(3);
    REQUIRE(table.set(0, 0, core::attr_int64(80)).ok());
    REQUIRE(table.set(0, 1, core::attr_int64(250)).ok());
    REQUIRE(table.set(0, 2, core::attr_int64(600)).ok());

    core::StyleCondition orta;
    orta.field    = "yogunluk";
    orta.test     = core::StyleCondition::Test::Range;
    orta.low      = 100;
    orta.high     = 400;
    orta.has_low  = true;
    orta.has_high = true;

    CHECK(!orta.matches(core::FeatureView::from_row(table, 0)));
    CHECK(orta.matches(core::FeatureView::from_row(table, 1)));
    CHECK(!orta.matches(core::FeatureView::from_row(table, 2)));
}

// ------------------------------------------------------- the symbol shelf ----

TEST_CASE("SEMBOL: MPYY paketi kendi ağacıyla rafa giriyor")
{
    // The tree is the REGULATION's, not ours. MPYY EK-1 files its rows by annex
    // and then by a section path, and the shelf indexes that path rather than
    // inventing a taxonomy (CLAUDE.md 5.13).
    const std::string path = std::string(PIRICAD_DATA_DIR) + "/catalogs/mpyy/plan-gosterim.json";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());

    std::ostringstream buffer;
    buffer << in.rdbuf();
    auto parsed = core::Json::parse(buffer.str());
    REQUIRE(parsed.ok());

    auto catalog = core::StyleCatalog::from_json(parsed.value());
    if (!catalog) FAIL_WITH("katalog", catalog.error().message);
    REQUIRE(catalog.ok());

    core::StyleLibrary shelf;
    const std::size_t added = shelf.add_catalog(catalog.value());
    CHECK(added >= std::size_t{400});
    CHECK_EQ(shelf.size(), added);

    // Five annexes at the top, each named the way the package names it.
    const std::vector<std::string> root;
    const auto top = shelf.children(root);
    CHECK_EQ(top.size(), std::size_t{5});
    CHECK(top[0].find("EK-1a") != std::string::npos);

    // One level down is the section list, and two levels down are rows.
    const std::vector<std::string> annex{top[0]};
    CHECK(!shelf.children(annex).empty());

    // Turkish folding on BOTH sides: a lower-case query finds an upper-case
    // label. `std::tolower` cannot do this and CLAUDE.md 5.6 bans it outright.
    const auto hits = shelf.search("orman");
    CHECK(!hits.empty());
    bool found_upper = false;
    for (const core::LibraryEntry* e : hits)
        if (e->label.find("ORMAN") != std::string::npos) found_upper = true;
    CHECK(found_upper);

    // A group nobody declared is empty, not an error: a shelf can be asked about
    // a drawer that does not exist.
    const std::vector<std::string> nowhere{"BÖYLE BİR EK YOK"};
    CHECK(shelf.in_group(nowhere).empty());
    CHECK(shelf.children(nowhere).empty());
}

TEST_CASE("SEMBOL: aynı kimlik yerinde değişir, rafın sonuna eklenmez")
{
    // A newer package version restating a row is an update to that row. Appending
    // instead would leave two entries under one id and make `find` depend on which
    // one was asked for.
    core::StyleLibrary shelf;

    core::LibraryEntry first;
    first.id    = "x";
    first.label = "ESKİ";
    first.group = {"A"};
    shelf.add(first);

    core::LibraryEntry other;
    other.id    = "y";
    other.label = "ÖTEKİ";
    other.group = {"A"};
    shelf.add(other);

    core::LibraryEntry updated;
    updated.id    = "x";
    updated.label = "YENİ";
    updated.group = {"A"};
    shelf.add(updated);

    CHECK_EQ(shelf.size(), std::size_t{2});
    REQUIRE(shelf.find("x") != nullptr);
    CHECK_EQ(shelf.find("x")->label, std::string("YENİ"));

    // ...and in its original position, so the shelf's order is the package's.
    const std::vector<std::string> group{"A"};
    const auto rows = shelf.in_group(group);
    REQUIRE(rows.size() == std::size_t{2});
    CHECK_EQ(rows[0]->id, std::string("x"));
}

// --------------------------------------------------------------- ETİKET ----

TEST_CASE("ETİKET: parselin kendi öznitelikleri kendi ortasına yazılıyor")
{
    // The case this command exists for: MPYY prints TAKS and KAKS inside a circle
    // on a `yapılaşma koşulu` island. The circle is the layer's own symbol; the
    // values are the feature's attributes; this is what carries one to the other.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=taks tur=metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=cephe tur=uzunluk", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 40000,0 40000,30000 0,30000", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK taks 1 \"0,30\"", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK cephe 1 12500", Origin::Test).ok());

    const std::size_t before = f.doc.live_entity_count();

    auto labelled = f.bus.execute_line(
        "ETİKET katman=PARSEL bicim=\"TAKS {taks} · Cephe {cephe} m\"", Origin::Test);
    if (!labelled) FAIL_WITH("ETİKET", labelled.error().message);
    REQUIRE(labelled.ok());

    // One label entity, on its own layer so a sheet can be plotted without them.
    CHECK_EQ(f.doc.live_entity_count(), before + 1);
    const core::LayerId target = f.doc.find_layer("PARSEL ETİKET");
    REQUIRE(target != core::kNoLayer);

    // A `uzunluk` column is stored in millimetres and printed in METRES, because
    // that is the unit a plan sheet writes.
    bool found = false;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.entities().alive(e) || f.doc.entities().layer[e] != target) continue;
        const std::string_view text = f.doc.texts().text(f.doc.entities().slot[e]);
        CHECK_EQ(std::string(text), std::string("TAKS 0,30 · Cephe 12,5 m"));
        found = true;
    }
    CHECK(found);

    // One undo step takes every label back together.
    REQUIRE(f.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(f.doc.live_entity_count(), before);
}

TEST_CASE("ETİKET: tanınmayan sütun adı silinmiyor, görünür kalıyor")
{
    // Silently deleting it would hide a typo in a format string that a plan sheet
    // is about to be printed from.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=ada tur=tam_sayi", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 40000,0 40000,30000 0,30000", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK ada 1 1234", Origin::Test).ok());

    REQUIRE(
        f.bus.execute_line("ETİKET katman=PARSEL bicim=\"{ada}/{parsell}\"", Origin::Test).ok());

    const core::LayerId target = f.doc.find_layer("PARSEL ETİKET");
    REQUIRE(target != core::kNoLayer);

    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.entities().alive(e) || f.doc.entities().layer[e] != target) continue;
        CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])),
                 std::string("1234/{parsell}"));
    }
}

TEST_CASE("STİL: MPYY yapılaşma koşulu göstermi sıfırdan kurulabiliyor")
{
    // The point this proves: the designer's own parts are enough to PRODUCE the
    // published gösterim, not only to apply its picture. MPYY prints a circle with
    // a horizontal rule, TAKS above it and KAKS below, and each parcel's own two
    // numbers stacked around the rule.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 70000,0 70000,52000 0,52000", Origin::Test).ok());

    const std::array<const char*, 6> stack{
        "STİL katman=PARSEL tip=dolgu dolgu=584376224",
        "STİL katman=PARSEL ekle=evet tip=cizgi renk=4282203457 kalinlik=500",
        "STİL katman=PARSEL ekle=evet tip=merkez-isaretci sekil=daire birim=zemin boyut=26000",
        "STİL katman=PARSEL ekle=evet tip=merkez-isaretci sekil=cizik aci=90000000 "
        "birim=zemin boyut=22000",
        "STİL katman=PARSEL ekle=evet tip=yazi-isaretci yazi=TAKS birim=zemin boyut=2600 "
        "kaydirma=9500",
        "STİL katman=PARSEL ekle=evet tip=yazi-isaretci yazi=KAKS birim=zemin boyut=2600 "
        "kaydirma=-9500"};

    for (const char* line : stack) {
        auto applied = f.bus.execute_line(line, Origin::Test);
        if (!applied) FAIL_WITH(line, applied.error().message);
        REQUIRE(applied.ok());
    }

    const core::StyleId sid = f.doc.entities().style[0];
    REQUIRE(sid != core::kByLayerStyle);

    const core::Symbol sym = f.doc.styles().symbol_at(sid);
    REQUIRE(sym.layers.size() == std::size_t{6});

    // The rule is the one turned on its side; without the rotation reaching the
    // renderer it cut the two numbers in half instead of separating them.
    CHECK(sym.layers[3].shape == core::MarkerShape::Tick);
    CHECK_EQ(sym.layers[3].angle_udeg, 90000000);

    // The two words belong to the SYMBOL, one above the centre and one below.
    CHECK(sym.layers[4].type == core::SymbolLayerType::TextMarker);
    CHECK_EQ(sym.layers[4].text, std::string("TAKS"));
    CHECK_EQ(sym.layers[4].offset.value, 9500);
    CHECK(sym.layers[4].offset.unit == core::Unit::Ground);
    CHECK_EQ(sym.layers[5].text, std::string("KAKS"));
    CHECK_EQ(sym.layers[5].offset.value, -9500);
}

TEST_CASE("STİL: çizgi tipi bir desendir, resim değil")
{
    // The line MPYY prints for a province boundary is a dash, a gap, a dot and a
    // gap — four numbers. Carried as a picture it could not be recoloured, could
    // not be written into DWG as a line type, and above all could not TURN A
    // CORNER: a
    // stamped rectangle rotates to one edge and leaves a wedge of nothing on the
    // outside of every bend. Carried as a pattern the renderer joins it.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 60,0 60,45 0,45", Origin::Test).ok());

    auto applied = f.bus.execute_line("STİL katman=PARSEL tip=cizgi kalinlik=500 desen=\"8 1 1 1\"",
                                      Origin::Test);
    if (!applied) FAIL_WITH("desenli STİL", applied.error().message);
    REQUIRE(applied.ok());

    // The pattern reached the drawing's own store, in hundredths of a stroke width.
    REQUIRE(f.doc.dashes().size() == std::size_t{2}); // the solid sentinel, and this
    const core::DashPattern& p = f.doc.dashes().at(1);
    CHECK_EQ(static_cast<int>(p.count), 4);
    CHECK_EQ(static_cast<int>(p.lengths[0]), 800);
    CHECK_EQ(static_cast<int>(p.lengths[1]), 100);
    CHECK_EQ(static_cast<int>(p.lengths[2]), 100);
    CHECK_EQ(static_cast<int>(p.lengths[3]), 100);

    // ...and the entities point at it.
    const core::StyleId sid = f.doc.entities().style[0];
    REQUIRE(sid != core::kByLayerStyle);
    const core::Symbol sym = f.doc.styles().symbol_at(sid);
    REQUIRE(!sym.layers.empty());
    CHECK_EQ(static_cast<int>(sym.layers.front().look.dash), 1);

    // Interning is deduplicated: the same pattern twice is one entry.
    REQUIRE(f.bus
                .execute_line("STİL katman=PARSEL tip=cizgi kalinlik=700 desen=\"8 1 1 1\"",
                              Origin::Test)
                .ok());
    CHECK_EQ(f.doc.dashes().size(), std::size_t{2});

    // `sürekli` is the solid stroke, and it is not a new entry either.
    REQUIRE(f.bus.execute_line("STİL katman=PARSEL tip=cizgi desen=sürekli", Origin::Test).ok());
    CHECK_EQ(f.doc.dashes().size(), std::size_t{2});
    const core::Symbol solid = f.doc.styles().symbol_at(f.doc.entities().style[0]);
    CHECK_EQ(static_cast<int>(solid.layers.front().look.dash), 0);
}

TEST_CASE("STİL: bozuk bir desen sessizce düz çizgiye dönmüyor")
{
    // Each of these is a catalogue or a typing defect, and drawing something
    // plausible instead would hide it. A boundary silently drawn solid is a
    // different legal statement from the one that was asked for.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60,0", Origin::Test).ok());

    // An odd number of parts leaves a mark with no gap after it.
    const auto odd =
        f.bus.execute_line("STİL katman=PARSEL tip=cizgi desen=\"8 1 1\"", Origin::Test);
    REQUIRE(!odd);
    CHECK(odd.error().message.find("çift") != std::string::npos);

    // More parts than the store can hold.
    const auto many = f.bus.execute_line(
        "STİL katman=PARSEL tip=cizgi desen=\"1 1 1 1 1 1 1 1 1 1\"", Origin::Test);
    REQUIRE(!many);

    // Something that is not a number at all.
    const auto words =
        f.bus.execute_line("STİL katman=PARSEL tip=cizgi desen=\"uzun kısa\"", Origin::Test);
    REQUIRE(!words);
    CHECK(words.error().message.find("okunamayan") != std::string::npos);

    // None of them touched the drawing (§2.5).
    CHECK_EQ(style_column(f.doc), all_by_layer(f.doc));
    CHECK_EQ(f.doc.dashes().size(), std::size_t{1});
}

TEST_CASE("STİL: her ölçü kendi birimini taşıyabiliyor")
{
    // The defect this pins down: the style designer shows a unit combo beside
    // every measure, and the command had one `birim` for all four. A marker sized
    // on the ground and repeated at a paper interval — how a boundary glyph is
    // specified — came back with both measures in whichever unit the size was in,
    // so the applied symbol was not the symbol the dialog displayed.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60000,0", Origin::Test).ok());

    auto applied = f.bus.execute_line(
        "STİL katman=PARSEL tip=isaretci-cizgi sekil=daire birim=zemin boyut=26000 "
        "aralik=1500 aralik_birim=kagit kaydirma=400 kaydirma_birim=piksel",
        Origin::Test);
    if (!applied) FAIL_WITH("karma birimli STİL", applied.error().message);
    REQUIRE(applied.ok());

    const core::StyleId sid = f.doc.entities().style[0];
    REQUIRE(sid != core::kByLayerStyle);

    const core::Symbol sym = f.doc.styles().symbol_at(sid);
    REQUIRE(sym.layers.size() == std::size_t{1});

    const core::SymbolLayer& sl = sym.layers.front();

    // `birim` still governs the measure that names no unit of its own.
    CHECK_EQ(sl.size.value, 26000);
    CHECK(sl.size.unit == core::Unit::Ground);

    // ...and the two that do keep theirs.
    CHECK_EQ(sl.interval.value, 1500);
    CHECK(sl.interval.unit == core::Unit::Paper);
    CHECK_EQ(sl.offset.value, 400);
    CHECK(sl.offset.unit == core::Unit::Pixel);
}

TEST_CASE("STİL: tanınmayan ölçü birimi sessizce başka bir birime dönmüyor")
{
    // Silently falling back to `birim` would be the same defect wearing a
    // different hat: the drawing would carry a measure the user never wrote.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÇİZGİ 0,0 60000,0", Origin::Test).ok());

    const auto refused = f.bus.execute_line(
        "STİL katman=PARSEL tip=cizgi boyut=100 boyut_birim=fersah", Origin::Test);
    REQUIRE(!refused);
    CHECK(refused.error().message.find("fersah") != std::string::npos);
    CHECK(refused.error().message.find("boyut_birim") != std::string::npos);

    // And it left the drawing alone (§2.5).
    CHECK_EQ(style_column(f.doc), all_by_layer(f.doc));
}

TEST_CASE("ETİKET: MPYY yapılaşma koşulu, her parselin KENDİ sayılarıyla")
{
    // The symbol the regulation prints for a building condition is a circle with
    // a horizontal rule across it: the floor area ratio above the rule and the
    // building coverage ratio below. Both figures are ATTRIBUTES of the parcel, so
    // two parcels wear the same symbol and show different numbers.
    //
    // The split between the two halves is model.md R29 and P7: attribute columns
    // are never read by the frame path and no expression is evaluated inside it.
    // The SYMBOL draws the circle and the rule — the same for every parcel, one
    // entry in the style column. The COMMAND writes the numbers, as text entities
    // that carry, move, snap and export like any other object.
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN IMAR", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=taks tur=metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=kaks tur=metin", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ALAN 0,0 70,0 70,52 0,52", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 80,0 150,0 150,52 80,52", Origin::Test).ok());

    REQUIRE(f.bus.execute_line("ÖZNİTELİK taks 1 \"0,40\"", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK kaks 1 \"1,20\"", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK taks 2 \"0,30\"", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK kaks 2 \"0,90\"", Origin::Test).ok());

    REQUIRE(f.bus
                .execute_line("STİL katman=IMAR tip=merkez-isaretci sekil=daire birim=zemin "
                              "boyut=26000",
                              Origin::Test)
                .ok());
    REQUIRE(f.bus
                .execute_line("STİL katman=IMAR ekle=evet tip=merkez-isaretci sekil=cizik "
                              "aci=90000000 birim=zemin boyut=22000",
                              Origin::Test)
                .ok());

    // ONE STYLE for both parcels: the circle and the rule say nothing about a
    // particular parcel, so they cost one entry in the style column however many
    // parcels wear them.
    CHECK_EQ(f.doc.entities().style[0], f.doc.entities().style[1]);

    // The two figures, each with its own offset from the parcel's centre. Without
    // an offset both land on the rule between them, which is what this case was
    // written after seeing.
    auto up = f.bus.execute_line(
        "ETİKET katman=IMAR bicim=\"{kaks}\" hedef=KOSUL_UST yukseklik=3200 kaydirma=4500",
        Origin::Test);
    if (!up) FAIL_WITH("üst etiket", up.error().message);
    REQUIRE(up.ok());

    REQUIRE(f.bus
                .execute_line("ETİKET katman=IMAR bicim=\"{taks}\" hedef=KOSUL_ALT "
                              "yukseklik=3200 kaydirma=-7000",
                              Origin::Test)
                .ok());

    // Two parcels, two labels each.
    std::vector<std::string> written;
    std::vector<core::Mm> heights;
    const auto& entities = f.doc.entities();
    for (core::EntityId e = 0; e < entities.size(); ++e) {
        if (!entities.alive(e)) continue;
        const std::string_view text = f.doc.texts().text(entities.slot[e]);
        if (text.empty()) continue;
        written.emplace_back(text);

        // The baseline's own y, which is where the label was placed.
        const core::RingSpan span = f.doc.geometry().rings_of(entities.slot[e]);
        heights.push_back(f.doc.geometry().ring_ys(span.first)[0]);
    }
    CHECK_EQ(written.size(), std::size_t{4});

    // Each parcel says its OWN numbers, not the layer's.
    const auto has = [&](const char* what) {
        return std::find(written.begin(), written.end(), std::string(what)) != written.end();
    };
    CHECK(has("1,20"));
    CHECK(has("0,40"));
    CHECK(has("0,90"));
    CHECK(has("0,30"));

    // ...and the floor area figure sits ABOVE the coverage one.
    REQUIRE(heights.size() == std::size_t{4});
    const core::Mm highest = *std::max_element(heights.begin(), heights.end());
    const core::Mm lowest  = *std::min_element(heights.begin(), heights.end());
    CHECK(highest > lowest);
}

TEST_CASE("ETİKET: iki satır noktanın etrafına yığılıyor")
{
    Rig f;
    REQUIRE(f.bus.execute_line("KATMAN PARSEL", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=taks tur=metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("SÜTUN kimlik=kaks tur=metin", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ALAN 0,0 70000,0 70000,52000 0,52000", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK taks 1 \"0,30\"", Origin::Test).ok());
    REQUIRE(f.bus.execute_line("ÖZNİTELİK kaks 1 \"1,50\"", Origin::Test).ok());

    // The two-character escape is what lets a command line ask for two lines.
    REQUIRE(
        f.bus.execute_line("ETİKET katman=PARSEL bicim=\"{taks}\\n{kaks}\"", Origin::Test).ok());

    const core::LayerId target = f.doc.find_layer("PARSEL ETİKET");
    REQUIRE(target != core::kNoLayer);

    bool found = false;
    for (core::EntityId e = 0; e < f.doc.entities().size(); ++e) {
        if (!f.doc.entities().alive(e) || f.doc.entities().layer[e] != target) continue;
        CHECK_EQ(std::string(f.doc.texts().text(f.doc.entities().slot[e])),
                 std::string("0,30\n1,50"));
        found = true;
    }
    CHECK(found);
}

TEST_CASE("SEMBOL: vektör paketinin her satırı katmanlarıyla rafa giriyor")
{
    // The vector package is the one that has to SURVIVE a corner, a recolour and
    // a DWG export, so it is the one whose declared stacks are checked here. It
    // is loaded on top of the picture package in the shell, and a row it restates
    // replaces the picture — see main_window.cpp.
    const std::string path =
        std::string(PIRICAD_DATA_DIR) + "/catalogs/mpyy-vektor/plan-gosterim.json";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());

    std::ostringstream buffer;
    buffer << in.rdbuf();
    auto parsed = core::Json::parse(buffer.str());
    REQUIRE(parsed.ok());

    auto catalog = core::StyleCatalog::from_json(parsed.value());
    if (!catalog) FAIL_WITH("vektör kataloğu", catalog.error().message);
    REQUIRE(catalog.ok());

    // Every picture the package names, resolved to a DIFFERENT id per file so a
    // layer pointing at the wrong one is visible as a wrong id and not as a
    // coincidence.
    std::unordered_map<std::string, core::ImageId> minted;
    const auto resolve = [&minted](const std::string& file) -> core::ImageId {
        auto [it, fresh] = minted.emplace(file, core::ImageId{});
        if (fresh) it->second = core::ImageId{static_cast<std::uint32_t>(minted.size())};
        return it->second;
    };

    core::StyleLibrary shelf;
    const std::size_t added = shelf.add_catalog(catalog.value(), resolve);
    CHECK(added >= std::size_t{460});

    // WHAT THIS TEST IS FOR. A declared layer may name a picture — a cogwheel on
    // a boundary, a wave on a shoreline, a bolt inside a frame — and for a while
    // the parser read every other field of a layer and silently dropped that one.
    // The package parsed, the shelf filled, and the glyphs drew nothing.
    std::size_t with_image = 0;
    std::size_t layers     = 0;
    for (const core::StyleEntry& row : catalog.value().entries()) {
        for (const core::DeclaredLayer& d : row.layers) {
            ++layers;
            if (!d.image.empty()) ++with_image;
        }
    }
    CHECK(layers >= std::size_t{900});
    CHECK(with_image >= std::size_t{50});

    // And the id survives all the way onto the symbol the shelf hands out.
    std::size_t drawn = 0;
    for (const core::LibraryEntry& e : shelf.entries())
        for (const core::SymbolLayer& l : e.symbol.layers)
            if (l.image != core::kNoImage) ++drawn;
    CHECK(drawn >= with_image);
}
