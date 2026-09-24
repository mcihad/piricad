// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — tests: YAZDIR and YAZDIRMAPROFİLİ (commands/print.cpp).
//
// What is proven here is the COMMAND, not the sheet: the engine seam, the
// arguments the bus refuses, what the journal records — and, the point of it,
// what the journal does NOT record. A PDF password in a journal line is a
// password in every copy of that file, and the journal is a plain JSONL that
// gets attached to bug reports.
//
// The sheet itself is proved twice elsewhere: `tests/unit/test_io.cpp` over the
// profile store and the encryption, and the `print-pdf` ctest over a real PDF
// written by the real binary — Qt writes the file and this suite links no Qt.
#include "kentos_test.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/io/print_profiles.hpp"
#include "kentos_cad/script/json_runner.hpp"

#include <string>
#include <vector>

using namespace kentos;
using namespace kentos::command;

namespace {

/// A bus with a print engine that records instead of printing. The seam is what
/// the application installs (`app::PrintService`); a fake one here proves the
/// command layer without Qt, a printer or a file.
struct Rig
{
    core::Document doc;
    Registry reg;
    Journal journal;
    UndoStack undo;
    Bus bus{doc, reg, journal, undo};
    std::string said;

    /// Every request the commands sent, in order.
    std::vector<PrintRequest> asked;

    /// The profile store the fake engine keeps, so `listele` and `resolve`
    /// answer the way the real service answers.
    io::PrintProfiles profiles{io::PrintProfiles::builtin()};

    explicit Rig(bool with_engine = true)
    {
        register_builtin_commands(reg);
        bus.on_echo = [this](std::string_view s) { said.append(s).append("\n"); };
        if (!with_engine) return;

        bus.on_print_request =
            [this](const PrintRequest& request) -> Task<core::Result<std::string>> {
            return this->handle(request);
        };
    }

    Task<core::Result<std::string>> handle(PrintRequest request)
    {
        asked.push_back(request);
        switch (request.verb) {
        case PrintRequest::Verb::Profiles: co_return profiles.listing();
        case PrintRequest::Verb::SetProfile: {
            io::PrintProfile p;
            p.name = request.profile;
            if (!request.paper.empty()) p.paper = request.paper;
            if (request.width_mm > 0) p.width_mm = request.width_mm;
            if (request.height_mm > 0) p.height_mm = request.height_mm;
            if (request.landscape >= 0) p.landscape = request.landscape == 1;
            if (request.dpi > 0) p.dpi = request.dpi;
            if (request.margin_mm >= 0) p.margin_mm = request.margin_mm;
            if (auto st = profiles.upsert(std::move(p)); !st) co_return st.error();
            co_return "Profil kaydedildi: " + request.profile;
        }
        case PrintRequest::Verb::RemoveProfile:
            if (auto st = profiles.remove(request.profile); !st) co_return st.error();
            co_return "Profil silindi: " + request.profile;
        case PrintRequest::Verb::SetDefault:
            if (auto st = profiles.set_default(request.profile); !st) co_return st.error();
            co_return "Varsayılan profil: " + profiles.default_name();
        case PrintRequest::Verb::ToPdf: {
            auto resolved = profiles.resolve(request);
            if (!resolved) co_return resolved.error();
            co_return "PDF yazıldı: " + request.path + " — " +
                io::describe_print_profile(resolved.value());
        }
        case PrintRequest::Verb::ToPrinter: {
            auto resolved = profiles.resolve(request);
            if (!resolved) co_return resolved.error();
            co_return "Yazıcıya gönderildi: " +
                (request.printer.empty() ? std::string("(varsayılan)") : request.printer);
        }
        }
        co_return core::err(core::ErrorCode::Internal, "bilinmeyen istek");
    }

    core::Result<DispatchResult> run(const char* line)
    {
        return bus.execute_line(line, Origin::Test);
    }

