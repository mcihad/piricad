// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings — .claude/model.md R38–R42.
//
// The subject of these tests is the SCOPE BOUNDARY, not the container. A setting
// in the wrong scope is a document that hashes differently on two machines, or a
// legal export that changes because someone switched to the dark theme. So every
// case here asks one of three questions: does the boundary hold, is an out-of-range
// value clamped and reported, and can a user find out why a value is what it is.
#include "piricad_test.hpp"

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/core/text.hpp"
#include "piricad/script/json_runner.hpp"

#include <cstdint>
#include <optional>
#include <string>

using namespace piricad;
using namespace piricad::core;

namespace {

bool mentions(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

/// A private catalogue, so a test can declare exactly the shape it is about
/// without waiting for the product to grow a setting of that shape.
SettingCatalog test_catalogue()
{
    SettingCatalog c;
    (void)c.add(SettingSpec{
        .id       = "test.proje.sayi",
        .names    = {"sayı", "sayi"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(10),
        .range    = SettingRange::between(0, 100),
        .values   = {},
        .unit     = "adet",
        .summary  = "Proje kapsamında bir tam sayı.",
        .section  = "Sınama",
    });
    (void)c.add(SettingSpec{
        .id       = "test.uygulama.bayrak",
        .names    = {"bayrak"},
        .type     = SettingType::Bool,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(false),
        .range    = SettingRange::between(0, 1),
        .values   = {},
        .unit     = "",
        .summary  = "Uygulama kapsamında bir anahtar.",
        .section  = "Sınama",
    });
    (void)c.add(SettingSpec{
        .id       = "test.oturum.gecici",
        .names    = {"geçici", "gecici"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Session,
        .fallback = SettingValue::integer(0),
        .range    = SettingRange::between(0, 9),
        .values   = {},
        .unit     = "",
        .summary  = "Oturum kapsamında bir sayı.",
        .section  = "Sınama",
    });
    return c;
}

struct Rig
{
    Document doc;
    command::Registry reg;
    command::Journal journal;
    command::UndoStack undo;
    command::Bus bus{doc, reg, journal, undo};
    std::string echoed;

    Rig()
    {
        command::register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { echoed.append(s).append("\n"); };
    }

    core::Status line(const std::string& text)
    {
        echoed.clear();
        auto r = bus.execute_line(text, command::Origin::Test);
        if (!r) return r.error();
        return core::ok();
    }
};

} // namespace

// ------------------------------------------------------------- the specs ----

TEST_CASE("SettingSpec: her bildirim eksiksiz ve kataloğa kabul edilmiş")
{
    const SettingCatalog& cat = builtin_settings();
    CHECK(cat.size() == 55); // her X-makro satırı kabul edildi

    for (const auto& spec : cat.all()) {
        CHECK(!spec.id.empty());
        CHECK(!spec.names.empty());
        CHECK(!spec.summary.empty());
        CHECK(spec.fallback.type() == spec.type);
        // R38: the declaration is the whole truth, so a default it cannot honour is
        // a defect and SettingCatalog::add must already have refused it.
        if (spec.type != SettingType::Text) CHECK(spec.range.contains(spec.fallback.scalar()));
        if (spec.type == SettingType::Enum) CHECK(spec.fallback.as_enum() < spec.values.size());
    }
}

TEST_CASE("R40: dışa aktarılan belgenin baytını değiştiren her ayar proje kapsamında")
{
    const SettingCatalog& cat = builtin_settings();

    // std::optional, not a bare at(): SettingCatalog::at indexes its vector, and
    // cat.find() returns kNoSetting (0xFFFFFFFF) for an id that has been renamed
    // or removed — so an unguarded at(find(id)) reads specs_[0xFFFFFFFF]. A
    // missing id must FAIL this case, not corrupt the run that reports it.
    const auto scope_of = [&](const char* id) -> std::optional<SettingScope> {
        const std::uint32_t index = cat.find(id);
        if (index == kNoSetting) {
            FAIL_WITH("bildirilmemiş ayar kimliği", id);
            return std::nullopt;
        }
        return cat.at(index).scope;
    };

    // These five reach the paper, the koordinat cetveli or the file. R40 is applied
    // literally: display precision LOOKS like a preference and is not one.
    CHECK(scope_of("core.crs.id") == SettingScope::Project);
    CHECK(scope_of("core.crs.hassasiyet") == SettingScope::Project);
    CHECK(scope_of("core.cizim.birim") == SettingScope::Project);
    CHECK(scope_of("core.cizim.cizgi_tipi_olcegi") == SettingScope::Project);

    // The plot scale turns every paper measure into a ground one, the angle unit
    // decides whether 100 is a right angle or a bit over a quarter of one, and the
    // area unit decides whether a parcel is 1200 or 1,2. All three are statements
    // about how the DOCUMENT's own numbers are read, so all three travel with it.
    CHECK(scope_of("core.plan.olcek") == SettingScope::Project);
    CHECK(scope_of("core.aci.birim") == SettingScope::Project);
    CHECK(scope_of("core.alan.birim") == SettingScope::Project);

    // ...and the aids are not. A ruler, a snap marker and a north arrow change
    // what the screen shows and not one byte of what is exported.
    for (const char* aid :
         {"core.yakalama.uzanti_carpani", "core.yakalama.isaret_boyu", "core.yakalama.isaret_rengi",
          "core.yakalama.ipucu", "core.izgara.renk", "core.izgara.ana_renk", "core.izgara.adim_y",
          "core.cetvel.gorunur", "core.cetvel.kalinlik", "core.cetvel.birim",
          "core.harita.olcek_cubugu", "core.harita.kuzey_oku", "core.harita.koordinat_gostergesi",
          "core.harita.imlec", "core.harita.imlec_boyu", "core.harita.yakinlastirma_adimi",
          "core.harita.tekerlek_ters", "core.secim.renk", "core.secim.vurgu_renk"})
        CHECK(scope_of(aid) == SettingScope::App);
    CHECK(scope_of("core.cizim.metin_yuksekligi") == SettingScope::Project);
    CHECK(scope_of("core.katalog.paket_surumu") == SettingScope::Project);
    // R40 applied literally: a tolerance makes two corners one point, which changes
    // the coordinate an ifraz produces, which changes a byte of the tapu. It cannot
    // be a per-machine preference.
    CHECK(scope_of("core.topoloji.dugum_toleransi") == SettingScope::Project);
    CHECK(scope_of("core.topoloji.en_kucuk_alan") == SettingScope::Project);

    // These reach the screen and the user's machine, and nothing else.
    CHECK(scope_of("core.arayuz.tema") == SettingScope::App);
    CHECK(scope_of("core.arayuz.dil") == SettingScope::App);
    CHECK(scope_of("core.dosya.otomatik_kayit") == SettingScope::App);
    CHECK(scope_of("core.dosya.son_dosya_sayisi") == SettingScope::App);
    CHECK(scope_of("core.tuval.arkaplan") == SettingScope::App);
    CHECK(scope_of("core.izgara.gorunur") == SettingScope::App);
    CHECK(scope_of("core.izgara.mod") == SettingScope::App);
    CHECK(scope_of("core.izgara.adim") == SettingScope::App);
    CHECK(scope_of("core.izgara.ana_cizgi") == SettingScope::App);
    // In pixels, not ground metres: it belongs to the user's hand and screen.
    CHECK(scope_of("core.yakalama.tolerans") == SettingScope::App);
    CHECK(scope_of("core.secim.tolerans") == SettingScope::App);

    // R43: input aids are not document state.
    CHECK(scope_of("core.yakalama.modlar") == SettingScope::Session);
    CHECK(scope_of("core.yakalama.dik_mod") == SettingScope::Session);
    CHECK(scope_of("core.yakalama.kutupsal_aci") == SettingScope::Session);
    CHECK(scope_of("core.yakalama.izgara") == SettingScope::Session);

    // The list above is a snapshot: it locks today's named answers but applies
    // R40 to nothing new, so a later setting gets no scrutiny from it. This
    // does: every spec's summary must state WHY its scope is what it is, so the
    // R40 answer is written down where the reviewer of the new spec sees it.
    for (const auto& spec : cat.all()) {
        // The summary must NAME the scope it claims — "proje", "uygulama" or
        // "oturum" — so the sentence a reviewer reads is the R40 answer and not a
        // description of the value. Every one already does; the next one
        // cannot be added without writing its answer down.
        if (spec.summary.find(setting_scope_label(spec.scope)) == std::string::npos)
            FAIL_WITH("R40 gerekçesi özet metninde yazılmamış: özet kapsamı "
                      "adlandırmıyor",
                      spec.id + " (" + setting_scope_label(spec.scope) + ")");
    }
}

TEST_CASE("R35: veri paketi sürümünün kullanılabilir bir varsayılanı yoktur")
{
    // R35 makes the package version stamp a CRITICAL part of the document: a file
    // whose regulatory basis is unknown must not open silently. It used to fall
    // back to "0.1.0", so a document that recorded NOTHING reported a plausible
    // version and only is_explicit() could tell the two apart — which no caller
    // checked. An empty value is the visible statement "dayanağı bilinmiyor".
    const SettingCatalog& cat = builtin_settings();
    const std::uint32_t index = cat.find("core.katalog.paket_surumu");
    CHECK(index != kNoSetting);
    if (index == kNoSetting) return;

    CHECK(cat.at(index).fallback.as_text().empty());

    Settings project{cat, SettingScopeMask::Project};
    CHECK(!project.is_explicit("core.katalog.paket_surumu"));
    CHECK(project.get("core.katalog.paket_surumu").as_text().empty());

    // A document that DOES declare one is distinguishable from one that does not.
    auto stamped = parse_setting(cat.at(index), "2024.1");
    CHECK(stamped.ok());
    if (!stamped) return;
    CHECK(project.set("core.katalog.paket_surumu", stamped.value()).ok());
    CHECK(project.is_explicit("core.katalog.paket_surumu"));
    CHECK_EQ(std::string(project.get("core.katalog.paket_surumu").as_text()),
             std::string("2024.1"));
}

TEST_CASE("R21/P8: hiçbir ayar kayan nokta değil — ondalık istek bildirilmiş birimle karşılanır")
{
    const SettingCatalog& cat = builtin_settings();
    for (const auto& spec : cat.all())
        CHECK((spec.type == SettingType::Bool || spec.type == SettingType::Int ||
               spec.type == SettingType::Length || spec.type == SettingType::Text ||
               spec.type == SettingType::Enum));

    // "Çizgi tipi ölçeği 0,5 olsun" is expressed as 500 per-mille, not as a double.
    const SettingSpec& scale = cat.at(cat.find("core.cizim.cizgi_tipi_olcegi"));
    CHECK_EQ(scale.unit, std::string("‰"));

    auto half = parse_setting(scale, "500");
    CHECK(half.ok());
    CHECK_EQ(half.value().as_int(), std::int64_t{500});

    // A real fraction is refused rather than silently rounded: a fixed-point setting
    // has no half unit.
    auto fraction = parse_setting(scale, "0.5");
    CHECK(!fraction.ok());
    CHECK(mentions(fraction.error().message, "tam sayı"));

    // The command line renders its number token as "1000.000000"; that is the same
    // integer and must be accepted, or every documented example would fail.
    auto from_cli = parse_setting(scale, "1000.000000");
    CHECK(from_cli.ok());
    CHECK_EQ(from_cli.value().as_int(), std::int64_t{1000});
}

// ------------------------------------------------------------ the store -----

TEST_CASE("R39: kapsam kutusu yabancı kapsamı reddeder — proje kutusuna tercih yazılamaz")
{
    const SettingCatalog cat = test_catalogue();
    Settings project{cat, SettingScopeMask::Project};
    Settings app{cat, SettingScopeMask::App};

    auto wrong = project.set("test.uygulama.bayrak", SettingValue::boolean(true));
    CHECK(!wrong.ok());
    CHECK(mentions(wrong.error().message, "uygulama"));
    CHECK(mentions(wrong.error().message, "proje"));

    auto other_way = app.set("test.proje.sayi", SettingValue::integer(5));
    CHECK(!other_way.ok());
    CHECK(mentions(other_way.error().message, "proje"));

    // The right box accepts it, and nothing leaked into the wrong one.
    CHECK(project.set("test.proje.sayi", SettingValue::integer(5)).ok());
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{5});
    CHECK(!app.is_explicit("test.proje.sayi"));
}

TEST_CASE("R42: aralık dışı değer kırpılır, uyarı kaydedilir, dosya açılmaz olmaz")
{
    const SettingCatalog cat = test_catalogue();
    Settings project{cat, SettingScopeMask::Project};

    // Written by a version whose maximum was higher. It must open.
    auto change = project.set("test.proje.sayi", SettingValue::integer(1000));
    CHECK(change.ok());
    CHECK(change.value().clamped);
    CHECK_EQ(change.value().after.as_int(), std::int64_t{100});
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{100});

    CHECK_EQ(project.warnings().size(), std::size_t{1});
    CHECK_EQ(project.warnings().front().id, std::string("test.proje.sayi"));
    CHECK(mentions(project.warnings().front().message, "kırpıldı"));
    CHECK(mentions(project.warnings().front().message, "1000"));

    // The lower bound is clamped the same way, and warnings accumulate rather than
    // replace: two bad values in one file are two things the user must be told.
    CHECK(project.set("test.proje.sayi", SettingValue::integer(-7)).ok());
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{0});
    CHECK_EQ(project.warnings().size(), std::size_t{2});

