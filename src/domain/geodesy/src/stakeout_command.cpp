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
// and setting-out sheet is written in grad and a full circle is 400 — and
// CLOCKWISE FROM NORTH by default (`core.aci.kural`), which is what `@mesafe<açı`
// reads too: what the engineer types and what this sheet prints are one
// convention (TODOS-CAD P0-4). The direction and the text come from
// core/angle.hpp, shared with ÖLÇ and the canvas readout, so there is one copy.
//
// READ-ONLY. It computes and reports; it changes nothing.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
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

Task<void> run(Context& ctx)
{
    auto station = co_await ctx.point("istasyon", "Aletin durduğu nokta");
    if (!station) co_return; // ESC before anything was computed

    // The unit and the rule the whole sheet is written in, read once. Under the
    // default semt rule every direction is an azimuth and a relative angle is
    // turned clockwise from the backsight; under matematik both are counted
    // counter-clockwise from east, which is what a user who chose that rule
    // expects to compare against.
    const core::AngleConvention convention = ctx.session().bus().angle_convention();

    // The backsight is optional and changes what the angle column MEANS, so the
    // report says which it is rather than leaving the reader to work it out.
    const Value backsight_arg = ctx.argument("baglama");
    bool relative             = false;
    double zero_turns         = 0.0;
    core::Point2 backsight{};

    if (!backsight_arg.empty() && !backsight_arg.as_points().empty()) {
        backsight  = backsight_arg.as_points().front();
        relative   = true;
        zero_turns = core::direction_turns(*station, backsight, convention.rule);
    }

    const core::Document& doc = ctx.document();

    // What to set out: the selection when there is one, otherwise every point in
    // the drawing. A surveyor asking for an aplikasyon of nothing means all of it.
    std::vector<core::EntityId> targets;

    // NAMED, and not for tidiness. `Context::argument` returns a `Value` BY
    // VALUE and `as_ids()` hands back a reference into it, so writing the two
    // together binds the loop to a container inside a temporary that dies at the
    // end of the initialiser — the loop then walked freed memory. C++23 extends a
    // range-for's temporaries to cover exactly this; this project is C++20
    // (Article 2.2), where it is undefined behaviour and GCC says so.
    const Value picked = ctx.argument("nesneler");
    for (std::int64_t raw : picked.as_ids()) {
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

    const core::AttrTable& table = doc.attributes();
    const core::AttrId no        = table.find("nokta_no");

    std::string report =
        "Aplikasyon — istasyon " + metres(station->x) + " / " + metres(station->y) + "\n";
    const bool semt = convention.rule == core::AngleRule::Semt;
    if (relative)
        report += semt ? "  açılar bağlama yönünden saat yönünde (semt açısı)\n"
                       : "  açılar bağlama yönünden saat yönünün tersine (matematik)\n";
    else
        report += std::string("  açılar ") + core::angle_rule_label(convention.rule) +
                  (semt ? " (azimut)\n" : " (matematik)\n");
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
        const double turns =
            core::direction_turns(*station, at, convention.rule) - (relative ? zero_turns : 0.0);

        name.resize(std::max<std::size_t>(name.size(), 10), ' ');
        std::string length = metres(distance);
        length = std::string(16 - std::min<std::size_t>(length.size(), 16), ' ') + length;

        report.append("  ")
            .append(name)
            .append(" ")
            .append(length)
            .append("   ")
            .append(core::angle_text(turns, convention.unit))
            .append("\n");
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
        .title    = "Aplikasyon",
        .category = Category::Query,
        .params =
            {
                Param::point("istasyon", "Aletin durduğu nokta").en("station"),
                Param::points("baglama", Arity::optional(),
                              "Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür")
                    .en("backsight"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::None,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible | Flags::ReadOnly,
        .summary = "İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).",
        .run     = &run,
    };
}

} // namespace kentos::command
