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

#include "kentos_cad/command/drawing_catalogs.hpp"

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/domain/geodesy/crs_catalog.hpp"
#include "kentos_cad/domain/geodesy/transform.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// The systems a user is offered: the TM 3° zones of the data package, by the
/// names a surveyor writes (`TUREF/TM30`). Read at every call, like every
/// catalogue, so a data release takes effect without a restart; nothing when the
/// package cannot be found, and the question is still asked.
std::vector<std::string> offered_systems()
{
    std::vector<std::string> out;
    const std::string file = resolve_catalog_path("data/crs/tm3-dilimleri.json");
    if (file.empty()) return out;
    const std::string dir = std::filesystem::path(file).parent_path().string();
    auto catalogue        = domain::geodesy::CrsCatalog::load(dir);
    if (!catalogue) return out;
    // The datum as a surveyor writes it in a system's name: "TUREF (ITRF96)" is
    // `TUREF/TM30`, which is what the resolver reads (crs_service.hpp).
    const std::string& datum = catalogue.value().parameters().datum;
    const std::string prefix = datum.substr(0, datum.find(' '));
    for (const domain::geodesy::Tm3Zone& zone : catalogue.value().zones())
        out.push_back(prefix + "/" + zone.name);
    return out;
}

Task<void> run(Context& ctx)
{
    if (!domain::geodesy::Transform::available()) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "PROJ bu yapıda yok; koordinat dönüşümü yapılamaz. "
                   "KENTOS_WITH_PROJ=ON ile derleyin.");
        co_return;
    }

    // THE TARGET, NAMED OR ASKED FOR. The menu entry used to answer "zorunlu
    // 'hedef' parametresi eksik" and stop. The zones the data package declares
    // are OFFERED — never a restriction: a PROJ string or an EPSG code the
    // package does not list is still an answer (`Prompt::choices`).
    std::string target = ctx.argument("hedef").as_text();
    if (target.empty()) {
        auto typed = co_await ctx.text("hedef", "Hedef koordinat sistemi", offered_systems());
        if (!typed || typed->empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "Hedef koordinat sistemi eksik. Örnek: DÖNÜŞTÜR hedef=EPSG:5256");
            co_return;
        }
        target = *typed;
    }

    // The source defaults to what the document already says it is, which is the
    // case that needs no thinking; naming it is for a drawing whose label is
    // wrong and the user knows better.
    const std::string source = ctx.argument("kaynak").empty() ? ctx.document().crs().id()
                                                              : ctx.argument("kaynak").as_text();

    if (source.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Çizimin koordinat sistemi tanımsız. Önce AYAR koordinat_sistemi ile "
                   "söyleyin, ya da DÖNÜŞTÜR kaynak= ile verin.");
        co_return;
    }
    if (source == target) {
        ctx.echo("Kaynak ve hedef aynı sistem: " + source + ". Yapılacak bir şey yok.");
        co_return;
    }

    auto built = domain::geodesy::Transform::between(source, target);
    if (!built) {
        ctx.refuse(built.error());
        co_return;
    }
    const domain::geodesy::Transform& transform = built.value();

    if (!transform.projected_both_ways()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu dönüşümün bir ucu coğrafi (derece). Çizim geometrisi tam sayı "
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
                ctx.refuse(st.error());
                co_return; // the bus rolls the whole drawing back
            }

            parts.push_back(std::move(moved));
            rings.push_back(
                core::RingGeometry::RingInput{parts.back(), geom.ring_role[r], geom.ring_part[r]});
        }

        if (rings.empty()) continue;
        if (auto st = ctx.transaction().set_geometry(e, rings); !st) {
            ctx.refuse(st.error());
            co_return;
        }
        ++touched;
    }

    // THE LABEL FOLLOWS THE COORDINATES. A drawing whose numbers moved and whose
    // CRS still names the old system is worse than one never transformed: every
    // reader downstream would trust the label.
    if (auto st = ctx.transaction().set_crs(core::Crs(target)); !st) {
        ctx.refuse(st.error());
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
        .title    = "Dönüştür",
        .category = Category::Modify,
        .params =
            {
                Param::text("hedef", Arity::exactly(1),
                            "Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36")
                    .en("target"),
                Param::text("kaynak", Arity::optional(),
                            "Kaynak sistem; yoksa çizimin kendi koordinat sistemi")
                    .en("source"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.",
        .run     = &run,
    };
}

} // namespace kentos::command
