// SPDX-License-Identifier: GPL-3.0-or-later
// DIŞREFERANS — external references (TODOS C-14, model.md R45a).
//
// The acceptance this file holds, one case per sentence of it:
//   * a reference is drawn where its file drew it and snapped to like a block,
//     and no command edits it in place;
//   * the project file holds its name and its path, never its objects — they
//     come from the source on every open, so a changed source is what the next
//     open (or a reload) shows;
//   * a missing source does not keep the drawing from opening;
//   * a project folder carried elsewhere finds its references by relative path
//     and by name beside it.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/io/format.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
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

    core::BlockId block(const std::string& name) const { return doc.blocks().find(name); }

    /// The live members of `name`.
    std::size_t members(const std::string& name) const
    {
        const core::BlockId b = block(name);
        if (b == core::kNoBlock) return 0;
        std::size_t n = 0;
        for (const core::EntityKey k : doc.blocks().at(b).members)
            if (const core::EntityId m = doc.slot_of(k); m != core::kNoEntity && doc.alive(m)) ++n;
        return n;
    }

    /// The live block references on the sheet.
    std::vector<core::EntityId> references() const
    {
        std::vector<core::EntityId> out;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.entities().standalone(e) && doc.entities().kind[e] == core::kBlockReferenceKind)
                out.push_back(e);
        return out;
    }

    core::SnapResult snap(Point2 aim, std::uint32_t modes) const
    {
        core::SnapQuery q;
        q.aim    = aim;
        q.radius = 250;
        q.modes  = modes;
        return core::snap(doc, q);
    }
};

class TempDir
{
public:
    explicit TempDir(const char* tag)
    {
        path_ = fs::temp_directory_path() / (std::string("kentoscad-xref-") + tag);
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

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

/// A base map: a road on its own layer and a manhole cover, a block placed
/// twice, saved to `path`.
void write_source(const std::string& path, bool extra = false, const std::string& crs = {})
{
    Rig src;
    if (!crs.empty()) src.run("AYAR core.crs.id " + crs);
    src.run("KATMAN ad=YOL");
    src.run("ÇİZGİ 0,0 100,0");
    src.run("DAİRE merkez=50,20 cevre=51,20");
    src.run("BLOK ad=KAPAK taban=50,20 nesneler=2");
    src.run("BLOKEKLE ad=KAPAK nokta=80,20");
    if (extra) src.run("ÇİZGİ 0,50 100,50");
    src.run("FARKLIKAYDET \"" + path + "\"");
}

std::uint32_t min_reader_of(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    io::FileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header.min_reader_version;
}

} // namespace

TEST_CASE("DIŞREFERANS: bağlanan dosya yerinde çizilir, yakalanır ve düzenlenmez")
{
    TempDir tmp("bagla");
    const std::string source = tmp.file("altlik.pcad");
    write_source(source);

    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\"");
    const core::BlockId xref = host.block("altlik");
    REQUIRE(xref != core::kNoBlock);
    const core::BlockDef& def = host.doc.blocks().at(xref);
    CHECK((def.flags & core::kBlockExternal) != 0);
    CHECK_EQ(def.path, fs::path(source).lexically_normal().string());
    // The road and the cover's reference are its members; the cover's block
    // arrives as its dependent, under the reference's name.
    CHECK_EQ(host.members("altlik"), std::size_t{3});
    const core::BlockId kapak = host.block("altlik|KAPAK");
    REQUIRE(kapak != core::kNoBlock);
    CHECK((host.doc.blocks().at(kapak).flags & core::kBlockDependent) != 0);
    CHECK_EQ(host.members("altlik|KAPAK"), std::size_t{1});
    CHECK(host.doc.find_layer("altlik|YOL") != core::kNoLayer);
    REQUIRE_EQ(host.references().size(), std::size_t{1});

