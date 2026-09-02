// SPDX-License-Identifier: GPL-3.0-or-later
// core.vertex_move (KÖŞETAŞI), core.vertex_insert (KÖŞEEKLE) — corner editing.
//
// A drawing is not finished when it is drawn. A parsel corner lands on the wrong
// monument, a road edge needs a bend the surveyor did not measure the first time,
// and until these commands existed the only repair this program offered was to
// erase the object and draw it again — which mints a new key, drops every
// attribute hung off the old one and puts a hole in the journal where a
// correction should be (model.md R4, R28).
//
// So the geometry is REPLACED and the identity is kept. `Transaction::set_geometry`
// is the primitive; everything here is the arithmetic of turning "corner 3 of
// parsel 12" into a new ring list.
//
// A vertex is addressed by a 1-BASED index that runs over the entity's rings in
// order — the exterior first, then each hole (R11). The user reads corner numbers
// off the same drawing the program numbered, and a surveyor counting corners on a
// parsel starts at one. Ring-and-offset is what this converts to internally, and
// it never reaches a command line or a journal line.
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One entity's geometry, unpacked into something that can be edited and handed
/// straight back to `set_geometry`.
struct Rings
{
    std::vector<std::vector<core::Point2>> points; ///< one vector per ring
    std::vector<core::RingRole> roles;
    std::vector<std::uint16_t> parts;

    std::size_t vertex_count() const
    {
        std::size_t n = 0;
        for (const auto& r : points) n += r.size();
        return n;
    }
};

/// Reads the rings of one entity out of the document's arena.
Rings read_rings(const core::Document& doc, core::EntityId slot)
{
    Rings out;
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);

    for (std::uint32_t i = 0; i < span.count; ++i) {
        const std::uint32_t ring = span.first + i;
        const auto xs            = doc.geometry().ring_xs(ring);
        const auto ys            = doc.geometry().ring_ys(ring);

        std::vector<core::Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v) pts.push_back(core::Point2{xs[v], ys[v]});

        out.points.push_back(std::move(pts));
        out.roles.push_back(doc.geometry().ring_role[ring]);
        out.parts.push_back(doc.geometry().ring_part[ring]);
    }
    return out;
}

/// Hands `rings` to the transaction in the shape `set_geometry` wants. The spans
/// borrow from `r`, so it must outlive the call — which it does, since both are
/// locals of the command body.
core::Status write_rings(Context& ctx, core::EntityId slot, const Rings& r)
{
    std::vector<core::RingGeometry::RingInput> input;
    input.reserve(r.points.size());
    for (std::size_t i = 0; i < r.points.size(); ++i)
        input.push_back(core::RingGeometry::RingInput{r.points[i], r.roles[i], r.parts[i]});

    return ctx.transaction().set_geometry(slot, input);
}

/// Where a 1-based whole-entity corner number lands. `ring` is an index into
/// `Rings::points`, `at` an index inside that ring.
struct Where
{
    std::size_t ring{0};
    std::size_t at{0};
    bool found{false};
};

Where locate(const Rings& r, std::int64_t one_based)
{
    if (one_based < 1) return {};

    auto remaining = static_cast<std::size_t>(one_based - 1);
    for (std::size_t i = 0; i < r.points.size(); ++i) {
        if (remaining < r.points[i].size()) return Where{i, remaining, true};
        remaining -= r.points[i].size();
    }
    return {};
}

/// Reads a single whole number out of an argument, whichever shape it arrived in.
///
/// A parameter declared `Arity::exactly(1)` binds as a scalar `Kind::Int`, while a
/// selection and a repeated argument bind as `Kind::IdList`. Both spell the same
/// thing at a command line — `kose=2` — so both are accepted here rather than
/// making the caller know which arity the declaration happened to use.
bool single_id(const Value& v, std::int64_t& out, std::size_t& count)
{
    if (v.kind() == Value::Kind::IdList) {
        const Value::Ints& ids = v.as_ids();
        count                  = ids.size();
        if (ids.size() != 1) return false;
        out = ids[0];
        return true;
    }
    if (v.kind() == Value::Kind::Int) {
        count = 1;
        out   = v.as_int();
        return true;
    }
    count = 0;
    return false;
}

