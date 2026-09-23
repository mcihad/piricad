// SPDX-License-Identifier: GPL-3.0-or-later
// core.split (BÖL), core.trim (BUDA), core.extend (UZAT)
//
// The three edits that make a sketch into a drawing. A surveyor's lines arrive
// crossing each other and stopping short of each other; these are what bring them
// to their intersections.
//
// ALL THREE WORK ON POLYLINES ONLY, and say so rather than doing something
// approximate to a curve. Trimming an arc to a line is a real operation and it is
// not this one: it needs the circle-line intersection, and a command that silently
// treated an arc as its chord would move a road curve by however much the chord
// misses the arc. That is a wrong drawing, not a coarse one (§12).
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trim_curve.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace kentos::command {
namespace {

/// One entity's single open ring, or false having said why not.
bool open_run(Context& ctx, std::int64_t id, core::EntityId& slot, std::vector<core::Point2>& pts)
{
    const core::Document& doc = ctx.document();

    if (id <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    slot           = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.");
        return false;
    }

    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1 || doc.geometry().ring_role[span.first] != core::RingRole::Open) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " açık bir çizgi değil; bu komut yalnız açık çizgilerle çalışır.");
        return false;
    }

    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    pts.clear();
    pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        pts.push_back(core::Point2{xs[v], ys[v]});
    return true;
}

/// Replaces `slot`'s geometry with one open run.
bool write_run(Context& ctx, core::EntityId slot, const std::vector<core::Point2>& pts)
{
    const core::RingGeometry::RingInput ring{pts, core::RingRole::Open, 0};
    auto st = ctx.transaction().set_geometry(slot, {&ring, 1});
    if (!st) {
        ctx.refuse(st.error());
        return false;
    }
    return true;
}

/// Where along a polyline a point falls: the segment index and the fraction along
/// it. Returns false when the line has no segments.
bool locate_on(const std::vector<core::Point2>& pts, core::Point2 probe, std::size_t& segment,
               double& along, core::Point2& foot)
{
    if (pts.size() < 2) return false;

    double best = -1.0;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const core::Point2 f = core::closest_point_on_segment(pts[i], pts[i + 1], probe);
        const double d       = core::distance_squared(f, probe);
        if (best < 0.0 || d < best) {
            best    = d;
            segment = i;
            foot    = f;

            const double ex   = core::mm_to_metres(pts[i + 1].x - pts[i].x);
            const double ey   = core::mm_to_metres(pts[i + 1].y - pts[i].y);
            const double fx   = core::mm_to_metres(f.x - pts[i].x);
            const double fy   = core::mm_to_metres(f.y - pts[i].y);
            const double len2 = ex * ex + ey * ey;
            along             = len2 > 0.0 ? (fx * ex + fy * ey) / len2 : 0.0;
        }
    }
    return best >= 0.0;
}

