// SPDX-License-Identifier: GPL-3.0-or-later
// core.hatch — TARAMA. A pattern-filled face over a boundary.
//
// The boundary is either a set of closed entities the user selects — a parcel,
// a circle, a closed polyline — or a ring of points typed straight in. The
// pattern comes from the catalogue under /data/catalogs/dxf by name (CLAUDE.md
// 5.13: the angles and spacings are data), and the hatch is drawn through a
// Symbol interned here, at commit, so the frame path reads one u32 (model.md
// R14). What is stored is the definition (core/hatch.hpp): the loops and the
// pattern, so the hatch goes out to DXF as a HATCH and comes back as one.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// A rational from a typed scale: six decimals, reduced. `2` is 2/1, `0.5` is 1/2.
core::Ratio ratio_of(double v)
{
    const auto num       = static_cast<std::int64_t>(std::llround(v * 1000000.0));
    std::int64_t den     = 1000000;
    const std::int64_t g = std::gcd(num < 0 ? -num : num, den);
    if (g > 1) return core::Ratio{num / g, den / g};
    return core::Ratio{num, den};
}

/// The name a user-defined pattern goes by — AutoCAD's, so a DXF reader knows it.
constexpr const char* kUserPattern = "_USER";

/// The pattern as a sentence names it: a catalogue pattern by its name, one of
/// the user's own by its spacing — `_USER` is the file's word, not the user's.
/// `with`: the instrumental case, "… deseniyle".
std::string pattern_phrase(const core::HatchDef& def, bool with)
{
    if (def.pattern_type == 0 && def.families.size() == 1)
        return metres_text(core::hatch_family_spacing_mm(def, def.families.front())) +
               (with ? " m aralıklı kendi deseninizle" : " m aralıklı kendi deseniniz");
    return "'" + def.name + (with ? "' deseniyle" : "' deseni");
}

/// Reads the pattern onto `def`: `desen` from the catalogue, or `aralik` for a
/// set of lines of the user's own; then `aci`, `olcek`, `cift`, `baslangic`.
/// `fresh` is a new hatch — SOLID and the plan scale unless named, and every
/// resolved value recorded; otherwise only what is named changes (TARAMADÜZENLE).
/// False after refusing.
bool read_pattern(Context& ctx, core::HatchDef& def, bool fresh)
{
    const Value named   = ctx.argument("desen");
    const Value spacing = ctx.argument("aralik");
    if (!named.empty() && !spacing.empty() &&
        !core::turkish_key_equals(named.as_text(), kUserPattern)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "aralik kendi çizdiğiniz desenin aralığıdır; katalogdaki bir desenle birlikte "
                   "verilmez: ya desen=… ya aralik=…");
        return false;
    }
    bool from_catalogue = false;
    if (!spacing.empty()) {
        // A USER-DEFINED PATTERN: one family of lines this far apart, in metres
        // on the ground — no catalogue, no scale (DXF pattern type 0).
        if (!(spacing.as_number() > 0.0)) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Tarama aralığı sıfırdan büyük olmalı.");
            return false;
        }
        core::HatchDef::Family lines;
        lines.offset_y_um =
            static_cast<std::int64_t>(std::llround(spacing.as_number() * 1'000'000.0));
        def.name         = kUserPattern;
        def.solid        = false;
        def.pattern_type = 0;
        def.scale        = core::Ratio{1, 1};
        def.families     = {lines};
        ctx.record("aralik", spacing);
    } else if (!named.empty() || fresh) {
        std::string wanted = "SOLID";
        if (!named.empty()) wanted = named.as_text();
        std::string configured = std::string(
            ctx.session().bus().app_settings().get("core.tarama.desen_katalogu").as_text());
        if (const Value k = ctx.argument("katalog"); !k.empty()) configured = k.as_text();
        const std::string path = resolve_catalog_path(configured);
        if (path.empty()) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Tarama deseni kataloğu bulunamadı: '" + configured +
                           "'. TERCİH desen_kataloğu ile yolunu kurun ya da katalog= verin.");
            return false;
        }
        auto catalog = load_hatch_patterns(path);
        if (!catalog) {
            ctx.refuse(catalog.error());
            return false;
        }
        const HatchPattern* pattern = catalog.value().find(wanted);
        if (pattern == nullptr) {
            std::string known;
            for (const HatchPattern& p : catalog.value().patterns)
                known += (known.empty() ? "" : ", ") + p.id;
            ctx.refuse(core::ErrorCode::InvalidArgument, "Tanınmayan tarama deseni: '" + wanted +
                                                             "'. Katalogdaki desenler: " + known +
                                                             ".");
            return false;
        }
        def.name         = pattern->id;
        def.solid        = pattern->families.empty();
        def.pattern_type = 1;
        def.families     = pattern->families;
        from_catalogue   = true;
        if (def.scale.den <= 0 || def.scale.num <= 0 || fresh) def.scale = core::Ratio{1, 1};
        ctx.record("desen", Value::text(def.name));
    }

    const Value angle = ctx.argument("aci");
    if (!angle.empty() || fresh) {
        const double degrees = angle.empty() ? 0.0 : angle.as_number();
        def.angle_udeg       = static_cast<std::int64_t>(std::llround(degrees * 1000000.0));
        ctx.record("aci", Value::number(degrees));
    }

    // The scale: what the pattern's micrometres are multiplied by. Without one,
    // the plan scale, so a 3,175 mm spacing on paper is 3,175 m on a 1/1000
    // sheet — the pattern looks on the pafta the way its designer drew it. A
    // pattern of the user's own is already in metres and takes none.
    const Value scale = ctx.argument("olcek");
    if (def.pattern_type != 0 && (!scale.empty() || (fresh && from_catalogue))) {
        const double v =
            scale.empty()
                ? static_cast<double>(
                      ctx.session().bus().project_settings().get("core.plan.olcek").as_int())
                : scale.as_number();
        if (!(v > 0.0)) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Tarama ölçeği sıfırdan büyük olmalı.");
            return false;
        }
        def.scale = ratio_of(v);
        ctx.record("olcek", Value::number(v));
    }
    if (const Value twice = ctx.argument("cift"); !twice.empty()) {
        def.double_lines = twice.as_bool();
        ctx.record("cift", twice);
    }
    if (const Value at = ctx.argument("baslangic"); !at.empty()) {
        def.origin = at.as_point();
        ctx.record("baslangic", at);
    }
    return true;
}

