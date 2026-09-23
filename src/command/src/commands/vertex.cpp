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
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <optional>
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
        for (const auto& r : points)
            n += r.size();
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
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(core::Point2{xs[v], ys[v]});

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
///
/// `corners_only` is what KÖŞEEKLE asks: a new corner can only go into a
/// polyline, because every other kind's vertices are a DEFINITION and one more
/// of them would leave a record that is no longer that kind.
bool resolve_entity(const Context& ctx, const Value& given, core::EntityId& out, bool corners_only)
{
    if (given.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Düzenlenecek nesne belirtilmedi. Örnek: KÖŞETAŞI nesne=1 kose=2");
        return false;
    }

    std::int64_t id   = 0;
    std::size_t count = 0;
    if (!single_id(given, id, count)) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Bir seferde tek nesne düzenlenir; " +
                                                         std::to_string(count) + " nesne verildi.");
        return false;
    }
    if (id <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }

    const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    const core::EntityId slot = ctx.document().slot_of(key);
    if (slot == core::kNoEntity || !ctx.document().alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }

    if (auto st = ctx.document().editable(slot); !st) {
        ctx.refuse(st.error());
        return false;
    }

    // A CURVE HAS NO CORNERS TO ADD. Its stored vertices are its definition — a
    // circle's are a centre and a radius handle, an arc's add the two ends — and
    // one more would leave a record that is no longer a curve at all. Moving one
    // is another matter: every kind names its grips (core/grips.hpp).
    if (corners_only && ctx.document().entities().kind[slot] != core::kPolylineKind) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Nesne " + std::to_string(id) +
                       " bir eğri; eğrinin arasına köşe eklenemez. Tutamaklarını KÖŞETAŞI ile "
                       "taşıyabilirsiniz.");
        return false;
    }

    out = slot;
    return true;
}

/// WHICH OBJECT AND WHICH CORNER, the way a hand says it: ONE CLICK.
///
/// The named `nesne` and `kose` are read first, then a single highlighted object
/// stands in for `nesne`; whatever is still missing is asked for as a POINT on
/// the drawing (`yer`), which names the object under it and the corner — or,
/// for KÖŞEEKLE, the edge — nearest it. The menu entries used to answer
/// "Düzenlenecek nesne belirtilmedi" and stop, so a corner could be moved only
/// by somebody who knew the object's key and the corner's number: two things a
/// drawing shows as a place, never as numbers.
///
/// Returns false when the command is over. On success `object` holds the ids
/// to record and `corner` the 1-based corner — the grip moved, or the corner
/// the new one follows.
Task<bool> pick_corner(Context& ctx, bool insert, Value& object, core::EntityId& slot,
                       std::int64_t& corner)
{
    const core::Document& doc = ctx.document();
    Bus& bus                  = ctx.session().bus();

    object = ctx.argument("nesne");
    if (object.empty()) {
        const auto keys = bus.selection().keys();
        if (keys.size() == 1) object = Value::ids({static_cast<std::int64_t>(core::raw(keys[0]))});
    }

    const Value numbered = ctx.argument("kose");
    std::optional<core::Point2> pointed;
    if (object.empty() || numbered.empty()) {
        pointed = co_await ctx.point("yer", insert ? "Köşe eklenecek kenara tıklayın"
                                                   : "Taşınacak köşeye tıklayın");
        if (!pointed) {
            if (object.empty())
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           std::string("Düzenlenecek nesne belirtilmedi. Örnek: ") +
                               (insert ? "KÖŞEEKLE" : "KÖŞETAŞI") + " nesne=1 kose=2");
            else
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Köşe numarası belirtilmedi. İlk köşe 1'dir.");
            co_return false;
        }
        if (object.empty()) {
            // The click's own pick box — the one SEÇ uses — so what is under
            // the cursor is what is taken. A client with no screen picks what
            // lies exactly under the point, which a corner always does.
            const core::EntityId hit =
                core::pick_nearest(doc, *pointed, bus.aid_settings().pick_radius);
            if (hit == core::kNoEntity) {
                ctx.refuse(core::ErrorCode::NotFound,
                           insert ? "Orada köşe eklenecek bir çizgi ya da alan yok. Bir kenarın "
                                    "üzerine tıklayın."
                                  : "Orada köşesi taşınacak bir nesne yok. Bir nesnenin köşesine "
                                    "tıklayın.");
                co_return false;
            }
            object = Value::ids({static_cast<std::int64_t>(core::raw(doc.key_of(hit)))});
        }
    }

    if (!resolve_entity(ctx, object, slot, insert)) co_return false;

    if (!numbered.empty()) {
        std::size_t count = 0;
        if (!single_id(numbered, corner, count)) {
            ctx.refuse(core::ErrorCode::InvalidArgument, "Tek bir köşe numarası beklenir; " +
                                                             std::to_string(count) +
                                                             " değer verildi.");
            co_return false;
        }
        co_return true;
    }

    // `pointed` was asked for above whenever `kose` was missing, which is the
    // only way here; said again so that nothing reads an empty point.
    if (!pointed) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Köşe numarası belirtilmedi. İlk köşe 1'dir.");
        co_return false;
    }
    const std::optional<std::size_t> found =
        insert ? core::nearest_edge(doc, slot, *pointed) : core::nearest_grip(doc, slot, *pointed);
    if (!found) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   insert ? "Bu nesnenin köşe eklenecek bir kenarı yok."
                          : "Bu nesnenin taşınacak bir köşesi ya da tutamağı yok.");
        co_return false;
    }
    corner = static_cast<std::int64_t>(*found) + 1;
    co_return true;
}

