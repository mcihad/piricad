// SPDX-License-Identifier: GPL-3.0-or-later
// core.xref — DIŞREFERANS: a drawing kept in its own file, shown in this one
// (TODOS C-14, model.md R45a).
//
// An external reference is a block definition whose members come from a file:
// a base map, a neighbouring sheet, a surveyor's field drawing kept by
// somebody else. It is drawn and snapped to through its references like any
// block and edited by nobody here — its file is where it changes, and a
// reload brings the change in. The project file holds its name and its path,
// never its objects, so a changed source is what the next open shows.
//
// Reading the file is /src/io's, through the file seam (`FileRequest`'s
// `XrefAttach`, `XrefLoad`, `XrefRepath`); what needs no file — unloading,
// binding, taking a reference off, listing — is done here, in the document.
#include "kentos_cad/command/block_edit.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <string>
#include <system_error>
#include <vector>

namespace kentos::command {
namespace {

core::Ratio ratio_of(double v)
{
    const auto num       = static_cast<std::int64_t>(std::llround(v * 1000000.0));
    std::int64_t den     = 1000000;
    const std::int64_t g = std::gcd(num < 0 ? -num : num, den);
    if (g > 1) return core::Ratio{num / g, den / g};
    return core::Ratio{num, den};
}

/// The external references on the drawing, by name, for a refusal that met a
/// name it does not know.
std::string references_known(const core::Document& doc)
{
    std::string known;
    for (core::BlockId b = 0; b < doc.blocks().size(); ++b)
        if (is_external_reference(doc, b))
            known += (known.empty() ? "" : ", ") + doc.blocks().at(b).name;
    return known.empty() ? "Çizimde dış referans yok; DIŞREFERANS dosya=<yol> ile bağlayın."
                         : "Dış referanslar: " + known + ".";
}

/// The external reference `ad=` names, refused by name when there is none.
Task<core::BlockId> named_reference(Context& ctx, const char* purpose)
{
    const Value named = ctx.argument("ad");
    if (named.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument, std::string("Hangi dış referans ") + purpose +
                                                         ": ad=<ad>. " +
                                                         references_known(ctx.document()));
        co_return core::kNoBlock;
    }
    const core::BlockId b = ctx.document().blocks().find(named.as_text());
    if (b == core::kNoBlock || !is_external_reference(ctx.document(), b)) {
        ctx.refuse(core::ErrorCode::NotFound, "'" + named.as_text() +
                                                  "' adında bir dış referans yok. " +
                                                  references_known(ctx.document()));
        co_return core::kNoBlock;
    }
    ctx.record("ad", Value::text(ctx.document().blocks().at(b).name));
    co_return b;
}

/// Asks the file engine to do `verb` for the reference named `block`.
Task<std::optional<std::string>> ask_files(Context& ctx, FileRequest::Verb verb, std::string block,
                                           std::string path, std::string* resolved = nullptr)
{
    Bus& bus = ctx.session().bus();
    if (!bus.on_file_request) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Dosya motoru bağlı değil; dış referans bu yapıda okunamıyor.");
        co_return std::nullopt;
    }
    FileRequest request;
    request.verb           = verb;
    request.tx             = &ctx.transaction();
    request.session        = &ctx.session();
    request.block          = std::move(block);
    request.path           = std::move(path);
    request.resolved_block = resolved;
    auto said              = co_await bus.on_file_request(request);
    if (!said) {
        ctx.refuse(said.error());
        co_return std::nullopt;
    }
    co_return said.value();
}

/// Sets the flags of `block` and keeps its path.
Status set_flags(Context& ctx, core::BlockId block, std::uint8_t flags)
{
    return ctx.transaction().set_block_external(block, ctx.document().blocks().at(block).path,
                                                flags);
}

// ---------------------------------------------------------------- attach ----

