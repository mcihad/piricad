// SPDX-License-Identifier: GPL-3.0-or-later
// The file commands — YENİ, AÇ, KAYDET, FARKLIKAYDET, İÇEAKTAR, DIŞAAKTAR.
//
// Starting, opening and saving are COMMANDS, like everything else that changes
// application state (Article 1.1). The file dialog is not the feature; it collects one
// argument and hands it to the same bus a script uses, so every one of these runs
// headless, from the command line, from a JSON script and from the AI schema with
// no privileged path for the mouse (Article 1.2, 5.15).
//
// The bodies are thin on purpose. The format work — the mmap-able native layout,
// the untrusted-input bounds checking, GDAL — lives in /src/io, and arrives here
// through `Bus::on_file_request`, which `io::FileService` installs. That is the
// same seam `BETİK` uses for the script engine, and it exists because Article 3.2
// forbids /src/command from including /src/io while io.md R4 requires these to be
// registered commands. See `kentos_cad/io/service.hpp` for the full reasoning.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <string>

namespace kentos::command {
namespace {

/// One place where "no file engine" is reported, so the message a headless test
/// sees and the message a user sees are the same string.
bool engine_missing(Context& ctx, Bus& bus)
{
    if (bus.on_file_request) return false;
    ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                 "Dosya motoru bağlı değil; bu ortamda dosya açılıp "
                                 "kaydedilemez."));
    return true;
}

/// Sends the request and reports. A file operation that could not be carried out
/// FAILS the command rather than echoing a complaint: inside a script that must
/// abort the run and roll the document back, and a save that silently did nothing
/// is the worst outcome this product has.
Task<void> submit(Context& ctx, Bus& bus, const FileRequest& request)
{
    auto result = co_await bus.on_file_request(request);
    if (!result) {
        ctx.session().fail(result.error());
        co_return;
    }
    // A FILE THIS CALL WROTE IS PART OF ITS ANSWER. Only the verbs that write
    // one: opening and importing READ a file, and naming a read as an output
    // would tell a client it produced something it did not (TODOS C-03).
    switch (request.verb) {
    case FileRequest::Verb::Save:
    case FileRequest::Verb::SaveAs:
    case FileRequest::Verb::Export:
    case FileRequest::Verb::ExportStyle:
    case FileRequest::Verb::ExportPoints:
        if (!request.path.empty()) ctx.wrote(request.path);
        break;
    case FileRequest::Verb::New:
    case FileRequest::Verb::Open:
    case FileRequest::Verb::Import:
    case FileRequest::Verb::ImportPoints: break;
    }
    ctx.echo(result.value());
}

/// The path a save should go to: what the user typed, or the file the document
/// already belongs to. Resolved HERE rather than in /src/io so the journal line
/// carries the concrete path and a replay saves to the same place (§2.2).
std::string resolve_target(Bus& bus, const std::string& typed)
{
    if (!typed.empty()) return typed;
    return bus.on_current_file ? bus.on_current_file() : std::string{};
}

// ----------------------------------------------------------------- YENİ -----

/// YENİ — an empty drawing in place of the current one.
///
/// IT ASKS NOTHING, and that is the decision rather than an omission. A drawing
/// with unsaved work in it must not be thrown away silently, but the question
/// cannot live here: a command body is the one thing in this program that runs
/// identically for a mouse, a typed line, a script and an agent (kentoscad.md
/// §2.4), and "kaydedilsin mi?" has no answer in a batch run. `AÇ` settled this
/// the same way — it replaces the document without asking, and `docs/komutlar/
/// open.md` says so in one sentence — and the shell asks AROUND the command:
/// `MainWindow::newProject` puts the three-button question up first and
/// dispatches this only once the user has answered, exactly as `closeEvent`
/// does. So the person at the workstation is protected by the window, and a
/// script that says YENİ gets what it asked for.
Task<void> run_new(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb = FileRequest::Verb::New;
    co_await submit(ctx, bus, request);
    if (ctx.session().state() == SessionState::Failed) co_return;

    // AND THE VIEW GOES BACK TO WHERE A DRAWING STARTS. Left alone it would
    // still be framing a corner of the parcel that is no longer there, so an
    // empty canvas would look like a lost one. It is a view request like
    // `YAKINLAŞ SIFIRLA`'s, so it touches no document and no journal line — and
    // it is HERE rather than in the window, because a capability reachable only
    // by mouse is the one thing Article 5.15 forbids outright.
    if (bus.on_view_request) bus.on_view_request("SIFIRLA", 1.0);
}