    // Drawn where the file drew it: the road's end is a snap point, and so is
    // the cover's centre, through the reference inside the reference.
    CHECK_EQ(host.snap(Point2{100'060, 40}, core::SnapEndpoint).point, (Point2{100'000, 0}));
    CHECK_EQ(host.snap(Point2{80'050, 20'050}, core::SnapCenter).point, (Point2{80'000, 20'000}));
    CHECK_EQ(core::pick_nearest(host.doc, Point2{60'000, 10}, 100), host.references().front());

    // And edited by nobody here.
    const std::string key = std::to_string(core::raw(host.doc.key_of(host.references().front())));
    CHECK(host.refused("BLOKDÜZENLE ad=altlik").find("dış referans") != std::string::npos);
    CHECK(host.refused("BLOKDÜZENLE nesne=" + key).find("dış referans") != std::string::npos);
    CHECK(host.refused("BLOKDÜZENLE ad=altlik|KAPAK").find("dış referans") != std::string::npos);
    CHECK(host.refused("PATLAT nesne=" + key).find("patlatılamaz") != std::string::npos);

    // One command, one undo step: the definition empties, the reference goes.
    host.run("GERİAL");
    CHECK(host.references().empty());
    CHECK_EQ(host.members("altlik"), std::size_t{0});
}

TEST_CASE("DIŞREFERANS: proje dosyası adı ve yolu tutar, nesneleri tutmaz; açınca kaynaktan gelir")
{
    TempDir tmp("kaydet");
    const std::string source  = tmp.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);

    std::uint64_t hash = 0;
    {
        Rig host;
        host.run("ÇİZGİ 0,-10 10,-10"); // before: its key stands below the members'
        host.run("DIŞREFERANS dosya=\"" + source + "\"");
        host.run("ÇİZGİ 0,-20 10,-20"); // after: the key sequence has a gap before it
        host.run("LİDER noktalar=10,-20 13,-23 16,-23 metin=Yol");
        hash = host.doc.content_hash();
        host.said.clear();
        host.run("FARKLIKAYDET \"" + project + "\"");
        // The drawing's own: two lines, the leader and its words, the reference.
        CHECK(host.said.find("(5 nesne") != std::string::npos);
    }
    CHECK_EQ(min_reader_of(project), io::kMinReaderVersionExternal);

    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK(back.said.find("uyarı") == std::string::npos);
    CHECK_EQ(back.members("altlik"), std::size_t{3});
    CHECK_EQ(back.members("altlik|KAPAK"), std::size_t{1});
    CHECK_EQ(back.references().size(), std::size_t{1});
    CHECK_EQ(back.doc.content_hash(), hash);
    // Keys kept through the gap: the line drawn after the reference is still
    // the object it was, and the next object gets a key never used before.
    CHECK(back.doc.slot_of(static_cast<core::EntityKey>(1)) != core::kNoEntity);
    back.run("NOKTA 5,5");
    const core::EntityKey newest =
        back.doc.key_of(static_cast<core::EntityId>(back.doc.entities().size() - 1));
    CHECK(core::raw(newest) > 7);
}

TEST_CASE("DIŞREFERANS: kaynak değişince yenile ve yeniden açma değişikliği getirir")
{
    TempDir tmp("yenile");
    const std::string source  = tmp.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);

    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\"");
    host.run("FARKLIKAYDET \"" + project + "\"");
    CHECK_EQ(host.members("altlik"), std::size_t{3});

    write_source(source, true); // somebody else adds a line to the base map
    host.run("DIŞREFERANS islem=yenile ad=altlik");
    CHECK_EQ(host.members("altlik"), std::size_t{4});
    CHECK_EQ(host.snap(Point2{100'040, 50'030}, core::SnapEndpoint).point,
             (Point2{100'000, 50'000}));
    // Reloading is one undo step, and undone the old picture is back.
    host.run("GERİAL");
    CHECK_EQ(host.members("altlik"), std::size_t{3});

    Rig reopened;
    reopened.run("AÇ \"" + project + "\"");
    CHECK_EQ(reopened.members("altlik"), std::size_t{4});
}

TEST_CASE("DIŞREFERANS: kayıp kaynak çizimi açılmaz yapmaz; yol ile yeniden bağlanır")
{
    TempDir tmp("kayip");
    const std::string source  = tmp.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);
    {
        Rig host;
        host.run("ÇİZGİ 0,-10 10,-10");
        host.run("DIŞREFERANS dosya=\"" + source + "\"");
        host.run("FARKLIKAYDET \"" + project + "\"");
    }
    const std::string moved = tmp.file("arsiv.pcad");
    fs::rename(source, moved);

    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK(back.said.find("'altlik' dış referansı yüklenemedi") != std::string::npos);
    CHECK_EQ(back.members("altlik"), std::size_t{0});
    CHECK_EQ(back.references().size(), std::size_t{1}); // still there, drawn empty
    CHECK(back.doc.alive(back.doc.slot_of(static_cast<core::EntityKey>(1))));
    back.run("DIŞREFERANS islem=listele");
    CHECK(back.said.find("bulunamadı") != std::string::npos);

    back.run("DIŞREFERANS islem=yol ad=altlik dosya=\"" + moved + "\"");
    CHECK_EQ(back.members("altlik"), std::size_t{3});
    CHECK_EQ(back.doc.blocks().at(back.block("altlik")).path,
             fs::path(moved).lexically_normal().string());
}

