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
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trig.hpp"

#include <algorithm>
#include <array>
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

/// The style catalogue the command reads, or nothing after the reason was
/// echoed.
std::optional<DimensionStyleCatalog> catalog_for(Context& ctx)
{
    std::string configured =
        std::string(ctx.session().bus().app_settings().get("core.olcu.stil_katalogu").as_text());
    if (const Value k = ctx.argument("katalog"); !k.empty()) configured = k.as_text();
    const std::string path = resolve_catalog_path(configured);
    if (path.empty()) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Ölçü stili kataloğu bulunamadı: '" + configured +
                       "'. TERCİH ölçü_stilleri ile yolunu kurun ya da katalog= verin.");
        return std::nullopt;
    }
    auto catalog = load_dimension_styles(path);
    if (!catalog) {
        ctx.refuse(catalog.error());
        return std::nullopt;
    }
    return std::move(catalog.value());
}

/// The project's plan scale, as its denominator.
std::int64_t plan_scale(Context& ctx)
{
    return ctx.session().bus().project_settings().get("core.plan.olcek").as_int();
}

/// Style `wanted` laid out for a 1/`denominator` sheet, or nothing after the
/// reason was echoed.
std::optional<GroundStyle> style_for(Context& ctx, const std::string& wanted,
                                     std::int64_t denominator)
{
    const auto catalog = catalog_for(ctx);
    if (!catalog) return std::nullopt;
    const DimensionStyle* style = catalog->find(wanted);
    if (style == nullptr) {
        std::string known;
        for (const DimensionStyle& s : catalog.value().styles)
            known += (known.empty() ? "" : ", ") + s.id;
        ctx.refuse(core::ErrorCode::NotFound,
                   "Tanınmayan ölçü stili: '" + wanted + "'. Katalogdaki stiller: " + known + ".");
        return std::nullopt;
    }

    // Paper micrometres to ground millimetres by the sheet scale: 2500 µm on a
    // 1/1000 sheet is 2,5 m on the ground.
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
    out.figures.scale_basis       = denominator;
    out.text_height               = std::max<core::Mm>(1, ground(style->text_height_um));
    return out;
}

/// The style a new dimension is drawn with: `stil=`, else the project's
/// (`AYAR ölçü_stili`), for the plan scale.
std::string wanted_style(Context& ctx)
{
    if (const Value s = ctx.argument("stil"); !s.empty()) return s.as_text();
    return std::string(ctx.session().bus().project_settings().get("core.olcu.stil").as_text());
}

std::optional<GroundStyle> style_for(Context& ctx)
{
    return style_for(ctx, wanted_style(ctx), plan_scale(ctx));
}

core::DrawingUnit drawing_unit(Context& ctx)
{
    return core::drawing_unit_from_setting(
        ctx.session().bus().project_settings().get("core.cizim.birim").as_enum());
}

// ------------------------------------------------------- presentation ----
//
// HOW THE FIGURE IS WRITTEN, never what it is (TODOS C-10). ÖLÇÜ and ÖLÇÜDÜZENLE
// read the same parameters through the same function, declared once below, so a
// prefix typed at the drawing and one typed at an edit are the same prefix.

/// The `DimensionDef::unit` of an angle unit: AngleUnit + 1.
std::uint8_t angle_code(core::AngleUnit unit)
{
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(unit) + 1);
}

/// Reads the presentation parameters into `def` and records each one given.
/// False after refusing.
bool read_presentation(Context& ctx, core::DimensionDef& def)
{
    for (const char* name : {"onek", "sonek"}) {
        const Value v = ctx.argument(name);
        if (v.empty()) continue;
        (std::string_view(name) == "onek" ? def.prefix : def.suffix) = v.as_text();
        ctx.record(name, v);
    }
    if (const Value v = ctx.argument("birim"); !v.empty()) {
        // AN ANGLE IS NOT WRITTEN IN METRES, nor a length in grad: the two word
        // lists share one parameter, and the wrong one for the type is said.
        const std::string& word = v.as_text();
        const auto code         = core::dimension_unit_code(def, word);
        if (!code) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       core::dimension_is_angle(def)
                           ? "'" + word +
                                 "' bir uzunluk birimi; açı ölçüsü grad, derece ya da radyan "
                                 "yazar."
                           : "'" + word +
                                 "' bir açı birimi; bu ölçü bir uzunluk yazar: cizim, mm, cm, m "
                                 "ya da km.");
            return false;
        }
        def.unit = *code;
        ctx.record("birim", v);
    }
    if (const Value v = ctx.argument("hassasiyet"); !v.empty()) {
        def.precision = static_cast<std::uint8_t>(v.as_int());
        ctx.record("hassasiyet", v);
    }

    // A TOLERANCE IS A LENGTH OR AN ANGLE, in the units every other parameter
    // of this program is given in: metres, or degrees for an angular dimension.
    const bool angle =
        def.type == core::DimensionType::Angular || def.type == core::DimensionType::Angular3P;
    const auto amount = [angle](double v) {
        return angle ? static_cast<std::int64_t>(std::llround(v * 1'000'000.0))
                     : core::mm_round(v * 1000.0);
    };
    const Value both = ctx.argument("tolerans");
    const Value up   = ctx.argument("tolerans_ust");
    const Value down = ctx.argument("tolerans_alt");
    const Value how  = ctx.argument("tolerans_bicim");
    for (const Value* v : {&both, &up, &down})
        if (!v->empty() && v->as_number() < 0.0) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tolerans pozitif yazılır; aşağı sapma tolerans_alt= ile verilir.");
            return false;
        }
    if (!both.empty()) {
        def.tolerance_plus  = amount(both.as_number());
        def.tolerance_minus = def.tolerance_plus;
        def.tolerance =
            def.tolerance_plus == 0 ? core::DimTolerance::None : core::DimTolerance::Symmetric;
        ctx.record("tolerans", both);
    }
    if (!up.empty() || !down.empty()) {
        if (!up.empty()) def.tolerance_plus = amount(up.as_number());
        if (!down.empty()) def.tolerance_minus = amount(down.as_number());
        def.tolerance = core::DimTolerance::Deviation;
        if (!up.empty()) ctx.record("tolerans_ust", up);
        if (!down.empty()) ctx.record("tolerans_alt", down);
    }
    if (!how.empty()) {
        const std::string& w = how.as_text();
        def.tolerance        = core::DimTolerance::Symmetric;
        if (core::turkish_key_equals(w, "sinir")) def.tolerance = core::DimTolerance::Limits;
        if (core::turkish_key_equals(w, "sapma")) def.tolerance = core::DimTolerance::Deviation;
        if (def.tolerance == core::DimTolerance::Symmetric)
            def.tolerance_minus = def.tolerance_plus;
        ctx.record("tolerans_bicim", how);
    }
    if (def.tolerance_plus == 0 && def.tolerance_minus == 0)
        def.tolerance = core::DimTolerance::None;
    return true;
}

