// SPDX-License-Identifier: GPL-3.0-or-later
// BLOKKIRP — a block reference or an external reference clipped to a boundary
// (TODOS C-14, fourth stage; model.md R45).
//
// What this file holds, one case per sentence of the promise:
//   * a reference without a clip is written exactly as before, and a clipped
//     one carries its boundary through the payload and back;
//   * what a clip hides is not drawn, not snapped to and not picked, and the
//     reference's box is the box of what it still draws;
//   * the boundary lives where the definition is, so it moves, turns and
//     scales with the reference;
//   * a face the boundary cuts keeps its fill inside and is never stroked
//     along the cut;
//   * an external reference stays clipped across a reload and a save, and a
//     file holding a clip asks for a reader that knows one;
//   * every refusal says what was wrong, and every step is one undo step.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/io/format.hpp"
#include "kentos_cad/io/service.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
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
    core::Json report;

    Rig()
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(r.ok(), line << ": " << (r.ok() ? std::string() : r.error().message));
        report = r.value().report;
    }

    std::string refused(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        return r.ok() ? std::string() : r.error().message;
    }

    core::EntityId slot(std::int64_t key) const
    {
        return doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    }

    /// The newest live block reference on the sheet, by key.
    std::int64_t last_reference() const
    {
        std::int64_t found = 0;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().standalone(e) &&
                doc.entities().kind[e] == core::kBlockReferenceKind)
                found = static_cast<std::int64_t>(core::raw(doc.key_of(e)));
        return found;
    }

    /// The newest live object of `kind` on the sheet, by key.
    std::int64_t newest(core::KindId kind) const
    {
        std::int64_t found = 0;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().standalone(e) && doc.entities().kind[e] == kind)
                found = std::max(found, static_cast<std::int64_t>(core::raw(doc.key_of(e))));
        return found;
    }

    core::BlockReference reference(std::int64_t key) const
    {
        return core::block_reference_of(doc.geometry(), doc.entities().slot[slot(key)]).value();
    }

    core::Box2 box(std::int64_t key) const { return doc.entities().box_of(slot(key)); }

    /// What reference `key` draws, as runs.
    core::EmitBuffer drawn(std::int64_t key) const
    {
        core::EmitBuffer out;
        core::entity_outline(doc, slot(key), out);
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
        path_ = fs::temp_directory_path() / (std::string("kentoscad-blokkirp-") + tag);
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

std::uint32_t min_reader_of(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    io::FileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header.min_reader_version;
}

/// A block of a 100 m line along x and a 10 m circle round (80, 0), placed
/// where it was drawn by BLOK itself. The reference's key.
std::int64_t line_and_circle(Rig& r)
{
    r.run("ÇİZGİ 0,0 100,0");
    r.run("DAİRE merkez=80,0 cevre=90,0");
    r.run("BLOK ad=B taban=0,0 nesneler=1 nesneler=2");
    return r.last_reference();
}

/// Whether any run of `buf` that is a line of the drawing has an edge lying
/// along the vertical line x = `x`.
bool stroked_along_x(const core::EmitBuffer& buf, core::Mm x)
{
    for (std::size_t r = 0; r < buf.run_total(); ++r) {
        if (!buf.run_edge(r)) continue;
        const auto xs       = buf.run_xs(r);
        const std::size_t n = xs.size();
        const std::size_t e = buf.run_closed[r] != 0 ? n : (n == 0 ? 0 : n - 1);
        for (std::size_t v = 0; v < e; ++v)
            if (xs[v] == x && xs[(v + 1) % n] == x) return true;
    }
    return false;
}

} // namespace

