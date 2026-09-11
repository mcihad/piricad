// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: the processing tools (processing.md).
//
// What is proven here: a tool is a command with the four shared parameters;
// its scope resolves to the objects it applies to and no others; its output
// lands on the layer asked for as ordinary objects; a stop leaves the drawing
// untouched; and the three clients produce one document and one journal.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/processing/registry.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <atomic>
#include <cmath>
#include <stop_token>
#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string said;

    Rig()
    {
        register_builtin_commands(reg);
        processing::register_processing_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
    }

    void run(const char* line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

/// One caption of the document: its text and where its centre sits.
struct Caption
{
    std::string text;
    core::Point2 at{};
    core::Point2 end{};
    core::LayerId layer{0};
};

std::vector<Caption> captions_of(const core::Document& doc)
{
    std::vector<Caption> out;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;
        const std::uint32_t slot = doc.entities().slot[e];
        if (!doc.texts().has(slot)) continue;
        const core::RingSpan rs = doc.geometry().rings_of(slot);
        const auto xs           = doc.geometry().ring_xs(rs.first);
        const auto ys           = doc.geometry().ring_ys(rs.first);
        out.push_back(Caption{std::string(doc.texts().text(slot)), core::Point2{xs[0], ys[0]},
                              core::Point2{xs[1], ys[1]}, doc.entities().layer[e]});
    }
    return out;
}

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

} // namespace

TEST_CASE("İŞLEM: her araç bir komuttur; dört ortak parametre önde, kendi parametreleri arkada")
{
    Rig f;
    REQUIRE_EQ(processing::processing_tools().size(), 3u);

    const CommandSpec* uz = f.reg.resolve("UZUNLUKYAZ");
    REQUIRE(uz != nullptr);
    CHECK_EQ(uz->id, std::string("islem.uzunluk_yaz"));
    CHECK_EQ(uz->category, Category::Processing);
    REQUIRE(uz->params.size() >= 5u);
    CHECK_EQ(uz->params[0].name, std::string("nesneler"));
    CHECK_EQ(uz->params[1].name, std::string("kapsam"));
    CHECK_EQ(uz->params[2].name, std::string("pencere"));
    CHECK_EQ(uz->params[3].name, std::string("katman"));
    CHECK_EQ(uz->params[4].name, std::string("birim"));
    CHECK(f.reg.resolve("LABELLENGTH") == uz);
    CHECK(f.reg.resolve("UZY") == uz);

    const CommandSpec* kn = f.reg.resolve("KÖŞENUMARALA");
    REQUIRE(kn != nullptr);
    CHECK_EQ(kn->id, std::string("islem.kose_numarala"));
    CHECK(f.reg.resolve("KOSENUMARALA") == kn);

    // The registry finds a tool by id and by any name, Turkish-folded.
    CHECK(processing::find_tool("islem.uzunluk_yaz") != nullptr);
    CHECK(processing::find_tool("uzunlukyaz") != nullptr);
    CHECK(processing::find_tool("yok.boyle.bir.arac") == nullptr);

    // What it applies to, said for the panel and the docs.
    const auto& spec = processing::find_tool("islem.uzunluk_yaz")->spec();
    CHECK(processing::applies_to(spec.applies, processing::Applies::Lines));
    CHECK(processing::applies_to(spec.applies, processing::Applies::Faces));
    CHECK_FALSE(processing::applies_to(spec.applies, processing::Applies::Points));
    CHECK_EQ(spec.group, std::string("Etiketleme"));
}

