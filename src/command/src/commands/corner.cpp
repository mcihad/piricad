// SPDX-License-Identifier: GPL-3.0-or-later
// core.chamfer (PAH), core.fillet (YUVARLA)
//
// The two ways a corner stops being a corner. A kerb return, a building's cut
// corner at a junction, a road curve where two alignments meet: all of them are
// one of these two operations on a vertex.
//
// BOTH ARE EXACT WHERE THEY CAN BE. The tangent points are computed from unit
// vectors built with `sqrt` — correctly rounded by IEEE-754 — and never from
// `atan2` and a trigonometric call, which are not required to agree between
// platforms (§7.3). The half-angle identities below exist for that reason and not
// to save a call.
//
// A ROUNDED CORNER IS AN ARC AND THE OBJECT STAYS ONE OBJECT (TODOS O-2). The
// corner is cut on the object's path (`core::cut_path_corner`) and the path is
// written back into the same slot (`rewrite_path`): a polyline rounded at a
// corner becomes an arc polyline in place, keeping its key, its layer, its
// attributes and whatever follows it, and a parcel stays a face whose area is
// the arc's own (model.md R9b). It used to break an open line into two lines
// and an arc, and to draw a closed shape's arc into the ring as sixteen chords
// with a sentence about how far they strayed — "you add points", the user
// said, "a rounded corner should be an arc".
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/core/corner.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/fillet.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace kentos::command {
namespace {

/// Millimetres as the metres a user reads, three decimals, Turkish comma.
std::string metres_text(core::Mm value)
{
    const auto whole = static_cast<std::uint64_t>(value < 0 ? -value : value);
    std::string frac = std::to_string(whole % 1000);
    while (frac.size() < 3)
        frac.insert(frac.begin(), '0');
    return (value < 0 ? "-" : "") + std::to_string(whole / 1000) + "," + frac + " m";
}

/// A line or an area whose corners can be cut: its slot, its ring's vertices
/// (what names a corner), whether it closes, and its path (what is cut).
struct Cornered
{
    std::int64_t id{0};
    core::EntityId slot{core::kNoEntity};
    std::vector<core::Point2> pts;
    bool closed{false};
    core::CurvePath path;
};

/// Whether a slot of this kind holds corners: a polyline or an arc polyline,
/// never a caption's baseline.
bool has_corners(const core::Document& doc, core::EntityId slot)
{
    const core::KindId kind = doc.entities().kind[slot];
    return (kind == core::kPolylineKind || kind == core::kArcPolylineKind) &&
           !doc.texts().has(doc.entities().slot[slot]);
}

/// Why object `id` has no corners to cut, or nothing when it has them.
std::optional<core::Error> corner_problem(const core::Document& doc, std::int64_t id, Cornered& out)
{
    if (id <= 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Geçersiz nesne kimliği: " + std::to_string(id) +
                             ". Kimlikler 1'den başlar.");
    out.id         = id;
    const auto key = static_cast<core::EntityKey>(static_cast<std::uint64_t>(id));
    out.slot       = doc.slot_of(key);
    if (out.slot == core::kNoEntity || !doc.alive(out.slot))
        return core::err(core::ErrorCode::NotFound,
                         "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
    if (!has_corners(doc, out.slot))
        return core::err(core::ErrorCode::Unsupported,
                         "Nesne " + std::to_string(id) +
                             " bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve "
                             "alanlarda çalışır.");

    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[out.slot]);
    if (span.count != 1)
        return core::err(core::ErrorCode::Unsupported,
                         "Nesne " + std::to_string(id) +
                             " çok halkalı; köşe işlemleri tek halkalı nesnelerde çalışır.");

    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    out.pts.clear();
    out.pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        out.pts.push_back(core::Point2{xs[v], ys[v]});
    out.closed = doc.geometry().ring_role[span.first] != core::RingRole::Open;
    auto path  = core::path_of(doc, out.slot);
    if (!path)
        return core::err(core::ErrorCode::Unsupported,
                         "Nesne " + std::to_string(id) + " bir yol olarak okunamıyor.");
    out.path = std::move(*path);
    return std::nullopt;
}

/// The object behind one id — or the reason refused.
bool corner_of(const Context& ctx, const Value& given, Cornered& out)
{
    std::int64_t id = 0;
    if (given.kind() != Value::Kind::IdList)
        id = given.as_int();
    else if (given.as_ids().size() == 1)
        id = given.as_ids()[0];
    if (auto problem = corner_problem(ctx.document(), id, out)) {
        ctx.refuse(std::move(*problem));
        return false;
    }
    return true;
}

/// Whether a path has an arc piece: the object is an arc polyline once written.
bool bends(const core::CurvePath& path)
{
    return std::ranges::any_of(
        path.pieces, [](const core::PathPiece& p) { return p.kind == core::PathPiece::Kind::Arc; });
}

/// The path behind one object of a pair, or nothing with the reason said.
std::optional<core::CurvePath> pair_path(const Context& ctx, std::int64_t id, core::EntityId& slot)
{
    const core::Document& doc = ctx.document();
    slot = id > 0 ? doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(id)))
                  : core::kNoEntity;
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(id));
        return std::nullopt;
    }
    auto path = core::path_of(doc, slot);
    if (!path)
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(id) +
                       " iki nesne arasındaki köşede kullanılamıyor; çizgi, yay, daire ya da "
                       "yaylı çoklu çizgi seçin.");
    return path;
}

