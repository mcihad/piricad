// SPDX-License-Identifier: GPL-3.0-or-later
// core.offset — OFSET. A parallel of what is selected, at a given distance.
//
// The construction a cadastral or zoning job leans on hardest after the line
// itself: a road right-of-way from a centre line, a çekme mesafesi from a parcel
// boundary, a protection band along a watercourse. Netcad users reach for it by
// reflex and it shipped as a disabled button for a phase.
//
// THE PARALLEL IS A NEW OBJECT, not a change to the old one. Offsetting a
// boundary must leave the boundary where it is — it is the measured thing and the
// parallel is a derived one — so this adds and never edits.
//
// The geometry itself is Clipper2's, behind `core/offset.hpp`: CLAUDE.md 5.16
// forbids hand-rolling a solved problem, and a hand-rolled offset is wrong on
// exactly the shapes a parcel has (reflex corners, near-doubled-back edges).
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/units.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

core::JoinStyle join_from(const std::string& word)
{
    if (core::turkish_key_equals(word, "yuvarlak")) return core::JoinStyle::Round;
    if (core::turkish_key_equals(word, "pah")) return core::JoinStyle::Bevel;
    return core::JoinStyle::Miter;
}

Task<void> run(Context& ctx)
{
    // The argument, the selection, or ASKED FOR — see `want_objects`. Refusing an
    // empty selection meant the tool-column button did nothing unless the user had
    // already highlighted something.
    Value::Ints requested;
    if (!co_await want_objects(ctx, "nesneler", "Paraleli çizilecek nesneleri seçin, Enter'a basın",
                               requested))
        co_return;

    // The distance is a LENGTH the user states, so it is asked for the way a
    // length is asked for and recorded in millimetres like every other measure.
    const Value given = ctx.argument("mesafe");
    core::Mm distance = 0;
    if (!given.empty()) {
        distance = static_cast<core::Mm>(given.as_int());
    } else {
        auto typed = co_await ctx.number("mesafe", "Ofset mesafesi (metre; eksi değer içeri)");
        if (!typed) co_return; // ESC before anything was drawn
        distance = core::mm_round(*typed * static_cast<double>(core::kMmPerMetre));
    }

    if (distance == 0) {
        ctx.echo("Ofset mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.");
        co_return;
    }

    const core::JoinStyle join = join_from(ctx.argument("kose").as_text());

    const core::Document& doc = ctx.document();
    std::size_t made          = 0;

    for (std::int64_t raw : requested) {
        if (raw <= 0) {
            ctx.echo("Geçersiz nesne kimliği: " + std::to_string(raw) +
                     ". Kimlikler 1'den başlar.");
            co_return; // the bus rolls the whole transaction back
        }

        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }

        const core::RingGeometry& geom = doc.geometry();
        const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);

            std::vector<core::Point2> points;
            points.reserve(xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v)
                points.push_back(core::Point2{xs[v], ys[v]});

            const bool closed = geom.ring_role[r] != core::RingRole::Open;

            auto parallel = core::offset_ring(points, closed, distance, join);
            if (!parallel) {
                ctx.echo(parallel.error().message);
                co_return;
            }

            // NONE IS AN ANSWER. Shrinking a parcel past half its own width leaves
            // nothing, and saying so is the honest result — a hand-rolled offset
            // would return an inside-out ring here and draw a knot.
            if (parallel.value().empty()) {
                ctx.echo("Bu mesafede paralel kalmıyor: şekil kendi içinde kapanıyor.");
                continue;
            }

            for (const core::OffsetRing& ring : parallel.value()) {
                const core::RingGeometry::RingInput input{ring.points, core::RingRole::Exterior, 0};
                auto created = ctx.transaction().add_area(ctx.active_layer(), {&input, 1});
                if (!created) {
                    ctx.echo(created.error().message);
                    co_return;
                }
                ++made;
            }
        }
    }

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("mesafe", Value::integer(distance));
    if (!ctx.argument("kose").empty()) ctx.record("kose", ctx.argument("kose"));

    ctx.echo(std::to_string(made) + " paralel çizildi (" +
             std::to_string(distance / core::kMmPerMetre) + " m).");
}

} // namespace

KENTOS_COMMAND(offset)
{
    return CommandSpec{
        .id       = "core.offset",
        .names    = {"OFSET", "OFFSET", "OF"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ofseti alınacak nesneler; yoksa etkin seçim"},
                Param::integer("mesafe", Arity::optional(),
                               "Ofset mesafesi, milimetre; eksi değer içeri"),
                Param::text("kose", Arity::optional(),
                            "KÖŞE | YUVARLAK | PAH — dış köşenin biçimi"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Seçili nesnelerin verilen mesafede paralelini çizer.",
        .run     = &run,
    };
}

} // namespace kentos::command