/// Where the DRAWN segment `a`-`b` first crosses the polyline `pts`.
///
/// The segment, not its infinite extension — and that is the one place a line
/// cut and a face cut differ on purpose. A face has to be spanned to be divided,
/// so `core::half_plane` reaches well past it; a line is cut where the stroke
/// actually crossed it, because a stroke drawn across one boundary must not also
/// cut every other selected line that happens to lie on the same infinite line.
///
/// The first crossing, not the nearest: a cut enters a line once, and a user who
/// drew through a bend means the bend they drew through. Returns false when the
/// stroke misses, which is not an error — one cut may cross some of a selection
/// and not the rest.
bool segment_crossing(const std::vector<core::Point2>& pts, core::Point2 a, core::Point2 b,
                      core::Point2& out)
{
    const double abx = core::mm_to_metres(b.x - a.x);
    const double aby = core::mm_to_metres(b.y - a.y);

    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const core::Point2 p = pts[i];
        const core::Point2 q = pts[i + 1];

        const double pqx = core::mm_to_metres(q.x - p.x);
        const double pqy = core::mm_to_metres(q.y - p.y);

        const double denom = abx * pqy - aby * pqx;
        if (denom == 0.0) continue; // parallel

        const double apx = core::mm_to_metres(p.x - a.x);
        const double apy = core::mm_to_metres(p.y - a.y);

        // How far along each: `t` along this segment of the polyline, `u` along
        // the stroke the user drew. Both have to be inside their own segment for
        // the two to have actually met.
        const double t = (apx * aby - apy * abx) / denom;
        if (t < 0.0 || t > 1.0) continue;

        const double u = (apx * pqy - apy * pqx) / denom;
        if (u < 0.0 || u > 1.0) continue;

        out = core::Point2{p.x + core::mm_round(pqx * t), p.y + core::mm_round(pqy * t)};
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ BÖL ----

/// Splits one open line at a point that lies on it. The legacy `nokta` form.
bool split_run_at(Context& ctx, core::EntityId slot, const std::vector<core::Point2>& pts,
                  core::Point2 at)
{
    std::size_t segment = 0;
    double along        = 0.0;
    core::Point2 foot{};
    if (!locate_on(pts, at, segment, along, foot)) {
        ctx.refuse(core::ErrorCode::InvalidArgument, "Çizgide bölünecek kenar yok.");
        return false;
    }

    // A split AT an end produces a zero-length piece, which is not a line. Told
    // rather than silently producing a record the geometry layer would refuse.
    if ((segment == 0 && along <= 0.0) || (segment + 2 == pts.size() && along >= 1.0)) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
        return false;
    }

    // The FIRST half keeps the object, so its key, layer, style and attributes
    // stay with it (model.md R4, R28). The second half is a new object.
    std::vector<core::Point2> head(pts.begin(),
                                   pts.begin() + static_cast<std::ptrdiff_t>(segment) + 1);
    head.push_back(foot);

    std::vector<core::Point2> tail;
    tail.push_back(foot);
    tail.insert(tail.end(), pts.begin() + static_cast<std::ptrdiff_t>(segment) + 1, pts.end());

    if (head.size() < 2 || tail.size() < 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
        return false;
    }

    if (!write_run(ctx, slot, head)) return false;

    auto made = ctx.transaction().add_polyline(ctx.document().entities().layer[slot], tail);
    if (!made) {
        ctx.refuse(made.error());
        return false;
    }

    if (const core::StyleId style = ctx.document().entities().style[slot];
        style != core::kByLayerStyle) {
        auto st = ctx.transaction().set_entity_style(made.value(), style);
        if (!st) {
            ctx.refuse(st.error());
            return false;
        }
    }
    return true;
}

/// The rings of one entity as a `core::Polygon`, or false when it encloses nothing.
bool face_of(const core::Document& doc, core::EntityId slot, core::Polygon& out)
{
    const core::RingGeometry& geom = doc.geometry();
    const core::RingSpan span      = geom.rings_of(doc.entities().slot[slot]);

    out.exterior.clear();
    out.holes.clear();

    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        if (geom.ring_role[r] == core::RingRole::Open) continue;

        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        std::vector<core::Point2> ring;
        ring.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            ring.push_back(core::Point2{xs[v], ys[v]});

        if (geom.ring_role[r] == core::RingRole::Exterior && out.exterior.empty())
            out.exterior = std::move(ring);
        else if (ring.size() >= 3)
            out.holes.push_back(std::move(ring));
    }
    return out.exterior.size() >= 3;
}

