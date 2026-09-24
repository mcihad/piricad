// SPDX-License-Identifier: GPL-3.0-or-later
//
// A CAPTION THAT SAYS WHAT ITS OBJECT IS, AND KEEPS SAYING IT (TODOS C-12).
//
// An area written in a parcel is a statement about the parcel. Each case here
// changes the parcel — a corner dragged, a column edited, the whole command run
// again — and asks whether the label still says the truth, in the same undo
// step, without a second label appearing on top of the first.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/text_fields.hpp"
#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <filesystem>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;
using core::Point2;
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
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
    }

    /// Every caption on `layer`, in slot order.
    std::vector<std::string> captions(const std::string& layer) const
    {
        std::vector<std::string> out;
        const core::LayerId l = doc.find_layer(layer);
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().layer[e] == l &&
                doc.texts().has(doc.entities().slot[e]))
                out.emplace_back(doc.texts().text(doc.entities().slot[e]));
        return out;
    }

    core::EntityId caption_on(const std::string& layer) const
    {
        const core::LayerId l = doc.find_layer(layer);
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().layer[e] == l &&
                doc.texts().has(doc.entities().slot[e]))
                return e;
        return core::kNoEntity;
    }

    Point2 anchor_of(core::EntityId e) const
    {
        return doc.geometry().vertex(doc.geometry().rings_of(doc.entities().slot[e]).first, 0);
    }
};

/// A parcel of 20 × 10 m on layer PARSEL with ada 101, and nothing else.
void parcel(Rig& r)
{
    r.run("KATMAN ad=PARSEL");
    r.run("SÜTUN kimlik=ada tur=tam_sayi");
    r.run("ALAN 0,0 20,0 20,10 0,10");
    r.run("ÖZNİTELİK ada 1 101");
}

} // namespace

TEST_CASE("YAZI ALANLARI: {sutun} sütunun değeri, {#alan} alan, {#cevre} çevre; bilinmeyen kalır")
{
    CHECK_EQ(core::format_area(1'234'560'000, 2, ','), std::string("1234,56"));
    CHECK_EQ(core::format_area(200'000'000, 0, ','), std::string("200"));

    Rig r;
    parcel(r);
    const core::EntityId e = r.doc.slot_of(core::EntityKey{1});
    auto filled = fill_fields(r.doc, e, "Ada {ada} · {#alan} m² · Ç={#cevre} m {yok} {#yok}");
    REQUIRE(filled.ok());
    // A brace that names nothing is left as it is, so a typo is seen, not lost.
    CHECK_EQ(filled.value(), std::string("Ada 101 · 200,00 m² · Ç=60,00 m {yok} {#yok}"));
}

TEST_CASE("ETİKET: etiket nesnesini izler — köşe taşınınca alanı, sütun değişince değeri yeniden "
          "yazılır; tek geri alma adımı")
{
    Rig r;
    parcel(r);
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\\n{#alan} m²\"");
    REQUIRE_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"101\n200,00 m²"});
    const core::EntityId label = r.caption_on("PARSEL ETİKET");
    const core::Attachment* a  = r.doc.attachments().get(label);
    REQUIRE(a != nullptr);
    CHECK_EQ(a->anchor, core::AttachAnchor::Centre);
    CHECK_EQ(a->derive, core::AttachDerive::Fields);
    CHECK_EQ(r.anchor_of(label), (Point2{10'000, 5'000}));

    // A corner pulled out: the area and the middle move with it, in that edit.
    const std::size_t depth = r.undo.undo_depth();
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=20,20");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    CHECK_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"101\n300,00 m²"});
    CHECK_EQ(r.anchor_of(label), (Point2{10'000, 10'000}));

    // A column edited: the label says the new ada at once.
    r.run("ÖZNİTELİK ada 1 102");
    CHECK_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"102\n300,00 m²"});

    r.run("GERİAL");
    CHECK_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"101\n300,00 m²"});
    r.run("GERİAL");
    CHECK_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"101\n200,00 m²"});
}

TEST_CASE("ETİKET: yeniden çalışınca yazdığı etiketleri yeniler, üstüne ikinci takım yazmaz; "
          "bagla=hayır bağsız yazar")
{
    Rig r;
    parcel(r);
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\"");
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\"");
    CHECK_EQ(r.captions("PARSEL ETİKET").size(), std::size_t{1});
    CHECK(r.said.find("0 etiket yazıldı: 'PARSEL ETİKET' katmanına; 1 etiket yenilendi.") !=
          std::string::npos);

    // Another format is another label.
    r.run("ETİKET katman=PARSEL bicim=\"{#alan} m²\"");
    CHECK_EQ(r.captions("PARSEL ETİKET").size(), std::size_t{2});

    // Told not to follow: a caption as it always was, standing where it was put.
    r.run("ETİKET katman=PARSEL bicim=\"A={#alan}\" hedef=SERBEST bagla=hayır");
    const core::EntityId free = r.caption_on("SERBEST");
    REQUIRE(free != core::kNoEntity);
    CHECK(r.doc.attachments().get(free) == nullptr);
    r.run("KÖŞETAŞI nesne=1 kose=3 nokta=20,20");
    CHECK_EQ(r.captions("SERBEST"), std::vector<std::string>{"A=200,00"});
}

