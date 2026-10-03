// SPDX-License-Identifier: GPL-3.0-or-later
// core.attribute_calc — ÖZNİTELİKHESAPLA. The field calculator (TODOS G-03).
//
// One expression, one column, many rows, ONE undo step. The expression is the row language of
// `expression.hpp` — the very grammar of the table's filter bar — over the row's own cells and over
// what its geometry says (`$alan`, `$uzunluk`, `$x`, `$y`, `$fid`, `$katman`).
//
// ALL OR NOTHING (Article 1.6). Every row is computed BEFORE any is written; a row whose value
// cannot be made (a text that is no number, a division by zero, a number a whole-number column
// cannot hold) stops the command with the row named, and nothing has changed. The same pass is the
// PREVIEW: `onizle=evet` computes exactly what a run would write, says so, and writes nothing — the
// change summary a person reads before they trust a calculation over a thousand parcels.
//
// WHAT IT WRITES goes through `core::attr_parse`, the one inverse of `attr_display`, so a
// calculated `0,40` is the cell a typed `0,40` would be, and the column's own type and catalogue
// check it.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/expression.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/attribute.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/text.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// THE `$` WORDS A ROW ANSWERS, as the calculator window and the manual list them. `pseudo_column`
/// below answers exactly these (a unit test walks the list through it), so the list cannot name a
/// word the language does not know.
const std::vector<FunctionHelp>& pseudo_table()
{
    static const std::vector<FunctionHelp> kWords = {
        {"$fid", "$fid", "Nesnenin kalıcı kimliği; silinmeyle değişmez."},
        {"$katman", "$katman", "Nesnenin katmanının adı."},
        {"$alan", "$alan", "Kapalı bir nesnenin alanı, metrekare."},
        {"$uzunluk", "$uzunluk", "Nesnenin çevresi ya da uzunluğu, metre."},
        {"$x", "$x", "İlk köşenin X'i (yukarı, kuzey), metre."},
        {"$y", "$y", "İlk köşenin Y'si (sağa, doğu), metre."},
    };
    return kWords;
}

/// What a row's geometry says, for the `$` words. Metres, the unit a surveyor reads; the same words
/// the layout table's computed columns use.
std::optional<std::string> pseudo_column(const core::Document& doc, core::EntityId e,
                                         std::string_view word)
{
    const auto number = [](double v) {
        return std::optional<std::string>(core::format_general(v, 15));
    };
    if (core::turkish_key_equals(word, "$fid"))
        return std::to_string(static_cast<std::uint64_t>(core::raw(doc.key_of(e))));
    if (core::turkish_key_equals(word, "$katman")) {
        const core::Layer* held = doc.layer(doc.entities().layer[e]);
        return held != nullptr ? std::optional<std::string>(held->name) : std::nullopt;
    }
    if (core::turkish_key_equals(word, "$alan"))
        return number(static_cast<double>(doc.entity_area(e)) / 1.0e6);
    if (core::turkish_key_equals(word, "$uzunluk"))
        return number(static_cast<double>(doc.entity_perimeter(e)) / 1.0e3);
    if (core::turkish_key_equals(word, "$x") || core::turkish_key_equals(word, "$y")) {
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
        if (span.count == 0 || doc.geometry().ring_xs(span.first).empty()) return std::nullopt;
        // "Sağa" (Y) is the east value and "yukarı" (X) the north one — the Turkish convention the
        // layout table's `$y` and `$x` already follow.
        const bool east = core::turkish_key_equals(word, "$y");
        return number(core::mm_to_metres(east ? doc.geometry().ring_xs(span.first)[0]
                                              : doc.geometry().ring_ys(span.first)[0]));
    }
    return std::nullopt;
}

/// A number written for `attr_parse` to read back: the whole digits, a point, no grouping.
std::string number_for(double v, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    const double r     = std::round(v * scale) / scale;
    if (r == std::floor(r) && std::abs(r) < 1e15) return std::to_string(static_cast<long long>(r));
    return core::format_general(r, 15);
}

