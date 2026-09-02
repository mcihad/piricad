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
    for (core::AttrId c = 0; c < table.columns(); ++c) {
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
Task<void> run_column(Context& ctx)
{
    const Value id   = ctx.argument("kimlik");
    const Value type = ctx.argument("tur");

    if (id.empty()) {
        list_schema(ctx);
        co_return;
    }
    if (type.empty()) {
        ctx.echo("Kullanım: SÜTUN <kimlik> <tur>. Türler: tam_sayi, uzunluk, evet_hayir, metin.");
        co_return;
    }

    core::AttrSpec spec;
    spec.id             = id.as_text();
    spec.name_tr        = id.as_text();
    const std::string t = type.as_text();

    if (core::turkish_iequals(t, "tam_sayi") || core::turkish_iequals(t, "tam_sayı"))
        spec.type = core::AttrType::Int64;
    else if (core::turkish_iequals(t, "uzunluk"))
        spec.type = core::AttrType::Length;
    else if (core::turkish_iequals(t, "evet_hayir") || core::turkish_iequals(t, "evet_hayır"))
        spec.type = core::AttrType::Bool;
    else if (core::turkish_iequals(t, "metin"))
        spec.type = core::AttrType::Text;
    else {
        ctx.echo("Bilinmeyen öznitelik türü: '" + t +
                 "'. Beklenen: tam_sayi, uzunluk, evet_hayir, metin.");
        co_return;
    }

    auto made = ctx.transaction().declare_attribute(std::move(spec));
    if (!made) {
        ctx.echo(made.error().message);
        co_return;
    }

    ctx.record("kimlik", id);
    ctx.record("tur", type);
    ctx.echo("Sütun tanımlandı: " + id.as_text() + " (" + t + ")");
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
                Param::text("tur", Arity::optional(), "tam_sayi, uzunluk, evet_hayir veya metin"),
            },
        // NOT undoable, and for the same reason a layer is not: the schema is what
        // rows are addressed against, and undoing a declaration would invalidate
        // every row index the journal already holds.
        .undo    = UndoPolicy::None,
        .flags   = Flags::Scriptable,
        .summary = "Belgeye öznitelik sütunu tanımlar ve tanımlı sütunları listeler.",
        .run     = &run_column,
    };
}

} // namespace kentos::command