/// The presentation parameters, declared once for both commands that read them.
std::vector<Param> presentation_params()
{
    return {
        Param::text("onek", Arity::optional(), "Değerin önüne yazılan: R, Ø, ≈ …").en("prefix"),
        Param::text("sonek", Arity::optional(), "Değerin ardına yazılan: \" m\", \" (eski)\" …")
            .en("suffix"),
        Param::choice("birim", Arity::optional(),
                      {"cizim", "mm", "cm", "m", "km", "grad", "derece", "radyan"},
                      "Değerin yazıldığı birim. Uzunlukta cizim (çizimin birimi, varsayılan), "
                      "mm, cm, m, km; açıda grad, derece, radyan (varsayılan projenin "
                      "açı_birimi ayarı)")
            .en("unit"),
        Param::integer_range("hassasiyet", Arity::optional(), 0, 8,
                             "Ondalık basamak sayısı; varsayılan stilinki")
            .en("precision"),
        Param::number("tolerans", Arity::optional(),
                      "Simetrik tolerans: ± bu kadar; uzunlukta metre, açıda derece. 0 kaldırır")
            .en("tolerance"),
        Param::number("tolerans_ust", Arity::optional(),
                      "Üst sapma: + bu kadar; uzunlukta metre, açıda derece")
            .en("tolerance_upper"),
        Param::number("tolerans_alt", Arity::optional(),
                      "Alt sapma: − bu kadar, pozitif yazılır; uzunlukta metre, açıda derece")
            .en("tolerance_lower"),
        Param::choice("tolerans_bicim", Arity::optional(), {"simetrik", "sapma", "sinir"},
                      "simetrik: ±; sapma: +üst/−alt; sinir: iki sınır değer, ölçünün yerine")
            .en("tolerance_style"),
    };
}

/// `base` with the presentation parameters after it.
std::vector<Param> with_presentation(std::vector<Param> base)
{
    for (Param& p : presentation_params())
        base.push_back(std::move(p));
    return base;
}