/// The cell an expression's value makes in `spec`'s column, or why it cannot.
core::Result<core::AttrValue> to_cell(const core::AttrSpec& spec, const ExprValue& v)
{
    using K = ExprValue::Kind;
    if (v.is_null()) return core::attr_absent(spec.type);

    std::string text;
    switch (spec.type) {
    case core::AttrType::Int64:
        if (v.kind == K::Number) {
            if (std::abs(v.number - std::round(v.number)) > 1e-9)
                return core::err(core::ErrorCode::InvalidArgument,
                                 "sonuç " + number_for(v.number, 6) + ": '" + spec.id +
                                     "' tam sayı sütunu bir kesir tutamaz (round, floor ya da ceil "
                                     "kullanın)");
            text = number_for(v.number, 0);
        } else {
            text = v.as_text();
        }
        break;
    case core::AttrType::Length:
        // The calculation is in METRES, as the table shows the column; the store keeps whole
        // millimetres and `attr_parse` reads a whole number of them. A TEXT value is taken as it
        // is, in millimetres, exactly as `ÖZNİTELİK deger=` reads it.
        text = v.kind == K::Number ? std::to_string(std::llround(v.number * 1000.0)) : v.as_text();
        break;
    case core::AttrType::Decimal:
        text = v.kind == K::Number ? number_for(v.number, spec.scale) : v.as_text();
        break;
    case core::AttrType::Bool:
        text = v.kind == K::Number ? (v.number != 0.0 ? "evet" : "hayır") : v.as_text();
        break;
    case core::AttrType::Text:
    case core::AttrType::CodeRef:
    case core::AttrType::Date: text = v.as_text(); break;
    }
    return core::attr_parse(spec, text);
}

struct Planned
{
    core::EntityId entity{core::kNoEntity};
    core::AttrValue value;
};