TEST_CASE("ETİKET: sütunu boşalan etiket boş kalır, silinmez; sütun dolunca geri gelir")
{
    Rig r;
    parcel(r);
    r.run("ETİKET katman=PARSEL bicim=\"{ada}\"");
    const core::EntityId label = r.caption_on("PARSEL ETİKET");
    r.run("ÖZNİTELİK ada 1 yok");
    REQUIRE(r.doc.alive(label));
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[label])), std::string(" "));
    r.run("ÖZNİTELİK ada 1 205");
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[label])), std::string("205"));
}

TEST_CASE("BAĞLA: tur=alan bag=merkez — yazı hemen alan olur, parsel değişince yeniden yazılır")
{
    Rig r;
    parcel(r);
    r.run("METİN noktalar=10,6 yazi=? hizalama=merkez");
    const core::EntityId caption = r.doc.slot_of(core::EntityKey{2});
    r.run("BAĞLA nesneler=2 kaynak=1 bag=merkez tur=alan");
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[caption])),
             std::string("200,00 m²"));
    // Attached where it stood: a metre above the middle, and it stays a metre
    // above the new middle.
    r.run("TAŞI nesneler=1 baslangic=0,0 bitis=5,0");
    CHECK_EQ(r.anchor_of(caption), (Point2{15'000, 6'000}));
    r.run("KÖŞETAŞI nesne=1 kose=2 nokta=35,0");
    CHECK_EQ(std::string(r.doc.texts().text(r.doc.entities().slot[caption])),
             std::string("250,00 m²"));

    // tur=bicim with its own format: a column and a figure in one caption.
    r.run("METİN noktalar=10,3 yazi=? hizalama=merkez");
    r.run("BAĞLA nesneler=3 kaynak=1 bag=merkez tur=bicim bicim=\"Ada {ada}: {#alan} m²\"");
    CHECK_EQ(
        std::string(r.doc.texts().text(r.doc.entities().slot[r.doc.slot_of(core::EntityKey{3})])),
        std::string("Ada 101: 250,00 m²"));
    auto no_format = r.bus.execute_line("BAĞLA nesneler=3 kaynak=1 tur=bicim", Origin::Test);
    CHECK_FALSE(no_format.ok());
}

TEST_CASE("IO: ortaya bağlı, kalıptan doldurulan etiket dosyaya yazılır; açılan çizimde izlemeyi "
          "sürdürür")
{
    const fs::path dir = fs::temp_directory_path() / "kentoscad-yazi-alanlari";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string path = (dir / "etiket.pcad").string();

    Rig written;
    parcel(written);
    written.run("ETİKET katman=PARSEL bicim=\"{ada} · {#alan} m²\"");
    const std::uint64_t hash = written.doc.content_hash();
    written.run("FARKLIKAYDET \"" + path + "\"");

    Rig reloaded;
    reloaded.run("AÇ \"" + path + "\"");
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    reloaded.run("KÖŞETAŞI nesne=1 kose=3 nokta=20,20");
    CHECK_EQ(reloaded.captions("PARSEL ETİKET"), std::vector<std::string>{"101 · 300,00 m²"});
    fs::remove_all(dir, ec);
}

TEST_CASE("ETİKET KANIT: bağlı etiket komut satırından ve betikten aynı belge, aynı günlük")
{
    Rig line;
    parcel(line);
    line.run("ETİKET katman=PARSEL bicim=\"{ada}\\n{#alan} m²\" yukseklik=1500");

    Rig scr;
    parcel(scr);
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"json({"komutlar":[{"cmd":"core.label","args":{
        "katman":"PARSEL","bicim":"{ada}\n{#alan} m²","yukseklik":1500}}]})json");
    CHECK(ran.ok());

    CHECK_EQ(line.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(line.journal.entries().back().args.to_json().dump(),
             scr.journal.entries().back().args.to_json().dump());
}

// ------------------------------------------------------------ BULDEĞİŞTİR ----

namespace {

std::string words_of(const Rig& r, std::int64_t key)
{
    const core::EntityId e =
        r.doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    return std::string(r.doc.texts().text(r.doc.entities().slot[e]));
}

} // namespace

