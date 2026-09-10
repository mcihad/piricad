// SPDX-License-Identifier: GPL-3.0-or-later
// core.dimension — ÖLÇÜ, and core.leader — LİDER.
//
// ÖLÇÜ measures and shows: two points, a line location, and the figures of a
// dimension STYLE from the catalogue under /data/catalogs/dxf (arrow, extension
// lines, text height, decimals — data, not code, CLAUDE.md 5.13), brought from
// paper micrometres to ground millimetres by the plan scale (model.md R20). The
// text is computed here, in integers, and stored as the entity's caption; the
// lines are rebuilt from the definition every frame (core/dimension.hpp).
//
// LİDER is an arrowed line; its text, when given, is a separate METİN-like
// entity at the last vertex, the way every CAD format keeps them.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The style the command draws with, in GROUND millimetres, or nothing after
/// the reason was echoed.
struct GroundStyle
{
    core::DimensionDef figures; ///< arrow, extensions, gap, precision, separator, style name
    core::Mm text_height{2500};
};

std::optional<GroundStyle> style_for(Context& ctx)
{
    std::string configured =
        std::string(ctx.session().bus().app_settings().get("core.olcu.stil_katalogu").as_text());
    if (const Value k = ctx.argument("katalog"); !k.empty()) configured = k.as_text();
    const std::string path = resolve_catalog_path(configured);
    if (path.empty()) {
        ctx.echo("Ölçü stili kataloğu bulunamadı: '" + configured +
                 "'. TERCİH ölçü_stilleri ile yolunu kurun ya da katalog= verin.");
        return std::nullopt;
    }
    auto catalog = load_dimension_styles(path);
    if (!catalog) {
        ctx.echo(catalog.error().message);
        return std::nullopt;
    }
    std::string wanted = "ISO-25";
    if (const Value s = ctx.argument("stil"); !s.empty()) wanted = s.as_text();
    const DimensionStyle* style = catalog.value().find(wanted);
    if (style == nullptr) {
        std::string known;
        for (const DimensionStyle& s : catalog.value().styles)
            known += (known.empty() ? "" : ", ") + s.id;
        ctx.echo("Tanınmayan ölçü stili: '" + wanted + "'. Katalogdaki stiller: " + known + ".");
        return std::nullopt;
    }

    // Paper micrometres to ground millimetres by the plan scale: 2500 µm on a
    // 1/1000 sheet is 2,5 m on the ground.
    const std::int64_t denominator =
        ctx.session().bus().project_settings().get("core.plan.olcek").as_int();
    const auto ground = [denominator](std::int32_t um) {
        return core::mul_div_round(um, denominator, 1000);
    };
    GroundStyle out;
    out.figures.arrow             = style->arrow;
    out.figures.arrow_size        = ground(style->arrow_um);
    out.figures.extension_beyond  = ground(style->extension_beyond_um);
    out.figures.extension_offset  = ground(style->extension_offset_um);
    out.figures.text_gap          = ground(style->text_gap_um);
    out.figures.precision         = style->precision;
    out.figures.decimal_separator = style->decimal_separator;
    out.figures.style             = style->id;
    out.text_height               = std::max<core::Mm>(1, ground(style->text_height_um));
    return out;
}

core::DrawingUnit drawing_unit(Context& ctx)
{
    return core::drawing_unit_from_setting(
        ctx.session().bus().project_settings().get("core.cizim.birim").as_enum());
}

// ------------------------------------------------------------------- ÖLÇÜ ----

