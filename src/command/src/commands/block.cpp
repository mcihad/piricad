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
#include "kentos_cad/command/external_ref.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/text_fields.hpp"
#include "kentos_cad/core/transform.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
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

/// Where the caption that names `field` stands when reference `e` draws it —
/// the first member caption of its block that names it — so its value is asked
/// for there. The insertion point for a field only a block inside names.
core::Point2 field_place(const core::Document& doc, core::EntityId e, const std::string& field)
{
    const std::uint32_t gslot = doc.entities().slot[e];
    const core::Point2 at     = core::block_reference_insertion(doc.geometry(), gslot);
    auto ref                  = core::block_reference_of(doc.geometry(), gslot);
    if (!ref || ref.value().block >= doc.blocks().size()) return at;
    const core::BlockDef& def = doc.blocks().at(ref.value().block);
    for (const core::EntityKey key : def.members) {
        const core::EntityId m = doc.slot_of(key);
        if (m == core::kNoEntity || !doc.alive(m)) continue;
        const std::uint32_t slot = doc.entities().slot[m];
        if (!doc.texts().has(slot)) continue;
        const std::vector<std::string> named = core::field_names(doc.texts().text(slot));
        if (std::find(named.begin(), named.end(), field) == named.end()) continue;
        const core::RingSpan span = doc.geometry().rings_of(slot);
        if (span.count == 0 || doc.geometry().ring_xs(span.first).empty()) continue;
        const core::Point2 start{doc.geometry().ring_xs(span.first)[0],
                                 doc.geometry().ring_ys(span.first)[0]};
        return core::place_block_point(ref.value(), at, def.base, start, 0, 0);
    }
    return at;
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
    auto placed = place_reference(ctx, *base, ref);
    if (!placed) {
        ctx.refuse(placed.error());
        co_return;
    }
    // A caption naming `{no}` makes `no` a field of the block: its column is
    // declared now, so every reference has a cell to carry its value.
    auto declared = ensure_field_columns(ctx, block.value());
    if (!declared) {
        ctx.refuse(declared.error());
        co_return;
    }

    ctx.record("ad", Value::text(*name));
    ctx.record("taban", Value::point(*base));
    ctx.record("nesneler", Value::ids(requested));
    if (!description.empty()) ctx.record("aciklama", Value::text(description));
    std::string fields;
    for (const std::string& f : core::block_fields(ctx.document(), block.value()))
        fields += (fields.empty() ? "" : ", ") + f;
    ctx.echo("'" + *name + "' bloğu " + std::to_string(slots.size()) +
             " nesneyle tanımlandı ve yerine bir referans kondu" +
             (fields.empty() ? std::string(".") : "; alanları: " + fields + "."));
}

// --------------------------------------------------------------- BLOKEKLE ----

