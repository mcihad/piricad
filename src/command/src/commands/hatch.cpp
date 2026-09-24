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
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/trig.hpp"

#include <cmath>
#include <numeric>
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
    std::string wanted = "SOLID";
    if (const Value d = ctx.argument("desen"); !d.empty()) wanted = d.as_text();

    std::string configured =
        std::string(ctx.session().bus().app_settings().get("core.tarama.desen_katalogu").as_text());
    if (const Value k = ctx.argument("katalog"); !k.empty()) configured = k.as_text();
    const std::string path = resolve_catalog_path(configured);
    if (path.empty()) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Tarama deseni kataloğu bulunamadı: '" + configured +
                       "'. TERCİH desen_kataloğu ile yolunu kurun ya da katalog= verin.");
        co_return;
    }
    auto catalog = load_hatch_patterns(path);
    if (!catalog) {
        ctx.refuse(catalog.error());
        co_return;
    }
    const HatchPattern* pattern = catalog.value().find(wanted);
    if (pattern == nullptr) {
        std::string known;
        for (const HatchPattern& p : catalog.value().patterns)
            known += (known.empty() ? "" : ", ") + p.id;
        ctx.refuse(core::ErrorCode::InvalidArgument, "Tanınmayan tarama deseni: '" + wanted +
                                                         "'. Katalogdaki desenler: " + known + ".");
        co_return;
    }

    double angle_deg = 0.0;
    if (const Value a = ctx.argument("aci"); !a.empty()) angle_deg = a.as_number();

    // The scale: what the pattern's micrometres are multiplied by. Without one,
    // the plan scale, so a 3,175 mm spacing on paper is 3,175 m on a 1/1000
    // sheet — the pattern looks on the pafta the way its designer drew it.
    double scale =
        static_cast<double>(ctx.session().bus().project_settings().get("core.plan.olcek").as_int());
    if (const Value s = ctx.argument("olcek"); !s.empty()) scale = s.as_number();
    if (!(scale > 0.0)) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Tarama ölçeği sıfırdan büyük olmalı.");
        co_return;
    }

    core::HatchDef def;
    def.name         = pattern->id;
    def.solid        = pattern->families.empty();
    def.pattern_type = 1;
    def.angle_udeg   = static_cast<std::int64_t>(std::llround(angle_deg * 1000000.0));
    def.scale        = ratio_of(scale);
    def.origin       = rings.front().points.front();
    def.families     = pattern->families;

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
    ctx.record("desen", Value::text(def.name));
    ctx.record("aci", Value::number(angle_deg));
    ctx.record("olcek", Value::number(scale));
    std::string said = "'" + def.name + "' deseniyle tarama çizildi (" +
                       std::to_string(rings.size()) + " sınır halkası";
    if (holes > 0) said += ", " + std::to_string(holes) + " delik";
    said += ')';
    if (link && !sources.empty())
        said += "; " + std::to_string(sources.size()) +
                " sınır nesnesine bağlı, o değişince tarama da güncellenir";
    ctx.echo(said + "." +
             (dashes ? " Desenin kesik dizisi bu sürümde çizilmez, dosyada korunur." : ""));
}

} // namespace

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
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.",
        .run = &run,
    };
}

} // namespace kentos::command
