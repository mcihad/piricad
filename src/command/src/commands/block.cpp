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
#include "kentos_cad/command/block_edit.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
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

/// The sentence that lists the drawing's blocks, for a refusal that met a name
/// it does not know.
std::string blocks_known(const core::Document& doc)
{
    std::string known;
    for (const core::BlockDef& d : doc.blocks().all())
        known += (known.empty() ? "" : ", ") + d.name;
    return known.empty() ? "Çizimde tanımlı blok yok; önce BLOK ile tanımlayın."
                         : "Tanımlı bloklar: " + known + ".";
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
    // payload through the general mutator — with its style, caption and cells
    // (`copy_into_block`, BLOKDÜZENLE's copy too), and the original is erased.
    // Undo restores the originals; the copies go dark with the transaction's
    // rollback like any other add.
    for (const core::EntityId e : slots) {
        if (auto member = copy_into_block(ctx, e, block.value()); !member) {
            ctx.refuse(member.error());
            co_return;
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
        ctx.refuse(core::ErrorCode::NotFound,
                   "'" + *name + "' adında blok yok. " + blocks_known(ctx.document()));
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

// ------------------------------------------------------------ BLOKDÜZENLE ----
//
// A DEFINITION EDITED ON THE SHEET, in three steps that are each one command,
// one transaction and one undo step (TODOS C-13):
//
//   aç      the members come out as ordinary drawing objects — their own kind,
//           layer, look and caption, exactly as the definition holds them —
//           moved to the reference's insertion point, upright and full size;
//           the reference steps aside (hidden) while its definition is out;
//   kaydet  the definition is rebuilt from the objects handed back: an object
//           that is a member unchanged keeps that member as it was, anything
//           else becomes a member, a member nothing came back for leaves, and
//           every reference that draws the block has its box brought up to
//           date — so the new picture is everywhere at once;
//   vazgeç  the objects are removed and the reference comes back.
//
// WHY STATELESS. Which objects belong to the edit is the client's to say —
// the shell tracks what it opened and what was drawn since — so every step is
// complete as data, journals as data and replays alone (Article 1.4). Nothing
// is remembered between steps that a file, an undo or a replay could lose.
//
// WHY UPRIGHT AND FULL SIZE. The objects are the definition, not one
// reference's picture of it: opened in a turned or scaled reference's frame,
// saving would have to undo that frame, and a turn by an angle that is not a
// quarter does not come back to the millimetre. A move is integers added, so
// opening and saving a block without touching it leaves it byte for byte what
// it was. A reference placed at scale 1, unturned, is edited exactly in place.

/// The block and frame a BLOKDÜZENLE step works on: the definition, the
/// reference it was opened from (or none, opened by name), and the move from
/// the definition's own place to where it is opened.
struct EditTarget
{
    core::BlockId block{core::kNoBlock};
    core::EntityId reference{core::kNoEntity};
    std::int64_t reference_key{0};
    core::Mm dx{0};
    core::Mm dy{0};
};

/// The target named by `ad=` or `nesne=`; `ask` lets `nesne` come from the
/// selection or the pointer, which only opening may — a reference being edited
/// is hidden and cannot be pointed at.
Task<bool> edit_target(Context& ctx, bool ask, EditTarget& out)
{
    const core::Document& doc = ctx.document();
    if (const Value named = ctx.argument("ad"); !named.empty()) {
        out.block = doc.blocks().find(named.as_text());
        if (out.block == core::kNoBlock) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "'" + named.as_text() + "' adında bir blok yok. " + blocks_known(doc));
            co_return false;
        }
        ctx.record("ad", Value::text(doc.blocks().at(out.block).name));
        co_return true;
    }

    std::vector<std::int64_t> picked;
    if (ask) {
        if (!co_await want_objects(ctx, "nesne", "Düzenlenecek blok referansını seçin", picked, 1,
                                   "BLOKDÜZENLE nesne=12", core::kBlockReferenceKind))
            co_return false;
    } else {
        const Value arg = ctx.argument("nesne");
        picked          = arg.as_ids();
        if (picked.empty() && arg.kind() == Value::Kind::Int) picked.push_back(arg.as_int());
        if (picked.size() != 1) {
            ctx.refuse(
                core::ErrorCode::InvalidArgument,
                "Hangi bloğun düzenlendiğini söyleyin: nesne=<açılan referans> ya da "
                "ad=<blok>.\n  Örnek: BLOKDÜZENLE islem=kaydet nesne=12 nesneler=40 nesneler=41");
            co_return false;
        }
    }
    const std::int64_t raw = picked.front();
    const core::EntityId e =
        raw > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)))
                : core::kNoEntity;
    if (e == core::kNoEntity || !doc.alive(e) ||
        doc.entities().kind[e] != core::kBlockReferenceKind ||
        (doc.entities().flags[e] & core::FlagInBlock) != 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nesne " + std::to_string(raw) + " çizimdeki bir blok referansı değil.");
        co_return false;
    }
    const std::uint32_t gslot = doc.entities().slot[e];
    auto ref                  = core::block_reference_of(doc.geometry(), gslot);
    if (!ref || ref.value().block >= doc.blocks().size()) {
        ctx.refuse(core::ErrorCode::NotFound, "Referansın blok tanımı bulunamadı.");
        co_return false;
    }
    out.block               = ref.value().block;
    out.reference           = e;
    out.reference_key       = raw;
    const core::Point2 at   = core::block_reference_insertion(doc.geometry(), gslot);
    const core::Point2 base = doc.blocks().at(out.block).base;
    out.dx                  = at.x - base.x;
    out.dy                  = at.y - base.y;
    ctx.record("nesne", Value::ids({raw}));
    co_return true;
}