/// The vertex of polyline `slot` a click at `at` names, when it names one: a
/// corner within the click's own pick box — exactly under it for a client with
/// no screen.
std::optional<std::size_t> corner_under(const core::Document& doc, core::EntityId slot,
                                        core::Point2 at, core::Mm reach)
{
    if (!has_corners(doc, slot)) return std::nullopt;
    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[slot]);
    if (span.count != 1) return std::nullopt;
    const auto xs = doc.geometry().ring_xs(span.first);
    const auto ys = doc.geometry().ring_ys(span.first);
    std::vector<core::Point2> pts;
    pts.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        pts.push_back(core::Point2{xs[v], ys[v]});
    const bool closed    = doc.geometry().ring_role[span.first] != core::RingRole::Open;
    const auto at_corner = core::nearest_corner(pts, closed, at);
    if (!at_corner) return std::nullopt;
    const auto r = static_cast<double>(reach);
    if (core::distance_squared(pts[*at_corner], at) > r * r) return std::nullopt;
    return at_corner;
}

/// The vertex two adjacent edges of one polyline share, when the two clicks
/// fall on such edges — "these two edges" is the same corner as "this corner".
std::optional<core::Point2> shared_corner(const core::CurvePath& path, core::Point2 a,
                                          core::Point2 b)
{
    const std::size_t pa = core::place_of(path, a).piece;
    const std::size_t pb = core::place_of(path, b).piece;
    const std::size_t n  = path.pieces.size();
    if (pa == pb) return std::nullopt;
    if (pb == pa + 1) return path.pieces[pa].to;
    if (pa == pb + 1) return path.pieces[pb].to;
    if (path.closed && ((pa == 0 && pb + 1 == n) || (pb == 0 && pa + 1 == n)))
        return pa == 0 ? path.pieces[pb].to : path.pieces[pa].to;
    return std::nullopt;
}

