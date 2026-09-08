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
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/text.hpp"

#include <string>

namespace kentos::command {
namespace {

/// Parses a value against the column's declared type. There is no guessing: the
/// schema said what this column holds, and a value that is not that is refused
/// with the type named, not silently coerced.
core::Result<core::AttrValue> parse_for(const core::AttrSpec& spec, const std::string& text)
{
    if (core::turkish_iequals(text, "yok") || core::turkish_iequals(text, "bos") ||
        core::turkish_iequals(text, "boş"))
        return core::AttrValue{spec.type, false, 0, {}};

    switch (spec.type) {
    case core::AttrType::Text: return core::attr_text(text);
    case core::AttrType::CodeRef: return core::attr_code(text);
    case core::AttrType::Bool: {
        if (core::turkish_iequals(text, "evet") || text == "1") return core::attr_bool(true);
        if (core::turkish_iequals(text, "hayır") || core::turkish_iequals(text, "hayir") ||
            text == "0")
            return core::attr_bool(false);
        return core::err(core::ErrorCode::InvalidArgument,
                         "'" + spec.id + "' özniteliği evet/hayır bekliyor. Girilen: '" + text +
                             "'");
    }
    case core::AttrType::Int64:
    case core::AttrType::Length: {
        try {
            std::size_t used  = 0;
            const long long v = std::stoll(text, &used);
            if (used != text.size()) throw std::invalid_argument("kuyruk");
            return spec.type == core::AttrType::Length
                       ? core::attr_mm(static_cast<core::Mm>(v))
                       : core::attr_int64(static_cast<std::int64_t>(v));
        } catch (const std::exception&) {
            return core::err(core::ErrorCode::InvalidArgument,
                             "'" + spec.id + "' özniteliği tam sayı bekliyor" +
                                 (spec.type == core::AttrType::Length ? " (milimetre)" : "") +
                                 ". Girilen: '" + text + "'");
        }
    }
    case core::AttrType::Decimal: {
        // The column's own precision, not the one the typing happened to use. A
        // value with more digits than the column declares is REFUSED rather than
        // rounded: a document that quietly turned 0.405 into 0.40 would be saying
        // something the user did not.
        const auto scaled = core::decimal_from_text(text, spec.scale);
        if (!scaled)
            return core::err(core::ErrorCode::InvalidArgument,
                             "'" + spec.id + "' özniteliği " + std::to_string(spec.scale) +
                                 " basamaklı ondalık sayı bekliyor. Girilen: '" + text + "'");
        return core::attr_decimal(*scaled, spec.scale);
    }
    case core::AttrType::Date: {
        const auto days = core::date_from_text(text);
        if (!days)
            return core::err(core::ErrorCode::InvalidArgument,
                             "'" + spec.id +
                                 "' özniteliği YYYY-AA-GG biçiminde tarih bekliyor. "
                                 "Girilen: '" +
                                 text + "'");
        return core::attr_date(*days);
    }
    }
    return core::err(core::ErrorCode::Internal, "Bilinmeyen öznitelik türü.");
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
        ctx.echo("Bilinmeyen öznitelik: '" + name.as_text() +
                 "'. Tanımlı olanları görmek için argümansız ÖZNİTELİK yazın.");
        co_return;
    }

    const Value target = ctx.argument("nesne");
    if (target.empty()) {
        ctx.echo("Hangi nesne? Kullanım: ÖZNİTELİK <ad> <nesne-kimliği> [deger]");
        co_return;
    }

    const auto key            = static_cast<core::EntityKey>(target.as_int());
    const core::EntityId slot = ctx.document().slot_of(key);
    if (slot == core::kNoEntity) {
        ctx.echo("Bilinmeyen nesne: " + std::to_string(target.as_int()) +
                 ". Nesne kimliklerini SEÇ ile görebilirsiniz.");
        co_return;
    }

    const Value value = ctx.argument("deger");
    if (value.empty()) {
        auto had = ctx.document().attribute(col, slot);
        if (!had) {
            ctx.echo(had.error().message);
            co_return;
        }
        ctx.echo(name.as_text() + " [" + std::to_string(target.as_int()) +
                 "] = " + show(had.value()));
        co_return;
    }

    auto parsed = parse_for(table.column(col)->spec(), value.as_text());
    if (!parsed) {
        ctx.echo(parsed.error().message);
        co_return;
    }

