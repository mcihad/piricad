// SPDX-License-Identifier: GPL-3.0-or-later
// core.stretch — ESNET.
//
// THE VERB EVERY OTHER ONE IS A SPECIAL CASE OF. `TAŞI` moves a whole object and
// `KÖŞETAŞI` moves one named corner; a stretch moves SOME of an object's points
// and leaves the rest, which is what a surveyor does when a road widens and the
// parcels along one side of it have to follow while their far boundaries stay on
// the tapu.
//
// WHICH POINTS FOLLOW IS DECIDED BY A WINDOW, not by a selection. That is what
// this command was waiting for and why the plan deferred it: selecting a VERTEX
// needs machinery this program does not have — selection here is per object. But
// the window IS the vertex filter, exactly as it is in every CAD: a grip inside
// the crossing window follows, a grip outside it stays, and an object with no
// grip inside is not touched at all. Nothing has to be selectable for that to
// work, so the deferral's condition is met by re-reading the problem rather than
// by building a vertex-selection model (TODOS-CAD P3).
//
// EVERY TARGET IS COMPUTED FROM THE SNAPSHOT, before anything moves. A circle's
// grips are its centre and a radius handle; moving the centre already carries the
// handle, so a second move computed against the LIVE geometry would move it twice
// and grow the circle. Against the snapshot the second move lands where the
// handle already is and changes nothing — a translation, which is what windowing
// a whole circle should do.
//
// A POLYLINE IS WRITTEN IN ONE GO and every other kind grip by grip. The split is
// `core.vertex_move`'s own (vertex.cpp): a polyline's rings ARE its shape, so all
// its moved corners go in one `set_geometry` and no half-moved ring is ever
// offered to the validator. A definition-bearing kind — circle, arc, ellipse,
// dimension, block reference — goes through `core::move_grip`, which is the one
// place that knows what moving THAT kind's handle means.
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/pick.hpp"

#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// Whether `p` lies in the closed box.
bool inside(const core::Box2& b, core::Point2 p)
{
    return p.x >= b.min_x && p.x <= b.max_x && p.y >= b.min_y && p.y <= b.max_y;
}

/// The box two corners make, whichever way round they were given.
core::Box2 box_of(core::Point2 a, core::Point2 b)
{
    return core::Box2{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.x > b.x ? a.x : b.x,
                      a.y > b.y ? a.y : b.y};
}

/// Moves the windowed corners of one polyline, all of them in one write.
core::Status stretch_polyline(Context& ctx, core::EntityId slot, const core::Box2& window,
                              core::Mm dx, core::Mm dy, std::size_t& moved)
{
    const core::RingGeometry& g = ctx.document().geometry();
    const core::RingSpan span   = g.rings_of(ctx.document().entities().slot[slot]);

    std::vector<std::vector<core::Point2>> rings;
    std::vector<core::RingGeometry::RingInput> input;
    rings.reserve(span.count);
    input.reserve(span.count);

    std::size_t here = 0;
    for (std::uint32_t i = 0; i < span.count; ++i) {
        const std::uint32_t ring = span.first + i;
        const auto xs            = g.ring_xs(ring);
        const auto ys            = g.ring_ys(ring);

        std::vector<core::Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v) {
            core::Point2 p{xs[v], ys[v]};
            if (inside(window, p)) {
                p.x += dx;
                p.y += dy;
                ++here;
            }
            pts.push_back(p);
        }
        rings.push_back(std::move(pts));
    }

    if (here == 0) return core::Status{};

    for (std::uint32_t i = 0; i < span.count; ++i)
        input.push_back(core::RingGeometry::RingInput{rings[i], g.ring_role[span.first + i],
                                                      g.ring_part[span.first + i]});

    if (auto st = ctx.transaction().set_geometry(slot, input); !st) return st.error();
    moved += here;
    return core::Status{};
}

/// Moves the windowed grips of one definition-bearing kind, one at a time, each
/// to a target taken from the snapshot.
core::Status stretch_by_grips(Context& ctx, core::EntityId slot, const core::Box2& window,
                              core::Mm dx, core::Mm dy, std::size_t& moved)
{
    // SNAPSHOT FIRST. `move_grip` reads the live document, so the positions that
    // decide what is windowed and where each grip goes are taken once, before the
    // first write.
    const std::vector<core::GripPoint> before = core::entity_grips(ctx.document(), slot);

    for (std::size_t i = 0; i < before.size(); ++i) {
        if (!inside(window, before[i].at)) continue;

        const core::Point2 to{before[i].at.x + dx, before[i].at.y + dy};
        auto edit = core::move_grip(ctx.document(), slot, i, to);
        if (!edit) return edit.error();

        const auto inputs = edit.value().inputs();
        if (auto st = ctx.transaction().set_kind_geometry(slot, inputs, edit.value().payload); !st)
            return st.error();
        ++moved;
    }
    return core::Status{};
}

