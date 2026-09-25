// SPDX-License-Identifier: GPL-3.0-or-later
// core.explode — PATLAT, core.align — HİZALA,
// core.divide — BÖLÜMLE, core.pedit — ÇİZGİDÜZENLE.
//
// PATLAT TAKES A THING APART into the pieces it is drawn from: a run of edges
// into single edges, a face into its boundary, an arc polyline into its lines
// and arcs, a block reference into its members placed where they stand. It is
// what a drafter reaches for when the grouping is in the way — one corner of a
// parcel has to move and the parcel is a face, one leg of a fence has to go and
// the fence is one run.
//
// A MEMBER COMES OUT IN ITS OWN KIND (TODOS C-13). Before this a definition
// holding a circle, an arc or a caption was refused outright, because placing
// a circle's drawn outline would have made a 128-gon whose area is not πr² —
// the number a tapu reads (§12). Each member is now made again as what it is
// and carried by the reference's own placement (`core::block_placement`), the
// arithmetic that draws it: a circle stays a circle, a caption a caption, and
// only what no kind can hold — an arc bent into an ellipse, an inner block
// leaned — is named and refused.
//
// HİZALA IS NOT OTURT. `OTURT` is a least-squares Helmert fit over many common
// points and belongs to geodesy; this is the drafting verb — one or two point
// pairs, move and turn (and scale when asked), no adjustment. Each page names
// the other, because the two are easy to confuse and picking the wrong one
// silently changes what a drawing claims.
//
// BÖLÜMLE PUTS MARKS ALONG SOMETHING THAT EXISTS. `ARANOKTA` divides a line
// given by two points; this divides an OBJECT — a surveyed kerb, a road centre
// line — which is the form a station peg list is actually asked for.
//
// ÇİZGİDÜZENLE is the small edits a run needs and nothing else does: close it,
// open it, reverse it, thin it out.
#include "kentos_cad/command/block_edit.hpp"
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"
#include "kentos_cad/command/transform_edit.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/transform.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One ring of an entity, as points.
std::vector<core::Point2> ring_points(const core::Document& doc, std::uint32_t ring)
{
    const auto xs = doc.geometry().ring_xs(ring);
    const auto ys = doc.geometry().ring_ys(ring);
    std::vector<core::Point2> out;
    out.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.push_back(core::Point2{xs[v], ys[v]});
    return out;
}

/// The name of a kind, for a refusal that says what it found.
const char* kind_word(core::KindId k)
{
    switch (k) {
    case core::kPolylineKind: return "çizgi";
    case core::kCircleKind: return "daire";
    case core::kArcKind: return "yay";
    case core::kPointKind: return "nokta";
    case core::kEllipseKind: return "elips";
    case core::kArcPolylineKind: return "yaylı çizgi";
    case core::kSplineKind: return "spline";
    case core::kHatchKind: return "tarama";
    case core::kBlockReferenceKind: return "blok referansı";
    case core::kDimensionKind: return "ölçü";
    case core::kLeaderKind: return "kılavuz çizgi";
    default: return "bilinmeyen tür";
    }
}

// ----------------------------------------------------------------- PATLAT ----

/// What one exploded object came apart into, for the echo and the report.
struct Pieces
{
    std::vector<core::EntityId> made;        ///< every piece, in the order made
    std::map<std::string, std::size_t> kind; ///< how many of each kind
    std::size_t copies{1};                   ///< how many copies of a grid
    std::size_t onto_reference{0};           ///< pieces set down on the reference's layer
    std::size_t reference_look{0};           ///< pieces given the reference's look
    std::size_t hidden{0};                   ///< pieces of hidden members, kept hidden
    std::size_t values_written{0};           ///< captions written with the reference's values
    std::size_t values_left{0};              ///< the reference's own attribute cells, not carried
    std::string block;                       ///< the definition's name
};

/// The pieces' keys, for the report.
core::Json keys_of(const core::Document& doc, const std::vector<core::EntityId>& made)
{
    core::Json out = core::Json::array({});
    for (const core::EntityId e : made)
        out.push(core::Json::integer(static_cast<std::int64_t>(core::raw(doc.key_of(e)))));
    return out;
}

/// The attribute cells reference `e` carries that none of its block's fields
/// names (`core::block_fields`): the values a piece has nowhere to show.
std::size_t unshown_cells(const core::Document& doc, core::EntityId e, core::BlockId block)
{
    const std::vector<std::string> fields = core::block_fields(doc, block);
    std::size_t left                      = 0;
    for (std::size_t c = 0; c < doc.attributes().columns(); ++c) {
        const core::AttrColumn* col = doc.attributes().column(static_cast<core::AttrId>(c));
        if (col == nullptr) continue;
        auto held = doc.attribute(static_cast<core::AttrId>(c), e);
        if (!held || !held.value().present) continue;
        if (std::find(fields.begin(), fields.end(), col->spec().id) == fields.end()) ++left;
    }
    return left;
}