/// Cuts one face in two along the infinite line through `a`-`b`.
///
/// The geometric half of an ifraz and nothing more: no ada numbering, no area
/// report, no adjacency rule. `İFRAZ` is that same cut with the cadastral half
/// on top, which is why the shape it cuts with lives in `core::half_plane` and
/// not in either command.
bool cut_face(Context& ctx, core::EntityId slot, const core::Polygon& face, core::Point2 a,
              core::Point2 b, std::size_t& made)
{
    core::Box2 box;
    for (const core::Point2& p : face.exterior)
        box.extend(p);

    std::vector<core::Polygon> pieces;
    for (const bool left : {true, false}) {
        const core::Polygon side = core::half_plane(a, b, box, left);
        if (side.exterior.empty()) continue;

        auto part = core::polygon_boolean({face}, {side}, core::BooleanOp::Intersection);
        if (!part) {
            ctx.refuse(part.error());
            return false;
        }
        for (core::Polygon& piece : part.value())
            pieces.push_back(std::move(piece));
    }

    if (pieces.size() < 2) return true; // the line missed this face; not an error

    const core::LayerId layer = ctx.document().entities().layer[slot];
    const core::StyleId style = ctx.document().entities().style[slot];

    for (const core::Polygon& piece : pieces) {
        std::vector<core::RingGeometry::RingInput> rings;
        rings.push_back(core::RingGeometry::RingInput{piece.exterior, core::RingRole::Exterior, 0});
        for (const std::vector<core::Point2>& hole : piece.holes)
            rings.push_back(core::RingGeometry::RingInput{hole, core::RingRole::Interior, 0});

        auto created = ctx.transaction().add_area(layer, rings);
        if (!created) {
            ctx.refuse(created.error());
            return false;
        }
        if (style != core::kByLayerStyle) {
            auto st = ctx.transaction().set_entity_style(created.value(), style);
            if (!st) {
                ctx.refuse(st.error());
                return false;
            }
        }

        // EVERY COLUMN TRAVELS TO BOTH HALVES. Splitting a shape does not change
        // what it IS — both halves of a cut woodland are still woodland — so
        // unlike a merge there is nothing here to disagree about.
        const core::AttrTable& table = ctx.document().attributes();
        for (std::size_t c = 0; c < table.columns(); ++c) {
            const auto col = static_cast<core::AttrId>(c);
            auto had       = ctx.document().attribute(col, slot);
            if (!had || !had.value().present) continue;
            if (auto st = ctx.transaction().set_attribute(col, created.value(), had.value()); !st) {
                ctx.refuse(st.error());
                return false;
            }
        }
        ++made;
    }

    if (auto st = ctx.transaction().erase_entity(slot); !st) {
        ctx.refuse(st.error());
        return false;
    }
    return true;
}