// ------------------------------------------------------------------- AÇ -----

Task<void> run_open(Context& ctx)
{
    auto path = co_await ctx.text("dosya", "Açılacak proje dosyası");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb = FileRequest::Verb::Open;
    request.path = *path;
    co_await submit(ctx, bus, request);
}

// --------------------------------------------------------------- KAYDET -----

Task<void> run_save(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    const Value typed  = ctx.argument("dosya");
    std::string target = resolve_target(bus, typed.empty() ? std::string{} : typed.as_text());

    if (target.empty()) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument,
                      "Bu çizim henüz bir dosyaya bağlı değil. FARKLIKAYDET ile bir ad verin."));
        co_return;
    }
    ctx.record("dosya", Value::text(target));

    FileRequest request;
    request.verb = FileRequest::Verb::Save;
    request.path = std::move(target);
    co_await submit(ctx, bus, request);
}

// --------------------------------------------------------- FARKLIKAYDET -----

Task<void> run_save_as(Context& ctx)
{
    auto path = co_await ctx.text("dosya", "Kaydedilecek proje dosyası");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb = FileRequest::Verb::SaveAs;
    request.path = *path;
    co_await submit(ctx, bus, request);
}

// ------------------------------------------------------------ İÇEAKTAR -----

Task<void> run_import(Context& ctx)
{
    auto path = co_await ctx.text("dosya", "İçe aktarılacak dosya");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb    = FileRequest::Verb::Import;
    request.path    = *path;
    request.session = &ctx.session();

    if (const Value format = ctx.argument("bicim"); !format.empty()) {
        request.format = format.as_text();
        ctx.record("bicim", format);
    }

    // WHICH LAYERS. Empty is every layer, so a bare İÇEAKTAR is unchanged and
    // this is purely additive. The wizard's checklist sends the same argument the
    // command line does — there is no private road from the dialog (Article 1.2).
    if (const Value only = ctx.argument("katmanlar"); !only.empty()) {
        // ONE text, comma separated — `katmanlar="PARSEL,BİNA"`. A list would be
        // the tidier shape, but a comma cannot occur in a DXF or DWG layer name
        // (AutoCAD refuses it), so the separator is unambiguous and the argument
        // stays one journal token that a human can read and retype.
        const std::string& raw = only.as_text();
        std::string one;
        for (const char c : raw) {
            if (c == ',') {
                if (!one.empty()) request.layers.push_back(one);
                one.clear();
            } else if (!(one.empty() && (c == ' ' || c == '\t'))) {
                one.push_back(c);
            }
        }
        while (!one.empty() && (one.back() == ' ' || one.back() == '\t'))
            one.pop_back();
        if (!one.empty()) request.layers.push_back(one);

        ctx.record("katmanlar", only);
    }

    // WHICH FIELDS BECOME COLUMNS. Same shape as `katmanlar`: one text, comma
    // separated, `*` for all. Absent means none, so a drawing imported before
    // this parameter existed imports exactly as it did.
    if (const Value wanted = ctx.argument("alanlar"); !wanted.empty()) {
        const std::string text = wanted.as_text();
        std::string one;
        for (const char ch : text) {
            if (ch == ',') {
                if (!one.empty()) request.fields.push_back(one);
                one.clear();
                continue;
            }
            if (ch == ' ' && one.empty()) continue;
            one.push_back(ch);
        }
        while (!one.empty() && one.back() == ' ')
            one.pop_back();
        if (!one.empty()) request.fields.push_back(one);
        ctx.record("alanlar", wanted);
    }

    // The command's OWN transaction, so the whole import is one undo step and any
    // failure rolls the document back to exactly its pre-import state (io.md R17,
    // P11). This is the only file verb that mutates the document in place.
    request.tx = &ctx.transaction();
    co_await submit(ctx, bus, request);
}

// ------------------------------------------------------------ DIŞAAKTAR -----