/// THE CORNER BETWEEN TWO OBJECTS (TODOS C-06): line–line, line–arc, arc–arc.
/// The picks say which part of each stays and so which corner is meant; the
/// size is typed or shown, with the result drawn at the cursor by the very
/// function that makes it (`core::fillet_pair`, `core::chamfer_pair`).
Task<void> run_pair(Context& ctx, bool fillet, std::int64_t id_a, core::Point2 pick_a,
                    std::int64_t id_b, core::Point2 pick_b)
{
    core::EntityId slot_a = core::kNoEntity;
    core::EntityId slot_b = core::kNoEntity;
    const auto path_a     = pair_path(ctx, id_a, slot_a);
    if (!path_a) co_return;
    const auto path_b = pair_path(ctx, id_b, slot_b);
    if (!path_b) co_return;
    const bool trim = !ctx.has_argument("budama") || ctx.argument("budama").as_bool();

    // THE SIZE, TYPED OR SHOWN: measured from where the two meet — the sharp
    // corner a zero radius makes — and drawn there as the cursor moves.
    core::Point2 origin = pick_b;
    if (auto sharp = core::fillet_pair(*path_a, pick_a, *path_b, pick_b, 0))
        origin = sharp.value().on_a;
    PointOptions guide;
    guide.rubber_band    = true;
    guide.rubber_origin  = origin;
    guide.rubber_shape   = RubberShape::PairCorner;
    guide.rubber_payload = core::encode_pair_corner_guide(core::PairCornerGuide{.key_a  = id_a,
                                                                                .key_b  = id_b,
                                                                                .pick_a = pick_a,
                                                                                .pick_b = pick_b,
                                                                                .fillet = fillet,
                                                                                .trim   = trim});
    guide.pick_distance  = true;
    auto size =
        co_await ctx.number(fillet ? "yaricap" : "mesafe",
                            fillet ? "Yuvarlatma yarıçapı (metre) — yazın ya da gösterin; 0 keskin "
                                     "köşe"
                                   : "Köşeden kesilecek mesafe (metre) — yazın ya da gösterin",
                            std::move(guide));
    if (!size) co_return;
    const core::Mm want = core::mm_round(*size * static_cast<double>(core::kMmPerMetre));
    core::Mm second     = want;
    if (const Value v = ctx.argument("ikinci_mesafe"); !fillet && !v.empty())
        second = core::mm_round(v.as_number() * static_cast<double>(core::kMmPerMetre));

    auto made = fillet ? core::fillet_pair(*path_a, pick_a, *path_b, pick_b, want)
                       : core::chamfer_pair(*path_a, pick_a, *path_b, pick_b, want, second);
    if (!made) {
        ctx.refuse(made.error());
        co_return;
    }
    const core::PairCorner& corner = made.value();

    // THE SOURCES CUT OR CARRIED TO THE CORNER, unless asked not to be; a
    // closed shape — a circle a line is rounded against — stays whole.
    std::vector<PathEdit> edits;
    if (trim) {
        for (const auto& [slot, piece] :
             {std::pair{slot_a, &corner.a}, std::pair{slot_b, &corner.b}}) {
            PathEdit edit;
            if (!replace_with_pieces(ctx, slot, {*piece}, edit)) co_return;
            edits.push_back(std::move(edit));
        }
    }
    std::vector<std::int64_t> added;
    if (corner.has_link) {
        core::CurvePath link;
        link.pieces.push_back(corner.link);
        const core::EntityId like = ctx.document().alive(slot_a) ? slot_a : slot_b;
        // The corner's own piece came from the two objects it joins.
        const core::EntityKey both[] = {ctx.document().entities().key[slot_a],
                                        ctx.document().entities().key[slot_b]};
        if (!add_path_like(ctx, like, link, added, both)) co_return;
    }

    ctx.record("nesne", Value::ids({id_a, id_b}));
    ctx.record("nokta", Value::point(pick_a));
    ctx.record("ikinci_nokta", Value::point(pick_b));
    ctx.record(fillet ? "yaricap" : "mesafe", Value::number(*size));
    if (const Value v = ctx.argument("ikinci_mesafe"); !fillet && !v.empty())
        ctx.record("ikinci_mesafe", v);
    if (!trim) ctx.record("budama", Value::boolean(false));
    core::Json report = core::Json::object({});
    report.set("duzenlenen", edits_json(edits));
    core::Json keys = core::Json::array({});
    for (const std::int64_t k : added)
        keys.push(core::Json::integer(k));
    report.set("eklenen", std::move(keys));
    ctx.report(std::move(report));

    if (fillet && want == 0)
        ctx.echo("İki nesne keskin köşede buluştu.");
    else if (fillet)
        ctx.echo("İki nesne arasında köşe yuvarlatıldı (yarıçap " + metres_text(want) + ").");
    else
        ctx.echo("İki çizgi arasında pah kırıldı.");
}