Task<void> run(Context& ctx)
{
    // ---- the boundary -------------------------------------------------------
    std::vector<std::vector<core::Point2>> store;
    std::vector<core::RingGeometry::RingInput> rings;
    std::vector<std::int64_t> requested;
    std::vector<core::EntityId> sources;
    std::vector<core::Point2> typed;
    core::HatchBoundary boundary;

    if (ctx.has_argument("noktalar")) {
        // The corners, one awaited point at a time, the way ÇOKLUÇİZGİ reads
        // its own: a typed list and a JSON array both arrive this way.
        while (auto p = co_await ctx.point(
                   "noktalar", "Sınır köşesi",
                   PointOptions{.rubber_band   = !typed.empty(),
                                .rubber_origin = typed.empty() ? core::Point2{} : typed.back(),
                                .rubber_shape  = RubberShape::Ring,
                                .rubber_chain  = typed}))
            typed.push_back(*p);
        if (typed.size() < 3) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tarama sınırı en az üç nokta ister; verilen " +
                           std::to_string(typed.size()) + ".");
            co_return;
        }
        if (typed.size() > 1 && typed.front() == typed.back()) typed.pop_back();
        store.push_back(typed);
        rings.push_back(core::RingGeometry::RingInput{store.back(), core::RingRole::Exterior, 0});
    } else {
        if (!co_await want_objects(ctx, "nesneler", "Taranacak kapalı nesneleri seçin, sonra Enter",
                                   requested, 0, "TARAMA nesneler=1 desen=ANSI31"))
            co_return;
        for (const std::int64_t raw : requested) {
            if (raw <= 0) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Geçersiz nesne kimliği: " + std::to_string(raw) +
                               ". Kimlikler 1'den başlar.");
                co_return;
            }
            const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
            const core::EntityId slot = ctx.document().slot_of(key);
            if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
                ctx.refuse(core::ErrorCode::NotFound,
                           "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
                co_return;
            }
            if (core::closed_loops_of(ctx.document(), slot).empty()) {
                ctx.refuse(
                    core::ErrorCode::InvalidArgument,
                    "Nesne " + std::to_string(raw) +
                        " kapalı değil; tarama sınırı kapalı bir alan, daire, elips ya da kapalı "
                        "çoklu çizgi olmalı.");
                co_return;
            }
            sources.push_back(slot);
        }
        // HOLES BY NESTING (core/hatch_link.hpp): a parcel's courtyard, and a
        // selected object inside another, are islands the pattern stays out of.
        auto made = core::hatch_boundary(ctx.document(), sources, 0);
        if (!made) {
            ctx.refuse(made.error());
            co_return;
        }
        boundary = std::move(made.value());
        rings    = boundary.rings();
    }
    // `store` is complete now; the spans in `rings` point into it and must not
    // be taken before it stops growing.
    if (!typed.empty()) rings.front().points = std::span<const core::Point2>(store.front());

    // ---- the pattern --------------------------------------------------------
    //
    // THROUGH THE DRAWING'S ORIGIN unless `baslangic` says otherwise — AutoCAD's
    // default too. Every hatch of one pattern then lies on one lattice: two
    // parcels hatched one at a time meet without a seam along the edge they
    // share, and a thousand of them draw as one pass rather than a thousand
    // (render.md R7). A pattern set on each boundary's first corner did
    // neither.
    core::HatchDef def;
    if (!read_pattern(ctx, def, true)) co_return;

    // ---- the entity and its symbol ------------------------------------------
    const core::LayerId layer               = ctx.active_layer();
    const std::vector<std::uint8_t> payload = core::encode_hatch(def);
    auto created = ctx.transaction().add_kind(layer, core::kHatchKind, rings, payload);
    if (!created) {
        ctx.refuse(created.error());
        co_return;
    }
    // Resolved AT COMMIT (model.md R14): the layer's ink is what the hatch is
    // drawn in, written into the style column now, never looked up per frame.
    std::uint32_t ink = 0xFF000000u;
    if (layer < ctx.document().layers().size())
        ink = ctx.document().layers()[layer].appearance.rgba;
    const core::StyleId style = ctx.transaction().intern_symbol(hatch_symbol(def, ink));
    if (auto st = ctx.transaction().set_entity_style(created.value(), style); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    // FOLLOWS ITS BOUNDARY (TODOS C-11): the objects it was drawn over, by key;
    // at every commit that reshapes one of them its loops are built again.
    const bool link = !ctx.has_argument("bagla") || ctx.argument("bagla").as_bool(true);
    if (link && !sources.empty()) {
        std::vector<core::HatchSource> tied;
        tied.reserve(sources.size());
        for (const core::EntityId src : sources)
            tied.push_back(core::HatchSource{ctx.document().entities().key[src], false});
        if (auto st = ctx.transaction().set_hatch_links(created.value(), tied); !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }
    if (!link) ctx.record("bagla", Value::boolean(false));

    bool dashes = false;
    for (const core::HatchDef::Family& f : def.families)
        if (!f.dashes_um.empty()) dashes = true;
    std::size_t holes = 0;
    for (const core::RingRole role : boundary.roles)
        holes += role == core::RingRole::Interior ? 1 : 0;

    if (!typed.empty())
        ctx.record("noktalar", Value::points(typed));
    else
        ctx.record("nesneler", Value::ids(requested));
    std::string said = pattern_phrase(def, true) + (def.double_lines ? " çapraz" : "") +
                       " tarama çizildi (" + std::to_string(rings.size()) + " sınır halkası";
    if (holes > 0) said += ", " + std::to_string(holes) + " delik";
    said += ')';
    if (link && !sources.empty())
        said += "; " + std::to_string(sources.size()) +
                " sınır nesnesine bağlı, o değişince tarama da güncellenir";
    ctx.echo(said + "." +
             (dashes ? " Desenin kesik dizisi bu sürümde çizilmez, dosyada korunur." : ""));
}

// ------------------------------------------------------------ TARAMADÜZENLE ----

/// The DXF island style (group 75) a `stil=` word names.
std::uint16_t island_style(std::string_view word)
{
    if (core::turkish_key_equals(word, "dis")) return 1;
    if (core::turkish_key_equals(word, "yoksay")) return 2;
    return 0;
}

Task<void> run_edit(Context& ctx)
{
    std::vector<std::int64_t> picked;
    if (!co_await want_objects(ctx, "nesneler", "Düzenlenecek taramaları seçin, sonra Enter",
                               picked, 0, "TARAMADÜZENLE nesneler=1 desen=ANSI37",
                               core::kHatchKind))
        co_return;
    const core::Document& doc = ctx.document();
    std::vector<core::EntityId> hatches;
    std::size_t other = 0;
    const auto take   = [&hatches](core::EntityId h) {
        if (std::ranges::find(hatches, h) == hatches.end()) hatches.push_back(h);
    };
    for (const std::int64_t raw : picked) {
        const auto key         = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId e = doc.slot_of(key);
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        if (doc.entities().kind[e] == core::kHatchKind) {
            take(e);
            continue;
        }
        // THE PARCEL MEANS ITS HATCH. A hatch lies on the edges of what it
        // fills, so the parcel is what a hand points at, and what a surveyor
        // names: the hatches that follow an object are the ones it asks about.
        bool bound = false;
        for (const core::EntityId h : doc.hatch_links().linked()) {
            if (!doc.alive(h)) continue;
            for (const core::HatchSource& src : *doc.hatch_links().get(h))
                if (src.source == key && !src.broken) {
                    take(h);
                    bound = true;
                }
        }
        if (!bound) ++other;
    }
    if (hatches.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Seçimde tarama yok; TARAMADÜZENLE yalnız taramaları düzenler.");
        co_return;
    }

    // NOTHING NAMED IS A QUESTION: the pattern, the first hatch's own offered.
    bool named = false;
    for (const char* p : {"desen", "aralik", "aci", "olcek", "cift", "baslangic", "stil"})
        named = named || ctx.has_argument(p);
    if (!named) {
        std::vector<std::string> offer;
        if (auto now = core::hatch_of(doc.geometry(), doc.entities().slot[hatches.front()]); now)
            offer.push_back(now.value().name);
        const auto typed = co_await ctx.text("desen", "Taramanın deseni", offer);
        if (!typed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Değiştirilecek bir şey verilmedi: desen=, aralik=, aci=, olcek=, cift=, "
                       "baslangic= ya da stil=.");
            co_return;
        }
    }

    const Value style_word = ctx.argument("stil");
    std::string said;
    for (const core::EntityId e : hatches) {
        if (const auto st = doc.editable(e); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        const std::uint32_t row = doc.entities().slot[e];
        auto stored             = core::hatch_of(doc.geometry(), row);
        if (!stored) {
            ctx.refuse(stored.error());
            co_return;
        }
        core::HatchDef def = stored.value();
        if (!read_pattern(ctx, def, false)) co_return;

        // THE ISLANDS AGAIN, when their rule changed: from the boundary objects
        // while the hatch follows them, else from its own loops — which can
        // only lose islands, never find ones it no longer has.
        std::optional<core::HatchBoundary> rebuilt;
        if (!style_word.empty()) {
            def.style = island_style(style_word.as_text());
            ctx.record("stil", style_word);
            const std::vector<core::HatchSource>* sources = doc.hatch_links().get(e);
            std::vector<core::EntityId> live;
            bool broken = sources == nullptr;
            if (sources != nullptr)
                for (const core::HatchSource& s : *sources) {
                    const core::EntityId src = doc.slot_of(s.source);
                    broken = broken || s.broken || src == core::kNoEntity || !doc.alive(src);
                    if (!broken) live.push_back(src);
                }
            if (!broken) {
                auto made = core::hatch_boundary(doc, live, def.style);
                if (made) rebuilt = std::move(made.value());
            }
            if (!rebuilt) {
                std::vector<std::vector<core::Point2>> loops;
                const core::RingSpan span = doc.geometry().rings_of(row);
                for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                    const auto xs = doc.geometry().ring_xs(r);
                    const auto ys = doc.geometry().ring_ys(r);
                    std::vector<core::Point2> pts;
                    pts.reserve(xs.size());
                    for (std::size_t v = 0; v < xs.size(); ++v)
                        pts.push_back(core::Point2{xs[v], ys[v]});
                    loops.push_back(std::move(pts));
                }
                rebuilt = core::nest_loops(std::move(loops), def.style);
            }
        }
        const std::vector<std::uint8_t> payload = core::encode_hatch(def);
        auto st = rebuilt ? ctx.transaction().set_kind_geometry(e, rebuilt->rings(), payload)
                          : ctx.transaction().set_kind_payload(e, payload);
        if (!st) {
            ctx.refuse(st.error());
            co_return;
        }
        // DRAWN BY ITS STYLE (model.md R14): the new pattern is a new symbol.
        const core::StyleId was = doc.entities().style[e];
        std::uint32_t ink       = 0xFF000000u;
        if (was != core::kByLayerStyle && was < doc.styles().size())
            ink = doc.styles().symbol_at(was).primary().rgba;
        if (auto s = ctx.transaction().set_entity_style(
                e, ctx.transaction().intern_symbol(hatch_symbol(def, ink)));
            !s) {
            ctx.refuse(s.error());
            co_return;
        }
        if (said.empty()) {
            said = pattern_phrase(def, false);
            if (def.angle_udeg != 0)
                said += ", açı " + core::format_dimension_angle(def.angle_udeg, 2, ',');
            if (def.double_lines) said += ", çapraz";
        }
    }
    ctx.record("nesneler", Value::ids(picked));
    ctx.echo("Tarama düzenlendi: " + std::to_string(hatches.size()) + " tarama; " + said +
             (other > 0 ? "; tarama olmayan " + std::to_string(other) + " nesne atlandı" : "") +
             ".");
}

} // namespace

