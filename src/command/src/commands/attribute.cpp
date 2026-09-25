// SPDX-License-Identifier: GPL-3.0-or-later
// core.attribute — ÖZNİTELİK. The one way a value gets onto an entity.
//
// model.md R28 is the load-bearing rule here: an attribute write is a column, a
// row and a value, so this command has ONE write path no matter how many columns
// a catalogue declares. Adding "ada no" to the schema adds no code here, no Op
// kind and no journal case — which is the point of a schema that lives in /data.
//
// R29/P29 is the other half: nothing in the frame path reads what this writes.
// The renderer reads a StyleId, resolved at commit time. An attribute is what the
// parcel IS; the style column is what the parcel LOOKS LIKE, and the second is
// derived from the first by a rule, never looked up per frame.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/text.hpp"

#include <string>

namespace kentos::command {
namespace {

/// The one parser, in core, so the prompt and the attribute grid cannot disagree
/// about what `evet` or `0,40` means. It used to live here as a local function;
/// see `core::attr_parse` for why it moved.
core::Result<core::AttrValue> parse_for(const core::AttrSpec& spec, const std::string& text)
{
    return core::attr_parse(spec, text);
}

std::string show(const core::AttrValue& v)
{
    if (!v.present) return "yok";
    switch (v.type) {
    case core::AttrType::Text:
    case core::AttrType::CodeRef: return v.text;
    case core::AttrType::Bool: return v.number != 0 ? "evet" : "hayır";
    case core::AttrType::Length: return std::to_string(v.number) + " mm";
    case core::AttrType::Int64: return std::to_string(v.number);
    case core::AttrType::Decimal:
        return core::decimal_to_text(v.number, v.scale, core::DecimalMark::Point);
    case core::AttrType::Date: return core::date_to_text(v.number);
    }
    return "?";
}

void list_schema(Context& ctx)
{
    const core::AttrTable& table = ctx.document().attributes();
    if (table.columns() == 0) {
        ctx.echo("Bu belgede tanımlı öznitelik yok. Şema veri paketinden yüklenir; "
                 "elle sütun eklemek için ÖZNİTELİK TANIMLA kullanın.");
        return;
    }
    ctx.echo("Öznitelikler (" + std::to_string(table.columns()) + " sütun, " +
             std::to_string(table.rows()) + " satır):");
    // COUNTED IN THE BOUND'S OWN TYPE. `AttrId` is narrower than `columns()`
    // returns, so a table with more columns than the id can count would wrap and
    // loop for ever. The id is made where it is used, which is the one place the
    // narrowing is real.
    for (std::size_t i = 0; i < table.columns(); ++i) {
        const auto c               = static_cast<core::AttrId>(i);
        const core::AttrSpec& spec = table.column(c)->spec();
        ctx.echo("    " + spec.id + "  (" + core::attr_type_name(spec.type) + ")" +
                 (spec.layer.empty() ? "  proje" : "  katman: " + spec.layer) +
                 (spec.required ? "  zorunlu" : "") +
                 (spec.catalog.empty() ? "" : "  katalog: " + spec.catalog) + "  — " +
                 spec.name_tr);
    }
}

Task<void> run(Context& ctx)
{
    const Value name = ctx.argument("ad");
    if (name.empty()) {
        list_schema(ctx);
        co_return;
    }

    const core::AttrTable& table = ctx.document().attributes();
    const core::AttrId col       = table.find(name.as_text());
    if (col == core::kNoAttr) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Bilinmeyen öznitelik: '" + name.as_text() +
                       "'. Tanımlı olanları görmek için argümansız ÖZNİTELİK yazın.");
        co_return;
    }

    const Value target = ctx.argument("nesne");
    if (target.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Hangi nesne? Kullanım: ÖZNİTELİK <ad> <nesne-kimliği> [deger]");
        co_return;
    }