Task<void> run_dimension(Context& ctx)
{
    core::DimensionType type = core::DimensionType::Aligned;
    if (const Value t = ctx.argument("tur"); !t.empty()) {
        const std::string& w = t.as_text();
        if (core::turkish_key_equals(w, "hizali") || core::turkish_key_equals(w, "aligned"))
            type = core::DimensionType::Aligned;
        else if (core::turkish_key_equals(w, "dogrusal") || core::turkish_key_equals(w, "linear"))
            type = core::DimensionType::Linear;
        else if (core::turkish_key_equals(w, "yaricap") || core::turkish_key_equals(w, "radius"))
            type = core::DimensionType::Radial;
        else if (core::turkish_key_equals(w, "cap") || core::turkish_key_equals(w, "diameter"))
            type = core::DimensionType::Diametric;
        else if (core::turkish_key_equals(w, "acisal") || core::turkish_key_equals(w, "angular"))
            type = core::DimensionType::Angular3P;
        else {
            ctx.echo("Tanınmayan ölçü türü: '" + w +
                     "'. Türler: hizali, dogrusal, yaricap, cap, acisal.");
            co_return;
        }
    }

    const auto style = style_for(ctx);
    if (!style) co_return;

    core::DimensionDef def = style->figures;
    def.type               = type;
    if (const Value m = ctx.argument("metin"); !m.empty()) def.override_text = m.as_text();

    const bool angular = type == core::DimensionType::Angular3P;
    auto p1 = co_await ctx.point("birinci", angular ? "Birinci kolun ucu" : "Birinci nokta");
    if (!p1) co_return;
    auto p2 = co_await ctx.point(
        "ikinci", angular ? "İkinci kolun ucu" : "İkinci nokta",
        PointOptions{.rubber_band = true, .rubber_origin = *p1, .rubber_shape = RubberShape::Line});
    if (!p2) co_return;
    std::vector<core::Point2> picks{*p1, *p2};
    if (angular) {
        auto v = co_await ctx.point("tepe", "Açının tepe noktası");
        if (!v) co_return;
        picks.push_back(*v);
    }
    // The whole dimension under the cursor — line, extension lines, arrows —
    // laid out by the same function that will lay it out on the click.
    auto where = co_await ctx.point(
        "konum", angular ? "Ölçü yayının geçeceği nokta" : "Ölçü çizgisinin yeri",
        PointOptions{.rubber_band    = true,
                     .rubber_origin  = *p1,
                     .rubber_shape   = RubberShape::Dimension,
                     .rubber_chain   = picks,
                     .rubber_payload = core::encode_dimension(def)});
    if (!where) co_return;

    if (*p1 == *p2) {
        ctx.echo("İki nokta aynı; ölçülecek bir uzunluk yok.");
        co_return;
    }

    // The definition points, the caption's place and the measurement, from the
    // one layout every client of a dimension shares (core/dimension.hpp).
    core::DimensionLayout layout;
    if (!core::dimension_layout(def, picks, *where, style->text_height, layout)) {
        ctx.echo(angular ? "Açının tepe noktası kolların ucuyla aynı olamaz."
                         : "Ölçü bu noktalarla kurulamıyor.");
        co_return;
    }
    if (def.measurement == 0 && def.override_text.empty()) {
        ctx.echo("Ölçü sıfır çıktı; noktalar ölçülecek bir uzunluk ya da açı vermiyor.");
        co_return;
    }
    const std::string text = core::dimension_text(def, drawing_unit(ctx));

    const auto base = core::dimension_baseline(layout.text_centre, layout.text_dir_x,
                                               layout.text_dir_y, style->text_height, text);
    const std::vector<core::Point2>& defs = layout.defs;
    const std::vector<core::RingGeometry::RingInput> rings{
        core::RingGeometry::RingInput{base, core::RingRole::Open, 0},
        core::RingGeometry::RingInput{defs, core::RingRole::Open, 0},
    };
    const std::vector<std::uint8_t> payload = core::encode_dimension(def);
    auto created =
        ctx.transaction().add_kind(ctx.active_layer(), core::kDimensionKind, rings, payload);
    if (!created) {
        ctx.echo(created.error().message);
        co_return;
    }
    if (auto st = ctx.transaction().set_text(created.value(), text, style->text_height,
                                             core::TextAnchor::MiddleCentre);
        !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.record("birinci", Value::point(*p1));
    ctx.record("ikinci", Value::point(*p2));
    if (angular) ctx.record("tepe", Value::point(picks[2]));
    ctx.record("konum", Value::point(*where));
    ctx.record("tur", Value::text(core::dimension_type_name(type)));
    ctx.record("stil", Value::text(def.style));
    if (!def.override_text.empty()) ctx.record("metin", Value::text(def.override_text));
    ctx.echo("Ölçü çizildi: " + text + " (" + def.style + ").");
}

// ------------------------------------------------------------------ LİDER ----

Task<void> run_leader(Context& ctx)
{
    std::vector<core::Point2> points;
    auto first = co_await ctx.point("noktalar", "Okun ucu: gösterilen nokta");
    if (!first) co_return;
    points.push_back(*first);
    core::Point2 previous = *first;
    while (
        auto next = co_await ctx.point(
            "noktalar", "Sonraki köşe",
            PointOptions{.rubber_band = true, .rubber_origin = previous, .rubber_chain = points})) {
        points.push_back(*next);
        previous = *next;
    }
    if (points.size() < 2) {
        ctx.echo("Bir lider en az iki nokta ister: okun ucu ve yazının yanı.");
        co_return;
    }

    const auto style = style_for(ctx);
    if (!style) co_return;

    core::LeaderDef def;
    def.arrow      = true;
    def.arrow_size = style->figures.arrow_size;
    const core::RingGeometry::RingInput ring{points, core::RingRole::Open, 0};
    const std::vector<std::uint8_t> payload = core::encode_leader(def);
    auto created                            = ctx.transaction().add_kind(
        ctx.active_layer(), core::kLeaderKind,
        std::span<const core::RingGeometry::RingInput>(&ring, 1), payload);
    if (!created) {
        ctx.echo(created.error().message);
        co_return;
    }

    std::string words;
    if (const Value m = ctx.argument("metin"); !m.empty()) words = m.as_text();
    if (!words.empty()) {
        // The caption beside the last vertex, a gap away, reading to the right —
        // its own entity, so it moves and edits like any METİN.
        const core::Point2 last = points.back();
        const core::Point2 start{last.x + style->figures.text_gap, last.y - style->text_height / 2};
        const auto base = core::dimension_baseline(start, 1.0, 0.0, style->text_height, words);
        auto caption    = ctx.transaction().add_polyline(ctx.active_layer(), base);
        if (!caption) {
            ctx.echo(caption.error().message);
            co_return;
        }
        if (auto st = ctx.transaction().set_text(caption.value(), words, style->text_height,
                                                 core::TextAnchor::BaselineLeft);
            !st) {
            ctx.echo(st.error().message);
            co_return;
        }
    }

    ctx.record("noktalar", Value::points(points));
    ctx.record("stil", Value::text(style->figures.style));
    if (!words.empty()) ctx.record("metin", Value::text(words));
    ctx.echo(std::to_string(points.size()) + " noktalı lider çizildi" +
             (words.empty() ? "." : ": " + words));
}

} // namespace