Task<void> run_export(Context& ctx)
{
    auto path = co_await ctx.text("dosya", "Dışa aktarılacak dosya");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb = FileRequest::Verb::Export;
    request.path = *path;

    if (const Value format = ctx.argument("bicim"); !format.empty()) {
        request.format = format.as_text();
        ctx.record("bicim", format);
    }
    // WHICH DXF RELEASE. Recorded as the year the user typed, so a replay writes
    // the same file; the file service refuses it for any other format.
    if (const Value version = ctx.argument("surum"); !version.empty()) {
        request.version = static_cast<int>(version.as_int());
        ctx.record("surum", version);
    }
    co_await submit(ctx, bus, request);
}

/// STİLAKTAR — one layer's symbology as a QGIS QML style file.
///
/// Separate from DIŞAAKTAR because they export different things: one writes the
/// GEOMETRY out in another format, the other writes how it LOOKS. Folding them
/// into one verb would mean a `bicim=QML` that silently ignores every entity in
/// the drawing, which is the kind of surprise a file command must not hold.
Task<void> run_export_style(Context& ctx)
{
    auto layer = co_await ctx.text("katman", "Stili aktarılacak katman");
    if (!layer || layer->empty()) co_return;

    auto path = co_await ctx.text("dosya", "Yazılacak QML dosyası");
    if (!path || path->empty()) co_return;

    Bus& bus = ctx.session().bus();
    if (engine_missing(ctx, bus)) co_return;

    FileRequest request;
    request.verb  = FileRequest::Verb::ExportStyle;
    request.path  = *path;
    request.layer = *layer;
    co_await submit(ctx, bus, request);
}

} // namespace

KENTOS_COMMAND(exportstyle)
{
    return CommandSpec{
        .id       = "core.exportstyle",
        .names    = {"STİLAKTAR", "STILAKTAR", "EXPORTSTYLE", "STAKTAR"},
        .category = Category::File,
        .params =
            {
                Param::text("katman", Arity::exactly(1), "Stili aktarılacak katmanın adı"),
                Param::text("dosya", Arity::exactly(1), "Yazılacak .qml dosyasının yolu"),
            },
        // Writes a file and touches no entity, so there is nothing to undo and
        // nothing to journal as a document mutation.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.",
        .run     = &run_export_style,
        .effect  = Effect::Query | Effect::FileWrite,
    };
}

KENTOS_COMMAND(newfile)
{
    return CommandSpec{
        .id       = "core.new",
        .names    = {"YENİ", "YENI", "NEW"},
        .category = Category::File,
        .params   = {},
        // NOT UNDOABLE, and `None` is the only honest answer rather than the
        // convenient one. An undo step is an inverse `Op` list over ONE document
        // (transaction.hpp); this command does not edit a document, it puts a
        // different one in the window, so there is no inverse to record and the
        // steps already on the stack point at slots that no longer exist. That
        // is why the stack is cleared rather than left standing: a GERİAL over a
        // stale entry would apply the previous drawing's edits to this one, and
        // in a cadastral drawing the slot that used to be a parcel is now
        // somebody else's parcel (model.md R5). `AÇ` carries `None` for exactly
        // this reason and the two must not disagree.
        .undo = UndoPolicy::None,
        // NOT `Interactive`, unlike every other file command: that flag means
        // "can ask the user for input mid-run" and this one asks nothing, having
        // no parameters to ask about. `GERİAL` is the precedent. It costs no
        // client anything — the flag gates nothing, it DESCRIBES — and the
        // generated reference would otherwise tell a reader this command
        // prompts.
        .flags = Flags::Scriptable,
        // NOT `ReadOnly`, although it opens no transaction: `Bus::finish` skips
        // the journal entry for a ReadOnly command, and a replay that silently
        // dropped the YENİ would rebuild the new drawing's commands on top of
        // the old drawing (Article 6.4).
        //
        // Deliberately NOT AiAccessible, for the reason AÇ is not: it discards
        // unsaved work and cannot be undone, and .claude/ai.md keeps a
        // destructive, non-undoable act out of reach of a suggestion.
        .summary = "Boş bir çizim açar; ekrandaki çizimin yerine geçer.",
        .run     = &run_new,
        // REPLACES THE DRAWING AND RESETS THE VIEW. No file is read and none is
        // written — that is what separates it from AÇ.
        .effect = Effect::DocumentEdit | Effect::ViewChange,
    };
}