    auto before = ctx.document().attribute(col, slot);
    if (auto st = ctx.transaction().set_attribute(col, slot, parsed.value()); !st) {
        ctx.echo(st.error().message);
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
        .category = Category::Modify,
        .params =
            {
                Param::text("ad", Arity::optional(),
                            "Öznitelik kimliği; yoksa tanımlı sütunlar listelenir"),
                Param::integer("nesne", Arity::optional(), "Nesnenin kalıcı kimliği"),
                Param::text("deger", Arity::optional(),
                            "Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır"),
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
        auto dropped = ctx.transaction().drop_attribute(column);
        if (!dropped) {
            ctx.echo(dropped.error().message);
            co_return;
        }
        ctx.record("kimlik", id);
        ctx.record("sil", Value::boolean(true));
        ctx.echo("Sütun silindi: " + column);
        co_return;
    }

    const Value type    = ctx.argument("tur");
    const Value label   = ctx.argument("ad");
    const Value about   = ctx.argument("aciklama");
    const Value must    = ctx.argument("zorunlu");
    const Value catalog = ctx.argument("katalog");
    const Value digits  = ctx.argument("basamak");

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

        if (!type.empty()) {
            const auto wanted = core::attr_type_from_name(type.as_text());
            if (!wanted) {
                ctx.echo("Bilinmeyen öznitelik türü: '" + type.as_text() +
                         "'. Beklenen: " + kTypeWords + ".");
                co_return;
            }
            next.type = *wanted;
        }

        auto amended = ctx.transaction().amend_attribute(column, next);
        if (!amended) {
            ctx.echo(amended.error().message);
            co_return;
        }

        ctx.record("kimlik", id);
        if (!label.empty()) ctx.record("ad", label);
        if (!about.empty()) ctx.record("aciklama", about);
        if (!must.empty()) ctx.record("zorunlu", must);
        if (!catalog.empty()) ctx.record("katalog", catalog);
        if (!digits.empty()) ctx.record("basamak", digits);
        ctx.echo("Sütun güncellendi: " + column);
        co_return;
    }

    // ---- declare ----
    if (type.empty()) {
        ctx.echo(std::string("Kullanım: SÜTUN <kimlik> <tur>. Türler: ") + kTypeWords + ".");
        co_return;
    }

    const std::string word = type.as_text();
    const auto wanted      = core::attr_type_from_name(word);
    if (!wanted) {
        ctx.echo("Bilinmeyen öznitelik türü: '" + word + "'. Beklenen: " + kTypeWords + ".");
        co_return;
    }

    core::AttrSpec spec;
    spec.id      = column;
    spec.name_tr = label.empty() ? column : label.as_text();
    spec.type    = *wanted;
    if (!about.empty()) spec.summary_tr = about.as_text();
    if (!must.empty()) spec.required = must.as_bool();
    if (!catalog.empty()) spec.catalog = catalog.as_text();

    // TWO DIGITS BY DEFAULT, because that is what a TAKS, a KAKS and a rate all
    // carry, and a decimal column declared with none would be an integer with a
    // point in its name.
    if (spec.type == core::AttrType::Decimal)
        spec.scale = digits.empty() ? 2 : static_cast<std::uint8_t>(digits.as_int());
    if (spec.scale > core::kMaxScale) spec.scale = core::kMaxScale;

    auto made = ctx.transaction().declare_attribute(spec);
    if (!made) {
        ctx.echo(made.error().message);
        co_return;
    }

    ctx.record("kimlik", id);
    ctx.record("tur", type);
    if (!label.empty()) ctx.record("ad", label);
    if (!about.empty()) ctx.record("aciklama", about);
    if (!must.empty()) ctx.record("zorunlu", must);
    if (!catalog.empty()) ctx.record("katalog", catalog);
    if (spec.type == core::AttrType::Decimal) ctx.record("basamak", Value::integer(spec.scale));

    ctx.echo("Sütun tanımlandı: " + column + " (" + core::attr_type_name(spec.type) + ")");
}

} // namespace

KENTOS_COMMAND(column)
{
    return CommandSpec{
        .id       = "core.column",
        .names    = {"SÜTUN", "SUTUN", "COLUMN", "STN"},
        .category = Category::Modify,
        .params =
            {
                Param::text("kimlik", Arity::optional(),
                            "Sütun kimliği; yoksa tanımlı sütunlar listelenir"),
                Param::text("tur", Arity::optional(),
                            "tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod"),
                Param::text("ad", Arity::optional(), "Panelde görünen Türkçe ad"),
                Param::text("aciklama", Arity::optional(), "Tek satırlık açıklama"),
                Param::boolean("zorunlu", Arity::optional(), "Her satır bir değer taşımalı mı"),
                Param::text("katalog", Arity::optional(),
                            "Yalnız 'kod' türü için: katalog kimliği"),
                Param::integer("basamak", Arity::optional(),
                               "Yalnız 'ondalik' için: noktadan sonraki basamak sayısı"),
                Param::boolean("sil", Arity::optional(),
                               "Sütunu ve içindeki bütün değerleri siler"),
            },
        // NOT undoable, and for the same reason a layer is not: the schema is what
        // rows are addressed against, and undoing a declaration would invalidate
        // every row index the journal already holds.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable,
        .summary = "Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler.",
        .run     = &run_column,
    };
}

} // namespace kentos::command