/// LINKED TO WHAT IT MEASURES (TODOS C-10): every definition point of `created`
/// that sits exactly on a vertex, a centre or an arc's end is tied to it, so
/// the dimension follows when that geometry moves. Found from the drawing, not
/// from the hand — a click that snapped, a typed corner and a script's point
/// link alike (Article 1.2). How many were tied, or nothing after refusing.
std::optional<std::size_t> link_points(Context& ctx, core::EntityId created,
                                       core::DimensionType type, std::span<const core::Point2> defs)
{
    std::vector<core::DimLink> links;
    const std::vector<std::optional<core::DimRole>> roles = core::dim_roles(type);
    for (std::size_t i = 0; i < defs.size() && i < roles.size(); ++i) {
        const std::optional<core::DimRole>& role = roles[i];
        if (!role.has_value()) continue;
        auto found = core::dim_anchor_at(ctx.document(), defs[i], role.value(), created);
        if (!found) continue;
        found->point = static_cast<std::uint8_t>(i);
        links.push_back(*found);
    }
    if (links.empty()) return std::size_t{0};
    if (auto st = ctx.transaction().set_dimension_links(created, links); !st) {
        ctx.refuse(st.error());
        return std::nullopt;
    }
    return links.size();
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
        else if (core::turkish_key_equals(w, "acisal") || core::turkish_key_equals(w, "angular") ||
                 // AND THE MODEL'S OWN STABLE NAME FOR IT, which is what this
                 // command RECORDS (`core::dimension_type_name`) and therefore
                 // what a journal replay hands back. It did not accept it: the
                 // line this command wrote was a line it refused to read, so an
                 // angular dimension replayed as nothing at all and a golden
                 // scenario rebuilt one entity short (Article 1.4).
                 //
                 // Spelled out rather than derived from the name table, because
                 // `acisal` is the stable name of the OTHER angular type — the
                 // five-point `Angular`, which this command has never had a word
                 // for — and deriving the list would make one word mean two
                 // types.
                 core::turkish_key_equals(w, "acisal3") || core::turkish_key_equals(w, "angular3"))
            type = core::DimensionType::Angular3P;
        else if (core::turkish_key_equals(w, "koordinat") ||
                 core::turkish_key_equals(w, "ordinat") || core::turkish_key_equals(w, "ordinate"))
            // THE MODEL HELD THIS ALL ALONG. `DimensionType::Ordinate` was
            // declared, encoded, decoded and drawn, and no word reached it — so
            // an ordinate table, which is how a building's corners are given on
            // a Turkish application sheet, could not be drawn at all. The same
            // shape CLAUDE.md 5.15 forbids: a capability nobody can ask for.
            type = core::DimensionType::Ordinate;
        else if (core::turkish_key_equals(w, "yay") || core::turkish_key_equals(w, "arclength"))
            type = core::DimensionType::ArcLength;
        else {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Tanınmayan ölçü türü: '" + w +
                           "'. Türler: hizali, dogrusal, yaricap, cap, acisal, koordinat, yay.");
            co_return;
        }
    }

    const auto style = style_for(ctx);
    if (!style) co_return;

    core::DimensionDef def = style->figures;
    def.type               = type;
    if (const Value m = ctx.argument("metin"); !m.empty() && m.as_text() != "<>")
        def.override_text = m.as_text();
    if (!read_presentation(ctx, def)) co_return;

    const bool angular   = type == core::DimensionType::Angular3P;
    const bool ordinate  = type == core::DimensionType::Ordinate;
    const bool arclength = type == core::DimensionType::ArcLength;
    const bool radial    = type == core::DimensionType::Radial;
    const bool diametric = type == core::DimensionType::Diametric;
    const bool linear    = type == core::DimensionType::Linear;

    // AN ANGLE IN THE PROJECT'S OWN UNIT (TODOS C-17): grad unless the user
    // said otherwise, the unit ÖLÇ, AÇIÖLÇ and every typed `@d<a` already use —
    // an angle ölçü written in degrees beside a traverse measured in grad is two
    // conventions on one sheet. Recorded, so the figure is the same wherever the
    // drawing is opened.
    if (angular && !ctx.has_argument("birim")) {
        const core::AngleUnit unit = ctx.session().bus().angle_convention().unit;
        def.unit                   = angle_code(unit);
        ctx.record("birim", Value::text(core::dimension_unit_word(def)));
    }

    // R AND Ø ARE PART OF THE FIGURE (ISO 129-1, TODOS C-17): a radius is
    // written R7,50 and a diameter Ø15,00, the way a sheet is read — a bare
    // 7,50 beside a circle does not say which of the two it is. `onek=` still
    // decides, and `onek=""` writes none.
    if ((radial || diametric) && !ctx.has_argument("onek")) {
        def.prefix = radial ? "R" : "Ø";
        ctx.record("onek", Value::text(def.prefix));
    }

    // What was picked, in the order `core::dimension_layout` reads it.
    std::vector<core::Point2> picks;

    if ((radial || diametric || arclength) && !ctx.has_argument("birinci")) {
        // A CIRCLE OR AN ARC IS TAKEN WITH ONE CLICK (TODOS C-17). Asking for its
        // centre and then for a point on it made the user find by eye the one
        // point a radius dimension exists to report, and a centre picked a hair
        // off gave a radius a hair off — on a sheet somebody signs. The click
        // names the curve; the centre, the radius and an arc's ends are its
        // own, exact as stored. Recorded as the points they gave, so a replay
        // needs neither the click nor the curve to still be there.
        auto on = co_await ctx.point("nokta", arclength ? "Ölçülecek yaya tıklayın"
                                                        : "Ölçülecek daireye ya da yaya tıklayın");
        if (!on) co_return;
        // The click's own pick box — the one SEÇ uses — and a millimetre for a
        // client with no screen, which names a point ON the curve.
        const core::Mm reach =
            std::max<core::Mm>(ctx.session().bus().aid_settings().pick_radius, 1);
        const auto curve = core::dim_curve_at(ctx.document(), *on, reach, arclength);
        if (!curve) {
            ctx.refuse(core::ErrorCode::NotFound,
                       arclength ? "Orada bir yay yok. Yay uzunluğu için yayın kendisine tıklayın; "
                                   "yayı noktalarıyla vermek için: ÖLÇÜ tur=yay birinci=<merkez> "
                                   "ikinci=<başlangıç> bitis=<bitiş> konum=<yazı>."
                                 : "Orada bir daire ya da yay yok. Çemberin kendisine tıklayın; "
                                   "merkezi ve çemberden bir noktayı kendiniz vermek için: ÖLÇÜ "
                                   "tur=yaricap birinci=<merkez> ikinci=<çemberde> konum=<yazı>.");
            co_return;
        }
        ctx.record("nokta", Value{});
        if (arclength) {
            picks = {curve->centre, curve->start, curve->end};
        } else {
            if (radial) {
                picks = {curve->centre,
                         core::dimension_rim_point(curve->centre, curve->radius, *on)};
            } else {
                const auto ends =
                    core::dimension_diameter_ends(curve->centre, 2 * curve->radius, *on);
                picks = {ends[0], ends[1]};
            }
        }
    } else if (angular) {
        // THE VERTEX FIRST, then the arms from it (TODOS C-17): the order a hand
        // measures an angle in, and the one AÇIÖLÇ asks — so each arm is drawn
        // from the vertex while it is aimed, and the second one shows the sweep
        // between them and its reading.
        auto apex = co_await ctx.point("tepe", "Açının tepe noktası");
        if (!apex) co_return;
        auto arm_a = co_await ctx.point("birinci", "Birinci kolun ucu",
                                        PointOptions{.rubber_band   = true,
                                                     .rubber_origin = *apex,
                                                     .rubber_shape  = RubberShape::Line});
        if (!arm_a) co_return;
        auto arm_b = co_await ctx.point("ikinci", "İkinci kolun ucu",
                                        PointOptions{.rubber_band   = true,
                                                     .rubber_origin = *apex,
                                                     .rubber_shape  = RubberShape::Angle,
                                                     .rubber_chain  = {*arm_a}});
        if (!arm_b) co_return;
        picks = {*arm_a, *arm_b, *apex};
    } else {
        // WHAT EACH TYPE ASKS FOR, in its own words. A prompt that said "birinci
        // nokta" for an ordinate would be asking for the origin without saying
        // so, and an origin picked by mistake moves every figure on the sheet.
        auto p1 = co_await ctx.point("birinci", ordinate ? "Ordinatların okunduğu başlangıç noktası"
                                                : arclength ? "Yayın merkezi"
                                                : radial    ? "Dairenin merkezi"
                                                : diametric ? "Çapın bir ucu"
                                                            : "Ölçülecek ilk nokta");
        if (!p1) co_return;
        auto p2 = co_await ctx.point("ikinci",
                                     ordinate    ? "Ölçülecek nokta"
                                     : arclength ? "Yayın başlangıç noktası"
                                     : radial    ? "Çember üzerinde bir nokta"
                                     : diametric ? "Çapın öbür ucu"
                                                 : "Ölçülecek ikinci nokta",
                                     PointOptions{.rubber_band   = true,
                                                  .rubber_origin = *p1,
                                                  .rubber_shape  = RubberShape::Line});
        if (!p2) co_return;
        picks = {*p1, *p2};
        if (arclength) {
            auto e = co_await ctx.point("bitis", "Yayın bitiş noktası (saat yönünün tersine)",
                                        PointOptions{.rubber_band   = true,
                                                     .rubber_origin = *p1,
                                                     .rubber_shape  = RubberShape::Arc,
                                                     .rubber_chain  = {*p2}});
            if (!e) co_return;
            picks.push_back(*e);
        }
    }

    // The whole dimension under the cursor — line, extension lines, arrows and
    // the figure it will write — laid out by the same function that will lay it
    // out on the click.
    auto where = co_await ctx.point(
        "konum",
        angular ? "Ölçü yayının geçeceği nokta; yay hangi açının içindeyse o ölçülür"
        : ordinate ? "Yazının geleceği yer; yana çekmek sağa, yukarı çekmek yukarı değerini okur"
        : radial    ? "Yazının yeri; yarıçap çizgisi ona doğru uzanır"
        : diametric ? "Yazının yeri; çap çizgisi ona doğru uzanır"
        : arclength ? "Yazının geleceği yer"
        : linear ? "Ölçü çizgisinin yeri; üste ya da alta çekmek yatay, yana çekmek düşey ölçer"
                 : "Ölçü çizgisinin yeri",
        PointOptions{.rubber_band    = true,
                     .rubber_origin  = picks.front(),
                     .rubber_shape   = RubberShape::Dimension,
                     .rubber_chain   = picks,
                     .rubber_payload = core::encode_dimension_guide({def, style->text_height})});
    if (!where) co_return;

    if (picks[0] == picks[1]) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "İki nokta aynı; ölçülecek bir uzunluk yok.");
        co_return;
    }

    // The definition points, the caption's place and the measurement, from the
    // one layout every client of a dimension shares (core/dimension.hpp).
    core::DimensionLayout layout;
    if (!core::dimension_layout(def, picks, *where, style->text_height, layout)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   angular ? "Açının tepe noktası kolların ucuyla aynı olamaz."
                           : "Ölçü bu noktalarla kurulamıyor.");
        co_return;
    }
    if (def.measurement == 0 && def.override_text.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Ölçü sıfır çıktı; noktalar ölçülecek bir uzunluk ya da açı vermiyor.");
        co_return;
    }
    const std::string text = core::dimension_text(def, drawing_unit(ctx));
    core::dimension_fit(def, layout, text, style->text_height);

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
        ctx.refuse(created.error());
        co_return;
    }
    if (auto st = ctx.transaction().set_text(created.value(), text, style->text_height,
                                             core::TextAnchor::MiddleCentre);
        !st) {
        ctx.refuse(st.error());
        co_return;
    }

    const bool link   = !ctx.has_argument("bagla") || ctx.argument("bagla").as_bool(true);
    const auto linked = link ? link_points(ctx, created.value(), type, defs) : std::size_t{0};
    if (!linked) co_return;

    // THE POINTS THE DIMENSION WAS BUILT ON — for a radius and a diameter, the
    // ends the line was turned to (`core::dimension_layout`), so the journal says
    // where the dimension is rather than where the hand first was.
    const bool turned = radial || diametric;
    ctx.record("birinci", Value::point(turned ? defs[0] : picks[0]));
    ctx.record("ikinci", Value::point(turned ? defs[1] : picks[1]));
    if (angular) ctx.record("tepe", Value::point(picks[2]));
    if (arclength) ctx.record("bitis", Value::point(picks[2]));
    ctx.record("konum", Value::point(*where));
    ctx.record("tur", Value::text(core::dimension_type_name(type)));
    ctx.record("stil", Value::text(def.style));
    if (!def.override_text.empty()) ctx.record("metin", Value::text(def.override_text));
    if (!link) ctx.record("bagla", Value::boolean(false));
    std::string said = "Ölçü çizildi: " + text + " (" + def.style + ")";
    if (*linked != 0)
        said += "; " + std::to_string(*linked) + " noktası ölçtüğü nesneye bağlı, o değişince " +
                "ölçü de güncellenir";
    ctx.echo(said + ".");
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
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bir kılavuz çizgi en az iki nokta ister: okun ucu ve yazının yanı.");
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
        ctx.refuse(created.error());
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
            ctx.refuse(caption.error());
            co_return;
        }
        if (auto st = ctx.transaction().set_text(caption.value(), words, style->text_height,
                                                 core::TextAnchor::BaselineLeft);
            !st) {
            ctx.refuse(st.error());
            co_return;
        }
    }

    ctx.record("noktalar", Value::points(points));
    ctx.record("stil", Value::text(style->figures.style));
    if (!words.empty()) ctx.record("metin", Value::text(words));
    ctx.echo(std::to_string(points.size()) + " noktalı kılavuz çizgi çizildi" +
             (words.empty() ? "." : ": " + words));
}