Task<void> run_calc(Context& ctx)
{
    Bus& bus                     = ctx.session().bus();
    const core::Document& doc    = ctx.document();
    const core::AttrTable& table = doc.attributes();

    std::vector<std::string> known;
    for (std::size_t c = 0; c < table.columns(); ++c)
        known.push_back(table.column(static_cast<core::AttrId>(c))->spec().id);

    auto target = co_await ctx.text("ad", "Hesaplanıp yazılacak sütun", known);
    if (!target || target->empty()) co_return;
    const core::AttrId col = table.find(*target);
    if (col == core::kNoAttr) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Bilinmeyen sütun: '" + *target + "'. " +
                       (known.empty() ? std::string("Belgede hiç öznitelik sütunu yok; önce SÜTUN "
                                                    "kimlik=<ad> tur=<tür> ile tanımlayın.")
                                      : "Tanımlı sütunlar: " +
                                            [&known] {
                                                std::string names;
                                                for (const std::string& k : known)
                                                    names += (names.empty() ? "" : ", ") + k;
                                                return names;
                                            }() +
                                            ". Yenisi için SÜTUN kimlik=<ad> tur=<tür>."));
        co_return;
    }
    const core::AttrSpec& spec = table.column(col)->spec();

    auto text = co_await ctx.text("ifade", "İfade");
    if (!text || text->empty()) co_return;
    auto expression = Expression::compile(*text);
    if (!expression) {
        ctx.refuse(expression.error());
        co_return;
    }

    std::optional<Expression> filter;
    if (const Value f = ctx.argument("filtre"); !f.empty() && !f.as_text().empty()) {
        auto read = Expression::compile(f.as_text());
        if (!read) {
            ctx.refuse(read.error().code, "filtre: " + read.error().message);
            co_return;
        }
        filter = std::move(read.value());
    }

    // ---- WHICH ROWS: named, else a layer's, else the selection, else the active layer's ----
    std::vector<core::EntityId> rows;
    std::string scope;
    const auto in_block = [&doc](core::EntityId e) {
        return (doc.entities().flags[e] & core::FlagInBlock) != 0;
    };
    if (const Value named = ctx.argument("nesneler"); !named.empty()) {
        for (const std::int64_t raw : named.as_ids()) {
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
            if (e == core::kNoEntity || !doc.alive(e)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Bilinmeyen nesne: " + std::to_string(raw) +
                               ". Nesne kimliklerini SEÇ ile görebilirsiniz.");
                co_return;
            }
            rows.push_back(e);
        }
        scope = "verilen " + std::to_string(rows.size()) + " nesne";
    } else {
        core::LayerId layer = core::kNoLayer;
        if (const Value l = ctx.argument("katman"); !l.empty()) {
            layer = doc.find_layer(l.as_text());
            if (layer == core::kNoLayer) {
                ctx.refuse(core::ErrorCode::NotFound, "Bilinmeyen katman: '" + l.as_text() + "'.");
                co_return;
            }
        } else if (!bus.selection().empty()) {
            for (const core::EntityKey k : bus.selection().keys()) {
                const core::EntityId e = doc.slot_of(k);
                if (e != core::kNoEntity && doc.alive(e)) rows.push_back(e);
            }
            scope = "seçim (" + std::to_string(rows.size()) + " nesne)";
        } else {
            layer = bus.active_layer();
        }
        if (layer != core::kNoLayer) {
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && !in_block(e) && doc.entities().layer[e] == layer)
                    rows.push_back(e);
            scope = "'" + doc.layer(layer)->name + "' katmanı (" + std::to_string(rows.size()) +
                    " nesne)";
        }
    }
    if (rows.empty()) {
        ctx.refuse(
            core::ErrorCode::InvalidArgument,
            "Hesaplanacak satır yok: " + (scope.empty() ? std::string("kapsam boş") : scope) +
                ". nesneler=, katman= ya da bir seçimle kapsamı söyleyin.");
        co_return;
    }

    // ---- EVERY ROW IS COMPUTED BEFORE ANY IS WRITTEN ----
    core::EntityId current   = core::kNoEntity;
    const FieldReader reader = [&](std::string_view name) -> std::optional<std::string> {
        if (!name.empty() && name.front() == '$') return pseudo_column(doc, current, name);
        const core::AttrId held = table.find(name);
        if (held == core::kNoAttr) return std::nullopt;
        const auto cell = doc.attribute(held, current);
        if (!cell || !cell.value().present) return std::nullopt;
        // The point mark and no grouping: the same text the table's filter reads.
        return core::attr_display(cell.value(), core::DecimalMark::Point);
    };

    std::vector<Planned> plan;
    std::size_t same = 0, emptied = 0, unmatched = 0;

    struct Sample
    {
        std::uint64_t key;
        std::string before, after;
    };

    std::vector<Sample> samples;

    const auto where = [&](core::EntityId e) {
        const core::Layer* held = doc.layer(doc.entities().layer[e]);
        return "nesne " + std::to_string(static_cast<std::uint64_t>(core::raw(doc.key_of(e)))) +
               (held != nullptr ? " ('" + held->name + "' katmanı)" : std::string());
    };

    for (const core::EntityId e : rows) {
        current = e;
        if (!core::attr_applies_to(spec, doc.layer(doc.entities().layer[e]) != nullptr
                                             ? doc.layer(doc.entities().layer[e])->name
                                             : std::string())) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       where(e) + ": '" + spec.id + "' sütunu bu katmana tanımlı değil (sütun '" +
                           spec.layer +
                           "' katmanına özel). Satır kapsamını sütunun katmanıyla sınırlayın.");
            co_return;
        }
        if (filter) {
            auto matched = filter->evaluate(reader);
            if (!matched) {
                ctx.refuse(core::ErrorCode::ValidationFailed,
                           where(e) + ": filtre değerlendirilemedi: " + matched.error().message);
                co_return;
            }
            if (!matched.value().truthy()) {
                ++unmatched;
                continue;
            }
        }
        auto value = expression.value().evaluate(reader);
        if (!value) {
            ctx.refuse(core::ErrorCode::ValidationFailed,
                       where(e) + ": " + value.error().message + " (ifade: " + *text + ")");
            co_return;
        }
        auto cell = to_cell(spec, value.value());
        if (!cell) {
            ctx.refuse(cell.error().code, where(e) + ": " + cell.error().message);
            co_return;
        }
        const auto had = doc.attribute(col, e);
        if (had && had.value() == cell.value()) {
            ++same;
            continue;
        }
        if (!cell.value().present) ++emptied;
        if (samples.size() < 5)
            samples.push_back({static_cast<std::uint64_t>(core::raw(doc.key_of(e))),
                               had && had.value().present
                                   ? core::attr_display(had.value(), core::DecimalMark::Point)
                                   : std::string("boş"),
                               cell.value().present
                                   ? core::attr_display(cell.value(), core::DecimalMark::Point)
                                   : std::string("boş")});
        plan.push_back({e, std::move(cell.value())});
    }

    const bool preview = ctx.argument("onizle").as_bool();
    if (!preview) {
        for (const Planned& p : plan)
            if (auto st = ctx.transaction().set_attribute(col, p.entity, p.value); !st) {
                ctx.refuse(st.error()); // the bus rolls back every row already written
                co_return;
            }
    }

    // ---- WHAT IT DID (or WOULD DO), as a change summary ----
    std::string said = preview ? "Önizleme (hiçbir şey yazılmadı): " : "Hesaplandı: ";
    said += spec.id + " = " + *text + "\n  kapsam: " + scope + "\n  " +
            std::to_string(plan.size()) + " satır " + (preview ? "değişecek" : "değişti") + " · " +
            std::to_string(same) + " aynı kaldı · " + std::to_string(emptied) + " boşaltıldı";
    if (filter) said += " · " + std::to_string(unmatched) + " süzgece uymadı";
    for (const Sample& s : samples)
        said += "\n  [" + std::to_string(s.key) + "] " + s.before + " → " + s.after;
    if (plan.size() > samples.size())
        said += "\n  … ve " + std::to_string(plan.size() - samples.size()) + " satır daha";
    ctx.echo(said);

    core::Json report;
    report.set("sutun", core::Json::string(spec.id));
    report.set("ifade", core::Json::string(*text));
    report.set("kapsam", core::Json::string(scope));
    report.set("onizleme", core::Json::boolean(preview));
    report.set("satir", core::Json::integer(static_cast<std::int64_t>(rows.size())));
    report.set("degisen", core::Json::integer(static_cast<std::int64_t>(plan.size())));
    report.set("ayni", core::Json::integer(static_cast<std::int64_t>(same)));
    report.set("bosaltilan", core::Json::integer(static_cast<std::int64_t>(emptied)));
    report.set("suzgece_uymayan", core::Json::integer(static_cast<std::int64_t>(unmatched)));
    core::Json examples = core::Json::array({});
    for (const Sample& s : samples) {
        core::Json one;
        one.set("nesne", core::Json::integer(static_cast<std::int64_t>(s.key)));
        one.set("eski", core::Json::string(s.before));
        one.set("yeni", core::Json::string(s.after));
        examples.push(std::move(one));
    }
    report.set("ornekler", std::move(examples));
    ctx.report(std::move(report));

    // WHAT A REPLAY NEEDS to find the same rows: the column by its canonical id, the expression,
    // and the scope as it was RESOLVED — a selection is session state and is never journalled
    // (model.md R43), so it is written down as the objects it held.
    ctx.record("ad", Value::text(spec.id));
    ctx.record("ifade", Value::text(*text));
    if (filter) ctx.record("filtre", ctx.argument("filtre"));
    if (const Value named = ctx.argument("nesneler"); !named.empty()) {
        ctx.record("nesneler", named);
    } else if (const Value l = ctx.argument("katman"); !l.empty()) {
        ctx.record("katman", l);
    } else if (!bus.selection().empty()) {
        Value::Ints ids;
        for (const core::EntityId e : rows)
            ids.push_back(static_cast<std::int64_t>(core::raw(doc.key_of(e))));
        ctx.record("nesneler", Value::ids(ids));
    } else {
        ctx.record("katman", Value::text(doc.layer(bus.active_layer())->name));
    }
    if (preview) ctx.record("onizle", Value::boolean(true));
}

} // namespace

