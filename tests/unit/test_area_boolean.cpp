// SPDX-License-Identifier: GPL-3.0-or-later
// Public area operations: measured geometry, atomic edits and equal clients.
#include "piricad_test.hpp"

#include "piricad/command/area_face.hpp"
#include "piricad/command/bus.hpp"
#include "piricad/command/job.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/core/kernel.hpp"
#include "piricad/io/service.hpp"
#include "piricad/io/vector.hpp"
#include "piricad/script/json_runner.hpp"

#include <array>
#include <cmath>
#include <string>
#include <thread>

using namespace piricad;
using namespace piricad::command;

namespace {
struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};

    Rig() { register_builtin_commands(reg); }

    DispatchResult run(const std::string& line)
    {
        auto done = bus.execute_line(line, Origin::Test);
        REQUIRE_MESSAGE(done.ok(), line << ": " << (done ? "" : done.error().message));
        return done.value();
    }

    void squares()
    {
        run("KATMAN ad=ALANLAR");
        run("ALAN 0,0 10,0 10,10 0,10");
        run("ALAN 5,5 15,5 15,15 5,15");
    }
};

std::string commanded(const Journal& journal)
{
    std::string result;
    for (const auto& entry : journal.entries())
        result += entry.command_id + " " + entry.args.to_json().dump() + "\n";
    return result;
}

core::Mm2 stored_area(const core::Document& doc)
{
    core::Mm2 total = 0;
    for (core::EntityId slot = 0; slot < doc.entities().size(); ++slot) {
        if (!doc.alive(slot)) continue;
        if (auto face = area_face(doc, slot)) total += face_area(face->face);
    }
    return total;
}

void unchanged(Rig& r, const std::string& line)
{
    const auto hash    = r.doc.content_hash();
    const auto undo    = r.undo.undo_depth();
    const auto journal = r.journal.size();
    auto refused       = r.bus.execute_line(line, Origin::Test);
    CHECK_FALSE(refused.ok());
    CHECK_EQ(r.doc.content_hash(), hash);
    CHECK_EQ(r.undo.undo_depth(), undo);
    CHECK_EQ(r.journal.size(), journal);
}
} // namespace

TEST_CASE("ALAN BOOLEAN: dört işlem gerçek alanı oluşturur, tek adımda geri alınır")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");

    struct Case
    {
        const char* name;
        core::Mm2 area;
        std::size_t parts;
    };

    for (const auto& c : std::array<Case, 4>{{{"BİRLEŞİM", 175'000'000, 1},
                                              {"KESİŞİM", 25'000'000, 1},
                                              {"FARK", 75'000'000, 1},
                                              {"SİMETRİKFARK", 150'000'000, 2}}}) {
        CAPTURE(c.name);
        Rig r;
        r.squares();
        const auto before = r.doc.content_hash();
        const auto undo   = r.undo.undo_depth();
        const auto done   = r.run(std::string(c.name) + " nesneler=1 nesneler=2");
        REQUIRE(done.report.find("alan_mm2"));
        CHECK_EQ(done.report.find("alan_mm2")->as_int(), c.area);
        CHECK_EQ(done.report.find("parca")->as_int(), static_cast<std::int64_t>(c.parts));
        CHECK(done.report.find("geometri_cekirdegi")->as_string().starts_with("OpenCASCADE"));
        CHECK_EQ(stored_area(r.doc), c.area);
        CHECK_EQ(r.doc.live_entity_count(), c.parts);
        CHECK_EQ(r.undo.undo_depth(), undo + 1);
        const auto after = r.doc.content_hash();
        r.run("GERİAL");
        CHECK_EQ(r.doc.content_hash(), before);
        r.run("YİNELE");
        CHECK_EQ(r.doc.content_hash(), after);
        Rig replay;
        for (const auto& entry : r.journal.entries())
            REQUIRE(
                replay.bus.dispatch(Invocation{entry.command_id, entry.args, Origin::Batch}).ok());
        CHECK_EQ(replay.doc.content_hash(), after);
    }
}