/// A BLOCK REFERENCE TAKEN APART: every member, of every copy of its grid, made
/// again in its own kind — a circle a circle, a caption a caption, a block
/// inside the block a reference — and carried by the placement the reference
/// draws it with (`core::block_placement`), so each piece lands on the
/// millimetre it was drawn on, with the layer and the look the drawing gave it
/// (`place_member`, the rule BLOKDÜZENLE shares).
///
/// WHAT CANNOT BE WRITTEN IS NAMED, not approximated: under a placement that
/// differs across and up, an arc polyline's arcs and a turned inner block would
/// lean, and neither kind can hold that (`transform.cpp` says which).
bool explode_reference(Context& ctx, core::EntityId slot, Pieces& out)
{
    const core::Document& doc = ctx.document();
    const std::uint32_t gslot = doc.entities().slot[slot];
    auto ref                  = core::block_reference_of(doc.geometry(), gslot);
    if (!ref) {
        ctx.refuse(ref.error());
        return false;
    }
    const core::BlockReference placed = ref.value();
    if (placed.block >= doc.blocks().size()) {
        ctx.refuse(core::ErrorCode::NotFound, "Blok tanımı bulunamadı.");
        return false;
    }
    // Copied, not referred to: the pieces are added to the document under it.
    const std::vector<core::EntityKey> members = doc.blocks().at(placed.block).members;
    const core::Point2 base                    = doc.blocks().at(placed.block).base;
    out.block                                  = doc.blocks().at(placed.block).name;
    const core::Point2 insertion = core::block_reference_insertion(doc.geometry(), gslot);
    out.copies                   = static_cast<std::size_t>(placed.rows) * placed.columns;
    out.values_left              = unshown_cells(doc, slot, placed.block);

    for (int row = 0; row < static_cast<int>(placed.rows); ++row) {
        for (int column = 0; column < static_cast<int>(placed.columns); ++column) {
            const core::Xform x = core::block_placement(placed, insertion, base, column, row);
            for (const core::EntityKey key : members) {
                const core::EntityId member = doc.slot_of(key);
                if (member == core::kNoEntity || !doc.alive(member)) continue;
                const core::KindId kind = doc.entities().kind[member];
                auto made               = place_member(ctx, member, x, slot);
                if (!made) {
                    ctx.refuse(made.error().code, "Blok '" + out.block + "' içindeki bir " +
                                                      kind_word(kind) +
                                                      " yerine konamadı: " + made.error().message);
                    return false;
                }
                if (made.value().onto_reference) ++out.onto_reference;
                if (made.value().reference_look) ++out.reference_look;
                if (made.value().hidden) ++out.hidden;
                if (made.value().filled) ++out.values_written;
                out.made.push_back(made.value().piece);
                ++out.kind[kind_word(kind)];
            }
        }
    }
    return true;
}

/// A RUN OF EDGES INTO SINGLE EDGES. A face's boundary becomes an open run
/// per ring, which is what "take the face apart" means: a face is a closed
/// thing and its pieces are not. Each edge keeps the run's look.
bool explode_polyline(Context& ctx, core::EntityId slot, Pieces& out)
{
    const core::Document& doc = ctx.document();
    const core::LayerId home  = doc.entities().layer[slot];
    const core::StyleId look  = doc.entities().style[slot];
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        std::vector<core::Point2> pts = ring_points(doc, r);
        const bool closed             = doc.geometry().ring_role[r] != core::RingRole::Open;
        if (closed && pts.size() >= 3) pts.push_back(pts.front());
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            const core::Point2 pair[2]{pts[i], pts[i + 1]};
            auto one = ctx.transaction().add_polyline(home, pair);
            if (!one) {
                ctx.refuse(one.error());
                return false;
            }
            if (look != core::kByLayerStyle)
                if (auto st = ctx.transaction().set_entity_style(one.value(), look); !st) {
                    ctx.refuse(st.error());
                    return false;
                }
            out.made.push_back(one.value());
            ++out.kind[kind_word(core::kPolylineKind)];
        }
    }
    return true;
}