/// The guide the canvas draws while the new place is aimed: the object as it
/// will be, from the edit the command is about to make (`core::grip_preview`,
/// `core::insert_preview`), and the two edges following the cursor with it.
PointOptions grip_guide(const Value& object, std::int64_t corner, core::Point2 from, bool insert)
{
    const std::int64_t key = object.kind() == Value::Kind::IdList && !object.as_ids().empty()
                                 ? object.as_ids()[0]
                                 : object.as_int();
    PointOptions o;
    o.rubber_band    = true;
    o.rubber_origin  = from;
    o.rubber_shape   = RubberShape::Grip;
    o.rubber_payload = core::encode_grip_guide(core::GripGuide{
        .key = key, .index = static_cast<std::uint32_t>(corner - 1), .insert = insert});
    return o;
}

core::DrawingUnit drawing_unit(Context& ctx)
{
    return core::drawing_unit_from_setting(
        ctx.session().bus().project_settings().get("core.cizim.birim").as_enum());
}

/// Writes a grip edit into `slot`: the rings for a polyline, the rings and the
/// payload for every other kind, and a dimension's caption re-said for its new
/// number in the drawing's unit. False, having refused, when it is not taken.
bool write_grip_edit(Context& ctx, core::EntityId slot, core::GripEdit& g)
{
    const core::KindId kind = ctx.document().entities().kind[slot];
    // A dimension says a number; a moved definition point changes it, and the
    // caption is re-laid for the new text in the drawing's unit.
    std::string text;
    core::Mm height = 0;
    if (kind == core::kDimensionKind && g.caption_centre) {
        if (auto def = core::decode_dimension(g.payload)) {
            height          = ctx.document().texts().height(ctx.document().entities().slot[slot]);
            text            = core::dimension_text(def.value(), drawing_unit(ctx));
            const auto base = core::dimension_baseline(*g.caption_centre, g.caption_dir_x,
                                                       g.caption_dir_y, height, text);
            g.points[0]     = {base[0], base[1]};
        }
    }
    const auto inputs     = g.inputs();
    const core::Status st = kind == core::kPolylineKind
                                ? ctx.transaction().set_geometry(slot, inputs)
                                : ctx.transaction().set_kind_geometry(slot, inputs, g.payload);
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    if (!text.empty()) {
        if (auto set =
                ctx.transaction().set_text(slot, text, height, core::TextAnchor::MiddleCentre);
            !set) {
            ctx.refuse(set.error());
            return false;
        }
    }
    return true;
}

/// KÖŞETAŞI for every kind but the polyline: the grip table says what moving
/// grip `corner` means, and a dimension's caption is re-said afterwards.
Task<void> move_grip_of(Context& ctx, core::EntityId slot, const Value& object, std::int64_t corner)
{
    const auto grips = core::entity_grips(ctx.document(), slot);
    if (corner < 1 || static_cast<std::size_t>(corner) > grips.size()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu nesnenin " + std::to_string(corner) + ". tutamağı yok; " +
                       std::to_string(grips.size()) + " tutamağı var.");
        co_return;
    }
    const auto index        = static_cast<std::size_t>(corner - 1);
    const core::Point2 from = grips[index].at;

    auto to =
        co_await ctx.point("nokta", "Tutamağın yeni yeri", grip_guide(object, corner, from, false));
    if (!to) co_return;

    auto edit = core::move_grip(ctx.document(), slot, index, *to);
    if (!edit) {
        ctx.refuse(edit.error());
        co_return;
    }
    if (!write_grip_edit(ctx, slot, edit.value())) co_return;

    ctx.record("nesne",
               Value::ids({static_cast<std::int64_t>(core::raw(ctx.document().key_of(slot)))}));
    ctx.record("kose", Value::integer(corner));
    ctx.record("yer", Value{});
    ctx.record("nokta", Value::point(*to));
}