    project.clear_warnings();
    CHECK(project.set("test.proje.sayi", SettingValue::integer(50)).ok());
    CHECK(project.warnings().empty()); // an in-range value warns about nothing
}

TEST_CASE("R42: seçenek listesi dışındaki indis de kırpılır")
{
    Settings app{builtin_settings(), SettingScopeMask::App};

    auto change = app.set("core.arayuz.tema", SettingValue::enumerated(7));
    CHECK(change.ok());
    CHECK(change.value().clamped);
    // "sistem, acik, koyu" — three options, so the last valid index is 2.
    CHECK_EQ(change.value().after.as_enum(), std::uint16_t{2});
    CHECK(mentions(app.warnings().front().message, "kırpıldı"));
}

TEST_CASE("Bilinmeyen ayar kimliği eyleme geçirilebilir bir mesajla reddedilir")
{
    Settings project{builtin_settings(), SettingScopeMask::Project};

    auto missing = project.lookup("koordinat_hasasiyeti"); // one letter short
    CHECK(!missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);
    CHECK(mentions(missing.error().message, "Bilinmeyen ayar"));
    CHECK(mentions(missing.error().message, "koordinat_hasasiyeti")); // what was typed
    CHECK(mentions(missing.error().message, "tanımlı ayar"));         // what was expected

    // set() must not invent a second wording for the same failure.
    auto write = project.set("koordinat_hasasiyeti", SettingValue::integer(4));
    CHECK(!write.ok());
    CHECK_EQ(write.error().message, missing.error().message);

    // A near miss gets a suggestion, because "check the manual" is not a fix.
    auto near = project.lookup("koordinat_");
    CHECK(!near.ok());
    CHECK(mentions(near.error().message, "Bunu mu demek istediniz"));
}

