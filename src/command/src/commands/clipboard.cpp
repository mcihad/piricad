// SPDX-License-Identifier: GPL-3.0-or-later
// core.copy_clip — PANOYAKOPYALA, core.cut — KES, core.paste — YAPIŞTIR.
//
// THE PAYLOAD IS THE NATIVE FORMAT. A clipboard holding a JSON of its own would
// be a second description of a document to keep in step with the first, and this
// program already has one that round-trips every kind, every style, every layer
// and every attribute column (io.md). So what goes on the clipboard is a native
// project file holding only the selection, written by the same writer a save
// uses and read by the same reader an open uses.
//
// KES IS A COPY AND AN ERASE IN ONE TRANSACTION, which is what makes it one undo
// step: a cut that left the copy done and the erase undone would be a cut the
// user could not take back.
//
// A FILE, NOT AN OS CLIPBOARD — here. `/src/command` may not touch a file at all
// (Article 3.2), so the work goes through `Bus::on_file_request` exactly as
// `AÇ` and `KAYDET` do, and `io::FileService` owns the bytes. The OS clipboard is
// the window layer's job because Qt is; `dosya=` is the road a script and a
// headless run take, and it is the same road (Article 1.2).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The keys the caller means: the named ones, else the live selection, else
/// ASKED FOR — the order every modify command keeps (context.hpp `want_objects`).
///
/// THIS USED NOT TO ASK. The reasoning was that a copy with nothing selected is
/// a mistake rather than a question; the result was that Kes and Panoya Kopyala
/// in the Düzen menu were dead with nothing highlighted, which is the one state a
/// user reaching for a menu is most likely to be in. Every CAD program answers
/// COPYCLIP with an empty selection by asking which objects, and so does this.
/// Returns false when the command is over — cancelled, or refused by the helper.
Task<bool> wanted(Context& ctx, const char* message, const char* example,
                  std::vector<std::uint64_t>& out)
{
    std::vector<std::int64_t> ids;
    if (!co_await want_objects(ctx, "nesneler", message, ids, 0, example)) co_return false;
    out.clear();
    for (const std::int64_t raw : ids)
        if (raw > 0) out.push_back(static_cast<std::uint64_t>(raw));
    co_return true;
}

/// THE BASE POINT a paste will carry the payload by (TODOS C-08): given as
/// `taban`, or — with `tabanli=evet`, what the column's "taban noktalı" entry
/// sends — asked for after the objects, so it can be picked on them. None
/// leaves the payload's lower-left corner, as a paste always used. False when
/// the command is over.
Task<bool> base_point(Context& ctx, std::optional<core::Point2>& base)
{
    base.reset();
    if (const Value v = ctx.argument("taban"); !v.empty()) {
        base = v.as_point();
        co_return true;
    }
    if (const Value v = ctx.argument("tabanli"); v.empty() || !v.as_bool()) co_return true;
    auto picked = co_await ctx.point("taban", "Taban noktası: yapıştırırken gösterilen yere gelir");
    if (!picked) co_return false;
    base = *picked;
    ctx.record("tabanli", Value{});
    co_return true;
}

Task<void> run_copy(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (!bus.on_file_request) {
        ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                     "Dosya motoru bağlı değil; pano bu yapıda çalışmıyor."));
        co_return;
    }

    std::vector<std::uint64_t> keys;
    if (!co_await wanted(ctx, "Panoya alınacak nesneleri seçin, sonra Enter",
                         "PANOYAKOPYALA nesneler=1", keys))
        co_return;
    std::optional<core::Point2> base;
    if (!co_await base_point(ctx, base)) co_return;

    FileRequest request;
    request.verb     = FileRequest::Verb::ClipboardCopy;
    request.entities = keys;
    request.base     = base;
    if (const Value v = ctx.argument("dosya"); !v.empty()) request.path = v.as_text();

    auto said = co_await bus.on_file_request(request);
    if (!said) {
        ctx.session().fail(said.error());
        co_return;
    }
    ctx.record("nesneler", ctx.argument("nesneler").empty() ? Value::ids([&keys] {
                   Value::Ints ids;
                   ids.reserve(keys.size());
                   for (const std::uint64_t k : keys)
                       ids.push_back(static_cast<std::int64_t>(k));
                   return ids;
               }())
                                                            : ctx.argument("nesneler"));
    if (base) ctx.record("taban", Value::point(*base));
    ctx.echo(said.value());
}

