// SPDX-License-Identifier: GPL-3.0-or-later
// core.print (YAZDIR), core.print_profile (YAZDIRMAPROFİLİ)
//
// A print is a WINDOW of the drawing put onto a SHEET. The sheet comes from a
// profile — paper, orientation, resolution, margin — chosen by name or, with no
// name, the default one; any field can be overridden on the line. The window
// comes from two corners, given on the line or picked. Where the sheet goes is
// either a PDF file or a printer, and the command insists on knowing which:
// a script that "prints" to whatever printer happens to be the default is a
// script that prints to the wrong office.
//
// The work — a PDF writer, a printer driver, the profile file — is Qt's and
// the application's, behind `Bus::on_print_request` (bus.hpp). This file owns
// the two commands, their parameters and what the journal records: the profile
// NAME and every explicit override, never the PDF passwords.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <string>

namespace kentos::command {
namespace {

bool engine_missing(Context& ctx, Bus& bus)
{
    if (bus.on_print_request) return false;
    ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                 "Yazdırma motoru bağlı değil; bu ortamda yazdırılamaz ve PDF "
                                 "alınamaz. Uygulama içinden çalıştırın."));
    return true;
}

/// Sends the request and reports. A print that could not be carried out FAILS
/// the command, for the reason a file command does: a script must abort, and a
/// sheet that silently never came out is the worst outcome a plot has.
Task<void> submit(Context& ctx, Bus& bus, const PrintRequest& request)
{
    auto result = co_await bus.on_print_request(request);
    if (!result) {
        ctx.session().fail(result.error());
        co_return;
    }
    // THE SHEET IS THE ANSWER, and a client told only "tamam" cannot find it.
    if (!request.path.empty()) ctx.wrote(request.path);
    ctx.echo(result.value());
}

/// `yon=dikey|yatay` into the request's tri-state, refusing anything else.
bool read_orientation(Context& ctx, PrintRequest& request)
{
    const Value v = ctx.argument("yon");
    if (v.empty()) return true;
    const std::string& word = v.as_text();
    if (core::turkish_key_equals(word, "yatay") || core::turkish_key_equals(word, "landscape")) {
        request.landscape = 1;
        ctx.record("yon", Value::text("yatay"));
        return true;
    }
    if (core::turkish_key_equals(word, "dikey") || core::turkish_key_equals(word, "portrait")) {
        request.landscape = 0;
        ctx.record("yon", Value::text("dikey"));
        return true;
    }
    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                 "'yon' dikey ya da yatay olmalı. Girilen: '" + word + "'"));
    return false;
}

/// The sheet fields shared by both commands: paper, size, resolution, margin.
/// Each is recorded as typed, so a replay resolves the same sheet.
bool read_sheet(Context& ctx, PrintRequest& request)
{
    if (const Value v = ctx.argument("kagit"); !v.empty()) {
        request.paper = v.as_text();
        ctx.record("kagit", v);
    }
    if (const Value v = ctx.argument("genislik"); !v.empty()) {
        request.width_mm = v.as_int();
        ctx.record("genislik", v);
    }
    if (const Value v = ctx.argument("yukseklik"); !v.empty()) {
        request.height_mm = v.as_int();
        ctx.record("yukseklik", v);
    }
    if (!read_orientation(ctx, request)) return false;
    if (const Value v = ctx.argument("dpi"); !v.empty()) {
        request.dpi = v.as_int();
        ctx.record("dpi", v);
    }
    if (const Value v = ctx.argument("kenar"); !v.empty()) {
        request.margin_mm = v.as_int();
        ctx.record("kenar", v);
    }
    return true;
}

// ---- YAZDIR ------------------------------------------------------------------