// ------------------------------------------------------ shared by the two ----

/// Writes a rebuilt dimension back: its rings, its payload and its caption.
core::Status write_rebuild(Context& ctx, core::EntityId dim, const core::DimensionRebuild& r)
{
    const std::array<core::RingGeometry::RingInput, 2> rings{
        core::RingGeometry::RingInput{r.baseline, core::RingRole::Open, 0},
        core::RingGeometry::RingInput{r.defs, core::RingRole::Open, 0}};
    if (auto st = ctx.transaction().set_kind_geometry(dim, rings, r.payload); !st) return st;
    return ctx.transaction().set_text(dim, r.text, r.text_height, core::TextAnchor::MiddleCentre);
}

/// The live dimensions `picked` names, in order; every other object is counted
/// in `other`. False after refusing a key that names nothing.
bool dimensions_of(const Context& ctx, const std::vector<std::int64_t>& picked,
                   std::vector<core::EntityId>& out, std::size_t& other)
{
    const core::Document& doc = ctx.document();
    for (const std::int64_t raw : picked) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            return false;
        }
        if (doc.entities().kind[e] == core::kDimensionKind)
            out.push_back(e);
        else
            ++other;
    }
    return true;
}

/// The words of a parameter that takes several, one or many.
std::vector<std::string> words_of(const Value& v)
{
    if (v.empty()) return {};
    if (!v.as_texts().empty()) return v.as_texts();
    return {v.as_text()};
}

/// The caption and, when it is typed by hand, the measured figure beside it —
/// the two a user has to see side by side to know which one the sheet shows.
std::string told(const core::DimensionDef& def, const std::string& text, core::DrawingUnit unit)
{
    std::string out = "\"" + text + "\"";
    if (core::dimension_text_is_manual(def))
        out += " (elle yazılmış; ölçülen " + core::dimension_value_text(def, unit) + ")";
    return out;
}

// ------------------------------------------------------------ ÖLÇÜDÜZENLE ----

Task<void> run_dimension_edit(Context& ctx)
{
    std::vector<std::int64_t> picked;
    if (!co_await want_objects(ctx, "nesneler", "Düzenlenecek ölçüleri seçin, sonra Enter", picked,
                               0, "ÖLÇÜDÜZENLE nesneler=1 onek=R", core::kDimensionKind))
        co_return;
    std::vector<core::EntityId> dims;
    std::size_t other = 0;
    if (!dimensions_of(ctx, picked, dims, other)) co_return;
    if (dims.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Seçimde ölçü yok; ÖLÇÜDÜZENLE yalnız ölçüleri düzenler.");
        co_return;
    }
    const core::Document& doc     = ctx.document();
    const core::DrawingUnit unit  = drawing_unit(ctx);
    const std::uint32_t first_row = doc.entities().slot[dims.front()];

    // NOTHING NAMED IS A QUESTION: the caption. It is what a hand reaching for
    // the tool means, and the first dimension's own text is offered so fixing
    // one letter is not retyping the figure — as `<>` when it is the measured
    // one, which is what keeps it measured.
    bool named = false;
    for (const char* p :
         {"metin", "onek", "sonek", "birim", "hassasiyet", "tolerans", "tolerans_ust",
          "tolerans_alt", "tolerans_bicim", "stil", "yazi_yeri", "sifirla"})
        named = named || ctx.has_argument(p);
    Value text = ctx.argument("metin");
    if (!named) {
        auto now = core::dimension_of(doc.geometry(), first_row);
        std::vector<std::string> offer;
        if (now)
            offer.push_back(now.value().override_text.empty() ? "<>" : now.value().override_text);
        auto typed = co_await ctx.text("metin", "Ölçünün yazısı; <> ölçülen değerdir", offer);
        if (!typed) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Değiştirilecek bir şey verilmedi: metin=, onek=, sonek=, tolerans=, "
                       "birim=, hassasiyet=, stil=, yazi_yeri= ya da sifirla=.");
            co_return;
        }
        text = Value::text(*typed);
    }

    const std::vector<std::string> resets = words_of(ctx.argument("sifirla"));
    const auto reset                      = [&resets](std::string_view what) {
        return std::ranges::any_of(resets, [what](const std::string& w) {
            return core::turkish_key_equals(w, what) || core::turkish_key_equals(w, "hepsi");
        });
    };

    std::optional<core::Point2> caption;
    if (const Value at = ctx.argument("yazi_yeri"); !at.empty()) caption = at.as_point();
    std::optional<DimensionStyleCatalog> catalog;

    std::string said;
    std::size_t done = 0;
    for (const core::EntityId e : dims) {
        if (!doc.editable(e)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Ölçü " + std::to_string(core::raw(doc.key_of(e))) +
                           " kilitli katmanda; düzenlenemez.");
            co_return;
        }
        const std::uint32_t row = doc.entities().slot[e];
        auto stored             = core::dimension_of(doc.geometry(), row);
        if (!stored) {
            ctx.refuse(stored.error());
            co_return;
        }
        core::DimensionDef def = stored.value();
        core::Mm height        = 0;

        // THE STYLE FIRST, so what is named after it overrides it: `stil=MIMARI
        // hassasiyet=3` is the architectural style with three decimals.
        if (const Value st = ctx.argument("stil"); !st.empty()) {
            const std::int64_t basis = def.scale_basis > 0 ? def.scale_basis : plan_scale(ctx);
            const auto style         = style_for(ctx, st.as_text(), basis);
            if (!style) co_return;
            def.arrow             = style->figures.arrow;
            def.arrow_size        = style->figures.arrow_size;
            def.extension_beyond  = style->figures.extension_beyond;
            def.extension_offset  = style->figures.extension_offset;
            def.text_gap          = style->figures.text_gap;
            def.precision         = style->figures.precision;
            def.decimal_separator = style->figures.decimal_separator;
            def.style             = style->figures.style;
            def.scale_basis       = basis;
            height                = style->text_height;
        }
        if (reset("metin")) def.override_text.clear();
        if (reset("onek")) def.prefix.clear();
        if (reset("sonek")) def.suffix.clear();
        // BACK TO WHAT A NEW ONE WOULD WRITE: the drawing's unit for a length,
        // the project's angle unit for an angle. Code 0 of an angle is degrees
        // (what every angle was written in before C-17), so resetting to it
        // would leave a grad sheet with one degree figure on it.
        if (reset("birim"))
            def.unit = core::dimension_is_angle(def)
                           ? angle_code(ctx.session().bus().angle_convention().unit)
                           : std::uint8_t{0};
        if (reset("tolerans")) {
            def.tolerance       = core::DimTolerance::None;
            def.tolerance_plus  = 0;
            def.tolerance_minus = 0;
        }
        if (reset("yazi_yeri")) def.user_text_position = false;
        if (reset("hassasiyet")) {
            // Back to the style's own decimals, when the catalogue still has it.
            if (!catalog) catalog = catalog_for(ctx);
            if (!catalog) co_return;
            if (const DimensionStyle* style = catalog->find(def.style); style != nullptr) {
                def.precision         = style->precision;
                def.decimal_separator = style->decimal_separator;
            }
        }
        if (!text.empty()) def.override_text = text.as_text() == "<>" ? "" : text.as_text();
        if (!read_presentation(ctx, def)) co_return;

        const core::Point2* at = caption ? &*caption : nullptr;
        auto rebuilt           = core::dimension_rebuild(
            doc, e, {.def = &def, .text_height = height, .caption = at}, unit);
        if (!rebuilt) {
            ctx.refuse(rebuilt.error());
            co_return;
        }
        if (const auto st = write_rebuild(ctx, e, rebuilt.value()); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        if (done < 3)
            said +=
                (said.empty() ? "" : ", ") + told(rebuilt.value().def, rebuilt.value().text, unit);
        ++done;
    }

    ctx.record("nesneler", Value::ids(picked));
    if (!text.empty()) ctx.record("metin", text);
    if (!resets.empty()) ctx.record("sifirla", Value::texts(resets));
    if (caption) ctx.record("yazi_yeri", Value::point(*caption));
    if (const Value st = ctx.argument("stil"); !st.empty()) ctx.record("stil", st);
    ctx.echo("Ölçü düzenlendi: " + std::to_string(done) + " ölçü; yazısı " + said +
             (done > 3 ? " …" : "") +
             (other > 0 ? "; ölçü olmayan " + std::to_string(other) + " nesne atlandı" : "") + ".");
}