TEST_CASE("Tür uyuşmazlığı reddedilir — metin ayarına sayı yazılamaz")
{
    Settings project{builtin_settings(), SettingScopeMask::Project};

    auto wrong = project.set("core.crs.id", SettingValue::integer(5254));
    CHECK(!wrong.ok());
    CHECK(wrong.error().code == ErrorCode::InvalidArgument);
    CHECK(mentions(wrong.error().message, "metin"));
    CHECK(!project.is_explicit("core.crs.id"));
}

TEST_CASE("Metin ayarı kapasiteyi aşınca kırpılmaz, reddedilir")
{
    // A silently shortened CRS id is a WRONG CRS, and a TM30/TM33 mix-up is the
    // classic field blunder (model.md R36). Truncation is never the answer here.
    const std::string too_long(kSettingTextCapacity, 'A');
    auto v = SettingValue::text(too_long);
    CHECK(!v.ok());
    CHECK(mentions(v.error().message, "bayt"));

    const std::string longest(kSettingTextCapacity - 1, 'A');
    auto fits = SettingValue::text(longest);
    CHECK(fits.ok());
    CHECK_EQ(std::string(fits.value().as_text()), longest);
}

TEST_CASE("Varsayılan mı, açıkça ayarlanmış mı — ikisi ayrı sorular")
{
    const SettingCatalog cat = test_catalogue();
    Settings project{cat, SettingScopeMask::Project};

    CHECK(project.is_default("test.proje.sayi"));
    CHECK(!project.is_explicit("test.proje.sayi"));
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{10});

    // Pinning a value that happens to equal the default is still a decision the
    // user made, and the file records it.
    CHECK(project.set("test.proje.sayi", SettingValue::integer(10)).ok());
    CHECK(project.is_default("test.proje.sayi"));
    CHECK(project.is_explicit("test.proje.sayi"));

    CHECK(project.set("test.proje.sayi", SettingValue::integer(42)).ok());
    CHECK(!project.is_default("test.proje.sayi"));
    CHECK(project.is_explicit("test.proje.sayi"));

    CHECK(project.reset("test.proje.sayi").ok());
    CHECK(project.is_default("test.proje.sayi"));
    CHECK(!project.is_explicit("test.proje.sayi"));
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{10});

    CHECK(!project.reset("yok_boyle_bir_ayar").ok());
}