TEST_CASE("UZUNLUKYAZ: çizginin uzunluğunu kenara paralel, üstüne, istenen birim ve biçimde yazar")
{
    Rig f;
    f.run("KATMAN ad=YOL");
    f.run("ÇİZGİ 0,0 10,0");

    f.run("UZUNLUKYAZ nesneler=1");
    auto caps = captions_of(f.doc);
    REQUIRE_EQ(caps.size(), 1u);
    CHECK_EQ(caps[0].text, std::string("10,00 m"));
    // Parallel: the baseline runs along the line; above it: the left of a
    // left-to-right edge.
    CHECK_EQ(caps[0].at.y, caps[0].end.y);
    CHECK(caps[0].end.x > caps[0].at.x);
    CHECK(caps[0].at.y > 0);
    CHECK_EQ(caps[0].at.x, core::Mm{5000}); // centred on the edge
    // On the active layer when no `katman` was given.
    CHECK_EQ(caps[0].layer, f.doc.find_layer("YOL"));
    CHECK(f.said.find("1 nesneye uygulandı, 1 nesne üretildi") != std::string::npos);

    // One undo step removes them all.
    f.run("GERİAL");
    CHECK(captions_of(f.doc).empty());

    // The unit, the decimals, the separator and the format are the user's.
    f.run("UZUNLUKYAZ nesneler=1 birim=santimetre ondalik=0 bicim=\"L={}\" ayrac=nokta "
          "katman=ETİKET");
    caps = captions_of(f.doc);
    REQUIRE_EQ(caps.size(), 1u);
    CHECK_EQ(caps[0].text, std::string("L=1000"));
    CHECK_EQ(caps[0].layer, f.doc.find_layer("ETİKET"));
    CHECK(f.doc.find_layer("ETİKET") != f.doc.find_layer("YOL"));

    // A bad choice is refused by name, and nothing is drawn.
    f.run("GERİAL");
    f.said.clear();
    f.run("UZUNLUKYAZ nesneler=1 birim=parsek");
    CHECK(captions_of(f.doc).empty());
    CHECK(f.said.find("'birim' için tanınmayan değer: 'parsek'") != std::string::npos);
}

TEST_CASE("UZUNLUKYAZ: kapalı alanda her kenar dışa yazılır; kısa kenar atlanır")
{
    Rig f;
    f.run("KATMAN ad=PARSEL");
    f.run("ALAN 0,0 40,0 40,30 0,30");
    f.run("UZUNLUKYAZ nesneler=1 taraf=dis");
    const auto caps = captions_of(f.doc);
    REQUIRE_EQ(caps.size(), 4u);
    // Every caption sits OUTSIDE the parcel.
    for (const Caption& c : caps)
        CHECK((c.at.x < 0 || c.at.x > 40000 || c.at.y < 0 || c.at.y > 30000));
    CHECK_EQ(caps[0].text, std::string("40,00 m"));
    CHECK_EQ(caps[1].text, std::string("30,00 m"));

    f.run("GERİAL");
    f.run("UZUNLUKYAZ nesneler=1 enaz=35000");
    CHECK_EQ(captions_of(f.doc).size(), 2u); // the two 40 m edges only
}

TEST_CASE("İŞLEM: kapsam — seçim, görünüm penceresi ve bütün proje; uygun olmayan nesne sayılır")
{
    Rig f;
    f.run("KATMAN ad=YOL");
    f.run("ÇİZGİ 0,0 10,0");        // 1
    f.run("ÇİZGİ 100,100 110,100"); // 2
    f.run("NOKTA 50,50");           // 3: not a line, never labelled

    // The whole project: both lines, the point counted as skipped.
    f.run("UZUNLUKYAZ kapsam=proje");
    CHECK_EQ(captions_of(f.doc).size(), 2u);
    CHECK(f.said.find("1 nesne bu araca uygun olmadığı için atlandı") != std::string::npos);
    f.run("GERİAL");

    // The viewport: only what the window touches. The two corners are given as
    // two `pencere=` values, the way a named list grows at the command line.
    f.run("UZUNLUKYAZ kapsam=gorunum pencere=-1,-1 pencere=20,20");
    REQUIRE_EQ(captions_of(f.doc).size(), 1u);
    CHECK_EQ(captions_of(f.doc)[0].at.x, core::Mm{5000});
    f.run("GERİAL");

    // The selection, which is the default.
    f.run("SEÇ nesneler=2");
    f.run("UZUNLUKYAZ");
    REQUIRE_EQ(captions_of(f.doc).size(), 1u);
    CHECK_EQ(captions_of(f.doc)[0].at.x, core::Mm{105000});
    f.run("GERİAL");

    // A window needs its two corners.
    f.said.clear();
    f.run("UZUNLUKYAZ kapsam=gorunum");
    CHECK(captions_of(f.doc).empty());
    CHECK(f.said.find("görünümün iki köşesini ister") != std::string::npos);

    // Nothing in scope fits: said, and nothing drawn.
    f.said.clear();
    f.run("UZUNLUKYAZ nesneler=3");
    CHECK(captions_of(f.doc).empty());
    CHECK(f.said.find("uygun nesne yok") != std::string::npos);
}

