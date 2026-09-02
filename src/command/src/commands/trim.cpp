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

#include "kentos_cad/core/geometry.hpp"
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

// ------------------------------------------------------------------ BÖL ----

Task<void> run_split(Context& ctx)
{
    const Value given = ctx.argument("nesne");
    if (given.empty()) {
        ctx.echo("Bölünecek çizgi belirtilmedi. Örnek: BÖL nesne=1 nokta=30,0");
        co_return;
    }

    std::int64_t id = 0;
    if (given.kind() == Value::Kind::IdList) {
        const Value::Ints& ids = given.as_ids();
        if (ids.size() != 1) {
            ctx.echo("Bir seferde tek çizgi bölünür; " + std::to_string(ids.size()) +
                     " nesne verildi.");
            co_return;
        }
        id = ids[0];
    } else {
        id = given.as_int();
    }

    core::EntityId slot = core::kNoEntity;
    std::vector<core::Point2> pts;
    if (!open_run(ctx, id, slot, pts)) co_return;

    auto at = co_await ctx.point("nokta", "Bölme noktası");
    if (!at) co_return;

    std::size_t segment = 0;
    double along        = 0.0;
    core::Point2 foot{};
    if (!locate_on(pts, *at, segment, along, foot)) {
        ctx.echo("Çizgide bölünecek kenar yok.");
        co_return;
    }

    // A split AT an end produces a zero-length piece, which is not a line. Told
    // rather than silently producing a record the geometry layer would refuse.
    if ((segment == 0 && along <= 0.0) || (segment + 2 == pts.size() && along >= 1.0)) {
        ctx.echo("Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
        co_return;
    }

    // The FIRST half keeps the object, so its key, layer, style and attributes
    // stay with it (model.md R4, R28). The second half is a new object, exactly
    // as an ifraz produces one parcel that continues and one that is new.
    std::vector<core::Point2> head(pts.begin(),
                                   pts.begin() + static_cast<std::ptrdiff_t>(segment) + 1);
    head.push_back(foot);

    std::vector<core::Point2> tail;
    tail.push_back(foot);
    tail.insert(tail.end(), pts.begin() + static_cast<std::ptrdiff_t>(segment) + 1, pts.end());

    if (head.size() < 2 || tail.size() < 2) {
        ctx.echo("Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
        co_return;
    }

    if (!write_run(ctx, slot, head)) co_return;

    auto made = ctx.transaction().add_polyline(ctx.document().entities().layer[slot], tail);
    if (!made) {
        ctx.echo(made.error().message);
        co_return;
    }

    // The style column travels, when the source did not simply inherit its layer's.
    if (const core::StyleId style = ctx.document().entities().style[slot];
        style != core::kByLayerStyle) {
        auto st = ctx.transaction().set_entity_style(made.value(), style);
        if (!st) {
            ctx.echo(st.error().message);
            co_return;
        }
    }

    // RECORDED IN THE SHAPE IT ARRIVED. The bus validates again after the body
    // runs, over the values the command actually resolved (§2.6), so a `nesne`
    // recorded as a bare integer would fail post-run validation against a
    // parameter declared `ParamKind::Selection`.
    ctx.record("nesne", given);
    ctx.record("nokta", Value::point(*at));
    ctx.echo("Çizgi ikiye bölündü.");
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

    const Value target_arg = ctx.argument("nesne");
    const Value edge_arg   = ctx.argument("sinir");
    if (target_arg.empty() || edge_arg.empty()) {
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

    const std::int64_t target_id = one_id(target_arg);
    const std::int64_t edge_id   = one_id(edge_arg);

    core::EntityId target = core::kNoEntity;
    core::EntityId edge   = core::kNoEntity;
    std::vector<core::Point2> pts;
    std::vector<core::Point2> edge_pts;
    if (!open_run(ctx, target_id, target, pts)) co_return;
    if (!open_run(ctx, edge_id, edge, edge_pts)) co_return;

    // WHICH END. The point says which end of the line the user is working on,
    // exactly as it does in every drawing program: click the bit you want gone,
    // or the end you want pushed out.
    auto at = co_await ctx.point("nokta", extend ? "Uzatılacak uç" : "Atılacak parça");
    if (!at) co_return;

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
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Bölünecek çizginin kimliği"},
                Param::point("nokta", "Bölme noktası"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir çizgiyi verilen noktadan ikiye böler.",
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
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Budanacak çizginin kimliği"},
                Param{"sinir", ParamKind::Selection, Arity::exactly(1), "Sınır çizgisinin kimliği"},
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
                Param{"nesne", ParamKind::Selection, Arity::exactly(1),
                      "Uzatılacak çizginin kimliği"},
                Param{"sinir", ParamKind::Selection, Arity::exactly(1), "Sınır çizgisinin kimliği"},
                Param::point("nokta", "Uzatılacak ucun yakınında bir nokta"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir çizgiyi sınır çizgisine ulaşana kadar uzatır.",
        .run     = &run_extend,
    };
}

} // namespace kentos::command
