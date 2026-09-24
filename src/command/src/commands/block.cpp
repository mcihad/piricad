// SPDX-License-Identifier: GPL-3.0-or-later
// core.block — BLOK, and core.insert — BLOKEKLE.
//
// BLOK turns the selected entities into a named DEFINITION and puts one
// reference where they were: the entities are copied into the definition, kind,
// payload, style, caption and attributes intact, flagged in-block (model.md
// R45), the originals are erased, and a reference at the base point draws them
// exactly where they stood. Undo brings the originals back; the definition
// stays, inert, as a layer would (R45: append-only).
//
// BLOKEKLE places a definition: at a point, scaled, turned, mirrored by a
// negative scale, and as a grid. The transform is stored as rationals and
// micro-degrees (core/block_reference.hpp); the drawn form is the members,
// placed by exact integer arithmetic.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"

#include <cmath>
#include <numeric>
#include <string>
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

/// Places one reference entity with `ref` at `at`, bounds computed here.
Result<core::EntityId> place(Context& ctx, core::Point2 at, core::BlockReference ref)
{
    ref.bounds = core::block_reference_bounds(ctx.document(), at, ref);
    const core::Point2 pts[1]{at};
    const core::RingGeometry::RingInput ring{std::span<const core::Point2>(pts, 1),
                                             core::RingRole::Open, 0};
    const std::vector<std::uint8_t> payload = core::encode_block_reference(ref);
    return ctx.transaction().add_kind(ctx.active_layer(), core::kBlockReferenceKind,
                                      std::span<const core::RingGeometry::RingInput>(&ring, 1),
                                      payload);
}

// ------------------------------------------------------------------- BLOK ----

Task<void> run_block(Context& ctx)
{
    auto name = co_await ctx.text("ad", "Bloğun adı");
    if (!name || name->empty()) co_return;
    if (ctx.document().blocks().find(*name) != core::kNoBlock) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + *name + "' adında bir blok zaten var; blok adları benzersizdir.");
        co_return;
    }

    auto base = co_await ctx.point("taban", "Bloğun taban noktası");
    if (!base) co_return;

    std::vector<std::int64_t> requested;
    if (!co_await want_objects(ctx, "nesneler", "Bloğa girecek nesneleri seçin, sonra Enter",
                               requested, 0, "BLOK ad=KAPAK taban=0,0 nesneler=1"))
        co_return;

    std::vector<core::EntityId> slots;
    for (const std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Geçersiz nesne kimliği: " + std::to_string(raw) +
                           ". Kimlikler 1'den başlar.");
            co_return;
        }
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = ctx.document().slot_of(key);
        if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        if (auto st = ctx.document().editable(slot); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        slots.push_back(slot);
    }

    std::string description;
    if (const Value d = ctx.argument("aciklama"); !d.empty()) description = d.as_text();

    auto block = ctx.transaction().add_block(*name, description, *base);
    if (!block) {
        ctx.refuse(block.error());
        co_return;
    }

    // Each entity is copied into the definition as it is — kind, rings and
    // payload through the general mutator — with its style, caption and cells,
    // and the original is erased. Undo restores the originals; the copies go
    // dark with the transaction's rollback like any other add.
    const core::Document& doc = ctx.document();
    for (const core::EntityId e : slots) {
        const std::uint32_t gslot = doc.entities().slot[e];
        const core::RingSpan span = doc.geometry().rings_of(gslot);
        std::vector<std::vector<core::Point2>> store;
        std::vector<core::RingGeometry::RingInput> rings;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = doc.geometry().ring_xs(r);
            const auto ys = doc.geometry().ring_ys(r);
            std::vector<core::Point2> pts;
            for (std::size_t v = 0; v < xs.size(); ++v)
                pts.push_back(core::Point2{xs[v], ys[v]});
            store.push_back(std::move(pts));
            rings.push_back(core::RingGeometry::RingInput{
                {}, doc.geometry().ring_role[r], doc.geometry().ring_part[r]});
        }
        for (std::size_t i = 0; i < rings.size(); ++i)
            rings[i].points = std::span<const core::Point2>(store[i]);
        const std::vector<std::uint8_t> payload(doc.geometry().payload_of(gslot).begin(),
                                                doc.geometry().payload_of(gslot).end());

        auto member = ctx.transaction().add_kind(doc.entities().layer[e], doc.entities().kind[e],
                                                 rings, payload, block.value());
        if (!member) {
            ctx.refuse(member.error());
            co_return;
        }
        const core::EntityId m = member.value();
        if (const core::StyleId st = doc.entities().style[e]; st != core::kByLayerStyle)
            if (auto s = ctx.transaction().set_entity_style(m, st); !s) {
                ctx.refuse(s.error());
                co_return;
            }
        if (doc.texts().has(gslot))
            if (auto s = ctx.transaction().set_text(
                    m, std::string(doc.texts().text(gslot)), doc.texts().height(gslot),
                    doc.texts().anchor(gslot), doc.texts().lines(gslot));
                !s) {
                ctx.refuse(s.error());
                co_return;
            }
        const core::AttrTable& attrs = doc.attributes();
        for (std::size_t c = 0; c < attrs.columns(); ++c) {
            auto cell = attrs.get(static_cast<core::AttrId>(c), gslot);
            if (!cell || !cell.value().present) continue;
            if (auto s =
                    ctx.transaction().set_attribute(static_cast<core::AttrId>(c), m, cell.value());
                !s) {
                ctx.refuse(s.error());
                co_return;
            }
        }
        if (auto s = ctx.transaction().erase_entity(e); !s) {
            ctx.refuse(s.error());
            co_return;
        }
    }

    core::BlockReference ref;
    ref.block   = block.value();
    auto placed = place(ctx, *base, ref);
    if (!placed) {
        ctx.refuse(placed.error());
        co_return;
    }

    ctx.record("ad", Value::text(*name));
    ctx.record("taban", Value::point(*base));
    ctx.record("nesneler", Value::ids(requested));
    if (!description.empty()) ctx.record("aciklama", Value::text(description));
    ctx.echo("'" + *name + "' bloğu " + std::to_string(slots.size()) +
             " nesneyle tanımlandı ve yerine bir referans kondu.");
}