/// EVERY CORNER OF ONE OR MANY OBJECTS (`hepsi`), a chain cut by hand one
/// after another: the size typed or shown from the first object's first
/// corner, every object drawn at the cursor, a corner the size does not fit
/// passed over and counted — and, with several, an object that has no corners
/// to cut passed over and counted too (TODOS C-06).
Task<void> run_every(Context& ctx, bool fillet, const std::vector<std::int64_t>& ids)
{
    std::vector<Cornered> targets;
    std::size_t unfit_objects = 0;
    for (const std::int64_t id : ids) {
        Cornered t;
        auto problem = corner_problem(ctx.document(), id, t);
        if (!problem && t.pts.size() < 3)
            problem = core::err(core::ErrorCode::InvalidArgument, "Bu çizginin köşesi yok.");
        if (problem) {
            // One object named is refused for what it is; among several it is
            // passed over, as a corner the size does not fit is.
            if (ids.size() == 1) {
                ctx.refuse(std::move(*problem));
                co_return;
            }
            ++unfit_objects;
            continue;
        }
        targets.push_back(std::move(t));
    }
    if (targets.empty()) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Seçilen nesnelerin hiçbirinde işlenecek köşe yok; köşeli bir çizgi ya da "
                   "alan seçin.");
        co_return;
    }

    const Cornered& lead    = targets.front();
    const std::size_t first = lead.closed ? 0 : 1;
    core::CornerPreview shown{
        .key = lead.id, .at = static_cast<std::uint32_t>(first), .fillet = fillet, .every = true};
    for (std::size_t k = 1; k < targets.size(); ++k)
        shown.also.push_back(targets[k].id);
    PointOptions guide;
    guide.rubber_band    = true;
    guide.rubber_origin  = lead.pts[first];
    guide.rubber_shape   = RubberShape::Corner;
    guide.rubber_payload = core::encode_corner_preview(shown);
    guide.pick_distance  = true;
    auto size            = co_await ctx.number(fillet ? "yaricap" : "mesafe",
                                    fillet ? "Bütün köşeler için yarıçap (metre) — yazın ya da "
                                                        "gösterin"
                                                      : "Bütün köşeler için mesafe (metre) — yazın ya da "
                                                        "gösterin",
                                    std::move(guide));
    if (!size) co_return;
    const core::Mm want = core::mm_round(*size * static_cast<double>(core::kMmPerMetre));
    if (want <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string(fillet ? "Yarıçap" : "Mesafe") + " sıfırdan büyük olmalı.");
        co_return;
    }

    std::vector<core::PathCorners> runs;
    runs.reserve(targets.size());
    std::size_t cut = 0;
    for (const Cornered& t : targets) {
        runs.push_back(core::cut_every_path_corner(t.path, want, fillet));
        cut += runs.back().cut;
    }
    if (cut == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Bu değer hiçbir köşeye sığmıyor; daha küçük bir değer verin.");
        co_return;
    }

    // EACH OBJECT WRITTEN BACK INTO ITSELF: a polyline whose corners became
    // arcs is an arc polyline now, by the same key, with its attributes and
    // its followers (model.md R9b, TODOS C-08, O-2).
    std::vector<PathEdit> edits;
    std::size_t skipped       = 0;
    std::size_t touched       = 0;
    std::size_t bent_lines    = 0;
    std::size_t bent_areas    = 0;
    const core::Document& doc = ctx.document();
    for (std::size_t k = 0; k < targets.size(); ++k) {
        const core::PathCorners& all = runs[k];
        skipped += all.skipped;
        if (all.cut == 0) continue;
        ++touched;
        const bool was_plain = doc.entities().kind[targets[k].slot] == core::kPolylineKind;
        if (!rewrite_path(ctx, targets[k].slot, all.path)) co_return;
        edits.push_back(PathEdit{.source = targets[k].id, .result = {targets[k].id}});
        if (was_plain && bends(all.path)) ++(targets[k].closed ? bent_areas : bent_lines);
    }
    ctx.record("nesne", Value::ids(ids));
    ctx.record("hepsi", Value::boolean(true));
    ctx.record(fillet ? "yaricap" : "mesafe", Value::number(*size));
    ctx.record("nokta", Value{});
    if (!edits.empty()) ctx.report(edits_json(edits));

    std::string said =
        std::to_string(cut) + (fillet ? " köşe yuvarlatıldı" : " köşeye pah kırıldı");
    if (targets.size() + unfit_objects > 1) said += " (" + std::to_string(touched) + " nesnede)";
    if (targets.size() == 1 && bent_lines == 1) {
        said += "; çizgi tek bir yaylı çoklu çizgi oldu.";
    } else if (targets.size() == 1 && bent_areas == 1) {
        said += "; alan yaylı kenarlı bir alan oldu.";
    } else if (bent_lines + bent_areas != 0) {
        std::string became;
        if (bent_lines != 0) became = std::to_string(bent_lines) + " açık çizgi yaylı çoklu çizgi";
        if (bent_areas != 0)
            became += (became.empty() ? "" : " ve ") + std::to_string(bent_areas) +
                      " alan yaylı kenarlı alan";
        said += "; " + became + " oldu.";
    } else {
        said += '.';
    }
    if (skipped != 0)
        said += "\n  " + std::to_string(skipped) +
                " köşe bu değere sığmadığı ya da düz olduğu için olduğu gibi kaldı.";
    if (unfit_objects != 0)
        said += "\n  " + std::to_string(unfit_objects) +
                " nesne köşeli bir çizgi ya da alan olmadığı için olduğu gibi kaldı.";
    ctx.echo(said);
}