std::vector<FunctionHelp> expression_pseudo_columns()
{
    return pseudo_table();
}

PIRICAD_COMMAND(attribute_calc)
{
    return CommandSpec{
        .id       = "core.attribute_calc",
        .names    = {"ÖZNİTELİKHESAPLA", "OZNITELIKHESAPLA", "ALANHESAPLA", "FIELDCALC", "ÖHESAPLA",
                     "OHESAPLA"},
        .title    = "Alan Hesaplayıcı",
        .category = Category::Modify,
        .params =
            {
                Param::text("ad", Arity::exactly(1), "Hesaplanıp yazılacak sütunun kimliği")
                    .en("name"),
                Param::text("ifade", Arity::exactly(1),
                            "Hesaplanacak ifade; sütunlar \"çift\", metinler 'tek' tırnakta. "
                            "Örnek: round(\"alan_m2\" * 0.4, 2)")
                    .en("expression"),
                Param::text("katman", Arity::optional(),
                            "Bu katmanın bütün satırları; yoksa seçim, o da boşsa aktif katman")
                    .en("layer"),
                Param::text("filtre", Arity::optional(),
                            "Yalnız bu ifadenin doğru çıktığı satırlar (tablonun süzme ifadesiyle "
                            "aynı dil)")
                    .en("filter"),
                Param::boolean("onizle", Arity::optional(),
                               "Hiçbir şey yazma: neyin değişeceğini söyle; varsayılan hayır")
                    .en("preview"),
                Param{
                    "nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                    "Yalnız bu nesnelerin kalıcı kimlikleri; verilirse katman ve seçim yok sayılır"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir ifadeyi satırlar üzerinde hesaplayıp bir öznitelik sütununa yazar; tek "
                   "geri alma adımı, önizlemesi ve değişim özetiyle.",
        .run     = &run_calc,
        .effect  = Effect::Query | Effect::DocumentEdit,
    };
}

} // namespace piricad::command