TEST_CASE("BULDEĞİŞTİR: bulmak seçer; Türkçe büyük/küçük harf, tam kelime")
{
    Rig r;
    r.run("METİN noktalar=0,0 yazi=\"ADA 101\"");   // 1
    r.run("METİN noktalar=0,5 yazi=\"Ada 102\"");   // 2
    r.run("METİN noktalar=0,10 yazi=ADALET");       // 3
    r.run("METİN noktalar=0,15 yazi=\"istanbul\""); // 4
    r.run("METİN noktalar=0,20 yazi=IRMAK");        // 5

    r.said.clear();
    r.run("BULDEĞİŞTİR bul=ada");
    CHECK(r.said.find("3 yazıda 3 eşleşme:") != std::string::npos);
    CHECK_EQ(r.bus.selection().size(), std::size_t{3});

    // The short form the manual prints: the word alone is what is looked for.
    r.run("SEÇ mod=TEMİZLE");
    r.run("BUL ada");
    CHECK_EQ(r.bus.selection().size(), std::size_t{3});
    // What was selected before the find comes back as the previous selection.
    r.run("SEÇ NESNE nesneler=4");
    r.run("BUL ada");
    CHECK_EQ(r.bus.selection().size(), std::size_t{3});
    r.run("SEÇ mod=ÖNCEKİ");
    REQUIRE_EQ(r.bus.selection().size(), std::size_t{1});
    CHECK(r.bus.selection().contains(static_cast<core::EntityKey>(std::uint64_t{4})));

    auto blank = r.bus.execute_line("BULDEĞİŞTİR bul=\"\"", Origin::Test);
    REQUIRE_FALSE(blank.ok());
    CHECK_EQ(blank.error().message, std::string("Aranacak yazı boş olamaz."));

    r.run("BULDEĞİŞTİR bul=ada tam_kelime=evet");
    CHECK_EQ(r.bus.selection().size(), std::size_t{2}); // ADALET is another word
    r.run("BULDEĞİŞTİR bul=Ada buyuk_kucuk=evet tam_kelime=evet");
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});

    // Punctuation and signs end a word, letters of every alphabet do not.
    r.run("METİN noktalar=0,25 yazi=\"«ADA» ADA… ⌀120 ADAÇAYI\""); // 6
    r.said.clear();
    r.run("BULDEĞİŞTİR bul=ada tam_kelime=evet nesneler=6");
    CHECK(r.said.find("1 yazıda 2 eşleşme:") != std::string::npos);
    r.said.clear();
    r.run("BULDEĞİŞTİR bul=120 tam_kelime=evet nesneler=6");
    CHECK(r.said.find("1 yazıda 1 eşleşme:") != std::string::npos);

    // In this alphabet the capital of dotted i is the dotted capital, and of
    // dotless i the plain I: the capital city found in lower case, a river
    // written with a plain I found only through its dotless lower case.
    r.run("BULDEĞİŞTİR bul=İSTANBUL");
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
    r.said.clear();
    r.run("BULDEĞİŞTİR bul=irmak");
    CHECK(r.said.find("hiçbir yazıda bulunmadı") != std::string::npos);
    r.run("BULDEĞİŞTİR bul=ırmak");
    CHECK_EQ(r.bus.selection().size(), std::size_t{1});
}

TEST_CASE("BULDEĞİŞTİR: önizleme önce, değişiklik uygula ile; tek geri alma adımı; betikte "
          "uygula yoksa hiçbir şey değişmez")
{
    Rig r;
    r.run("METİN noktalar=0,0 yazi=\"ADA 101\"");
    r.run("METİN noktalar=0,5 yazi=\"ADA 102 / ADA 103\"");
    const std::uint64_t before = r.doc.content_hash();
    const std::size_t depth    = r.undo.undo_depth();

    // Asked and not answered: a script cannot answer, so the command stops at
    // the question — with the preview already said and the way to apply named.
    r.said.clear();
    r.run("BULDEĞİŞTİR bul=ADA degistir=Ada");
    CHECK(r.said.find("2 yazıda 3 eşleşme:") != std::string::npos);
    CHECK(r.said.find("«ADA 102 / ADA 103» → «Ada 102 / Ada 103»") != std::string::npos);
    CHECK(r.said.find("Değiştirilmedi; uygulamak için uygula=evet verin.") != std::string::npos);
    CHECK_EQ(r.doc.content_hash(), before);
    CHECK_EQ(r.undo.undo_depth(), depth);

    r.run("BULDEĞİŞTİR bul=ADA degistir=Ada uygula=hayır");
    CHECK_EQ(r.doc.content_hash(), before);
    CHECK_EQ(r.undo.undo_depth(), depth);

    r.run("BULDEĞİŞTİR bul=ADA degistir=Ada uygula=evet");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    CHECK_EQ(words_of(r, 1), std::string("Ada 101"));
    CHECK_EQ(words_of(r, 2), std::string("Ada 102 / Ada 103"));
    CHECK(r.said.find("2 yazıda 3 eşleşme değiştirildi.") != std::string::npos);

    r.run("GERİAL");
    CHECK_EQ(words_of(r, 1), std::string("ADA 101"));
    CHECK_EQ(words_of(r, 2), std::string("ADA 102 / ADA 103"));
}