Task<void> attach(Context& ctx)
{
    const Value file = ctx.argument("dosya");
    if (file.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bağlanacak dosyayı verin: DIŞREFERANS dosya=<proje, DXF ya da DWG dosyası>.");
        co_return;
    }
    const Value named = ctx.argument("ad");
    std::string name;
    auto said =
        co_await ask_files(ctx, FileRequest::Verb::XrefAttach,
                           named.empty() ? std::string() : named.as_text(), file.as_text(), &name);
    if (!said) co_return;
    const core::BlockId block = ctx.document().blocks().find(name);
    if (block == core::kNoBlock) {
        ctx.refuse(core::ErrorCode::Internal, "Bağlanan dış referans tanımda bulunamadı.");
        co_return;
    }

    // WHERE IT STANDS: its own coordinates, unless told otherwise. A base map
    // or a neighbouring sheet is drawn in the drawing's system and goes where
    // it was drawn; `nokta=`, `olcek=` and `aci=` place it like a block.
    core::Point2 at{0, 0};
    if (const Value p = ctx.argument("nokta"); !p.empty()) at = p.as_point();
    double scale = 1.0;
    if (const Value s = ctx.argument("olcek"); !s.empty()) scale = s.as_number();
    if (scale == 0.0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Dış referansın ölçeği sıfır olamaz; aynalamak için eksi bir ölçek verin.");
        co_return;
    }
    double angle = 0.0;
    if (const Value a = ctx.argument("aci"); !a.empty()) angle = a.as_number();

    core::BlockReference ref;
    ref.block         = block;
    ref.sx            = ratio_of(scale);
    ref.sy            = ratio_of(scale);
    ref.rotation_udeg = static_cast<std::int64_t>(std::llround(angle * 1000000.0));
    auto placed       = place_reference(ctx, at, ref);
    if (!placed) {
        ctx.refuse(placed.error());
        co_return;
    }

    ctx.record("dosya", Value::text(file.as_text()));
    ctx.record("ad", Value::text(name));
    ctx.record("nokta", Value::point(at));
    if (scale != 1.0) ctx.record("olcek", Value::number(scale));
    if (angle != 0.0) ctx.record("aci", Value::number(angle));
    if (at == core::Point2{0, 0} && scale == 1.0 && angle == 0.0)
        ctx.echo(*said + " Dosyanın kendi koordinatlarında, yerinde çizildi.");
    else
        ctx.echo(*said + " Referans Sağa (Y) " + metres_text(at.x) + " m, Yukarı (X) " +
                 metres_text(at.y) + " m noktasında.");
}

// --------------------------------------------------------- reload / load ----

Task<void> reload(Context& ctx)
{
    std::string block;
    if (const Value named = ctx.argument("ad"); !named.empty()) {
        const core::BlockId b = co_await named_reference(ctx, "yenilenecek");
        if (b == core::kNoBlock) co_return;
        if ((ctx.document().blocks().at(b).flags & core::kBlockUnloaded) != 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "'" + named.as_text() +
                           "' boşaltılmış; yeniden görmek için DIŞREFERANS islem=yukle ad=" +
                           named.as_text() + ".");
            co_return;
        }
        block = ctx.document().blocks().at(b).name;
    }
    auto said = co_await ask_files(ctx, FileRequest::Verb::XrefLoad, block, {});
    if (!said) co_return;
    ctx.echo(*said);
}

Task<void> load(Context& ctx)
{
    const core::BlockId b = co_await named_reference(ctx, "yüklenecek");
    if (b == core::kNoBlock) co_return;
    const core::BlockDef def = ctx.document().blocks().at(b);
    if ((def.flags & core::kBlockUnloaded) == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + def.name +
                       "' zaten yüklü; dosyası değiştiyse DIŞREFERANS islem=yenile ad=" + def.name +
                       ".");
        co_return;
    }
    if (auto st = set_flags(ctx, b, static_cast<std::uint8_t>(def.flags & ~core::kBlockUnloaded));
        !st) {
        ctx.refuse(st.error());
        co_return;
    }
    auto said = co_await ask_files(ctx, FileRequest::Verb::XrefLoad, def.name, {});
    if (!said) co_return;
    ctx.echo(*said);
}

// ---------------------------------------------------------------- unload ----

Task<void> unload(Context& ctx)
{
    const core::BlockId b = co_await named_reference(ctx, "boşaltılacak");
    if (b == core::kNoBlock) co_return;
    const core::BlockDef def = ctx.document().blocks().at(b);
    if ((def.flags & core::kBlockUnloaded) != 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "'" + def.name + "' zaten boşaltılmış.");
        co_return;
    }
    auto gone = empty_external(ctx.transaction(), b);
    if (!gone) {
        ctx.refuse(gone.error());
        co_return;
    }
    if (auto st = set_flags(ctx, b, static_cast<std::uint8_t>(def.flags | core::kBlockUnloaded));
        !st) {
        ctx.refuse(st.error());
        co_return;
    }
    ctx.echo("'" + def.name + "' boşaltıldı: " + std::to_string(gone.value()) +
             " nesne çizimden çıktı; referansları yerinde, boş. Dosya açılışta da okunmaz; "
             "yeniden görmek için DIŞREFERANS islem=yukle ad=" +
             def.name + ".");
}

// ---------------------------------------------------------------- repath ----

