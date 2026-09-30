// SPDX-License-Identifier: GPL-3.0-or-later
// core.extent_check — KAPSAMDENETİM: the objects cut off from the drawing's
// majority, found, marked and named — and never moved (netcad_plan.md N-01).
//
// Netcad's Shift+Limit Bul moves them to a HATALI layer by itself (wiki
// 217385147). A cadastral drawing is a legal document, and which object is
// "wrong" is the engineer's call: this answers the question and writes out the
// one line that would act on the answer, for the user to run or not.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/measure_mark.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/detached.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/json.hpp"
#include "piricad/core/units.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace piricad::command {
namespace {

/// How many objects the next-step lines name before they stop; the report
/// names every one.
constexpr std::size_t kNamedAtMost = 20;

/// Where zero is not a place: a drawing whose majority is further than this
/// from 0,0 has no object there on purpose (TUREF has no parcel at 0,0).
constexpr double kFarFromZeroMm = 100'000'000.0; // 100 km

/// How close to 0,0 an object is said to have fallen there.
constexpr core::Mm kNearZeroMm = 1'000'000; // 1 km

/// A distance as the answer reads it: kilometres to one decimal from a
/// kilometre up, metres below.
std::string distance_text(double mm)
{
    const auto whole = static_cast<core::Mm>(std::llround(mm));
    if (whole >= 1'000'000) return core::metres_fixed(whole / 1000, 1, ',') + " km";
    return core::metres_fixed(whole, 1, ',') + " m";
}

core::Json point_json(core::Point2 p)
{
    core::Json out = core::Json::array({});
    out.push(core::Json::integer(p.x));
    out.push(core::Json::integer(p.y));
    return out;
}

Task<void> run_extent_check(Context& ctx)
{
    Bus& bus                  = ctx.session().bus();
    const core::Document& doc = bus.document();
    const int factor =
        static_cast<int>(bus.project_settings().get("core.denetim.kopukluk_carpani").as_int());

    const core::DetachedReport found = core::find_detached(doc, factor);

    core::Json report;
    report.set("denetlenen", core::Json::integer(static_cast<std::int64_t>(found.checked)));
    report.set("carpan", core::Json::integer(factor));
    core::Json rows = core::Json::array({});

    if (found.checked < 3) {
        ctx.echo("KAPSAMDENETİM: kopukluk için en az üç nesne gerekir; çizimde " +
                 std::to_string(found.checked) + " nesne var.");
        report.set("kopuk", std::move(rows));
        ctx.report(std::move(report));
        co_return;
    }
    report.set("merkez", point_json(found.centre));
    report.set("yaricap_mm", core::Json::integer(std::llround(found.radius_mm)));

    if (found.detached.empty()) {
        ctx.echo("KAPSAMDENETİM: " + std::to_string(found.checked) +
                 " nesne denetlendi; hiçbiri çizimin çoğunluğundan kopuk değil.");
        report.set("kopuk", std::move(rows));
        ctx.report(std::move(report));
        co_return;
    }

    ctx.echo("KAPSAMDENETİM: " + std::to_string(found.checked) + " nesne denetlendi; " +
             std::to_string(found.detached.size()) +
             " nesne çizimin çoğunluğundan kopuk (çoğunluğun yarıçapı " +
             distance_text(found.radius_mm) + ", kopukluk çarpanı " + std::to_string(factor) +
             "):");

    const double cx        = static_cast<double>(found.centre.x);
    const double cy        = static_cast<double>(found.centre.y);
    const bool zero_is_far = std::sqrt(cx * cx + cy * cy) > kFarFromZeroMm;
    std::string named;
    std::size_t named_count = 0;
    for (const core::DetachedObject& d : found.detached) {
        const auto key          = static_cast<std::int64_t>(core::raw(doc.key_of(d.entity)));
        const core::Layer* on   = doc.layer(doc.entities().layer[d.entity]);
        const std::string layer = on != nullptr ? on->name : std::string();
        const bool near_zero    = zero_is_far && std::llabs(d.centre.x) < kNearZeroMm &&
                                  std::llabs(d.centre.y) < kNearZeroMm;

        ctx.echo("  nesne " + std::to_string(key) + " · " + layer + " · çoğunluğun merkezinden " +
                 distance_text(d.distance_mm) +
                 (near_zero ? " · sıfıra yakın: koordinatını yitirmiş olabilir" : ""));

        core::Json row;
        row.set("nesne", core::Json::integer(key));
        row.set("katman", core::Json::string(layer));
        row.set("merkez", point_json(d.centre));
        row.set("uzaklik_mm", core::Json::integer(std::llround(d.distance_mm)));
        row.set("sifira_yakin", core::Json::boolean(near_zero));
        rows.push(std::move(row));

        // MARKED WHERE IT IS, as a measurement is (`measure_mark.hpp`): view
        // state, gone when the drawing changes or Esc is pressed.
        ctx.mark(MeasureMark{
            MeasureMark::Shape::Point, {d.centre}, {"kopuk · " + distance_text(d.distance_mm)}});

        if (named_count < kNamedAtMost) {
            named += " nesneler=" + std::to_string(key);
            ++named_count;
        }
    }

    // THE NEXT STEP, NAMED AND NOT TAKEN: moving an object is an edit of a
    // legal document, and the engineer decides which of these is wrong.
    ctx.echo("Seçmek için: SEÇ" + named);
    ctx.offer(Offer{.title = "Kopuk nesne: " + std::to_string(found.detached.size()),
                    .text  = "Çizimin çoğunluğundan ayrı duruyorlar. Seçin, sonra ne "
                             "yapacağınıza karar verin; hiçbiri taşınmadı.",
                    .label = "Seç",
                    .line  = "SEÇ" + named});
    ctx.echo("Ayrı bir katmana almak için: KATMANAT" + named + " katman=HATALI");
    if (found.detached.size() > kNamedAtMost)
        ctx.echo("(İlk " + std::to_string(kNamedAtMost) +
                 " nesne yazıldı; hepsinin kimliği cevaptadır.)");

    report.set("kopuk", std::move(rows));
    ctx.report(std::move(report));
}

} // namespace

PIRICAD_COMMAND(extent_check)
{
    return CommandSpec{
        .id       = "core.extent_check",
        .names    = {"KAPSAMDENETİM", "KAPSAMDENETIM", "EXTENTCHECK", "KPD"},
        .title    = "Kapsam Denetimi",
        .category = Category::Query,
        .params   = {},
        .undo     = UndoPolicy::None,
        .flags    = Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly | Flags::NoEffect,
        .summary  = "Çizimin çoğunluğundan kopuk nesneleri — sıfıra düşmüş, başka bir koordinat "
                    "sisteminde gelmiş — bulur, işaretler ve bildirir; hiçbirini taşımaz.",
        .run      = &run_extent_check,
        .effect   = Effect::Query,
    };
}

} // namespace piricad::command