/// The drawing objects handed to `kaydet` and `vazgeç`, as slots.
Task<bool> edit_objects(Context& ctx, const EditTarget& target, std::vector<core::EntityId>& out)
{
    std::vector<std::int64_t> ids;
    if (!co_await want_objects(ctx, "nesneler", "Bloğun düzenlenen nesnelerini seçin, sonra Enter",
                               ids, 0, "BLOKDÜZENLE islem=kaydet nesne=12 nesneler=40 nesneler=41"))
        co_return false;
    const core::Document& doc = ctx.document();
    for (const std::int64_t raw : ids) {
        const core::EntityId e =
            raw > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)))
                    : core::kNoEntity;
        if (e == core::kNoEntity || !doc.alive(e) ||
            (doc.entities().flags[e] & core::FlagInBlock) != 0) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı, silinmiş ya da bir blok tanımının içinde: " +
                           std::to_string(raw));
            co_return false;
        }
        if (e == target.reference) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Düzenlenen referansın kendisi bloğun nesnelerinden biri olamaz.");
            co_return false;
        }
        if (std::ranges::find(out, e) == out.end()) out.push_back(e);
    }
    ctx.record("nesneler", Value::ids(ids));
    co_return true;
}

/// The keys of `slots`, for the report.
core::Json keys_json(const core::Document& doc, const std::vector<core::EntityId>& slots)
{
    core::Json out = core::Json::array({});
    for (const core::EntityId e : slots)
        out.push(core::Json::integer(static_cast<std::int64_t>(core::raw(doc.key_of(e)))));
    return out;
}

