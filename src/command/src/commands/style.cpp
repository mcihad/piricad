// SPDX-License-Identifier: GPL-3.0-or-later
// core.style — STİL
//
// .claude/model.md R14, stated once and obeyed here:
//
//   **A GIS renderer is a command that writes the style column.
//     Style is NEVER derived at frame time.**
//
// This is that command. It resolves an appearance for every entity on one layer —
// from a catalogue package's declarative rules, from a named catalogue row, or
// from values given on the command line — interns the result in the document's
// StyleTable and writes one `StyleId` into `style[e]` inside the transaction. The
// renderer afterwards reads a `u32` and evaluates nothing (model.md P7).
//
// The catalogue itself is DATA (CLAUDE.md 5.13): every colour, paper width, draw
// order and row identity lives under /data/catalogs and is loaded at runtime.
// Nothing in this file knows what any of those rows mean.
//
// The two passes are deliberate. Everything that can fail — reading the package,
// matching a rule, looking a row up — happens before the first write, so a
// failure leaves the document untouched instead of half-styled (§2.5, R13).
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/style_rule.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

/// Reads one catalogue package from disk. Core may not do this (core.md P9), so
/// the file boundary is here and the parsing is core's.
core::Result<core::StyleCatalog> load_catalog(const std::string& path)
{
    const std::ifstream in(path, std::ios::binary);
    if (!in)
        return core::err(core::ErrorCode::IoFailure,
                         "Stil kataloğu okunamadı: '" + path +
                             "'. Dosya yolunu denetleyin; göreli yol çalışma dizinine göre "
                             "çözülür.");

    std::ostringstream buffer;
    buffer << in.rdbuf();

    auto parsed = core::Json::parse(buffer.str());
    if (!parsed)
        return core::err(core::ErrorCode::ParseError, "Stil kataloğu geçerli JSON değil: '" + path +
                                                          "'. " + parsed.error().message);

    return core::StyleCatalog::from_json(parsed.value());
}

/// The classifying values one entity offers a rule today.
///
/// Everything here comes from data the document already holds. When `Document`
/// grows its declared attribute columns (model.md R27), `FeatureView::from_row`
/// layers them on top of these and no rule, catalogue or test changes.
core::FeatureView feature_of(const core::Document& doc, core::EntityId e, const core::Layer& layer)
{
    core::FeatureView view;

    const auto& geometry      = doc.geometry();
    const core::RingSpan span = geometry.rings_of(doc.entities().slot[e]);

    bool has_face          = false;
    std::uint64_t vertices = 0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geometry.ring_role[r] != core::RingRole::Open) has_face = true;
        vertices += geometry.ring_count[r];
    }

    const char* shape = "cizgi";
    if (has_face)
        shape = "alan";
    else if (vertices <= 1)
        shape = "nokta";

    view.set_text("geometri", shape);
    view.set_text("katman", layer.name);
    if (!layer.catalog_ref.empty()) view.set_text("katman_kodu", layer.catalog_ref);
    view.set_number("halka_sayisi", static_cast<std::int64_t>(span.count));
    view.set_number("tepe_sayisi", static_cast<std::int64_t>(vertices));
    view.set_number("alan_mm2", doc.entity_area(e));
    view.set_number("cevre_mm", doc.entity_perimeter(e));

    // The declared attribute columns, layered ON TOP of the derived fields above.
    // The comment on this function used to say this would happen the day Document
    // grew attribute columns and that no rule, catalogue or test would change; it
    // grew them, and none did.
    //
    // Attributes win over derived names on a collision, because a column the user
    // declared is a statement about the parcel and `geometri` is an observation
    // about its rings. A drawing that declares a column called `katman` means
    // that column.
    const core::AttrTable& attrs = doc.attributes();
    if (attrs.columns() > 0) {
        const core::FeatureView row = core::FeatureView::from_row(attrs, doc.entities().slot[e]);
        for (const auto& [name, value] : row.fields())
            view.set(name, value);
    }

    return view;
}

/// Finds a catalogue row by its id, and failing that by its label.
///
/// Both are offered because a drawing is tagged by a person. `nip-toplu-konut-
/// alani-siniri` is what the package calls the row; `TOPLU KONUT ALANI` is what a
/// planner writes in an attribute cell, and refusing the second would make the
/// feature unusable by the people it is for. The id is tried first, so a package
/// whose ids and labels collide resolves to the id — the stable one.
const core::StyleEntry* row_for(const core::StyleCatalog& catalog, const std::string& value)
{
    if (auto by_id = catalog.entry(value); by_id) return by_id.value();
    for (const core::StyleEntry& e : catalog.entries())
        if (e.label == value) return &e;
    return nullptr;
}