// ------------------------------------------------------------ ÖLÇÜYENİLE ----

/// Millimetres on the ground as millimetres on a 1/`scale` sheet, one decimal.
std::string paper_mm(core::Mm ground, std::int64_t scale)
{
    const std::int64_t tenths = core::mul_div_round(ground, 10, scale); // 0,1 mm steps
    return std::to_string(tenths / 10) + "," + std::to_string(tenths % 10) + " mm";
}

Task<void> run_dimension_refresh(Context& ctx)
{
    const core::Document& doc = ctx.document();
    std::vector<core::EntityId> dims;
    std::size_t other = 0;
    const Value named = ctx.argument("nesneler");
    if (!named.empty()) {
        std::vector<std::int64_t> picked;
        if (named.kind() == Value::Kind::IdList) picked = named.as_ids();
        if (named.kind() == Value::Kind::Int) picked.push_back(named.as_int());
        if (!dimensions_of(ctx, picked, dims, other)) co_return;
        ctx.record("nesneler", named);
    } else {
        // EVERY DIMENSION OF THE DRAWING, which is what a sheet scale is about.
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kDimensionKind &&
                (doc.entities().flags[e] & core::FlagInBlock) == 0)
                dims.push_back(e);
    }
    if (dims.empty()) {
        ctx.refuse(core::ErrorCode::NotFound, "Yenilenecek ölçü yok.");
        co_return;
    }

    std::int64_t to = ctx.argument("olcek").as_int();
    if (to <= 0) to = plan_scale(ctx);
    if (to <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Pafta ölçeği bilinmiyor: olcek=<N> verin (1/N) ya da AYAR plan_ölçeği ile "
                   "kurun.");
        co_return;
    }
    ctx.record("olcek", Value::integer(to));
    const std::int64_t assumed = ctx.argument("eski_olcek").as_int();
    if (assumed > 0) ctx.record("eski_olcek", Value::integer(assumed));

    // A DIMENSION FROM BEFORE THE SCALE WAS RECORDED — a file, a DXF — still
    // says which style it came from, and the style's paper arrow against the
    // ground one gives the scale it was drawn for.
    const auto catalog  = catalog_for(ctx);
    const auto basis_of = [&](const core::DimensionDef& def) -> std::int64_t {
        if (def.scale_basis > 0) return def.scale_basis;
        if (assumed > 0) return assumed;
        if (!catalog) return 0;
        const DimensionStyle* style = catalog->find(def.style);
        if (style == nullptr || style->arrow_um <= 0 || def.arrow_size <= 0) return 0;
        return core::mul_div_round(def.arrow_size, 1000, style->arrow_um);
    };

    const core::DrawingUnit unit = drawing_unit(ctx);
    std::size_t done             = 0;
    std::size_t current          = 0;
    std::size_t unknown          = 0;
    std::size_t locked           = 0;
    std::int64_t from            = 0;
    bool mixed                   = false;
    core::Mm shown               = 0;
    for (const core::EntityId e : dims) {
        const std::uint32_t row = doc.entities().slot[e];
        auto stored             = core::dimension_of(doc.geometry(), row);
        if (!stored) continue;
        core::DimensionDef def   = stored.value();
        const std::int64_t basis = basis_of(def);
        if (basis <= 0) {
            ++unknown;
            continue;
        }
        const core::Mm height_now = doc.texts().height(row);
        if (basis == to && def.scale_basis == to &&
            core::dimension_text(def, unit) == doc.texts().text(row)) {
            ++current;
            continue;
        }
        if (!doc.editable(e)) {
            ++locked;
            continue;
        }
        const auto scaled = [basis, to](core::Mm v) { return core::mul_div_round(v, to, basis); };
        def.arrow_size    = scaled(def.arrow_size);
        def.extension_beyond  = scaled(def.extension_beyond);
        def.extension_offset  = scaled(def.extension_offset);
        def.text_gap          = scaled(def.text_gap);
        def.scale_basis       = to;
        const core::Mm height = std::max<core::Mm>(1, scaled(height_now));
        auto rebuilt = core::dimension_rebuild(doc, e, {.def = &def, .text_height = height}, unit);
        if (!rebuilt) {
            ++unknown;
            continue;
        }
        if (const auto st = write_rebuild(ctx, e, rebuilt.value()); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        mixed = mixed || (from != 0 && from != basis);
        from  = basis;
        shown = height;
        ++done;
    }

    std::vector<std::string> parts;
    if (done > 0)
        parts.push_back(
            std::to_string(done) + " ölçü " +
            (mixed ? std::string("farklı ölçeklerden") : "1/" + std::to_string(from) + "'den") +
            " 1/" + std::to_string(to) + " paftasına uyarlandı: oklar, uzatma çizgileri ve " +
            "yazılar kâğıtta aynı boyda kalır (yazı " + paper_mm(shown, to) + ").");
    if (current > 0)
        parts.push_back(std::to_string(current) + " ölçü zaten 1/" + std::to_string(to) + " için.");
    if (locked > 0) parts.push_back(std::to_string(locked) + " ölçü kilitli katmanda, atlandı.");
    if (unknown > 0)
        parts.push_back(std::to_string(unknown) +
                        " ölçünün hangi pafta ölçeği için çizildiği bilinmiyor; eski_olcek=<N> "
                        "ile söyleyin.");
    if (other > 0) parts.push_back("Ölçü olmayan " + std::to_string(other) + " nesne atlandı.");
    std::string said = parts.empty() ? std::string("Uyarlanacak ölçü yok.") : parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i)
        said += " " + parts[i];
    ctx.echo(said);
}

// ----------------------------------------------------- ZİNCİRÖLÇÜ, BAZÖLÇÜ ----
//
// A ROW OF FIGURES ON ONE LINE (TODOS C-10). A façade's openings, the lots
// along a road, the offsets of a building from its corner: a surveyor states
// them as a CHAIN (each from the last point) or from one BASE (each from the
// first point, the lines stacked one spacing apart). Each figure is its own
// dimension — linked, editable, followed — laid on the direction of the one the
// run starts from, so the figures line up and read as one row.