TEST_CASE("ALAN BOOLEAN: kaynakları koruma, ilk katman ve özniteliklerin anlamı")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    for (const char* op : {"BİRLEŞİM", "FARK"}) {
        CAPTURE(op);
        Rig r;
        r.squares();
        r.run("SÜTUN kimlik=kod tur=metin");
        r.run("SÜTUN kimlik=ortak tur=metin");
        r.run("ÖZNİTELİK ad=kod nesne=1 deger=A");
        r.run("ÖZNİTELİK ad=kod nesne=2 deger=B");
        r.run("ÖZNİTELİK ad=ortak nesne=1 deger=aynı");
        r.run("ÖZNİTELİK ad=ortak nesne=2 deger=aynı");
        r.run("RENK nesneler=1 renk=#cc6633");
        r.run("KATMAN ad=DİĞER");
        r.run("KATMANAT nesneler=2 katman=DİĞER");
        const auto first  = r.doc.slot_of(static_cast<core::EntityKey>(1));
        const auto second = r.doc.slot_of(static_cast<core::EntityKey>(2));
        const auto style  = r.doc.entities().style[first];
        const auto done   = r.run(std::string(op) + " nesneler=1 nesneler=2 kaynaklari_koru=evet");
        CHECK_EQ(r.doc.live_entity_count(), 3u);
        CHECK(r.doc.alive(first));
        CHECK(r.doc.alive(second));
        CHECK(done.report.find("kaynaklar_korundu")->as_bool());
        const auto made = r.doc.slot_of(static_cast<core::EntityKey>(3));
        CHECK_EQ(r.doc.entities().layer[made], r.doc.entities().layer[first]);
        const auto kod    = r.doc.attribute(r.doc.attributes().find("kod"), made);
        const auto common = r.doc.attribute(r.doc.attributes().find("ortak"), made);
        REQUIRE(kod);
        REQUIRE(common);
        CHECK_EQ(common.value().text, "aynı");
        if (std::string(op) == "FARK") {
            CHECK_EQ(kod.value().text, "A");
            CHECK_EQ(r.doc.entities().style[made], style);
            CHECK(done.report.find("ayrisan_sutunlar")->as_array().empty());
        } else {
            CHECK_FALSE(kod.value().present);
            CHECK_EQ(r.doc.entities().style[made], core::kByLayerStyle);
            CHECK_EQ(done.report.find("ayrisan_sutunlar")->as_array().size(), 1u);
        }
    }
}

TEST_CASE("ALAN BOOLEAN: boş sonuç kaynak silmez, düz delik gerçekten deliktir")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    for (const char* op : {"KESİŞİM", "SİMETRİKFARK", "FARK"}) {
        Rig r;
        r.run("ALAN 0,0 10,0 10,10 0,10");
        if (std::string(op) == "KESİŞİM")
            r.run("ALAN 20,0 30,0 30,10 20,10");
        else
            r.run("ALAN 0,0 10,0 10,10 0,10");
        const auto hash = r.doc.content_hash();
        const auto undo = r.undo.undo_depth();
        const auto done = r.run(std::string(op) + " nesneler=1 nesneler=2");
        CHECK_EQ(done.report.find("parca")->as_int(), 0);
        CHECK(done.report.find("kaynaklar_korundu")->as_bool());
        CHECK_EQ(r.doc.content_hash(), hash);
        CHECK_EQ(r.undo.undo_depth(), undo);
    }
    Rig hole;
    hole.run("ALAN 0,0 10,0 10,10 0,10");
    hole.run("ALAN 2,2 8,2 8,8 2,8");
    const auto done = hole.run("FARK nesneler=1 nesneler=2");
    CHECK_EQ(done.report.find("alan_mm2")->as_int(), 64'000'000);
    auto face = area_face(hole.doc, hole.doc.slot_of(static_cast<core::EntityKey>(3)));
    REQUIRE(face);
    CHECK_EQ(face->face.holes.size(), 1u);
    CHECK_EQ(face_area(face->face), 64'000'000);
}

TEST_CASE("ALAN BOOLEAN: daire sınırı yay kalır, eğrili delik sessizce düzleşmez")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.run("DAİRE merkez=0,0 cevre=5,0");
    r.run("ALAN 0,-10 10,-10 10,10 0,10");
    const auto done = r.run("KESİŞİM nesneler=1 nesneler=2");
    auto face       = area_face(r.doc, r.doc.slot_of(static_cast<core::EntityKey>(3)));
    REQUIRE(face);
    CHECK_EQ(r.doc.entities().kind[r.doc.slot_of(static_cast<core::EntityKey>(3))],
             core::kArcPolylineKind);
    int arcs = 0;
    for (const auto& piece : face->face.outer.pieces)
        if (piece.kind == core::PathPiece::Kind::Arc) {
            ++arcs;
            CHECK_EQ(piece.radius, 5'000);
            CHECK_EQ(piece.centre, (core::Point2{0, 0}));
        }
    CHECK(arcs > 0);
    CHECK(std::abs(done.report.find("alan_mm2")->as_int() - 39'269'908) <= 1);
    Rig cut;
    cut.run("ALAN -10,-10 10,-10 10,10 -10,10");
    cut.run("DAİRE merkez=0,0 cevre=5,0");
    unchanged(cut, "FARK nesneler=1 nesneler=2");
}