    void must(const char* line)
    {
        auto r = run(line);
        if (!r) FAIL_WITH(line, r.error().message);
    }
};

std::string journal_of(const Journal& j)
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

TEST_CASE("YAZDIR: iki komut kayıtta; adlar, kategori ve parametre sırası")
{
    Rig f;

    const CommandSpec* print = f.reg.resolve("YAZDIR");
    REQUIRE(print != nullptr);
    CHECK_EQ(print->id, std::string("core.print"));
    CHECK_EQ(print->category, Category::File);
    CHECK(f.reg.resolve("PRINT") == print);
    CHECK(f.reg.resolve("PLOT") == print);
    CHECK(f.reg.resolve("YZDR") == print);
    // Writes no entity: nothing to undo, and the journal carries it as a record
    // of what was done rather than as a document mutation.
    CHECK_EQ(print->undo, UndoPolicy::None);
    CHECK(has_flag(print->flags, Flags::ReadOnly));
    CHECK_EQ(print->params.front().name, std::string("pencere"));
    CHECK_EQ(print->params.front().kind, ParamKind::PointList);

    const CommandSpec* profile = f.reg.resolve("YAZDIRMAPROFİLİ");
    REQUIRE(profile != nullptr);
    CHECK_EQ(profile->id, std::string("core.print_profile"));
    CHECK(f.reg.resolve("YAZDIRMAPROFILI") == profile);
    CHECK(f.reg.resolve("PRINTPROFILE") == profile);
    CHECK_EQ(profile->params.front().name, std::string("islem"));
}

TEST_CASE("YAZDIRMAPROFİLİ: listeler, ekler, varsayılan yapar, siler; geçersizi adıyla reddeder")
{
    Rig f;

    f.must("YAZDIRMAPROFİLİ islem=listele");
    CHECK(f.said.find("A4 Dikey") != std::string::npos);
    CHECK(f.said.find("* ") != std::string::npos); // the default is marked
    REQUIRE_EQ(f.asked.size(), std::size_t{1});
    CHECK(f.asked.front().verb == PrintRequest::Verb::Profiles);

    f.must("YAZDIRMAPROFİLİ islem=ekle ad=\"A3 Pafta\" kagit=A3 yon=yatay dpi=150 kenar=5");
    REQUIRE_EQ(f.asked.size(), std::size_t{2});
    const PrintRequest& added = f.asked.back();
    CHECK(added.verb == PrintRequest::Verb::SetProfile);
    CHECK_EQ(added.profile, std::string("A3 Pafta"));
    CHECK_EQ(added.paper, std::string("A3"));
    CHECK_EQ(added.landscape, 1);
    CHECK_EQ(added.dpi, 150);
    CHECK_EQ(added.margin_mm, 5);
    REQUIRE(f.profiles.find("A3 Pafta") != nullptr);
    CHECK_EQ(f.profiles.find("A3 Pafta")->sheet_width_mm(), 420);

    f.must("YAZDIRMAPROFİLİ islem=varsayilan ad=\"A3 Pafta\"");
    CHECK_EQ(f.profiles.default_name(), std::string("A3 Pafta"));
    f.must("YAZDIRMAPROFİLİ islem=sil ad=\"A3 Pafta\"");
    CHECK(f.profiles.find("A3 Pafta") == nullptr);

    // A word that is not an operation, and an operation with no name.
    auto unknown = f.run("YAZDIRMAPROFİLİ islem=parlat");
    CHECK_FALSE(unknown.ok());
    CHECK(unknown.error().message.find("Tanınmayan işlem") != std::string::npos);
    auto nameless = f.run("YAZDIRMAPROFİLİ islem=sil");
    CHECK_FALSE(nameless.ok());
    CHECK(nameless.error().message.find("profil adı gerekir") != std::string::npos);

    // `yon` takes two words and nothing else.
    auto sideways = f.run("YAZDIRMAPROFİLİ islem=ekle ad=Yan kagit=A4 yon=çapraz");
    CHECK_FALSE(sideways.ok());
    CHECK(sideways.error().message.find("dikey ya da yatay") != std::string::npos);

    // A CUSTOM SHEET HAS TO SAY HOW BIG IT IS: only the command can tell "not
    // given" from a number, so this refusal lives here (commands/print.cpp).
    auto sizeless = f.run("YAZDIRMAPROFİLİ islem=ekle ad=Rulo kagit=ozel");
    CHECK_FALSE(sizeless.ok());
    CHECK(sizeless.error().message.find("genislik ve yukseklik") != std::string::npos);
    f.must("YAZDIRMAPROFİLİ islem=ekle ad=Rulo kagit=ozel genislik=900 yukseklik=1200");
    REQUIRE(f.profiles.find("Rulo") != nullptr);
    CHECK_EQ(f.profiles.find("Rulo")->width_mm, 900);

    // A paper the table does not hold is refused by the store, through the engine.
    auto tiny = f.run("YAZDIRMAPROFİLİ islem=ekle ad=Küçük kagit=A9");
    CHECK_FALSE(tiny.ok());
    CHECK(tiny.error().message.find("Tanınmayan kâğıt") != std::string::npos);
}

TEST_CASE("YAZDIR: pencere ve çıktı yeri zorunlu; ikisi birlikte verilemez")
{
    Rig f;

    // Neither a file nor a printer: the message names both parameters.
    auto nowhere = f.run("YAZDIR pencere=0,0 pencere=40,30");
    CHECK_FALSE(nowhere.ok());
    CHECK(nowhere.error().message.find("dosya=<yol>") != std::string::npos);
    CHECK(nowhere.error().message.find("yazici=<ad>") != std::string::npos);
    CHECK(f.asked.empty()); // nothing reached the engine

    // Both at once is a question with two answers.
    auto both = f.run("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/a.pdf yazici=Ofis");
    CHECK_FALSE(both.ok());
    CHECK(both.error().message.find("birlikte verilemez") != std::string::npos);

    // A window that is a line rather than a rectangle.
    auto flat = f.run("YAZDIR pencere=0,0 pencere=0,30 dosya=/tmp/a.pdf");
    CHECK_FALSE(flat.ok());
    CHECK(flat.error().message.find("dikdörtgen") != std::string::npos);

    // The window as two corners, in either order, becomes the box it encloses.
    f.must("YAZDIR pencere=45,35 pencere=-5,-5 dosya=/tmp/pafta.pdf");
    REQUIRE_EQ(f.asked.size(), std::size_t{1});
    const PrintRequest& sent = f.asked.back();
    CHECK(sent.verb == PrintRequest::Verb::ToPdf);
    CHECK_EQ(sent.window.min_x, core::Mm{-5000});
    CHECK_EQ(sent.window.min_y, core::Mm{-5000});
    CHECK_EQ(sent.window.max_x, core::Mm{45000});
    CHECK_EQ(sent.window.max_y, core::Mm{35000});
    CHECK_EQ(sent.path, std::string("/tmp/pafta.pdf"));
    CHECK(f.said.find("PDF yazıldı") != std::string::npos);

    // An empty `yazici` is the system's default printer, which is a VALUE and
    // not an omission: the command must be able to say "wherever this machine
    // prints" without naming a queue.
    f.must("YAZDIR pencere=0,0 pencere=40,30 yazici=\"\"");
    CHECK(f.asked.back().verb == PrintRequest::Verb::ToPrinter);
    CHECK(f.asked.back().printer.empty());
    CHECK(f.said.find("Yazıcıya gönderildi") != std::string::npos);

    // A profile that does not exist is named, with the list of the ones that do.
    auto missing = f.run("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/a.pdf profil=\"Yok Böyle\"");
    CHECK_FALSE(missing.ok());
    CHECK(missing.error().message.find("Böyle bir yazdırma profili yok") != std::string::npos);
}

TEST_CASE("YAZDIR: şifre ve izinler motora gider, günlüğe HİÇBİRİ yazılmaz")
{
    Rig f;
    f.must("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/gizli.pdf sifre=cokgizli "
           "sahip_sifresi=patron degistirilebilir=hayır baslik=Pafta yazar=Mühendis");

    // The engine got them — it has to, it does the encrypting.
    REQUIRE_EQ(f.asked.size(), std::size_t{1});
    CHECK_EQ(f.asked.back().user_password, std::string("cokgizli"));
    CHECK_EQ(f.asked.back().owner_password, std::string("patron"));
    CHECK(f.asked.back().allow_print); // not withheld
    CHECK(f.asked.back().allow_copy);  // not withheld
    CHECK_FALSE(f.asked.back().allow_modify);

    // AND NOTHING WAS JOURNALLED, which is what this case exists for. A plot is
    // a read-only command: it writes no entity, so it is not a document
    // mutation, and replaying one would rewrite somebody's PDF — the same
    // reason AÇ and KAYDET are not journalled either. The password therefore
    // cannot reach the journal at all, by construction rather than by care.
    CHECK(journal_of(f.journal).empty());

    // A permission word that is not a yes or a no is refused by the BUS, before
    // the body runs: they are declared `Bool`, so nothing here parses a word.
    auto nonsense =
        f.run("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/a.pdf sifre=x yazdirilabilir=parlat");
    CHECK_FALSE(nonsense.ok());

    // Everything withheld: three refusals, no special word for "none".
    f.must("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/kapali.pdf sifre=x "
           "yazdirilabilir=hayır kopyalanabilir=hayır degistirilebilir=hayır");
    CHECK_FALSE(f.asked.back().allow_print);
    CHECK_FALSE(f.asked.back().allow_copy);
    CHECK_FALSE(f.asked.back().allow_modify);

    // A comma-separated list was the first shape of this and the parser read
    // `yazdir,kopyala` as a COORDINATE — three booleans have no such corner.
    CHECK(f.run("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/a.pdf izin=yazdir,kopyala").ok() ==
          false);
}

TEST_CASE("YAZDIR: motor bağlı değilse söyler; komut satırı ile betik aynı isteği üretir")
{
    // No engine: the command says so rather than pretending a sheet came out.
    Rig headless(false);
    auto refused = headless.run("YAZDIR pencere=0,0 pencere=40,30 dosya=/tmp/a.pdf");
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().message.find("Yazdırma motoru bağlı değil") != std::string::npos);
    auto refused_profile = headless.run("YAZDIRMAPROFİLİ islem=listele");
    CHECK_FALSE(refused_profile.ok());