/// Whether `e` is a dimension a run can start from: linear or aligned.
bool starts_run(const core::Document& doc, core::EntityId e)
{
    if (e >= doc.entities().size() || !doc.alive(e) ||
        doc.entities().kind[e] != core::kDimensionKind ||
        (doc.entities().flags[e] & core::FlagInBlock) != 0)
        return false;
    auto def = core::dimension_of(doc.geometry(), doc.entities().slot[e]);
    return def && (def.value().type == core::DimensionType::Linear ||
                   def.value().type == core::DimensionType::Aligned);
}

/// The dimension a run starts from: `temel=`, else the newest linear or aligned
/// dimension of the drawing. Nothing after refusing.
std::optional<core::EntityId> run_base(const Context& ctx)
{
    const core::Document& doc = ctx.document();
    if (const Value b = ctx.argument("temel"); !b.empty()) {
        const std::int64_t raw = b.kind() == Value::Kind::IdList && !b.as_ids().empty()
                                     ? b.as_ids().front()
                                     : b.as_int();
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw)));
        if (!starts_run(doc, e)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Nesne " + std::to_string(raw) +
                           " doğrusal ya da hizalı bir ölçü değil; zincir ve baz ölçü onlardan "
                           "kurulur.");
            return std::nullopt;
        }
        return e;
    }
    for (auto e = static_cast<core::EntityId>(doc.entities().size()); e-- > 0;)
        if (starts_run(doc, e)) return e;
    ctx.refuse(core::ErrorCode::NotFound,
               "Başlanacak doğrusal ya da hizalı bir ölçü yok: önce ÖLÇÜ çizin ya da temel= "
               "verin.");
    return std::nullopt;
}

Task<void> run_dimension_run(Context& ctx, bool baseline)
{
    const auto base = run_base(ctx);
    if (!base) co_return;
    const core::Document& doc = ctx.document();
    const std::uint32_t row   = doc.entities().slot[*base];
    auto stored               = core::dimension_of(doc.geometry(), row);
    if (!stored) {
        ctx.refuse(stored.error());
        co_return;
    }
    const core::RingSpan span = doc.geometry().rings_of(row);
    const core::Point2 b1     = doc.geometry().vertex(span.first + 1, 0);
    const core::Point2 b2     = doc.geometry().vertex(span.first + 1, 1);
    const core::Point2 line   = doc.geometry().vertex(span.first + 1, 2);
    const core::Mm height     = doc.texts().height(row);

    // THE ROW'S DIRECTION AND FIGURES: the base's own. An aligned base gives
    // its line's direction to linear figures, which is what keeps every figure
    // of the row on the one line.
    core::DimensionDef proto = stored.value();
    proto.type               = core::DimensionType::Linear;
    proto.override_text.clear();
    proto.user_text_position = false;
    if (stored.value().type == core::DimensionType::Aligned) {
        std::int64_t a = core::atan2_udeg(b2.y - b1.y, b2.x - b1.x) % core::kUDegFullCircle;
        if (a < 0) a += core::kUDegFullCircle;
        proto.rotation_udeg = a;
    }
    const core::SinCos t = core::sin_cos_udeg(proto.rotation_udeg);
    const double nx      = -t.sin;
    const double ny      = t.cos;
    const double off =
        static_cast<double>(line.x - b1.x) * nx + static_cast<double>(line.y - b1.y) * ny;
    const double side = off < 0.0 ? -1.0 : 1.0;

    // THE SPACING BETWEEN STACKED LINES, on paper in the style, on the ground
    // by the sheet scale the base was drawn for.
    core::Mm spacing = height * 3 / 2;
    if (baseline) {
        if (auto catalog = catalog_for(ctx))
            if (const DimensionStyle* style = catalog->find(proto.style); style != nullptr) {
                const std::int64_t scale =
                    proto.scale_basis > 0 ? proto.scale_basis : plan_scale(ctx);
                const std::int32_t um = style->baseline_spacing_um > 0
                                            ? style->baseline_spacing_um
                                            : style->text_height_um * 3 / 2;
                spacing               = core::mul_div_round(um, scale, 1000);
            }
    }

    const bool link = !ctx.has_argument("bagla") || ctx.argument("bagla").as_bool(true);
    const core::DrawingUnit unit = drawing_unit(ctx);
    core::Point2 from            = baseline ? b1 : b2;
    std::vector<std::string> figures;
    std::int64_t total  = 0;
    std::size_t skipped = 0;
    std::size_t linked  = 0;
    for (std::size_t k = 1;; ++k) {
        const core::Point2 at =
            baseline
                ? core::Point2{line.x + core::mm_round(side * nx * static_cast<double>(spacing) *
                                                       static_cast<double>(k)),
                               line.y + core::mm_round(side * ny * static_cast<double>(spacing) *
                                                       static_cast<double>(k))}
                : line;
        auto next = co_await ctx.point(
            "noktalar",
            baseline ? "Tabandan ölçülecek sonraki nokta; Enter bitirir"
                     : "Zincirin sonraki noktası; Enter bitirir",
            PointOptions{.rubber_band    = true,
                         .rubber_origin  = from,
                         .rubber_shape   = RubberShape::DimensionNext,
                         .rubber_chain   = {from, at},
                         .rubber_payload = core::encode_dimension_guide({proto, height})});
        if (!next) break;
        core::DimensionDef def = proto;
        const std::array<core::Point2, 2> picks{from, *next};
        core::DimensionLayout layout;
        if (*next == from || !core::dimension_layout(def, picks, at, height, layout, true) ||
            def.measurement == 0) {
            // A point measured where the last one was is no figure; said, not
            // drawn, and the run goes on.
            ++skipped;
            if (baseline) --k;
            continue;
        }
        const std::string text = core::dimension_text(def, unit);
        core::dimension_fit(def, layout, text, height);
        const auto caption = core::dimension_baseline(layout.text_centre, layout.text_dir_x,
                                                      layout.text_dir_y, height, text);
        const std::vector<core::RingGeometry::RingInput> rings{
            core::RingGeometry::RingInput{caption, core::RingRole::Open, 0},
            core::RingGeometry::RingInput{layout.defs, core::RingRole::Open, 0},
        };
        auto created = ctx.transaction().add_kind(ctx.active_layer(), core::kDimensionKind, rings,
                                                  core::encode_dimension(def));
        if (!created) {
            ctx.refuse(created.error());
            co_return;
        }
        if (auto st = ctx.transaction().set_text(created.value(), text, height,
                                                 core::TextAnchor::MiddleCentre);
            !st) {
            ctx.refuse(st.error());
            co_return;
        }
        if (link) {
            const auto tied = link_points(ctx, created.value(), def.type, layout.defs);
            if (!tied) co_return;
            linked += *tied;
        }
        figures.push_back(text);
        total += def.measurement;
        if (!baseline) from = *next;
    }
    if (figures.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   baseline ? "Tabandan ölçülecek nokta verilmedi." : "Zincire nokta verilmedi.");
        co_return;
    }

    ctx.record("temel", Value::ids({static_cast<std::int64_t>(core::raw(doc.key_of(*base)))}));
    if (!link) ctx.record("bagla", Value::boolean(false));
    std::string row_text;
    for (const std::string& f : figures)
        row_text += (row_text.empty() ? "" : " · ") + f;
    std::string said = std::string(baseline ? "Baz ölçü: " : "Zincir ölçü: ") +
                       std::to_string(figures.size()) + " ölçü eklendi (" + row_text + ")";
    if (!baseline && figures.size() > 1) {
        core::DimensionDef sum = proto;
        sum.measurement        = total;
        sum.prefix.clear();
        sum.suffix.clear();
        sum.tolerance = core::DimTolerance::None;
        said += "; toplam " + core::dimension_value_text(sum, unit);
    }
    if (skipped > 0)
        said += "; " + std::to_string(skipped) +
                " nokta bir öncekiyle aynı yerde ölçüldüğü için atlandı";
    if (linked > 0) said += "; " + std::to_string(linked) + " nokta ölçtüğü nesneye bağlı";
    ctx.echo(said + ".");
}

