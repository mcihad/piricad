// SPDX-License-Identifier: GPL-3.0-or-later
// core.feature_class — KALEM, core.feature_class_bind — KALEMBAĞLA,
// core.feature_class_check — KALEMDENETİM (TODOS G-04).
//
// The three commands are the three things a digitiser does with a class: PICK it and draw, BRING
// EXISTING CAD objects under it, and ASK whether everything on its layer still is what it says. The
// definitions are a data package (`feature_classes.hpp`); what these commands add to the document
// is a layer that follows a class, the columns the class declares, and the active layer.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/feature_classes.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/text.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace piricad::command {
namespace {

using core::ErrorCode;

/// "a, b ve c" for a sentence.
std::string joined(const std::vector<std::string>& parts, const char* last = ", ")
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        out += (i == 0 ? "" : last) + parts[i];
    return out;
}

/// The names of the classes a package holds, for a refusal that has to say what would have worked.
std::string class_names(const FeatureClassCatalog& catalog)
{
    std::vector<std::string> names;
    for (const FeatureClass& c : catalog.classes)
        names.push_back(c.id);
    return joined(names);
}

/// The Turkish word for a column type, as `SÜTUN tur=` takes it.
const char* type_word(core::AttrType t)
{
    switch (t) {
    case core::AttrType::Int64: return "tam_sayi";
    case core::AttrType::Length: return "uzunluk";
    case core::AttrType::Bool: return "evet_hayir";
    case core::AttrType::Text: return "metin";
    case core::AttrType::CodeRef: return "kod";
    case core::AttrType::Decimal: return "ondalik";
    case core::AttrType::Date: return "tarih";
    }
    return "metin";
}

/// What materialising a class would do to the drawing, worked out WITHOUT doing it: a preview
/// writes nothing, and a refusal must come before the first change (a column once declared is not
/// taken back).
struct ClassPlan
{
    const FeatureClass* cls{nullptr};
    core::LayerId layer{core::kNoLayer}; ///< the layer when it exists already
    bool new_layer{false};
    std::size_t objects_there{0}; ///< objects already on an existing layer the class takes over

    enum class Action : std::uint8_t { Declare, Keep, Widen };

    struct Column
    {
        const ClassField* field{nullptr};
        Action action{Action::Keep};
    };

    std::vector<Column> columns;

    std::size_t declared() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            columns, [](const Column& c) { return c.action == Action::Declare; }));
    }
};

core::Result<ClassPlan> plan_class(const Bus& bus, const FeatureClassCatalog& catalog,
                                   const FeatureClass& cls)
{
    const core::Document& doc = bus.document();
    ClassPlan plan;
    plan.cls       = &cls;
    plan.layer     = doc.find_layer(cls.layer);
    plan.new_layer = plan.layer == core::kNoLayer;

    if (!plan.new_layer) {
        const core::Layer* held = doc.layer(plan.layer);
        if (!held->feature_class.empty() &&
            (reference_package(held->feature_class) != catalog.package_id ||
             reference_class(held->feature_class) != cls.id))
            return core::err(ErrorCode::ValidationFailed,
                             "'" + held->name + "' katmanı zaten '" +
                                 reference_class(held->feature_class) + "' sınıfını izliyor. '" +
                                 cls.name +
                                 "' sınıfı bu katmanı kullanıyor; önce ötekini bırakın ya da "
                                 "sınıfın katman adını paketinizde değiştirin.");
        if (held->feature_class.empty())
            for (core::EntityId e = 0; e < doc.entities().size(); ++e)
                if (doc.alive(e) && doc.entities().layer[e] == plan.layer) ++plan.objects_there;
    }

    const core::AttrTable& table = doc.attributes();
    for (const ClassField& f : cls.fields) {
        ClassPlan::Column column;
        column.field             = &f;
        const core::AttrId found = table.find(f.id);
        if (found == core::kNoAttr) {
            column.action = ClassPlan::Action::Declare;
        } else {
            const core::AttrSpec& have = table.column(found)->spec();
            if (have.type != f.type)
                return core::err(ErrorCode::ValidationFailed,
                                 "'" + f.id + "' sütunu belgede " + type_word(have.type) + ", '" +
                                     cls.name + "' sınıfı onu " + type_word(f.type) +
                                     " ister. Sütunun türü değişmez; paketinizde alanın kimliğini "
                                     "değiştirin ya da belgedeki sütunu başka bir adla yeniden "
                                     "tanımlayın.");
            const bool mine = have.layer.empty() || core::turkish_key_equals(have.layer, cls.layer);
            column.action   = mine ? ClassPlan::Action::Keep : ClassPlan::Action::Widen;
        }
        plan.columns.push_back(column);
    }
    if (bus.in_batch() && std::ranges::any_of(plan.columns, [](const ClassPlan::Column& c) {
            return c.action == ClassPlan::Action::Widen;
        }))
        return core::err(ErrorCode::Unsupported,
                         "'" + cls.name +
                             "' sınıfı başka bir katmanın sütununu ortak yapacak; bu "
                             "değişiklik bir betiğin ya da toplu işin içinde geri alınamaz. KALEM "
                             "komutunu betikten önce ayrıca çalıştırın.");
    return plan;
}