/// AN ARC POLYLINE INTO ITS EDGES: a straight edge a line, a bent one the arc
/// it is — its centre, its radius and its sweep as the run held them, never
/// its drawn chords. What `ÖLÇEKLE` tells a user to do before a stretch the
/// arcs cannot take ("önce PATLAT ile kenarlarına ayırın"), and what it now
/// does. A constant width does not go with the pieces: a line has none.
bool explode_arc_polyline(Context& ctx, core::EntityId slot, Pieces& out)
{
    const core::Document& doc = ctx.document();
    const std::uint32_t gslot = doc.entities().slot[slot];
    auto def                  = core::arc_polyline_of(doc.geometry(), gslot);
    if (!def) {
        ctx.refuse(def.error());
        return false;
    }
    const core::LayerId home  = doc.entities().layer[slot];
    const core::StyleId look  = doc.entities().style[slot];
    const core::RingSpan span = doc.geometry().rings_of(gslot);
    if (span.count == 0) return true;
    const std::vector<core::Point2> pts = ring_points(doc, span.first);
    const bool closed       = doc.geometry().ring_role[span.first] != core::RingRole::Open;
    const std::size_t n     = pts.size();
    const std::size_t edges = n < 2 ? 0 : (closed ? n : n - 1);
    std::size_t next        = 0;
    for (std::size_t s = 0; s < edges; ++s) {
        const core::Point2 a = pts[s];
        const core::Point2 b = pts[(s + 1) % n];
        while (next < def.value().arcs.size() && def.value().arcs[next].segment < s)
            ++next;
        const core::ArcPolyline::Arc* arc =
            next < def.value().arcs.size() && def.value().arcs[next].segment == s
                ? &def.value().arcs[next]
                : nullptr;
        core::Result<core::EntityId> one = core::err(core::ErrorCode::Internal, "");
        if (arc != nullptr) {
            // `core.arc` sweeps counter-clockwise from its start to its end, so
            // a clockwise edge is written from its far end.
            const core::Point2 ring[4]{arc->centre,
                                       core::Point2{arc->centre.x + arc->radius, arc->centre.y},
                                       arc->ccw ? a : b, arc->ccw ? b : a};
            const core::RingGeometry::RingInput input{std::span<const core::Point2>(ring, 4),
                                                      core::RingRole::Open, 0};
            one = ctx.transaction().add_kind(home, core::kArcKind, {&input, 1}, {});
        } else {
            const core::Point2 pair[2]{a, b};
            one = ctx.transaction().add_polyline(home, pair);
        }
        if (!one) {
            ctx.refuse(one.error());
            return false;
        }
        if (look != core::kByLayerStyle)
            if (auto st = ctx.transaction().set_entity_style(one.value(), look); !st) {
                ctx.refuse(st.error());
                return false;
            }
        out.made.push_back(one.value());
        ++out.kind[kind_word(arc != nullptr ? core::kArcKind : core::kPolylineKind)];
    }
    return true;
}

/// "3 çizgi, 1 daire" — the pieces by kind, in the order the map keeps.
std::string kinds_text(const std::map<std::string, std::size_t>& kinds)
{
    std::string out;
    for (const auto& [word, count] : kinds) {
        if (!out.empty()) out += ", ";
        out += std::to_string(count) + " " + word;
    }
    return out;
}

Task<void> run_explode(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Patlatılacak nesneleri seçin, Enter'a basın", chosen,
                               0, "PATLAT nesneler=1"))
        co_return;
    if (chosen.empty()) co_return;

    const core::Document& doc = ctx.document();
    std::size_t made          = 0;
    std::size_t gone          = 0;
    core::Json rows           = core::Json::array({});

    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }

        const core::KindId kind = doc.entities().kind[slot];
        Pieces pieces;
        bool taken = false;
        if (kind == core::kBlockReferenceKind)
            taken = explode_reference(ctx, slot, pieces);
        else if (kind == core::kPolylineKind)
            taken = explode_polyline(ctx, slot, pieces);
        else if (kind == core::kArcPolylineKind)
            taken = explode_arc_polyline(ctx, slot, pieces);
        else {
            ctx.refuse(core::ErrorCode::Unsupported,
                       std::string("Nesne ") + std::to_string(id) + " bir " + kind_word(kind) +
                           "; PATLAT çizgileri, alanları, yaylı çoklu çizgileri ve blok "
                           "referanslarını patlatır. Bir daire, yay ya da yazı zaten tek "
                           "parçadır.");
            co_return;
        }
        if (!taken) co_return;

        auto st = ctx.transaction().erase_entity(slot);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++gone;
        made += pieces.made.size();

        // WHAT IT CAME APART INTO, said and reported: the pieces by kind, and
        // for a block what the drawing gave them and what stayed behind.
        std::string said = "Nesne " + std::to_string(id) + " (" + kind_word(kind);
        if (!pieces.block.empty()) said += " '" + pieces.block + "'";
        said += ") → " + (pieces.made.empty() ? std::string("parça yok") : kinds_text(pieces.kind));
        if (pieces.copies > 1) said += "; " + std::to_string(pieces.copies) + " kopya";
        if (pieces.onto_reference > 0)
            said += "; " + std::to_string(pieces.onto_reference) + " parça 0 katmanından " +
                    "referansın katmanına";
        if (pieces.reference_look > 0)
            said += "; " + std::to_string(pieces.reference_look) + " parça referansın görünüşünde";
        if (pieces.hidden > 0) said += "; " + std::to_string(pieces.hidden) + " gizli parça";
        if (pieces.values_written > 0)
            said += "; " + std::to_string(pieces.values_written) +
                    " yazıya referansın öznitelik değeri işlendi";
        if (pieces.values_left > 0)
            said += "; referansın hiçbir yazıda görünmeyen " + std::to_string(pieces.values_left) +
                    " öznitelik değeri parçalara geçmedi";
        ctx.echo(said + ".");

        core::Json row;
        row.set("nesne", core::Json::integer(id));
        row.set("tur", core::Json::string(kind_word(kind)));
        if (!pieces.block.empty()) {
            row.set("blok", core::Json::string(pieces.block));
            row.set("kopya", core::Json::integer(static_cast<std::int64_t>(pieces.copies)));
            row.set("katman_devri",
                    core::Json::integer(static_cast<std::int64_t>(pieces.onto_reference)));
            row.set("gorunus_devri",
                    core::Json::integer(static_cast<std::int64_t>(pieces.reference_look)));
            row.set("gizli", core::Json::integer(static_cast<std::int64_t>(pieces.hidden)));
            row.set("yazilan_deger",
                    core::Json::integer(static_cast<std::int64_t>(pieces.values_written)));
            row.set("birakilan_oznitelik",
                    core::Json::integer(static_cast<std::int64_t>(pieces.values_left)));
        }
        core::Json kinds = core::Json::object({});
        for (const auto& [word, count] : pieces.kind)
            kinds.set(word, core::Json::integer(static_cast<std::int64_t>(count)));
        row.set("turler", std::move(kinds));
        row.set("parcalar", keys_of(doc, pieces.made));
        rows.push(std::move(row));
    }

    ctx.record("nesne", Value::ids(chosen));
    core::Json report;
    report.set("patlatilan", core::Json::integer(static_cast<std::int64_t>(gone)));
    report.set("parca", core::Json::integer(static_cast<std::int64_t>(made)));
    report.set("nesneler", std::move(rows));
    ctx.report(std::move(report));
    ctx.echo(std::to_string(gone) + " nesne patlatıldı, " + std::to_string(made) + " parça çıktı.");
}