Task<void> run_block_edit(Context& ctx)
{
    std::string step = "ac";
    if (const Value v = ctx.argument("islem"); !v.empty()) step = v.as_text();
    const bool opening = core::turkish_key_equals(step, "ac");
    const bool saving  = core::turkish_key_equals(step, "kaydet");
    // The word as the spec spells it, whatever folding the caller typed.
    std::string word = "vazgec";
    if (opening) word = "ac";
    if (saving) word = "kaydet";
    ctx.record("islem", Value::text(word));

    EditTarget target;
    if (!co_await edit_target(ctx, opening, target)) co_return;
    const core::Document& doc = ctx.document();
    const std::string name    = doc.blocks().at(target.block).name;
    const bool from_reference = target.reference != core::kNoEntity;
    const bool reference_aside =
        from_reference && (doc.entities().flags[target.reference] & core::FlagHidden) != 0;
    core::Json report;
    report.set("islem", core::Json::string(word));
    report.set("blok", core::Json::string(name));
    if (from_reference) report.set("referans", core::Json::integer(target.reference_key));

    if (opening) {
        if (from_reference) {
            if (reference_aside) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Referans " + std::to_string(target.reference_key) +
                               " zaten düzenlemeye açık (gizli). Bitirmek için BLOKDÜZENLE "
                               "islem=kaydet ya da islem=vazgec.");
                co_return;
            }
            if (auto st = doc.editable(target.reference); !st) {
                ctx.refuse(st.error());
                co_return;
            }
        }
        // Copied, not referred to: the objects are added under it.
        const std::vector<core::EntityKey> members = doc.blocks().at(target.block).members;
        std::vector<core::EntityId> opened;
        for (const core::EntityKey key : members) {
            const core::EntityId m = doc.slot_of(key);
            if (m == core::kNoEntity || !doc.alive(m)) continue;
            auto made = copy_out_of_block(ctx, m, target.dx, target.dy);
            if (!made) {
                ctx.refuse(made.error().code,
                           "Blok '" + name + "' açılamadı: " + made.error().message);
                co_return;
            }
            opened.push_back(made.value());
        }
        if (from_reference) {
            if (auto st = ctx.transaction().set_entity_hidden(target.reference, true); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            // AND OUT OF THE SELECTION: a hidden reference left selected is one
            // SİL away from being deleted unseen while its definition is out.
            Bus& bus = ctx.session().bus();
            if (bus.selection().remove(doc.key_of(target.reference)) && bus.on_selection_changed)
                bus.on_selection_changed();
        }
        report.set("oteleme", core::Json::array({core::Json::integer(target.dx),
                                                 core::Json::integer(target.dy)}));
        report.set("parcalar", keys_json(doc, opened));
        ctx.report(std::move(report));

        std::string said =
            "'" + name + "' bloğu düzenlemeye açıldı: " + std::to_string(opened.size()) + " nesne";
        if (!from_reference) {
            said += " tanımın kendi yerinde.";
        } else {
            auto ref =
                core::block_reference_of(doc.geometry(), doc.entities().slot[target.reference]);
            const bool plain = ref && ref.value().sx == core::Ratio{1, 1} &&
                               ref.value().sy == core::Ratio{1, 1} &&
                               ref.value().rotation_udeg == 0;
            said += plain ? " referansın yerinde; referans düzenleme bitene dek gizli."
                          : " referansın ekleme noktasında, kendi yönünde ve 1:1 (referans "
                            "dönük ya da ölçekli); referans düzenleme bitene dek gizli.";
        }
        ctx.echo(said + " Bitirince BLOKDÜZENLE islem=kaydet (ya da vazgec).");
        co_return;
    }

    if (from_reference && !reference_aside) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Referans " + std::to_string(target.reference_key) +
                       " düzenlemeye açık değil; önce BLOKDÜZENLE nesne=" +
                       std::to_string(target.reference_key) + " ile açın.");
        co_return;
    }
    std::vector<core::EntityId> objects;
    if (!co_await edit_objects(ctx, target, objects)) co_return;

    if (!saving) {
        for (const core::EntityId e : objects)
            if (auto st = ctx.transaction().erase_entity(e); !st) {
                ctx.refuse(st.error());
                co_return;
            }
        if (from_reference)
            if (auto st = ctx.transaction().set_entity_hidden(target.reference, false); !st) {
                ctx.refuse(st.error());
                co_return;
            }
        report.set("silinen", core::Json::integer(static_cast<std::int64_t>(objects.size())));
        ctx.report(std::move(report));
        ctx.echo("'" + name + "' bloğunun düzenlenmesinden vazgeçildi: " +
                 std::to_string(objects.size()) + " nesne kaldırıldı, tanım değişmedi.");
        co_return;
    }

    if (objects.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kaydedilecek nesne yok: bir blok boş kalamaz. Açılanı geri almak için "
                   "BLOKDÜZENLE islem=vazgec.");
        co_return;
    }

    // THE DEFINITION REBUILT FROM WHAT CAME BACK. An object that is a member
    // unchanged keeps that member — same key, same place in the draw order —
    // and anything else is written in, moved back to the definition's place.
    std::vector<core::EntityId> members;
    for (const core::EntityKey key : doc.blocks().at(target.block).members)
        if (const core::EntityId m = doc.slot_of(key); m != core::kNoEntity && doc.alive(m))
            members.push_back(m);
    std::vector<bool> kept(members.size(), false);
    std::size_t unchanged = 0;
    std::vector<core::EntityId> written;
    for (const core::EntityId e : objects) {
        std::size_t match = members.size();
        for (std::size_t i = 0; i < members.size() && match == members.size(); ++i)
            if (!kept[i] && same_as_member(doc, e, -target.dx, -target.dy, members[i])) match = i;
        if (match < members.size()) {
            kept[match] = true;
            ++unchanged;
        } else {
            auto made = copy_into_block(ctx, e, target.block, -target.dx, -target.dy);
            if (!made) {
                ctx.refuse(made.error().code,
                           "Blok '" + name + "' kaydedilemedi: " + made.error().message);
                co_return;
            }
            written.push_back(made.value());
        }
        if (auto st = ctx.transaction().erase_entity(e); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }
    std::size_t removed = 0;
    for (std::size_t i = 0; i < members.size(); ++i) {
        if (kept[i]) continue;
        if (auto st = ctx.transaction().erase_member(members[i]); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        ++removed;
    }
    if (from_reference)
        if (auto st = ctx.transaction().set_entity_hidden(target.reference, false); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    auto boxes = refresh_references(ctx, target.block);
    if (!boxes) {
        ctx.refuse(boxes.error());
        co_return;
    }
    std::size_t references = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().kind[e] != core::kBlockReferenceKind) continue;
        auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[e]);
        if (ref && ref.value().block == target.block) ++references;
    }

    report.set("korunan", core::Json::integer(static_cast<std::int64_t>(unchanged)));
    report.set("yazilan", core::Json::integer(static_cast<std::int64_t>(written.size())));
    report.set("cikarilan", core::Json::integer(static_cast<std::int64_t>(removed)));
    report.set("referans_sayisi", core::Json::integer(static_cast<std::int64_t>(references)));
    report.set("kutusu_yenilenen", core::Json::integer(static_cast<std::int64_t>(boxes.value())));
    ctx.report(std::move(report));
    ctx.echo("'" + name + "' bloğu kaydedildi: " + std::to_string(unchanged) +
             " üye olduğu gibi kaldı, " + std::to_string(written.size()) + " üye yazıldı, " +
             std::to_string(removed) + " üye çıkarıldı. Bloğun " + std::to_string(references) +
             " referansı yeni biçimi çiziyor.");
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

