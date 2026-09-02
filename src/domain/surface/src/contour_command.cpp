// SPDX-License-Identifier: GPL-3.0-or-later
// core.contour — EŞYÜKSELTİ. The contour lines a plan sheet carries.
//
// A crew levels a site and comes back with a few hundred numbered points, each
// with a Z. This turns them into the lines a designer reads the ground from.
//
// THE HEIGHTS COME FROM THE `kot` COLUMN, which is what `NOKTALAR` fills when it
// reads a field list. A point with no `kot` is not levelled and is left out
// rather than treated as zero — a sea-level point in the middle of a hillside
// would drag every contour around it.
//
// THE TRIANGULATION IS NOT STORED. It is an intermediate; keeping it would add an
// entity kind the whole program has to learn about for a thing nobody draws.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/domain/surface/contour.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

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
    if (!domain::surface::available()) {
        ctx.echo("Üçgenleme bu yapıda yok; eş yükselti eğrisi çizilemez. "
                 "KENTOS_WITH_CDT=ON ile derleyin.");
        co_return;
    }

    const Value given = ctx.argument("aralik");
    core::Mm interval = 1000; // one metre, the interval a 1/1000 sheet usually carries
    if (!given.empty()) interval = static_cast<core::Mm>(given.as_int());
    if (interval <= 0) {
        ctx.echo("Eş yükselti aralığı sıfırdan büyük olmalı. Örnek: EŞYÜKSELTİ aralik=500 (0,5 m)");
        co_return;
    }

    const core::Document& doc    = ctx.document();
    const core::AttrTable& table = doc.attributes();
    const core::AttrId kot       = table.find("kot");
    if (kot == core::kNoAttr) {
        ctx.echo("Çizimde 'kot' sütunu yok. Kotlu bir nokta listesini NOKTALAR ile okuyun.");
        co_return;
    }

    // What to level from: the selection when there is one, otherwise every point
    // in the drawing that carries a height.
    std::vector<core::EntityId> slots;
    for (core::EntityKey k : ctx.session().bus().selection().keys()) {
        const core::EntityId slot = doc.slot_of(k);
        if (slot != core::kNoEntity && doc.alive(slot)) slots.push_back(slot);
    }
    if (slots.empty())
        for (core::EntityId e = 0; e < doc.entities().size(); ++e)
            if (doc.alive(e) && doc.entities().kind[e] == core::kPointKind) slots.push_back(e);

    std::vector<domain::surface::Level> levels;
    levels.reserve(slots.size());
    for (core::EntityId slot : slots) {
        auto height = doc.attribute(kot, slot);
        // NOT LEVELLED, NOT USED. Treating a missing height as zero would drag
        // every contour near it down to sea level.
        if (!height || !height.value().present) continue;

        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        if (span.count == 0) continue;
        const auto xs = doc.geometry().ring_xs(span.first);
        const auto ys = doc.geometry().ring_ys(span.first);
        if (xs.empty()) continue;

        levels.push_back(domain::surface::Level{core::Point2{xs[0], ys[0]},
                                                static_cast<core::Mm>(height.value().number)});
    }

    if (levels.size() < 3) {
        ctx.echo("Kotlu nokta sayısı yetersiz: " + std::to_string(levels.size()) +
                 ". Yüzey en az üç kotlu nokta ister.");
        co_return;
    }

    auto traced = domain::surface::trace_contours(levels, interval);
    if (!traced) {
        ctx.echo(traced.error().message);
        co_return;
    }
    if (traced.value().empty()) {
        ctx.echo("Bu aralıkta eş yükselti eğrisi yok: arazinin kot farkı aralıktan küçük.");
        co_return;
    }

    // The lines land on their own layer so they can be styled and switched off as
    // a set — which is what a plan sheet does with them.
    const Value layer_arg = ctx.argument("katman");
    const std::string layer_name =
        layer_arg.empty() ? std::string("ESYUKSELTI") : layer_arg.as_text();

    const command::LayerId layer = ctx.transaction().ensure_layer(layer_name);

    // The height goes on every line as an attribute, because a contour without its
    // level is a line nobody can label.
    core::AttrId height_column = table.find("kot");

    std::size_t drawn = 0;
    for (const domain::surface::Contour& c : traced.value()) {
        // CLOSED IS NOT THE SAME AS ENCLOSING. A run whose two ends meet but whose
        // area is zero is a line that went out and came back along itself — which
        // happens where a level lies exactly along a row of levelled points, and a
        // grid of them on a planned slope does that at every whole metre. Handing
        // it to `add_area` gets it refused, correctly, as a zero-area face; it is
        // a polyline, and drawing it as one is the honest answer.
        const core::Mm2 enclosed = core::ring_area(c.path);
        const bool is_face = c.closed && (enclosed > 0 || enclosed < 0);

        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{c.path, core::RingRole::Exterior, 0});

        auto created = is_face ? ctx.transaction().add_area(layer, rings)
                               : ctx.transaction().add_polyline(layer, c.path);
        if (!created) {
            ctx.echo(created.error().message);
            co_return; // the bus rolls the whole set back
        }

        if (auto st = ctx.transaction().set_attribute(height_column, created.value(),
                                                      core::attr_mm(c.height));
            !st) {
            ctx.echo(st.error().message);
            co_return;
        }
        ++drawn;
    }

    ctx.record("aralik", Value::integer(interval));
    if (!layer_arg.empty()) ctx.record("katman", layer_arg);

    ctx.echo(std::to_string(drawn) + " eş yükselti eğrisi çizildi (" + metres(interval) +
             " m aralıkla, " + std::to_string(levels.size()) + " kotlu noktadan), '" + layer_name +
             "' katmanına.");
}

} // namespace

KENTOS_COMMAND(contour)
{
    return CommandSpec{
        .id       = "core.contour",
        .names    = {"EŞYÜKSELTİ", "ESYUKSELTI", "CONTOUR", "EŞY"},
        .category = Category::Draw,
        .params =
            {
                Param::integer("aralik", Arity::optional(),
                               "Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m)"),
                Param::text("katman", Arity::optional(),
                            "Eğrilerin çizileceği katman; varsayılan ESYUKSELTI"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Scriptable | Flags::AiAccessible,
        .summary = "Kotlu noktalardan eş yükselti eğrileri çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
