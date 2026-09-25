// SPDX-License-Identifier: GPL-3.0-or-later
//
// Long work (TODOS F-05, fourth stage): a command whose work is long hands it to
// a job — off the UI thread in the application, counted on the status strip and
// stopped by Durdur — and the drawing is the job's until it is back.
//
// The cases hold that lifecycle to five things: the job counts across all its
// passes; while it runs nothing can change the drawing underneath it, whoever
// sends the change; a job run on another thread answers exactly what the same
// command answers in place (Article 1.2 — the GUI hosts it, a script does not);
// a job stopped half way leaves no trace and claims no result; and a long check
// with many findings says how many and which kinds before it lists any.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/job.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"
#include "kentos_cad/domain/surface/contour.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/io/staging.hpp"
#include "kentos_cad/io/vector.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <span>
#include <string>
#include <thread>
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
    io::FileService files{bus};
    std::string said;
    std::vector<MeasureMark> marks;

    Rig()
    {
        register_builtin_commands(reg);
        domain::cadastre::register_cadastre_commands(reg);
        domain::surface::register_surface_commands(reg);
        bus.on_echo         = [this](std::string_view s) { said.append(s).append("\n"); };
        bus.on_measure_mark = [this](const MeasureMark& m) { marks.push_back(m); };
    }

    void run(const std::string& line)
    {
        auto r = bus.execute_line(line, Origin::Test);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

/// Everything a stopped job could leave behind.
struct Mark
{
    std::uint64_t hash{0};
    std::size_t undo{0};
    std::size_t journal{0};
    std::size_t live{0};
    std::size_t layers{0};

    bool operator==(const Mark&) const = default;
};

std::ostream& operator<<(std::ostream& out, const Mark& m)
{
    return out << "{özet " << m.hash << ", geri " << m.undo << ", günlük " << m.journal
               << ", nesne " << m.live << ", katman " << m.layers << "}";
}

Mark mark(const Rig& r)
{
    return Mark{r.doc.content_hash(), r.undo.undo_depth(), r.journal.size(),
                r.doc.live_entity_count(), r.doc.layers().size()};
}

/// `n` parcels on a 20 m grid, 100 to a row; with `overlap`, each odd one is
/// moved 5 m west onto its neighbour, so every pair claims 5 m × 20 m of
/// ground. A fixture, drawn through one transaction and left off the undo
/// stack: the subject is what TOPOLOJİ does with it.
void draw_grid(Rig& r, int n, bool overlap = false)
{
    Transaction tx(r.doc, "kurulum");
    const LayerId layer = tx.ensure_layer("PARSEL");
    for (int i = 0; i < n; ++i) {
        const core::Mm x =
            static_cast<core::Mm>(i % 100) * 20'000 + (overlap && i % 2 == 1 ? -5'000 : 0);
        const core::Mm y = static_cast<core::Mm>(i / 100) * 20'000;
        const std::array<core::Point2, 4> ring{core::Point2{x, y}, core::Point2{x + 20'000, y},
                                               core::Point2{x + 20'000, y + 20'000},
                                               core::Point2{x, y + 20'000}};
        const core::RingGeometry::RingInput input{ring, core::RingRole::Exterior, 0};
        REQUIRE(tx.add_area(layer, std::span<const core::RingGeometry::RingInput>(&input, 1)).ok());
    }
    (void)tx.release();
}

/// A `side` × `side` levelling survey, 5 m apart, on a hill: heights in `kot`.
void level_grid(Rig& r, int side)
{
    Transaction tx(r.doc, "kurulum");
    const LayerId layer = tx.ensure_layer("NOKTA");
    core::AttrSpec spec;
    spec.id      = "kot";
    spec.name_tr = "kot";
    spec.type    = core::AttrType::Length;
    auto kot     = tx.declare_attribute(std::move(spec));
    REQUIRE(kot.ok());
    for (int i = 0; i < side; ++i) {
        for (int j = 0; j < side; ++j) {
            const core::Point2 at{static_cast<core::Mm>(i) * 5000, static_cast<core::Mm>(j) * 5000};
            auto made = tx.add_point(layer, at);
            REQUIRE(made.ok());
            const auto di       = static_cast<core::Mm>(i - side / 2);
            const auto dj       = static_cast<core::Mm>(j - side / 2);
            const core::Mm high = 900'000 - (di * di + dj * dj) * 40 + ((i * 31 + j * 17) % 13) * 7;
            REQUIRE(tx.set_attribute(kot.value(), made.value(), core::attr_mm(high)).ok());
        }
    }
    (void)tx.release();
}

/// Starts `line` as the application starts it — a session it keeps — with a host
/// that parks the job instead of running it, and returns the parked session.
std::unique_ptr<Session> park(Rig& r, const std::string& line)
{
    Session* parked   = nullptr;
    r.bus.on_job_host = [&parked](Session& s) { parked = &s; };
    auto started      = r.bus.begin_interactive(line, Origin::Gui);
    if (!started) FAIL_WITH(line, started.error().message);
    REQUIRE(started.value()->working());
    REQUIRE(parked == started.value().get());
    r.bus.on_job_host = nullptr;
    return std::move(started.value());
}

/// Runs the parked job on ANOTHER thread, as the application's host does, and
/// finishes the session on this one.
core::Result<DispatchResult> host(Rig& r, Session& session)
{
    Job* job = session.job();
    std::thread worker([job] { job->work(job->control()); });
    worker.join();
    session.resume_job();
    return r.bus.finish(session);
}

/// Runs the parked job on this thread with a watcher that presses Durdur as
/// soon as the job has counted past zero — half way, not before it began.
core::Result<DispatchResult> stop_half_way(Rig& r, Session& session, std::uint32_t* at_stop)
{
    Job& job = *session.job();
    std::atomic<bool> done{false};
    std::thread watcher([&] {
        while (!done.load() && job.permille.load() == 0) {
        }
        if (at_stop != nullptr) *at_stop = job.permille.load();
        job.stop.request_stop();
    });
    job.work(job.control());
    done.store(true);
    watcher.join();
    session.resume_job();
    return r.bus.finish(session);
}

} // namespace

// ============================================================ the control ===

TEST_CASE("UZUN İŞ: iş denetimi evreler boyunca sayar ve bitince bine varır")
{
    std::atomic<std::uint32_t> permille{0};
    const JobControl control{std::stop_token{}, &permille};
    control.at(0, 10, 100, 800);
    CHECK_EQ(permille.load(), 100u);
    control.at(5, 10, 100, 800);
    CHECK_EQ(permille.load(), 450u);
    control.at(10, 10, 100, 800);
    CHECK_EQ(permille.load(), 800u);
    control.at(3, 0);
    CHECK_EQ(permille.load(), 1000u);
    control.at(9, 5);
    CHECK_EQ(permille.load(), 1000u);

    // NO FIGURE, NO CRASH: a job that is not counted is still handed a control.
    const JobControl none{};
    none.at(1, 2);
    CHECK_FALSE(none.cancelled());

    Job job;
    const JobControl mine = job.control();
    mine.at(1, 4);
    CHECK_EQ(job.permille.load(), 250u);
    job.stop.request_stop();
    CHECK(mine.cancelled());
}

// ========================================================= the one writer ===

TEST_CASE("UZUN İŞ: iş sürerken çizimi değiştiren her yol reddedilir; okuma açık kalır")
{
    Rig r;
    draw_grid(r, 2000);
    auto session = park(r, "TOPOLOJİ");
    REQUIRE(r.bus.running_job() != nullptr);
    CHECK_EQ(r.bus.running_job()->label, "Topoloji denetimi");
    const Mark before = mark(r);

    const auto busy = [](const core::Error& e) {
        CHECK_EQ(e.code, core::ErrorCode::Busy);
        CHECK(e.message.find("Bir iş sürüyor (Topoloji denetimi)") != std::string::npos);
        CHECK(e.message.find("Durdur") != std::string::npos);
    };

    // EVERY CLIENT, every road that writes.
    for (const Origin origin : {Origin::CommandLine, Origin::Script, Origin::Ai, Origin::Gui}) {
        auto drawn = r.bus.execute_line("ÇİZGİ 0,0 10,10", origin);
        REQUIRE_FALSE(drawn.ok());
        busy(drawn.error());
    }
    for (const char* line : {"SİL nesneler=1", "GERİAL", "KATMAN ad=YENİ",
                             "TAŞI nesneler=1 "
                             "baslangic=0,0 "
                             "bitis=5,0"}) {
        auto refused = r.bus.execute_line(line, Origin::Script);
        REQUIRE_FALSE(refused.ok());
        busy(refused.error());
    }
    auto batch = r.bus.begin_batch("betik");
    REQUIRE_FALSE(batch.ok());
    busy(batch.error());
    const std::array<Invocation, 1> steps{Invocation{"core.erase",
                                                     [] {
                                                         Args a;
                                                         a.set("nesneler", Value::ids({1}));
                                                         return a;
                                                     }(),
                                                     Origin::Ai}};
    auto seen = r.bus.preview(steps);
    REQUIRE_FALSE(seen.ok());
    busy(seen.error());
    auto another = r.bus.begin_interactive("ÇİZGİ", Origin::Gui);
    REQUIRE_FALSE(another.ok());
    busy(another.error());

    // READING STAYS OPEN: a query answers from the drawing the job is reading.
    CHECK(r.bus.execute_line("NESNEBİLGİ nesneler=1", Origin::Script).ok());
    CHECK_EQ(mark(r), before);

    // AND THE DRAWING OPENS AGAIN when the job is back.
    auto done = host(r, *session);
    REQUIRE(done.ok());
    CHECK(r.bus.running_job() == nullptr);
    CHECK(r.bus.writable().ok());
    CHECK(r.bus.execute_line("ÇİZGİ 0,0 10,10", Origin::Script).ok());
}

TEST_CASE("UZUN İŞ: işini bitirmeden giden bir oturum çizimi kilitli bırakmaz")
{
    Rig r;
    draw_grid(r, 100);
    {
        auto session = park(r, "TOPOLOJİ");
        REQUIRE_FALSE(r.bus.writable().ok());
        session->cancel();
        session->job()->work(session->job()->control());
    }
    CHECK(r.bus.writable().ok());
    CHECK(r.bus.execute_line("ÇİZGİ 0,0 10,10", Origin::Script).ok());
}

// ================================================================ TOPOLOJİ ===

TEST_CASE("UZUN İŞ: TOPOLOJİ başka bir iş parçacığında da yerinde de aynı cevabı verir")
{
    // The GUI hosts the job; the command line and a script run it in place. The
    // answer, the structured report, the marks and the journal line are one.
    Rig direct;
    Rig hosted;
    draw_grid(direct, 400, /*overlap=*/true);
    draw_grid(hosted, 400, /*overlap=*/true);

    auto in_place = direct.bus.execute_line("TOPOLOJİ", Origin::Gui);
    REQUIRE(in_place.ok());
    auto session = park(hosted, "TOPOLOJİ");
    auto off_ui  = host(hosted, *session);
    REQUIRE(off_ui.ok());

    CHECK_EQ(hosted.said, direct.said);
    CHECK_EQ(session->report().dump(), in_place.value().report.dump());
    CHECK_EQ(hosted.marks.size(), direct.marks.size());
    CHECK_EQ(hosted.journal.canonical(), direct.journal.canonical());
    CHECK(direct.said.find("200 kusur — 200 örtüşme.") != std::string::npos);
}

TEST_CASE("UZUN İŞ: yarıda durdurulan TOPOLOJİ sonuç vermez ve günlüğe yazılmaz")
{
    Rig r;
    draw_grid(r, 20000);
    const Mark before = mark(r);
    auto session      = park(r, "TOPOLOJİ");
    r.said.clear();

    std::uint32_t at_stop = 0;
    auto done             = stop_half_way(r, *session, &at_stop);
    MESSAGE("durdurulduğunda ilerleme: ", at_stop, "‰");
    REQUIRE(done.ok());
    CHECK_FALSE(done.value().mutated);
    CHECK(r.said.find("Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.") !=
          std::string::npos);
    // HALF A CHECK IS NOT A SMALLER CHECK: nothing about findings is said.
    CHECK(r.said.find("kusur") == std::string::npos);
    CHECK(r.marks.empty());
    CHECK_EQ(mark(r), before);
    CHECK(r.bus.writable().ok());
}

TEST_CASE("UZUN İŞ: çok kusurlu TOPOLOJİ önce sayar, ilk yirmisini yazar, hepsini cevapta verir")
{
    Rig r;
    draw_grid(r, 60, /*overlap=*/true); // thirty overlapping pairs
    r.said.clear();
    auto done = r.bus.execute_line("TOPOLOJİ", Origin::Script);
    REQUIRE(done.ok());

    // THE COUNT BY KIND COMES FIRST.
    CHECK(
        r.said.starts_with("Topoloji denetimi (bütün çizim, 60 nesne): 30 kusur — 30 örtüşme.\n"));
    std::size_t listed = 0;
    for (std::size_t at = r.said.find("\n  Nesne "); at != std::string::npos;
         at             = r.said.find("\n  Nesne ", at + 1))
        ++listed;
    CHECK_EQ(listed, 20u);
    CHECK(r.said.find("  … ve 10 kusur daha; hepsi tuvalde işaretli.\n") != std::string::npos);

    // EVERY ONE IN THE STRUCTURED ANSWER, and every one marked.
    const core::Json& report = done.value().report;
    REQUIRE(report.find("kusur") != nullptr);
    CHECK_EQ(report.find("kusur")->as_int(), 30);
    CHECK_EQ(report.find("bakilan")->as_int(), 60);
    CHECK_EQ(report.find("turler")->find("ortusme")->as_int(), 30);
    const core::Json* all = report.find("kusurlar");
    REQUIRE(all != nullptr);
    REQUIRE_EQ(all->as_array().size(), 30u);
    const core::Json& first = all->as_array().front();
    CHECK_EQ(first.find("tur")->as_string(), "ortusme");
    CHECK_EQ(first.find("alan_mm2")->as_int(), 5'000 * 20'000);
    CHECK(first.find("diger") != nullptr);
    CHECK(first.find("aciklama")->as_string().find("örtüşüyor") != std::string::npos);

    // THE GROUND BOTH CLAIM, outlined: the overlap is a ring on the canvas.
    REQUIRE_EQ(r.marks.size(), 30u);
    CHECK_EQ(r.marks.front().shape, MeasureMark::Shape::Ring);
    CHECK_EQ(r.marks.front().points.size(), 4u);
    REQUIRE_FALSE(r.marks.front().labels.empty());
    CHECK_EQ(r.marks.front().labels.front(), "örtüşme 100,00 m²");
}

TEST_CASE("UZUN İŞ: TOPOLOJİ ilerlemesini dört geçiş boyunca, geri gitmeden sayar")
{
    // The §10.1 figure — 100 000 parcels in 2 s — is the bench's
    // (`domain.topoloji_100k_parsel`), where a timing belongs; a unit run under a
    // sanitizer is no measure of one. What is checked here is what the status
    // strip shows: a figure that moves, never goes back and ends at a thousand.
    Rig r;
    draw_grid(r, 20000);
    auto session = park(r, "TOPOLOJİ");
    Job& job     = *session->job();
    std::vector<std::uint32_t> seen;
    std::atomic<bool> done{false};
    std::thread watcher([&] {
        while (!done.load()) {
            const std::uint32_t now = job.permille.load();
            if (seen.empty() || seen.back() != now) seen.push_back(now);
        }
    });
    const auto t0 = std::chrono::steady_clock::now();
    job.work(job.control());
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    done.store(true);
    watcher.join();
    session->resume_job();
    REQUIRE(r.bus.finish(*session).ok());
    MESSAGE("20 000 parsel: ", ms, " ms, ", seen.size(), " ayrı ilerleme değeri");
    CHECK_EQ(job.permille.load(), 1000u);
    CHECK(seen.size() > 3);
    CHECK(std::is_sorted(seen.begin(), seen.end()));
}

// ===================================================== EŞYÜKSELTİ / HACİM ===

TEST_CASE("UZUN İŞ: EŞYÜKSELTİ işte de yerinde de aynı çizimi ve aynı günlüğü bırakır")
{
    if (!domain::surface::available()) PENDING("KENTOS_WITH_CDT=OFF; EŞYÜKSELTİ sınanamıyor.");
    Rig direct;
    Rig hosted;
    level_grid(direct, 40);
    level_grid(hosted, 40);

    REQUIRE(direct.bus.execute_line("EŞYÜKSELTİ aralik=1000", Origin::Gui).ok());
    auto session = park(hosted, "EŞYÜKSELTİ aralik=1000");
    REQUIRE(host(hosted, *session).ok());

    CHECK_EQ(hosted.doc.content_hash(), direct.doc.content_hash());
    CHECK_EQ(hosted.journal.canonical(), direct.journal.canonical());
    CHECK_EQ(hosted.said, direct.said);
    CHECK_EQ(hosted.undo.undo_depth(), 1u);
}

TEST_CASE("UZUN İŞ: yarıda durdurulan EŞYÜKSELTİ ve HACİM iz bırakmaz")
{
    if (!domain::surface::available()) PENDING("KENTOS_WITH_CDT=OFF; EŞYÜKSELTİ sınanamıyor.");
    Rig r;
    level_grid(r, 150);
    const Mark before = mark(r);

    auto contours = park(r, "EŞYÜKSELTİ aralik=200");
    auto stopped  = stop_half_way(r, *contours, nullptr);
    REQUIRE(stopped.ok());
    CHECK_FALSE(stopped.value().mutated);
    CHECK(r.said.find("Eş yükselti çizimi durduruldu; çizim değişmedi.") != std::string::npos);
    CHECK(r.doc.find_layer("ESYUKSELTI") == core::kNoLayer);
    CHECK_EQ(mark(r), before);

    auto volume = park(r, "HACİM kot=880000");
    auto halted = stop_half_way(r, *volume, nullptr);
    REQUIRE(halted.ok());
    CHECK(r.said.find("Hacim hesabı durduruldu; sonuç verilmedi.") != std::string::npos);
    CHECK(r.said.find("kazı") == std::string::npos);
    CHECK_EQ(mark(r), before);
}

// ================================================================ DIŞAAKTAR ===

TEST_CASE("UZUN İŞ: DIŞAAKTAR işte yazar ve sayar; yarıda durdurulursa eski dosya kalır")
{
    if (!io::vector_backend_available()) PENDING("KENTOS_WITH_GDAL=OFF.");
    const auto dir = std::filesystem::temp_directory_path() / "kentos-uzun-is-disaaktar";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string target = (dir / "ada.gpkg").string();

    Rig r;
    r.run("AYAR koordinat_sistemi EPSG:5254");
    draw_grid(r, 20000);

    // WRITTEN ON ANOTHER THREAD, counted to the end.
    {
        auto session = park(r, "DIŞAAKTAR dosya=\"" + target + "\"");
        CHECK(r.bus.running_job()->label == "Dışa aktarılıyor: ada.gpkg");
        Job& job  = *session->job();
        auto done = host(r, *session);
        REQUIRE(done.ok());
        CHECK_EQ(job.permille.load(), 1000u);
        CHECK(r.said.find("Dışa aktarıldı: " + target) != std::string::npos);
    }
    std::ifstream in(target, std::ios::binary);
    const std::string written((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    in.close();
    REQUIRE_FALSE(written.empty());

    // STOPPED HALF WAY: the file that was there stays, byte for byte, and the
    // command says it stopped — not "Hata".
    r.run("ÇİZGİ 0,0 10,10");
    const Mark before = mark(r);
    r.said.clear();
    auto session = park(r, "DIŞAAKTAR dosya=\"" + target + "\"");
    auto stopped = stop_half_way(r, *session, nullptr);
    REQUIRE(stopped.ok());
    CHECK(r.said.find("Dışa aktarma durduruldu") != std::string::npos);
    CHECK(r.said.find("Hata") == std::string::npos);
    CHECK_EQ(mark(r), before);
    std::ifstream again(target, std::ios::binary);
    const std::string kept((std::istreambuf_iterator<char>(again)),
                           std::istreambuf_iterator<char>());
    CHECK(kept == written);
    std::size_t staged = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        if (io::is_staging_name(entry.path().filename().string())) ++staged;
    CHECK_EQ(staged, 0u);
    std::filesystem::remove_all(dir);
}