Task<void> run_print(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    // WHAT GOES ON THE SHEET, said either way. `merkez` is the surveyor's way —
    // a pafta is "1:1000, centred here" — and two corners are the interface's,
    // because that is what a dragged frame produces. The centre wins when both
    // are given, and the window it stands for is computed by the engine, which
    // is the only side that knows the paper's printable size.
    PrintRequest request;
    const Value centre = ctx.argument("merkez");
    Value::Points corners;

    // ---- a layout prints ITSELF, and takes no window -----------------------
    //
    // A layout carries its own paper, its own margin and a map frame that knows
    // where it looks (`core/layout.hpp`), so `pencere`, `merkez`, `olcek` and
    // `profil` have nothing left to decide. Giving one anyway is REFUSED rather
    // than ignored: an argument silently dropped is an argument the user
    // believed in (command.md P15).
    if (const Value sheet = ctx.argument("yerlesim"); !sheet.empty()) {
        for (const char* other : {"pencere", "merkez", "olcek", "profil"})
            if (!ctx.argument(other).empty()) {
                ctx.session().fail(
                    core::err(core::ErrorCode::InvalidArgument,
                              "yerlesim ile '" + std::string(other) +
                                  "' birlikte verilmez: çıktı yerleşimi kendi kâğıdını ve kendi "
                                  "harita penceresini taşır."));
                co_return;
            }

        const std::string named = sheet.as_text();
        if (ctx.session().bus().document().layouts().find(named) == nullptr) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Çıktı yerleşimi yok: '" + named +
                                             "'. ÇIKTIYERLEŞİMİ islem=listele ile adları görün."));
            co_return;
        }
        request.layout = named;
        ctx.record("yerlesim", sheet);

        // PREFLIGHT, AND IT DOES NOT REFUSE. A sheet with one broken map link
        // still has to print the rest of itself; what it could not honour rides
        // out on `DispatchResult::warnings`, so a client that reports success
        // reports it WITH the problems rather than instead of them (L-15, C-03).
        const core::Layout* found = ctx.session().bus().document().layouts().find(named);
        if (found != nullptr)
            for (const std::string& one : core::layout_trouble(*found))
                ctx.warn(one);
    } else if (!centre.empty()) {
        request.has_centre = true;
        request.centre     = centre.as_point();
        ctx.record("merkez", centre);

        // THE SCALE, AND IT IS RECORDED RESOLVED: given on the line, else the
        // drawing's own plan scale (`core.plan.olcek`, a Project setting that
        // travels with the file). Writing the resolved number down is what makes
        // a replay produce the same sheet after somebody changes that setting.
        std::int64_t scale = ctx.argument("olcek").as_int();
        if (scale <= 0)
            scale = ctx.session().bus().project_settings().get("core.plan.olcek").as_int();
        if (scale <= 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Ölçek bilinmiyor: olcek=<N> verin (1:N) ya da projenin "
                                         "plan ölçeğini ayarlayın."));
            co_return;
        }
        request.scale = scale;
        ctx.record("olcek", Value::integer(scale));
    } else {
        corners = ctx.argument("pencere").as_points();
        if (corners.size() < 2) {
            corners.clear();
            auto first = co_await ctx.point("pencere", "Yazdırılacak alanın ilk köşesi");
            if (!first) co_return;
            auto second = co_await ctx.point("pencere", "Karşı köşe",
                                             PointOptions{.rubber_band   = true,
                                                          .rubber_origin = *first,
                                                          .rubber_shape  = RubberShape::Rectangle});
            if (!second) co_return;
            corners = {*first, *second};
        } else {
            ctx.record("pencere", Value::points(corners));
        }
        if (corners[0].x == corners[1].x || corners[0].y == corners[1].y) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Yazdırma penceresinin iki köşesi bir dikdörtgen "
                                         "çizmeli; iki köşe aynı doğru üzerinde."));
            co_return;
        }
    }

    // WHERE IT GOES: a file or a printer, exactly one of them.
    const Value file    = ctx.argument("dosya");
    const Value printer = ctx.argument("yazici");
    if (file.empty() && printer.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Çıktının yeri verilmedi: PDF için dosya=<yol>, yazıcı için "
                                     "yazici=<ad> yazın (yazici=\"\" sistem varsayılanıdır)."));
        co_return;
    }
    if (!file.empty() && !printer.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "dosya ve yazici birlikte verilemez; çıktı ya PDF dosyasına "
                                     "ya yazıcıya gider."));
        co_return;
    }

    if (engine_missing(ctx, bus)) co_return;

    request.verb = file.empty() ? PrintRequest::Verb::ToPrinter : PrintRequest::Verb::ToPdf;
    if (!request.has_centre && request.layout.empty())
        request.window =
            core::Box2{std::min(corners[0].x, corners[1].x), std::min(corners[0].y, corners[1].y),
                       std::max(corners[0].x, corners[1].x), std::max(corners[0].y, corners[1].y)};
    if (!file.empty()) {
        request.path = file.as_text();
        ctx.record("dosya", file);
    } else {
        request.printer = printer.as_text();
        ctx.record("yazici", printer);
    }
    if (const Value v = ctx.argument("profil"); !v.empty()) {
        request.profile = v.as_text();
        ctx.record("profil", v);
    }
    if (!read_sheet(ctx, request)) co_return;
    if (const Value v = ctx.argument("baslik"); !v.empty()) {
        request.title = v.as_text();
        ctx.record("baslik", v);
    }
    if (const Value v = ctx.argument("yazar"); !v.empty()) {
        request.author = v.as_text();
        ctx.record("yazar", v);
    }

    // ENCRYPTION. The two passwords are READ and never recorded: a print is a
    // read-only command and so is not journalled at all (the same decision AÇ
    // and KAYDET carry, and for the stronger reason here — replaying a plot
    // would silently rewrite somebody's PDF). The permissions are ordinary
    // booleans, so the bus validates them and nothing here parses a word.
    request.user_password  = ctx.argument("sifre").as_text();
    request.owner_password = ctx.argument("sahip_sifresi").as_text();
    const auto permission  = [&ctx](const char* name, bool& into) {
        const Value v = ctx.argument(name);
        if (v.empty()) return;
        into = v.as_bool(true);
        ctx.record(name, v);
    };
    permission("yazdirilabilir", request.allow_print);
    permission("kopyalanabilir", request.allow_copy);
    permission("degistirilebilir", request.allow_modify);
    co_await submit(ctx, bus, request);
}