Task<void> run_cut(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (!bus.on_file_request) {
        ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                     "Dosya motoru bağlı değil; pano bu yapıda çalışmıyor."));
        co_return;
    }

    std::vector<std::uint64_t> keys;
    if (!co_await wanted(ctx, "Kesilecek nesneleri seçin, sonra Enter", "KES nesneler=1", keys))
        co_return;
    std::optional<core::Point2> base;
    if (!co_await base_point(ctx, base)) co_return;

    FileRequest request;
    request.verb     = FileRequest::Verb::ClipboardCopy;
    request.entities = keys;
    request.base     = base;
    if (const Value v = ctx.argument("dosya"); !v.empty()) request.path = v.as_text();

    auto said = co_await bus.on_file_request(request);
    if (!said) {
        ctx.session().fail(said.error());
        co_return;
    }

    // THE ERASE IS IN THIS COMMAND'S OWN TRANSACTION, so the copy and the
    // removal are one undo step. A cut whose copy survived its own undo would be
    // a cut the user could not take back.
    const core::Document& doc = ctx.document();
    std::size_t gone          = 0;
    for (const std::uint64_t raw : keys) {
        const core::EntityId slot = doc.slot_of(static_cast<core::EntityKey>(raw));
        if (slot == core::kNoEntity || !doc.alive(slot)) continue;
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        if (auto st = ctx.transaction().erase_entity(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++gone;
    }

    ctx.record("nesneler", [&keys] {
        Value::Ints ids;
        ids.reserve(keys.size());
        for (const std::uint64_t k : keys)
            ids.push_back(static_cast<std::int64_t>(k));
        return Value::ids(std::move(ids));
    }());
    if (base) ctx.record("taban", Value::point(*base));
    ctx.echo(said.value() + " " + std::to_string(gone) + " nesne çizimden silindi.");
}

Task<void> run_paste(Context& ctx)
{
    Bus& bus = ctx.session().bus();
    if (!bus.on_file_request) {
        ctx.session().fail(core::err(core::ErrorCode::Unsupported,
                                     "Dosya motoru bağlı değil; pano bu yapıda çalışmıyor."));
        co_return;
    }

    bool in_place = false;
    if (const Value v = ctx.argument("yerinde"); !v.empty()) in_place = v.as_bool();

    core::Point2 at{};
    if (!in_place) {
        // THE PAYLOAD FOLLOWS THE CURSOR unless the user asked for otherwise:
        // that is what pasting means to a hand. A copy between two drawings in
        // the same coordinate system wants `yerinde=evet` instead, and the
        // command says which it did.
        auto where = co_await ctx.point(
            "nokta",
            "Yapıştırılacak yer: taban noktası ya da nesnelerin sol alt köşesi buraya gelir");
        if (!where) co_return;
        at = *where;
    }

    FileRequest request;
    request.verb     = FileRequest::Verb::ClipboardPaste;
    request.tx       = &ctx.transaction();
    request.session  = &ctx.session();
    request.at       = at;
    request.in_place = in_place;
    if (const Value v = ctx.argument("dosya"); !v.empty()) request.path = v.as_text();

    auto said = co_await bus.on_file_request(request);
    if (!said) {
        ctx.session().fail(said.error());
        co_return;
    }
    if (!in_place) ctx.record("nokta", Value::point(at));
    ctx.echo(said.value());
}

} // namespace

KENTOS_COMMAND(copy_clip)
{
    return CommandSpec{
        .id       = "core.copy_clip",
        .names    = {"PANOYAKOPYALA", "PANOKOPYALA", "COPYCLIP", "PKP"},
        .title    = "Panoya Kopyala",
        .category = Category::File,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Panoya alınacak nesneler; verilmezse seçim kullanılır"}
                    .en("objects"),
                Param::text("dosya", Arity::optional(),
                            "Panonun yazılacağı dosya; verilmezse ortak pano dosyası")
                    .en("file"),
                Param{"taban", ParamKind::Point, Arity::optional(),
                      "Yapıştırırken gösterilen yere gelecek taban noktası"}
                    .en("base_point"),
                Param::boolean("tabanli", Arity::optional(),
                               "evet: taban noktası nesneler seçildikten sonra sorulur")
                    .en("with_base"),
            },
        // NOT `SingleTransaction`: a copy changes nothing in the drawing, so it
        // leaves no undo step. It is a `File` command because what it does is
        // write a file (R43, and the same reason `KAYDET` is one).
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::ReadOnly | Flags::AiAccessible,
        .summary = "Seçili nesneleri çizimin kendi biçiminde panoya yazar.",
        .run     = &run_copy,
        .effect  = Effect::FileWrite,
    };
}

KENTOS_COMMAND(cut)
{
    return CommandSpec{
        .id       = "core.cut",
        .names    = {"KES", "CUT", "KS"},
        .title    = "Kes",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kesilecek nesneler; verilmezse seçim kullanılır"}
                    .en("objects"),
                Param::text("dosya", Arity::optional(),
                            "Panonun yazılacağı dosya; verilmezse ortak pano dosyası")
                    .en("file"),
                Param{"taban", ParamKind::Point, Arity::optional(),
                      "Yapıştırırken gösterilen yere gelecek taban noktası"}
                    .en("base_point"),
                Param::boolean("tabanli", Arity::optional(),
                               "evet: taban noktası nesneler seçildikten sonra sorulur")
                    .en("with_base"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı.",
        .run     = &run_cut,
        .effect  = Effect::DocumentEdit | Effect::FileWrite,
    };
}

KENTOS_COMMAND(paste)
{
    return CommandSpec{
        .id       = "core.paste",
        .names    = {"YAPIŞTIR", "YAPISTIR", "PASTE", "YP"},
        .title    = "Yapıştır",
        .category = Category::Modify,
        .params =
            {
                Param::points("nokta", Arity::optional(),
                              "Yapıştırılacak yerin sol alt köşesi; yerinde=evet ile gereksiz")
                    .en("point"),
                Param::boolean("yerinde", Arity::optional(),
                               "Kopyalandığı koordinatlara yapıştırır")
                    .en("in_place"),
                Param::text("dosya", Arity::optional(),
                            "Okunacak pano dosyası; verilmezse ortak pano dosyası")
                    .en("file"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Panodaki nesneleri çizime koyar; tek geri alma adımı.",
        .run     = &run_paste,
        .effect  = Effect::DocumentEdit | Effect::FileRead,
    };
}

} // namespace kentos::command