Task<void> run(Context& ctx)
{
    auto layer_name = co_await ctx.text("katman", "Katman adı");
    if (!layer_name || layer_name->empty()) co_return;

    Bus& bus                  = ctx.session().bus();
    const core::LayerId layer = bus.document().find_layer(*layer_name);
    if (layer == core::kNoLayer) {
        ctx.session().fail(
            core::err(core::ErrorCode::NotFound, "Katman bulunamadı: '" + *layer_name +
                                                     "'. Önce KATMAN komutuyla oluşturun."));
        co_return;
    }

    const bool clear               = ctx.argument("sifirla").as_bool(false);
    const std::int64_t denominator = ctx.argument("olcek").as_int(0);

    if (denominator < 0 || denominator > 0xFFFFFFFFLL) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'olcek' ölçek paydası 0 ile 4294967295 arasında bir tam "
                                     "sayı olmalı (0 = ölçekten bağımsız). Girilen: " +
                                         std::to_string(denominator)));
        co_return;
    }
    const auto scale = static_cast<core::ScaleDenominator>(denominator);

    const Value code    = ctx.argument("kod");
    const Value package = ctx.argument("paket");

    if (!code.empty() && package.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'kod' verildi ama 'paket' verilmedi: hangi katalogdan "
                                     "okunacağı belirsiz. 'paket=' ile katalog dosyasını verin."));
        co_return;
    }
    if (clear && !package.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'sifirla' ile 'paket' aynı komutta kullanılamaz: biri "
                                     "stili siler, diğeri yazar."));
        co_return;
    }

    const Value classify_by = ctx.argument("sinifla");
    if (!classify_by.empty() && package.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'sinifla' verildi ama 'paket' verilmedi: hangi katalogla "
                                     "eşleşeceği belirsiz. 'paket=' ile katalog dosyasını verin."));
        co_return;
    }
    if (!classify_by.empty() && !code.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "'sinifla' ile 'kod' aynı komutta kullanılamaz: biri her "
                                     "nesneyi kendi özniteliğine göre sınıflar, diğeri hepsine "
                                     "aynı satırı yazar."));
        co_return;
    }

    core::AttrId classify_col = core::kNoAttr;
    if (!classify_by.empty()) {
        classify_col = bus.document().attributes().find(classify_by.as_text());
        if (classify_col == core::kNoAttr) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Bilinmeyen öznitelik: '" + classify_by.as_text() +
                                             "'. Tanımlı sütunları SÜTUN ile görebilirsiniz."));
            co_return;
        }
    }

    std::optional<core::StyleCatalog> catalog;
    if (!package.empty()) {
        auto loaded = load_catalog(package.as_text());
        if (!loaded) {
            ctx.session().fail(loaded.error());
            co_return;
        }
        catalog = std::move(loaded.value());
    }

    const core::Layer& record = *bus.document().layer(layer);

    // ---- pass 1: decide. Nothing below this point may fail. ----
    std::vector<core::EntityId> targets;
    std::vector<core::Appearance> resolved;
    std::string row_label;
    std::size_t classified   = 0;
    std::size_t unclassified = 0;
    {
        const auto& entities = bus.document().entities();
        for (core::EntityId e = 0; e < entities.size(); ++e) {
            if (!entities.alive(e)) continue;
            if (entities.layer[e] != layer) continue;

            core::Appearance appearance = record.appearance;

            if (catalog.has_value() && classify_col != core::kNoAttr) {
                // The CATEGORIZED renderer: every entity is styled by what its own
                // attribute says. The binding lives HERE and not in the package,
                // because what a drawing calls its column is a project decision;
                // the regulation states what a gösterim looks like, never what
                // your attribute is named (CLAUDE.md 5.13, data.md R6).
                auto cell = bus.document().attribute(classify_col, e);
                if (!cell) {
                    ctx.session().fail(cell.error());
                    co_return;
                }
                if (!cell.value().present) {
                    // An untagged entity keeps the layer default and is counted.
                    // Guessing a gösterim for a parcel that declares none is
                    // exactly the invention a legal drawing must not contain.
                    ++unclassified;
                    targets.push_back(e);
                    resolved.push_back(appearance);
                    continue;
                }

                const std::string value = cell.value().type == core::AttrType::Text ||
                                                  cell.value().type == core::AttrType::CodeRef
                                              ? cell.value().text
                                              : std::to_string(cell.value().number);

                const core::StyleEntry* row = row_for(*catalog, value);
                if (row == nullptr) {
                    ctx.session().fail(core::err(
                        core::ErrorCode::NotFound,
                        "'" + value + "' değeri '" + catalog->id() + "' (" +
                            catalog->package_version() +
                            ") kataloğunda ne kimlik ne ad olarak bulundu. Katalog sürümünü ve "
                            "öznitelik değerini denetleyin."));
                    co_return;
                }
                appearance = core::apply_entry(*row, appearance);
                ++classified;
                if (row_label.empty()) row_label = row->label.empty() ? row->id : row->label;
            } else if (catalog.has_value()) {
                auto entry = code.empty()
                                 ? catalog->classify(feature_of(bus.document(), e, record), scale)
                                 : catalog->entry(code.as_text());
                if (!entry) {
                    ctx.session().fail(entry.error());
                    co_return;
                }
                appearance = core::apply_entry(*entry.value(), appearance);
                if (row_label.empty())
                    row_label =
                        entry.value()->label.empty() ? entry.value()->id : entry.value()->label;
            }

            if (const Value v = ctx.argument("renk"); !v.empty()) {
                appearance.rgba       = static_cast<std::uint32_t>(v.as_int());
                appearance.src_colour = core::Source::Explicit;
            }
            if (const Value v = ctx.argument("kalinlik"); !v.empty()) {
                appearance.width_um  = static_cast<std::int32_t>(v.as_int());
                appearance.src_width = core::Source::Explicit;
            }
            if (const Value v = ctx.argument("dolgu"); !v.empty()) {
                appearance.fill_rgba = static_cast<std::uint32_t>(v.as_int());
                appearance.src_fill  = core::Source::Explicit;
            }
            if (const Value v = ctx.argument("sira"); !v.empty())
                appearance.z_order = static_cast<std::int16_t>(v.as_int());

            targets.push_back(e);
            resolved.push_back(appearance);
        }
    }

    // ---- pass 2: write ----
    // A scale window turns the write into a one-layer SYMBOL rather than a bare
    // appearance, because the window lives on the symbol. Without a window the
    // appearance is interned directly and the id is identical to what it always
    // was — a drawing that declares no scale range is byte for byte unchanged.
    const Value scale_min = ctx.argument("olcek_min");
    const Value scale_max = ctx.argument("olcek_max");
    const bool windowed   = !scale_min.empty() || !scale_max.empty();

    core::StyleId last = core::kByLayerStyle;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        core::StyleId style = core::kByLayerStyle;
        if (!clear && windowed) {
            core::Symbol sym = core::Symbol::of(resolved[i]);
            sym.min_scale = static_cast<std::uint32_t>(scale_min.empty() ? 0 : scale_min.as_int());
            sym.max_scale = static_cast<std::uint32_t>(scale_max.empty() ? 0 : scale_max.as_int());
            style         = ctx.transaction().intern_symbol(sym);
        } else if (!clear) {
            style = ctx.transaction().intern_style(resolved[i]);
        }
        if (auto st = ctx.transaction().set_entity_style(targets[i], style); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        last = style;
    }

    // Recorded so a replay resolves the same rows whichever client typed them.
    for (const char* name : {"paket", "kod", "sinifla", "olcek", "olcek_min", "olcek_max", "renk",
                             "kalinlik", "dolgu", "sira", "sifirla"}) {
        if (const Value v = ctx.argument(name); !v.empty()) ctx.record(name, v);
    }

    if (targets.empty()) {
        ctx.echo("'" + record.name + "' katmanında nesne yok; stil yazılmadı.");
        co_return;
    }

    std::string message =
        std::to_string(targets.size()) + " nesneye stil yazıldı: '" + record.name + "'";
    if (!classify_by.empty())
        message += ", '" + classify_by.as_text() + "' özniteliğine göre sınıflandı (" +
                   std::to_string(classified) + " eşleşti, " + std::to_string(unclassified) +
                   " öznitelik taşımıyor)";
    else if (!row_label.empty())
        message += ", katalog satırı '" + row_label + "'";
    message +=
        clear ? ", katman varsayılanına döndü." : ", stil kimliği " + std::to_string(last) + ".";
    ctx.echo(message);
}

} // namespace