// ---- YAZDIRMAPROFİLİ -----------------------------------------------------------

struct Operation
{
    const char* name;
    PrintRequest::Verb verb;
    bool needs_name;
};

constexpr Operation kOperations[] = {
    {"listele", PrintRequest::Verb::Profiles, false},
    {"ekle", PrintRequest::Verb::SetProfile, true},
    {"sil", PrintRequest::Verb::RemoveProfile, true},
    {"varsayilan", PrintRequest::Verb::SetDefault, true},
};

std::string operation_list()
{
    std::string out;
    for (const Operation& op : kOperations)
        out += (out.empty() ? "" : " / ") + std::string(op.name);
    return out;
}

Task<void> run_print_profile(Context& ctx)
{
    Bus& bus = ctx.session().bus();

    auto typed = co_await ctx.text("islem", "İşlem: " + operation_list());
    if (!typed) co_return;

    const Operation* op = nullptr;
    for (const Operation& candidate : kOperations)
        if (core::turkish_key_equals(*typed, candidate.name)) op = &candidate;
    if (op == nullptr) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Tanınmayan işlem: '" + *typed + "'. İşlemler: " + operation_list()));
        co_return;
    }
    ctx.record("islem", Value::text(op->name));

    PrintRequest request;
    request.verb = op->verb;
    if (op->needs_name) {
        auto name = co_await ctx.text("ad", "Profil adı");
        if (!name || name->empty()) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, std::string("'") + op->name +
                                                                "' için profil adı gerekir: "
                                                                "ad=<ad>"));
            co_return;
        }
        request.profile = *name;
    }
    if (op->verb == PrintRequest::Verb::SetProfile) {
        if (!read_sheet(ctx, request)) co_return;
        // A CUSTOM SHEET HAS TO SAY HOW BIG IT IS. Only the command can tell
        // "not given" from a number: the store is handed a profile that always
        // carries some size, and it would accept A4's by default — a roll-paper
        // profile that silently came out A4 is the wrong sheet, quietly.
        if (core::turkish_key_equals(request.paper, "ozel") &&
            (request.width_mm <= 0 || request.height_mm <= 0)) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "kagit=ozel için genislik ve yukseklik milimetre "
                                         "olarak verilmeli: genislik=<mm> yukseklik=<mm>."));
            co_return;
        }
    }

    if (engine_missing(ctx, bus)) co_return;
    co_await submit(ctx, bus, request);
}

} // namespace