TEST_CASE("SettingChange tek yazmayı tam olarak geri alır")
{
    const SettingCatalog cat = test_catalogue();
    Settings project{cat, SettingScopeMask::Project};

    // Undo of the FIRST write must remove the entry, not write the default back:
    // "there was no value here" and "the value here was 10" are different documents.
    auto first = project.set("test.proje.sayi", SettingValue::integer(42));
    CHECK(first.ok());
    CHECK(!first.value().was_explicit);
    CHECK_EQ(first.value().before.as_int(), std::int64_t{10});

    auto second = project.set("test.proje.sayi", SettingValue::integer(77));
    CHECK(second.ok());
    CHECK(second.value().was_explicit);
    CHECK_EQ(second.value().before.as_int(), std::int64_t{42});

    CHECK(project.revert(second.value()).ok());
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{42});
    CHECK(project.is_explicit("test.proje.sayi"));

    CHECK(project.revert(first.value()).ok());
    CHECK(!project.is_explicit("test.proje.sayi"));
    CHECK_EQ(project.get("test.proje.sayi").as_int(), std::int64_t{10});
}

// ------------------------------------------------------------- the hash -----

TEST_CASE("R39: fold yalnızca PROJE kapsamını sayar")
{
    const SettingCatalog cat = test_catalogue();
    Settings all{cat, SettingScopeMask::All};

    const std::uint64_t empty = all.fold(1469598103934665603ULL);

    CHECK(all.set("test.uygulama.bayrak", SettingValue::boolean(true)).ok());
    CHECK_EQ(all.fold(1469598103934665603ULL), empty); // App: not document content

    CHECK(all.set("test.oturum.gecici", SettingValue::integer(3)).ok());
    CHECK_EQ(all.fold(1469598103934665603ULL), empty); // Session: not document content

    CHECK(all.set("test.proje.sayi", SettingValue::integer(42)).ok());
    const std::uint64_t with_project = all.fold(1469598103934665603ULL);
    CHECK(with_project != empty); // Project: document content

    // P15 in spirit: a dark theme must not change a document's fingerprint.
    CHECK(all.set("test.uygulama.bayrak", SettingValue::boolean(false)).ok());
    CHECK_EQ(all.fold(1469598103934665603ULL), with_project);

    // And undoing the project write restores the fingerprint exactly.
    CHECK(all.reset("test.proje.sayi").ok());
    CHECK_EQ(all.fold(1469598103934665603ULL), empty);
}

TEST_CASE("fold yazma sırasına değil içeriğe bağlıdır")
{
    const SettingCatalog cat = test_catalogue();
    Settings a{cat, SettingScopeMask::All};
    Settings b{cat, SettingScopeMask::All};

    CHECK(a.set("test.proje.sayi", SettingValue::integer(7)).ok());
    CHECK(a.set("test.uygulama.bayrak", SettingValue::boolean(true)).ok());

    CHECK(b.set("test.uygulama.bayrak", SettingValue::boolean(true)).ok());
    CHECK(b.set("test.proje.sayi", SettingValue::integer(7)).ok());

    CHECK_EQ(a.fold(0), b.fold(0));

    // The value is folded, not just the id: 7 and 8 must not collide.
    CHECK(b.set("test.proje.sayi", SettingValue::integer(8)).ok());
    CHECK(a.fold(0) != b.fold(0));
}

TEST_CASE("explicit_ids kimliğe göre sıralı — her okuyucu aynı sırayı görür")
{
    const SettingCatalog cat = test_catalogue();
    Settings all{cat, SettingScopeMask::All};

    CHECK(all.set("test.oturum.gecici", SettingValue::integer(1)).ok());
    CHECK(all.set("test.proje.sayi", SettingValue::integer(2)).ok());
    CHECK(all.set("test.uygulama.bayrak", SettingValue::boolean(true)).ok());

    const auto ids = all.explicit_ids();
    CHECK_EQ(ids.size(), std::size_t{3});
    CHECK_EQ(ids[0], std::string("test.oturum.gecici"));
    CHECK_EQ(ids[1], std::string("test.proje.sayi"));
    CHECK_EQ(ids[2], std::string("test.uygulama.bayrak"));
}

// ------------------------------------------------------------- the names ----

TEST_CASE("Ayar adları Türkçe katlanır — ÇİZGİ_TİPİ_ÖLÇEĞİ ile cizgi_tipi_olcegi aynı ayar")
{
    const SettingCatalog& cat = builtin_settings();

    const std::uint32_t turkish = cat.find("çizgi_tipi_ölçeği");
    const std::uint32_t ascii   = cat.find("cizgi_tipi_olcegi");
    const std::uint32_t english = cat.find("ltscale");
    const std::uint32_t by_id   = cat.find("core.cizim.cizgi_tipi_olcegi");

    CHECK(turkish != kNoSetting);
    CHECK_EQ(turkish, ascii);
    CHECK_EQ(turkish, english);
    CHECK_EQ(turkish, by_id);

    // The dotted/dotless i trap: folding "dil" with the ASCII table would produce
    // "DIL", which is a different word. turkish_upper produces "DİL".
    CHECK(cat.find("DİL") == cat.find("dil"));
    CHECK(turkish_iequals("dil", "DİL"));
}

TEST_CASE("Aynı ad iki ayara verilemez")
{
    SettingCatalog c = test_catalogue();
    auto clash       = c.add(SettingSpec{
              .id       = "test.baska.sayi",
              .names    = {"sayi"}, // already taken by test.proje.sayi
              .type     = SettingType::Int,
              .scope    = SettingScope::Project,
              .fallback = SettingValue::integer(0),
              .range    = SettingRange::unbounded(),
              .values   = {},
              .unit     = "",
              .summary  = "Çakışan ad.",
              .section  = "Sınama",
    });
    CHECK(!clash.ok());
    CHECK(mentions(clash.error().message, "test.proje.sayi"));
}