// ------------------------------------------------------------- ÖLÇÜSTİLİ ----

/// Paper micrometres as millimetres, two decimals at most: `2500` is `2,5`.
std::string paper_text(std::int32_t um)
{
    std::string out         = std::to_string(um / 1000);
    const std::int32_t frac = um % 1000;
    if (frac != 0) {
        std::string digits = std::to_string(frac + 1000).substr(1);
        while (!digits.empty() && digits.back() == '0')
            digits.pop_back();
        out += "," + digits;
    }
    return out;
}

const char* arrow_word(core::ArrowStyle a)
{
    switch (a) {
    case core::ArrowStyle::Closed: return "kapalı ok";
    case core::ArrowStyle::Open: return "açık ok";
    case core::ArrowStyle::Tick: return "45° çentik";
    }
    return "ok";
}

/// WHAT A STYLE LOOKS LIKE, on paper and on this drawing's ground (TODOS C-10):
/// a style is chosen by what it prints, and a name says nothing about that.
Task<void> run_dimension_style(Context& ctx)
{
    const auto catalog = catalog_for(ctx);
    if (!catalog) co_return;
    const std::string current = wanted_style(ctx);
    const std::int64_t scale  = plan_scale(ctx);
    const Value named         = ctx.argument("ad");
    if (!named.empty() && catalog->find(named.as_text()) == nullptr) {
        std::string known;
        for (const DimensionStyle& s : catalog->styles)
            known += (known.empty() ? "" : ", ") + s.id;
        ctx.refuse(core::ErrorCode::NotFound, "Tanınmayan ölçü stili: '" + named.as_text() +
                                                  "'. Katalogdaki stiller: " + known + ".");
        co_return;
    }
    if (!named.empty()) ctx.record("ad", named);

    core::Json rows = core::Json::array({});
    ctx.echo("Ölçü stilleri (katalog " + catalog->package_version + "; yeni ölçüler " + current +
             " ile çizilir, AYAR ölçü_stili değiştirir):");
    for (const DimensionStyle& s : catalog->styles) {
        if (!named.empty() && !core::turkish_key_equals(s.id, named.as_text())) continue;
        const std::int32_t spacing =
            s.baseline_spacing_um > 0 ? s.baseline_spacing_um : s.text_height_um * 3 / 2;
        const auto ground = [scale](std::int32_t um) {
            core::DimensionDef d;
            d.measurement = core::mul_div_round(um, scale, 1000);
            d.precision   = 2;
            return core::dimension_value_text(d, core::DrawingUnit::Metre);
        };
        std::string line =
            "  " + s.id + (core::turkish_key_equals(s.id, current) ? " (varsayılan)" : "") +
            " — yazı " + paper_text(s.text_height_um) + " mm, " + arrow_word(s.arrow) + " " +
            paper_text(s.arrow_um) + " mm, " + std::to_string(s.precision) + " ondalık, ayraç '" +
            std::string(1, s.decimal_separator) + "', baz aralığı " + paper_text(spacing) +
            " mm; 1/" + std::to_string(scale) + " paftada yazı zeminde " +
            ground(s.text_height_um) + " m";
        if (!s.description.empty()) line += ". " + s.description;
        ctx.echo(line);

        core::Json row;
        row.set("ad", core::Json::string(s.id));
        row.set("varsayilan", core::Json::boolean(core::turkish_key_equals(s.id, current)));
        row.set("ok", core::Json::string(arrow_word(s.arrow)));
        row.set("ok_boyu_um", core::Json::integer(s.arrow_um));
        row.set("uzatma_fazlasi_um", core::Json::integer(s.extension_beyond_um));
        row.set("uzatma_boslugu_um", core::Json::integer(s.extension_offset_um));
        row.set("yazi_boslugu_um", core::Json::integer(s.text_gap_um));
        row.set("yazi_yuksekligi_um", core::Json::integer(s.text_height_um));
        row.set("baz_araligi_um", core::Json::integer(spacing));
        row.set("ondalik", core::Json::integer(s.precision));
        row.set("ondalik_ayraci", core::Json::string(std::string(1, s.decimal_separator)));
        row.set("aciklama", core::Json::string(s.description));
        rows.push(std::move(row));
    }
    core::Json report;
    report.set("katalog", core::Json::string(catalog->package_version));
    report.set("varsayilan", core::Json::string(current));
    report.set("plan_olcegi", core::Json::integer(scale));
    report.set("stiller", std::move(rows));
    ctx.report(std::move(report));
}

Task<void> run_dimension_continue(Context& ctx)
{
    co_await run_dimension_run(ctx, false);
}

Task<void> run_dimension_baseline(Context& ctx)
{
    co_await run_dimension_run(ctx, true);
}

} // namespace