/// The keys an argument names, whichever shape it arrived in.
std::vector<std::int64_t> ids_of(const Value& given)
{
    if (given.kind() == Value::Kind::IdList) return given.as_ids();
    if (given.kind() == Value::Kind::Int) return {given.as_int()};
    return {};
}

/// KÖŞETAŞI AT A PLACE SEVERAL OBJECTS SHARE — the corner two parcels have in
/// common, the end where a road line meets its curve. Every object's grips
/// lying exactly at `kaynak` move to the new place together, in one step, so
/// the shared corner stays shared: moving it in one parcel alone is what opens
/// a sliver between two titles (TODOS C-07). `named` is whether the objects
/// were given by key — then each must have the corner — or taken from the
/// selection, where those without it are simply not part of it.
Task<void> run_shared(Context& ctx, std::vector<std::int64_t> ids, bool named)
{
    const core::Document& doc = ctx.document();
    const Bus& bus            = ctx.session().bus();

    struct Held
    {
        std::int64_t key{0};
        core::EntityId slot{core::kNoEntity};
        std::vector<core::GripPoint> grips;
    };

    std::vector<Held> held;
    std::size_t locked = 0;
    for (const std::int64_t id : ids) {
        const core::EntityId slot =
            id > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(id)))
                   : core::kNoEntity;
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
            co_return;
        }
        if (const auto st = doc.editable(slot); !st) {
            // Named, the object's own reason stops the edit; in a selection
            // it is passed over, counted and said.
            if (named) {
                ctx.refuse(core::Error{st.error().code,
                                       "Nesne " + std::to_string(id) + ": " + st.error().message});
                co_return;
            }
            ++locked;
            continue;
        }
        held.push_back(Held{.key = id, .slot = slot, .grips = core::entity_grips(doc, slot)});
    }
    if (held.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Seçilen nesnelerin hiçbiri düzenlenemiyor; " +
                                                         std::to_string(locked) +
                                                         " nesne kilitli katmanda.");
        co_return;
    }

    // WHERE: the place given, the numbered grip of the first object, or a click
    // — which takes the grip nearest it among all the objects.
    std::optional<core::Point2> from;
    if (const Value k = ctx.argument("kaynak"); !k.empty()) from = k.as_point();
    if (!from) {
        std::int64_t corner = 0;
        std::size_t count   = 0;
        if (const Value numbered = ctx.argument("kose");
            !numbered.empty() && single_id(numbered, corner, count)) {
            if (corner < 1 || static_cast<std::size_t>(corner) > held.front().grips.size()) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Nesne " + std::to_string(held.front().key) + "'in " +
                               std::to_string(corner) + ". köşesi ya da tutamağı yok.");
                co_return;
            }
            from = held.front().grips[static_cast<std::size_t>(corner - 1)].at;
        }
    }
    if (!from) {
        auto pointed = co_await ctx.point("yer", "Taşınacak ortak köşeye tıklayın");
        if (!pointed) co_return;
        double best = -1.0;
        for (const Held& h : held)
            for (const core::GripPoint& g : h.grips) {
                const double d = core::distance_squared(g.at, *pointed);
                if (best < 0.0 || d < best) {
                    best = d;
                    from = g.at;
                }
            }
        const auto reach = static_cast<double>(bus.aid_settings().pick_radius);
        if (!from || best > reach * reach) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Orada seçili nesnelerin bir köşesi yok. Bir köşeye tıklayın.");
            co_return;
        }
    }

    // WHICH GRIPS: every one at that exact place, in every object.
    struct Moving
    {
        const Held* of{nullptr};
        std::vector<std::size_t> indices;
    };

    std::vector<Moving> moving;
    for (const Held& h : held) {
        Moving m{.of = &h, .indices = {}};
        for (std::size_t i = 0; i < h.grips.size(); ++i)
            if (h.grips[i].at == *from) m.indices.push_back(i);
        if (m.indices.empty()) {
            if (named) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Nesne " + std::to_string(h.key) +
                               "'in bu noktada köşesi ya da tutamağı yok.");
                co_return;
            }
            continue;
        }
        moving.push_back(std::move(m));
    }
    if (moving.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu noktada seçili nesnelerin köşesi ya da tutamağı yok.");
        co_return;
    }

    // THE NEW PLACE, every object drawn as it will be while it is aimed.
    core::GripGuide guide{.key    = moving.front().of->key,
                          .index  = static_cast<std::uint32_t>(moving.front().indices.front()),
                          .insert = false,
                          .also   = {}};
    for (std::size_t k = 0; k < moving.size(); ++k)
        for (std::size_t j = k == 0 ? 1 : 0; j < moving[k].indices.size(); ++j)
            guide.also.push_back(
                core::GripGuide::More{.key   = moving[k].of->key,
                                      .index = static_cast<std::uint32_t>(moving[k].indices[j])});
    PointOptions aim;
    aim.rubber_band    = true;
    aim.rubber_origin  = *from;
    aim.rubber_shape   = RubberShape::Grip;
    aim.rubber_payload = core::encode_grip_guide(guide);
    auto to            = co_await ctx.point(
        "nokta", moving.size() > 1 ? "Ortak köşenin yeni yeri" : "Köşenin yeni yeri",
        std::move(aim));
    if (!to) co_return;

    std::vector<std::int64_t> did;
    for (const Moving& m : moving) {
        std::vector<core::GripMove> moves;
        moves.reserve(m.indices.size());
        for (const std::size_t i : m.indices)
            moves.push_back(core::GripMove{.index = i, .to = *to});
        auto edit = core::move_grips(doc, m.of->slot, moves);
        if (!edit) {
            ctx.refuse(core::Error{edit.error().code, "Nesne " + std::to_string(m.of->key) + ": " +
                                                          edit.error().message});
            co_return;
        }
        if (!write_grip_edit(ctx, m.of->slot, edit.value())) co_return;
        did.push_back(m.of->key);
    }

    ctx.record("nesne", Value::ids(did));
    ctx.record("kaynak", Value::point(*from));
    ctx.record("kose", Value{});
    ctx.record("yer", Value{});
    ctx.record("nokta", Value::point(*to));
    if (did.size() > 1 || locked > 0)
        ctx.echo(
            std::to_string(did.size()) + " nesnenin ortak köşesi taşındı" +
            (locked > 0 ? ", " + std::to_string(locked) + " nesne kilitli katmanda atlandı" : "") +
            ".");
}