// --------------------------------------------------------------- BLOKEKLE ----

Task<void> run_insert(Context& ctx)
{
    // THE BLOCKS THIS DRAWING HAS, offered rather than remembered. Pressing the
    // Blok Ekle button used to ask for a name and wait: a question only somebody
    // who already knew the answer could give, with the list one refused attempt
    // away in the error message below.
    std::vector<std::string> defined;
    for (const core::BlockDef& d : ctx.document().blocks().all())
        defined.push_back(d.name);

    // NOTHING TO PLACE IS SAID FIRST, not after a name is typed. With no block in
    // the drawing this sat at "Yerleştirilecek bloğun adı" with an empty list of
    // choices — a question with no possible answer — and the refusal that
    // explained what a block IS came only after the user guessed a name. A user
    // who has never defined one reads that prompt as a tool that does nothing,
    // and said so — they could not tell what it was for.
    if (defined.empty()) {
        ctx.refuse(
            core::ErrorCode::NotFound,
            "Bu çizimde tanımlı blok yok, yerleştirilecek bir şey de yok. Blok, bir kez "
            "çizilip çok kez yerleştirilen bir semboldür (rögar kapağı, direk, ağaç): önce "
            "nesneleri seçip BLOK ad=<isim> ile tanımlayın, sonra BLOKEKLE onları istediğiniz "
            "her yere koyar.");
        co_return;
    }

    auto name = co_await ctx.text("ad", "Yerleştirilecek bloğun adı", defined);
    if (!name || name->empty()) co_return;
    const core::BlockId block = ctx.document().blocks().find(*name);
    if (block == core::kNoBlock) {
        std::string known;
        for (const core::BlockDef& d : ctx.document().blocks().all())
            known += (known.empty() ? "" : ", ") + d.name;
        ctx.refuse(core::ErrorCode::NotFound,
                   "'" + *name + "' adında blok yok." +
                       (known.empty() ? " Çizimde tanımlı blok yok; önce BLOK ile tanımlayın."
                                      : " Tanımlı bloklar: " + known + "."));
        co_return;
    }

    double sx = 1.0;
    if (const Value s = ctx.argument("olcek"); !s.empty()) sx = s.as_number();
    double sy = sx;
    if (const Value s = ctx.argument("olcek_y"); !s.empty()) sy = s.as_number();
    if (sx == 0.0 || sy == 0.0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Blok ölçeği sıfır olamaz; aynalamak için eksi bir ölçek verin.");
        co_return;
    }
    double angle_deg = 0.0;
    if (const Value a = ctx.argument("aci"); !a.empty()) angle_deg = a.as_number();

    std::int64_t columns = 1;
    std::int64_t rows    = 1;
    if (const Value c = ctx.argument("sutun"); !c.empty()) columns = c.as_int();
    if (const Value r = ctx.argument("satir"); !r.empty()) rows = r.as_int();
    if (columns < 1 || rows < 1 || columns > 65535 || rows > 65535) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Dizi en az 1×1, en çok 65535×65535 olabilir.");
        co_return;
    }
    core::Mm column_spacing = 0;
    core::Mm row_spacing    = 0;
    if (const Value c = ctx.argument("sutun_aralik"); !c.empty()) column_spacing = c.as_int();
    if (const Value r = ctx.argument("satir_aralik"); !r.empty()) row_spacing = r.as_int();
    if ((columns > 1 && column_spacing == 0) || (rows > 1 && row_spacing == 0)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Birden çok sütun ya da satır için aralık (milimetre) verin: sutun_aralik= ve "
                   "satir_aralik=.");
        co_return;
    }

    core::BlockReference ref;
    ref.block          = block;
    ref.sx             = ratio_of(sx);
    ref.sy             = ratio_of(sy);
    ref.rotation_udeg  = static_cast<std::int64_t>(std::llround(angle_deg * 1000000.0));
    ref.columns        = static_cast<std::uint16_t>(columns);
    ref.rows           = static_cast<std::uint16_t>(rows);
    ref.column_spacing = column_spacing;
    ref.row_spacing    = row_spacing;

    // The reference, drawn under the cursor before it is placed: the canvas
    // expands the definition with the same scale, turn and grid.
    auto at = co_await ctx.point("nokta", "Ekleme noktası",
                                 PointOptions{.rubber_band    = true,
                                              .rubber_shape   = RubberShape::Block,
                                              .rubber_payload = core::encode_block_reference(ref)});
    if (!at) co_return;

    auto placed = place(ctx, *at, ref);
    if (!placed) {
        ctx.refuse(placed.error());
        co_return;
    }

    ctx.record("ad", Value::text(*name));
    ctx.record("nokta", Value::point(*at));
    ctx.record("olcek", Value::number(sx));
    if (sy != sx) ctx.record("olcek_y", Value::number(sy));
    ctx.record("aci", Value::number(angle_deg));
    if (columns > 1 || rows > 1) {
        ctx.record("sutun", Value::integer(columns));
        ctx.record("satir", Value::integer(rows));
        ctx.record("sutun_aralik", Value::integer(column_spacing));
        ctx.record("satir_aralik", Value::integer(row_spacing));
    }
    ctx.echo("'" + *name + "' bloğu yerleştirildi" +
             (columns > 1 || rows > 1
                  ? " (" + std::to_string(columns) + "×" + std::to_string(rows) + " dizi)."
                  : "."));
}

} // namespace

