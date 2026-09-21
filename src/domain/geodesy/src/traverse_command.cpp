// SPDX-License-Identifier: GPL-3.0-or-later
// geodesy.traverse — POLİGON. A traverse reduced, closed and distributed.
//
// A traverse is the skeleton every other measurement hangs off. The crew starts
// on a known point with a known backsight, walks the line reading a BREAKING
// ANGLE and a SIDE at each station, and arrives at a second known point with a
// known foresight. The arithmetic is then three separate questions:
//
//   1. THE ANGULAR CLOSURE. The sum of the breaking angles must carry the
//      starting bearing onto the closing bearing. What it misses by is f_β, and
//      it is distributed equally over the stations — an angle read badly is read
//      badly at one station and there is nothing in the numbers that says which,
//      so the honest correction is the same at each.
//   2. THE COORDINATES, computed with the corrected bearings.
//   3. THE LINEAR CLOSURE. The last computed station must land on the known
//      closing point. What it misses by is f_s, distributed either equally or in
//      proportion to the side lengths (`dagitim=`) — the Bowditch rule, which
//      says a long side earns more of the error because a long side carries more
//      of the measurement.
//
// WHETHER A CLOSURE IS ACCEPTABLE IS A REGULATORY QUESTION and this file does
// not answer it. The limits come from
// `/data/catalogs/geodesy/poligon-toleranslari.json` and a run that exceeds one
// is REFUSED with the regulation named (CLAUDE.md 5.13, domain.md R23). That
// package is marked `onay: BEKLİYOR` — its numbers await a harita mühendisi, as
// CLAUDE.md 6.11 requires — and the command says so on every refusal rather than
// letting a number nobody signed look like a rule.
//
// THE REPORT IS STRUCTURED (`Context::report`, command.md R26), because a
// traverse sheet is what gets filed: every station with its corrected bearing,
// its side, its coordinates and its share of both corrections.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/json.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// Metres with three decimals, in integers, the way ÖLÇ prints one.
std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

/// One class's limits, as the catalogue states them.
struct Tolerance
{
    std::string id;                 ///< `ana`, `ara`, `tamamlayici`
    std::string name;               ///< the Turkish label
    std::int64_t angle_cc{0};       ///< c in f_β ≤ c·√n, centigrad
    std::int64_t length_divisor{0}; ///< o in f_s/[S] ≤ 1/o
    bool approved{false};           ///< whether a domain expert signed the package
    std::string source;             ///< the regulation, annex and madde
};

/// The tolerance class `wanted` from the shipped package.
///
/// A MISSING PACKAGE IS A REFUSAL, not a default. A traverse accepted against a
/// tolerance this program made up is worse than one that was not accepted at
/// all: the first goes into a file with a signature on it.
core::Result<Tolerance> tolerance_for(std::string_view wanted)
{
    auto text = read_catalog_text("data/catalogs/geodesy/poligon-toleranslari.json");
    if (!text)
        return core::err(core::ErrorCode::NotFound,
                         "Poligon tolerans kataloğu okunamadı: " + text.error().message +
                             " Kapanma sınırları mevzuat verisidir ve programa gömülmez "
                             "(CLAUDE.md 5.13); /data kurulumunu KENTOS_DATA ile gösterin.");

    auto parsed = core::Json::parse(text.value());
    if (!parsed)
        return core::err(core::ErrorCode::ParseError,
                         "Poligon tolerans kataloğu ayrıştırılamadı: " + parsed.error().message);

    const core::Json& root = parsed.value();
    Tolerance out;
    if (const core::Json* s = root.find("source"); s != nullptr && s->is_string())
        out.source = s->as_string();
    if (const core::Json* k = root.find("kapsam"); k != nullptr)
        if (const core::Json* onay = k->find("onay"); onay != nullptr && onay->is_string())
            out.approved = onay->as_string() == "ONAYLI";

    const core::Json* classes = root.find("siniflar");
    if (classes == nullptr || !classes->is_array())
        return core::err(core::ErrorCode::ParseError,
                         "Poligon tolerans kataloğunda 'siniflar' dizisi yok.");

    std::string known;
    for (const core::Json& one : classes->as_array()) {
        const core::Json* id = one.find("id");
        if (id == nullptr || !id->is_string()) continue;
        if (!known.empty()) known += ", ";
        known += id->as_string();
        if (!core::turkish_key_equals(id->as_string(), wanted)) continue;

        out.id = id->as_string();
        if (const core::Json* v = one.find("ad"); v != nullptr && v->is_string())
            out.name = v->as_string();
        if (const core::Json* v = one.find("aci_katsayisi_cc"); v != nullptr && v->is_number())
            out.angle_cc = v->as_int();
        if (const core::Json* v = one.find("boy_orani_bolen"); v != nullptr && v->is_number())
            out.length_divisor = v->as_int();
        return out;
    }
    return core::err(core::ErrorCode::InvalidArgument,
                     "Poligon sınıfı bulunamadı: '" + std::string(wanted) +
                         "'. Katalogdaki sınıflar: " + known + ".");
}