KENTOS_COMMAND(print)
{
    return CommandSpec{
        .id       = "core.print",
        .names    = {"YAZDIR", "PRINT", "PLOT", "YZDR"},
        .title    = "Yazdır",
        .category = Category::File,
        .params =
            {
                Param::points("pencere", Arity{0, 2},
                              "Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da "
                              "verilmezse tıklatılır")
                    .en("window"),
                // OPTIONAL, and it has to be spelled out: `Param::point` takes
                // no arity and defaults to "exactly one", which made a print
                // with two corners refuse for want of a centre.
                Param{"merkez", ParamKind::Point, Arity::optional(),
                      "Kâğıdın ortalanacağı nokta; pencere yerine kullanılır"}
                    .en("center"),
                Param::integer("olcek", Arity::optional(),
                               "Ölçek paydası (1000 = 1/1000); merkez ile kullanılır, "
                               "verilmezse projenin plan ölçeği")
                    .en("scale"),
                Param::text("yerlesim", Arity::optional(),
                            "Basılacak çıktı yerleşiminin adı (ÇIKTIYERLEŞİMİ ile kurulur). "
                            "Verildiğinde kâğıt, kenar ve harita penceresi yerleşimden "
                            "gelir; pencere, merkez, olcek ve profil ile birlikte "
                            "verilmez")
                    .renamed_from("pafta")
                    .en("layout"),
                Param::text("dosya", Arity::optional(),
                            "PDF yazılacak dosya; yazici ile birlikte verilmez")
                    .en("file"),
                Param::text("yazici", Arity::optional(),
                            "Yazıcının adı; \"\" sistem varsayılanı. dosya ile birlikte verilmez")
                    .en("printer"),
                Param::text("profil", Arity::optional(),
                            "Yazdırma profili; verilmezse varsayılan profil")
                    .en("profile"),
                Param::text("kagit", Arity::optional(),
                            "Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel (genislik ve yukseklik ile)")
                    .en("paper"),
                Param::integer("genislik", Arity::optional(),
                               "ozel kâğıdın eni, milimetre (dikey duruşta)")
                    .en("width"),
                Param::integer("yukseklik", Arity::optional(),
                               "ozel kâğıdın boyu, milimetre (dikey duruşta)")
                    .en("height"),
                Param::text("yon", Arity::optional(), "dikey ya da yatay").en("orientation"),
                Param::integer("dpi", Arity::optional(), "Çözünürlük, inç başına nokta (72–4800)")
                    .en("dpi"),
                Param::integer("kenar", Arity::optional(), "Dört yandaki kenar boşluğu, milimetre")
                    .en("margin"),
                Param::text("baslik", Arity::optional(), "PDF belge başlığı").en("title"),
                Param::text("yazar", Arity::optional(), "PDF yazar alanı").en("author"),
                Param::text("sifre", Arity::optional(),
                            "PDF açma şifresi (kullanıcı şifresi); günlüğe yazılmaz")
                    .en("password"),
                Param::text("sahip_sifresi", Arity::optional(),
                            "PDF izinlerini değiştirme şifresi (sahip şifresi); günlüğe yazılmaz")
                    .en("owner_password"),
                Param::boolean("yazdirilabilir", Arity::optional(),
                               "Şifreli PDF: sahip şifresi olmayan yazdırabilir mi; "
                               "varsayılan evet")
                    .en("printable"),
                Param::boolean("kopyalanabilir", Arity::optional(),
                               "Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan evet")
                    .en("copyable"),
                Param::boolean("degistirilebilir", Arity::optional(),
                               "Şifreli PDF: belge değiştirilebilir mi; varsayılan evet")
                    .en("modifiable"),
            },
        // Writes a file or drives a printer and touches no entity: nothing to
        // undo, nothing to journal as a document mutation.
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly | Flags::AiAccessible,
        .summary = "Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF "
                   "dosyasına yazar ya da yazıcıya gönderir.",
        .run = &run_print,
        // THE WORST CASE, AND IT IS NOT VERB-SHAPED. `dosya=` writes a file and
        // `yazici=` sends the sheet to a printer, which is a thing that cannot be
        // taken back — so both are declared and the policy narrows by argument.
        // `ReadOnly` said nothing about either.
        .effect = Effect::Query | Effect::FileWrite | Effect::ExternalWrite,
    };
}

KENTOS_COMMAND(print_profile)
{
    return CommandSpec{
        .id       = "core.print_profile",
        .names    = {"YAZDIRMAPROFİLİ", "YAZDIRMAPROFILI", "PRINTPROFILE", "YZP"},
        .title    = "Yazdırma Profili",
        .category = Category::File,
        .params =
            {
                Param::text("islem", Arity::exactly(1), "listele, ekle, sil ya da varsayilan")
                    .en("action"),
                Param::text("ad", Arity::optional(), "Profilin adı (ekle, sil, varsayilan)")
                    .en("name"),
                Param::text("kagit", Arity::optional(),
                            "Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel; ekle için, varsayılan A4")
                    .en("paper"),
                Param::integer("genislik", Arity::optional(), "ozel kâğıdın eni, milimetre")
                    .en("width"),
                Param::integer("yukseklik", Arity::optional(), "ozel kâğıdın boyu, milimetre")
                    .en("height"),
                Param::text("yon", Arity::optional(), "dikey ya da yatay; varsayılan dikey")
                    .en("orientation"),
                Param::integer("dpi", Arity::optional(), "Çözünürlük; varsayılan 300").en("dpi"),
                Param::integer("kenar", Arity::optional(),
                               "Kenar boşluğu, milimetre; varsayılan 10")
                    .en("margin"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; "
                   "profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır.",
        .run     = &run_print_profile,
        // A PROFILE IS THIS MACHINE'S SETTING, not the drawing's: it is stored
        // beside the preferences and travels with neither the file nor the sheet.
        .effect      = Effect::Query | Effect::SettingsChange,
        .effect_verb = "islem",
        .verb_effects =
            {
                {"listele", Effect::Query},
                {"ekle", Effect::SettingsChange},
                {"sil", Effect::SettingsChange},
                {"varsayilan", Effect::SettingsChange},
            },
    };
}

} // namespace kentos::command
