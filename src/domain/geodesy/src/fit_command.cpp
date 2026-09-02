// SPDX-License-Identifier: GPL-3.0-or-later
// core.fit — OTURT. Puts a locally-measured drawing onto the map.
//
// THE JOB THIS EXISTS FOR. A crew sets a station, calls it 0,0 and surveys for a
// week. Every distance and every angle in the result is right and the whole thing
// is nowhere on the map. Later somebody reads two or three published points —
// nirengi, poligon, a röper with a TUREF coordinate — and the drawing has to move
// onto them WITHOUT deforming: a similarity, never an affine (`helmert.hpp`).
//
// ONE TRANSACTION FOR THE WHOLE DRAWING. Every vertex moves or none does. A
// half-transformed cadastral sheet is the exact failure Article 1.6 names, and it
// is worse here than anywhere else because the halves would both look plausible.
//
// THE CONTROL IS KEPT. How a drawing was put on the map is part of what the
// drawing IS — a reviewer has to be able to see the points, the residuals and the
// scale that was accepted — so the fit is reported in full and the parameters go
// into the journal with the command.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/domain/geodesy/helmert.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

std::string mm_text(core::Mm v)
{
    const bool negative = v < 0;
    const auto abs_mm   = static_cast<std::uint64_t>(negative ? -v : v);
    std::string frac    = std::to_string(abs_mm % 1000);
    frac                = std::string(3 - frac.size(), '0') + frac;
    return (negative ? "-" : "") + std::to_string(abs_mm / 1000) + "," + frac;
}

std::string fixed3(double v)
{
    const auto scaled = static_cast<std::int64_t>(v * 1000.0 + (v < 0 ? -0.5 : 0.5));
    return mm_text(static_cast<core::Mm>(scaled));
}

Task<void> run(Context& ctx)
{
    using domain::geodesy::ControlPoint;

    // The pairs arrive as a flat run of points: local, map, local, map... One
    // parameter rather than two, because two lists could disagree in length and
    // then the command would have to guess which one was short.
    const Value::Points given = ctx.argument("noktalar").as_points();
    if (given.size() < 4 || (given.size() % 2) != 0) {
        ctx.echo("OTURT nokta ÇİFTLERİ ister: yerel, harita, yerel, harita... "
                 "En az iki çift (dört nokta) gerekir. Verilen: " +
                 std::to_string(given.size()) + " nokta.");
        co_return;
    }

    std::vector<ControlPoint> control;
    control.reserve(given.size() / 2);
    for (std::size_t i = 0; i + 1 < given.size(); i += 2)
        control.push_back(ControlPoint{given[i], given[i + 1]});

    const Value lock = ctx.argument("olcek_kilitli");
    const bool locked = !lock.empty() && lock.as_bool();

    auto fitted = domain::geodesy::fit_helmert(control, locked);
    if (!fitted) {
        ctx.echo(fitted.error().message);
        co_return;
    }
    const domain::geodesy::Helmert2D& fit = fitted.value();

    // ---- the report, BEFORE anything moves ----
    //
    // A surveyor accepts or rejects a fit on its residuals, so they are printed
    // whether or not the fit is applied. The command applies it in the same run
    // because a bus command is one transaction; the record is what makes the
    // decision reviewable afterwards.
    std::string report = "Oturtma: " + std::to_string(control.size()) + " kontrol noktası, ölçek " +
                         fixed3(fit.scale) + (locked ? " (kilitli)" : "") + ", dönüklük " +
                         fixed3(fit.rotation_grad) + " grad";
    report += "\n  karesel ortalama artık (RMS): " + mm_text(fit.rms) + " m";
    report += "\n  en büyük artık: " + mm_text(fit.worst) + " m";
    for (std::size_t i = 0; i < fit.residuals.size(); ++i)
        report += "\n  " + std::to_string(i + 1) + ". nokta artığı: " + mm_text(fit.residuals[i]) +
                  " m";
    ctx.echo(report);

    // ---- move every vertex ----
    const core::Document& doc      = ctx.document();
    const core::RingGeometry& geom = doc.geometry();

    std::vector<core::Point2> moved;
    std::vector<core::RingGeometry::RingInput> rings;
    std::size_t touched = 0;

    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;

        const core::RingSpan span = geom.rings_of(doc.entities().slot[e]);
        rings.clear();

        // The vertices are copied out before anything is written, because
        // `set_geometry` replaces the entity's rings and the spans above would
        // then name storage that has moved on.
        std::vector<std::vector<core::Point2>> parts;
        parts.reserve(span.count);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);

            moved.clear();
            moved.reserve(xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v)
                moved.push_back(fit.apply(core::Point2{xs[v], ys[v]}));

            parts.push_back(moved);
            rings.push_back(
                core::RingGeometry::RingInput{parts.back(), geom.ring_role[r], geom.ring_part[r]});
        }

        if (rings.empty()) continue;
        if (auto st = ctx.transaction().set_geometry(e, rings); !st) {
            ctx.echo(st.error().message);
            co_return; // the bus rolls the whole drawing back
        }
        ++touched;
    }

    // ---- and say where it now is ----
    if (const Value target = ctx.argument("sistem"); !target.empty()) {
        if (auto st = ctx.transaction().set_crs(core::Crs(target.as_text())); !st) {
            ctx.echo(st.error().message);
            co_return;
        }
    }

    ctx.record("noktalar", Value::points(given));
    if (!lock.empty()) ctx.record("olcek_kilitli", lock);
    if (const Value target = ctx.argument("sistem"); !target.empty()) ctx.record("sistem", target);

    ctx.echo(std::to_string(touched) + " nesne haritaya oturtuldu.");
}

} // namespace

KENTOS_COMMAND(fit)
{
    return CommandSpec{
        .id       = "core.fit",
        .names    = {"OTURT", "FIT", "GEOREF", "OTR"},
        .category = Category::Modify,
        .params =
            {
                Param::points("noktalar", Arity{4, 0xFFFFFFFFu},
                              "Kontrol çiftleri: yerel, harita, yerel, harita..."),
                Param::boolean("olcek_kilitli", Arity::optional(),
                               "Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez"),
                Param::text("sistem", Arity::optional(),
                            "Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).",
        .run     = &run,
    };
}

} // namespace kentos::command

namespace kentos::command {
/// Declared here because its body lives in `stakeout_command.cpp`; the registrar
/// below is the one place this module's commands are named.
KENTOS_COMMAND(stakeout);
KENTOS_COMMAND(reproject);
} // namespace kentos::command

namespace kentos::domain::geodesy {

void register_geodesy_commands(kentos::command::Registry& r)
{
    // One entry today. It sits here rather than in the builtin X-macro list
    // because that list lives in `/src/command`, which may not depend on a domain
    // module (CLAUDE.md Article 3.2).
    (void)r.add(kentos::command::kentos_command_fit());
    (void)r.add(kentos::command::kentos_command_stakeout());
    (void)r.add(kentos::command::kentos_command_reproject());
}

} // namespace kentos::domain::geodesy