TEST_CASE("Bildirimi tutarsız ayar kataloğa giremez")
{
    SettingCatalog c;

    // A default outside the declared range: the spec contradicts itself.
    auto bad_range = c.add(SettingSpec{
        .id       = "test.kotu.aralik",
        .names    = {"kotu_aralik"},
        .type     = SettingType::Int,
        .scope    = SettingScope::Project,
        .fallback = SettingValue::integer(500),
        .range    = SettingRange::between(0, 100),
        .values   = {},
        .unit     = "",
        .summary  = "Aralığın dışında varsayılan.",
        .section  = "Sınama",
    });
    CHECK(!bad_range.ok());

    // An enum with no options can never be satisfied.
    auto bad_enum = c.add(SettingSpec{
        .id       = "test.kotu.secenek",
        .names    = {"kotu_secenek"},
        .type     = SettingType::Enum,
        .scope    = SettingScope::App,
        .fallback = SettingValue::enumerated(0),
        .range    = SettingRange::between(0, 0),
        .values   = {},
        .unit     = "",
        .summary  = "Seçeneksiz seçenek.",
        .section  = "Sınama",
    });
    CHECK(!bad_enum.ok());

    // A default whose type disagrees with the declaration.
    auto bad_type = c.add(SettingSpec{
        .id       = "test.kotu.tur",
        .names    = {"kotu_tur"},
        .type     = SettingType::Int,
        .scope    = SettingScope::App,
        .fallback = SettingValue::boolean(true),
        .range    = SettingRange::unbounded(),
        .values   = {},
        .unit     = "",
        .summary  = "Türü tutmayan varsayılan.",
        .section  = "Sınama",
    });
    CHECK(!bad_type.ok());

    CHECK_EQ(c.size(), std::size_t{0});
}

// ------------------------------------------------------- format / parse -----

TEST_CASE("Her tür metne çevrilip geri okunur")
{
    const SettingCatalog& cat = builtin_settings();

    const auto round_trip = [&](const char* id, const char* typed, const char* shown) {
        const std::uint32_t index = cat.find(id);
        if (index == kNoSetting) {
            FAIL_WITH("bildirilmemiş ayar kimliği", id);
            return;
        }
        const SettingSpec& spec = cat.at(index);
        auto parsed             = parse_setting(spec, typed);
        CHECK(parsed.ok());
        if (!parsed.ok()) return;
        CHECK_EQ(format_setting(spec, parsed.value()), std::string(shown));
        // and the canonical form parses back to the same value
        auto again = parse_setting(spec, format_setting(spec, parsed.value()));
        CHECK(again.ok());
        CHECK(again.value() == parsed.value());
    };

    round_trip("core.yakalama.dik_mod", "evet", "evet");
    round_trip("core.yakalama.dik_mod", "HAYIR", "hayır");
    round_trip("core.crs.hassasiyet", "4", "4");
    round_trip("core.cizim.metin_yuksekligi", "3000", "3000");
    round_trip("core.crs.id", "TUREF/TM33", "TUREF/TM33");
    round_trip("core.cizim.birim", "metre", "metre");
    round_trip("core.arayuz.tema", "koyu", "koyu");

    // The database connection. Round-tripping through text is exactly what
    // `MainWindow::savePreferences` and `loadPreferences` do, so this is the
    // check that these four survive a restart. A host name with a dot in it is
    // the case worth naming: it must not be read as a number.
    round_trip("core.veritabani.sunucu", "sunucu.belediye.gov.tr", "sunucu.belediye.gov.tr");
    round_trip("core.veritabani.port", "5433", "5433");
    round_trip("core.veritabani.ad", "piricad", "piricad");
    round_trip("core.veritabani.kullanici", "harita", "harita");

    // A mask is readable in hexadecimal, which is how a user thinks about it.
    const SettingSpec& mask = cat.at(cat.find("core.yakalama.modlar"));
    auto hex                = parse_setting(mask, "0x1F");
    CHECK(hex.ok());
    CHECK_EQ(hex.value().as_int(), std::int64_t{31});

    // An option that is not on the list names the list.
    const SettingSpec& units = cat.at(cat.find("core.cizim.birim"));
    auto wrong               = parse_setting(units, "fersah");
    CHECK(!wrong.ok());
    CHECK(mentions(wrong.error().message, "milimetre, santimetre, metre"));
}

// ---------------------------------------------------------- via the bus -----

TEST_CASE("AYAR: proje ayarını yazar, okur ve nereden geldiğini söyler")
{
    Rig rig;

    CHECK(rig.line("AYAR koordinat_hassasiyeti varsayilan").ok());
    CHECK(rig.line("AYAR koordinat_hassasiyeti").ok());
    CHECK(mentions(rig.echoed, "varsayılan"));
    CHECK(mentions(rig.echoed, "core.crs.hassasiyet")); // the id, for a support call

    CHECK(rig.line("AYAR koordinat_hassasiyeti 5").ok());
    CHECK(mentions(rig.echoed, "koordinat_hassasiyeti = 5"));

    CHECK(rig.line("AYAR koordinat_hassasiyeti").ok());
    CHECK(mentions(rig.echoed, "açıkça ayarlandı"));
    CHECK(mentions(rig.echoed, "= 5"));

    // Back to the declared default, in a script, without Ctrl+Z.
    CHECK(rig.line("AYAR koordinat_hassasiyeti varsayilan").ok());
    CHECK(rig.line("AYAR koordinat_hassasiyeti").ok());
    CHECK(mentions(rig.echoed, "varsayılan"));
}

TEST_CASE("AYAR: argümansız çağrı proje ayarlarını listeler, tercihleri değil")
{
    Rig rig;
    CHECK(rig.line("AYAR").ok());

    CHECK(mentions(rig.echoed, "proje ayarları"));
    CHECK(mentions(rig.echoed, "koordinat_sistemi"));
    CHECK(!mentions(rig.echoed, "tema")); // an application preference is not listed here
}