    const auto key            = static_cast<core::EntityKey>(target.as_int());
    const core::EntityId slot = ctx.document().slot_of(key);
    if (slot == core::kNoEntity) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Bilinmeyen nesne: " + std::to_string(target.as_int()) +
                       ". Nesne kimliklerini SEÇ ile görebilirsiniz.");
        co_return;
    }

    const Value value = ctx.argument("deger");
    if (value.empty()) {
        auto had = ctx.document().attribute(col, slot);
        if (!had) {
            ctx.refuse(had.error());
            co_return;
        }
        ctx.echo(name.as_text() + " [" + std::to_string(target.as_int()) +
                 "] = " + show(had.value()));
        co_return;
    }

    auto parsed = parse_for(table.column(col)->spec(), value.as_text());
    if (!parsed) {
        ctx.refuse(parsed.error());
        co_return;
    }

    auto before = ctx.document().attribute(col, slot);
    if (auto st = ctx.transaction().set_attribute(col, slot, parsed.value()); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    // Canonical column id, not the alias that was typed, so a journal replay
    // resolves the same column whatever the user's keyboard did (§2.2).
    ctx.record("ad", Value::text(table.column(col)->spec().id));
    ctx.record("nesne", target);
    ctx.record("deger", value);
    ctx.echo(name.as_text() + " [" + std::to_string(target.as_int()) + "] = " +
             show(parsed.value()) + (before ? "   (önceki: " + show(before.value()) + ")" : ""));
}

} // namespace