Task<void> run_split(Context& ctx)
{
    // THE ARGUMENT, THE SELECTION, OR ASKED FOR — in that order, like every other
    // modify command.
    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne", "Kesilecek nesneleri seçin, Enter'a basın", chosen))
        co_return;

    // THE LEGACY POINT FORM, kept because it is in journals and in scripts: a
    // single open line split at a point that lies on it. Nothing collects it
    // interactively any more — a hand draws the cut.
    // GUARDED ON `noktalar` BEING ABSENT, not merely on `nokta` being present. A
    // bare trailing token — `BÖL noktalar=10,-5 10,15` — binds positionally to the
    // next unfilled parameter, which is this one, and the command would then take
    // the second half of the cut line for a legacy split point and refuse the
    // face. A cut line named means a cut line meant.
    if (const Value at_arg = ctx.argument("nokta");
        !at_arg.empty() && ctx.argument("noktalar").empty()) {
        if (chosen.size() != 1) {
            ctx.refuse(
                core::ErrorCode::InvalidArgument,
                "`nokta` ile tek nesne bölünür; " + std::to_string(chosen.size()) +
                    " nesne verildi. Birden çok nesne için `noktalar` ile kesme çizgisi verin.");
            co_return;
        }
        core::EntityId slot = core::kNoEntity;
        std::vector<core::Point2> pts;
        if (!open_run(ctx, chosen.front(), slot, pts)) co_return;
        if (!split_run_at(ctx, slot, pts, at_arg.as_point())) co_return;

        ctx.record("nesne", Value::ids({chosen.front()}));
        ctx.record("nokta", at_arg);
        ctx.echo("Çizgi ikiye bölündü.");
        co_return;
    }

    // A CUT IS A LINE THE USER DRAWS, which is how a surveyor states one: an
    // ifraz line is agreed on the ground and drawn on the sheet, not nominated as
    // a distance along an edge. The second point carries the rubber band, so the
    // cut is visible against the drawing before it is committed.
    auto first = co_await ctx.point("noktalar", "Kesme çizgisinin ilk noktası");
    if (!first) co_return;

    PointOptions band;
    band.rubber_band   = true;
    band.rubber_origin = *first;
    auto second        = co_await ctx.point("noktalar", "Kesme çizgisinin ikinci noktası", band);
    if (!second) co_return;

    if (first->x == second->x && first->y == second->y) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kesme çizgisinin iki ucu aynı nokta; bir doğrultu belirtmiyor.");
        co_return;
    }

    const core::Document& doc = ctx.document();

    std::size_t cut_lines = 0;
    std::size_t cut_faces = 0;
    std::size_t untouched = 0;

    for (const std::int64_t raw : chosen) {
        const auto key            = static_cast<core::EntityKey>(static_cast<std::uint64_t>(raw));
        const core::EntityId slot = doc.slot_of(key);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
            co_return;
        }

        core::Polygon face;
        if (face_of(doc, slot, face)) {
            const std::size_t before = cut_faces;
            if (!cut_face(ctx, slot, face, *first, *second, cut_faces)) co_return;
            if (cut_faces == before) ++untouched;
            continue;
        }

        // An open line: split where the cut crosses it.
        core::EntityId run_slot = core::kNoEntity;
        std::vector<core::Point2> pts;
        if (!open_run(ctx, raw, run_slot, pts)) co_return;

        core::Point2 crossing{};
        if (!segment_crossing(pts, *first, *second, crossing)) {
            ++untouched;
            continue;
        }
        if (!split_run_at(ctx, run_slot, pts, crossing)) co_return;
        ++cut_lines;
    }

    if (cut_lines == 0 && cut_faces == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kesme çizgisi seçilen nesnelerin hiçbirinden geçmiyor.");
        co_return;
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.record("noktalar", Value::points({*first, *second}));

    std::string said;
    if (cut_lines != 0) said += std::to_string(cut_lines) + " çizgi bölündü.";
    if (cut_faces != 0) {
        if (!said.empty()) said += "  ";
        said += std::to_string(cut_faces) + " parça alan çıktı.";
    }
    if (untouched != 0)
        said += "\n  " + std::to_string(untouched) + " nesneyi kesme çizgisi kesmiyor.";
    ctx.echo(said);
}

// ----------------------------------------------------------- BUDA / UZAT ----

/// Writes `path` into `slot` in place, so its key, layer, style and attributes
/// stay with it (model.md R4, R28): a polyline takes the path's vertices, an arc
/// its one arc. False, having refused, when the kind cannot hold the path.
bool write_path(Context& ctx, core::EntityId slot, const core::CurvePath& path)
{
    const core::KindId kind = ctx.document().entities().kind[slot];
    if (kind == core::kPolylineKind) return write_run(ctx, slot, core::path_vertices(path));
    if (kind == core::kArcKind && path.pieces.size() == 1 &&
        path.pieces[0].kind == core::PathPiece::Kind::Arc) {
        const core::PathPiece& arc = path.pieces[0];
        const std::vector<core::Point2> ring{
            arc.centre, core::Point2{arc.centre.x + arc.radius, arc.centre.y}, arc.from, arc.to};
        const core::RingGeometry::RingInput input{ring, core::RingRole::Open, 0};
        if (auto st = ctx.transaction().set_kind_geometry(slot, {&input, 1}, {}); !st) {
            ctx.refuse(st.error());
            return false;
        }
        return true;
    }
    ctx.refuse(core::ErrorCode::Unsupported, "Kalan parça bu nesnenin türünde yazılamıyor.");
    return false;
}