// ----------------------------------------------------------------- HİZALA ----

Task<void> run_align(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Hizalanacak nesneleri seçin, Enter'a basın", chosen,
                               0, "HİZALA nesneler=1 kaynak=0,0 hedef=10,10"))
        co_return;
    if (chosen.empty()) co_return;

    // THE OBJECTS RIDE UNDER THE CURSOR at every step, drawn by the transform
    // this body applies (`core::ghost_xform`, `core::align_xform`): first carried
    // by the move of the first pair, then turned — and, asked to, scaled — by
    // the second. Before this the targets were aimed along a bare line and the
    // turn was seen only after it had happened.
    auto from1 = co_await ctx.point("kaynak", "Birinci kaynak nokta");
    if (!from1) co_return;
    auto to1 = co_await ctx.point(
        "hedef", "Birinci kaynağın gideceği yer",
        PointOptions{.rubber_band    = true,
                     .rubber_origin  = *from1,
                     .rubber_shape   = RubberShape::Ghost,
                     .rubber_payload = core::encode_ghost_spec(core::GhostSpec{.keys = chosen})});
    if (!to1) co_return;

    bool scaling = false;
    if (const Value v = ctx.argument("olcekle"); !v.empty()) scaling = v.as_bool();

    // THE SECOND PAIR IS OPTIONAL AND IT IS WHAT ADDS THE TURN. One pair is a
    // move; two are a move and a rotation, and a rotation needs two. ASKED, so
    // the turn is not a script's alone: Enter at the second source keeps the
    // move and nothing else, the way every CAD's ALIGN reads it.
    std::optional<core::Point2> from2;
    std::optional<core::Point2> to2;
    if (const Value v = ctx.argument("kaynak2"); !v.empty() && !v.as_points().empty())
        from2 = v.as_points().front();
    else
        from2 = co_await ctx.point("kaynak2", "İkinci kaynak nokta — Enter: yalnız taşı",
                                   PointOptions{.rubber_band   = true,
                                                .rubber_origin = *to1,
                                                .rubber_base   = false,
                                                .rubber_shape  = RubberShape::Fixed,
                                                .rubber_chain  = {*from1, *to1}});
    if (from2) {
        if (const Value v = ctx.argument("hedef2"); !v.empty() && !v.as_points().empty())
            to2 = v.as_points().front();
        else
            to2 =
                co_await ctx.point("hedef2", "İkinci kaynağın gideceği doğrultu",
                                   PointOptions{.rubber_band    = true,
                                                .rubber_origin  = *to1,
                                                .rubber_shape   = RubberShape::Ghost,
                                                .rubber_payload = core::encode_ghost_spec(
                                                    core::GhostSpec{.kind  = core::GhostKind::Align,
                                                                    .keys  = chosen,
                                                                    .from1 = *from1,
                                                                    .to1   = *to1,
                                                                    .from2 = *from2,
                                                                    .scale = scaling})});
    }
    const bool turning = from2 && to2;

    // A THIRD PAIR SAYS WHICH SIDE (TODOS C-08). Two pairs fix a move, a turn
    // and a scale; in the plane a third decides whether the objects are also
    // turned over — when the source triangle runs the other way round from the
    // target one, they are reflected, so the third point lands on the side of
    // the line its target is on. Only given, never asked: two pairs are how
    // every CAD aligns in the plane.
    bool flip = false;
    std::optional<core::Point2> from3;
    std::optional<core::Point2> to3;
    if (const Value v = ctx.argument("kaynak3"); turning && !v.empty() && !v.as_points().empty())
        from3 = v.as_points().front();
    if (const Value v = ctx.argument("hedef3"); from3 && !v.empty() && !v.as_points().empty())
        to3 = v.as_points().front();
    if (from3 && !to3) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                     "Üçüncü kaynağın gideceği yer verilmedi: hedef3=<nokta>."));
        co_return;
    }
    if (from2 && to2 && from3 && to3) {
        const auto side = [](core::Point2 a, core::Point2 b, core::Point2 c) {
            const double t = static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y) -
                             static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
            return (t > 0.0) - (t < 0.0);
        };
        const int was     = side(*from1, *from2, *from3);
        const int becomes = side(*to1, *to2, *to3);
        if (was == 0 || becomes == 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Üçüncü nokta ilk ikisiyle aynı doğru üzerinde; hangi "
                                         "yana düştüğü okunamıyor."));
            co_return;
        }
        flip = was != becomes;
    }

    // ONE TRANSFORM, the one the ghost drew: `p -> to1 + factor · turn(p - from1)`.
    core::Xform x = core::ghost_xform(core::GhostKind::Translate, *from1, *to1);
    if (turning) {
        auto aligned = core::align_xform(*from1, *to1, *from2, *to2, scaling, flip);
        if (!aligned) {
            ctx.session().fail(core::err(
                core::ErrorCode::InvalidArgument,
                "İki kaynak ya da iki hedef nokta aynı; doğrultu ve ölçek hesaplanamaz."));
            co_return;
        }
        x = *aligned;
    }
    const double factor = turning ? x.factor : 1.0;

    const core::Document& doc = ctx.document();
    std::size_t moved         = 0;
    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        // EVERY KIND, BY THE ONE TRANSFORM EVERY VERB USES: a circle stays a
        // circle of the scaled radius, a caption turns and grows, a block
        // keeps its symbol, a dimension says its new figure. It aligned lines
        // and faces only (TODOS C-08).
        if (!transform_entity(ctx, slot, x)) co_return;
        ++moved;
    }

    ctx.record("nesne", Value::ids(chosen));
    if (turning) {
        ctx.record("kaynak2", Value::point(*from2));
        ctx.record("hedef2", Value::point(*to2));
    }
    if (from3 && to3) {
        ctx.record("kaynak3", Value::point(*from3));
        ctx.record("hedef3", Value::point(*to3));
    }
    std::string how = " (taşındı).";
    if (turning)
        how = factor == 1.0 ? " (taşındı ve döndürüldü" : " (taşındı, döndürüldü ve ölçeklendi";
    if (turning) how += flip ? "; üçüncü nokta öbür yana düştüğü için ters çevrildi)." : ").";
    ctx.echo(std::to_string(moved) + " nesne hizalandı" + how);
}