TEST_CASE("DIŞREFERANS: proje klasörü taşınınca referansı göreli yoluyla bulunur")
{
    TempDir from("tasi-a");
    TempDir to("tasi-b");
    fs::create_directories(from.path() / "altliklar");
    const std::string source  = (from.path() / "altliklar" / "altlik.pcad").string();
    const std::string project = from.file("proje.pcad");
    write_source(source);
    {
        Rig host;
        host.run("DIŞREFERANS dosya=\"" + source + "\"");
        host.run("FARKLIKAYDET \"" + project + "\"");
    }
    // The whole folder, carried to another place, and nothing left behind.
    fs::copy(from.path(), to.path(),
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::remove_all(from.path());

    Rig back;
    back.run("AÇ \"" + to.file("proje.pcad") + "\"");
    CHECK(back.said.find("uyarı") == std::string::npos);
    CHECK_EQ(back.members("altlik"), std::size_t{3});
    CHECK_EQ(back.doc.blocks().at(back.block("altlik")).path,
             (to.path() / "altliklar" / "altlik.pcad").lexically_normal().string());
}

TEST_CASE("DIŞREFERANS: yolu kaybolan referans proje klasöründe adıyla bulunur")
{
    TempDir tmp("yaninda");
    TempDir far("uzak");
    const std::string source  = far.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);
    {
        Rig host;
        host.run("DIŞREFERANS dosya=\"" + source + "\"");
        host.run("FARKLIKAYDET \"" + project + "\"");
    }
    // Sent by e-mail: the base map arrives beside the project, not where it was.
    fs::copy_file(source, tmp.file("altlik.pcad"));
    fs::remove(source);

    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK_EQ(back.members("altlik"), std::size_t{3});
    CHECK(back.said.find("proje klasöründe bulundu") != std::string::npos);
}

TEST_CASE("DIŞREFERANS: boşalt ve yükle; boşaltılan açılışta okunmaz")
{
    TempDir tmp("bosalt");
    const std::string source  = tmp.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);

    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\"");
    host.run("DIŞREFERANS islem=bosalt ad=altlik");
    CHECK_EQ(host.members("altlik"), std::size_t{0});
    CHECK_EQ(host.members("altlik|KAPAK"), std::size_t{0});
    CHECK((host.doc.blocks().at(host.block("altlik")).flags & core::kBlockUnloaded) != 0);
    CHECK_EQ(host.references().size(), std::size_t{1});
    host.run("FARKLIKAYDET \"" + project + "\"");

    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK_EQ(back.members("altlik"), std::size_t{0});
    back.run("DIŞREFERANS islem=yukle ad=altlik");
    CHECK_EQ(back.members("altlik"), std::size_t{3});
    CHECK((back.doc.blocks().at(back.block("altlik")).flags & core::kBlockUnloaded) == 0);
}

TEST_CASE("DIŞREFERANS: bağla çizime katar, kaldır referanslarıyla siler")
{
    TempDir tmp("bagla-kaldir");
    const std::string source  = tmp.file("altlik.pcad");
    const std::string project = tmp.file("proje.pcad");
    write_source(source);

    {
        Rig host;
        host.run("DIŞREFERANS dosya=\"" + source + "\"");
        host.run("DIŞREFERANS islem=bagla ad=altlik");
        CHECK_EQ(host.doc.blocks().at(host.block("altlik")).flags, 0);
        CHECK_EQ(host.doc.blocks().at(host.block("altlik|KAPAK")).flags, 0);
        CHECK(host.doc.external_rows().empty());
        host.run("FARKLIKAYDET \"" + project + "\"");
        // A bound block is an ordinary one: it can be edited now.
        CHECK(host.refused("BLOKDÜZENLE ad=altlik").empty());
    }
    fs::remove(source); // the bound drawing no longer needs it

    Rig back;
    back.run("AÇ \"" + project + "\"");
    CHECK(back.said.find("uyarı") == std::string::npos);
    CHECK_EQ(back.members("altlik"), std::size_t{3});

    // Taken off: its references go with it; its name can be attached again.
    write_source(source);
    Rig other;
    other.run("DIŞREFERANS dosya=\"" + source + "\" ad=ALTLIK2");
    other.run("DIŞREFERANS islem=kaldir ad=ALTLIK2");
    CHECK(other.references().empty());
    CHECK_EQ(other.members("ALTLIK2"), std::size_t{0});
    other.said.clear();
    other.run("DIŞREFERANS islem=listele");
    CHECK(other.said.find("ALTLIK2") == std::string::npos);
    other.run("DIŞREFERANS dosya=\"" + source + "\" ad=ALTLIK2");
    CHECK_EQ(other.members("ALTLIK2"), std::size_t{3});
}