TEST_CASE("KÖŞENUMARALA: seçilen köşeden başlar, saat yönünün tersine sayar, biçimi kullanıcının")
{
    Rig f;
    f.run("KATMAN ad=PARSEL");
    f.run("ALAN 0,0 10,0 10,10 0,10");

    f.run("KÖŞENUMARALA nesneler=1 baslangic=10,10 onek=A basamak=5");
    const auto caps = captions_of(f.doc);
    REQUIRE_EQ(caps.size(), 4u);
    CHECK_EQ(caps[0].text, std::string("A00001"));
    CHECK_EQ(caps[1].text, std::string("A00002"));
    CHECK_EQ(caps[3].text, std::string("A00004"));
    // A00001 sits outside the corner (10,10): up and to the right of it.
    CHECK(caps[0].at.x > 10000);
    CHECK(caps[0].at.y > 10000);
    // Counter-clockwise from (10,10): next is (0,10), outside it up-left.
    CHECK(caps[1].at.x < 0);
    CHECK(caps[1].at.y > 10000);
    // Horizontal text.
    CHECK_EQ(caps[0].at.y, caps[0].end.y);

    // Clockwise, from 7, no padding, with a suffix.
    f.run("GERİAL");
    f.run("KÖŞENUMARALA nesneler=1 baslangic=10,10 yon=saat ilk=7 sonek=\".\"");
    const auto cw = captions_of(f.doc);
    REQUIRE_EQ(cw.size(), 4u);
    CHECK_EQ(cw[0].text, std::string("7."));
    CHECK_EQ(cw[1].text, std::string("8."));
    // Clockwise from (10,10) goes to (10,0): outside it, down-right.
    CHECK(cw[1].at.x > 10000);
    CHECK(cw[1].at.y < 0);

    // Every number on a named layer, in one undo step.
    f.run("GERİAL");
    f.run("KÖŞENUMARALA nesneler=1 katman=NUMARA basamak=2 dolgu=x");
    const auto padded = captions_of(f.doc);
    REQUIRE_EQ(padded.size(), 4u);
    CHECK_EQ(padded[0].text, std::string("x1"));
    CHECK_EQ(padded[0].layer, f.doc.find_layer("NUMARA"));
    f.run("GERİAL");
    CHECK(captions_of(f.doc).empty());
}

TEST_CASE("İŞLEM: durdurulan araç hiçbir şey üretmez")
{
    const processing::ProcessingTool* tool = processing::find_tool("islem.uzunluk_yaz");
    REQUIRE(tool != nullptr);

    processing::ToolInput input;
    processing::InputEntity line;
    line.cls  = processing::Applies::Lines;
    line.kind = core::kPolylineKind;
    line.rings.push_back(processing::InputEntity::Ring{{{0, 0}, {10000, 0}}, core::RingRole::Open});
    input.entities.push_back(line);
    for (const auto& p : tool->spec().params)
        if (!p.fallback.empty())
            input.args.set(p.name, p.kind == ParamKind::Integer
                                       ? Value::integer(std::stoll(p.fallback))
                                       : Value::text(p.fallback));

    std::stop_source stop;
    stop.request_stop();
    std::atomic<std::uint32_t> permille{0};
    processing::ToolOutput output;
    const auto status = tool->run(input, output, processing::Progress{stop.get_token(), &permille});
    CHECK_FALSE(status.ok());
    CHECK_EQ(status.error().code, core::ErrorCode::Cancelled);
    CHECK(output.captions.empty());

    // Not stopped: one caption, and the progress figure reaches the end.
    std::stop_source go;
    processing::ToolOutput done;
    CHECK(tool->run(input, done, processing::Progress{go.get_token(), &permille}).ok());
    CHECK_EQ(done.captions.size(), 1u);
    CHECK_EQ(permille.load(), 1000u);
}

TEST_CASE("İŞLEM KANIT: komut satırı ve betik aynı belgeyi ve günlüğü üretir; günlük yeniden oynar")
{
    Rig cli;
    cli.run("KATMAN ad=YOL");
    cli.run("ÇİZGİ 0,0 12.5,0");
    cli.run("UZUNLUKYAZ nesneler=1 birim=metre ondalik=2 katman=ETİKET");

    Rig scr;
    scr.run("KATMAN ad=YOL");
    scr.run("ÇİZGİ 0,0 12.5,0");
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "İşlem kanıtı",
            "komutlar": [
                {"cmd": "islem.uzunluk_yaz",
                 "args": {"nesneler": [1], "birim": "metre", "ondalik": 2, "katman": "ETİKET"}}
            ]
        })");
        if (!r) FAIL_WITH("betik", r.error().message);
    }
    CHECK_EQ(cli.doc.content_hash(), scr.doc.content_hash());
    CHECK_EQ(what_happened(cli.journal), what_happened(scr.journal));

    // The journal names the objects and every parameter it used, so a replay on
    // an empty selection and a different view does the same thing.
    Rig replay;
    for (const auto& e : cli.journal.entries()) {
        auto r = replay.bus.dispatch(Invocation{e.command_id, e.args, Origin::Batch});
        if (!r) FAIL_WITH(e.command_id.c_str(), r.error().message);
    }
    CHECK_EQ(replay.doc.content_hash(), cli.doc.content_hash());
}