Task<void> run(Context& ctx)
{
    // THE WINDOW FIRST, because it is what the command is about. A crossing
    // window, so a parcel whose far boundary stays put is still a candidate: a
    // `Window` pick would need the whole object inside and would then have
    // nothing to leave behind.
    auto corner_a = co_await ctx.point("pencere", "Esnetme penceresinin bir köşesi");
    if (!corner_a) co_return;
    auto corner_b = co_await ctx.point("pencere", "Pencerenin karşı köşesi",
                                       PointOptions{.rubber_band   = true,
                                                    .rubber_origin = *corner_a,
                                                    .rubber_shape  = RubberShape::Rectangle});
    if (!corner_b) co_return;

    const core::Box2 window = box_of(*corner_a, *corner_b);
    if (window.min_x == window.max_x || window.min_y == window.max_y) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Esnetme penceresi bir çizgi: iki köşe aynı sırada ya da "
                                     "aynı kolonda. Alanı olan bir pencere verin."));
        co_return;
    }

    auto from = co_await ctx.point("baslangic", "Esnetmenin başlangıç noktası");
    if (!from) co_return;
    auto to = co_await ctx.point("bitis", "Esnetmenin bitiş noktası",
                                 PointOptions{.rubber_band   = true,
                                              .rubber_origin = *from,
                                              .rubber_shape  = RubberShape::Line});
    if (!to) co_return;

    const core::Mm dx = to->x - from->x;
    const core::Mm dy = to->y - from->y;

    // THE CANDIDATES: the named objects if any were named, otherwise everything
    // the window touches. Naming them is the restriction a script wants and the
    // window is the one a hand wants; neither is a privilege the other lacks
    // (Article 1.2).
    std::vector<core::EntityId> slots;
    const Value named = ctx.argument("nesneler");
    if (!named.empty()) {
        for (const std::int64_t raw : named.as_ids()) {
            if (raw <= 0) continue;
            const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
            const core::EntityId slot = ctx.document().slot_of(key);
            if (slot != core::kNoEntity && ctx.document().alive(slot)) slots.push_back(slot);
        }
    } else {
        core::pick_in_box(ctx.document(), window, core::PickMode::Crossing, slots);
    }

    std::size_t moved   = 0;
    std::size_t touched = 0;
    std::size_t locked  = 0;
    std::vector<std::int64_t> did;

    for (const core::EntityId slot : slots) {
        // A LOCKED LAYER REFUSES AND THE RUN CONTINUES, exactly as the other edit
        // verbs do: a window over a sheet is a rough gesture and one locked parcel
        // under it is not a reason to abandon the whole stretch. It is counted and
        // said, because a silent skip is a stretch that looks like it worked.
        if (auto st = ctx.document().editable(slot); !st) {
            ++locked;
            continue;
        }

        const std::size_t was = moved;
        core::Status st       = ctx.document().entities().kind[slot] == core::kPolylineKind
                                    ? stretch_polyline(ctx, slot, window, dx, dy, moved)
                                    : stretch_by_grips(ctx, slot, window, dx, dy, moved);
        if (!st) {
            // A ring that now crosses itself, or a handle a kind refuses, names
            // itself. The bus rolls the whole transaction back — no half-stretched
            // sheet, ever (Article 1.6).
            ctx.session().fail(st.error());
            co_return;
        }
        if (moved > was) {
            ++touched;
            did.push_back(static_cast<std::int64_t>(ctx.document().key_of(slot)));
        }
    }

    if (moved == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   locked > 0 ? "Pencerede esnetilecek köşe yok; " + std::to_string(locked) +
                                    " nesne kilitli katmanda."
                              : "Pencerede esnetilecek köşe yok. Pencere, taşınacak köşelerin "
                                "üzerinden geçmelidir.");
        co_return;
    }

    ctx.record("pencere", Value::points({*corner_a, *corner_b}));
    ctx.record("baslangic", Value::point(*from));
    ctx.record("bitis", Value::point(*to));
    // THE RESOLVED OBJECTS, so a replay stretches exactly what this run stretched
    // rather than whatever the window happens to touch in the document it is
    // replayed into (model.md P4).
    ctx.record("nesneler", Value::ids(did));

    ctx.echo(std::to_string(moved) + " köşe esnetildi (" + std::to_string(touched) + " nesne)" +
             (locked > 0 ? ", " + std::to_string(locked) + " nesne kilitli katmanda atlandı" : "") +
             ".");
}

} // namespace

KENTOS_COMMAND(stretch)
{
    return CommandSpec{
        .id       = "core.stretch",
        .names    = {"ESNET", "STRETCH", "ES"},
        .title    = "Esnet",
        .category = Category::Modify,
        .params =
            {
                Param::points("pencere", Arity{2, 2},
                              "Esnetme penceresinin iki köşesi; içindeki köşeler taşınır")
                    .en("window"),
                Param::point("baslangic", "Esnetmenin başlangıç noktası").en("start"),
                Param::point("bitis", "Esnetmenin bitiş noktası").en("end"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Yalnız bu nesneler esnetilir; verilmezse pencerenin dokunduğu her nesne"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır.",
        .run     = &run,
    };
}

} // namespace kentos::command
