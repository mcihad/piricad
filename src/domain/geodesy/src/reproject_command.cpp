// SPDX-License-Identifier: GPL-3.0-or-later
// core.reproject — DÖNÜŞTÜR. Move the whole drawing between coordinate systems.
//
// THE DAILY JOB THIS EXISTS FOR. Turkish cadastral archives are full of ED50
// sheets and every new one is TUREF; a municipality's own layers may be in a
// different three-degree zone from the sheet next to them. Getting from one to
// the other is a datum shift plus a projection change, and it is PROJ's work —
// §9 names it, and reimplementing a datum shift is how a boundary moves half a
// metre without anybody noticing.
//
// PROJECTED TO PROJECTED ONLY. Document geometry is stored as integer millimetres
// (Article 2.4); degrees do not fit in that unit, and rounding 29.830716° to the
// nearest "millimetre" moves the point about a hundred metres. `Transform`
// refuses a geographic side for the span overload and this command says so.
//
// ONE TRANSACTION FOR THE WHOLE DRAWING, like OTURT: every vertex moves or none
// does. A half-reprojected cadastral sheet is Article 1.6's failure, and here both
// halves would look plausible.
//
// THE CRS FOLLOWS THE GEOMETRY. A drawing whose coordinates moved but whose CRS
// still names the old system is worse than one that was never transformed: every
// reader downstream would trust the label.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/domain/geodesy/transform.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

Task<void> run(Context& ctx)
{
    if (!domain::geodesy::Transform::available()) {
        ctx.echo("PROJ bu yapıda yok; koordinat dönüşümü yapılamaz. "
                 "KENTOS_WITH_PROJ=ON ile derleyin.");
        co_return;
    }

    const Value target_arg = ctx.argument("hedef");
    if (target_arg.empty()) {
        ctx.echo("Hedef koordinat sistemi eksik. Örnek: DÖNÜŞTÜR hedef=EPSG:5256");
        co_return;
    }

    // The source defaults to what the document already says it is, which is the
    // case that needs no thinking; naming it is for a drawing whose label is
    // wrong and the user knows better.
    const std::string source = ctx.argument("kaynak").empty()
                                   ? ctx.document().crs().id()
                                   : ctx.argument("kaynak").as_text();
    const std::string target = target_arg.as_text();

    if (source.empty()) {
        ctx.echo("Çizimin koordinat sistemi tanımsız. Önce AYAR koordinat_sistemi ile "
                 "söyleyin, ya da DÖNÜŞTÜR kaynak= ile verin.");
        co_return;
    }
    if (source == target) {
        ctx.echo("Kaynak ve hedef aynı sistem: " + source + ". Yapılacak bir şey yok.");
        co_return;
    }

    auto built = domain::geodesy::Transform::between(source, target);
    if (!built) {
        ctx.echo(built.error().message);
        co_return;
    }
    const domain::geodesy::Transform& transform = built.value();

    if (!transform.projected_both_ways()) {
        ctx.echo("Bu dönüşümün bir ucu coğrafi (derece). Çizim geometrisi tam sayı "
                 "milimetredir ve derece o birime sığmaz: 29,830716° en yakın "
                 "'milimetreye' yuvarlandığında nokta yüz metre kayar. Projeksiyonlu "
                 "bir hedef seçin (örnek: EPSG:5256).");
        co_return;
    }

    // ---- move every vertex ----
    const core::Document& doc      = ctx.document();
    const core::RingGeometry& geom = doc.geometry();

    std::size_t touched = 0;
    for (core::EntityId e = 0; e < doc.entities().size(); ++e) {
        if (!doc.alive(e)) continue;

        const core::RingSpan span = geom.rings_of(doc.entities().slot[e]);

        // The vertices are copied out before anything is written: `set_geometry`
        // replaces the entity's rings and the spans above would then name storage
        // that has moved on.
        std::vector<std::vector<core::Point2>> parts;
        std::vector<core::RingGeometry::RingInput> rings;
        parts.reserve(span.count);

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);

            std::vector<core::Point2> moved;
            moved.reserve(xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v)
                moved.push_back(core::Point2{xs[v], ys[v]});

            if (auto st = transform.forward(std::span<core::Point2>(moved)); !st) {
                ctx.echo(st.error().message);
                co_return; // the bus rolls the whole drawing back
            }

            parts.push_back(std::move(moved));
            rings.push_back(
                core::RingGeometry::RingInput{parts.back(), geom.ring_role[r], geom.ring_part[r]});
        }

        if (rings.empty()) continue;
        if (auto st = ctx.transaction().set_geometry(e, rings); !st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++touched;
    }

    // THE LABEL FOLLOWS THE COORDINATES. A drawing whose numbers moved and whose
    // CRS still names the old system is worse than one never transformed: every
    // reader downstream would trust the label.
    if (auto st = ctx.transaction().set_crs(core::Crs(target)); !st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.record("kaynak", Value::text(source));
    ctx.record("hedef", Value::text(target));

    ctx.echo(std::to_string(touched) + " nesne dönüştürüldü: " + source + " -> " + target +
             "   (PROJ " + domain::geodesy::Transform::backend_version() + ")");
}

} // namespace

KENTOS_COMMAND(reproject)
{
    return CommandSpec{
        .id       = "core.reproject",
        .names    = {"DÖNÜŞTÜR", "DONUSTUR", "REPROJECT", "DNS"},
        .category = Category::Modify,
        .params =
            {
                Param::text("hedef", Arity::exactly(1),
                            "Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36"),
                Param::text("kaynak", Arity::optional(),
                            "Kaynak sistem; yoksa çizimin kendi koordinat sistemi"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.",
        .run     = &run,
    };
}

} // namespace kentos::command