KENTOS_COMMAND(attribute)
{
    return CommandSpec{
        .id       = "core.attribute",
        .names    = {"ÖZNİTELİK", "OZNITELIK", "ATTRIBUTE", "ÖZN", "OZN"},
        .title    = "Öznitelik",
        .category = Category::Modify,
        .params =
            {
                Param::text("ad", Arity::optional(),
                            "Öznitelik kimliği; yoksa tanımlı sütunlar listelenir")
                    .en("name"),
                Param::integer("nesne", Arity::optional(), "Nesnenin kalıcı kimliği").en("object"),
                Param::text("deger", Arity::optional(),
                            "Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır")
                    .en("value"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.",
        .run     = &run,
    };
}

// ---------------------------------------------------------------- SÜTUN ----

namespace {

/// Declaring a column is its own command rather than a mode of ÖZNİTELİK, and the
/// reason is boring and decisive: a column id is text and an entity id is a
/// number, so one command cannot bind both to the same positional slot. Splitting
/// them also matches what they are — one changes the SCHEMA, the other changes a
/// VALUE, and only the second is undoable.
/// Every type word `SÜTUN` accepts, for the message a wrong one gets.
///
/// Built from the ONE list in core rather than repeated here: this command used
/// to carry its own if-chain, which is exactly how `metin` comes to mean one
/// thing in one command and something else in another (see the note above
/// `core::attr_type_from_name`).
const char* kTypeWords = "tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod";

/// Refuses a schema edit that keeps no record while a batch is open, with the
/// way out; true when it refused.
bool refuse_in_batch(Context& ctx, const std::string& column, const char* verb)
{
    if (!ctx.session().bus().in_batch()) return false;
    ctx.refuse(core::ErrorCode::Unsupported,
               "'" + column + "' sütunu bir betiğin ya da toplu işin içinde " + verb +
                   ": bu değişiklik geri alınamaz, iş yarıda kalırsa geri getirilemezdi. "
                   "SÜTUN komutunu betikten önce ayrıca çalıştırın.");
    return true;
}

Task<void> run_column(Context& ctx)
{
    // NAMED, AND NOT FOR TIDINESS. `Context::argument` returns a `Value` BY
    // VALUE and `as_text()` hands back a reference into it, so reading one
    // straight out of the call dangles the moment the temporary dies.
    const Value id = ctx.argument("kimlik");
    if (id.empty()) {
        list_schema(ctx);
        co_return;
    }
    const std::string column = id.as_text();

    // ---- drop ----
    if (ctx.argument("sil").as_bool()) {
        // NOT INSIDE A BATCH (TODOS F-05): a drop takes its cells with it and
        // keeps no record, so a script that failed after it could not give them
        // back — and a script that fails leaves nothing behind.
        if (refuse_in_batch(ctx, column, "silinmez")) co_return;
        auto dropped = ctx.transaction().drop_attribute(column);
        if (!dropped) {
            ctx.refuse(dropped.error());
            co_return;
        }
        ctx.record("kimlik", id);
        ctx.record("sil", Value::boolean(true));
        ctx.echo("Sütun silindi: " + column);
        // A SHEET THAT PRINTED IT is told now, not at the next print (TODOS F-04):
        // its table or chart names a column the drawing no longer has.
        std::string named;
        std::size_t count = 0;
        for (const core::SheetTie& t : core::sheet_ties(ctx.document())) {
            if (!t.broken || t.kind != core::SheetTieKind::Column ||
                !core::turkish_key_equals(t.name, column))
                continue;
            ++count;
            named += (named.empty() ? "" : ", ") + std::string("'") + t.layout + "' ▸ " +
                     (t.item.empty() ? std::string("atlas") : "'" + t.item + "'");
        }
        if (count != 0)
            ctx.echo("Silinen '" + column + "' sütununu " + std::to_string(count) +
                     " pafta öğesi okuyordu (" + named +
                     "); o öğe artık onu çıkaramaz. Görmek için: BAĞIMLILIK");
        co_return;
    }

    const Value type    = ctx.argument("tur");
    const Value label   = ctx.argument("ad");
    const Value about   = ctx.argument("aciklama");
    const Value must    = ctx.argument("zorunlu");
    const Value catalog = ctx.argument("katalog");
    const Value digits  = ctx.argument("basamak");
    const Value scoped  = ctx.argument("katman");

    const core::AttrTable& table = ctx.document().attributes();
    const core::AttrId found     = table.find(column);

    // ---- amend: what an existing column says about itself ----
    //
    // A second call naming a column that already exists is an EDIT, not a
    // duplicate declaration. What it may change is what the column says about
    // itself; the id and the type are the column's identity and the meaning of
    // its stored integers, and `AttrColumn::amend` refuses both.
    if (found != core::kNoAttr) {
        core::AttrSpec next = table.column(found)->spec();
        if (!label.empty()) next.name_tr = label.as_text();
        if (!about.empty()) next.summary_tr = about.as_text();
        if (!must.empty()) next.required = must.as_bool();
        if (!catalog.empty()) next.catalog = catalog.as_text();
        if (!digits.empty()) next.scale = static_cast<std::uint8_t>(digits.as_int());
        if (!scoped.empty()) next.layer = scoped.as_text();

        if (!type.empty()) {
            const auto wanted = core::attr_type_from_name(type.as_text());
            if (!wanted) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Bilinmeyen öznitelik türü: '" + type.as_text() +
                               "'. Beklenen: " + kTypeWords + ".");
                co_return;
            }
            next.type = *wanted;
        }

        // Nor amended inside one: a new scale rescales every cell, and an aborted
        // script could not scale them back.
        if (refuse_in_batch(ctx, column, "değiştirilmez")) co_return;
        auto amended = ctx.transaction().amend_attribute(column, next);
        if (!amended) {
            ctx.refuse(amended.error());
            co_return;
        }

        ctx.record("kimlik", id);
        if (!label.empty()) ctx.record("ad", label);
        if (!about.empty()) ctx.record("aciklama", about);
        if (!must.empty()) ctx.record("zorunlu", must);
        if (!catalog.empty()) ctx.record("katalog", catalog);
        if (!digits.empty()) ctx.record("basamak", digits);
        if (!scoped.empty()) ctx.record("katman", scoped);
        ctx.echo("Sütun güncellendi: " + column);
        co_return;
    }

    // ---- declare ----
    if (type.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string("Kullanım: SÜTUN <kimlik> <tur>. Türler: ") + kTypeWords + ".");
        co_return;
    }

    const std::string word = type.as_text();
    const auto wanted      = core::attr_type_from_name(word);
    if (!wanted) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bilinmeyen öznitelik türü: '" + word + "'. Beklenen: " + kTypeWords + ".");
        co_return;
    }

    core::AttrSpec spec;
    spec.id      = column;
    spec.name_tr = label.empty() ? column : label.as_text();
    spec.type    = *wanted;
    if (!about.empty()) spec.summary_tr = about.as_text();
    if (!must.empty()) spec.required = must.as_bool();
    if (!catalog.empty()) spec.catalog = catalog.as_text();

    // TWO KINDS OF COLUMN. Without `katman` this is the PROJECT's: `ada` and
    // `parsel` are facts about every parcel in the drawing. With it, the column
    // belongs to that layer alone — `direk_yuksekligi` means nothing on a road
    // centreline, and declaring it project-wide puts an empty row in the
    // inspector of every object there is.
    if (!scoped.empty()) spec.layer = scoped.as_text();

    // TWO DIGITS BY DEFAULT, because that is what a TAKS, a KAKS and a rate all
    // carry, and a decimal column declared with none would be an integer with a
    // point in its name.
    if (spec.type == core::AttrType::Decimal)
        spec.scale = digits.empty() ? 2 : static_cast<std::uint8_t>(digits.as_int());
    if (spec.scale > core::kMaxScale) spec.scale = core::kMaxScale;

    auto made = ctx.transaction().declare_attribute(spec);
    if (!made) {
        ctx.refuse(made.error());
        co_return;
    }

    ctx.record("kimlik", id);
    ctx.record("tur", type);
    if (!label.empty()) ctx.record("ad", label);
    if (!about.empty()) ctx.record("aciklama", about);
    if (!must.empty()) ctx.record("zorunlu", must);
    if (!catalog.empty()) ctx.record("katalog", catalog);
    if (!scoped.empty()) ctx.record("katman", scoped);
    if (spec.type == core::AttrType::Decimal) ctx.record("basamak", Value::integer(spec.scale));

    ctx.echo("Sütun tanımlandı: " + column + " (" + core::attr_type_name(spec.type) + ")" +
             (spec.layer.empty() ? ", proje geneli" : ", yalnız '" + spec.layer + "' katmanında"));
}

} // namespace