Task<void> repath(Context& ctx)
{
    const core::BlockId b = co_await named_reference(ctx, "yeni dosyaya bağlanacak");
    if (b == core::kNoBlock) co_return;
    const Value file = ctx.argument("dosya");
    if (file.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Dış referansın yeni dosyasını verin: dosya=<yol>.");
        co_return;
    }
    auto said = co_await ask_files(ctx, FileRequest::Verb::XrefRepath,
                                   ctx.document().blocks().at(b).name, file.as_text());
    if (!said) co_return;
    ctx.record("dosya", Value::text(file.as_text()));
    ctx.echo(*said);
}

// ------------------------------------------------------------------ bind ----

Task<void> bind(Context& ctx)
{
    const core::BlockId b = co_await named_reference(ctx, "çizime bağlanacak");
    if (b == core::kNoBlock) co_return;
    const core::BlockDef def = ctx.document().blocks().at(b);
    if ((def.flags & core::kBlockUnloaded) != 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + def.name +
                       "' boşaltılmış; bağlanacak bir şey yok. Önce DIŞREFERANS islem=yukle ad=" +
                       def.name + ".");
        co_return;
    }
    // WHAT IT HOLDS BECOMES THE DRAWING'S: the definition and the blocks its
    // file defines lose their tie to the file, and their members are written
    // with the drawing from now on. The names stay — `ALTLIK|KAPAK` is where
    // that block came from, and saying so is worth more than a tidier name.
    std::vector<core::BlockId> all = external_dependents(ctx.document(), b);
    all.push_back(b);
    std::size_t members = 0;
    for (const core::BlockId d : all) {
        for (const core::EntityKey k : ctx.document().blocks().at(d).members)
            if (const core::EntityId m = ctx.document().slot_of(k);
                m != core::kNoEntity && ctx.document().alive(m))
                ++members;
        if (auto st = ctx.transaction().set_block_external(d, {}, 0); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }
    ctx.echo("'" + def.name + "' çizime bağlandı: artık sıradan bir blok; " +
             std::to_string(members) +
             " nesnesi bu çizimle birlikte kaydedilir, dosyası değişse de değişmez.");
}

// ---------------------------------------------------------------- detach ----

Task<void> detach(Context& ctx)
{
    const core::BlockId b = co_await named_reference(ctx, "kaldırılacak");
    if (b == core::kNoBlock) co_return;
    const core::Document& doc = ctx.document();
    const core::BlockDef def  = doc.blocks().at(b);

    // A REFERENCE INSIDE ANOTHER DEFINITION cannot be erased from here: that
    // block would lose part of its picture without anyone having asked.
    const std::vector<core::BlockId> own = external_dependents(doc, b);
    for (core::BlockId other = 0; other < doc.blocks().size(); ++other) {
        if (other == b || std::ranges::find(own, other) != own.end()) continue;
        const std::vector<core::BlockId>& uses = doc.blocks().at(other).uses;
        if (std::ranges::find(uses, b) == uses.end()) continue;
        bool drawn = false;
        for (const core::EntityKey k : doc.blocks().at(other).members) {
            const core::EntityId m = doc.slot_of(k);
            if (m == core::kNoEntity || !doc.alive(m) ||
                doc.entities().kind[m] != core::kBlockReferenceKind)
                continue;
            auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[m]);
            drawn    = drawn || (ref && ref.value().block == b);
        }
        if (drawn) {
            ctx.refuse(core::ErrorCode::ValidationFailed,
                       "'" + def.name + "' '" + doc.blocks().at(other).name +
                           "' bloğunun içinde kullanılıyor; önce o bloktan çıkarın "
                           "(BLOKDÜZENLE), sonra kaldırın.");
            co_return;
        }
    }

    const std::vector<core::EntityId> references = sheet_references(doc, b);
    for (const core::EntityId e : references)
        if (auto st = ctx.transaction().erase_entity(e); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    auto gone = empty_external(ctx.transaction(), b);
    if (!gone) {
        ctx.refuse(gone.error());
        co_return;
    }
    if (auto st = set_flags(ctx, b,
                            static_cast<std::uint8_t>(core::kBlockExternal | core::kBlockDetached));
        !st) {
        ctx.refuse(st.error());
        co_return;
    }
    ctx.echo("'" + def.name + "' dış referansı kaldırıldı: " + std::to_string(references.size()) +
             " referans silindi. Dosyasına dokunulmadı.");
}

// ------------------------------------------------------------------ list ----