TEST_CASE("BLOKKIRP: kırpmasız yük eskisiyle bayt bayt aynı; kırpılmış yük sınırıyla gidip gelir")
{
    core::BlockReference ref;
    ref.block                             = 3;
    ref.rotation_udeg                     = 30'000'000;
    ref.bounds                            = core::Box2{0, 0, 10'000, 5'000};
    const std::vector<std::uint8_t> plain = core::encode_block_reference(ref);
    // Layout 1, as every reference was written before clips existed.
    REQUIRE(plain.size() > 2);
    CHECK_EQ(static_cast<int>(plain[0] | (plain[1] << 8)), int{core::kBlockReferenceLayout});
    auto back = core::decode_block_reference(plain);
    REQUIRE(back);
    CHECK(back.value() == ref);

    ref.clip                                = {{0, 0}, {10'000, 0}, {10'000, 5'000}};
    const std::vector<std::uint8_t> clipped = core::encode_block_reference(ref);
    CHECK_EQ(static_cast<int>(clipped[0] | (clipped[1] << 8)),
             int{core::kBlockReferenceClipLayout});
    CHECK_EQ(clipped.size(), plain.size() + 4 + 16 * 3);
    auto again = core::decode_block_reference(clipped);
    REQUIRE(again);
    CHECK(again.value() == ref);

    // A count off disk is a claim, and one the bytes do not bear is refused.
    std::vector<std::uint8_t> lying = clipped;
    lying[plain.size()]             = 200;
    CHECK_FALSE(core::decode_block_reference(lying));
    std::vector<std::uint8_t> short_one = clipped;
    short_one.pop_back();
    CHECK_FALSE(core::decode_block_reference(short_one));
    // Two corners bound nothing.
    core::BlockReference two        = ref;
    two.clip                        = {{0, 0}, {1'000, 0}};
    std::vector<std::uint8_t> bytes = core::encode_block_reference(two);
    CHECK_FALSE(core::decode_block_reference(bytes));
    // A layout-1 payload with bytes after it is not a clip in disguise.
    std::vector<std::uint8_t> trailing = plain;
    trailing.push_back(0);
    CHECK_FALSE(core::decode_block_reference(trailing));
}

TEST_CASE("BLOKKIRP: sınırın dışı çizilmez, yakalanmaz, seçilmez; kutu küçülür; kaldırınca döner")
{
    Rig r;
    const std::int64_t ref = line_and_circle(r);
    REQUIRE(ref != 0);
    CHECK_EQ(r.box(ref).max_x, 100'000);
    const std::size_t depth = r.undo.undo_depth();

    r.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=-10,-20 50,20");
    CHECK_EQ(r.undo.undo_depth(), depth + 1);
    CHECK_EQ(r.reference(ref).clip.size(), std::size_t{4});
    CHECK(r.said.find("4 köşeli sınırla kırpıldı") != std::string::npos);
    const core::Json* corners = r.report.find("kose");
    REQUIRE(corners != nullptr);
    CHECK_EQ(corners->as_int(), 4);

    // DRAWN: the line up to the boundary, the circle not at all.
    const core::EmitBuffer runs = r.drawn(ref);
    REQUIRE_EQ(runs.run_total(), std::size_t{1});
    CHECK_EQ(runs.run_xs(0).front(), 0);
    CHECK_EQ(runs.run_xs(0).back(), 50'000);
    const core::Box2 box = r.box(ref);
    CHECK_EQ(box.min_x, 0);
    CHECK_EQ(box.max_x, 50'000);

    // SNAPPED: the shown end is offered, the hidden end, middle and centre not.
    CHECK_EQ(r.snap(Point2{60, 40}, core::SnapEndpoint).point, (Point2{0, 0}));
    CHECK(r.snap(Point2{100'060, 40}, core::SnapEndpoint).mode != core::SnapEndpoint);
    CHECK(r.snap(Point2{50'060, 40}, core::SnapMidpoint).mode != core::SnapMidpoint);
    CHECK(r.snap(Point2{80'050, 50}, core::SnapCenter).mode != core::SnapCenter);
    CHECK_EQ(r.snap(Point2{30'000, 100}, core::SnapNearest).point, (Point2{30'000, 0}));
    CHECK(r.snap(Point2{75'000, 100}, core::SnapNearest).mode != core::SnapNearest);

    // PICKED where it is drawn, and missed where it is hidden.
    CHECK_EQ(core::pick_nearest(r.doc, Point2{25'000, 20}, 100), r.slot(ref));
    CHECK_EQ(core::pick_nearest(r.doc, Point2{75'000, 20}, 100), core::kNoEntity);
    CHECK_EQ(core::pick_nearest(r.doc, Point2{90'000, 20}, 100), core::kNoEntity);

    // TAKEN OFF: everything is back, in one more undo step.
    r.run("BLOKKIRP islem=kaldir nesne=" + std::to_string(ref));
    CHECK(r.reference(ref).clip.empty());
    CHECK_EQ(r.box(ref).max_x, 100'000);
    CHECK_EQ(r.snap(Point2{100'060, 40}, core::SnapEndpoint).point, (Point2{100'000, 0}));
    CHECK(r.said.find("yeniden bütün çiziliyor") != std::string::npos);

    // And each step undoes on its own.
    r.run("GERİAL");
    CHECK_EQ(r.box(ref).max_x, 50'000);
    r.run("GERİAL");
    CHECK(r.reference(ref).clip.empty());
    CHECK_EQ(r.box(ref).max_x, 100'000);
    CHECK_EQ(r.undo.undo_depth(), depth);
}

TEST_CASE("BLOKKIRP: sınır tanımın yerinde tutulur; referansla taşınır, döner, ölçeklenir")
{
    Rig r;
    line_and_circle(r);
    // Turned a quarter and doubled: the line runs from (200, 0) up to
    // (200, 200), the circle round (200, 160) with a radius of 20.
    r.run("BLOKEKLE ad=B nokta=200,0 aci=90 olcek=2");
    const std::int64_t ref = r.last_reference();
    r.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=190,-10 210,100");

    // Kept in the definition's own millimetres: the world rectangle carried
    // back through the turn and the scale.
    const std::vector<Point2> clip = r.reference(ref).clip;
    REQUIRE_EQ(clip.size(), std::size_t{4});
    for (const Point2 p : clip) {
        CHECK(p.x >= -5'000);
        CHECK(p.x <= 50'000);
        CHECK(p.y >= -5'000);
        CHECK(p.y <= 5'000);
    }
    core::Box2 box = r.box(ref);
    CHECK_EQ(box.min_y, 0);
    CHECK_EQ(box.max_y, 100'000);
    CHECK(r.snap(Point2{200'050, 160'050}, core::SnapCenter).mode != core::SnapCenter);

    // MOVED, and the clip goes with it: the same piece, ten metres east.
    r.run("TAŞI nesneler=" + std::to_string(ref) + " baslangic=0,0 bitis=10,0");
    box = r.box(ref);
    CHECK_EQ(box.min_x, 210'000);
    CHECK_EQ(box.max_x, 210'000);
    CHECK_EQ(box.max_y, 100'000);
    CHECK_EQ(r.snap(Point2{210'060, 40}, core::SnapEndpoint).point, (Point2{210'000, 0}));
    CHECK(r.snap(Point2{210'060, 200'040}, core::SnapEndpoint).mode != core::SnapEndpoint);

    // SCALED about its insertion point: the piece is twice as long.
    r.run("ÖLÇEKLE nesneler=" + std::to_string(ref) + " merkez=210,0 carpan=2");
    CHECK_EQ(r.box(ref).max_y, 200'000);
}

TEST_CASE("BLOKKIRP: kesilen yüzün dolgusu içeride kalır, kesik boyunca çizgi çizilmez")
{
    Rig r;
    r.run("ALAN 0,0 40,0 40,40 0,40");
    r.run("BLOK ad=A taban=0,0 nesneler=1");
    const std::int64_t ref = r.last_reference();
    r.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=20,-10 60,50");

    const core::EmitBuffer runs = r.drawn(ref);
    std::size_t faces           = 0;
    for (std::size_t i = 0; i < runs.run_total(); ++i) {
        if (runs.run_edge(i)) continue;
        ++faces;
        CHECK(runs.run_closed[i] != 0);
        const auto xs = runs.run_xs(i);
        const auto ys = runs.run_ys(i);
        REQUIRE_EQ(xs.size(), std::size_t{4});
        core::Box2 face;
        for (std::size_t v = 0; v < xs.size(); ++v)
            face.extend(Point2{xs[v], ys[v]});
        CHECK_EQ(face.min_x, 20'000);
        CHECK_EQ(face.max_x, 40'000);
        CHECK_EQ(face.min_y, 0);
        CHECK_EQ(face.max_y, 40'000);
    }
    // One face, filled where the square fills; the outline kept is the
    // square's own three edges inside, never the cut at x = 20.
    CHECK_EQ(faces, std::size_t{1});
    CHECK_FALSE(stroked_along_x(runs, 20'000));
    CHECK(stroked_along_x(runs, 40'000));

    // A click inside the shown half is on it; one in the hidden half is not;
    // and the cut offers no point to snap to.
    CHECK_EQ(core::pick_nearest(r.doc, Point2{30'000, 20'000}, 100), r.slot(ref));
    CHECK_EQ(core::pick_nearest(r.doc, Point2{10'000, 20'000}, 100), core::kNoEntity);
    CHECK(r.snap(Point2{20'050, 20'000}, core::SnapNearest).mode != core::SnapNearest);
    CHECK_EQ(r.box(ref).min_x, 20'000);
}

TEST_CASE("BLOKKIRP: dış referans kırpılır; yenilemede ve kaydedip açmada kırpma kalır")
{
    TempDir tmp("disref");
    const std::string source = tmp.file("altlik.pcad");
    {
        Rig src;
        src.run("ÇİZGİ 0,0 100,0");
        src.run("DAİRE merkez=50,20 cevre=51,20");
        src.run("BLOK ad=KAPAK taban=50,20 nesneler=2");
        src.run("BLOKEKLE ad=KAPAK nokta=80,20");
        src.run("FARKLIKAYDET \"" + source + "\"");
    }

    Rig host;
    host.run("DIŞREFERANS dosya=\"" + source + "\"");
    const std::int64_t ref = host.last_reference();
    host.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=-5,-5 60,30");
    CHECK_EQ(host.box(ref).max_x, 60'000);
    CHECK_EQ(host.snap(Point2{50'050, 20'050}, core::SnapCenter).point, (Point2{50'000, 20'000}));
    CHECK(host.snap(Point2{80'050, 20'050}, core::SnapCenter).mode != core::SnapCenter);

    // RELOADED: the file's objects come in again, and the clip is still the
    // reference's.
    host.run("DIŞREFERANS islem=yenile ad=altlik");
    CHECK_EQ(host.reference(ref).clip.size(), std::size_t{4});
    CHECK_EQ(host.box(ref).max_x, 60'000);

    // SAVED: a file holding a clip asks for a reader that knows one.
    const std::string saved = tmp.file("pafta.pcad");
    host.run("FARKLIKAYDET \"" + saved + "\"");
    CHECK_EQ(min_reader_of(saved), io::kMinReaderVersionClip);

    Rig reopened;
    reopened.run("AÇ \"" + saved + "\"");
    const std::int64_t again = reopened.last_reference();
    REQUIRE(again != 0);
    CHECK_EQ(reopened.reference(again).clip, host.reference(ref).clip);
    CHECK_EQ(reopened.box(again).max_x, 60'000);
    CHECK(reopened.snap(Point2{80'050, 20'050}, core::SnapCenter).mode != core::SnapCenter);

    // Taken off and saved again, the file asks only what an external
    // reference asks.
    reopened.run("BLOKKIRP islem=kaldir nesne=" + std::to_string(again));
    reopened.run("KAYDET");
    CHECK_EQ(min_reader_of(saved), io::kMinReaderVersionExternal);
}

TEST_CASE("BLOKKIRP: çokgen ve nesne sınırı; sınır çizgisi; her ret ne olduğunu söyler")
{
    Rig r;
    const std::int64_t ref = line_and_circle(r);
    const std::string id   = std::to_string(ref);

    // Refused by name, and nothing left behind.
    const std::size_t depth = r.undo.undo_depth();
    CHECK(r.refused("BLOKKIRP islem=kaldir nesne=" + id).find("kırpılmamış") != std::string::npos);
    CHECK(r.refused("BLOKKIRP islem=sinir nesne=" + id).find("kırpılmamış") != std::string::npos);
    CHECK(r.refused("BLOKKIRP nesne=" + id + " noktalar=200,200 300,300")
              .find("hiçbir şeyi içine almıyor") != std::string::npos);
    CHECK(
        r.refused("BLOKKIRP nesne=" + id + " noktalar=0,0 10,0 20,0").find("bir alan çevirmiyor") !=
        std::string::npos);
    // A script that forgot the boundary is told so, not answered with success.
    CHECK(r.refused("BLOKKIRP nesne=" + id).find("Kırpma sınırı verilmedi") != std::string::npos);
    CHECK(r.refused("BLOKKIRP nesne=" + id + " tur=cokgen").find("Kırpma sınırı verilmedi") !=
          std::string::npos);
    r.run("ÇİZGİ 0,50 10,50");
    const std::int64_t line = r.newest(core::kPolylineKind);
    CHECK(r.refused("BLOKKIRP nesne=" + std::to_string(line) + " noktalar=0,0 10,10")
              .find("blok referanslarını ve dış referansları kırpar") != std::string::npos);
    CHECK(r.refused("BLOKKIRP nesne=" + id + " cizgi=" + std::to_string(line))
              .find("bir alanı çevirmiyor") != std::string::npos);
    CHECK_EQ(r.undo.undo_depth(), depth + 1); ///< the line drawn above, nothing else

    // A POLYGON: three corners, the circle's centre left outside.
    r.run("BLOKKIRP nesne=" + id + " noktalar=-5,-5 60,-5 -5,30");
    CHECK_EQ(r.reference(ref).clip.size(), std::size_t{3});
    CHECK(r.snap(Point2{80'050, 50}, core::SnapCenter).mode != core::SnapCenter);

    // AN OBJECT: a circle drawn over the reference replaces the polygon, and
    // the boundary is the circle as it is drawn.
    r.run("DAİRE merkez=80,0 cevre=95,0");
    const std::int64_t ring = r.newest(core::kCircleKind);
    REQUIRE(ring != 0);
    r.run("BLOKKIRP nesne=" + id + " cizgi=" + std::to_string(ring));
    CHECK(r.reference(ref).clip.size() > 8);
    CHECK(r.said.find("önceki sınırın yerine") != std::string::npos);
    CHECK_EQ(r.snap(Point2{80'050, 50}, core::SnapCenter).point, (Point2{80'000, 0}));
    CHECK(r.snap(Point2{60, 40}, core::SnapEndpoint).mode != core::SnapEndpoint);

    // THE BOUNDARY DRAWN: a closed polyline on the active layer, where the
    // reference shows it.
    r.run("BLOKKIRP nesne=" + id + " noktalar=10,-10 40,10");
    r.run("BLOKKIRP islem=sinir nesne=" + id);
    const auto drawn = static_cast<core::EntityId>(r.doc.entities().size() - 1);
    REQUIRE(r.doc.entities().kind[drawn] == core::kPolylineKind);
    const core::Box2 outline = r.doc.entities().box_of(drawn);
    CHECK_EQ(outline.min_x, 10'000);
    CHECK_EQ(outline.max_x, 40'000);
    CHECK_EQ(outline.min_y, -10'000);
    CHECK_EQ(outline.max_y, 10'000);
}

TEST_CASE("BLOKKIRP: iç içe blokta dış referansın sınırı içteki bloğu da kırpar")
{
    Rig r;
    r.run("ÇİZGİ 0,0 100,0");
    r.run("BLOK ad=IC taban=0,0 nesneler=1");
    const std::int64_t inner = r.last_reference();
    r.run("BLOK ad=DIS taban=0,0 nesneler=" + std::to_string(inner));
    const std::int64_t outer = r.last_reference();
    r.run("BLOKKIRP nesne=" + std::to_string(outer) + " noktalar=-1,-1 30,1");
    CHECK_EQ(r.box(outer).max_x, 30'000);
    CHECK(r.snap(Point2{100'060, 40}, core::SnapEndpoint).mode != core::SnapEndpoint);
    CHECK_EQ(r.snap(Point2{60, 40}, core::SnapEndpoint).point, (Point2{0, 0}));
}

TEST_CASE("BLOKKIRP: çeyrek noktaları sınırın gösterdiği yerde; dönük ve aynalı referansta da")
{
    for (const char* placing :
         {"nokta=100,100 aci=30", "nokta=100,100 olcek=-1 olcek_y=1 aci=20"}) {
        Rig r;
        r.run("DAİRE merkez=0,0 cevre=10,0");
        r.run("BLOK ad=D taban=0,0 nesneler=1");
        r.run(std::string("BLOKEKLE ad=D ") + placing);
        const std::int64_t ref = r.last_reference();
        // The east half of the circle, as the sheet shows it.
        r.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=99,80 130,120");
        CHECK_MESSAGE(r.snap(Point2{110'040, 100'040}, core::SnapQuadrant).point ==
                          (Point2{110'000, 100'000}),
                      placing);
        CHECK_MESSAGE(r.snap(Point2{100'040, 110'040}, core::SnapQuadrant).point ==
                          (Point2{100'000, 110'000}),
                      placing);
        CHECK_MESSAGE(r.snap(Point2{90'040, 100'040}, core::SnapQuadrant).mode !=
                          core::SnapQuadrant,
                      placing);
    }
}

TEST_CASE("BLOKKIRP: PATLAT kırpmayı yok sayar ve söyler; DXF kırpmanın taşınmadığını söyler")
{
    TempDir tmp("dxf");
    Rig r;
    const std::int64_t ref = line_and_circle(r);
    r.run("BLOKKIRP nesne=" + std::to_string(ref) + " noktalar=-10,-20 50,20");

    r.run("AYAR core.crs.id EPSG:5254");
    const std::string out = tmp.file("pafta.dxf");
    r.run("DIŞAAKTAR \"" + out + "\"");
    CHECK(r.said.find("BLOKKIRP sınırı DXF'e taşınmadı") != std::string::npos);

    r.run("PATLAT nesne=" + std::to_string(ref));
    CHECK(r.said.find("kırpma sınırı yok sayıldı") != std::string::npos);
    const core::Json* rows = r.report.find("nesneler");
    REQUIRE(rows != nullptr);
    const core::Json* ignored = rows->as_array().front().find("kirpma_yok_sayildi");
    REQUIRE(ignored != nullptr);
    CHECK(ignored->as_bool());
    // The pieces are the definition's members, whole: the line to 100 m.
    CHECK(r.snap(Point2{100'060, 40}, core::SnapEndpoint).point == (Point2{100'000, 0}));
}

TEST_CASE("BLOKKIRP: yarıda Esc belgeyi değiştirmez, geri alma adımı bırakmaz")
{
    Rig r;
    const std::int64_t ref  = line_and_circle(r);
    const std::size_t depth = r.undo.undo_depth();
    const auto before       = r.doc.content_hash();
    const std::size_t lines = r.journal.size();

    // Pointed at the reference, one corner given, then Esc.
    auto started = r.bus.begin_interactive("BLOKKIRP", Origin::Gui);
    REQUIRE(started.ok());
    Session& s = *started.value();
    REQUIRE(s.supply(Value::ids({ref})).ok());
    REQUIRE(s.supply(Value::point(Point2{-10'000, -20'000})).ok());
    s.cancel();
    auto done          = r.bus.finish(s);
    const bool mutated = done.ok() && done.value().mutated;
    CHECK_FALSE(mutated);
    CHECK_EQ(r.undo.undo_depth(), depth);
    CHECK_EQ(r.doc.content_hash(), before);
    CHECK_EQ(r.journal.size(), lines);
    CHECK(r.reference(ref).clip.empty());
}