KENTOS_COMMAND(hatch_edit)
{
    return CommandSpec{
        .id       = "core.hatch_edit",
        .names    = {"TARAMADÜZENLE", "TARAMADUZENLE", "HATCHEDIT", "TDZ"},
        .title    = "Tarama Düzenle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Düzenlenecek taramalar; verilmezse seçim, o da boşsa sorulur"}
                    .en("objects"),
                Param::text("desen", Arity::optional(), "Katalogdaki desen adı").en("pattern"),
                Param::number("aralik", Arity::optional(),
                              "Kendi desen çizgilerinizin aralığı, metre; desen= yerine")
                    .measured_in("m")
                    .en("spacing"),
                Param::number("aci", Arity::optional(), "Desenin dönme açısı, derece").en("angle"),
                Param::number("olcek", Arity::optional(), "Desen ölçeği").en("scale"),
                Param::boolean("cift", Arity::optional(),
                               "Desen bir de dik açıyla çizilsin mi (çapraz tarama)")
                    .en("double"),
                Param::points("baslangic", Arity::optional(), "Desenin geçtiği nokta").en("origin"),
                Param::choice("stil", Arity::optional(), {"normal", "dis", "yoksay"},
                              "Adalar: normal — iç içe sırayla delik ve dolu; dis — yalnız en "
                              "dıştaki ve ilk delikler; yoksay — adasız")
                    .en("islands"),
                Param::text("katalog", Arity::optional(),
                            "Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu")
                    .en("catalog"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizilmiş taramanın desenini, açısını, ölçeğini, aralığını, başlangıcını ya "
                   "da ada kuralını değiştirir; bağı ve sınırı korunur.",
        .run = &run_edit,
    };
}

