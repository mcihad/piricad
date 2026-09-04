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

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// One entity's single open ring, or false having said why not.
bool open_run(Context& ctx, std::int64_t id, core::EntityId& slot, std::vector<core::Point2>& pts)
{
    const core::Document& doc = ctx.document();

    if (id <= 0) {
        ctx.echo("Geçersiz nesne kimliği: " + std::to_string(id) + ". Kimlikler 1'den başlar.");
        return false;
    }
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    slot           = doc.slot_of(key);
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return false;
    }
    if (doc.entities().kind[slot] != core::kPolylineKind) {
        ctx.echo("Nesne " + std::to_string(id) +
                 " bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.");
        return false;
    }

    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1 || doc.geometry().ring_role[span.first] != core::RingRole::Open) {
        ctx.echo("Nesne " + std::to_string(id) +
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
        ctx.echo(st.error().message);
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
        ctx.echo("Çizgide bölünecek kenar yok.");
        return false;
    }

    // A split AT an end produces a zero-length piece, which is not a line. Told
    // rather than silently producing a record the geometry layer would refuse.
    if ((segment == 0 && along <= 0.0) || (segment + 2 == pts.size() && along >= 1.0)) {
        ctx.echo("Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
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
        ctx.echo("Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
        return false;
    }

    if (!write_run(ctx, slot, head)) return false;

    auto made = ctx.transaction().add_polyline(ctx.document().entities().layer[slot], tail);
    if (!made) {
        ctx.echo(made.error().message);
        return false;
    }

    if (const core::StyleId style = ctx.document().entities().style[slot];
        style != core::kByLayerStyle) {
        auto st = ctx.transaction().set_entity_style(made.value(), style);
        if (!st) {
            ctx.echo(st.error().message);
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
            ctx.echo(part.error().message);
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
            ctx.echo(created.error().message);
            return false;
        }
        if (style != core::kByLayerStyle) {
            auto st = ctx.transaction().set_entity_style(created.value(), style);
            if (!st) {
                ctx.echo(st.error().message);
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
                ctx.echo(st.error().message);
                return false;
            }
        }
        ++made;
    }

    if (auto st = ctx.transaction().erase_entity(slot); !st) {
        ctx.echo(st.error().message);
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
            ctx.echo("`nokta` ile tek nesne bölünür; " + std::to_string(chosen.size()) +
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
        ctx.echo("Kesme çizgisinin iki ucu aynı nokta; bir doğrultu belirtmiyor.");
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
            ctx.echo("Nesne bulunamadı veya silinmiş: " + std::to_string(raw));
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
        ctx.echo("Kesme çizgisi seçilen nesnelerin hiçbirinden geçmiyor.");
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

/// Where `line` meets the infinite line through `a`-`b`, as a fraction along
/// `line`'s own segment. Returns false when they are parallel.
bool cut_fraction(core::Point2 p0, core::Point2 p1, core::Point2 a, core::Point2 b, double& t)
{
    const double dx = core::mm_to_metres(p1.x - p0.x);
    const double dy = core::mm_to_metres(p1.y - p0.y);
    const double ex = core::mm_to_metres(b.x - a.x);
    const double ey = core::mm_to_metres(b.y - a.y);

    const double denom = dx * ey - dy * ex;
    if (std::abs(denom) < 1e-12) return false; // parallel: no crossing to move to

    const double wx = core::mm_to_metres(a.x - p0.x);
    const double wy = core::mm_to_metres(a.y - p0.y);

    t = (wx * ey - wy * ex) / denom;
    return true;
}

/// Shared body: `keep_start` says which side of the cut survives a trim, and
/// `extend` says whether the end is being pushed out instead of pulled back.
Task<void> run_cut(Context& ctx, bool extend)
{
    const char* verb = extend ? "UZAT" : "BUDA";

    Value target_arg = ctx.argument("nesne");
    Value edge_arg   = ctx.argument("sinir");

    // TWO SELECTED OBJECTS ARE THE PAIR, AND THE CLICK SAYS WHICH IS WHICH.
    //
    // Without this the tool column's Buda button was dead: it sends the bare
    // command, so both arguments were always empty and the only outcome was the
    // "hem düzenlenecek çizgi hem sınır çizgisi gerekir" line, whatever was
    // selected. Filling them from the selection in order is not an option either —
    // `Selection::keys()` is sorted by key, not by the order the user clicked, so
    // "the first one is the target" would mean "whichever was drawn first", which
    // is not something the user said.
    //
    // The point resolves it, and it is the gesture every drawing program already
    // trains: you click the piece you want GONE. So the click is taken first, and
    // whichever of the two selected lines it lands nearer is the one being cut.
    const bool from_selection = target_arg.empty() && edge_arg.empty();
    std::vector<std::int64_t> pair;
    if (from_selection) {
        if (!co_await want_objects(ctx, "nesne",
                                   std::string(verb) +
                                       " için iki çizgi seçin — düzenlenecek ve sınır — "
                                       "sonra Enter'a basın",
                                   pair, 2))
            co_return;

        if (pair.size() != 2) {
            ctx.echo(std::string(verb) +
                     " tam iki çizgi ister: düzenlenecek olan ve sınır. Seçili: " +
                     std::to_string(pair.size()) + ". Ya da " + verb +
                     " nesne=1 sinir=2 nokta=5,0 yazın.");
            co_return;
        }
    } else if (target_arg.empty() || edge_arg.empty()) {
        ctx.echo(std::string(verb) +
                 " için hem düzenlenecek çizgi hem sınır çizgisi gerekir. "
                 "Örnek: " +
                 verb + " nesne=1 sinir=2 nokta=5,0");
        co_return;
    }

    const auto one_id = [&](const Value& v) {
        return v.kind() == Value::Kind::IdList
                   ? (v.as_ids().size() == 1 ? v.as_ids()[0] : std::int64_t{0})
                   : v.as_int();
    };

    // ASKED BEFORE THE PAIR IS SETTLED when it came from the selection, because
    // the answer is what settles it.
    auto at = co_await ctx.point("nokta", extend ? "Uzatılacak uç" : "Atılacak parça");
    if (!at) co_return;

    std::int64_t target_id = 0;
    std::int64_t edge_id   = 0;
    if (from_selection) {
        core::EntityId a = core::kNoEntity;
        core::EntityId b = core::kNoEntity;
        std::vector<core::Point2> a_pts;
        std::vector<core::Point2> b_pts;
        if (!open_run(ctx, pair[0], a, a_pts)) co_return;
        if (!open_run(ctx, pair[1], b, b_pts)) co_return;

        const auto nearest = [&](const std::vector<core::Point2>& run) {
            double best = -1.0;
            for (std::size_t i = 0; i + 1 < run.size(); ++i) {
                const core::Point2 f = core::closest_point_on_segment(run[i], run[i + 1], *at);
                const double d       = core::distance_squared(f, *at);
                if (best < 0.0 || d < best) best = d;
            }
            return best;
        };

        const bool first_is_target = nearest(a_pts) <= nearest(b_pts);
        target_id                  = first_is_target ? pair[0] : pair[1];
        edge_id                    = first_is_target ? pair[1] : pair[0];

        // RECORDED AS THE IDS IT RESOLVED, so a replay cuts the same line with the
        // same boundary whatever the selection holds then (model.md R43).
        target_arg = Value::ids({target_id});
        edge_arg   = Value::ids({edge_id});
    } else {
        target_id = one_id(target_arg);
        edge_id   = one_id(edge_arg);
    }

    core::EntityId target = core::kNoEntity;
    core::EntityId edge   = core::kNoEntity;
    std::vector<core::Point2> pts;
    std::vector<core::Point2> edge_pts;
    if (!open_run(ctx, target_id, target, pts)) co_return;
    if (!open_run(ctx, edge_id, edge, edge_pts)) co_return;

    const double to_start = core::distance_squared(pts.front(), *at);
    const double to_end   = core::distance_squared(pts.back(), *at);
    const bool at_start   = to_start <= to_end;

    // The segment that moves is the one at that end.
    const std::size_t seg = at_start ? 0 : pts.size() - 2;
    const core::Point2 p0 = at_start ? pts[1] : pts[seg];     // the anchored end
    const core::Point2 p1 = at_start ? pts[0] : pts[seg + 1]; // the end that moves

    // Every edge segment is a candidate; the nearest crossing to the moving end
    // is the one meant, because that is the first boundary the line reaches.
    bool found  = false;
    double best = 0.0;
    core::Point2 cut{};

    for (std::size_t i = 0; i + 1 < edge_pts.size(); ++i) {
        double t = 0.0;
        if (!cut_fraction(p0, p1, edge_pts[i], edge_pts[i + 1], t)) continue;

        const core::Point2 hit{p0.x + core::mm_round((static_cast<double>(p1.x - p0.x)) * t),
                               p0.y + core::mm_round((static_cast<double>(p1.y - p0.y)) * t)};

        // The crossing has to be ON the boundary segment, not merely on its line:
        // a line trimmed to where two boundaries would have met had they been
        // longer is a line trimmed to nothing that exists.
        const core::Point2 foot = core::closest_point_on_segment(edge_pts[i], edge_pts[i + 1], hit);
        if (core::distance_squared(foot, hit) > 1.0) continue;

        // Trimming pulls the end back (t < 1), extending pushes it out (t > 1).
        if (extend ? (t <= 1.0) : (t >= 1.0 || t <= 0.0)) continue;

        if (!found || std::abs(t - 1.0) < std::abs(best - 1.0)) {
            found = true;
            best  = t;
            cut   = hit;
        }
    }

    if (!found) {
        ctx.echo(extend ? "Bu uç, sınır çizgisine uzatılarak ulaşamıyor: kesişme yok."
                        : "Bu uç sınır çizgisini kesmiyor; budanacak bir şey yok.");
        co_return;
    }

    std::vector<core::Point2> out = pts;
    if (at_start)
        out.front() = cut;
    else
        out.back() = cut;

    if (!write_run(ctx, target, out)) co_return;

    // The shape they arrived in; see the note in BÖL.
    ctx.record("nesne", target_arg);
    ctx.record("sinir", edge_arg);
    ctx.record("nokta", Value::point(*at));
    ctx.echo(extend ? "Çizgi sınıra uzatıldı." : "Çizgi sınıra kadar budandı.");
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
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity{0, 0xFFFFFFFFu},
                      "Kesilecek nesneler; yoksa etkin seçim"},
                Param::points("noktalar", Arity{0, 2},
                              "Kesme çizgisinin iki noktası; arayüzde çizilir"),
                // THE LEGACY FORM, kept because it is written into journals and
                // into scripts already: one open line split at a point on it.
                // Nothing collects it interactively any more.
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Bölme noktası (tek çizgi; eski biçim)"},
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
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::optional(),
                      "Budanacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan"},
                Param{"sinir", ParamKind::Selection, Arity::optional(),
                      "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri"},
                Param::point("nokta", "Atılacak parçanın üzerindeki bir nokta"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir çizgiyi kestiği sınır çizgisine kadar budar.",
        .run     = &run_trim,
    };
}

KENTOS_COMMAND(extend)
{
    return CommandSpec{
        .id       = "core.extend",
        .names    = {"UZAT", "EXTEND", "UZ"},
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::optional(),
                      "Uzatılacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan"},
                Param{"sinir", ParamKind::Selection, Arity::optional(),
                      "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri"},
                Param::point("nokta", "Uzatılacak ucun yakınında bir nokta"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir çizgiyi sınır çizgisine ulaşana kadar uzatır.",
        .run     = &run_extend,
    };
}

} // namespace kentos::command