Task<void> run_move(Context& ctx)
{
    // MANY OBJECTS, OR A PLACE: the shared corner. Named by key, or every
    // object of a selection of more than one.
    std::vector<std::int64_t> ids = ids_of(ctx.argument("nesne"));
    const bool named              = !ids.empty();
    if (!named) {
        const auto keys = ctx.session().bus().selection().keys();
        if (keys.size() > 1)
            for (const auto key : keys)
                ids.push_back(static_cast<std::int64_t>(core::raw(key)));
    }
    if (ids.size() > 1 || !ctx.argument("kaynak").empty()) {
        co_await run_shared(ctx, std::move(ids), named);
        co_return;
    }

    Value object;
    core::EntityId slot = core::kNoEntity;
    std::int64_t corner = 0;
    if (!co_await pick_corner(ctx, false, object, slot, corner)) co_return;

    if (ctx.document().entities().kind[slot] != core::kPolylineKind) {
        co_await move_grip_of(ctx, slot, object, corner);
        co_return;
    }

    Rings rings       = read_rings(ctx.document(), slot);
    const Where where = locate(rings, corner);
    if (!where.found) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu nesnenin " + std::to_string(corner) + ". köşesi yok; " +
                       std::to_string(rings.vertex_count()) + " köşesi var.");
        co_return;
    }

    // THE OBJECT AS IT WILL BE, not a line from the corner: the two edges that
    // meet at the corner follow the cursor, drawn from the very edit this
    // command is about to make (`core::grip_preview`).
    const core::Point2 from = rings.points[where.ring][where.at];

    auto to =
        co_await ctx.point("nokta", "Köşenin yeni yeri", grip_guide(object, corner, from, false));
    if (!to) co_return; // ESC leaves the corner where it was

    rings.points[where.ring][where.at] = *to;

    auto st = write_rings(ctx, slot, rings);
    if (!st) {
        // A ring that now crosses itself, or a hole that has escaped its exterior,
        // is refused by the geometry layer and the message names which. The bus
        // rolls the transaction back, so the corner does not half-move.
        ctx.refuse(st.error());
        co_return;
    }

    ctx.record("nesne",
               Value::ids({static_cast<std::int64_t>(core::raw(ctx.document().key_of(slot)))}));
    // The DECLARED shape, so replaying the journal line binds exactly what this
    // run bound: `kose` is an `Arity::exactly(1)` integer, so it records as one.
    // `yer` is cleared: it only ever stood in for `kose`, and a replay that had
    // both would be reading one fact twice.
    ctx.record("kose", Value::integer(corner));
    ctx.record("yer", Value{});
    ctx.record("nokta", Value::point(*to));
}