    // THE EQUALITY PROOF (test.md R3): the same plot from the command line and
    // from a script gives one request and one journal line.
    Rig cli;
    cli.must("YAZDIR pencere=-5,-5 pencere=45,35 dosya=/tmp/pafta.pdf profil=\"A3 Yatay\" "
             "baslik=Pafta");

    Rig scr;
    {
        script::JsonRunner runner(scr.bus, script::Sandbox::Project);
        auto r = runner.run_text(R"({
            "ad": "Yazdırma kanıtı",
            "komutlar": [
                {"cmd": "core.print",
                 "args": {"pencere": [[-5000, -5000], [45000, 35000]],
                          "dosya": "/tmp/pafta.pdf",
                          "profil": "A3 Yatay",
                          "baslik": "Pafta"}}
            ]
        })");
        if (!r) FAIL_WITH("betik", r.error().message);
    }

    REQUIRE_EQ(cli.asked.size(), std::size_t{1});
    REQUIRE_EQ(scr.asked.size(), std::size_t{1});
    const PrintRequest& a = cli.asked.front();
    const PrintRequest& b = scr.asked.front();
    CHECK(a.verb == b.verb);
    CHECK_EQ(a.window.min_x, b.window.min_x);
    CHECK_EQ(a.window.max_y, b.window.max_y);
    CHECK_EQ(a.path, b.path);
    CHECK_EQ(a.profile, b.profile);
    CHECK_EQ(a.title, b.title);

    // NEITHER CLIENT JOURNALLED ANYTHING, and that is the right answer for a
    // plot: a journal is replayed on crash recovery, and a replayed plot would
    // write a PDF nobody asked for a second time. What the two clients must
    // agree on is the REQUEST, which is what is compared above.
    CHECK(journal_of(cli.journal).empty());
    CHECK(journal_of(scr.journal).empty());
    CHECK_EQ(journal_of(cli.journal), journal_of(scr.journal));
}