Task<void> run(Context& ctx)
{
    // ---- what the crew starts and ends on ---------------------------------
    auto start = co_await ctx.point("baslangic", "Başlangıç istasyonu (bilinen)");
    if (!start) co_return;
    auto backsight = co_await ctx.point("baglama", "Başlangıçtaki bağlama noktası (bilinen)");
    if (!backsight) co_return;

    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // THE ANGLES AND THE SIDES ARE RUNS, read from the field book in order. Both
    // are arguments rather than prompts for a reason: a traverse is transcribed
    // from a sheet, not clicked, and the interactive form is the same command
    // with the runs typed at the prompt.
    const Value::Numbers angles = ctx.argument("aci").as_numbers();
    const Value::Numbers sides  = ctx.argument("kenar").as_numbers();
    if (angles.empty() || sides.empty()) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Poligon için kırılma açıları (aci=) ve kenarlar (kenar=) "
                                     "gerekir; ölçü karnesindeki sırayla verilir."));
        co_return;
    }
    if (angles.size() != sides.size()) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Kırılma açısı ve kenar sayısı eşit olmalı: " + std::to_string(angles.size()) +
                " açı, " + std::to_string(sides.size()) +
                " kenar geldi. Her istasyonda bir açı ve ondan sonraki bir kenar okunur."));
        co_return;
    }

    const Value closing_arg   = ctx.argument("bitis");
    const Value foresight_arg = ctx.argument("bitis_baglama");
    const bool closed         = !closing_arg.empty() && !closing_arg.as_points().empty();

    std::string wanted = "ana";
    if (const Value v = ctx.argument("sinif"); !v.empty()) wanted = v.as_text();
    auto limits = tolerance_for(wanted);
    if (!limits) {
        ctx.session().fail(limits.error());
        co_return;
    }

    std::string spread = "esit";
    if (const Value v = ctx.argument("dagitim"); !v.empty()) spread = v.as_text();
    const bool by_length = core::turkish_key_equals(spread, "kenar");

    // ---- 1. the angular closure -------------------------------------------
    //
    // Bearings are carried in TURNS, which add; an angle in a unit does not add
    // until it is a turn (`core::polar_offset_turns`).
    const double start_bearing = core::direction_turns(*start, *backsight, convention.rule);

    std::vector<double> read_turns;
    read_turns.reserve(angles.size());
    for (const double one : angles)
        read_turns.push_back(core::turns_from_udeg(core::udeg_from_angle(one, convention.unit)));

    double angular_misclosure_turns = 0.0;
    const auto station_count        = static_cast<double>(angles.size());

    if (closed && !foresight_arg.empty() && !foresight_arg.as_points().empty()) {
        // The closing bearing the crew must arrive on, and what the read angles
        // actually carry the starting bearing to. A breaking angle turns the
        // line by a half turn less the angle, which is the convention a Turkish
        // field book is written in.
        const double close_bearing = core::direction_turns(
            closing_arg.as_points().front(), foresight_arg.as_points().front(), convention.rule);

        double carried = start_bearing;
        for (const double t : read_turns)
            carried += t - 0.5;

        // Folded into (-0.5, 0.5]: a misclosure is a small angle either way, and
        // reading it as almost a whole turn is how a tolerance check passes what
        // it should refuse.
        double miss = close_bearing - carried;
        miss -= std::floor(miss);
        if (miss > 0.5) miss -= 1.0;
        angular_misclosure_turns = miss;

        // THE REGULATION DECIDES, not this file. f_β ≤ c·√n, both in centigrad.
        const double miss_cc =
            std::abs(angular_misclosure_turns) * 400.0 * 100.0; ///< turns -> grad -> cc
        const double allowed_cc =
            static_cast<double>(limits.value().angle_cc) * std::sqrt(station_count);
        if (miss_cc > allowed_cc) {
            ctx.session().fail(core::err(
                core::ErrorCode::ValidationFailed,
                "Açı kapanma hatası toleransı aşıyor: " + std::to_string(std::llround(miss_cc)) +
                    " cc ölçüldü, en çok " + std::to_string(std::llround(allowed_cc)) +
                    " cc olabilir (" + limits.value().name + ", f_β ≤ " +
                    std::to_string(limits.value().angle_cc) + "·√" + std::to_string(angles.size()) +
                    "). Kaynak: " + limits.value().source +
                    (limits.value().approved
                         ? "."
                         : ". UYARI: bu tolerans paketi harita mühendisi onayı BEKLİYOR "
                           "(CLAUDE.md 6.11); bir üretim işinin kabulü için kullanılmaz.")));
            co_return;
        }
    }

    // ---- 2. the coordinates, on corrected bearings ------------------------
    const double per_station = closed ? angular_misclosure_turns / station_count : 0.0;

    std::vector<core::Point2> computed;
    std::vector<double> bearings;
    computed.reserve(angles.size());
    bearings.reserve(angles.size());

    double bearing  = start_bearing;
    core::Point2 at = *start;
    for (std::size_t i = 0; i < angles.size(); ++i) {
        bearing += read_turns[i] - 0.5 + per_station;
        bearings.push_back(bearing);
        at = at + core::polar_offset_turns(sides[i], bearing, convention.rule);
        computed.push_back(at);
    }

    // ---- 3. the linear closure --------------------------------------------
    core::Point2 linear_miss{};
    double total_length = 0.0;
    for (const double one : sides)
        total_length += one;

    if (closed) {
        const core::Point2 must_be = closing_arg.as_points().front();
        linear_miss                = core::Point2{must_be.x - at.x, must_be.y - at.y};

        const double miss_mm =
            std::sqrt(static_cast<double>(linear_miss.x) * static_cast<double>(linear_miss.x) +
                      static_cast<double>(linear_miss.y) * static_cast<double>(linear_miss.y));
        const double run_mm = total_length * static_cast<double>(core::kMmPerMetre);
        if (run_mm > 0.0) {
            const double allowed_mm = run_mm / static_cast<double>(limits.value().length_divisor);
            if (miss_mm > allowed_mm) {
                ctx.session().fail(core::err(
                    core::ErrorCode::ValidationFailed,
                    "Kenar kapanma hatası toleransı aşıyor: " +
                        metres(static_cast<core::Mm>(std::llround(miss_mm))) +
                        " m ölçüldü, en çok " +
                        metres(static_cast<core::Mm>(std::llround(allowed_mm))) + " m olabilir (" +
                        limits.value().name + ", f_s/[S] ≤ 1/" +
                        std::to_string(limits.value().length_divisor) +
                        "; [S] = " + metres(core::mm_from_metres(total_length)) +
                        " m). Kaynak: " + limits.value().source +
                        (limits.value().approved
                             ? "."
                             : ". UYARI: bu tolerans paketi harita mühendisi onayı BEKLİYOR "
                               "(CLAUDE.md 6.11); bir üretim işinin kabulü için kullanılmaz.")));
                co_return;
            }
        }

        // DISTRIBUTED. Equal shares, or Bowditch: a long side earns more of the
        // error because a long side carries more of the measurement.
        double carried = 0.0;
        for (std::size_t i = 0; i < computed.size(); ++i) {
            carried += by_length ? sides[i] : 1.0;
            const double share =
                by_length ? carried / total_length : carried / static_cast<double>(computed.size());
            computed[i] = core::Point2{
                computed[i].x + core::mm_round(share * static_cast<double>(linear_miss.x)),
                computed[i].y + core::mm_round(share * static_cast<double>(linear_miss.y))};
        }
    }

    // ---- the drawing and the sheet ----------------------------------------
    for (const core::Point2& p : computed) {
        auto created = ctx.transaction().add_point(ctx.active_layer(), p);
        if (!created) {
            ctx.session().fail(created.error());
            co_return;
        }
    }

    bool join = true;
    if (const Value v = ctx.argument("cizgi"); !v.empty()) join = v.as_bool();
    if (join) {
        std::vector<core::Point2> run;
        run.push_back(*start);
        for (const core::Point2& p : computed)
            run.push_back(p);
        auto line = ctx.transaction().add_polyline(ctx.active_layer(), run);
        if (!line) {
            ctx.session().fail(line.error());
            co_return;
        }
    }

    core::Json report;
    report.set("sinif", core::Json::string(limits.value().id));
    report.set("kaynak", core::Json::string(limits.value().source));
    report.set("onay", core::Json::string(limits.value().approved ? "ONAYLI" : "BEKLİYOR"));
    report.set("istasyon_sayisi", core::Json::integer(static_cast<std::int64_t>(angles.size())));
    report.set("toplam_kenar_mm", core::Json::integer(core::mm_from_metres(total_length)));
    report.set("dagitim", core::Json::string(by_length ? "kenar" : "esit"));
    report.set("kapali", core::Json::boolean(closed));
    if (closed) {
        report.set("aci_kapanma_cc",
                   core::Json::integer(std::llround(angular_misclosure_turns * 400.0 * 100.0)));
        core::Json miss = core::Json::array({});
        miss.push(core::Json::integer(linear_miss.x));
        miss.push(core::Json::integer(linear_miss.y));
        report.set("kenar_kapanma_mm", std::move(miss));
    }

    core::Json rows = core::Json::array({});
    for (std::size_t i = 0; i < computed.size(); ++i) {
        core::Json row;
        row.set("no", core::Json::integer(static_cast<std::int64_t>(i + 1)));
        row.set("semt", core::Json::string(core::angle_text(bearings[i], convention.unit)));
        row.set("kenar_mm", core::Json::integer(core::mm_from_metres(sides[i])));
        row.set("saga", core::Json::integer(computed[i].x));
        row.set("yukari", core::Json::integer(computed[i].y));
        rows.push(std::move(row));
    }
    report.set("istasyonlar", std::move(rows));
    ctx.report(std::move(report));

    std::string said = std::to_string(computed.size()) + " poligon noktası hesaplandı (" +
                       limits.value().name +
                       ", [S] = " + metres(core::mm_from_metres(total_length)) + " m";
    if (closed)
        said += ", açı kapanma " +
                std::to_string(std::llround(angular_misclosure_turns * 40000.0)) +
                " cc, kenar kapanma " +
                metres(static_cast<core::Mm>(std::llround(std::sqrt(
                    static_cast<double>(linear_miss.x) * static_cast<double>(linear_miss.x) +
                    static_cast<double>(linear_miss.y) * static_cast<double>(linear_miss.y))))) +
                " m, " + (by_length ? "kenar orantılı" : "eşit") + " dağıtıldı";
    said += ").";
    if (!limits.value().approved)
        said += " UYARI: tolerans paketi harita mühendisi onayı bekliyor.";
    ctx.echo(said);
}

} // namespace