KENTOS_COMMAND(hatch)
{
    return CommandSpec{
        .id       = "core.hatch",
        .names    = {"TARAMA", "TARAMA", "HATCH", "TRM"},
        .title    = "Tarama",
        .category = Category::Draw,
        .params =
            {
                // The corners come first so a typed run of points fills them:
                // the bus hands positional values to the first unsatisfied
                // parameter, and a selection is never typed as a point.
                Param::points("noktalar", Arity{0, 0xFFFFFFFFu},
                              "Sınır köşeleri, nesne seçmek yerine; en az üç nokta")
                    .en("points"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Sınırı verecek kapalı nesneler; yoksa etkin seçim ya da noktalar="}
                    .en("objects"),
                Param::text("desen", Arity::optional(),
                            "Katalogdaki desen adı: SOLID, ANSI31, NET…; varsayılan SOLID")
                    .en("pattern"),
                Param::number("aci", Arity::optional(), "Desenin dönme açısı, derece; varsayılan 0")
                    .en("angle"),
                Param::number("olcek", Arity::optional(),
                              "Desen ölçeği; varsayılan pafta ölçeğinin paydası (AYAR plan_ölçeği)")
                    .en("scale"),
                Param::text("katalog", Arity::optional(),
                            "Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu")
                    .en("catalog"),
                Param::boolean("bagla", Arity::optional(),
                               "Seçilen sınır nesnelerine bağlansın mı; bağlı tarama sınırı "
                               "değişince yeniden kurulur. Varsayılan evet")
                    .en("associate"),
                Param::number("aralik", Arity::optional(),
                              "Kendi desen çizgilerinizin aralığı, metre; desen= yerine")
                    .measured_in("m")
                    .en("spacing"),
                Param::boolean("cift", Arity::optional(),
                               "Desen bir de dik açıyla çizilsin mi (çapraz tarama)")
                    .en("double"),
                Param::points("baslangic", Arity::optional(),
                              "Desenin geçtiği nokta; verilmezse çizimin başlangıç noktası (0,0)")
                    .en("origin"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.",
        .run = &run,
    };
}

} // namespace kentos::command