TEST_CASE("ALAN BOOLEAN: yanlış ve açık girdiler tam geri alınır")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig r;
    r.squares();
    r.run("ÇİZGİ 0,0 10,0");
    for (const char* line :
         {"BİRLEŞİM nesneler=1", "BİRLEŞİM nesneler=1 nesneler=1",
          "BİRLEŞİM nesneler=1 nesneler=999", "KESİŞİM nesneler=1 nesneler=2 nesneler=3",
          "FARK nesneler=1 nesneler=3", "FARK nesneler=1 nesneler=2 tutulan=3",
          "SİMETRİKFARK nesneler=1 nesneler=2 kaynaklari_koru=belirsiz"})
        unchanged(r, line);
}

TEST_CASE("ALAN BOOLEAN: çok parçalı kaynağın her dış halkası ve kendi deliği korunur")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    if (!io::vector_backend_available())
        PENDING("PIRICAD_WITH_GDAL=OFF; çok parçalı kaynak okunamıyor.");

    struct Case
    {
        const char* name;
        core::Mm2 area;
        std::size_t parts;
    };

    for (const auto& c : std::array<Case, 4>{{{"BİRLEŞİM", 246'000'000, 2},
                                              {"KESİŞİM", 50'000'000, 1},
                                              {"FARK", 146'000'000, 2},
                                              {"SİMETRİKFARK", 196'000'000, 3}}}) {
        CAPTURE(c.name);
        Rig r;
        io::FileService files(r.bus);
        r.run("İÇEAKTAR dosya=\"" PIRICAD_GOLDEN_DIR "/cizim/alan-boolean-multipart.gpkg\"");
        REQUIRE_EQ(r.doc.live_entity_count(), 1u);
        auto faces = area_faces(r.doc, r.doc.slot_of(static_cast<core::EntityKey>(1)));
        REQUIRE(faces);
        REQUIRE_EQ(faces->size(), 2u);
        CHECK_EQ((*faces)[0].face.holes.size(), 1u);
        CHECK((*faces)[1].face.holes.empty());
        CHECK_FALSE(area_face(r.doc, r.doc.slot_of(static_cast<core::EntityKey>(1))));
        r.run("ALAN 485325,4310200 485335,4310200 485335,4310210 485325,4310210");
        const auto before = r.doc.content_hash();
        const auto done   = r.run(std::string(c.name) + " nesneler=1 nesneler=2");
        CHECK_EQ(done.report.find("alan_mm2")->as_int(), c.area);
        CHECK_EQ(done.report.find("parca")->as_int(), static_cast<std::int64_t>(c.parts));
        CHECK_EQ(stored_area(r.doc), c.area);
        r.run("GERİAL");
        CHECK_EQ(r.doc.content_hash(), before);
    }
    Rig ellipse;
    ellipse.run("ALAN -10,-10 10,-10 10,10 -10,10");
    ellipse.run("ELİPS merkez=0,0 birinci=5,0 ikinci=0,3");
    unchanged(ellipse, "BİRLEŞİM nesneler=1 nesneler=2");
}

TEST_CASE("ALAN BOOLEAN: tek seçili alan diğer girdiyi ister, farkta tutulacak alan sorulur")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig single;
    single.squares();
    single.run("SEÇ nesneler=1");
    auto started = single.bus.begin_interactive("KESİŞİM");
    REQUIRE(started);
    REQUIRE(started.value()->waiting());
    CHECK_EQ(started.value()->prompt().kind, ParamKind::Selection);
    REQUIRE(started.value()->supply(Value::ids({2})).ok());
    REQUIRE(single.bus.finish(*started.value()).ok());
    CHECK_EQ(stored_area(single.doc), 25'000'000);

    Rig multi;
    multi.squares();
    multi.run("SEÇ nesneler=2 nesneler=1");
    started = multi.bus.begin_interactive("FARK");
    REQUIRE(started);
    REQUIRE(started.value()->waiting());
    CHECK_EQ(started.value()->prompt().param, "tutulan");
    REQUIRE(started.value()->supply(Value::ids({2})).ok());
    const auto done = multi.bus.finish(*started.value());
    REQUIRE(done);
    CHECK_EQ(done.value().report.find("tutulan")->as_int(), 2);
    auto result = area_face(multi.doc, multi.doc.slot_of(static_cast<core::EntityKey>(3)));
    REQUIRE(result);
    CHECK_EQ(result->face.outer.pieces.front().from, (core::Point2{10'000, 5'000}));
    CHECK_EQ(stored_area(multi.doc), 75'000'000);
}