// --------------------------------------------------- BÖLÜMLE / İŞARETLE ----

Task<void> run_divide(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Bölünecek nesneyi seçin, Enter'a basın", chosen, 1,
                               "BÖLÜMLE nesne=1 sayi=4"))
        co_return;
    if (chosen.size() != 1) {
        ctx.session().fail(
            core::err(core::ErrorCode::InvalidArgument, "BÖLÜMLE tek bir nesneyle çalışır."));
        co_return;
    }

    const core::Document& doc = ctx.document();
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(chosen.front()));
    const core::EntityId slot = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.session().fail(core::err(core::ErrorCode::NotFound, "Nesne bulunamadı."));
        co_return;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.session().fail(
            core::err(core::ErrorCode::Unsupported,
                      std::string("Nesne bir ") + kind_word(doc.entities().kind[slot]) +
                          "; BÖLÜMLE bu sürümde çizgileri ve alan sınırlarını böler."));
        co_return;
    }

    const core::RingSpan span     = doc.geometry().rings_of(doc.entities().slot[slot]);
    std::vector<core::Point2> pts = ring_points(doc, span.first);
    if (doc.geometry().ring_role[span.first] != core::RingRole::Open && pts.size() >= 3)
        pts.push_back(pts.front());
    if (pts.size() < 2) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument, "Nesnenin iki köşesi yok."));
        co_return;
    }

    // The cumulative length at each vertex, so a station can be found by reading
    // along rather than by walking the run again per mark.
    std::vector<double> at;
    at.push_back(0.0);
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        at.push_back(at.back() + core::mm_to_metres(core::segment_length(pts[i], pts[i + 1])));
    const double total = at.back();
    if (total <= 0.0) {
        ctx.session().fail(core::err(core::ErrorCode::InvalidArgument, "Nesnenin uzunluğu sıfır."));
        co_return;
    }

    // EITHER A COUNT OR A SPACING, AND EXACTLY ONE. `sayi` cuts into that many
    // equal parts and marks the joins; `aralik` walks a fixed distance from the
    // start, which is what a chainage list is.
    Value count         = ctx.argument("sayi");
    const Value spacing = ctx.argument("aralik");

    // NEITHER GIVEN MEANS ASK, and `sayi` is the one asked for.
    //
    // It read both from arguments only, so pressing BÖLÜMLE in the tool column
    // asked which object and then REFUSED, telling the user to type `sayi=` — a
    // command reachable by mouse that cannot be finished by one (CLAUDE.md 5.15).
    // `sayi` is this command's own meaning ("into how many equal parts"); the
    // fixed interval is what the plan's İŞARETLE would have been and stays the
    // typed and scripted road, because one number cannot say which of the two it
    // is. The emptiness of the argument decides, never `InputSource`.
    if (count.empty() && spacing.empty()) {
        auto asked = co_await ctx.integer(
            "sayi", "Kaç eşit parçaya bölünecek (sabit aralık için aralik=<m> yazın). Uzunluk " +
                        metres_text(core::mm_from_metres(total)) + " m");
        if (!asked) co_return;
        count = Value::integer(*asked);
    }

    if (count.empty() == spacing.empty()) {
        ctx.session().fail(core::err(
            core::ErrorCode::InvalidArgument,
            "Tam olarak birini verin: sayi= (kaç eşit parça) ya da aralik= (sabit aralık, m). "
            "Nesnenin uzunluğu " +
                std::to_string(total) + " m."));
        co_return;
    }

    std::vector<double> stations;
    if (!count.empty()) {
        const std::int64_t parts = count.as_int();
        for (std::int64_t i = 1; i < parts; ++i)
            stations.push_back(total * static_cast<double>(i) / static_cast<double>(parts));
    } else {
        const double step = spacing.as_number();
        if (step <= 0.0) {
            ctx.session().fail(
                core::err(core::ErrorCode::InvalidArgument, "Aralık sıfır ya da eksi olamaz."));
            co_return;
        }
        for (double s = step; s < total; s += step)
            stations.push_back(s);
    }

    // A POINT, OR A BLOCK AT EVERY STATION. A row of manholes, poles, trees or
    // kerb markers is what a chainage list is FOR, and placing them one INSERT at
    // a time is the work this command exists to remove. The definition has to
    // exist already — minting one here would be `BLOK`'s job done twice (5.10).
    core::BlockId definition = core::kNoBlock;
    if (const Value named = ctx.argument("blok"); !named.empty()) {
        definition = doc.blocks().find(named.as_text());
        if (definition == core::kNoBlock) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "'" + named.as_text() +
                                             "' adlı blok yok. BLOK ile tanımlayın, sonra "
                                             "BÖLÜMLE blok=" +
                                             named.as_text() + " ile dizin."));
            co_return;
        }
    }

    // ALIGNED MEANS TURNED TO THE SEGMENT IT SITS ON, which is what a kerb
    // marker or an arrow wants and what a manhole cover does not care about.
    // Default off, because a block drawn upright stays upright unless asked.
    bool aligned = false;
    if (const Value v = ctx.argument("hizala"); !v.empty()) aligned = v.as_bool();

    std::size_t placed = 0;
    for (const double station : stations) {
        // Which segment the station falls on, and where along it.
        std::size_t i = 0;
        while (i + 2 < at.size() && at[i + 1] < station)
            ++i;
        const double span_m = at[i + 1] - at[i];
        const double along  = span_m > 0.0 ? (station - at[i]) / span_m : 0.0;
        const double dx     = static_cast<double>(pts[i + 1].x - pts[i].x);
        const double dy     = static_cast<double>(pts[i + 1].y - pts[i].y);
        const core::Point2 mark{pts[i].x + core::mm_round(along * dx),
                                pts[i].y + core::mm_round(along * dy)};

        if (definition == core::kNoBlock) {
            auto one = ctx.transaction().add_point(ctx.active_layer(), mark);
            if (!one) {
                ctx.session().fail(one.error());
                co_return;
            }
            ++placed;
            continue;
        }

        core::BlockReference ref;
        ref.block = definition;
        // THE SEGMENT'S OWN DIRECTION, from `atan2_udeg` and not from libm: a
        // reference's rotation is stored in micro-degrees counter-clockwise, which
        // is exactly what that function answers (§7.3).
        if (aligned)
            ref.rotation_udeg = core::atan2_udeg(pts[i + 1].y - pts[i].y, pts[i + 1].x - pts[i].x);
        ref.bounds = core::block_reference_bounds(doc, mark, ref);

        const core::Point2 one_point[1]{mark};
        const core::RingGeometry::RingInput ring{std::span<const core::Point2>(one_point, 1),
                                                 core::RingRole::Open, 0};
        const std::vector<std::uint8_t> payload = core::encode_block_reference(ref);
        auto made                               = ctx.transaction().add_kind(
            ctx.active_layer(), core::kBlockReferenceKind,
            std::span<const core::RingGeometry::RingInput>(&ring, 1), payload);
        if (!made) {
            ctx.session().fail(made.error());
            co_return;
        }
        ++placed;
    }

    ctx.record("nesne", Value::ids({chosen.front()}));
    ctx.echo(std::to_string(placed) + (definition == core::kNoBlock ? " işaret" : " blok") +
             " yerleştirildi (uzunluk " + std::to_string(total) + " m).");
}