/// One polyline's own corner — the tool as it was before the pair mode.
Task<void> run_vertex(Context& ctx, bool fillet, Value given, std::optional<core::Point2> at_pt);

Task<void> run_corner(Context& ctx, bool fillet)
{
    const char* verb          = fillet ? "YUVARLA" : "PAH";
    const core::Document& doc = ctx.document();
    Bus& bus                  = ctx.session().bus();

    // WHICH OBJECTS AND WHICH CORNER, three ways in:
    //   1. named, `nesne` — one object and its corner, or two and a pick on each;
    //   2. highlighted — the same;
    //   3. neither: ONE CLICK. On a polyline's own corner it names both the
    //      object and the corner; anywhere else it names the FIRST of two, and
    //      the second is asked for (TODOS C-06).
    // EVERY CORNER (`hepsi`) takes any number of objects — the whole
    // selection — and a click anywhere on one names it.
    const Value every = ctx.argument("hepsi");
    const bool all    = !every.empty() && every.as_bool();
    Value given       = ctx.argument("nesne");
    if (given.empty()) {
        const auto keys = bus.selection().keys();
        if (keys.size() == 1 || keys.size() == 2 || (all && !keys.empty())) {
            std::vector<std::int64_t> picked;
            picked.reserve(keys.size());
            for (const auto key : keys)
                picked.push_back(static_cast<std::int64_t>(core::raw(key)));
            given = Value::ids(std::move(picked));
        }
    }
    std::vector<std::int64_t> ids;
    if (given.kind() == Value::Kind::IdList)
        ids = given.as_ids();
    else if (!given.empty())
        ids.push_back(given.as_int());
    if (all && !ids.empty()) {
        co_await run_every(ctx, fillet, ids);
        co_return;
    }
    if (ids.size() > 2) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string("Köşe iki nesne arasında kurulur; ") + std::to_string(ids.size()) +
                       " nesnenin bütün köşeleri için hepsi=evet verin.");
        co_return;
    }

    if (ids.size() == 2) {
        auto pick_a = co_await ctx.point("nokta", "Birinci nesnenin kalacak parçasına tıklayın");
        if (!pick_a) co_return;
        auto pick_b = co_await ctx.point(
            "ikinci_nokta", "İkinci nesnenin kalacak parçasına tıklayın",
            PointOptions{.rubber_band = true, .rubber_origin = *pick_a, .rubber_base = false});
        if (!pick_b) co_return;
        co_await run_pair(ctx, fillet, ids[0], *pick_a, ids[1], *pick_b);
        co_return;
    }
    if (ids.size() == 1) {
        co_await run_vertex(ctx, fillet, Value::ids({ids[0]}), std::nullopt);
        co_return;
    }

    const char* ask = fillet ? "Yuvarlatılacak köşeye tıklayın ya da birinci nesneyi gösterin"
                             : "Pah kırılacak köşeye tıklayın ya da birinci nesneyi gösterin";
    if (all)
        ask = fillet ? "Bütün köşeleri yuvarlatılacak çizgiye ya da alana tıklayın"
                     : "Bütün köşelerine pah kırılacak çizgiye ya da alana tıklayın";
    auto first = co_await ctx.point("nokta", ask);
    if (!first) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   std::string(verb) + " için nesne belirtilmedi. Örnek: " + verb +
                       " nesne=1 nokta=10,10 " + (fillet ? "yaricap=3" : "mesafe=3"));
        co_return;
    }
    // The click's own pick box — the one SEÇ uses — so what is under the
    // cursor is what is taken; a client with no screen picks what lies exactly
    // under the point, which a corner always does.
    const core::Mm reach     = bus.aid_settings().pick_radius;
    const core::EntityId hit = core::pick_nearest(doc, *first, reach);
    if (hit == core::kNoEntity) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Orada köşesi kesilecek bir çizgi ya da alan yok. Bir çizginin iki "
                   "kenarının buluştuğu köşeye tıklayın.");
        co_return;
    }
    const auto id_a = static_cast<std::int64_t>(core::raw(doc.key_of(hit)));
    if (all) {
        co_await run_every(ctx, fillet, {id_a});
        co_return;
    }
    if (corner_under(doc, hit, *first, reach)) {
        co_await run_vertex(ctx, fillet, Value::ids({id_a}), first);
        co_return;
    }

    // FROM THE FIRST PICK TO THE CURSOR, so the pair being named is on screen
    // — nothing is aimed from it (`rubber_base`).
    auto second = co_await ctx.point(
        "ikinci_nokta", "İkinci nesneye, kalacak parçasından tıklayın",
        PointOptions{.rubber_band = true, .rubber_origin = *first, .rubber_base = false});
    if (!second) co_return;
    const core::EntityId other = core::pick_nearest(doc, *second, reach);
    if (other == core::kNoEntity) {
        ctx.refuse(core::ErrorCode::NotFound, "İkinci tıklamanın altında bir nesne yok.");
        co_return;
    }
    if (other == hit) {
        // TWO EDGES OF ONE POLYLINE are the corner between them.
        const auto path = core::path_of(doc, hit);
        const auto at   = path ? shared_corner(*path, *first, *second) : std::nullopt;
        if (!at || !has_corners(doc, hit)) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "İki tıklama aynı nesnenin bitişik olmayan yerlerinde; köşesini "
                       "işlemek için köşeye tıklayın.");
            co_return;
        }
        ctx.record("ikinci_nokta", Value{});
        co_await run_vertex(ctx, fillet, Value::ids({id_a}), at);
        co_return;
    }
    co_await run_pair(ctx, fillet, id_a, *first,
                      static_cast<std::int64_t>(core::raw(doc.key_of(other))), *second);
}