KENTOS_COMMAND(block_edit)
{
    return CommandSpec{
        .id       = "core.block_edit",
        .names    = {"BLOKDÜZENLE", "BLOKDUZENLE", "BEDIT", "BDZ"},
        .title    = "Bloğu Düzenle",
        .category = Category::Modify,
        .params =
            {
                Param::choice("islem", Arity::optional(), {"ac", "kaydet", "vazgec"},
                              "ac: tanımı düzenlemeye açar (varsayılan); kaydet: tanımı düzenlenen "
                              "nesnelerden yeniden kurar; vazgec: açılanı siler")
                    .en("action"),
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Düzenlenen blok referansı, bir tane; açarken yoksa etkin seçim"}
                    .en("reference"),
                Param::text("ad", Arity::optional(),
                            "Referans yerine bloğun adı: tanım kendi yerinde açılır")
                    .en("name"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "kaydet ve vazgec için bloğun nesneleri: açılanlar ve sonradan çizilenler"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Blok tanımını düzenlemeye açar ve düzenlenen nesnelerden yeniden kurar; "
                   "bütün referanslar yeni biçimi çizer.",
        .run     = &run_block_edit,
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
                              "Ölçek; varsayılan 1. Eksi değer aynalar; olcek_y verilmezse o da "
                              "eksi olur ve ikisi birlikte yarım dönüştür")
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