KENTOS_COMMAND(block)
{
    return CommandSpec{
        .id       = "core.block",
        .names    = {"BLOK", "BLOK", "BLOCK", "BLK"},
        .title    = "Blok Tanımla",
        .category = Category::Draw,
        .params =
            {
                Param::text("ad", Arity::exactly(1),
                            "Bloğun adı; Türkçe katlanmış hâliyle benzersiz")
                    .en("name"),
                Param::point("taban", "Taban noktası: referansların yerleştirildiği nokta")
                    .en("base"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Bloğa girecek nesneler; yoksa etkin seçim"}
                    .en("objects"),
                Param::text("aciklama", Arity::optional(), "Serbest açıklama").en("note"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.",
        .run     = &run_block,
    };
}

KENTOS_COMMAND(insert)
{
    return CommandSpec{
        .id       = "core.insert",
        .names    = {"BLOKEKLE", "BLOKEKLE", "INSERT", "BE"},
        .title    = "Blok Ekle",
        .category = Category::Draw,
        .params =
            {
                Param::text("ad", Arity::exactly(1), "Yerleştirilecek bloğun adı").en("name"),
                Param::point("nokta", "Ekleme noktası").en("point"),
                Param::number("olcek", Arity::optional(),
                              "Ölçek; eksi değer x'te aynalar; varsayılan 1")
                    .en("scale"),
                Param::number("olcek_y", Arity::optional(), "Y ölçeği, farklıysa; varsayılan olcek")
                    .en("scale_y"),
                Param::number("aci", Arity::optional(), "Dönme açısı, derece; varsayılan 0")
                    .en("angle"),
                Param::integer("sutun", Arity::optional(), "Dizi sütun sayısı; varsayılan 1")
                    .en("columns"),
                Param::integer("satir", Arity::optional(), "Dizi satır sayısı; varsayılan 1")
                    .en("rows"),
                Param::integer("sutun_aralik", Arity::optional(),
                               "Sütunlar arası, milimetre, döndürülmüş eksende")
                    .en("column_spacing"),
                Param::integer("satir_aralik", Arity::optional(),
                               "Satırlar arası, milimetre, döndürülmüş eksende")
                    .en("row_spacing"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.",
        .run     = &run_insert,
    };
}

} // namespace kentos::command