PIRICAD_COMMAND(style)
{
    return CommandSpec{
        .id       = "core.style",
        .names    = {"STİL", "STIL", "STYLE", "ST"},
        .category = Category::Layer,
        .params =
            {
                Param::text("katman", Arity::exactly(1),
                            "Stilin yazılacağı katmanın adı; katman var olmalı"),
                Param::text("paket", Arity::optional(), "Stil kataloğu paketinin dosya yolu"),
                Param::integer("olcek_min", Arity::optional(),
                               "Bu ölçek paydasından daha yakında çizilmez (1:N'deki N)"),
                Param::integer("olcek_max", Arity::optional(),
                               "Bu ölçek paydasından daha uzakta çizilmez"),
                Param::text("sinifla", Arity::optional(),
                            "Sınıflandırmada kullanılacak öznitelik; her nesne kendi "
                            "değerine göre stillenir"),
                Param::text("kod", Arity::optional(),
                            "Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir"),
                Param::integer("olcek", Arity::optional(),
                               "Ölçek paydası (1:N); 0 = ölçekten bağımsız"),
                Param::integer("renk", Arity::optional(), "Çizgi rengi, 0xAARRGGBB"),
                Param::integer("kalinlik", Arity::optional(),
                               "Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm)"),
                Param::integer("dolgu", Arity::optional(), "Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz"),
                Param::integer("sira", Arity::optional(), "Çizim sırası; büyük olan üste gelir"),
                Param::boolean("sifirla", Arity::optional(),
                               "Stili siler; nesneler katman varsayılanına döner"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen "
                   "değerlerden yazar.",
        .run     = &run,
    };
}

} // namespace piricad::command
