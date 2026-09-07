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
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/style_rule.hpp"
#include "kentos_cad/core/text.hpp"
#include <algorithm>
#include <cmath>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace kentos::command {
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

    // Then the other names the regulation publishes the row under. Last, so a
    // package can never shadow a real id or label with an alias.
    for (const core::StyleEntry& e : catalog.entries())
        for (const std::string& alias : e.aliases)
            if (alias == value) return &e;
    return nullptr;
}

/// Whether the catalogue row draws a SYMBOL rather than just a colour.
///
/// Two ways a row can: it names pictures, or it declares a symbol-layer stack.
/// Both have to take the same road out of here. A row that declared layers and
/// named no picture went down the plain-appearance branch instead, so every
/// entity got a flat colour and only the layer's own default carried the symbol —
/// which is visible the moment a declared row's second layer stops appearing.
bool has_symbol(const core::StyleEntry& row)
{
    return !row.layers.empty() || !row.image_hatch.empty() || !row.image_symbol.empty() ||
           !row.image_line.empty();
}

/// Reads one published picture and adds it to the drawing.
///
/// The bytes travel INSIDE the document from here on. A path would break the
/// moment the drawing is emailed to the belediye that has to check it; see
/// `kentos_cad/core/image_store.hpp`.
core::Result<core::ImageId> intern_picture(Context& ctx, const std::filesystem::path& dir,
                                           const std::string& file, const std::string& origin)
{
    const std::filesystem::path path = dir / file;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        const std::string missing = "Gösterim görseli açılamadı: '"; // ui-label
        return core::err(core::ErrorCode::NotFound,
                         missing + path.string() + "'. Paket eksik kurulmuş olabilir.");
    }

    std::vector<std::byte> bytes;
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size <= 0) {
        const std::string empty = "Gösterim görseli boş: '"; // ui-label
        return core::err(core::ErrorCode::ValidationFailed, empty + path.string() + "'.");
    }
    bytes.resize(static_cast<std::size_t>(size));
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(bytes.data()), size);

    return ctx.transaction().intern_image(bytes, origin);
}