TEST_CASE("YAZDIR: ölçülerin boyutlandığı paftadan başka ölçekte basılırken uyarır, yine de basar")
{
    // TODOS C-10: a 2,5 mm figure laid out for 1/1000 is half a millimetre tall
    // on a 1/5000 sheet. The sheet still prints; what it will look like is said
    // first, with the command that sizes the dimensions for it.
    Rig f;
    f.must("ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3");
    auto printed = f.run("YAZDIR merkez=10,0 olcek=5000 dosya=/tmp/pafta.pdf");
    if (!printed) FAIL_WITH("YAZDIR", printed.error().message);
    bool warned = false;
    for (const std::string& w : printed.value().warnings)
        warned = warned || (w.find("1 ölçü 1/1000 paftası için boyutlandırılmış; 1/5000 çıktıda "
                                   "yazıları 0,5 mm olur") != std::string::npos &&
                            w.find("ÖLÇÜYENİLE olcek=5000") != std::string::npos);
    CHECK(warned);

    // At the scale they were sized for, not a word.
    auto same = f.run("YAZDIR merkez=10,0 olcek=1000 dosya=/tmp/pafta.pdf");
    REQUIRE(same.ok());
    for (const std::string& w : same.value().warnings)
        CHECK(w.find("boyutlandırılmış") == std::string::npos);
}