// ============================================================================
// ALANDÜZENLE — a face brought to a wanted area
// ============================================================================

#include "kentos_cad/core/area_edit.hpp"
#include "kentos_cad/core/offset.hpp"

namespace {

core::Mm2 area_of(const std::vector<core::Point2>& ring)
{
    const core::Mm2 a = core::ring_area(ring);
    return a < 0 ? -a : a;
}

std::vector<core::Point2> exterior_of(const core::Document& doc, core::EntityId e)
{
    const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
    const auto xs           = doc.geometry().ring_xs(rs.first);
    const auto ys           = doc.geometry().ring_ys(rs.first);
    std::vector<core::Point2> out;
    for (std::size_t i = 0; i < xs.size(); ++i)
        out.push_back(core::Point2{xs[i], ys[i]});
    return out;
}

} // namespace

TEST_CASE("ALAN DÜZENLEME: kenar kaydırma, köşe çekme ve her yandan daraltma hedefe oturur")
{
    // A 40 × 30 m parcel: 1 200 m². Wanted: 1 500 m².
    const std::vector<core::Point2> parcel{{0, 0}, {40000, 0}, {40000, 30000}, {0, 30000}};
    const core::Mm2 target = 1'500'000'000;

    // EDGE: the top edge (from vertex 2 to 3) slides outward to y = 37,5 m.
    const auto t = core::area_edit_edge_offset(parcel, 2, target);
    REQUIRE(t.has_value());
    CHECK(std::abs(*t - 7500.0) <= 1.0); // the area is a step function of the millimetre
    const auto lifted = core::area_edit_shift_edge(parcel, 2, *t);
    REQUIRE_EQ(lifted.size(), 4u);
    CHECK_EQ(lifted[0], parcel[0]);
    CHECK_EQ(lifted[1], parcel[1]);
    CHECK(std::abs(lifted[2].y - 37500) <= 1);
    CHECK(std::abs(lifted[3].y - 37500) <= 1);
    CHECK(std::abs(area_of(lifted) - target) <= 40'000); // a millimetre on a 40 m edge

    // VERTEX: pulling corner 2 (40,30): the places that give 1 500 m² are a line
    // parallel to the diagonal prev→next; the cursor is projected onto it.
    const auto v = core::area_edit_vertex_for(parcel, 2, core::Point2{50000, 40000}, target);
    REQUIRE(v.has_value());
    const auto pulled = core::area_edit_move_vertex(parcel, 2, *v);
    CHECK(std::abs(area_of(pulled) - target) <= 60'000);
    // Cursor ON the solution line stays put.
    const auto again = core::area_edit_vertex_for(parcel, 2, *v, target);
    REQUIRE(again.has_value());
    CHECK(std::abs(again->x - v->x) <= 1);
    CHECK(std::abs(again->y - v->y) <= 1);

    // UNIFORM: every edge out by the one distance; still four corners.
    const auto grown = core::area_edit_uniform(parcel, target);
    REQUIRE(grown.ok());
    REQUIRE_EQ(grown.value().size(), 4u);
    CHECK(std::abs(area_of(grown.value()) - target) <= 300'000); // one mm on 140 m of edge
    const auto shrunk = core::area_edit_uniform(parcel, 600'000'000);
    REQUIRE(shrunk.ok());
    CHECK(std::abs(area_of(shrunk.value()) - core::Mm2{600'000'000}) <= 300'000);
    CHECK_FALSE(core::area_edit_uniform(parcel, 0).ok());

    // THE GHOST: far from the figure it follows the hand; near it, it snaps and
    // says so; and Enter's point lands on the figure wherever the hand is.
    core::AreaEditRequest req;
    req.mode        = core::AreaEditMode::Edge;
    req.index       = 2;
    req.target      = target;
    const auto free = core::area_edit_ghost(parcel, req, core::Point2{20000, 33000}, 500);
    REQUIRE_EQ(free.points.size(), 4u);
    CHECK_FALSE(free.snapped);
    CHECK(std::abs(free.points[2].y - 33000) <= 1);
    CHECK(std::abs(free.commit.y - 37500) <= 1);
    const auto near = core::area_edit_ghost(parcel, req, core::Point2{20000, 37200}, 500);
    CHECK(near.snapped);
    CHECK(std::abs(near.points[2].y - 37500) <= 1);
    CHECK_EQ(near.area, area_of(lifted));

    // The request survives its payload.
    req.key         = 42;
    req.grab        = core::Point2{1, -2};
    const auto back = core::decode_area_edit(core::encode_area_edit(req));
    REQUIRE(back.has_value());
    CHECK(*back == req);
    CHECK_FALSE(core::decode_area_edit(std::vector<std::uint8_t>{1, 2, 3}).has_value());

    CHECK_EQ(core::format_square_metres(1'500'000'000), std::string("1500,00 m²"));
    CHECK_EQ(core::format_square_metres(1'248'714'999), std::string("1248,71 m²"));
}

TEST_CASE(
    "ALANDÜZENLE: komut satırından her yandan, kenardan ve köşeden; kimlik korunur, geri alınır")
{
    Rig f;
    f.run("KATMAN ad=PARSEL");
    f.run("ALAN 0,0 40,0 40,30 0,30");
    const core::EntityId e = f.doc.slot_of(static_cast<core::EntityKey>(std::uint64_t{1}));
    REQUIRE(e != core::kNoEntity);
    CHECK_EQ(f.doc.entity_area(e), core::Mm2{1'200'000'000});

    // Uniform, the command line's way: four corners, the same object, the figure.
    f.run("ALANDÜZENLE nesneler=1 alan=1500");
    CHECK_EQ(f.doc.live_entity_count(), std::size_t{1});
    CHECK_EQ(exterior_of(f.doc, e).size(), 4u);
    CHECK(std::abs(f.doc.entity_area(e) - core::Mm2{1'500'000'000}) <= 300'000);
    CHECK(f.said.find("1200,00 m² → 1500") != std::string::npos);
    f.run("GERİAL");
    CHECK_EQ(f.doc.entity_area(e), core::Mm2{1'200'000'000});

    // One edge: only that edge's two corners move.
    f.run("ALANDÜZENLE nesneler=1 alan=1500 mod=kenar kenar=3");
    auto ring = exterior_of(f.doc, e);
    REQUIRE_EQ(ring.size(), 4u);
    CHECK_EQ(ring[0], (core::Point2{0, 0}));
    CHECK_EQ(ring[1], (core::Point2{40000, 0}));
    CHECK(std::abs(ring[2].y - 37500) <= 1);
    CHECK(std::abs(ring[3].y - 37500) <= 1);
    f.run("GERİAL");

    // One corner, along the normal of prev→next.
    f.run("ALANDÜZENLE nesneler=1 alan=1300 mod=kose kose=3");
    ring = exterior_of(f.doc, e);
    CHECK_EQ(ring[0], (core::Point2{0, 0}));
    CHECK_EQ(ring[1], (core::Point2{40000, 0}));
    CHECK_EQ(ring[3], (core::Point2{0, 30000}));
    CHECK(std::abs(f.doc.entity_area(e) - core::Mm2{1'300'000'000}) <= 60'000);
    f.run("GERİAL");

    // With a hand: the point the ghost's Enter would send lands the figure.
    f.run("ALANDÜZENLE nesneler=1 alan=1500 mod=kenar kenar=3 nokta=20,37.5");
    CHECK(std::abs(f.doc.entity_area(e) - core::Mm2{1'500'000'000}) <= 40'000);
    f.run("GERİAL");

    // Refusals leave the parcel alone.
    const std::uint64_t before = f.doc.content_hash();
    f.said.clear();
    f.run("ALANDÜZENLE nesneler=1 alan=0");
    CHECK_EQ(f.doc.content_hash(), before);
    CHECK(f.said.find("sıfırdan büyük") != std::string::npos);
    f.run("ALANDÜZENLE nesneler=1 alan=1500 mod=kenar kenar=9");
    CHECK_EQ(f.doc.content_hash(), before);
    f.run("ÇİZGİ 100,100 110,100");
    f.said.clear();
    f.run("ALANDÜZENLE nesneler=2 alan=1500");
    CHECK(f.said.find("uygun nesne yok") != std::string::npos);

    // The spec: a modify command, in place, no output layer.
    const CommandSpec* spec = f.reg.resolve("ALANDÜZENLE");
    REQUIRE(spec != nullptr);
    CHECK_EQ(spec->category, Category::Modify);
    CHECK(f.reg.resolve("ADJUSTAREA") == spec);
}