/// Builds the symbol a catalogue row's own pictures describe.
///
/// The stack itself is `core::symbol_of_entry`, shared with the symbol shelf, so
/// what a gallery thumbnail shows is what this applies. Only the resolver differs:
/// here the bytes are interned into the DOCUMENT, because a drawing carries the
/// pictures it uses.
core::Result<core::Symbol> build_from_row(Context& ctx, const core::StyleEntry& row,
                                          const std::filesystem::path& dir)
{
    core::Status failure = core::ok();

    auto resolve = [&](const std::string& file) -> core::ImageId {
        auto image = intern_picture(ctx, dir, file, row.id + " · " + row.source_ref);
        if (!image) {
            failure = image.error();
            return core::kNoImage;
        }
        return image.value();
    };

    core::Symbol sym = core::symbol_of_entry(
        row, resolve, [&ctx](const core::DashPattern& p, std::string_view origin) {
            auto id = ctx.transaction().intern_dash(p, origin);
            return id ? id.value() : core::kSolidDash;
        });
    if (!failure) return failure.error();
    return sym;
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

    // Resolve a direct catalogue row once, before walking the entity table. The
    // same row is also the layer renderer for an empty layer selected from the
    // gallery, so it cannot depend on finding a first entity.
    const core::StyleEntry* direct_entry = nullptr;
    if (catalog.has_value() && !code.empty()) {
        auto entry = catalog->entry(code.as_text());
        if (!entry) {
            ctx.session().fail(entry.error());
            co_return;
        }
        direct_entry = entry.value();
    }

    const std::filesystem::path package_dir =
        package.empty() ? std::filesystem::path{}
                        : std::filesystem::path(package.as_text()).parent_path();

    const core::Layer& record = *bus.document().layer(layer);

    // A layer is the ByLayer source for current and future entities. STİL still
    // materialises one StyleId per existing entity (model.md R14), but a direct
    // style must also update that source: otherwise choosing a style on an empty
    // layer visibly does nothing and the next entity reverts to the old default.
    // The complete stack is stored on the layer too. Existing entities still get
    // their resolved styles, while an empty layer and future ByLayer entities use
    // the same symbol rather than falling back to a plain appearance.

    // ---- the line type ----
    //
    // Read as LENGTHS, not as a row in a table somebody has to install: a
    // published line type is four numbers and the drawing carries them, exactly
    // as it carries the bytes of a raster symbol (dash_store.hpp). `sürekli` and
    // an empty value both mean the solid stroke every drawing had before line
    // types existed.
    core::DashPattern dash_pattern;
    bool has_dash = false;

    if (const Value v = ctx.argument("desen"); !v.empty()) {
        const std::string text = v.as_text();
        if (!core::turkish_key_equals(text, "sürekli") && !core::turkish_key_equals(text, "duz")) {
            std::vector<double> parts;
            std::istringstream words(text);
            std::string word;
            while (words >> word) {
                try {
                    parts.push_back(std::stod(word));
                } catch (const std::exception&) {
                    ctx.session().fail(core::err(
                        core::ErrorCode::ParseError,
                        "'desen' çizgi kalınlığının katı olarak sayılardan oluşur; "
                        "okunamayan parça: '" +
                            word + "'. Örnek: desen=\"8 1 1 1\" (kesik-nokta), desen=sürekli."));
                    co_return;
                }
            }

            if (parts.size() > core::kMaxDashSegments || parts.size() % 2 != 0 || parts.empty()) {
                ctx.session().fail(core::err(
                    core::ErrorCode::ValidationFailed,
                    "'desen' çizgi ve boşluk çiftlerinden oluşur ve en çok " +
                        std::to_string(core::kMaxDashSegments) +
                        " parça taşır. Verilen parça sayısı: " + std::to_string(parts.size()) +
                        "."));
                co_return;
            }

            dash_pattern.count = static_cast<std::uint8_t>(parts.size());
            for (std::size_t i = 0; i < parts.size(); ++i)
                dash_pattern.lengths[i] = static_cast<std::uint16_t>(
                    std::lround(std::clamp(parts[i], 0.01, 650.0) * 100.0));
        }
        has_dash = true;
    }

    core::DashId dash_id = core::kSolidDash;
    if (has_dash && dash_pattern.count > 0) {
        // Interned in the DECIDE phase, where a failure is still allowed to stop
        // the command. Additive and deduplicated, exactly like a picture: an id
        // handed out stays valid and there is no inverse Op to record.
        auto interned = ctx.transaction().intern_dash(dash_pattern, "STİL komutu");
        if (!interned) {
            ctx.session().fail(interned.error());
            co_return;
        }
        dash_id = interned.value();
    }

    const auto apply_appearance_arguments = [&ctx, has_dash,
                                             dash_id](core::Appearance& appearance) {
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

        // Applied HERE, with the other appearance arguments, and not on the
        // described symbol layer: the layer's `look` is overwritten wholesale by
        // the resolved appearance a few lines later, so a dash written onto the
        // layer alone never reached the drawing.
        if (has_dash) {
            appearance.dash     = dash_id;
            appearance.src_dash = core::Source::Explicit;
        }
    };

    // ---- pass 1: decide. Nothing below this point may fail. ----
    std::vector<core::EntityId> targets;
    std::vector<core::Appearance> resolved;

    /// The catalogue row each target resolved to, or null.
    ///
    /// Kept because the row is where the REGULATION'S OWN PICTURES are named, and
    /// those cannot be read until pass 2 where the transaction is. Borrowed from
    /// the catalogue, which outlives both passes.
    std::vector<const core::StyleEntry*> rows;
    std::string row_label;
    std::size_t classified   = 0;
    std::size_t unclassified = 0;
    {
        const auto& entities = bus.document().entities();
        for (core::EntityId e = 0; e < entities.size(); ++e) {
            if (!entities.alive(e)) continue;
            if (entities.layer[e] != layer) continue;

            core::Appearance appearance         = record.appearance;
            const core::StyleEntry* picture_row = nullptr;

            if (catalog.has_value() && classify_col != core::kNoAttr) {
                // The CATEGORIZED renderer: every entity is styled by what its own
                // attribute says. The binding lives HERE and not in the package,
                // because what a drawing calls its column is a project decision;
                // the regulation states what a `gösterim` looks like, never what
                // your attribute is named (CLAUDE.md 5.13, data.md R6).
                auto cell = bus.document().attribute(classify_col, e);
                if (!cell) {
                    ctx.session().fail(cell.error());
                    co_return;
                }
                if (!cell.value().present) {
                    // An untagged entity keeps the layer default and is counted.
                    // Guessing a `gösterim` for a parcel that declares none is
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
                appearance  = core::apply_entry(*row, appearance);
                picture_row = row;
                ++classified;
                if (row_label.empty()) row_label = row->label.empty() ? row->id : row->label;
            } else if (catalog.has_value()) {
                const core::StyleEntry* entry = direct_entry;
                if (code.empty()) {
                    auto matched = catalog->classify(feature_of(bus.document(), e, record), scale);
                    if (!matched) {
                        ctx.session().fail(matched.error());
                        co_return;
                    }
                    entry = matched.value();
                }
                appearance  = core::apply_entry(*entry, appearance);
                picture_row = entry;
                if (row_label.empty()) row_label = entry->label.empty() ? entry->id : entry->label;
            }

            apply_appearance_arguments(appearance);

            targets.push_back(e);
            resolved.push_back(appearance);
            rows.push_back(picture_row);
        }
    }

    // ---- the symbol layer, when one was described ----
    //
    // Reads the layer parameters into a `core::SymbolLayer`. Every name is
    // resolved against the table in core and an unknown one is an ERROR naming
    // what is valid: a symbol layer type nobody recognised is a symbol the user
    // meant and did not get, and defaulting it would draw a `demiryolu` as an
    // ordinary boundary — wrong, and wrong quietly.
    core::SymbolLayer described;
    // A `desen` on its own describes a symbol layer, so the stroke it belongs to
    // is built rather than the bare appearance being written.
    bool has_layer = has_dash;

    if (const Value v = ctx.argument("tip"); !v.empty()) {
        const auto type = core::symbol_layer_type_from_name(v.as_text());
        if (!type) {
            ctx.session().fail(
                core::err(core::ErrorCode::ValidationFailed,
                          "Bilinmeyen sembol katmanı tipi: '" + v.as_text() +
                              "'. Geçerli olanlar: " + core::symbol_layer_type_names() + "."));
            co_return;
        }
        described.type = *type;
        has_layer      = true;
    }

    core::Unit unit = core::Unit::Paper;
    if (const Value v = ctx.argument("birim"); !v.empty()) {
        const auto parsed = core::unit_from_name(v.as_text());
        if (!parsed) {
            ctx.session().fail(core::err(core::ErrorCode::ValidationFailed,
                                         "Bilinmeyen birim: '" + v.as_text() +
                                             "'. Geçerli olanlar: " + core::unit_names() + "."));
            co_return;
        }
        unit      = *parsed;
        has_layer = true;
    }

    if (const Value v = ctx.argument("sekil"); !v.empty()) {
        const auto shape = core::marker_shape_from_name(v.as_text());
        if (!shape) {
            ctx.session().fail(core::err(core::ErrorCode::ValidationFailed,
                                         "Bilinmeyen işaretçi şekli: '" + v.as_text() +
                                             "'. Geçerli olanlar: " + core::marker_shape_names() +
                                             "."));
            co_return;
        }
        described.shape = *shape;
        has_layer       = true;
    }

    if (const Value v = ctx.argument("yerlesim"); !v.empty()) {
        const auto placement = core::marker_placement_from_name(v.as_text());
        if (!placement) {
            ctx.session().fail(
                core::err(core::ErrorCode::ValidationFailed,
                          "Bilinmeyen işaretçi yerleşimi: '" + v.as_text() +
                              "'. Geçerli olanlar: " + core::marker_placement_names() + "."));
            co_return;
        }
        described.placement = *placement;
        has_layer           = true;
    }

    // Each measure may name its OWN unit; `birim` is the default for the ones that
    // do not. A gösterim routinely mixes them — a marker sized in paper
    // micrometres repeated at a ground interval is how a boundary glyph is
    // specified — and one unit for the whole layer cannot say that. The style
    // designer shows a unit beside every measure for the same reason, and had no
    // way to send three of them.
    std::string bad_unit;
    const auto measure = [&](const char* value_name, const char* unit_name, core::Measure& out) {
        const Value v = ctx.argument(value_name);
        if (v.empty()) return;

        core::Unit measured = unit;
        if (const Value named = ctx.argument(unit_name); !named.empty()) {
            const auto parsed = core::unit_from_name(named.as_text());
            if (!parsed) {
                bad_unit = std::string(unit_name) + "='" + named.as_text() + "'";
                return;
            }
            measured = *parsed;
        }

        out       = core::Measure{static_cast<std::int32_t>(v.as_int()), measured};
        has_layer = true;
    };
    measure("boyut", "boyut_birim", described.size);
    measure("aralik", "aralik_birim", described.interval);
    measure("aralik_y", "aralik_y_birim", described.spacing_y);
    measure("kaydirma", "kaydirma_birim", described.offset);
    measure("faz", "faz_birim", described.phase);

    if (!bad_unit.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::ValidationFailed,
                                     "Bilinmeyen birim: " + bad_unit +
                                         ". Geçerli olanlar: " + core::unit_names() + "."));
        co_return;
    }

    if (const Value v = ctx.argument("aci"); !v.empty()) {
        described.angle_udeg = static_cast<std::int32_t>(v.as_int());
        has_layer            = true;
    }
    if (const Value v = ctx.argument("yazi"); !v.empty()) {
        described.text = v.as_text();
        has_layer      = true;
    }

    // THE SLOT, and the column it needs, declared together.
    //
    // A caption and a slot are the two things a `yazi-isaretci` can be and it is
    // never both: `yazi` writes a word, `alan` writes whatever the parcel says.
    // Given both, the slot wins and the word is dropped — silently keeping a
    // fixed caption on a layer the user just parameterised would draw the wrong
    // thing on every object.
    //
    // The column is DECLARED HERE, in the same transaction. It is the connective
    // tissue the whole feature turns on: a symbol that asks for `taks` and a
    // drawing with no `taks` column is a symbol whose parameter can never be
    // filled in, and making the user notice that themselves — by drawing the
    // object, opening the panel and finding nothing — is how a feature reads as
    // broken. One command, one transaction, one undo step (Article 1.6).
    if (const Value v = ctx.argument("alan"); !v.empty()) {
        const std::string column = v.as_text();

        core::AttrType type = core::AttrType::Text;
        if (const Value t = ctx.argument("alan_tipi"); !t.empty()) {
            const auto parsed = core::attr_type_from_name(t.as_text());
            if (!parsed) {
                ctx.echo("Bilinmeyen alan türü: '" + t.as_text() +
                         "'. Beklenen: tam_sayi, uzunluk, evet_hayir, metin, kod.");
                co_return;
            }
            type = *parsed;
        }

        if (ctx.document().attributes().find(column) == core::kNoAttr) {
            core::AttrSpec spec;
            spec.id      = column;
            spec.name_tr = column;
            spec.type    = type;
            if (auto made = ctx.transaction().declare_attribute(std::move(spec)); !made) {
                ctx.echo(made.error().message);
                co_return;
            }
        }

        described.field      = column;
        described.field_type = type;
        described.text.clear();
        has_layer = true;
    }
    if (const Value v = ctx.argument("saydamlik"); !v.empty()) {
        described.opacity = static_cast<std::uint8_t>(std::clamp<std::int64_t>(v.as_int(), 0, 255));
        has_layer         = true;
    }

    const bool append = ctx.argument("ekle").as_bool();

    // ---- pass 2: write ----
    // A scale window turns the write into a one-layer SYMBOL rather than a bare
    // appearance, because the window lives on the symbol. Without a window the
    // appearance is interned directly and the id is identical to what it always
    // was — a drawing that declares no scale range is byte for byte unchanged.
    const Value scale_min = ctx.argument("olcek_min");
    const Value scale_max = ctx.argument("olcek_max");
    const bool windowed   = !scale_min.empty() || !scale_max.empty();

    const core::StyleTable& table = bus.document().styles();

    core::Appearance layer_appearance = record.appearance;
    apply_appearance_arguments(layer_appearance);

    // A row chosen directly from the gallery is a whole QGIS-style symbol, not
    // just its stroke colour. Build it with document-owned images so raster
    // hatches, line types and marker images reach the canvas and travel in the
    // project file. This has to happen even for an empty layer.
    std::optional<core::Symbol> direct_symbol;
    if (!clear && classify_by.empty() && direct_entry != nullptr) {
        auto built = build_from_row(ctx, *direct_entry, package_dir);
        if (!built) {
            ctx.session().fail(built.error());
            co_return;
        }
        direct_symbol = std::move(built.value());
        for (core::SymbolLayer& symbol_layer : direct_symbol->layers)
            apply_appearance_arguments(symbol_layer.look);
        layer_appearance = direct_symbol->primary();
    }

    // A classified catalogue command can intentionally resolve different rows
    // per entity, so it has no single honest layer default. All other requests do
    // have one: direct arguments cover an empty layer and a non-classified row
    // shares the appearance resolved for its first entity.
    const bool direct_layer_style = has_layer || !ctx.argument("renk").empty() ||
                                    !ctx.argument("kalinlik").empty() ||
                                    !ctx.argument("dolgu").empty() || !ctx.argument("sira").empty();
    if (!clear && classify_by.empty() && !direct_symbol.has_value() && !targets.empty())
        layer_appearance = resolved.front();
    const bool write_layer_appearance =
        !clear && classify_by.empty() &&
        (direct_layer_style || direct_symbol.has_value() || !targets.empty());

    if (write_layer_appearance && layer_appearance != record.appearance) {
        if (auto st = ctx.transaction().set_layer_appearance(layer, layer_appearance); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
    }

    // A described symbol is the layer's default renderer. Appending starts from
    // the layer default, not from an arbitrary first entity, so it works on an
    // empty layer and stays stable when entities have individual overrides.
    const bool changes_layer_symbol =
        !clear && classify_by.empty() &&
        (has_layer || direct_layer_style || direct_symbol.has_value());
    if (clear || changes_layer_symbol) {
        core::StyleId layer_style = core::kByLayerStyle;
        if (!clear && direct_symbol.has_value()) {
            layer_style = ctx.transaction().intern_symbol(*direct_symbol);
        } else if (!clear && has_layer) {
            core::Symbol symbol;
            if (append && record.style != core::kByLayerStyle && table.contains(record.style))
                symbol = table.symbol_at(record.style);
            else if (append)
                symbol = core::Symbol::of(record.appearance);

            core::SymbolLayer added = described;
            added.look              = layer_appearance;
            symbol.layers.push_back(std::move(added));
            layer_style = ctx.transaction().intern_symbol(symbol);
        }
        if (layer_style != record.style) {
            if (auto st = ctx.transaction().set_layer_style(layer, layer_style); !st) {
                ctx.session().fail(st.error());
                co_return;
            }
        }
    }

    core::StyleId last = core::kByLayerStyle;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        // A row that DRAWS A SYMBOL is a symbol, not a colour — whether it says so
        // with pictures or with a declared stack.
        // Without this the branch below was entered only for a scale window or a
        // hand-written layer, so `STİL kod=` applied the row's fill and dropped
        // the hatch and the glyph the annex prints.
        const bool row_symbol = i < rows.size() && rows[i] != nullptr && has_symbol(*rows[i]);

        core::StyleId style = core::kByLayerStyle;
        if (!clear && (windowed || has_layer || row_symbol)) {
            core::Symbol sym;

            // `ekle` stacks onto what this entity already carries. Reading the
            // existing symbol rather than the layer default is what makes a stack
            // buildable one invocation at a time — which is how the designer
            // drives it, and how a script writes the same thing.
            if (append) {
                const core::StyleId current = bus.document().entities().style[targets[i]];
                if (current != core::kByLayerStyle && table.contains(current))
                    sym = table.symbol_at(current);
            }

            if (has_layer) {
                core::SymbolLayer added = described;
                added.look              = resolved[i];
                sym.layers.push_back(added);
            } else if (row_symbol) {
                // The row was published WITH PICTURES, so the symbol is what the
                // regulation printed rather than a colour standing in for it.
                auto built = build_from_row(ctx, *rows[i], package_dir);
                if (!built) {
                    ctx.session().fail(built.error());
                    co_return;
                }
                sym.layers = std::move(built.value().layers);
            } else if (sym.layers.empty()) {
                sym = core::Symbol::of(resolved[i]);
            }

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
    for (const char* name :
         {"paket", "kod", "sinifla", "olcek", "olcek_min", "olcek_max", "renk", "kalinlik", "dolgu",
          "sira", "sifirla", "desen", "faz", "yazi", "alan", "alan_tipi"}) {
        if (const Value v = ctx.argument(name); !v.empty()) ctx.record(name, v);
    }

    if (targets.empty()) {
        ctx.echo(write_layer_appearance
                     ? "'" + record.name +
                           "' katmanında nesne yok; varsayılan katman görünümü güncellendi."
                     : "'" + record.name + "' katmanında nesne yok; stil yazılmadı.");
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

KENTOS_COMMAND(style)
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

                // The symbol layer. Every parameter below describes ONE layer of
                // a symbol; `ekle` is what turns a series of invocations into a
                // stack, which is how a plan gösterim is actually built — a fill,
                // a boundary of another colour, and a repeated glyph on top.
                Param::text("tip", Arity::optional(),
                            "Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, "
                            "cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci"),
                Param::boolean("ekle", Arity::optional(),
                               "Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir"),
                Param::text("sekil", Arity::optional(),
                            "İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, "
                            "ok, yarim-daire, besgen, altigen, cizik"),
                Param::text("yerlesim", Arity::optional(),
                            "İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta"),
                Param::text("birim", Arity::optional(),
                            "Ölçülerin birimi: kagit (µm), zemin (mm), piksel"),
                Param::text("boyut_birim", Arity::optional(),
                            "Yalnız `boyut` için birim; verilmezse `birim` geçerlidir"),
                Param::text("aralik_birim", Arity::optional(),
                            "Yalnız `aralik` için birim; verilmezse `birim` geçerlidir"),
                Param::text("aralik_y_birim", Arity::optional(),
                            "Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir"),
                Param::text("kaydirma_birim", Arity::optional(),
                            "Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir"),
                Param::integer("boyut", Arity::optional(),
                               "İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden"),
                Param::integer("aralik", Arity::optional(),
                               "Çizgi boyunca ya da desende birinci eksende aralık"),
                Param::integer("aralik_y", Arity::optional(),
                               "Nokta deseninde ikinci eksen; verilmezse kare desen"),
                Param::integer("aci", Arity::optional(),
                               "Desen açısı ya da işaretçi dönüklüğü, mikro derece"),
                Param::integer("kaydirma", Arity::optional(),
                               "Geometriden dik kaydırma, `birim` cinsinden"),
                Param::integer("faz", Arity::optional(),
                               "İlk işaretçinin çizgi boyunca kaç birim ileride "
                               "başlayacağı; verilmezse aralığın yarısı"),
                Param::text("faz_birim", Arity::optional(),
                            "Yalnız `faz` için birim; verilmezse `birim` geçerlidir"),
                Param::integer("saydamlik", Arity::optional(),
                               "Katman saydamlığı 0-255; 255 tam opak"),
                Param::text("desen", Arity::optional(),
                            "Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak "
                            "çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta)"),
                Param::text("yazi", Arity::optional(),
                            "yazi-isaretci katmanının yazdığı sabit metin"),
                Param::text("alan", Arity::optional(),
                            "yazi-isaretci katmanının okuyacağı öznitelik sütunu; "
                            "yoksa tanımlanır"),
                Param::text("alan_tipi", Arity::optional(),
                            "alan= sütununun türü: tam_sayi, uzunluk, evet_hayir, "
                            "metin, kod"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen "
                   "değerlerden yazar.",
        .run     = &run,
    };
}

} // namespace kentos::command