TEST_CASE("DIŞREFERANS: ad, dosya ve sistem denetlenir")
{
    TempDir tmp("denetim");
    const std::string source = tmp.file("altlik.pcad");
    write_source(source);

    Rig host;
    CHECK(host.refused("DIŞREFERANS").find("dosya=") != std::string::npos);
    CHECK(host.refused("DIŞREFERANS dosya=\"" + tmp.file("yok.pcad") + "\"").find("bulunamadı") !=
          std::string::npos);
    CHECK(host.refused("DIŞREFERANS dosya=\"" + source + "\" ad=A|B").find("'|'") !=
          std::string::npos);
    host.run("DAİRE merkez=0,0 cevre=1,0");
    host.run("BLOK ad=altlik taban=0,0 nesneler=1");
    CHECK(host.refused("DIŞREFERANS dosya=\"" + source + "\"").find("zaten var") !=
          std::string::npos);
    CHECK(host.refused("DIŞREFERANS islem=yenile ad=altlik").find("dış referans yok") !=
          std::string::npos);

    // Another coordinate system is refused, not drawn kilometres away.
    Rig other_system;
    other_system.run("AYAR core.crs.id EPSG:5255");
    const std::string elsewhere = tmp.file("baska.pcad");
    other_system.run("ÇİZGİ 0,0 1,0");
    other_system.run("FARKLIKAYDET \"" + elsewhere + "\"");
    Rig mine;
    mine.run("AYAR core.crs.id EPSG:5254");
    CHECK(mine.refused("DIŞREFERANS dosya=\"" + elsewhere + "\"").find("koordinat sisteminde") !=
          std::string::npos);
}

TEST_CASE("DIŞREFERANS: kılavuz sayfasının örnekleri yazıldığı gibi çalışır")
{
    // docs/komutlar/xref.md, line by line, against the files it names — the
    // manual's checker runs without a file engine and leaves file commands to
    // their own tests (test_docs.cpp), so this is where these examples run.
    TempDir tmp("kilavuz");
    const std::string crs = "EPSG:5254";
    write_source(tmp.file("altlik.pcad"), false, crs);
    fs::create_directories(tmp.path() / "arsiv");
    write_source((tmp.path() / "arsiv" / "altlik.pcad").string(), false, crs);
    {
        Rig neighbour;
        neighbour.run("AYAR core.crs.id " + crs);
        neighbour.run("ÇİZGİ 0,0 30,0 30,20");
        neighbour.run("DIŞAAKTAR \"" + tmp.file("komsu.dxf") + "\"");
    }

    std::ifstream page(std::string(KENTOS_DOCS_DIR) + "/komutlar/xref.md");
    REQUIRE(page.good());
    std::vector<std::string> lines;
    std::vector<std::pair<std::size_t, std::string>> expected; ///< after which line
    std::string line;
    bool examples = false;
    bool fenced   = false;
    while (std::getline(page, line)) {
        if (line.rfind("## ", 0) == 0) examples = line == "## Örnekler";
        if (!examples) continue;
        if (line.rfind("```", 0) == 0) {
            fenced = line == "```text" && !fenced ? true : false;
            continue;
        }
        if (!fenced) continue;
        if (line.rfind("DIŞREFERANS", 0) == 0)
            lines.push_back(line);
        else if (line.rfind("'", 0) == 0)
            expected.emplace_back(lines.size(), line);
    }
    REQUIRE(lines.size() >= 9);

    Rig host;
    host.run("AYAR core.crs.id " + crs);
    host.run("FARKLIKAYDET \"" + tmp.file("proje.pcad") + "\"");
    for (std::size_t i = 0; i < lines.size(); ++i) {
        host.said.clear();
        host.run(lines[i]);
        for (const auto& [after, words] : expected)
            if (after == i + 1)
                CHECK_MESSAGE(host.said.find(words) != std::string::npos,
                              lines[i] << " dedi: " << host.said);
    }
    // Where the page leaves the drawing: altlik bound into it, KOMSU gone.
    CHECK_EQ(host.doc.blocks().at(host.block("altlik")).flags, 0);
    CHECK_EQ(host.members("altlik"), std::size_t{3});
    CHECK((host.doc.blocks().at(host.block("KOMSU")).flags & core::kBlockDetached) != 0);

    // And the page's script, in a drawing of its own beside the same files.
    Rig scripted;
    scripted.run("AYAR core.crs.id " + crs);
    scripted.run("FARKLIKAYDET \"" + tmp.file("betik.pcad") + "\"");
    std::ifstream again(std::string(KENTOS_DOCS_DIR) + "/komutlar/xref.md");
    std::stringstream whole;
    whole << again.rdbuf();
    const std::string text = whole.str();
    const std::size_t from = text.find("```json\n");
    REQUIRE(from != std::string::npos);
    const std::size_t to = text.find("```", from + 8);
    script::JsonRunner runner(scripted.bus, script::Sandbox::Project);
    auto ran = runner.run_text(text.substr(from + 8, to - from - 8));
    if (!ran) FAIL_WITH("betik", ran.error().message);
    CHECK_EQ(scripted.members("altlik"), std::size_t{3});
}