/// Does `plan`. The caller has checked there is nothing left to refuse.
core::Result<core::LayerId> apply_plan(Context& ctx, const FeatureClassCatalog& catalog,
                                       const ClassPlan& plan)
{
    Transaction& tx       = ctx.transaction();
    const FeatureClass& c = *plan.cls;

    core::LayerId layer = plan.new_layer ? tx.ensure_layer(c.layer) : plan.layer;

    if (plan.new_layer) {
        if (!c.group.empty())
            if (auto st = tx.set_layer_group(layer, c.group); !st) return st.error();
        core::Appearance look = ctx.document().layer(layer)->appearance;
        bool restyled         = false;
        if (c.has_colour) {
            look.rgba = c.colour_rgba;
            restyled  = true;
        }
        if (c.width_um > 0) {
            look.width_um = c.width_um;
            restyled      = true;
        }
        if (c.has_fill && c.geometry == ClassGeometry::Area) {
            look.fill_rgba = c.fill_rgba;
            restyled       = true;
        }
        if (restyled)
            if (auto st = tx.set_layer_appearance(layer, look); !st) return st.error();
        // AN AREA'S FILL IS A SYMBOL LAYER, not a colour on the stroke: the scene paints an
        // interior only from a fill layer, so the class's `dolgu` is a fill layer under its
        // outline.
        if (c.has_fill && c.geometry == ClassGeometry::Area) {
            core::SymbolLayer fill;
            fill.type            = core::SymbolLayerType::SimpleFill;
            fill.look            = look;
            fill.look.fill_rgba  = c.fill_rgba;
            fill.look.src_fill   = core::Source::Explicit;
            fill.look.src_colour = core::Source::Explicit;
            core::SymbolLayer edge;
            edge.type = core::SymbolLayerType::SimpleLine;
            edge.look = look;
            core::Symbol symbol;
            symbol.layers.push_back(fill);
            symbol.layers.push_back(edge);
            if (auto st = tx.set_layer_style(layer, tx.intern_symbol(symbol)); !st)
                return st.error();
        }
        if (!c.layer_description.empty()) {
            core::LayerProps props = ctx.document().layer(layer)->props();
            props.description      = c.layer_description;
            if (auto st = tx.set_layer_props(layer, props); !st) return st.error();
        }
    }

    for (const ClassPlan::Column& column : plan.columns) {
        const ClassField& f = *column.field;
        if (column.action == ClassPlan::Action::Declare) {
            core::AttrSpec spec;
            spec.id         = f.id;
            spec.name_tr    = f.name;
            spec.summary_tr = f.summary;
            spec.type       = f.type;
            spec.required   = f.required;
            // A FIELD SEVERAL CLASSES OF THE PACKAGE CARRY (`sinif_kodu`) is the project's from the
            // start; one only this class carries belongs to its layer, so another layer's inspector
            // stays free of it. Deciding this when the column is made means no class ever has to
            // widen a column afterwards, which a batch could not undo.
            const bool shared =
                std::ranges::any_of(catalog.classes, [&](const FeatureClass& other) {
                    return other.id != c.id &&
                           std::ranges::any_of(other.fields,
                                               [&](const ClassField& g) { return g.id == f.id; });
                });
            spec.layer = shared ? std::string() : c.layer;
            spec.scale = f.type == core::AttrType::Decimal ? f.scale : std::uint8_t{0};
            if (auto made = tx.declare_attribute(spec); !made) return made.error();
        } else if (column.action == ClassPlan::Action::Widen) {
            // THE DRAWING ALREADY HAD THE COLUMN FOR ANOTHER LAYER: it becomes the project's.
            core::AttrSpec next =
                ctx.document().attributes().column(ctx.document().attributes().find(f.id))->spec();
            next.layer.clear();
            if (auto st = tx.amend_attribute(f.id, next); !st) return st.error();
        }
    }

    if (auto st = tx.set_layer_feature_class(layer, catalog.reference_to(c)); !st)
        return st.error();
    return layer;
}