/// Resolves the `nesne` argument to a live entity slot, or echoes why not.
///
/// The argument is a persistent KEY, like `SİL`'s and `SEÇ`'s, because a slot is
/// meaningful only inside one in-memory document and a journalled slot replays
/// onto whatever entity holds that index next — in a cadastral drawing, the
/// neighbouring parsel (model.md R5, P4).
bool resolve_entity(Context& ctx, core::EntityId& out)
{
    const Value given = ctx.argument("nesne");
    if (given.empty()) {
        ctx.echo("Düzenlenecek nesne belirtilmedi. Örnek: KÖŞETAŞI nesne=1 kose=2");
        return false;
    }

    std::int64_t id    = 0;
    std::size_t count  = 0;
    if (!single_id(given, id, count)) {
        ctx.echo("Bir seferde tek nesne düzenlenir; " + std::to_string(count) + " nesne verildi.");
        return false;
    }
    if (id <= 0) {
        ctx.echo("Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }

    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    const core::EntityId slot = ctx.document().slot_of(key);
    if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }

    // A CURVE HAS NO CORNERS. Its stored vertices are its definition — a circle's
    // are a centre and a radius handle, an arc's add the two ends — not a
    // boundary. Dragging one as if it were a corner would silently move the
    // centre or resize the curve, and inserting another would leave a record that
    // is no longer a curve at all (core/circle.hpp, core/arc.hpp).
    if (ctx.document().entities().kind[slot] != core::kPolylineKind) {
        ctx.echo("Nesne " + std::to_string(id) +
                 " bir eğri; eğrinin köşesi yoktur. Ölçüsünü değiştirmek için silip "
                 "yeniden çizin.");
        return false;
    }

    out = slot;
    return true;
}

/// The `kose` argument, or a message naming what was wrong with it. ASCII in the
/// NAME, like every declared parameter in this program: a surveyor types it on
/// whatever keyboard is in front of them. The MESSAGES stay Turkish.
bool resolve_corner(Context& ctx, std::int64_t& out)
{
    const Value given = ctx.argument("kose");
    if (given.empty()) {
        ctx.echo("Köşe numarası belirtilmedi. İlk köşe 1'dir.");
        return false;
    }

    std::size_t count = 0;
    if (!single_id(given, out, count)) {
        ctx.echo("Tek bir köşe numarası beklenir; " + std::to_string(count) + " değer verildi.");
        return false;
    }
    return true;
}

Task<void> run_move(Context& ctx)
{
    core::EntityId slot = core::kNoEntity;
    if (!resolve_entity(ctx, slot)) co_return;

    std::int64_t corner = 0;
    if (!resolve_corner(ctx, corner)) co_return;

    Rings rings         = read_rings(ctx.document(), slot);
    const Where where   = locate(rings, corner);
    if (!where.found) {
        ctx.echo("Bu nesnenin " + std::to_string(corner) + ". köşesi yok; " +
                 std::to_string(rings.vertex_count()) + " köşesi var.");
        co_return;
    }

    // The guide runs from the corner being moved, so the user sees the two edges
    // that will follow it rather than a line from nowhere.
    const core::Point2 from = rings.points[where.ring][where.at];

    auto to = co_await ctx.point("nokta", "Köşenin yeni yeri",
                                 PointOptions{.rubber_band = true, .rubber_origin = from});
    if (!to) co_return; // ESC leaves the corner where it was

    rings.points[where.ring][where.at] = *to;

    auto st = write_rings(ctx, slot, rings);
    if (!st) {
        // A ring that now crosses itself, or a hole that has escaped its exterior,
        // is refused by the geometry layer and the message names which. The bus
        // rolls the transaction back, so the corner does not half-move.
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.record("nesne", ctx.argument("nesne"));
    // The DECLARED shape, so replaying the journal line binds exactly what this
    // run bound: `kose` is an `Arity::exactly(1)` integer, so it records as one.
    ctx.record("kose", Value::integer(corner));
    ctx.record("nokta", Value::point(*to));
}

Task<void> run_insert(Context& ctx)
{
    core::EntityId slot = core::kNoEntity;
    if (!resolve_entity(ctx, slot)) co_return;

    std::int64_t corner = 0;
    if (!resolve_corner(ctx, corner)) co_return;

    Rings rings       = read_rings(ctx.document(), slot);
    const Where where = locate(rings, corner);
    if (!where.found) {
        ctx.echo("Bu nesnenin " + std::to_string(corner) + ". köşesi yok; " +
                 std::to_string(rings.vertex_count()) + " köşesi var.");
        co_return;
    }

    // `kose` names the corner the new one comes AFTER, so it names a SEGMENT: the
    // one leaving that corner. On a closed ring the last corner's segment is the
    // closing edge, which is why the wrap is a modulus and not a refusal — a
    // parsel's closing edge is an edge like any other and needs a bend as often.
    const std::vector<core::Point2>& ring = rings.points[where.ring];
    const bool closed                     = rings.roles[where.ring] != core::RingRole::Open;

    if (!closed && where.at + 1 >= ring.size()) {
        ctx.echo("Son köşeden sonra kenar yok: açık bir çizgide " + std::to_string(corner) +
                 ". köşe uçtur. Araya köşe eklemek için ondan önceki bir köşe verin.");
        co_return;
    }

    // The guide starts at the corner the segment leaves, which is where the new
    // bend will hinge from.
    const core::Point2 a = ring[where.at];

    auto at = co_await ctx.point("nokta", "Yeni köşenin yeri",
                                 PointOptions{.rubber_band = true, .rubber_origin = a});
    if (!at) co_return;

    rings.points[where.ring].insert(
        rings.points[where.ring].begin() + static_cast<std::ptrdiff_t>(where.at) + 1, *at);

    auto st = write_rings(ctx, slot, rings);
    if (!st) {
        ctx.echo(st.error().message);
        co_return;
    }

    ctx.record("nesne", ctx.argument("nesne"));
    // The DECLARED shape, so replaying the journal line binds exactly what this
    // run bound: `kose` is an `Arity::exactly(1)` integer, so it records as one.
    ctx.record("kose", Value::integer(corner));
    ctx.record("nokta", Value::point(*at));
}

} // namespace

KENTOS_COMMAND(vertex_move)
{
    return CommandSpec{
        .id       = "core.vertex_move",
        .names    = {"KÖŞETAŞI", "KOSETASI", "MOVEVERTEX", "KT"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşesi taşınacak nesnenin kimliği"},
                Param::integer("kose", Arity::exactly(1),
                               "Taşınacak köşenin sırası; ilk köşe 1'dir"),
                Param::point("nokta", "Köşenin yeni yeri"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesnenin köşesini yeni bir yere taşır.",
        .run     = &run_move,
    };
}

KENTOS_COMMAND(vertex_insert)
{
    return CommandSpec{
        .id       = "core.vertex_insert",
        .names    = {"KÖŞEEKLE", "KOSEEKLE", "ADDVERTEX", "KE"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşe eklenecek nesnenin kimliği"},
                Param::integer("kose", Arity::exactly(1),
                               "Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir"),
                Param::point("nokta", "Yeni köşenin yeri"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir kenarın ortasına yeni köşe ekler.",
        .run     = &run_insert,
    };
}

} // namespace kentos::command