KENTOS_COMMAND(dimension)
{
    return CommandSpec{
        .id       = "core.dimension",
        .names    = {"ÖLÇÜ", "OLCU", "DIMENSION", "ÖÇ"},
        .title    = "Ölçü",
        .category = Category::Draw,
        .params   = with_presentation({
            // OPTIONAL SINCE A CLICK ON THE CIRCLE GIVES THEM (`nokta`): a radius,
            // a diameter and an arc length are built on the curve's own points.
            // Every other type asks for them, and the body records them for all.
            Param{"birinci", ParamKind::Point, Arity::optional(),
                  "Birinci nokta; yarıçapta ve yayda merkez, çapta bir uç, açısal ölçüde "
                    "birinci kolun ucu, koordinatta başlangıç"}
                .en("first"),
            Param{"ikinci", ParamKind::Point, Arity::optional(),
                  "İkinci nokta; yarıçapta çemberden bir nokta, çapta öbür uç, yayda "
                    "başlangıç, açısal ölçüde ikinci kolun ucu"}
                .en("second"),
            Param::point("konum", "Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta, "
                                      "yarıçap ve çapta yazının yeri")
                .en("position"),
            Param{"nokta", ParamKind::Point, Arity::optional(),
                  "Yarıçap, çap ve yay uzunluğunda ölçülecek dairenin ya da yayın üstünde bir "
                    "nokta: merkez, yarıçap ve yayın uçları ondan alınır; birinci ve ikinci "
                    "verilmediğinde sorulur"}
                .en("point"),
            Param::text("tur", Arity::optional(),
                          "hizali (varsayılan), dogrusal, yaricap, cap, acisal, koordinat, "
                            "yay")
                .en("type"),
            Param::points("tepe", Arity::optional(), "Açısal ölçünün tepe noktası").en("apex"),
            Param::points("bitis", Arity::optional(), "Yay uzunluğu ölçüsünün bitiş noktası")
                .en("end"),
            Param::text("stil", Arity::optional(),
                          "Katalogdaki ölçü stili (ÖLÇÜSTİLİ listeler); verilmezse AYAR ölçü_stili")
                .en("style"),
            Param::text("metin", Arity::optional(),
                          "Ölçülen değer yerine yazılacak metin; içindeki <> ölçülen değerdir, "
                            "<> taşımayan metin elle yazılmış sayılır")
                .en("text"),
            Param::text("katalog", Arity::optional(),
                          "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
                .en("catalog"),
            Param::boolean("bagla", Arity::optional(),
                             "Tam denk geldiği köşe, merkez ya da yay ucuna bağlansın mı; "
                               "bağlı ölçü kaynağı değişince güncellenir. Varsayılan evet")
                .en("associate"),
        }),
          .undo = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.",
        .run = &run_dimension,
    };
}

KENTOS_COMMAND(dimension_edit)
{
    return CommandSpec{
        .id       = "core.dimension_edit",
        .names    = {"ÖLÇÜDÜZENLE", "OLCUDUZENLE", "DIMEDIT", "ÖDZ", "ODZ"},
        .title    = "Ölçü Düzenle",
        .category = Category::Modify,
        .params   = with_presentation({
            Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                  "Düzenlenecek ölçüler; verilmezse seçim, o da boşsa sorulur"}
                .en("objects"),
            Param::text("metin", Arity::optional(),
                          "Yazı: <> ölçülen değerdir; <> taşımayan metin elle yazılmış sayılır ve "
                            "öyle gösterilir. <> ya da boş metin ölçüye döndürür")
                .en("text"),
            Param::text("stil", Arity::optional(),
                          "Katalogdaki ölçü stili; ok, uzatma çizgileri, yazı ve ondalıklar ondan")
                .en("style"),
            Param::points("yazi_yeri", Arity::optional(),
                            "Yazının yeri; elle yerleştirilen yazı ölçüyle birlikte taşınır")
                .en("text_position"),
            Param::choice(
                "sifirla", Arity{0, 8},
                {"metin", "onek", "sonek", "tolerans", "birim", "hassasiyet", "yazi_yeri", "hepsi"},
                "Stile döndürülecekler; anahtar birden çok kez yazılabilir")
                .en("reset"),
            Param::text("katalog", Arity::optional(),
                          "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
                .en("catalog"),
        }),
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizilmiş ölçünün yazısını, önek ve sonekini, toleransını, birimini, "
                   "ondalıklarını, stilini ya da yazı yerini değiştirir.",
        .run = &run_dimension_edit,
    };
}

KENTOS_COMMAND(dimension_refresh)
{
    return CommandSpec{
        .id       = "core.dimension_refresh",
        .python   = "dimension_refresh",
        .names    = {"ÖLÇÜYENİLE", "OLCUYENILE", "DIMREFRESH", "ÖYN", "OYN"},
        .title    = "Ölçüleri Yenile",
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Yenilenecek ölçüler; verilmezse çizimin bütün ölçüleri"}
                    .en("objects"),
                Param::integer_range("olcek", Arity::optional(), 1, 1'000'000,
                                     "Paftanın ölçeği, 1/N'nin N'si; verilmezse AYAR plan_ölçeği")
                    .en("scale"),
                Param::integer_range("eski_olcek", Arity::optional(), 1, 1'000'000,
                                     "Ölçeği bilinmeyen ölçülerin çizildiği ölçek (dosyadan "
                                     "gelenler)")
                    .en("old_scale"),
                Param::text("katalog", Arity::optional(),
                            "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
                    .en("catalog"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Ölçüleri bir pafta ölçeğine uyarlar: oklar, uzatma çizgileri ve yazılar "
                   "kâğıtta aynı boyda kalır; yazılar çizimin birimiyle yeniden yazılır.",
        .run = &run_dimension_refresh,
    };
}

/// The parameters both runs take.
std::vector<Param> run_params(const char* points_help)
{
    return {
        Param{"temel", ParamKind::Selection, Arity::optional(),
              "Başlanacak doğrusal ya da hizalı ölçü; verilmezse çizimin en son ölçüsü"}
            .en("base"),
        Param::points("noktalar", Arity{0, 0xFFFFFFFFu}, points_help).en("points"),
        Param::boolean("bagla", Arity::optional(),
                       "Tam denk geldiği köşeye bağlansın mı; varsayılan evet")
            .en("associate"),
        Param::text("katalog", Arity::optional(),
                    "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
            .en("catalog"),
    };
}

KENTOS_COMMAND(dimension_continue)
{
    return CommandSpec{
        .id       = "core.dimension_continue",
        .names    = {"ZİNCİRÖLÇÜ", "ZINCIROLCU", "DIMCONTINUE", "ZÖ", "ZO"},
        .title    = "Zincir Ölçü",
        .category = Category::Draw,
        .params   = run_params("Zincirin sonraki noktaları, her biri bir öncekinden ölçülür"),
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Son ölçünün ikinci noktasından başlayarak aynı çizgi üzerinde art arda "
                   "ölçüler çizer; toplamı söyler.",
        .run = &run_dimension_continue,
    };
}

KENTOS_COMMAND(dimension_baseline)
{
    return CommandSpec{
        .id       = "core.dimension_baseline",
        .names    = {"BAZÖLÇÜ", "BAZOLCU", "DIMBASELINE", "BÖ", "BO"},
        .title    = "Baz Ölçü",
        .category = Category::Draw,
        .params   = run_params("Tabandan ölçülecek noktalar; her biri ilk noktadan ölçülür"),
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Son ölçünün ilk noktasından ölçülen ölçüleri, stilin aralığıyla üst üste "
                   "dizer.",
        .run = &run_dimension_baseline,
    };
}

KENTOS_COMMAND(dimension_style)
{
    return CommandSpec{
        .id       = "core.dimension_style",
        .python   = "dimension_style",
        .names    = {"ÖLÇÜSTİLİ", "OLCUSTILI", "DIMSTYLE", "ÖST", "OST"},
        .title    = "Ölçü Stilleri",
        .category = Category::Query,
        .params =
            {
                Param::text("ad", Arity::optional(), "Gösterilecek stil; verilmezse hepsi")
                    .en("name"),
                Param::text("katalog", Arity::optional(),
                            "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
                    .en("catalog"),
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Scriptable | Flags::ReadOnly | Flags::NoEffect | Flags::AiAccessible,
        .summary = "Ölçü stillerini kâğıttaki ve bu paftadaki boylarıyla listeler; hangisinin "
                   "varsayılan olduğunu söyler.",
        .run = &run_dimension_style,
    };
}

KENTOS_COMMAND(leader)
{
    return CommandSpec{
        .id       = "core.leader",
        .names    = {"LİDER", "LIDER", "LEADER", "LD"},
        .title    = "Kılavuz Çizgi",
        .category = Category::Draw,
        .params =
            {
                Param::points("noktalar", Arity::at_least(2), "Okun ucundan yazının yanına köşeler")
                    .en("points"),
                Param::text("metin", Arity::optional(), "Son köşenin yanına yazılacak metin")
                    .en("text"),
                Param::text("stil", Arity::optional(),
                            "Ok ve yazı boyunu veren ölçü stili; verilmezse AYAR ölçü_stili")
                    .en("style"),
                Param::text("katalog", Arity::optional(),
                            "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri")
                    .en("catalog"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir noktayı gösteren oklu kılavuz çizgi çizer, yanına yazı koyabilir.",
        .run     = &run_leader,
    };
}

} // namespace kentos::command