KENTOS_COMMAND(dimension)
{
    return CommandSpec{
        .id       = "core.dimension",
        .names    = {"ÖLÇÜ", "OLCU", "DIMENSION", "ÖÇ"},
        .category = Category::Draw,
        .params =
            {
                Param::point("birinci", "Birinci nokta; açısal ölçüde birinci kolun ucu"),
                Param::point("ikinci", "İkinci nokta; açısal ölçüde ikinci kolun ucu"),
                Param::point("konum", "Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta"),
                Param::text("tur", Arity::optional(),
                            "hizali (varsayılan), dogrusal, yaricap, cap, acisal"),
                Param::points("tepe", Arity::optional(), "Açısal ölçünün tepe noktası"),
                Param::text("stil", Arity::optional(),
                            "Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI"),
                Param::text("metin", Arity::optional(), "Ölçülen değer yerine yazılacak metin"),
                Param::text("katalog", Arity::optional(),
                            "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.",
        .run = &run_dimension,
    };
}

KENTOS_COMMAND(leader)
{
    return CommandSpec{
        .id       = "core.leader",
        .names    = {"LİDER", "LIDER", "LEADER", "LD"},
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::at_least(2),
                              "Okun ucundan yazının yanına köşeler"),
                Param::text("metin", Arity::optional(), "Son köşenin yanına yazılacak metin"),
                Param::text("stil", Arity::optional(),
                            "Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25"),
                Param::text("katalog", Arity::optional(),
                            "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.",
        .run     = &run_leader,
    };
}

} // namespace kentos::command