// --------------------------------------------------------- ÇİZGİDÜZENLE ----

Task<void> run_pedit(Context& ctx)
{
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Düzenlenecek çizgileri seçin, Enter'a basın", chosen,
                               0, "ÇİZGİDÜZENLE nesneler=1 islem=kapat"))
        co_return;
    if (chosen.empty()) co_return;

    auto verb = co_await ctx.text("islem", "İşlem: kapat / ac / ters / sadelestir",
                                  {"kapat", "ac", "ters", "sadelestir"});
    if (!verb) co_return;
    const auto is = [&verb](const char* word) { return core::turkish_key_equals(*verb, word); };

    // THE WORD IS CHECKED BEFORE ANYTHING IS EDITED. An unknown or empty one fell
    // through to the last branch — simplifying at a tolerance of nothing — and
    // the command reported "2 çizgi düzenlendi ()" before the bus's own check of
    // the resolved word refused it and rolled the edit back: a success sentence
    // and a refusal for one press, and the success was false.
    if (!is("kapat") && !is("ac") && !is("ters") && !is("sadelestir")) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Tanınmayan işlem: '" + *verb + "'. İşlemler: kapat / ac / ters / sadelestir");
        co_return;
    }

    core::Mm tolerance = 0;
    if (is("sadelestir")) {
        double given = 0.0;
        if (const Value v = ctx.argument("tolerans"); !v.empty())
            given = v.as_number();
        else {
            auto asked =
                co_await ctx.number("tolerans", "Sadeleştirme toleransı (m); bundan yakın köşeler "
                                                "atılır");
            if (!asked) co_return;
            given = *asked;
        }
        tolerance = core::mm_from_metres(given);
        if (tolerance <= 0) {
            ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                         "Sadeleştirme toleransı sıfırdan büyük olmalı."));
            co_return;
        }
    }

    const core::Document& doc = ctx.document();
    std::size_t touched       = 0;
    std::size_t dropped       = 0;

    for (const std::int64_t id : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.session().fail(core::err(core::ErrorCode::NotFound,
                                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id)));
            co_return;
        }
        if (auto st = doc.editable(slot); !st) {
            ctx.session().fail(st.error());
            co_return;
        }
        // A POLYLINE WHOSE EDGES BEND (TODOS C-05) is closed, opened and turned
        // round as the path it is, so every arc stays its arc: closing adds a
        // straight closing edge, opening removes the closing edge, and turning
        // it round walks each arc the other way. Thinning its vertices would
        // take the ends out from under its arcs, so that is refused by name.
        if (doc.entities().kind[slot] == core::kArcPolylineKind) {
            auto path = core::path_of(doc, slot);
            if (!path) {
                ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                             "Nesne " + std::to_string(id) +
                                                 " okunamadı; yaylı çoklu çizginin yayları "
                                                 "köşelerine uymuyor."));
                co_return;
            }
            core::CurvePath edited = *path;
            if (is("ters")) {
                edited = core::reversed(*path);
            } else if (is("kapat")) {
                if (!edited.closed) {
                    const core::Point2 end   = edited.pieces.back().to;
                    const core::Point2 start = edited.pieces.front().from;
                    if (end != start)
                        edited.pieces.push_back(core::PathPiece{.from = end, .to = start});
                    edited.closed = true;
                }
            } else if (is("ac")) {
                if (edited.closed) {
                    edited.pieces.pop_back(); ///< the closing edge, bent or straight
                    edited.closed = false;
                }
            } else {
                ctx.session().fail(core::err(
                    core::ErrorCode::Unsupported,
                    "Nesne " + std::to_string(id) +
                        " yaylı bir çoklu çizgi; sadeleştirmek yaylarının uçlarını atardı. "
                        "Yayları korumak için sadeleştirmeyin; gerekirse önce PATLAT ile ayırın."));
                co_return;
            }
            if (edited.pieces.empty() || !write_path(ctx, slot, edited)) {
                if (edited.pieces.empty())
                    ctx.session().fail(core::err(core::ErrorCode::InvalidArgument,
                                                 "Açılan şekilde kenar kalmıyor."));
                co_return;
            }
            ++touched;
            continue;
        }
        if (doc.entities().kind[slot] != core::kPolylineKind) {
            ctx.session().fail(
                core::err(core::ErrorCode::Unsupported,
                          std::string("Nesne ") + std::to_string(id) + " bir " +
                              kind_word(doc.entities().kind[slot]) +
                              "; ÇİZGİDÜZENLE çizgilerle, yaylı çoklu çizgilerle ve alanlarla "
                              "çalışır."));
            co_return;
        }

        const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
        std::vector<std::vector<core::Point2>> store;
        std::vector<core::RingGeometry::RingInput> rings;

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            std::vector<core::Point2> pts = ring_points(doc, r);
            core::RingRole role           = doc.geometry().ring_role[r];

            if (is("ters")) {
                std::reverse(pts.begin(), pts.end());
            } else if (is("kapat")) {
                role = r == span.first ? core::RingRole::Exterior : core::RingRole::Interior;
            } else if (is("ac")) {
                role = core::RingRole::Open;
            } else {
                // SADELEŞTİR: a vertex within the tolerance of the simplified
                // line carries no information about the shape.
                //
                // CLIPPER2 DOES IT, not a loop here. This used to walk the run
                // comparing each vertex against the line from the last KEPT one
                // to the next — which is a perpendicular-distance filter and not
                // what simplifying means: whether a vertex survives then depends
                // on which of its neighbours happened to survive before it, so
                // the same shape thinned differently depending on where the walk
                // started. Clipper2 has solved this and every degenerate case
                // around it, and CLAUDE.md 5.16 says a solved problem is not
                // re-solved here. The plan named `SimplifyPath` by name.
                const std::size_t was = pts.size();
                pts = core::simplify_ring(pts, tolerance, role != core::RingRole::Open);
                dropped += was - pts.size();
            }

            store.push_back(std::move(pts));
            rings.push_back(core::RingGeometry::RingInput{{}, role, doc.geometry().ring_part[r]});
        }
        for (std::size_t i = 0; i < rings.size(); ++i)
            rings[i].points = store[i];

        auto st = ctx.transaction().set_geometry(slot, rings);
        if (!st) {
            ctx.session().fail(st.error());
            co_return;
        }
        ++touched;
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.record("islem", Value::text(*verb));
    ctx.echo(std::to_string(touched) + " çizgi düzenlendi (" + *verb + ")" +
             (is("sadelestir") ? ", " + std::to_string(dropped) + " köşe atıldı." : "."));
}

} // namespace