/// The number of characters (not bytes) in a UTF-8 text, so a column of Turkish words lines up.
std::size_t glyphs(const std::string& text)
{
    std::size_t n = 0;
    for (const char c : text)
        if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++n;
    return n;
}

/// `text` followed by spaces up to `width` characters.
std::string padded(std::string text, std::size_t width)
{
    const std::size_t have = glyphs(text);
    if (have < width) text.append(width - have, ' ');
    return text;
}

/// The one line a class is listed with.
std::string listed(const FeatureClass& c, bool active)
{
    std::string line = "  " + padded(c.id, 13) + padded(c.name, 13) +
                       padded(class_geometry_noun(c.geometry), 13) + "katman " + c.layer;
    if (active) line += "   ← etkin";
    return line;
}

core::Json report_of_plan(const FeatureClassCatalog& catalog, const ClassPlan& plan)
{
    const FeatureClass& c = *plan.cls;
    core::Json r;
    r.set("sinif", core::Json::string(c.id));
    r.set("ad", core::Json::string(c.name));
    r.set("paket", core::Json::string(catalog.package_id + "@" + catalog.package_version));
    r.set("geometri", core::Json::string(class_geometry_word(c.geometry)));
    r.set("katman", core::Json::string(c.layer));
    r.set("yeni_katman", core::Json::boolean(plan.new_layer));
    r.set("katmandaki_nesne", core::Json::integer(static_cast<std::int64_t>(plan.objects_there)));
    core::Json fields = core::Json::array({});
    for (const ClassPlan::Column& column : plan.columns) {
        core::Json f;
        f.set("kimlik", core::Json::string(column.field->id));
        f.set("tur", core::Json::string(type_word(column.field->type)));
        f.set("varsayilan", core::Json::string(column.field->default_text));
        f.set("sutun", core::Json::string(column.action == ClassPlan::Action::Declare ? "yeni"
                                          : column.action == ClassPlan::Action::Widen ? "ortak"
                                                                                      : "var"));
        fields.push(std::move(f));
    }
    r.set("alanlar", std::move(fields));
    return r;
}

// =============================================================================================
// KALEM
// =============================================================================================

Task<void> run_pen(Context& ctx)
{
    Bus& bus           = ctx.session().bus();
    const auto catalog = bus.feature_classes();
    if (catalog == nullptr) {
        ctx.refuse(ErrorCode::NotFound, bus.feature_classes_error());
        co_return;
    }

    const Value word = ctx.argument("ad");

    // ---- no class named: list the package ----
    if (word.empty() || word.as_text().empty()) {
        const core::Layer* active = ctx.document().layer(bus.active_layer());
        const FeatureClass* now =
            active != nullptr ? catalog->of_reference(active->feature_class) : nullptr;
        std::string said = "Sayısallaştırma sınıfları (" + catalog->package_id + " " +
                           catalog->package_version + "):";
        core::Json r;
        core::Json rows = core::Json::array({});
        for (const FeatureClass& c : catalog->classes) {
            said += "\n" + listed(c, &c == now);
            core::Json row;
            row.set("sinif", core::Json::string(c.id));
            row.set("ad", core::Json::string(c.name));
            row.set("geometri", core::Json::string(class_geometry_word(c.geometry)));
            row.set("katman", core::Json::string(c.layer));
            row.set("etkin", core::Json::boolean(&c == now));
            rows.push(std::move(row));
        }
        said += "\nSeçmek için: KALEM <sınıf>   (örnek: KALEM " + catalog->classes.front().id + ")";
        r.set("paket", core::Json::string(catalog->package_id + "@" + catalog->package_version));
        r.set("siniflar", std::move(rows));
        ctx.report(std::move(r));
        ctx.echo(said);
        co_return;
    }

    const FeatureClass* cls = catalog->find(word.as_text());
    if (cls == nullptr) {
        ctx.refuse(ErrorCode::NotFound, "Bilinmeyen sınıf: '" + word.as_text() +
                                            "'. Tanımlı sınıflar: " + class_names(*catalog) +
                                            ". Listeyi görmek için argümansız KALEM yazın.");
        co_return;
    }

    auto made_plan = plan_class(bus, *catalog, *cls);
    if (!made_plan) {
        ctx.refuse(made_plan.error());
        co_return;
    }
    const ClassPlan& planned = made_plan.value();
    auto layer               = apply_plan(ctx, *catalog, planned);
    if (!layer) {
        ctx.refuse(layer.error());
        co_return;
    }
    bus.set_active_layer(layer.value());
    ctx.record("ad", word);

    std::vector<std::string> defaults;
    for (const ClassField& f : cls->fields)
        if (!f.default_text.empty()) defaults.push_back(f.id + "=" + f.default_text);
    std::string said = "Kalem: " + cls->name + " — " + class_geometry_noun(cls->geometry) +
                       ", katman " + cls->layer + (planned.new_layer ? " (yeni)" : "") + ", " +
                       std::to_string(cls->fields.size()) + " alan (" +
                       std::to_string(planned.declared()) + " yeni sütun)";
    if (!defaults.empty()) said += "\n  Başlangıç değerleri: " + joined(defaults, " · ");
    said += "\n  Şimdi çizdiğiniz her " + std::string(class_geometry_noun(cls->geometry)) + " " +
            cls->layer + " katmanına gider ve bu değerlerle başlar; " +
            "sınıfa uymayan şekil reddedilir.";
    if (planned.objects_there > 0)
        said += "\n  Uyarı: " + cls->layer + " katmanında zaten " +
                std::to_string(planned.objects_there) +
                " nesne var; denetlenmediler. KALEMDENETİM bakar.";
    ctx.report(report_of_plan(*catalog, planned));
    ctx.echo(said);
}