/// A new object holding `path`, drawn like `like`: its layer, its style and
/// every attribute cell — a trimmed road is still that road on both sides of
/// the gap (the rule BÖL keeps for both halves of a face).
bool add_like(Context& ctx, core::EntityId like, const core::CurvePath& path)
{
    const core::Document& doc = ctx.document();
    const core::LayerId layer = doc.entities().layer[like];
    const bool one_arc =
        path.pieces.size() == 1 && path.pieces[0].kind == core::PathPiece::Kind::Arc;
    const bool all_straight = std::ranges::all_of(path.pieces, [](const core::PathPiece& p) {
        return p.kind == core::PathPiece::Kind::Segment;
    });
    if (!one_arc && !all_straight) {
        ctx.refuse(core::ErrorCode::Unsupported, "Kalan parça bir çizgi ya da tek bir yay değil.");
        return false;
    }
    const core::PathPiece& arc = path.pieces[0];
    auto made = one_arc ? ctx.transaction().add_arc(layer, arc.centre, arc.radius, arc.from, arc.to)
                        : ctx.transaction().add_polyline(layer, core::path_vertices(path));
    if (!made) {
        ctx.refuse(made.error());
        return false;
    }
    if (const core::StyleId style = doc.entities().style[like]; style != core::kByLayerStyle)
        if (const auto st = ctx.transaction().set_entity_style(made.value(), style); !st) {
            ctx.refuse(st.error());
            return false;
        }
    const core::AttrTable& table = doc.attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const auto col = static_cast<core::AttrId>(c);
        auto had       = doc.attribute(col, like);
        if (!had || !had.value().present) continue;
        if (const auto st = ctx.transaction().set_attribute(col, made.value(), had.value()); !st) {
            ctx.refuse(st.error());
            return false;
        }
    }
    return true;
}

/// The ids a selection argument holds, whichever shape it arrived in.
std::vector<std::int64_t> ids_of(const Value& v)
{
    std::vector<std::int64_t> out = v.as_ids();
    if (out.empty() && v.kind() == Value::Kind::Int) out.push_back(v.as_int());
    return out;
}