KENTOS_COMMAND(explode)
{
    return CommandSpec{
        .id       = "core.explode",
        .names    = {"PATLAT", "EXPLODE", "PTL"},
        .title    = "Patlat",
        .category = Category::Modify,
        .params   = {Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                         "Patlatılacak nesneler"}
                         .en("object")},
        .undo     = UndoPolicy::SingleTransaction,
        .flags    = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiyi tek tek kenarlara, alanı sınırına, yaylı çoklu çizgiyi çizgi ve "
                   "yaylarına, blok referansını kendi türündeki bileşenlerine ayırır.",
        .run    = &run_explode,
        .effect = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(align)
{
    return CommandSpec{
        .id       = "core.align",
        .names    = {"HİZALA", "HIZALA", "ALIGN", "HZL"},
        .title    = "Hizala",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Hizalanacak nesneler"}
                    .en("object"),
                Param::point("kaynak", "Birinci kaynak nokta").en("source"),
                Param::point("hedef", "Birinci kaynağın gideceği yer").en("target"),
                Param::points("kaynak2", Arity::optional(),
                              "İkinci kaynak nokta; verilirse döndürme de yapılır")
                    .en("source2"),
                Param::points("hedef2", Arity::optional(), "İkinci kaynağın gideceği yer")
                    .en("target2"),
                Param::boolean("olcekle", Arity::optional(),
                               "İki çiftin uzunluk oranıyla ölçekler de")
                    .en("scale"),
                Param::points("kaynak3", Arity::optional(),
                              "Üçüncü kaynak nokta: hedefi ilk iki hedefin öbür yanındaysa "
                              "nesneler ters çevrilir")
                    .en("source3"),
                Param::points("hedef3", Arity::optional(), "Üçüncü kaynağın gideceği yan")
                    .en("target3"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse "
                   "ölçekler.",
        .run     = &run_align,
        .effect  = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(divide)
{
    return CommandSpec{
        .id       = "core.divide",
        .names    = {"BÖLÜMLE", "BOLUMLE", "DIVIDE", "BLM"},
        .title    = "Bölümle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Bölünecek nesne"}.en(
                    "object"),
                Param::integer_range("sayi", Arity::optional(), 2, 10000,
                                     "Kaç eşit parçaya bölünecek")
                    .en("count"),
                Param::number("aralik", Arity::optional(),
                              "Sabit aralık (m); başlangıçtan itibaren yürür")
                    .measured_in("m")
                    .en("spacing"),
                Param::text("blok", Arity::optional(),
                            "Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı")
                    .en("block"),
                Param::boolean("hizala", Arity::optional(),
                               "Bloğu üzerinde durduğu kenarın doğrultusuna çevirir")
                    .en("align"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya "
                   "blok yerleştirir.",
        .run     = &run_divide,
        .effect  = Effect::DocumentEdit,
    };
}

KENTOS_COMMAND(pedit)
{
    return CommandSpec{
        .id       = "core.pedit",
        .names    = {"ÇİZGİDÜZENLE", "CIZGIDUZENLE", "PEDIT", "ÇZD", "CZD"},
        .title    = "Çizgi Düzenle",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu}, "Düzenlenecek çizgiler"}
                    .en("object"),
                Param::choice("islem", Arity::optional(), {"kapat", "ac", "ters", "sadelestir"},
                              "kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · "
                              "sadelestir: yakın köşeleri at")
                    .en("action"),
                Param::number("tolerans", Arity::optional(),
                              "sadelestir: bu uzaklıktan yakın köşeler atılır (m)")
                    .measured_in("m")
                    .en("tolerance"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak "
                   "sadeleştirir.",
        .run     = &run_pedit,
        .effect  = Effect::DocumentEdit,
    };
}

} // namespace kentos::command