// =============================================================================================
// KALEMBAĞLA
// =============================================================================================

Task<void> run_bind(Context& ctx)
{
    Bus& bus           = ctx.session().bus();
    const auto catalog = bus.feature_classes();
    if (catalog == nullptr) {
        ctx.refuse(ErrorCode::NotFound, bus.feature_classes_error());
        co_return;
    }
    const core::Document& doc = ctx.document();

    std::vector<std::string> names;
    for (const FeatureClass& c : catalog->classes)
        names.push_back(c.id);
    auto word = co_await ctx.text("ad", "Hangi sınıfa bağlansın", names);
    if (!word || word->empty()) co_return;
    const FeatureClass* cls = catalog->find(*word);
    if (cls == nullptr) {
        ctx.refuse(ErrorCode::NotFound, "Bilinmeyen sınıf: '" + *word +
                                            "'. Tanımlı sınıflar: " + class_names(*catalog) + ".");
        co_return;
    }

    // ---- WHICH OBJECTS: named, else a layer's, else the selection ----
    std::vector<core::EntityId> rows;
    std::string scope;
    if (const Value named = ctx.argument("nesneler"); !named.empty()) {
        for (const std::int64_t raw : named.as_ids()) {
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
            if (e == core::kNoEntity || !doc.alive(e)) {
                ctx.refuse(ErrorCode::NotFound, "Bilinmeyen nesne: " + std::to_string(raw) +
                                                    ". Nesne kimliklerini SEÇ ile görebilirsiniz.");
                co_return;
            }
            rows.push_back(e);
        }
        scope = "verilen " + std::to_string(rows.size()) + " nesne";
    } else if (const Value l = ctx.argument("katman"); !l.empty()) {
        const core::LayerId layer = doc.find_layer(l.as_text());
        if (layer == core::kNoLayer) {
            ctx.refuse(ErrorCode::NotFound, "Bilinmeyen katman: '" + l.as_text() + "'.");
            co_return;
        }
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().layer[e] == layer &&
                (doc.entities().flags[e] & core::FlagInBlock) == 0)
                rows.push_back(e);
        scope =
            "'" + doc.layer(layer)->name + "' katmanı (" + std::to_string(rows.size()) + " nesne)";
    } else if (!bus.selection().empty()) {
        for (const core::EntityKey k : bus.selection().keys()) {
            const core::EntityId e = doc.slot_of(k);
            if (e != core::kNoEntity && doc.alive(e)) rows.push_back(e);
        }
        scope = "seçim (" + std::to_string(rows.size()) + " nesne)";
    }
    if (rows.empty()) {
        ctx.refuse(ErrorCode::InvalidArgument,
                   "Bağlanacak nesne yok. nesneler=, katman= ya da bir seçimle söyleyin.");
        co_return;
    }

    auto made_plan = plan_class(bus, *catalog, *cls);
    if (!made_plan) {
        ctx.refuse(made_plan.error());
        co_return;
    }
    const ClassPlan& planned = made_plan.value();

    // ---- THE FIELD MAPPING: `eski_sutun:sinif_alani`, the same kind of thing or a text ----
    struct Mapping
    {
        core::AttrId from{core::kNoAttr};
        core::AttrId to{core::kNoAttr};
        const ClassField* field{nullptr};
    };

    std::vector<Mapping> mappings;
    std::vector<std::string> mapping_words;
    const Value mapped = ctx.argument("esle");
    for (const std::string& pair : mapped.as_texts()) {
        const std::size_t colon = pair.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= pair.size()) {
            ctx.refuse(ErrorCode::InvalidArgument,
                       "esle 'eski_sutun:sinif_alani' biçiminde yazılır; verilen: '" + pair +
                           "'.\n  Örnek: KALEMBAĞLA ad=bina esle=kat:kat_sayisi");
            co_return;
        }
        const std::string from    = pair.substr(0, colon);
        const std::string to      = pair.substr(colon + 1);
        const core::AttrId source = doc.attributes().find(from);
        if (source == core::kNoAttr) {
            ctx.refuse(ErrorCode::NotFound, "esle: belgede '" + from + "' sütunu yok.");
            co_return;
        }
        const auto field =
            std::ranges::find_if(cls->fields, [&to](const ClassField& f) { return f.id == to; });
        if (field == cls->fields.end()) {
            std::vector<std::string> own;
            for (const ClassField& f : cls->fields)
                own.push_back(f.id);
            ctx.refuse(ErrorCode::NotFound, "esle: '" + cls->name + "' sınıfının '" + to +
                                                "' alanı yok. Alanları: " + joined(own) + ".");
            co_return;
        }
        const core::AttrType from_type = doc.attributes().column(source)->spec().type;
        if (from_type != field->type && field->type != core::AttrType::Text) {
            ctx.refuse(ErrorCode::ValidationFailed,
                       "esle: '" + from + "' (" + type_word(from_type) + ") '" + to + "' (" +
                           type_word(field->type) +
                           ") alanına eşlenemez; aynı türde ya da metin alanına eşleyin.");
            co_return;
        }
        Mapping m;
        m.from  = source;
        m.field = &*field;
        mappings.push_back(m);
        mapping_words.push_back(from + " → " + to);
    }

    // ---- WHAT FITS: sorted into those that can come, those already there and those that cannot
    // ----
    const core::LayerId target = planned.layer;
    std::vector<core::EntityId> coming;
    std::size_t already = 0;
    std::vector<std::string> unfit; // the first few, for the message
    std::size_t unfit_total  = 0;
    std::size_t locked_total = 0;
    std::map<std::string, std::size_t> unfit_by;
    for (const core::EntityId e : rows) {
        if (target != core::kNoLayer && doc.entities().layer[e] == target) {
            ++already;
            continue;
        }
        const ClassGeometry got = geometry_class_of(doc, e);
        if (got != cls->geometry) {
            ++unfit_total;
            ++unfit_by[class_geometry_noun(got)];
            if (unfit.size() < 5)
                unfit.push_back(std::to_string(core::raw(doc.key_of(e))) + " (" +
                                class_geometry_noun(got) + ")");
            continue;
        }
        if (!ctx.transaction().may_change(e)) {
            ++locked_total;
            continue;
        }
        coming.push_back(e);
    }

    const bool preview = ctx.argument("onizle").as_bool();
    std::size_t copied = 0;
    if (!preview) {
        if (coming.empty()) {
            ctx.refuse(ErrorCode::ValidationFailed,
                       "Sınıfa bağlanabilecek nesne yok: " +
                           (unfit_total != 0
                                ? std::to_string(unfit_total) + " nesne '" + cls->name +
                                      "' sınıfının " + class_geometry_noun(cls->geometry) +
                                      " şekline uymuyor"
                                : std::string("hepsi zaten katmanda ya da kilitli")) +
                           ". Önizlemek için onizle=evet.");
            co_return;
        }
        auto layer = apply_plan(ctx, *catalog, planned);
        if (!layer) {
            ctx.refuse(layer.error());
            co_return;
        }
        for (Mapping& m : mappings)
            m.to = ctx.document().attributes().find(m.field->id);

        for (const core::EntityId e : coming) {
            if (auto st = ctx.transaction().set_entity_layer(e, layer.value()); !st) {
                ctx.refuse(st.error());
                co_return;
            }
            for (const Mapping& m : mappings) {
                const auto from = ctx.document().attribute(m.from, e);
                if (!from || !from.value().present) continue; // nothing there: nothing to carry
                core::AttrValue value = from.value();
                if (m.field->type == core::AttrType::Text &&
                    ctx.document().attributes().column(m.from)->spec().type != core::AttrType::Text)
                    value =
                        core::attr_text(core::attr_display(from.value(), core::DecimalMark::Point));
                if (auto st = ctx.transaction().set_attribute(m.to, e, value); !st) {
                    ctx.refuse(st.error());
                    co_return;
                }
                ++copied;
            }
        }
        ctx.record("ad", Value::text(*word));
    }

    // ---- THE SENTENCES ----
    std::string said = preview ? "Önizleme (hiçbir şey yazılmadı): " : "Bağlandı: ";
    said += std::to_string(coming.size()) + " nesne '" + cls->name + "' sınıfına " +
            (preview ? "bağlanacak" : "bağlandı") + " (katman " + cls->layer +
            (planned.new_layer ? ", yeni" : "") + ")\n  kapsam: " + scope;
    if (already != 0) said += "\n  Zaten katmanda: " + std::to_string(already);
    if (unfit_total != 0) {
        std::vector<std::string> kinds;
        for (const auto& [noun, count] : unfit_by)
            kinds.push_back(std::to_string(count) + " " + noun);
        said += "\n  Uymayan, atlandı: " + joined(kinds) + " — sınıf " +
                class_geometry_noun(cls->geometry) + " ister. İlk nesneler: " + joined(unfit);
    }
    if (locked_total != 0)
        said += "\n  Kilitli ya da salt görüntü katmanda, atlandı: " + std::to_string(locked_total);
    if (!mapping_words.empty())
        said += "\n  Alan eşlemesi: " + joined(mapping_words) +
                (preview ? "" : " (" + std::to_string(copied) + " hücre taşındı)");
    std::vector<std::string> defaults;
    for (const ClassField& f : cls->fields)
        if (!f.default_text.empty()) defaults.push_back(f.id + "=" + f.default_text);
    if (!defaults.empty())
        said += "\n  Boş kalan alanlar şunlarla başlar: " + joined(defaults, " · ");
    ctx.echo(said);

    core::Json r = report_of_plan(*catalog, planned);
    r.set("onizleme", core::Json::boolean(preview));
    r.set("kapsam", core::Json::string(scope));
    r.set("baglanan", core::Json::integer(static_cast<std::int64_t>(coming.size())));
    r.set("zaten", core::Json::integer(static_cast<std::int64_t>(already)));
    r.set("uymayan", core::Json::integer(static_cast<std::int64_t>(unfit_total)));
    r.set("kilitli", core::Json::integer(static_cast<std::int64_t>(locked_total)));
    r.set("tasinan_hucre", core::Json::integer(static_cast<std::int64_t>(copied)));
    ctx.report(std::move(r));
}