Task<void> run_insert(Context& ctx)
{
    Value object;
    core::EntityId slot = core::kNoEntity;
    std::int64_t corner = 0;
    if (!co_await pick_corner(ctx, true, object, slot, corner)) co_return;

    // Checked BEFORE the new place is asked for, with the edit the command will
    // make, so a corner that has no edge after it is refused at once rather than
    // after the user has aimed.
    const core::Document& doc = ctx.document();
    const auto after          = static_cast<std::size_t>(corner - 1);
    const auto grips          = core::entity_grips(doc, slot);
    if (corner < 1 || after >= grips.size()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu nesnenin " + std::to_string(corner) + ". köşesi yok; " +
                       std::to_string(grips.size()) + " köşesi var.");
        co_return;
    }
    if (const auto probe = core::insert_vertex(doc, slot, after, grips[after].at); !probe) {
        ctx.refuse(probe.error());
        co_return;
    }

    // The guide starts at the corner the edge leaves, and the edge itself bends
    // to the cursor (`core::insert_preview`).
    auto at = co_await ctx.point("nokta", "Yeni köşenin yeri",
                                 grip_guide(object, corner, grips[after].at, true));
    if (!at) co_return;

    auto edit = core::insert_vertex(doc, slot, after, *at);
    if (!edit) {
        ctx.refuse(edit.error());
        co_return;
    }
    const auto inputs = edit.value().inputs();
    if (auto st = ctx.transaction().set_geometry(slot, inputs); !st) {
        ctx.refuse(st.error());
        co_return;
    }

    ctx.record("nesne",
               Value::ids({static_cast<std::int64_t>(core::raw(ctx.document().key_of(slot)))}));
    // The DECLARED shape, so replaying the journal line binds exactly what this
    // run bound: `kose` is an `Arity::exactly(1)` integer, so it records as one.
    ctx.record("kose", Value::integer(corner));
    ctx.record("yer", Value{});
    ctx.record("nokta", Value::point(*at));
}

} // namespace

KENTOS_COMMAND(vertex_move)
{
    return CommandSpec{
        .id       = "core.vertex_move",
        .names    = {"KÖŞETAŞI", "KOSETASI", "MOVEVERTEX", "KT"},
        .title    = "Köşe Taşı",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::at_least(1),
                      "Köşesi taşınacak nesne; birden çok nesne verilirse ortak köşeleri "
                      "birlikte taşınır"}
                    .en("object"),
                Param::integer("kose", Arity::optional(),
                               "Taşınacak köşenin sırası; ilk köşe 1'dir. Birden çok nesnede "
                               "birincinin köşesi; verilmezse yer ya da kaynak")
                    .en("vertex"),
                Param{"yer", ParamKind::Point, Arity::optional(),
                      "Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de "
                      "verilmezse altındaki nesne"}
                    .en("at"),
                Param{"kaynak", ParamKind::Point, Arity::optional(),
                      "Ortak köşenin bugünkü yeri: verilen nesnelerin o noktadaki bütün köşe "
                      "ve tutamakları birlikte taşınır"}
                    .en("shared_point"),
                Param::point("nokta", "Köşenin yeni yeri").en("point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.",
        .run     = &run_move,
    };
}

KENTOS_COMMAND(vertex_insert)
{
    return CommandSpec{
        .id       = "core.vertex_insert",
        .names    = {"KÖŞEEKLE", "KOSEEKLE", "ADDVERTEX", "KE"},
        .title    = "Köşe Ekle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Köşe eklenecek nesnenin kimliği"}
                    .en("object"),
                Param::integer("kose", Arity::exactly(1),
                               "Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir")
                    .en("vertex"),
                Param{"yer", ParamKind::Point, Arity::optional(),
                      "Kenarı gösteren nokta: kose verilmezse en yakın kenar, nesne de "
                      "verilmezse altındaki nesne"}
                    .en("at"),
                Param::point("nokta", "Yeni köşenin yeri").en("point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir kenarın ortasına yeni köşe ekler.",
        .run     = &run_insert,
    };
}

} // namespace kentos::command