TEST_CASE("BULDEĞİŞTİR: boş değiştirme sözcüğü siler; yazıyı boşaltacak değiştirme o yazıyı "
          "atlar ve söyler")
{
    Rig r;
    r.run("METİN noktalar=0,0 yazi=\"ADA 101 (eski)\""); // 1
    r.run("METİN noktalar=0,5 yazi=ESKİ");               // 2
    r.run("METİN noktalar=0,10 yazi=\"ESKİ \"");         // 3

    // Nothing in place of the word removes it. A caption the change would
    // leave with no words — or only spaces — is not written: an empty caption
    // is a bare line on the sheet, so it keeps its words and the answer says so.
    auto result =
        r.bus.execute_line("BULDEĞİŞTİR bul=\" (eski)\" degistir=\"\" uygula=evet", Origin::Test);
    REQUIRE(result.ok());
    CHECK_EQ(words_of(r, 1), std::string("ADA 101"));

    r.said.clear();
    result = r.bus.execute_line("BULDEĞİŞTİR bul=eski degistir=\"\" uygula=evet", Origin::Test);
    REQUIRE(result.ok());
    CHECK(r.said.find("boş kalacak 2 yazı atlandı") != std::string::npos);
    CHECK_EQ(words_of(r, 2), std::string("ESKİ"));
    CHECK_EQ(words_of(r, 3), std::string("ESKİ "));
    CHECK_EQ(result.value().report.find("bos_kalacak")->as_int(), 2);

    // The same from a script: an empty string is a replacement, not a missing one.
    Rig scr;
    scr.run("METİN noktalar=0,0 yazi=\"ADA 101 (eski)\"");
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"json({"komutlar":[{"cmd":"core.find_replace","args":{
        "bul":" (eski)","degistir":"","uygula":true}}]})json");
    CHECK(ran.ok());
    CHECK_EQ(words_of(scr, 1), std::string("ADA 101"));
}

TEST_CASE("BULDEĞİŞTİR: ölçü yazısına, kalıptan doldurulan etikete ve blok üyesine dokunmaz; "
          "yapılandırılmış önizleme")
{
    Rig r;
    parcel(r);
    r.run("ETİKET katman=PARSEL bicim=\"ADA {ada}\"");
    r.run("ÇİZGİ 0,-5 20,-5");
    r.run("ÖLÇÜ birinci=0,-5 ikinci=20,-5 konum=10,-8 onek=\"ADA \"");
    r.run("METİN noktalar=0,30 yazi=\"ADA NOTU\"");

    auto result = r.bus.execute_line("BULDEĞİŞTİR bul=ADA degistir=Ada uygula=evet", Origin::Test);
    REQUIRE(result.ok());
    CHECK(r.said.find("1 yazıda 1 eşleşme; kalıptan doldurulan 1 yazı atlandı") !=
          std::string::npos);
    CHECK_EQ(r.captions("PARSEL ETİKET"), std::vector<std::string>{"ADA 101"});
    const core::Json& report = result.value().report;
    REQUIRE(report.find("yazi") != nullptr);
    CHECK_EQ(report.find("yazi")->as_int(), 1);
    CHECK_EQ(report.find("atlanan")->as_int(), 1);
    REQUIRE_EQ(report.find("satirlar")->as_array().size(), std::size_t{1});
    CHECK_EQ(report.find("satirlar")->as_array().front().find("sonra")->as_string(),
             std::string("Ada NOTU"));
}

TEST_CASE("BULDEĞİŞTİR KANIT: komut satırından ve betikten aynı belge, aynı günlük")
{
    Rig line;
    line.run("METİN noktalar=0,0 yazi=\"ADA 101\"");
    line.run("BULDEĞİŞTİR bul=ADA degistir=Ada tam_kelime=evet uygula=evet");

    Rig scr;
    scr.run("METİN noktalar=0,0 yazi=\"ADA 101\"");
    script::JsonRunner runner(scr.bus, script::Sandbox::Project);
    auto ran = runner.run_text(R"json({"komutlar":[{"cmd":"core.find_replace","args":{
        "bul":"ADA","degistir":"Ada","tam_kelime":true,"uygula":true}}]})json");
    CHECK(ran.ok());
    CHECK_EQ(line.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(line.journal.entries().back().args.to_json().dump(),
             scr.journal.entries().back().args.to_json().dump());
}