// =============================================================================================
// KALEMDENETİM
// =============================================================================================

Task<void> run_check(Context& ctx)
{
    Bus& bus           = ctx.session().bus();
    const auto catalog = bus.feature_classes();
    if (catalog == nullptr) {
        ctx.refuse(ErrorCode::NotFound, bus.feature_classes_error());
        co_return;
    }
    const core::Document& doc = ctx.document();

    // ---- WHICH LAYERS: the class named, the layer named, or every layer that follows a class ----
    std::vector<core::LayerId> layers;
    if (const Value w = ctx.argument("ad"); !w.empty() && !w.as_text().empty()) {
        const FeatureClass* cls = catalog->find(w.as_text());
        if (cls == nullptr) {
            ctx.refuse(ErrorCode::NotFound, "Bilinmeyen sınıf: '" + w.as_text() +
                                                "'. Tanımlı sınıflar: " + class_names(*catalog) +
                                                ".");
            co_return;
        }
        const core::LayerId l = doc.find_layer(cls->layer);
        if (l == core::kNoLayer || doc.layer(l)->feature_class.empty()) {
            ctx.refuse(ErrorCode::NotFound, "'" + cls->name +
                                                "' sınıfını izleyen bir katman yok; önce KALEM " +
                                                cls->id + " ya da KALEMBAĞLA.");
            co_return;
        }
        layers.push_back(l);
    } else if (const Value l = ctx.argument("katman"); !l.empty()) {
        const core::LayerId id = doc.find_layer(l.as_text());
        if (id == core::kNoLayer) {
            ctx.refuse(ErrorCode::NotFound, "Bilinmeyen katman: '" + l.as_text() + "'.");
            co_return;
        }
        if (doc.layer(id)->feature_class.empty()) {
            ctx.refuse(ErrorCode::NotFound,
                       "'" + l.as_text() +
                           "' katmanı bir sınıfı izlemiyor; KALEMBAĞLA ile bağlayın.");
            co_return;
        }
        layers.push_back(id);
    } else {
        for (core::LayerId each = 0; each < doc.layers().size(); ++each)
            if (const core::Layer* held = doc.layer(each);
                held != nullptr && !held->feature_class.empty())
                layers.push_back(each);
    }
    if (layers.empty()) {
        ctx.echo("Bir sınıfı izleyen katman yok; denetlenecek bir şey yok. Başlamak için: KALEM");
        co_return;
    }

    std::string said;
    core::Json layer_rows   = core::Json::array({});
    core::Json finding_rows = core::Json::array({});
    std::size_t total       = 0;
    std::vector<core::EntityKey> offenders;
    for (const core::LayerId l : layers) {
        const core::Layer* held = doc.layer(l);
        const FeatureClass* cls = catalog->of_reference(held->feature_class);
        std::size_t objects     = 0;
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().layer[e] == l) ++objects;

        core::Json row;
        row.set("katman", core::Json::string(held->name));
        row.set("nesne", core::Json::integer(static_cast<std::int64_t>(objects)));
        if (cls == nullptr) {
            said += "\n" + held->name + ": " + reference_class(held->feature_class) +
                    " sınıfı bu kataloğda yok (paket '" + reference_package(held->feature_class) +
                    "'); " + std::to_string(objects) + " nesne denetlenemedi.";
            row.set("sinif", core::Json::string(reference_class(held->feature_class)));
            row.set("denetlendi", core::Json::boolean(false));
            layer_rows.push(std::move(row));
            continue;
        }
        const auto findings = check_feature_class(doc, l, *cls);
        total += findings.size();
        row.set("sinif", core::Json::string(cls->id));
        row.set("denetlendi", core::Json::boolean(true));
        row.set("bulgu", core::Json::integer(static_cast<std::int64_t>(findings.size())));
        layer_rows.push(std::move(row));

        std::string version_note;
        const std::string was = held->feature_class.substr(
            held->feature_class.find('@') == std::string::npos ? 0
                                                               : held->feature_class.find('@') + 1,
            held->feature_class.find('/') == std::string::npos
                ? std::string::npos
                : held->feature_class.find('/') - held->feature_class.find('@') - 1);
        if (!was.empty() && was != catalog->package_version)
            version_note = " (katman " + was + " sürümüyle bağlanmış, paket şimdi " +
                           catalog->package_version + ")";
        said += "\n" + held->name + " — " + cls->name + ": " + std::to_string(objects) +
                " nesne, " +
                (findings.empty() ? std::string("sorun yok")
                                  : std::to_string(findings.size()) + " bulgu") +
                version_note;

        std::map<ClassFinding::Rule, std::size_t> by_rule;
        for (const ClassFinding& f : findings)
            ++by_rule[f.rule];
        for (const auto& [rule, count] : by_rule)
            said += "\n    " + std::string(class_rule_word(rule)) + ": " + std::to_string(count);
        std::size_t shown = 0;
        for (const ClassFinding& f : findings) {
            if (f.object != core::EntityKey::None) offenders.push_back(f.object);
            if (shown < 8) {
                said += "\n    [" +
                        (f.object == core::EntityKey::None ? std::string("sınıf")
                                                           : std::to_string(core::raw(f.object))) +
                        "] " + f.detail;
                ++shown;
            }
            if (finding_rows.as_array().size() < 200) {
                core::Json one;
                one.set("katman", core::Json::string(held->name));
                one.set("nesne",
                        core::Json::integer(f.object == core::EntityKey::None
                                                ? 0
                                                : static_cast<std::int64_t>(core::raw(f.object))));
                one.set("kural", core::Json::string(class_rule_word(f.rule)));
                one.set("alan", core::Json::string(f.field));
                one.set("ayrinti", core::Json::string(f.detail));
                finding_rows.push(std::move(one));
            }
        }
        if (findings.size() > shown)
            said += "\n    … ve " + std::to_string(findings.size() - shown) + " bulgu daha";
    }

    if (ctx.argument("sec").as_bool() && !offenders.empty()) {
        bus.remember_selection();
        bus.selection().clear();
        std::ranges::sort(offenders, [](core::EntityKey a, core::EntityKey b) {
            return core::raw(a) < core::raw(b);
        });
        offenders.erase(std::ranges::unique(offenders).begin(), offenders.end());
        for (const core::EntityKey k : offenders)
            (void)bus.selection().add(k);
        if (bus.on_selection_changed) bus.on_selection_changed();
        said += "\nSorunlu " + std::to_string(offenders.size()) + " nesne seçildi.";
    }
    core::Json r;
    r.set("katmanlar", std::move(layer_rows));
    r.set("bulgular", std::move(finding_rows));
    r.set("toplam", core::Json::integer(static_cast<std::int64_t>(total)));
    ctx.report(std::move(r));
    ctx.echo("Sınıf denetimi:" + said);
}

} // namespace