Task<void> run_insert(Context& ctx)
{
    // A BLOCK FROM A LIBRARY FILE (TODOS C-13): a project, DXF or DWG file
    // kept as a symbol library. Its block — the one `ad=` names, its only one,
    // or the whole drawing named after the file — is brought into this drawing
    // inside this command's transaction, and placed like any other; a name
    // the drawing already has keeps the drawing's definition.
    std::string from_library;
    std::string library_note;
    if (const Value file = ctx.argument("dosya"); !file.empty()) {
        Bus& bus = ctx.session().bus();
        if (!bus.on_file_request) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Dosya motoru bağlı değil; kitaplıktan blok bu yapıda eklenemiyor.");
            co_return;
        }
        FileRequest request;
        request.verb    = FileRequest::Verb::BlockLibrary;
        request.tx      = &ctx.transaction();
        request.session = &ctx.session();
        request.path    = file.as_text();
        if (const Value named = ctx.argument("ad"); !named.empty()) {
            request.block = named.as_text();
        } else {
            // WHICH ONE, ASKED: the file's blocks are read first and, when it
            // holds more than one, offered as the name to place — as BLOKEKLE
            // offers the drawing's own. A script that named none is told the
            // names instead.
            std::vector<std::string> offered;
            FileRequest listing    = request;
            listing.tx             = nullptr;
            listing.library_blocks = &offered;
            if (auto listed = co_await bus.on_file_request(listing); !listed) {
                ctx.refuse(listed.error());
                co_return;
            }
            if (offered.size() > 1) {
                auto picked = co_await ctx.text("ad", "Kitaplıktan hangi blok", offered);
                if (!picked || picked->empty()) {
                    std::string known;
                    for (const std::string& n : offered)
                        known += (known.empty() ? "" : ", ") + n;
                    ctx.refuse(core::ErrorCode::InvalidArgument,
                               "'" + request.path +
                                   "' içinde birden çok blok var; hangisi: ad=<blok>. Bloklar: " +
                                   known + ".");
                    co_return;
                }
                request.block = *picked;
            }
        }
        request.resolved_block = &from_library;
        auto said              = co_await bus.on_file_request(request);
        if (!said) {
            ctx.refuse(said.error());
            co_return;
        }
        library_note = said.value();
        ctx.record("dosya", Value::text(request.path));
    }

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
    if (defined.empty() && from_library.empty()) {
        ctx.refuse(
            core::ErrorCode::NotFound,
            "Bu çizimde tanımlı blok yok, yerleştirilecek bir şey de yok. Blok, bir kez "
            "çizilip çok kez yerleştirilen bir semboldür (rögar kapağı, direk, ağaç): önce "
            "nesneleri seçip BLOK ad=<isim> ile tanımlayın, sonra BLOKEKLE onları istediğiniz "
            "her yere koyar.");
        co_return;
    }

    std::optional<std::string> name;
    if (from_library.empty())
        name = co_await ctx.text("ad", "Yerleştirilecek bloğun adı", defined);
    else
        name = from_library;
    // SAID, NOT SILENT: a script that named no block is told so, as a hand
    // that gave an empty name is; Esc is answered "İptal edildi" by the bus.
    if (!name || name->empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Yerleştirilecek blok verilmedi: ad=<blok> ya da dosya=<kitaplık>. " +
                       blocks_known(ctx.document()));
        co_return;
    }
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

    auto placed = place_reference(ctx, *at, ref);
    if (!placed) {
        ctx.refuse(placed.error());
        co_return;
    }

    // THE BLOCK'S FIELDS, each given this reference's value (TODOS C-13): a
    // member caption naming `{no}` is drawn with the reference's own `no`
    // cell — the ATTRIB a DXF writes on the insert. Given as `deger=no:12`;
    // asked, where each will stand, when a hand placed the block and gave
    // none, as AutoCAD asks for attribute values; a script that gave none is
    // not asked, and an empty answer leaves the cell empty.
    auto declared = ensure_field_columns(ctx, block);
    if (!declared) {
        ctx.refuse(declared.error());
        co_return;
    }
    const core::Document& doc             = ctx.document();
    const std::vector<std::string> fields = core::block_fields(doc, block);
    std::vector<std::pair<std::string, std::string>> given;
    const Value asked_for = ctx.argument("deger");
    for (const std::string& pair : asked_for.as_texts()) {
        const std::size_t colon = pair.find(':');
        if (colon == std::string::npos || colon == 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "deger 'sutun:değer' biçiminde yazılır; verilen: '" + pair +
                           "'.\n  Örnek: BLOKEKLE ad=ROGAR nokta=10,10 deger=no:R-12");
            co_return;
        }
        std::string field = pair.substr(0, colon);
        if (std::find(fields.begin(), fields.end(), field) == fields.end()) {
            std::string known;
            for (const std::string& f : fields)
                known += (known.empty() ? "" : ", ") + f;
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Blok '" + *name + "' '" + field + "' alanını taşımıyor. " +
                           (known.empty() ? std::string("Bu bloğun alanı yok.")
                                          : "Alanları: " + known + "."));
            co_return;
        }
        given.emplace_back(std::move(field), pair.substr(colon + 1));
    }
    std::vector<std::string> recorded;
    for (const std::string& field : fields) {
        std::string value;
        bool have = false;
        for (const auto& [f, v] : given)
            if (f == field) {
                value = v;
                have  = true;
            }
        if (!have && asked_for.empty())
            if (auto typed =
                    co_await ctx.text("deger", "'" + field + "' değeri — boş Enter boş bırakır",
                                      TextPlace{field_place(doc, placed.value(), field), false});
                typed)
                value = *typed;
        if (value.empty()) continue;
        const core::AttrId col         = doc.attributes().find(field);
        const core::AttrColumn* column = doc.attributes().column(col);
        if (column == nullptr) continue;
        auto parsed = core::attr_parse(column->spec(), value);
        if (!parsed) {
            ctx.refuse(parsed.error().code,
                       "'" + field + "' değeri okunamadı: " + parsed.error().message);
            co_return;
        }
        if (auto st = ctx.transaction().set_attribute(col, placed.value(), parsed.value()); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        recorded.push_back(field + ":" + value);
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
    if (!recorded.empty()) ctx.record("deger", Value::texts(recorded));
    std::string said = library_note.empty() ? std::string() : library_note + " ";
    said += "'" + *name + "' bloğu yerleştirildi";
    if (columns > 1 || rows > 1)
        said += " (" + std::to_string(columns) + "×" + std::to_string(rows) + " dizi)";
    if (!recorded.empty()) {
        said += "; değerler:";
        for (const std::string& pair : recorded)
            said += " " + pair;
    }
    if (!declared.value().empty()) {
        said += "; alan sütunu tanımlandı:";
        for (const std::string& column : declared.value())
            said += " " + column;
    }
    ctx.echo(said + ".");
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

/// THE BASE POINT MOVED, THE PICTURE KEPT (TODOS C-13). The new base is shown
/// on a reference — undone through that reference's placement back to the
/// definition — or, with `ad=`, given in the definition's own coordinates.
/// Every reference of the block, on the sheet or inside another definition,
/// is then stood where its new base is drawn now, so nothing on the sheet
/// moves: only where the next BLOKEKLE puts the block by does.
Task<void> move_base(Context& ctx, const EditTarget& target, const std::string& name,
                     core::Json report)
{
    const core::Document& doc = ctx.document();
    const core::Point2 was    = doc.blocks().at(target.block).base;
    std::vector<core::EntityId> references;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e) || doc.entities().kind[e] != core::kBlockReferenceKind) continue;
        auto ref = core::block_reference_of(doc.geometry(), doc.entities().slot[e]);
        if (!ref || ref.value().block != target.block) continue;
        // AN OPEN EDIT is out at its reference's insertion point less this
        // base; moving the base under it would save it to the wrong place.
        if ((doc.entities().flags[e] & core::FlagHidden) != 0 &&
            (doc.entities().flags[e] & core::FlagInBlock) == 0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "'" + name + "' bloğunun bir düzenlemesi açık (referans " +
                           std::to_string(core::raw(doc.key_of(e))) +
                           " gizli). Taban noktasını değiştirmeden önce düzenlemeyi kaydedin ya "
                           "da vazgeçin.");
            co_return;
        }
        references.push_back(e);
    }

    // Asked with a band from where the base is drawn now, on that reference.
    core::Point2 shown_at = was;
    std::optional<core::Xform> back;
    if (target.reference != core::kNoEntity) {
        const std::uint32_t gslot = doc.entities().slot[target.reference];
        const auto ref            = core::block_reference_of(doc.geometry(), gslot).value();
        const core::Point2 at     = core::block_reference_insertion(doc.geometry(), gslot);
        shown_at                  = at;
        back                      = core::block_placement_inverse(ref, at, was);
        if (!back) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Referans " + std::to_string(target.reference_key) +
                           " x ve y'de farklı ölçekli: bir nokta onun çiziminden tanıma tek "
                           "biçimde geri götürülemez. Yeni taban noktasını eşit ölçekli bir "
                           "referansta gösterin ya da ad= ile tanımın kendi koordinatında verin.");
            co_return;
        }
    }
    auto point = co_await ctx.point("taban", "Bloğun yeni taban noktası",
                                    PointOptions{.rubber_band = true, .rubber_origin = shown_at});
    if (!point) co_return;
    const core::Point2 base = back ? core::transformed(*back, *point) : *point;
    ctx.record("taban", Value::point(*point));
    if (base == was) {
        ctx.echo("'" + name + "' bloğunun taban noktası zaten orada; bir şey değişmedi.");
        co_return;
    }

    // Where each reference draws the new base NOW, found before the base moves;
    // then the base, then the references — whose boxes are worked out against
    // the new base.
    std::vector<core::Point2> stands;
    stands.reserve(references.size());
    for (const core::EntityId e : references) {
        const std::uint32_t gslot = doc.entities().slot[e];
        const auto ref            = core::block_reference_of(doc.geometry(), gslot).value();
        stands.push_back(core::place_block_point(
            ref, core::block_reference_insertion(doc.geometry(), gslot), was, base, 0, 0));
    }
    if (auto st = ctx.transaction().set_block_base(target.block, base); !st) {
        ctx.refuse(st.error());
        co_return;
    }
    for (std::size_t i = 0; i < references.size(); ++i)
        if (auto st = ctx.transaction().move_reference(references[i], stands[i]); !st) {
            ctx.refuse(st.error());
            co_return;
        }

    report.set("taban",
               core::Json::array({core::Json::integer(base.x), core::Json::integer(base.y)}));
    report.set("referans_sayisi",
               core::Json::integer(static_cast<std::int64_t>(references.size())));
    ctx.report(std::move(report));
    ctx.echo("'" + name + "' bloğunun taban noktası değişti; " + std::to_string(references.size()) +
             " referansı çizildiği yerde tutuldu. Bundan sonra BLOKEKLE bloğu bu noktasından "
             "yerleştirir.");
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
    const bool basing  = core::turkish_key_equals(step, "taban");
    // The word as the spec spells it, whatever folding the caller typed.
    std::string word = "vazgec";
    if (opening) word = "ac";
    if (saving) word = "kaydet";
    if (basing) word = "taban";
    ctx.record("islem", Value::text(word));

    EditTarget target;
    if (!co_await edit_target(ctx, opening || basing, target)) co_return;
    // AN EXTERNAL REFERENCE IS EDITED IN ITS OWN FILE (TODOS C-14): what is
    // drawn here is read from there on every open, so an edit made here would
    // be gone on the next one — and would silently disagree with the source
    // until then.
    if (is_external_block(ctx.document(), target.block)) {
        const std::string name = ctx.document().blocks().at(target.block).name;
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "'" + name +
                       "' bir dış referansın parçası; tanımı kendi dosyasında düzenlenir. "
                       "Değişikliği görmek için DIŞREFERANS islem=yenile; burada düzenlemek "
                       "için önce DIŞREFERANS islem=bagla ile çizime bağlayın.");
        co_return;
    }
    const core::Document& doc = ctx.document();
    const std::string name    = doc.blocks().at(target.block).name;
    const bool from_reference = target.reference != core::kNoEntity;
    const bool reference_aside =
        from_reference && (doc.entities().flags[target.reference] & core::FlagHidden) != 0;
    core::Json report;
    report.set("islem", core::Json::string(word));
    report.set("blok", core::Json::string(name));
    if (from_reference) report.set("referans", core::Json::integer(target.reference_key));

    if (basing) {
        co_await move_base(ctx, target, name, report);
        co_return;
    }

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
    if (auto declared = ensure_field_columns(ctx, target.block); !declared) {
        ctx.refuse(declared.error());
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
                Param::choice("islem", Arity::optional(), {"ac", "kaydet", "vazgec", "taban"},
                              "ac: tanımı düzenlemeye açar (varsayılan); kaydet: tanımı düzenlenen "
                              "nesnelerden yeniden kurar; vazgec: açılanı siler; taban: taban "
                              "noktasını taşır, referanslar yerinde kalır")
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
                Param{"taban", ParamKind::Point, Arity::optional(),
                      "taban için yeni taban noktası: referansın çiziminde, ad= ile tanımın "
                      "kendi koordinatında"}
                    .en("base"),
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
                Param::text("ad", Arity::optional(),
                            "Yerleştirilecek bloğun adı; dosya= ile kitaplıktaki bloğun adı")
                    .en("name"),
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
                Param::text("deger", Arity{0, 0xFFFFFFFFu},
                            "Bloğun alanlarının değerleri, sutun:değer; verilmezse elle "
                            "yerleştirmede her alan sorulur")
                    .en("values"),
                Param::text("dosya", Arity::optional(),
                            "Blok kitaplığı: bloğun alınacağı proje, DXF ya da DWG dosyası; ad= "
                            "dosyadaki bloğu seçer, blok yoksa bütün çizim dosyanın adıyla blok "
                            "olur")
                    .en("file"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.",
        .run     = &run_insert,
    };
}

} // namespace kentos::command