KENTOS_COMMAND(traverse)
{
    return CommandSpec{
        .id       = "geodesy.traverse",
        .names    = {"POLİGON", "POLIGON", "TRAVERSE", "PLG"},
        .title    = "Poligon Hesabı",
        .category = Category::Draw,
        .params =
            {
                Param::point("baslangic", "Başlangıç istasyonu (bilinen)"),
                Param::point("baglama", "Başlangıçtaki bağlama noktası (bilinen)"),
                Param::number("aci", Arity::at_least(0),
                              "Her istasyonda okunan kırılma açısı, ölçü karnesi sırasıyla"),
                Param::number("kenar", Arity::at_least(0), "Her istasyondan sonraki kenar (m)")
                    .measured_in("m"),
                Param::points("bitis", Arity::optional(),
                              "Bitiş istasyonu (bilinen); verilirse kapanma hesaplanır"),
                Param::points("bitis_baglama", Arity::optional(),
                              "Bitişteki bağlama noktası; açı kapanması için gerekir"),
                Param::choice("sinif", Arity::optional(), {"ana", "ara", "tamamlayici"},
                              "Tolerans sınıfı; katalogdan okunur"),
                Param::choice("dagitim", Arity::optional(), {"esit", "kenar"},
                              "Kenar kapanmasının dağıtımı: eşit ya da kenar orantılı"),
                Param::boolean("cizgi", Arity::optional(),
                               "Güzergâhı çizgiyle bağlar; varsayılan evet"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma "
                   "hatalarını dağıtır ve mevzuat toleransına karşı denetler.",
        .run     = &run,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
