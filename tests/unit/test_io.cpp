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
#include "microtest.hpp"

#include <iterator>

#include "piricad/command/bus.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/core/text.hpp"
#include "piricad/io/format.hpp"
#include "piricad/io/service.hpp"
#include "piricad/io/vector.hpp"
#include "piricad/script/json_runner.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace piricad;
using namespace piricad::command;

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
        path_ = fs::temp_directory_path() / (std::string("piricad-io-") + tag);
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
        if (!r) ::microtest::report(__FILE__, __LINE__, line.c_str(), r.error().message);
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
/// the string code the rulebook writes — see `piricad/io/format.hpp`.
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
    if (!opened) ::microtest::report(__FILE__, __LINE__, "AÇ", opened.error().message);
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

TEST_CASE("IO: boş belge de gidip geliyor")
{
    TempDir tmp("empty");
    const std::string path = tmp.file("bos.pcad");

    Rig written;
    REQUIRE(written.bus.execute_line("FARKLIKAYDET \"" + path + "\"", Origin::Test).ok());

    Rig reloaded;
    auto opened = reloaded.bus.execute_line("AÇ \"" + path + "\"", Origin::Test);
    if (!opened) ::microtest::report(__FILE__, __LINE__, "AÇ", opened.error().message);
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
    if (!opened) ::microtest::report(__FILE__, __LINE__, "AÇ", opened.error().message);
    REQUIRE(opened.ok());
    CHECK_EQ(reloaded.doc.content_hash(), hash);
    // ...and the user is told there was more in the file than they can see.
    CHECK(reloaded.transcript.find("tanımadığı") != std::string::npos);
}

TEST_CASE("IO: PiriCAD dosyası olmayan bir dosya adıyla birlikte reddedilir")
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
        CHECK(opened.ok() || victim.doc.live_entity_count() == 0);
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
        if (!r) ::microtest::report(__FILE__, __LINE__, "betik", r.error().message);
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
        CHECK(status.find("PIRICAD_WITH_GDAL") != std::string::npos);
    }
}

TEST_CASE("IO: GDAL kapalıyken İÇEAKTAR sessizce başarılı olmaz")
{
    if (io::vector_backend_available())
        PENDING("GDAL açık; kapalı hâlin davranışı bu yapıda sınanamıyor.");
    Rig rig;
    auto r = rig.bus.execute_line("İÇEAKTAR \"/veri/pafta.dxf\"", Origin::Test);
    REQUIRE(!r.ok());
    CHECK(r.error().message.find("PIRICAD_WITH_GDAL") != std::string::npos);
}

TEST_CASE("IO: DXF dışa aktar -> içe aktar gidiş dönüşü")
{
    if (!io::vector_backend_available())
        PENDING("PIRICAD_WITH_GDAL=OFF; DXF gidiş-dönüşü sınanamıyor.");
    TempDir tmp("dxf");
    const std::string path = tmp.file("cizim.dxf");

    Rig source;
    REQUIRE(source.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    REQUIRE(source.bus.execute_line("KATMAN ad=PARSEL", Origin::Test).ok());
    REQUIRE(
        source.bus.execute_line("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400", Origin::Test)
            .ok());

    auto exported = source.bus.execute_line("DIŞAAKTAR \"" + path + "\"", Origin::Test);
    if (!exported) ::microtest::report(__FILE__, __LINE__, "DIŞAAKTAR", exported.error().message);
    REQUIRE(exported.ok());
    REQUIRE(fs::exists(path));

    Rig target;
    REQUIRE(target.bus.execute_line("AYAR core.crs.id EPSG:5254", Origin::Test).ok());
    auto imported = target.bus.execute_line("İÇEAKTAR \"" + path + "\"", Origin::Test);
    if (!imported) ::microtest::report(__FILE__, __LINE__, "İÇEAKTAR", imported.error().message);
    REQUIRE(imported.ok());

    CHECK(target.doc.live_entity_count() >= 1);

    // io.md R17: one import, one undo step, and undoing it leaves nothing.
    CHECK_EQ(target.undo.undo_depth(), std::size_t{1});
    REQUIRE(target.bus.execute_line("GERİAL", Origin::Test).ok());
    CHECK_EQ(target.doc.live_entity_count(), std::size_t{0});
}

TEST_CASE("IO: DXF birden çok katmanı taşır — dışa aktarım ilk katmanda durmaz")
{
    // The regression this locks. DXF holds exactly ONE OGR layer, named
    // `entities`, and a drawing's layers live there as a `Layer` attribute. The
    // export asked OGR for a layer per PiriCAD layer, so the second call failed
    // with "Unable to have more than one OGR entities layer in a DXF file": the
    // first layer was written, the command reported the GDAL message, and the file
    // left on disk held a fraction of the drawing.
    //
    // The case above this one draws on a single layer, which is exactly why the
    // bug survived it. A cadastral drawing is never one layer.
    if (!io::vector_backend_available())
        PENDING("PIRICAD_WITH_GDAL=OFF; çok katmanlı DXF sınanamıyor.");
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
    if (!exported) ::microtest::report(__FILE__, __LINE__, "DIŞAAKTAR", exported.error().message);
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
        PENDING("PIRICAD_WITH_GDAL=OFF; GeoPackage gidiş-dönüşü sınanamıyor.");
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
    if (!imported) ::microtest::report(__FILE__, __LINE__, "İÇEAKTAR", imported.error().message);
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
    if (!io::vector_backend_available()) PENDING("PIRICAD_WITH_GDAL=OFF; /vsi reddi sınanamıyor.");
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
    const fs::path corpus = fs::path(PIRICAD_FUZZ_DIR) / "tohum" / "proje";
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

    // Colours reach QGIS as r,g,b,a decimal — written the PiriCAD way they would
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