TEST_CASE("R41: AYAR tercihe, TERCİH proje ayarına dokunamaz")
{
    Rig rig;

    CHECK(rig.line("AYAR tema koyu").ok()); // the command runs; the write is refused
    CHECK(mentions(rig.echoed, "uygulama"));
    CHECK(mentions(rig.echoed, "core.arayuz.tema"));

    CHECK(rig.line("TERCİH koordinat_hassasiyeti 2").ok());
    CHECK(mentions(rig.echoed, "proje"));

    // and the refusal actually refused: the value is still whatever AYAR set.
    CHECK(rig.line("AYAR koordinat_hassasiyeti 5").ok());
    CHECK(rig.line("AYAR koordinat_hassasiyeti").ok());
    CHECK(mentions(rig.echoed, "= 5"));
}

TEST_CASE("TERCİH: uygulama tercihini yazar ve okur")
{
    Rig rig;

    CHECK(rig.line("TERCIH tema koyu").ok()); // ASCII-folded alias
    CHECK(mentions(rig.echoed, "tema = koyu"));

    CHECK(rig.line("TERCİH tema").ok());
    CHECK(mentions(rig.echoed, "açıkça ayarlandı"));
    CHECK(mentions(rig.echoed, "seçenekler"));

    CHECK(rig.line("TERCIH tema varsayilan").ok());
}

TEST_CASE("R43: TERCİH günlüğe yazılmaz, AYAR yazılır")
{
    Rig rig;

    const std::size_t before = rig.journal.size();
    CHECK(rig.line("TERCIH otomatik_kayit 600").ok());
    CHECK_EQ(rig.journal.size(), before); // an application preference is not document state

    CHECK(rig.line("AYAR koordinat_hassasiyeti 4").ok());
    CHECK_EQ(rig.journal.size(), before + 1);

    // The journal records the canonical id, not the alias that was typed, so a
    // replay resolves the same setting whatever the user wrote.
    const auto& entry = rig.journal.entries().back();
    CHECK_EQ(entry.command_id, std::string("core.setting"));
    CHECK_EQ(entry.args.get("ad").as_text(), std::string("core.crs.hassasiyet"));
    CHECK_EQ(entry.args.get("deger").as_text(), std::string("4"));
}

TEST_CASE("AYAR: aralık dışı değer kırpılır ve kullanıcıya söylenir")
{
    Rig rig;

    CHECK(rig.line("AYAR koordinat_hassasiyeti 9").ok());
    CHECK(mentions(rig.echoed, "kırpıldı"));
    CHECK(mentions(rig.echoed, "= 6"));
}

TEST_CASE("AYAR: bilinmeyen ayar adı komutu düşürmez, yol gösterir")
{
    Rig rig;

    CHECK(rig.line("AYAR bilinmeyen_ayar 3").ok());
    CHECK(mentions(rig.echoed, "Bilinmeyen ayar"));
    CHECK(mentions(rig.echoed, "bilinmeyen_ayar"));
}

TEST_CASE("AYAR: geçersiz değer reddedilir, eski değer yerinde kalır")
{
    Rig rig;

    // The old value has to be a NON-default one, or the case cannot tell "the
    // rejection preserved what was there" from "nothing was ever written". This
    // used to set metre, which IS the declared default (enumerated(2)), so an
    // AYAR path that stored nothing at all passed.
    CHECK(rig.line("AYAR cizim_birimi santimetre").ok());
    CHECK(mentions(rig.echoed, "= santimetre"));
    CHECK(rig.bus.project_settings().is_explicit("core.cizim.birim"));

    CHECK(rig.line("AYAR cizim_birimi fersah").ok());
    CHECK(mentions(rig.echoed, "milimetre, santimetre, metre"));

    CHECK(rig.line("AYAR cizim_birimi").ok());
    CHECK(mentions(rig.echoed, "= santimetre"));
    CHECK_EQ(rig.bus.project_settings().get("core.cizim.birim").as_enum(), std::uint16_t{1});
}

TEST_CASE("R41: her Rig kendi ayar kutusunu taşır — komşu case bulaşmaz")
{
    // The two stores used to be function-local statics in commands/settings.cpp,
    // so every bus in the process shared one. That made the bus-level cases here
    // order-coupled: "R41" read back "= 5" from state a neighbouring case had
    // written, running a case in isolation changed its result, and a project CRS
    // set in one drawing leaked into the next File > New. The stores now belong to
    // the bus, so Rig construction is the isolation boundary.
    Rig first;
    CHECK(first.line("AYAR koordinat_hassasiyeti 6").ok());
    CHECK(first.bus.project_settings().is_explicit("core.crs.hassasiyet"));

    Rig second;
    CHECK(!second.bus.project_settings().is_explicit("core.crs.hassasiyet"));
    CHECK(second.line("AYAR koordinat_hassasiyeti").ok());
    CHECK(mentions(second.echoed, "varsayılan"));

    // The application store is per user and machine, not per document — but it is
    // still per bus here, because a process-wide one is what leaked.
    CHECK(first.line("TERCIH tema koyu").ok());
    CHECK(!second.bus.app_settings().is_explicit("core.arayuz.tema"));
}