KENTOS_COMMAND(open)
{
    return CommandSpec{
        .id       = "core.open",
        .names    = {"AÇ", "AC", "OPEN"},
        .category = Category::File,
        .params   = {Param::text("dosya", Arity::exactly(1),
                                 "Açılacak KentOSCad proje dosyasının yolu (.pcad)")},
        // Opening replaces the document, so there is nothing to undo back INTO —
        // the previous drawing is gone the moment the new one is on screen, which
        // is what every CAD and GIS application this product's users know does.
        // The undo stack is cleared rather than left pointing at slots that no
        // longer exist.
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable,
        // Deliberately NOT AiAccessible: replacing the document discards
        // unsaved work, and .claude/ai.md keeps a destructive, non-undoable act
        // out of reach of a suggestion.
        .summary = "Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar.",
        .run     = &run_open,
        // READS A FILE AND REPLACES THE DRAWING. `ReadOnly` is on it because it
        // opens no transaction, which says nothing about either half.
        .effect = Effect::Query | Effect::FileRead | Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(save)
{
    return CommandSpec{
        .id       = "core.save",
        .names    = {"KAYDET", "SAVE", "KYD"},
        .category = Category::File,
        .params   = {Param::text("dosya", Arity::optional(),
                                 "Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır")},
        // Saving does not change the document, so it is not an undo step and it
        // does not go through a transaction. ReadOnly is how the bus is told.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder.",
        .run     = &run_save,
        // WRITES OVER A FILE. This is the command CLAUDE.md's `ReadOnly` note
        // names first: the flag means "skips the transaction path", never "safe".
        .effect = Effect::Query | Effect::FileWrite,
    };
}

KENTOS_COMMAND(saveas)
{
    return CommandSpec{
        .id       = "core.saveas",
        .names    = {"FARKLIKAYDET", "SAVEAS", "FKAYDET"},
        .category = Category::File,
        .params = {Param::text("dosya", Arity::exactly(1), "Yeni proje dosyasının yolu (.pcad)")},
        .undo   = UndoPolicy::None,
        .flags  = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar.",
        .run     = &run_save_as,
        .effect  = Effect::Query | Effect::FileWrite,
    };
}

KENTOS_COMMAND(import)
{
    return CommandSpec{
        .id       = "core.import",
        .names    = {"İÇEAKTAR", "ICEAKTAR", "IMPORT", "IAKTAR"},
        .category = Category::File,
        .params =
            {
                Param::text("dosya", Arity::exactly(1), "İçe aktarılacak dosyanın yolu"),
                Param::text("bicim", Arity::optional(),
                            "Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur"),
                Param::text("katmanlar", Arity::optional(),
                            "Yalnızca bu katmanlar okunur, virgülle ayrılır; "
                            "verilmezse tümü"),
                Param::text("alanlar", Arity::optional(),
                            "Sütun olarak okunacak öznitelik alanları, virgülle; "
                            "* hepsi; verilmezse alan okunmaz"),
            },
        // io.md R17: one transaction, one undo entry, and a failure leaves the
        // document byte for byte as it was.
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable,
        .summary = "Dış bir veri dosyasını çizime ekler.",
        .run     = &run_import,
        .effect  = Effect::Query | Effect::FileRead | Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(exportfile)
{
    return CommandSpec{
        .id       = "core.export",
        .names    = {"DIŞAAKTAR", "DISAAKTAR", "EXPORT", "DAKTAR"},
        .category = Category::File,
        .params =
            {
                Param::text("dosya", Arity::exactly(1), "Yazılacak dosyanın yolu"),
                Param::text("bicim", Arity::optional(),
                            "Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur"),
                Param::integer("surum", Arity::optional(),
                               "DXF sürümü: 2000, 2004, 2007 (varsayılan), 2010, 2013, 2018"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly,
        .summary = "Çizimi dış bir veri biçimine yazar.",
        .run     = &run_export,
        .effect  = Effect::Query | Effect::FileWrite,
    };
}

} // namespace kentos::command
