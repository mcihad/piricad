// SPDX-License-Identifier: GPL-3.0-or-later
// core.split_parcel — İFRAZ. One parcel becomes two along a cutting line.
//
// THE CUT IS A STRAIGHT LINE THROUGH THE PARCEL, given as two points. That is
// what an ifraz almost always is on a sheet — a boundary agreed between the
// parties or laid down by a plan — and it is the one form whose result is exact:
// the line is extended past the parcel and the two half-planes are intersected
// with it, so no band is subtracted and no area is lost. A cut described by a
// polyline is a different problem and is not attempted here rather than being
// approximated, because a few square centimetres missing from a parsel is a few
// square centimetres somebody owns.
//
// AREAS ARE REPORTED, NEVER TARGETED. "Split this parcel into 400 m² and the
// rest" is an ifraz by area, it needs an iteration and a tolerance, and both of
// those are regulatory decisions (6.11, 5.13). This command cuts where it is told
// and prints what came out, so the surveyor can check it against the plan.
//
// THE ATTRIBUTES ARE COPIED TO BOTH SIDES, unchanged. That is not a rule about
// what an ifraz means — the new ada/parsel numbers come from TKGM — it is the
// only non-destructive thing to do with what was there, and the command says so.
#include "piricad/command/bus.hpp"
#include "piricad/command/context.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/spec.hpp"

#include "piricad/core/curve_path.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/offset.hpp"

#include "parcel_face.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace piricad::command {
namespace {

core::Mm2 abs_area(core::Mm2 v)
{
    return v < 0 ? -v : v;
}

std::string square_metres(core::Mm2 v)
{
    const auto cm2   = static_cast<std::uint64_t>((abs_area(v) + 5000) / 10000);
    std::string frac = std::to_string(cm2 % 100);
    if (frac.size() < 2) frac = "0" + frac;
    return std::to_string(cm2 / 100) + "," + frac + " m²";
}

Task<void> run(Context& ctx)
{
    // The parcel: named, highlighted, or ASKED FOR. The menu entry used to
    // answer "Seçili: 0" and stop, so the command could not be started the way
    // a hand starts it — reach for the tool, then point at the parcel.
    Value::Ints requested;
    if (!co_await want_objects(ctx, "nesneler", "İfraz edilecek parseli seçin, sonra Enter",
                               requested, 1, "İFRAZ nesneler=1 noktalar=10,-5 10,25"))
        co_return;

    auto first = co_await ctx.point("noktalar", "Ayırma çizgisinin ilk noktası");
    if (!first) co_return; // ESC before anything was cut

    auto second = co_await ctx.point("noktalar", "Ayırma çizgisinin ikinci noktası",
                                     PointOptions{.rubber_band = true, .rubber_origin = *first});
    if (!second) co_return;

    if (first->x == second->x && first->y == second->y) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Ayırma çizgisinin iki ucu aynı yerde; kesme yönü belirsiz.");
        co_return;
    }

    const core::Document& doc = ctx.document();
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(requested.front()));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(requested.front()));
        co_return;
    }

    // THE PARCEL AS A FACE, its arcs as arcs: a rounded corner stays on the
    // piece it falls in, the same arc (parcel_face.hpp, TODOS O-3).
    const std::optional<cadastre::ParcelFace> parcel = cadastre::parcel_face(doc, slot);
    if (!parcel) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(requested.front()) +
                       " kapalı bir alan değil; ifraz yalnız alanlar üzerinde çalışır.");
        co_return;
    }

    const core::Box2 box   = core::path_bounds(parcel->face.outer);
    const core::Mm2 before = cadastre::outer_area(parcel->face);

    std::vector<core::KernelFace> pieces;
    for (bool left : {true, false}) {
        const core::KernelFace side =
            cadastre::face_of(core::half_plane(*first, *second, box, left));
        auto part = cadastre::parcel_boolean(std::span(&parcel->face, 1), std::span(&side, 1),
                                             core::BooleanOp::Intersection, parcel->curved);
        if (!part) {
            ctx.refuse(part.error());
            co_return;
        }
        for (core::KernelFace& piece : part.value())
            pieces.push_back(std::move(piece));
    }

    if (pieces.size() < 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu çizgi parseli kesmiyor: ifraz için çizginin parselin içinden geçmesi "
                   "gerekir.");
        co_return;
    }

    // ---- write the pieces, copy the attributes, remove the original ----
    const core::AttrTable& table = doc.attributes();
    std::string said             = "İfraz: " + std::to_string(pieces.size()) + " parça.";

    for (const core::KernelFace& piece : pieces) {
        auto created = cadastre::add_face(ctx, ctx.active_layer(), piece);
        if (!created) {
            ctx.refuse(created.error());
            co_return;
        }
        // THE PARENT, remembered by key (core/lineage.hpp): it leaves the sheet
        // below, and the piece still says which parcel it was cut from.
        const core::EntityId parent[] = {slot};
        if (auto st = ctx.derive(created.value(), std::span<const core::EntityId>(parent)); !st) {
            ctx.refuse(st.error());
            co_return;
        }

        for (std::size_t c = 0; c < table.columns(); ++c) {
            const auto col = static_cast<core::AttrId>(c);
            auto had       = doc.attribute(col, slot);
            if (!had || !had.value().present) continue;
            if (auto st = ctx.transaction().set_attribute(col, created.value(), had.value()); !st) {
                ctx.refuse(st.error());
                co_return;
            }
        }

        said += "\n  " + square_metres(cadastre::outer_area(piece));
    }

    if (auto st = ctx.transaction().erase_entity(slot); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    // THE SUM IS PRINTED because it is what a surveyor checks first: an ifraz that
    // loses area has cut something it should not have, and a difference of a few
    // square centimetres is a few square centimetres somebody owns.
    core::Mm2 after = 0;
    for (const core::KernelFace& piece : pieces)
        after += cadastre::outer_area(piece);
    said += "\n  toplam " + square_metres(after) + "  ·  ifrazdan önce " + square_metres(before);

    ctx.record("nesneler", Value::ids(requested));
    ctx.record("noktalar", Value::points({*first, *second}));
    ctx.echo(said);
}

} // namespace

PIRICAD_COMMAND(split_parcel)
{
    return CommandSpec{
        .id       = "core.split_parcel",
        .names    = {"İFRAZ", "IFRAZ", "SUBDIVIDE", "İFR"},
        .title    = "İfraz",
        .category = Category::Modify,
        .params =
            {
                // `noktalar` FIRST, and the order is load bearing. A bare token
                // after a named argument is positional, and the parser hands
                // positionals to the declared params in order — with the
                // unbounded `nesneler` first, the cutting line's second point was
                // swallowed as an object id and İFRAZ waited forever for a point
                // it had already been given.
                Param::points("noktalar", Arity{0, 2}, "Ayırma çizgisinin iki ucu").en("points"),
                Param{"nesneler", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Ayrılacak parsel; yoksa etkin seçim"}
                    .en("objects"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).",
        .run     = &run,
        .targets = Targets::Faces,
    };
}

} // namespace piricad::command