TEST_CASE("AYAR komut satırından, betikten ve arayüzden aynı sonucu verir")
{
    // test.md R3 / piricad.md §16.5: the three clients are equal. A dialog, a typed
    // line and a JSON script must leave the same state behind and the same journal.
    // `origin` is the only field allowed to differ, and it is not compared.
    const auto what_happened = [](const command::Journal& j) {
        std::string out;
        for (const auto& e : j.entries())
            out += e.command_id + " " + e.args.to_json().dump() + "\n";
        return out;
    };

    // 1. the GUI: a settings dialog dispatches the command with its fields filled in
    Rig gui;
    command::Args args;
    args.set("ad", command::Value::text("core.crs.hassasiyet"));
    args.set("deger", command::Value::text("6"));
    CHECK(gui.bus.dispatch(command::Invocation{"core.setting", args, command::Origin::Gui}).ok());

    // 2. the command line, with the Turkish alias and no id in sight
    Rig cli;
    CHECK(cli.bus.execute_line("AYAR koordinat_hassasiyeti 6", command::Origin::CommandLine).ok());

    // 3. a JSON script through the same bus
    Rig script;
    piricad::script::JsonRunner runner(script.bus, piricad::script::Sandbox::Project);
    auto ran = runner.run_text(R"({"ad":"ayar","komutlar":[)"
                               R"({"cmd":"core.setting","args":{"ad":"core.crs.hassasiyet",)"
                               R"("deger":"6"}}]})");
    CHECK(ran.ok());

    CHECK_EQ(what_happened(gui.journal), what_happened(cli.journal));
    CHECK_EQ(what_happened(gui.journal), what_happened(script.journal));

    // Assert the CHANGE, not the equality of two constants. Each Rig now owns its
    // stores, so the fingerprint below is of what THAT client wrote — before this,
    // all three read one process-wide store and the comparison was between three
    // views of the same object.
    const std::uint64_t untouched = Settings{builtin_settings(), SettingScopeMask::Project}.fold(0);
    CHECK(gui.bus.project_settings().fold(0) != untouched); // the write moved it
    CHECK_EQ(gui.bus.project_settings().fold(0), cli.bus.project_settings().fold(0));
    CHECK_EQ(gui.bus.project_settings().fold(0), script.bus.project_settings().fold(0));

    // R39/P15: an App-scope write is not document content and must NOT move it.
    CHECK(gui.bus.app_settings().set("core.arayuz.tema", SettingValue::enumerated(2)).ok());
    CHECK_EQ(gui.bus.project_settings().fold(0), cli.bus.project_settings().fold(0));

    // No client touched the geometry. content_hash() is compared too, but it is
    // deliberately NOT the assertion that carries R39 here: Document::content_hash()
    // does not fold any Settings store yet (the PHASE-0 SEAM in
    // commands/settings.cpp), so over three empty documents it would pass even if
    // no client had written anything. That half of R39 lands with Document::settings().
    CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
    CHECK_EQ(gui.doc.content_hash(), script.doc.content_hash());

    // R39 says a Project setting is undoable and the command declares
    // UndoPolicy::SingleTransaction, but the write does not go through the
    // transaction yet, so it produces no Op and the bus pushes no undo entry. This
    // pins the SEAM, not the contract: when Transaction::set_setting() lands this
    // becomes 1 and undoing it must restore the previous value.
    CHECK_EQ(gui.undo.undo_depth(), std::size_t{0});
}

TEST_CASE("AYAR iptal edilirse hiçbir şey olmaz")
{
    // A cancelled command leaves an empty undo delta and writes nothing
    // (.claude/command.md R10). AYAR with no arguments only reads.
    Rig rig;
    const std::size_t before = rig.journal.size();
    CHECK(rig.line("AYAR").ok());
    CHECK_EQ(rig.undo.undo_depth(), std::size_t{0});
    CHECK_EQ(rig.journal.size(), before + 1); // it ran, it changed nothing
}

// ------------------------------------------------ session modes and the grid ----

TEST_CASE("R41: her kapsamın bir komutu var — oturum ayarları artık ulaşılabilir")
{
    // The gap this closes: yakalama.modlar, dik_mod and kutupsal_aci were declared
    // Session and had no store and no command. Every client could read the spec and
    // no client could write the value. R38 says the interface is generated from the
    // declaration; a declaration nobody can act on is not one.
    Rig r;
    CHECK(r.line("MOD dik_mod evet").ok());
    CHECK(r.bus.session_settings().get("core.yakalama.dik_mod").as_bool());

    CHECK(r.line("MOD ızgaraya_yakala evet").ok());
    CHECK(r.bus.session_settings().get("core.yakalama.izgara").as_bool());

    // The scope box refuses a foreign scope in both directions (R39): MOD may not
    // write a project setting and AYAR may not write a session one. The refusal is
    // a guiding message rather than a dropped command — the same deliberate choice
    // "AYAR: bilinmeyen ayar adı komutu düşürmez" records above — so the assertion
    // is that nothing was written and the message names the right scope.
    CHECK(r.line("MOD koordinat_sistemi TUREF/TM33").ok());
    CHECK(mentions(r.echoed, "proje"));
    CHECK(!r.bus.project_settings().is_explicit("core.crs.id"));

    // kutupsal_aci is untouched above, so is_explicit here reports the refusal and
    // not the legitimate MOD write two lines up.
    CHECK(r.line("AYAR kutupsal_açı 30000000").ok());
    CHECK(mentions(r.echoed, "oturum"));
    CHECK(!r.bus.session_settings().is_explicit("core.yakalama.kutupsal_aci"));

    CHECK(r.line("TERCİH ızgaraya_yakala evet").ok());
    CHECK(mentions(r.echoed, "oturum"));
}

TEST_CASE("R39: oturum modu ne belgeye ne tercih dosyasına sızar")
{
    Rig r;
    const std::uint64_t before = r.doc.content_hash();
    const std::uint64_t app    = r.bus.app_settings().fold(0);
    const std::uint64_t proj   = r.bus.project_settings().fold(0);

    CHECK(r.line("MOD kutupsal_açı 30000000").ok());

    CHECK_EQ(r.doc.content_hash(), before);      // belgeye girmez
    CHECK_EQ(r.bus.app_settings().fold(0), app); // tercihe girmez
    CHECK_EQ(r.bus.project_settings().fold(0), proj);
    CHECK((r.journal.entries().empty() ||
           r.journal.entries().back().command_id != "core.mode")); // belge mutasyonu değil
}