/// Shared body. THE CUTTING EDGES first — named (`sinir`), highlighted, or with
/// neither every object near the one clicked, which is how every CAD's quick
/// trim reads an empty selection — and then THE PIECES, one click each: the
/// piece clicked goes (BUDA) or the end clicked reaches the nearest edge (UZAT),
/// on a line, an arc or a circle. Every click lands in the same transaction, so
/// the whole run is one undo step (TODOS C-04).
Task<void> run_cut(Context& ctx, bool extend)
{
    const char* verb          = extend ? "UZAT" : "BUDA";
    const core::Document& doc = ctx.document();
    Bus& bus                  = ctx.session().bus();

    std::vector<std::int64_t> edge_keys;
    bool every = false;
    if (const Value all = ctx.argument("hepsi"); !all.empty() && all.as_bool()) {
        every = true;
    } else if (const Value bound = ctx.argument("sinir"); !bound.empty()) {
        edge_keys = ids_of(bound);
    } else {
        for (const core::EntityKey k : bus.selection().keys())
            edge_keys.push_back(static_cast<std::int64_t>(core::raw(k)));
        every = edge_keys.empty();
    }
    const auto invalid = [&ctx](std::span<const std::int64_t> keys) {
        const auto bad = std::ranges::find_if(keys, [](std::int64_t key) { return key <= 0; });
        if (bad == keys.end()) return false;
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(*bad) + ". Kimlikler 1'den başlar.");
        return true;
    };
    if (invalid(edge_keys)) co_return;
    for (const std::int64_t key : edge_keys) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Sınır nesnesi bulunamadı veya silinmiş: " + std::to_string(key));
            co_return;
        }
    }

    // ONE CLICK, ONE EDIT. The object is the one named for this click, or the one
    // under it — the same pick a click selects with — and the edges are the run's.
    std::vector<std::int64_t> targets;
    std::vector<core::Point2> picks;
    const auto one = [&](core::Point2 pick, std::int64_t named) -> bool {
        core::EntityId slot = core::kNoEntity;
        if (named > 0)
            slot = doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(named)));
        else
            slot = core::pick_nearest(doc, pick, bus.aid_settings().pick_radius);
        if (slot == core::kNoEntity || !doc.alive(slot)) {
            std::string why;
            if (named > 0)
                why = "Nesne bulunamadı veya silinmiş: " + std::to_string(named);
            else if (extend)
                why = "Tıklanan yerde uzatılacak bir nesne yok. Bir çizginin ya da yayın ucuna "
                      "tıklayın.";
            else
                why = "Tıklanan yerde budanacak bir nesne yok. Bir çizginin, yayın ya da "
                      "dairenin atılacak parçasına tıklayın.";
            ctx.refuse(core::ErrorCode::NotFound, std::move(why));
            return false;
        }
        const auto key = static_cast<std::int64_t>(core::raw(doc.key_of(slot)));
        if (const auto st = doc.editable(slot); !st) {
            ctx.refuse(st.error());
            return false;
        }
        const core::KindId kind                   = doc.entities().kind[slot];
        const std::optional<core::CurvePath> path = core::path_of(doc, slot);
        if (!path) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(key) +
                           " bu komutun işleyebileceği bir tür değil; " + verb +
                           " çizgi, yay ve dairelerde çalışır.");
            return false;
        }
        if (path->closed && kind == core::kPolylineKind) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(key) +
                           (extend ? " kapalı bir alan; ucu olmayan bir şekil uzatılmaz."
                                   : " kapalı bir alan; alanın bir parçası budanmaz. Alanı "
                                     "ikiye ayırmak için BÖL kullanın."));
            return false;
        }
        const std::vector<core::CurvePath> edges = core::cutting_edges(doc, slot, every, edge_keys);
        if (edges.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       std::string(verb) +
                           (extend ? " için ulaşılacak sınır yok: " : " için kesecek sınır yok: ") +
                           (every ? "tıklanan nesnenin yakınında başka bir çizgi, yay ya da "
                                    "daire yok."
                                  : "sınır olarak verilen nesneler tıklanan nesnenin kendisi "
                                    "ya da çizgi, yay veya daire değil."));
            return false;
        }

        if (extend) {
            auto reach = core::extend_curve(*path, edges, pick);
            if (!reach) {
                ctx.refuse(reach.error());
                return false;
            }
            if (!write_path(ctx, slot, reach.value().extended)) return false;
        } else {
            auto cut = core::trim_curve(*path, edges, pick);
            if (!cut) {
                ctx.refuse(cut.error());
                return false;
            }
            const std::vector<core::CurvePath>& kept = cut.value().kept;
            if (kind == core::kCircleKind) {
                // A CIRCLE CUT IS THE ARC THAT IS LEFT: a different kind, so a
                // new object drawn like the circle, and the circle goes.
                if (!add_like(ctx, slot, kept.front())) return false;
                if (const auto st = ctx.transaction().erase_entity(slot); !st) {
                    ctx.refuse(st.error());
                    return false;
                }
            } else {
                if (!write_path(ctx, slot, kept.front())) return false;
                if (kept.size() > 1 && !add_like(ctx, slot, kept[1])) return false;
            }
        }
        targets.push_back(key);
        picks.push_back(pick);
        return true;
    };

    const Value given = ctx.argument("nokta");
    if (!given.empty()) {
        const std::vector<std::int64_t> named = ids_of(ctx.argument("nesne"));
        if (invalid(named)) co_return;
        const Value::Points& points = given.as_points();
        for (std::size_t i = 0; i < points.size(); ++i)
            if (!one(points[i], i < named.size() ? named[i] : 0)) co_return;
    } else {
        // THE CLICKS, until Enter: each drawn before it is made — the piece that
        // goes marked as going, the reach an extension adds drawn as coming —
        // by the functions this body edits with (`core::trim_curve`,
        // `core::extend_curve`), against the same edges.
        const core::TrimGuide guide{.extend = extend, .every = every, .keys = edge_keys};
        while (auto at = co_await ctx.point(
                   "nokta",
                   extend ? "Uzatılacak uca tıklayın — Enter: bitir"
                          : "Atılacak parçaya tıklayın — Enter: bitir",
                   PointOptions{.rubber_band    = true,
                                .rubber_base    = false,
                                .rubber_shape   = RubberShape::Trim,
                                .rubber_payload = core::encode_trim_guide(guide)}))
            if (!one(*at, 0)) co_return;
    }

    if (picks.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string(verb) +
                       (extend ? ": hiçbir uç gösterilmedi. Uzatılacak uca"
                               : ": hiçbir parça gösterilmedi. Atılacak parçaya") +
                       " tıklayın ya da " + verb + " nokta=<nokta> yazın.");
        co_return;
    }

    // WHAT WAS DONE, in the words a replay reads: the edges as ids — or `hepsi`,
    // which a replay re-reads from the drawing it is replayed into, exactly as the
    // run read it — and each click with the object it edited.
    if (every)
        ctx.record("hepsi", Value::boolean(true));
    else
        ctx.record("sinir", Value::ids(edge_keys));
    ctx.record("nesne", Value::ids(targets));
    ctx.record("nokta", Value::points(picks));
    ctx.echo(extend ? std::to_string(picks.size()) + " uç sınıra uzatıldı."
                    : std::to_string(picks.size()) + " parça budandı.");
}

