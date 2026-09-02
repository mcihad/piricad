// SPDX-License-Identifier: GPL-3.0-or-later
// core.stakeout — APLİKASYON. The list that takes a design back to the field.
//
// A surveyor stands the instrument on a known station, sights a known direction,
// and needs — for every point they are about to set out — a BEARING to turn to
// and a DISTANCE to measure. That list is the whole product of an aplikasyon, and
// producing it by hand from a coordinate table is where mistakes get made.
//
// SEMT AÇISI, NOT AZIMUTH, WHEN A BACKSIGHT IS GIVEN. An instrument is zeroed on
// the backsight, so what the operator turns is the angle FROM that direction, not
// from north. Reporting the azimuth when the instrument reads a relative angle
// would be handing them a number they have to convert in their head, at the
// tripod, in the sun — which is exactly where a mistake becomes a boundary.
//
// GRAD BY DEFAULT (`core.aci.birim`), because a Turkish traverse, triangulation
// and setting-out sheet is written in grad and a full circle is 400.
//
// READ-ONLY. It computes and reports; it changes nothing. `atan2` is allowed here
// for the reason the canvas label uses it — nothing is stored and no golden
// fixture passes through this number (§7.3).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

constexpr double kPi = 3.14159265358979323846;

/// Metres with three decimals, in integers, the way ÖLÇ prints one.
std::string metres(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

/// A turn of a full circle, written in the project's angle unit.
std::string angle_text(double turns, int unit)
{
    turns -= std::floor(turns); // into [0, 1) whatever came in

    const double scaled = unit == 1 ? turns * 360.0 : unit == 2 ? turns * 2.0 * kPi : turns * 400.0;
    const int places    = unit == 2 ? 5 : 4;

    const double rounded = scaled * std::pow(10.0, places);
    const auto whole     = static_cast<std::int64_t>(rounded + 0.5);
    const auto divisor   = static_cast<std::int64_t>(std::pow(10.0, places) + 0.5);

    std::string frac = std::to_string(whole % divisor);
    frac             = std::string(static_cast<std::size_t>(places) - frac.size(), '0') + frac;

    const char* suffix = unit == 1 ? "°" : unit == 2 ? " rad" : " grad";
    return std::to_string(whole / divisor) + "," + frac + suffix;
}

/// Azimuth from `from` to `to`, as a fraction of a full turn, CLOCKWISE FROM
/// NORTH — `atan2(east, north)`, which is what an instrument reads.
double azimuth_turns(core::Point2 from, core::Point2 to)
{
    const auto east  = static_cast<double>(to.x - from.x);
    const auto north = static_cast<double>(to.y - from.y);
    if (east == 0.0 && north == 0.0) return 0.0;

    double turns = std::atan2(east, north) / (2.0 * kPi);
    if (turns < 0.0) turns += 1.0;
    return turns;
}

Task<void> run(Context& ctx)
{
    auto station = co_await ctx.point("istasyon", "Aletin durduğu nokta");
    if (!station) co_return; // ESC before anything was computed

    // The backsight is optional and changes what the angle column MEANS, so the
    // report says which it is rather than leaving the reader to work it out.
    const Value backsight_arg = ctx.argument("baglama");
    bool relative             = false;
    double zero_turns         = 0.0;
    core::Point2 backsight{};

    if (!backsight_arg.empty() && !backsight_arg.as_points().empty()) {
        backsight  = backsight_arg.as_points().front();
        relative   = true;
        zero_turns = azimuth_turns(*station, backsight);
    }

    const core::Document& doc = ctx.document();

    // What to set out: the selection when there is one, otherwise every point in
    // the drawing. A surveyor asking for an aplikasyon of nothing means all of it.
    std::vector<core::EntityId> targets;
    for (std::int64_t raw : ctx.argument("nesneler").as_ids()) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }
        targets.push_back(slot);
    }
    if (targets.empty())
        for (core::EntityKey k : ctx.session().bus().selection().keys()) {
            const core::EntityId slot = doc.slot_of(k);
            if (slot != core::kNoEntity && doc.alive(slot)) targets.push_back(slot);
        }
    if (targets.empty())
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kPointKind) targets.push_back(e);

    if (targets.empty()) {
        ctx.echo("Aplike edilecek nokta yok. NOKTALAR ile bir liste okuyun, NOKTA ile çizin "
                 "ya da nesne seçin.");
        co_return;
    }

    const int unit =
        static_cast<int>(ctx.session().bus().project_settings().get("core.aci.birim").as_enum());

    const core::AttrTable& table = doc.attributes();
    const core::AttrId no        = table.find("nokta_no");

    std::string report = "Aplikasyon — istasyon " + metres(station->x) + " / " +
                         metres(station->y) + "\n";
    report += relative ? "  açılar bağlama yönünden (semt açısı)\n"
                       : "  açılar kuzeyden saat yönünde (azimut)\n";
    report += "  nokta        mesafe (m)        açı\n";

    std::size_t counted = 0;
    for (core::EntityId slot : targets) {
        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        if (span.count == 0) continue;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        if (xs.empty()) continue;

        // The FIRST vertex, which for a point entity is the point itself. An area
        // or a line in the selection contributes its first corner rather than
        // being refused: setting out a parcel means setting out its corners, and
        // a surveyor who selected one meant that.
        const core::Point2 at{xs[0], ys[0]};

        std::string name;
        if (no != core::kNoAttr)
            if (auto had = doc.attribute(no, slot); had && had.value().present)
                name = had.value().text;
        if (name.empty())
            name = std::to_string(static_cast<std::uint64_t>(core::raw(doc.entities().key[slot])));

        const core::Mm distance = core::segment_length(*station, at);
        const double turns      = azimuth_turns(*station, at) - (relative ? zero_turns : 0.0);

        name.resize(std::max<std::size_t>(name.size(), 10), ' ');
        std::string length = metres(distance);
        length             = std::string(16 - std::min<std::size_t>(length.size(), 16), ' ') + length;

        report += "  " + name + " " + length + "   " + angle_text(turns, unit) + "\n";
        ++counted;
    }

    report += "  " + std::to_string(counted) + " nokta.";
    ctx.echo(report);

    ctx.record("istasyon", Value::point(*station));
    if (relative) ctx.record("baglama", Value::points({backsight}));
}

} // namespace

KENTOS_COMMAND(stakeout)
{
    return CommandSpec{
        .id       = "core.stakeout",
        .names    = {"APLİKASYON", "APLIKASYON", "STAKEOUT", "APL"},
        .category = Category::Query,
        .params =
            {
                Param::point("istasyon", "Aletin durduğu nokta"),
                Param::points("baglama", Arity::optional(),
                              "Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar"},
            },
        .undo  = UndoPolicy::None,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).",
        .run     = &run,
    };
}

} // namespace kentos::command