TEST_CASE("ALAN BOOLEAN: arayüz, komut satırı, JSON ve günlük aynı sonucu verir")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    for (const char* name : {"BİRLEŞİM", "KESİŞİM", "FARK", "SİMETRİKFARK"}) {
        Rig gui, cli, script;
        gui.squares();
        cli.squares();
        script.squares();
        auto started = gui.bus.begin_interactive(name);
        REQUIRE(started);
        REQUIRE(started.value()->waiting());
        REQUIRE(started.value()->supply(Value::ids({1, 2})).ok());
        if (std::string(name) == "FARK") REQUIRE(started.value()->supply(Value::ids({1})).ok());
        auto mouse = gui.bus.finish(*started.value());
        REQUIRE(mouse);
        const auto line = cli.run(std::string(name) + " nesneler=1 nesneler=2" +
                                  (std::string(name) == "FARK" ? " tutulan=1" : ""));
        script::JsonRunner runner(script.bus, script::Sandbox::Project);
        const auto id = cli.reg.resolve(name)->id;
        const auto json =
            runner.run_text("{\"komutlar\":[{\"cmd\":\"" + id + "\",\"args\":{\"nesneler\":[1,2]" +
                            (std::string(name) == "FARK" ? ",\"tutulan\":[1]" : "") + "}}]}");
        REQUIRE(json);
        CHECK_EQ(gui.doc.content_hash(), cli.doc.content_hash());
        CHECK_EQ(cli.doc.content_hash(), script.doc.content_hash());
        CHECK_EQ(commanded(gui.journal), commanded(cli.journal));
        CHECK_EQ(commanded(cli.journal), commanded(script.journal));
        CHECK_EQ(mouse.value().report.dump(), line.report.dump());
    }
}

TEST_CASE("ALAN BOOLEAN: iş parçacığı aynı sonucu verir, durdurma ve seçim iptali iz bırakmaz")
{
    if (!core::kernel_available()) PENDING("PIRICAD_WITH_OCCT=OFF; geometri çekirdeği yok.");
    Rig direct, worker;
    direct.squares();
    worker.squares();
    const auto done        = direct.run("SİMETRİKFARK nesneler=1 nesneler=2");
    worker.bus.on_job_host = [](Session&) {};
    auto started           = worker.bus.begin_interactive("SİMETRİKFARK nesneler=1 nesneler=2");
    REQUIRE(started);
    REQUIRE(started.value()->working());
    Job* job = started.value()->job();
    std::thread thread([job] { job->work(job->control()); });
    thread.join();
    started.value()->resume_job();
    auto hosted = worker.bus.finish(*started.value());
    REQUIRE(hosted);
    CHECK_EQ(worker.doc.content_hash(), direct.doc.content_hash());
    CHECK_EQ(commanded(worker.journal), commanded(direct.journal));
    CHECK_EQ(hosted.value().report.dump(), done.report.dump());
    Rig stop;
    stop.squares();
    const auto hash      = stop.doc.content_hash();
    const auto undo      = stop.undo.undo_depth();
    const auto journal   = stop.journal.size();
    stop.bus.on_job_host = [](Session&) {};
    started              = stop.bus.begin_interactive("BİRLEŞİM nesneler=1 nesneler=2");
    REQUIRE(started);
    REQUIRE(started.value()->working());
    job = started.value()->job();
    job->stop.request_stop();
    job->work(job->control());
    started.value()->resume_job();
    auto stopped = stop.bus.finish(*started.value());
    REQUIRE(stopped);
    CHECK(stopped.value().report.is_null());
    CHECK_EQ(stop.doc.content_hash(), hash);
    CHECK_EQ(stop.undo.undo_depth(), undo);
    CHECK_EQ(stop.journal.size(), journal);
    stop.bus.on_job_host = nullptr;
    started              = stop.bus.begin_interactive("FARK");
    REQUIRE(started);
    REQUIRE(started.value()->waiting());
    started.value()->cancel();
    REQUIRE(stop.bus.finish(*started.value()).ok());
    CHECK_EQ(stop.doc.content_hash(), hash);
    CHECK_EQ(stop.undo.undo_depth(), undo);
    CHECK_EQ(stop.journal.size(), journal);
}