Task<void> list(Context& ctx)
{
    const core::Document& doc = ctx.document();
    core::Json rows           = core::Json::array({});
    std::string said;
    for (core::BlockId b = 0; b < doc.blocks().size(); ++b) {
        if (!is_external_reference(doc, b)) continue;
        const core::BlockDef& def = doc.blocks().at(b);
        std::size_t members       = 0;
        for (const core::EntityKey k : def.members)
            if (const core::EntityId m = doc.slot_of(k); m != core::kNoEntity && doc.alive(m))
                ++members;
        std::error_code ec;
        const bool exists = !def.path.empty() && std::filesystem::is_regular_file(def.path, ec);
        const char* state = "yüklü";
        if ((def.flags & core::kBlockUnloaded) != 0)
            state = "boşaltıldı";
        else if (!exists)
            state = "bulunamadı";
        else if (members == 0)
            state = "boş";
        const std::size_t placed = sheet_references(doc, b).size();
        said += "\n  " + def.name + "  —  " + state + ", " + std::to_string(members) + " nesne, " +
                std::to_string(placed) + " referans  —  " + def.path;
        core::Json row = core::Json::object({});
        row.set("ad", core::Json::string(def.name));
        row.set("dosya", core::Json::string(def.path));
        row.set("durum", core::Json::string(state));
        row.set("nesne", core::Json::integer(static_cast<std::int64_t>(members)));
        row.set("referans", core::Json::integer(static_cast<std::int64_t>(placed)));
        rows.push(std::move(row));
    }
    core::Json report = core::Json::object({});
    report.set("dis_referanslar", std::move(rows));
    ctx.report(std::move(report));
    if (said.empty()) {
        ctx.echo("Çizimde dış referans yok. DIŞREFERANS dosya=<yol> bir proje, DXF ya da DWG "
                 "dosyasını bağlar.");
        co_return;
    }
    ctx.echo("Dış referanslar:" + said);
}

Task<void> run_xref(Context& ctx)
{
    std::string op = "ekle";
    if (const Value v = ctx.argument("islem"); !v.empty()) op = v.as_text();
    if (op != "ekle") ctx.record("islem", Value::text(op));
    if (op == "ekle")
        co_await attach(ctx);
    else if (op == "yenile")
        co_await reload(ctx);
    else if (op == "yukle")
        co_await load(ctx);
    else if (op == "bosalt")
        co_await unload(ctx);
    else if (op == "yol")
        co_await repath(ctx);
    else if (op == "bagla")
        co_await bind(ctx);
    else if (op == "kaldir")
        co_await detach(ctx);
    else
        co_await list(ctx);
}

} // namespace

KENTOS_COMMAND(xref)
{
    return CommandSpec{
        .id       = "core.xref",
        .names    = {"DIŞREFERANS", "DISREFERANS", "XREF", "DRF"},
        .title    = "Dış Referans",
        .category = Category::File,
        .params =
            {
                Param::choice(
                    "islem", Arity::optional(),
                    {"ekle", "yenile", "yukle", "bosalt", "yol", "bagla", "kaldir", "listele"},
                    "ekle: dosyayı bağlar ve bir referans koyar (varsayılan); yenile: "
                    "dosyayı yeniden okur; bosalt: çizimden çıkarır, referansı kalır; "
                    "yukle: boşaltılanı geri getirir; yol: yeni dosyasını gösterir; "
                    "bagla: çizime katar, sıradan blok olur; kaldir: referanslarıyla "
                    "siler; listele: bağlı olanları sayar")
                    .en("action"),
                Param::text("dosya", Arity::optional(),
                            "ekle ve yol için dosya: proje, DXF ya da DWG; göreli yol proje "
                            "dosyasının klasörüne göre okunur")
                    .en("file"),
                Param::text("ad", Arity::optional(),
                            "Dış referansın adı; ekle'de verilmezse dosyanın adı")
                    .en("name"),
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "ekle için referansın konduğu nokta; varsayılan başlangıç noktası (0,0): "
                      "dosya kendi koordinatında, yerinde çizilir"}
                    .en("point"),
                Param::number("olcek", Arity::optional(), "ekle için ölçek; varsayılan 1")
                    .en("scale"),
                Param::number("aci", Arity::optional(),
                              "ekle için dönme açısı, derece; varsayılan 0")
                    .en("angle"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir proje, DXF ya da DWG dosyasını çizime dış referans olarak bağlar: yerinde "
                   "çizilir, yakalanır, düzenlenmez; dosyası değişince yenilenir, kendisi çizime "
                   "yazılmaz.",
        .run = &run_xref,
    };
}

} // namespace kentos::command