Task<void> run_vertex(Context& ctx, bool fillet, Value given, std::optional<core::Point2> at_pt)
{
    const core::Document& doc = ctx.document();

    // WHICH OBJECT AND WHICH CORNER, three ways in:
    //   1. named, `nesne`, and then the corner by `nokta`;
    //   2. one object highlighted — the same;
    //   3. neither: ONE CLICK ON THE CORNER, which names both.
    //
    // The third is the one a hand uses and it did not exist: the menu entries
    // answered "PAH için nesne belirtilmedi" and stopped, so the tool could be
    // started only by somebody who already knew the object's key. A corner is a
    // place on the drawing, and pointing at it is the whole question.
    // The object and, when the dispatcher took it from a click, the corner:
    // `run_corner` has already decided this is a polyline's own corner.
    Cornered target;
    if (!corner_of(ctx, given, target)) co_return;

    if (!at_pt) {
        at_pt = co_await ctx.point("nokta", "İşlem yapılacak köşe");
        if (!at_pt) co_return;
    }

    const std::optional<std::size_t> at = core::nearest_corner(target.pts, target.closed, *at_pt);
    if (!at) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Burada iki kenarın buluştuğu bir köşe yok. Açık bir çizginin uçları köşe "
                   "değildir; iki kenarın buluştuğu bir noktayı gösterin.");
        co_return;
    }

    // THE SIZE, TYPED OR SHOWN, with the cut drawn at the cursor. The preview is
    // `core::cut_path_corner` — the function the command is about to call — at
    // the cursor's distance from the corner, and a click there hands that
    // distance back as the answer (`pick_distance`).
    PointOptions guide;
    guide.rubber_band    = true;
    guide.rubber_origin  = target.pts[*at];
    guide.rubber_shape   = RubberShape::Corner;
    guide.rubber_payload = core::encode_corner_preview(core::CornerPreview{
        .key    = target.id,
        .at     = static_cast<std::uint32_t>(*at),
        .fillet = fillet,
    });
    guide.pick_distance  = true;
    auto size            = co_await ctx.number(fillet ? "yaricap" : "mesafe",
                                    fillet ? "Yuvarlatma yarıçapı (metre) — yazın ya da gösterin"
                                                      : "Köşeden kesilecek mesafe (metre) — yazın ya da "
                                                        "gösterin",
                                    std::move(guide));
    if (!size) co_return;

    const core::Mm want = core::mm_round(*size * static_cast<double>(core::kMmPerMetre));
    auto cut            = core::cut_path_corner(target.path, *at, want, fillet);
    if (!cut) {
        ctx.refuse(cut.error());
        co_return;
    }

    // THE SAME OBJECT, its corner cut: its key, layer, style, attributes and
    // followers stay (model.md R4, R9b, R28).
    const bool was_plain = doc.entities().kind[target.slot] == core::kPolylineKind;
    if (!rewrite_path(ctx, target.slot, cut.value().path)) co_return;

    // ONE SHAPE ON EVERY ROAD: `nesne=1` typed parses to a number, a click and
    // a script's `[1]` to a list, and the journal is to be the same line from
    // all three (Article 6.4). The declared shape is a selection, so a list.
    ctx.record("nesne", Value::ids({target.id}));
    ctx.record("nokta", Value::point(*at_pt));
    ctx.record(fillet ? "yaricap" : "mesafe", Value::number(*size));
    if (!fillet)
        ctx.echo("Köşeye pah kırıldı.");
    else if (was_plain && target.closed)
        ctx.echo("Köşe yuvarlatıldı (yarıçap " + metres_text(want) +
                 "); alan yaylı kenarlı bir alan oldu.");
    else if (was_plain)
        ctx.echo("Köşe yuvarlatıldı (yarıçap " + metres_text(want) +
                 "); çizgi yaylı çoklu çizgi oldu.");
    else
        ctx.echo("Köşe yuvarlatıldı (yarıçap " + metres_text(want) + ").");
}