Task<void> run_trim(Context& ctx)
{
    co_await run_cut(ctx, false);
}

Task<void> run_extend(Context& ctx)
{
    co_await run_cut(ctx, true);
}

} // namespace

KENTOS_COMMAND(split)
{
    return CommandSpec{
        .id       = "core.split",
        .names    = {"BÖL", "BOL", "SPLIT", "BL"},
        .title    = "Böl",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kesilecek nesneler; yoksa etkin seçim"}
                    .en("object"),
                Param::points("noktalar", Arity{0, 2},
                              "Kesme çizgisinin iki noktası; arayüzde çizilir")
                    .en("points"),
                // THE LEGACY FORM, kept because it is written into journals and
                // into scripts already: one open line split at a point on it.
                // Nothing collects it interactively any more.
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Bölme noktası (tek çizgi; eski biçim)"}
                    .en("point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Nesneleri çizilen bir kesme çizgisiyle böler.",
        .run     = &run_split,
    };
}

KENTOS_COMMAND(trim)
{
    return CommandSpec{
        .id       = "core.trim",
        .names    = {"BUDA", "TRIM", "BD"},
        .title    = "Buda",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Budanan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne"}
                    .en("object"),
                Param{"sinir", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kesme sınırları; yoksa seçili nesneler, o da yoksa tıklanan nesnenin "
                      "yakınındaki her nesne"}
                    .en("boundary"),
                Param::boolean("hepsi", Arity::optional(),
                               "Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir "
                               "yokken öntanımlı)")
                    .en("every_edge"),
                Param::points("nokta", Arity{0, 0xFFFFFFFFu},
                              "Atılacak her parçanın üzerinde bir nokta, sırayla")
                    .en("point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve "
                   "dairede çalışır.",
        .run     = &run_trim,
    };
}

KENTOS_COMMAND(extend)
{
    return CommandSpec{
        .id       = "core.extend",
        .names    = {"UZAT", "EXTEND", "UZ"},
        .title    = "Uzat",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Uzatılan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne"}
                    .en("object"),
                Param{"sinir", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Uzatılacak sınırlar; yoksa seçili nesneler, o da yoksa tıklanan nesnenin "
                      "yakınındaki her nesne"}
                    .en("boundary"),
                Param::boolean("hepsi", Arity::optional(),
                               "Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir "
                               "yokken öntanımlı)")
                    .en("every_edge"),
                Param::points("nokta", Arity{0, 0xFFFFFFFFu},
                              "Uzatılacak her ucun yakınında bir nokta, sırayla")
                    .en("point"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, "
                   "yayınkini çemberi boyunca.",
        .run     = &run_extend,
    };
}

} // namespace kentos::command