TEST_CASE("R40: tolerans proje kapsamındadır — ifraz sonucunu değiştirir")
{
    // The literal application of R40. A node tolerance stored per machine would
    // make the same drawing produce two different parcel areas on two computers,
    // and the parcel area is what goes on the tapu.
    Rig r;
    const std::uint64_t untouched = Settings{builtin_settings(), SettingScopeMask::Project}.fold(0);

    CHECK(r.line("AYAR düğüm_toleransı 20").ok());
    CHECK_EQ(r.bus.project_settings().get("core.topoloji.dugum_toleransi").as_length(), 20);
    CHECK(r.bus.project_settings().fold(0) != untouched);

    // The same setting cannot be written as a per-machine preference: `TERCİH` cannot
    // reach into the project box, and if it could the tolerance would vary by machine.
    CHECK(r.line("TERCİH düğüm_toleransı 20").ok());
    CHECK(mentions(r.echoed, "proje"));
    CHECK(!r.bus.app_settings().is_explicit("core.topoloji.dugum_toleransi"));
}

TEST_CASE("Izgara ve seçme toleransı piksel cinsindendir, zemin metresi değil")
{
    // A tolerance in ground metres would shrink on screen as the user zooms in,
    // which is backwards: the hand does not get steadier at 1/100. Recorded as a
    // test because it is the kind of unit that gets 'fixed' into metres later.
    const SettingCatalog& cat = builtin_settings();
    for (const char* id : {"core.yakalama.tolerans", "core.secim.tolerans"}) {
        const SettingSpec& spec = cat.at(cat.find(id));
        CHECK_EQ(spec.unit, std::string("piksel"));
        CHECK(spec.type == SettingType::Int);
    }
    // The grid step is on the GROUND: a mesh that has to line up with the metrekare
    // defteri.
    const SettingSpec& adim = cat.at(cat.find("core.izgara.adim"));
    CHECK(adim.type == SettingType::Length);
    CHECK_EQ(adim.unit, std::string("mm"));
}

TEST_CASE("Izgara adımı ve ana çizgi aralığı sıfır olamaz")
{
    // A zero step is an infinite loop in the canvas and a zero major interval is a
    // division by zero. R42 clamps rather than failing, so the clamp is the guard.
    Rig r;
    CHECK(r.line("TERCİH ızgara_adımı 0").ok());
    CHECK(r.bus.app_settings().get("core.izgara.adim").as_length() >= 1);

    CHECK(r.line("TERCİH ana_çizgi 0").ok());
    CHECK(r.bus.app_settings().get("core.izgara.ana_cizgi").as_int() >= 1);
}

// ------------------------------------------------------ the settings service --

TEST_CASE("Ayar servisi: kapsamı bildirim belirler, çağıran değil")
{
    // The gap this closes: every caller used to pick a store by hand, so adding a
    // setting meant finding every reader and telling it which drawer to open — and
    // a reader that guessed wrong read a default and reported it as a value.
    Rig r;

    // One id from each scope, resolved from the declaration alone.
    CHECK(r.bus.store_for("core.arayuz.tema") == &r.bus.app_settings());
    CHECK(r.bus.store_for("core.crs.id") == &r.bus.project_settings());
    CHECK(r.bus.store_for("core.yakalama.dik_mod") == &r.bus.session_settings());

    // An id nobody declares resolves to nothing rather than to a wrong drawer.
    CHECK(r.bus.store_for("core.boyle.bir.ayar.yok") == nullptr);
}

TEST_CASE("Ayar servisi: yazma kapsamı bildirir, her istemci aynı yoldan geçer")
{
    Rig r;

    std::vector<core::SettingScope> heard;
    r.bus.on_settings_changed = [&heard](core::SettingScope scope) { heard.push_back(scope); };

    // An App-scope write reports App, and lands in the App store.
    // The theme is an ENUM, so its value is an index into the declared options —
    // `koyu` is the third. Writing it as text is what the parser does for a user.
    const core::SettingSpec& theme =
        core::builtin_settings().at(core::builtin_settings().find("core.arayuz.tema"));
    auto dark = core::parse_setting(theme, "koyu");
    REQUIRE(dark.ok());
    auto changed = r.bus.set_setting("core.arayuz.tema", dark.value());
    if (!changed) FAIL_WITH("tema", changed.error().message);
    REQUIRE(changed.ok());
    REQUIRE(heard.size() == std::size_t{1});
    CHECK(heard.front() == core::SettingScope::App);
    CHECK_EQ(core::format_setting(theme, r.bus.setting("core.arayuz.tema")), std::string("koyu"));

    // A Project-scope write reports Project. The caller said neither.
    auto zone = core::SettingValue::text("TUREF/TM33");
    REQUIRE(zone.ok());
    REQUIRE(r.bus.set_setting("core.crs.id", zone.value()).ok());
    REQUIRE(heard.size() == std::size_t{2});
    CHECK(heard.back() == core::SettingScope::Project);

    // An undeclared id is refused by NAME, not silently written somewhere.
    auto refused = r.bus.set_setting("core.yok", core::SettingValue::integer(1));
    CHECK(!refused);
    CHECK(refused.error().message.find("core.yok") != std::string::npos);
    CHECK_EQ(heard.size(), std::size_t{2});
}

TEST_CASE("TERCİH betikten de kalıcıdır — kapsam bildirimi her istemciye gider")
{
    // The asymmetry Article 1.2 forbids: preferences were written only when the
    // main window was destroyed, so the same line survived from the menu and was
    // lost from a script. The command now goes through the service, so the owner
    // of persistence hears about every client's write.
    Rig r;

    int app_writes            = 0;
    r.bus.on_settings_changed = [&app_writes](core::SettingScope scope) {
        if (scope == core::SettingScope::App) ++app_writes;
    };

    REQUIRE(r.bus.execute_line("TERCİH tema koyu", command::Origin::Script).ok());
    CHECK_EQ(app_writes, 1);

    // ...and a reset is a change too, or the old value comes back on next start.
    REQUIRE(r.bus.execute_line("TERCİH tema varsayılan", command::Origin::Script).ok());
    CHECK_EQ(app_writes, 2);
}