Task<void> run_chamfer(Context& ctx)
{
    co_await run_corner(ctx, false);
}

Task<void> run_fillet(Context& ctx)
{
    co_await run_corner(ctx, true);
}

} // namespace

KENTOS_COMMAND(chamfer)
{
    return CommandSpec{
        .id       = "core.chamfer",
        .names    = {"PAH", "CHAMFER", "PH"},
        .title    = "Pah",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::at_least(1),
                      "Köşesi kesilecek nesne; iki nesne verilirse aralarındaki köşe; hepsi=evet "
                      "ile bir ya da daha çok nesne"}
                    .en("object"),
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak "
                      "parçası; hepsi=evet ise verilmez"}
                    .en("point"),
                Param::number("mesafe", Arity::exactly(1),
                              "Köşeden her iki kenar boyunca kesilecek mesafe, metre")
                    .en("distance"),
                Param{"ikinci_nokta", ParamKind::Point, Arity::optional(),
                      "İki nesnede ikincinin kalacak parçası"}
                    .en("second_point"),
                Param::number("ikinci_mesafe", Arity::optional(),
                              "İki çizgi arasında ikinci çizgi boyunca kesilecek mesafe, metre; "
                              "verilmezse mesafe")
                    .measured_in("m")
                    .en("second_distance"),
                Param::boolean("budama", Arity::optional(),
                               "İki nesnede nesneler köşeye kadar kısaltılıp uzatılsın mı; "
                               "varsayılan evet")
                    .en("trim"),
                Param::boolean("hepsi", Arity::optional(),
                               "Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe "
                               "atlanır")
                    .en("every_corner"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary =
            "Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar).",
        .run     = &run_chamfer,
        .targets = Targets::Lines | Targets::Faces | Targets::Curves,
    };
}