TEST_CASE("DIŞREFERANS: liste durumları ve her adımın raporu panelle komutta aynı")
{
    // The panel and `islem=listele` read one listing
    // (`command::list_external_references`); every step reports which
    // references it acted on, which is how the panel knows whose file was read
    // again after a change on disk.
    TempDir tmp("liste");
    const std::string source = tmp.file("altlik.pcad");
    write_source(source);

    Rig host;
    const auto step = [&host](const std::string& line) {
        auto r = host.bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
        return r.value().report;
    };
    const auto names = [](const core::Json& report) {
        std::vector<std::string> out;
        if (const core::Json* list = report.find("adlar"); list != nullptr)
            for (const core::Json& n : list->as_array())
                out.push_back(n.as_string());
        return out;
    };
    const auto state_of = [&host](const std::string& name) {
        for (const ExternalListing& row : list_external_references(host.doc))
            if (row.name == name) return row.state;
        return ExternalListing::State::Empty;
    };

    const core::Json attached = step("DIŞREFERANS dosya=\"" + source + "\"");
    CHECK_EQ(attached.find("islem")->as_string(), std::string("ekle"));
    CHECK_EQ(names(attached), std::vector<std::string>{"altlik"});
    REQUIRE_EQ(list_external_references(host.doc).size(), std::size_t{1});
    const ExternalListing row = list_external_references(host.doc).front();
    CHECK_EQ(row.state, ExternalListing::State::Loaded);
    CHECK_EQ(row.members, std::size_t{3});
    CHECK_EQ(row.references, std::size_t{1});
    CHECK_EQ(std::string(external_state_word(row.state)), std::string("yüklü"));

    CHECK_EQ(names(step("DIŞREFERANS islem=yenile")), std::vector<std::string>{"altlik"});
    CHECK_EQ(names(step("DIŞREFERANS islem=bosalt ad=altlik")), std::vector<std::string>{"altlik"});
    CHECK_EQ(state_of("altlik"), ExternalListing::State::Unloaded);
    // Unloaded references are not read by a reload of all.
    CHECK(names(step("DIŞREFERANS islem=yenile")).empty());
    CHECK_EQ(names(step("DIŞREFERANS islem=yukle ad=altlik")), std::vector<std::string>{"altlik"});
    CHECK_EQ(state_of("altlik"), ExternalListing::State::Loaded);

    fs::rename(source, tmp.file("uzakta.pcad"));
    CHECK_EQ(state_of("altlik"), ExternalListing::State::Missing);
    const core::Json listed = step("DIŞREFERANS islem=listele");
    REQUIRE(listed.find("dis_referanslar") != nullptr);
    CHECK_EQ(listed.find("dis_referanslar")->as_array().front().find("durum")->as_string(),
             std::string("bulunamadı"));
    CHECK_EQ(
        names(step("DIŞREFERANS islem=yol ad=altlik dosya=\"" + tmp.file("uzakta.pcad") + "\"")),
        std::vector<std::string>{"altlik"});
    CHECK_EQ(names(step("DIŞREFERANS islem=kaldir ad=altlik")), std::vector<std::string>{"altlik"});
    CHECK(list_external_references(host.doc).empty());
}