KENTOS_COMMAND(column)
{
    return CommandSpec{
        .id       = "core.column",
        .names    = {"SÜTUN", "SUTUN", "COLUMN", "STN"},
        .title    = "Sütun",
        .category = Category::Modify,
        .params =
            {
                Param::text("kimlik", Arity::optional(),
                            "Sütun kimliği; yoksa tanımlı sütunlar listelenir")
                    .en("id"),
                Param::text("tur", Arity::optional(),
                            "tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod")
                    .en("type"),
                Param::text("ad", Arity::optional(), "Panelde görünen Türkçe ad").en("name"),
                Param::text("aciklama", Arity::optional(), "Tek satırlık açıklama").en("note"),
                Param::boolean("zorunlu", Arity::optional(), "Her satır bir değer taşımalı mı")
                    .en("required"),
                Param::text("katalog", Arity::optional(), "Yalnız 'kod' türü için: katalog kimliği")
                    .en("catalog"),
                Param::integer("basamak", Arity::optional(),
                               "Yalnız 'ondalik' için: noktadan sonraki basamak sayısı")
                    .en("digits"),
                Param::text("katman", Arity::optional(),
                            "Sütunu yalnız bu katmana tanımlar; yoksa proje geneli")
                    .en("layer"),
                Param::boolean("sil", Arity::optional(), "Sütunu ve içindeki bütün değerleri siler")
                    .en("delete"),
            },
        // NOT undoable, and for the same reason a layer is not: the schema is what
        // rows are addressed against, and undoing a declaration would invalidate
        // every row index the journal already holds.
        .undo  = UndoPolicy::None,
        .flags = Flags::Scriptable,
        .summary = "Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler.",
        .run = &run_column,
    };
}

} // namespace kentos::command