KENTOS_COMMAND(fillet)
{
    return CommandSpec{
        .id       = "core.fillet",
        .names    = {"YUVARLA", "FILLET", "YV"},
        .title    = "Yuvarla",
        .category = Category::Modify,
        .params =
            {
                Param{"nesne", ParamKind::Selection, Arity::at_least(1),
                      "Köşesi yuvarlatılacak nesne; iki nesne verilirse aralarındaki köşe; "
                      "hepsi=evet ile bir ya da daha çok nesne"}
                    .en("object"),
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak "
                      "parçası; hepsi=evet ise verilmez"}
                    .en("point"),
                Param::number("yaricap", Arity::exactly(1),
                              "Yuvarlatma yarıçapı, metre; iki nesnede 0 keskin köşe")
                    .en("radius"),
                Param{"ikinci_nokta", ParamKind::Point, Arity::optional(),
                      "İki nesnede ikincinin kalacak parçası"}
                    .en("second_point"),
                Param::boolean("budama", Arity::optional(),
                               "İki nesnede nesneler teğet noktalarına kadar kısaltılıp "
                               "uzatılsın mı; varsayılan evet")
                    .en("trim"),
                Param::boolean("hepsi", Arity::optional(),
                               "Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe "
                               "atlanır")
                    .en("every_corner"),
            },
        .undo  = UndoPolicy::SingleTransaction,
        .flags = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Bir köşeyi ya da iki nesne (çizgi, yay) arasındaki köşeyi verilen yarıçapta "
                   "yayla yuvarlatır; 0 yarıçap keskin köşe kurar.",
        .run     = &run_fillet,
        .targets = Targets::Lines | Targets::Faces | Targets::Curves,
    };
}

} // namespace kentos::command