PIRICAD_COMMAND(feature_class)
{
    return CommandSpec{
        .id       = "core.feature_class",
        .names    = {"KALEM", "SINIF", "PEN", "FEATURECLASS", "KLM"},
        .title    = "Sayısallaştırma Kalemi",
        .category = Category::Layer,
        .params =
            {
                Param::text("ad", Arity::optional(),
                            "Sınıfın kimliği, adı ya da kısaltması (Bina, Yol ekseni, Parsel…); "
                            "boşsa sınıflar listelenir")
                    .en("name"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Sayısallaştırma sınıfını (kalemi) seçer: katmanı ve alanlarını kurar, etkin "
                   "yapar; çizilen nesne sınıfın varsayılanlarıyla başlar.",
        .run     = &run_pen,
        .effect  = Effect::Query | Effect::DocumentEdit,
    };
}

PIRICAD_COMMAND(feature_class_bind)
{
    return CommandSpec{
        .id       = "core.feature_class_bind",
        .names    = {"KALEMBAĞLA", "KALEMBAGLA", "SINIFABAĞLA", "BINDCLASS", "KLB"},
        .title    = "Nesneleri Sınıfa Bağla",
        .category = Category::Layer,
        .params =
            {
                Param::text("ad", Arity::exactly(1), "Bağlanacağı sınıfın kimliği ya da adı")
                    .en("name"),
                Param{
                    "nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                    "Bağlanacak nesnelerin kalıcı kimlikleri; verilmezse katman, o da yoksa seçim"}
                    .en("objects"),
                Param::text("katman", Arity::optional(),
                            "Bu katmanın bütün nesneleri bağlanır (nesneler verilmediyse)")
                    .en("layer"),
                Param::text("esle", Arity{0, 0xFFFFFFFFu},
                            "Bir sütunun değerini sınıfın alanına taşır: 'eski_sutun:sinif_alani' "
                            "(aynı türde ya da metin alanına); birden çok kez verilebilir")
                    .en("map"),
                Param::boolean("onizle", Arity::optional(),
                               "Hiçbir şey yazma: neyin bağlanacağını ve neyin uymadığını söyle; "
                               "varsayılan hayır")
                    .en("preview"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Var olan CAD nesnelerini bir sayısallaştırma sınıfına bağlar: katmana alır, "
                   "alanları eşler, varsayılanları doldurur; önizlemesi ve tek geri alma adımı "
                   "vardır.",
        .run     = &run_bind,
        .effect  = Effect::Query | Effect::DocumentEdit,
    };
}

PIRICAD_COMMAND(feature_class_check)
{
    return CommandSpec{
        .id       = "core.feature_class_check",
        .names    = {"KALEMDENETİM", "KALEMDENETIM", "SINIFDENETİM", "CHECKCLASS", "KLD"},
        .title    = "Sınıf Denetimi",
        .category = Category::Query,
        .params =
            {
                Param::text("ad", Arity::optional(),
                            "Yalnız bu sınıfın katmanı; verilmezse sınıf izleyen bütün katmanlar")
                    .en("name"),
                Param::text("katman", Arity::optional(), "Yalnız bu katman").en("layer"),
                Param::boolean("sec", Arity::optional(),
                               "evet = sorunlu nesneleri seç; varsayılan hayır")
                    .en("select"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "Bir sınıfı izleyen katmanlardaki nesnelerin hâlâ sınıfın dediği gibi olup "
                   "olmadığına bakar: geometri, en küçük alan/uzunluk, zorunlu ve izinli "
                   "değerler.",
        .run     = &run_check,
        .effect  = Effect::Query,
    };
}

} // namespace piricad::command
