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
#include "kentos_cad/command/construct.hpp"
#include "kentos_cad/command/context.hpp"
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/command/session.hpp"
#include "kentos_cad/command/spec.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/break_run.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/core/trim_curve.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kentos::command {
namespace {

// ------------------------------------------------------------------ BÖL ----

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
        // Both halves remember the face they were cut from (core/lineage.hpp).
        const core::EntityId whole[] = {slot};
        if (auto st = ctx.derive(created.value(), std::span<const core::EntityId>(whole)); !st) {
            ctx.refuse(st.error());
            return false;
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

/// The path of one object BÖL can cut along, or nothing with the reason said:
/// a face is cut in two by a cut line and never opened along its boundary
/// (a parcel is not two lines), and a kind that is not a path — an ellipse, a
/// spline, a point, a caption — says so.
std::optional<core::CurvePath> split_target(const Context& ctx, std::int64_t key,
                                            core::EntityId& slot)
{
    const core::Document& doc = ctx.document();
    if (key <= 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(key) + ". Kimlikler 1'den başlar.");
        return std::nullopt;
    }
    slot = doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
    if (slot == core::kNoEntity || !doc.alive(slot)) {
        ctx.refuse(core::ErrorCode::NotFound,
                   "Nesne bulunamadı veya silinmiş: " + std::to_string(key));
        return std::nullopt;
    }
    core::Polygon face;
    if (doc.entities().kind[slot] == core::kPolylineKind && face_of(doc, slot, face)) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(key) +
                       " bir alan; alan kenarı boyunca açılmaz. İkiye ayırmak için BÖL'ü kesme "
                       "çizgisiyle kullanın (yontem=cizgi).");
        return std::nullopt;
    }
    auto path = core::path_of(doc, slot, core::PathScope::Curves);
    if (!path) {
        ctx.refuse(core::ErrorCode::Unsupported,
                   "Nesne " + std::to_string(key) +
                       " bu yöntemle bölünemiyor; BÖL çizgi, yay, daire, elips, spline ve yaylı "
                       "çoklu çizgide çalışır.");
        return std::nullopt;
    }
    return path;
}

/// Writes one object's pieces back (`replace_with_pieces`) and remembers what
/// its key became. False, having refused, when the document refuses a piece.
bool write_pieces(Context& ctx, core::EntityId slot, const std::vector<core::CurvePath>& pieces,
                  std::vector<PathEdit>& edits)
{
    PathEdit edit;
    if (!replace_with_pieces(ctx, slot, pieces, edit)) return false;
    edits.push_back(std::move(edit));
    return true;
}

/// "3 parça: 4,000 + 9,000 + 7,000 m" — the lengths a surveyor checks against
/// the source, for one object; a count for many.
std::string pieces_said(const std::vector<std::vector<core::CurvePath>>& cut)
{
    std::size_t pieces = 0;
    for (const auto& one : cut)
        pieces += one.size();
    if (cut.size() != 1)
        return std::to_string(cut.size()) + " nesne bölündü, " + std::to_string(pieces) +
               " parça çıktı.";
    std::string out = std::to_string(pieces) + " parça:";
    core::Mm total  = 0;
    for (std::size_t i = 0; i < cut.front().size(); ++i) {
        const core::Mm len = core::path_length(cut.front()[i]);
        total += len;
        out += (i == 0 ? " " : " + ") + metres_text(len);
    }
    return out + " = " + metres_text(total) + " m.";
}

Task<void> run_split(Context& ctx)
{
    // FIVE WAYS TO SAY WHERE, one verb (TODOS C-05): a CUT LINE drawn across
    // (`cizgi`, the default and what an ifraz line is), POINTS on the object
    // (`nokta`), every place the chosen objects CROSS each other (`kesisim`),
    // a DISTANCE from the start (`mesafe`), and EQUAL parts (`esit`). Every
    // piece keeps its kind — an arc cut is arcs — and every piece keeps the
    // layer, style and attributes of what it was cut from.
    std::string how = "cizgi";
    if (const Value v = ctx.argument("yontem"); !v.empty()) how = v.as_text();
    const auto is = [&how](const char* word) { return core::turkish_key_equals(how, word); };

    std::vector<std::int64_t> chosen;
    if (!co_await want_objects(ctx, "nesne",
                               is("nokta") ? "Bölünecek nesneyi seçin, Enter'a basın"
                                           : "Kesilecek nesneleri seçin, Enter'a basın",
                               chosen, is("nokta") ? 1 : 0, "BÖL nesne=1 yontem=esit sayi=3"))
        co_return;

    std::vector<PathEdit> edits;
    std::vector<std::vector<core::CurvePath>> cut; ///< what each written object became

    // THE LEGACY POINT FORM, kept because it is in journals and in scripts: a
    // single open line split at a point that lies on it — now any open path,
    // an arc as well as a line.
    // GUARDED ON `noktalar` BEING ABSENT, not merely on `nokta` being present. A
    // bare trailing token — `BÖL noktalar=10,-5 10,15` — binds positionally to the
    // next unfilled parameter, which is this one, and the command would then take
    // the second half of the cut line for a legacy split point and refuse the
    // face. A cut line named means a cut line meant.
    if (const Value at_arg = ctx.argument("nokta");
        !at_arg.empty() && ctx.argument("noktalar").empty() && is("cizgi")) {
        if (chosen.size() != 1) {
            ctx.refuse(
                core::ErrorCode::InvalidArgument,
                "`nokta` ile tek nesne bölünür; " + std::to_string(chosen.size()) +
                    " nesne verildi. Birden çok nesne için `noktalar` ile kesme çizgisi verin.");
            co_return;
        }
        core::EntityId slot = core::kNoEntity;
        const auto path     = split_target(ctx, chosen.front(), slot);
        if (!path) co_return;
        const core::Point2 at                     = at_arg.as_point();
        const std::vector<core::CurvePath> pieces = split_at_points(*path, {&at, 1});
        if (path->closed || pieces.size() < 2) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       path->closed ? "Kapalı bir şekil tek noktada bölünmez; en az iki bölme "
                                      "noktası verin."
                                    : "Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
            co_return;
        }
        if (!write_pieces(ctx, slot, pieces, edits)) co_return;
        ctx.record("nesne", Value::ids({chosen.front()}));
        ctx.record("nokta", at_arg);
        ctx.report(edits_json(edits));
        ctx.echo("Çizgi ikiye bölündü.");
        co_return;
    }

    if (is("nokta")) {
        // POINTS ON THE OBJECT, as many as the user gives: each is a cut, the
        // pieces it makes drawn as they will be before Enter, and a wrong one
        // taken back with ⌫ like a corner of a line.
        core::EntityId slot = core::kNoEntity;
        const auto path     = split_target(ctx, chosen.front(), slot);
        if (!path) co_return;
        const std::int64_t key = chosen.front();

        Value::Points given;
        if (const Value v = ctx.argument("noktalar"); !v.empty()) given = v.as_points();
        if (given.empty())
            for (;;) {
                auto p = co_await ctx.point(
                    "noktalar",
                    given.empty() ? "Bölme noktası"
                                  : "Sonraki bölme noktası — Enter: böl, ⌫: sonuncuyu geri al",
                    PointOptions{.rubber_band    = true,
                                 .rubber_origin  = given.empty() ? core::Point2{} : given.back(),
                                 .rubber_base    = false,
                                 .rubber_shape   = RubberShape::Split,
                                 .rubber_chain   = given,
                                 .rubber_payload = core::encode_break_guide(core::BreakGuide{key}),
                                 .can_retract    = !given.empty()});
                if (p) {
                    given.push_back(*p);
                    continue;
                }
                if (ctx.took_back() && !given.empty()) {
                    given.pop_back();
                    continue;
                }
                break;
            }
        if (given.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       "BÖL: bölme noktası verilmedi. Nesnenin üstünde bir noktaya tıklayın ya "
                       "da BÖL yontem=nokta noktalar=<nokta> yazın.");
            co_return;
        }
        const std::vector<core::CurvePath> pieces = split_at_points(*path, given);
        if (pieces.size() < 2) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       path->closed ? "Kapalı bir şekil tek noktada bölünmez; en az iki bölme "
                                      "noktası verin."
                                    : "Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.");
            co_return;
        }
        if (!write_pieces(ctx, slot, pieces, edits)) co_return;
        cut.push_back(pieces);
        ctx.record("nesne", Value::ids({key}));
        ctx.record("yontem", Value::text(how));
        ctx.record("noktalar", Value::points(given));
        ctx.report(edits_json(edits));
        ctx.echo(pieces_said(cut));
        co_return;
    }

    if (is("kesisim") || is("mesafe") || is("esit")) {
        // THE ANSWER FIRST, THEN THE CUTS: a distance or a count is asked once
        // for every chosen object.
        double distance    = 0.0;
        std::int64_t parts = 0;
        if (is("mesafe")) {
            auto d = co_await ctx.number("mesafe", "Baştan uzaklık (m)");
            if (!d) co_return;
            distance = *d;
        } else if (is("esit")) {
            auto n = co_await ctx.integer("sayi", "Kaç eşit parça (2–10000)");
            if (!n) co_return;
            parts = *n;
            if (parts < 2 || parts > 10000) {
                ctx.refuse(core::ErrorCode::InvalidArgument,
                           "Parça sayısı 2 ile 10000 arasında olmalı; " + std::to_string(parts) +
                               " verildi.");
                co_return;
            }
        }

        // EVERY PATH READ BEFORE ANY IS WRITTEN: the crossings of a network are
        // the crossings of the drawing as it was, not of its half-cut pieces.
        std::vector<core::EntityId> slots(chosen.size(), core::kNoEntity);
        std::vector<core::CurvePath> paths;
        paths.reserve(chosen.size());
        for (std::size_t i = 0; i < chosen.size(); ++i) {
            auto path = split_target(ctx, chosen[i], slots[i]);
            if (!path) co_return;
            paths.push_back(std::move(*path));
        }

        std::size_t untouched = 0;
        for (std::size_t i = 0; i < paths.size(); ++i) {
            const core::CurvePath& path = paths[i];
            const core::Mm length       = core::path_length(path);
            std::vector<core::PathPlace> places;
            if (is("kesisim")) {
                for (std::size_t j = 0; j < paths.size(); ++j) {
                    if (j == i) continue;
                    for (const core::PathCrossing& c : core::path_crossings(path, paths[j]))
                        places.push_back(c.at);
                }
            } else if (is("mesafe")) {
                const core::Mm at = core::mm_from_metres(distance);
                if (path.closed) {
                    ctx.refuse(core::ErrorCode::InvalidArgument,
                               "Nesne " + std::to_string(chosen[i]) +
                                   " kapalı; başı olmayan bir şekil baştan uzaklıkla bölünmez. "
                                   "yontem=esit ya da yontem=nokta kullanın.");
                    co_return;
                }
                if (at <= 0 || at >= length) {
                    ctx.refuse(core::ErrorCode::InvalidArgument,
                               "Nesne " + std::to_string(chosen[i]) + " " + metres_text(length) +
                                   " m uzunluğunda; bölme uzaklığı 0 ile " + metres_text(length) +
                                   " m arasında olmalı.");
                    co_return;
                }
                places.push_back(core::place_at_length(path, at));
            } else {
                const auto n = static_cast<std::size_t>(parts);
                for (std::size_t k = path.closed ? 0 : 1; k < n; ++k)
                    places.push_back(core::place_at_length(
                        path,
                        static_cast<core::Mm>(static_cast<double>(length) * static_cast<double>(k) /
                                              static_cast<double>(n))));
            }
            const std::vector<core::CurvePath> pieces = core::split_path(path, places);
            if (pieces.size() < 2) {
                ++untouched;
                continue;
            }
            if (!write_pieces(ctx, slots[i], pieces, edits)) co_return;
            cut.push_back(pieces);
        }
        if (cut.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       is("kesisim") ? "Seçilen nesneler birbirini hiçbir yerde kesmiyor."
                                     : "Seçilen nesnelerin hiçbiri bölünemedi.");
            co_return;
        }
        ctx.record("nesne", Value::ids(chosen));
        ctx.record("yontem", Value::text(how));
        if (is("mesafe")) ctx.record("mesafe", Value::number(distance));
        if (is("esit")) ctx.record("sayi", Value::integer(parts));
        ctx.report(edits_json(edits));
        std::string said = pieces_said(cut);
        if (untouched != 0)
            said += "\n  " + std::to_string(untouched) +
                    (is("kesisim") ? " nesneyi başka bir nesne kesmiyor." : " nesne bölünmedi.");
        ctx.echo(said);
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
    core::CurvePath stroke;
    stroke.pieces.push_back(core::PathPiece{.from = *first, .to = *second});

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
        if (doc.entities().kind[slot] == core::kPolylineKind && face_of(doc, slot, face)) {
            const std::size_t before = cut_faces;
            if (!cut_face(ctx, slot, face, *first, *second, cut_faces)) co_return;
            if (cut_faces == before) ++untouched;
            continue;
        }

        // A LINE, AN ARC, A CIRCLE OR A BENT POLYLINE: cut wherever the stroke
        // crosses it — every crossing, not the first — and each piece keeps its
        // kind.
        const auto path = core::path_of(doc, slot, core::PathScope::Curves);
        if (!path) {
            ctx.refuse(core::ErrorCode::Unsupported,
                       "Nesne " + std::to_string(raw) +
                           " bölünemiyor; BÖL çizgi, yay, daire, elips, spline, yaylı çoklu "
                           "çizgi ve alanlarla çalışır.");
            co_return;
        }
        std::vector<core::PathPlace> places;
        for (const core::PathCrossing& c : core::path_crossings(*path, stroke))
            places.push_back(c.at);
        const std::vector<core::CurvePath> pieces = core::split_path(*path, places);
        if (pieces.size() < 2) {
            ++untouched;
            continue;
        }
        if (!write_pieces(ctx, slot, pieces, edits)) co_return;
        cut.push_back(pieces);
        ++cut_lines;
    }

    if (cut_lines == 0 && cut_faces == 0) {
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Kesme çizgisi seçilen nesnelerin hiçbirinden geçmiyor.");
        co_return;
    }

    ctx.record("nesne", Value::ids(chosen));
    ctx.record("noktalar", Value::points({*first, *second}));
    if (!edits.empty()) ctx.report(edits_json(edits));

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

/// The ids a selection argument holds, whichever shape it arrived in.
std::vector<std::int64_t> ids_of(const Value& v)
{
    std::vector<std::int64_t> out = v.as_ids();
    if (out.empty() && v.kind() == Value::Kind::Int) out.push_back(v.as_int());
    return out;
}

/// Writes a trim into the document: what stays of `slot` in place, every
/// further piece as a new object drawn like it, and a circle — whose pieces are
/// arcs, a different kind — replaced by them (`replace_with_pieces`).
bool apply_trim(Context& ctx, core::EntityId slot, const core::CurveTrim& cut)
{
    PathEdit edit;
    return replace_with_pieces(ctx, slot, cut.kept, edit);
}

/// A flag argument, false when absent.
bool flag(const Context& ctx, std::string_view name)
{
    const Value v = ctx.argument(name);
    return !v.empty() && v.as_bool();
}

/// Shared body. THE CUTTING EDGES first — named (`sinir`), highlighted, or with
/// neither every object near the one clicked, which is how every CAD's quick
/// trim reads an empty selection — and then THE PIECES: one click each, the
/// piece clicked goes (BUDA) or the end clicked reaches the nearest edge (UZAT),
/// on a line, an arc or a circle; or all at once, every piece a drawn fence
/// crosses (`yontem=çit`). `tut` keeps the piece named and takes the rest;
/// `uzanti` lets a boundary that stops short cut along its own run. Every edit
/// lands in the same transaction, so the whole run is one undo step (C-04).
Task<void> run_cut(Context& ctx, bool extend)
{
    const char* verb          = extend ? "UZAT" : "BUDA";
    const core::Document& doc = ctx.document();
    Bus& bus                  = ctx.session().bus();

    core::TrimGuide guide;
    guide.extend = extend;
    guide.keep   = !extend && flag(ctx, "tut");
    guide.carry  = flag(ctx, "uzanti");
    if (flag(ctx, "hepsi")) {
        guide.every = true;
    } else if (const Value bound = ctx.argument("sinir"); !bound.empty()) {
        guide.keys = ids_of(bound);
    } else {
        for (const core::EntityKey k : bus.selection().keys())
            guide.keys.push_back(static_cast<std::int64_t>(core::raw(k)));
        guide.every = guide.keys.empty();
    }
    const auto invalid = [&ctx](std::span<const std::int64_t> keys) {
        const auto bad = std::ranges::find_if(keys, [](std::int64_t key) { return key <= 0; });
        if (bad == keys.end()) return false;
        ctx.refuse(core::ErrorCode::InvalidArgument,
                   "Geçersiz nesne kimliği: " + std::to_string(*bad) + ". Kimlikler 1'den başlar.");
        return true;
    };
    if (invalid(guide.keys)) co_return;
    for (const std::int64_t key : guide.keys) {
        const core::EntityId e =
            doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
        if (e == core::kNoEntity || !doc.alive(e)) {
            ctx.refuse(core::ErrorCode::NotFound,
                       "Sınır nesnesi bulunamadı veya silinmiş: " + std::to_string(key));
            co_return;
        }
    }

    // WHAT WAS DONE, in the words a replay reads: the edges as ids — or `hepsi`,
    // which a replay re-reads from the drawing it is replayed into, exactly as the
    // run read it.
    const auto record_edges = [&ctx, &guide] {
        if (guide.every)
            ctx.record("hepsi", Value::boolean(true));
        else
            ctx.record("sinir", Value::ids(guide.keys));
    };

    // ---- A FENCE: every piece it crosses, planned before anything changes ----
    std::vector<core::Point2> fence;
    if (const Value given = ctx.argument("cit"); !given.empty()) fence = given.as_points();
    const Value how = ctx.argument("yontem");
    const bool fencing =
        !fence.empty() || (!how.empty() && core::turkish_key_equals(how.as_text(), "çit"));
    if (fencing) {
        if (fence.empty()) {
            // DRAWN, corner by corner, with the whole edit on the canvas before
            // Enter applies it — the fence and every piece it takes.
            auto first = co_await ctx.point(
                "cit", extend
                           ? "Çitin ilk noktası — uzatılacak uçların yanından geçen bir çizgi"
                           : "Çitin ilk noktası — atılacak parçaların üzerinden geçen bir çizgi");
            if (first) fence.push_back(*first);
            while (!fence.empty()) {
                auto next = co_await ctx.point(
                    "cit", "Çitin sonraki noktası — Enter: uygula",
                    PointOptions{.rubber_band    = true,
                                 .rubber_origin  = fence.back(),
                                 .rubber_shape   = RubberShape::TrimFence,
                                 .rubber_chain   = fence,
                                 .rubber_payload = core::encode_trim_guide(guide)});
                if (!next) break;
                fence.push_back(*next);
            }
        }
        if (fence.size() < 2) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       std::string(verb) + ": çit en az iki noktadan oluşur. Çitin köşelerini " +
                           "tıklayın ya da " + verb + " cit=<nokta> <nokta> yazın.");
            co_return;
        }

        const core::FencePlan plan = core::plan_fence(doc, fence, guide);
        if (plan.edits.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                       extend ? "Çit, sınırlara uzatılabilecek bir uçtan geçmiyor."
                              : "Çit, sınırların kestiği bir parçadan geçmiyor.");
            co_return;
        }
        std::size_t done = 0;
        for (const core::FenceEdit& edit : plan.edits) {
            if (extend) {
                if (!write_path(ctx, edit.target, edit.reach.extended)) co_return;
                done += edit.reach.added.pieces.size();
            } else {
                if (!apply_trim(ctx, edit.target, edit.cut)) co_return;
                done += edit.cut.removed.size();
            }
        }

        ctx.record("yontem", Value::text("çit"));
        ctx.record("cit", Value::points(fence));
        record_edges();
        std::string said = extend ? std::to_string(done) + " uç sınıra uzatıldı"
                                  : std::to_string(done) + " parça budandı";
        said += " (" + std::to_string(plan.edits.size()) + " nesnede).";
        if (plan.passed_over != 0)
            said += "\n  " + std::to_string(plan.passed_over) +
                    (extend ? " nesne atlandı: ucu sınırlara ulaşmıyor ya da bu tür uzatılmaz."
                            : " nesne atlandı: sınırlar onu kesmiyor ya da bu tür budanmaz.");
        ctx.echo(said);
        co_return;
    }

    // ---- CLICKS: one edit each ----------------------------------------------
    // The object is the one named for this click, or the one under it — the same
    // pick a click selects with — and the edges are the run's.
    std::vector<std::int64_t> targets;
    std::vector<core::Point2> picks;
    std::size_t done = 0;
    const auto one   = [&](core::Point2 pick, std::int64_t named) -> bool {
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
                why = "Tıklanan yerde uzatılacak bir nesne yok. Bir çizginin, yayın ya da elips "
                        "yayının ucuna tıklayın.";
            else
                why = "Tıklanan yerde budanacak bir nesne yok. Bir çizginin, yayın, dairenin, "
                        "elipsin ya da spline'ın atılacak parçasına tıklayın.";
            ctx.refuse(core::ErrorCode::NotFound, std::move(why));
            return false;
        }
        const auto key = static_cast<std::int64_t>(core::raw(doc.key_of(slot)));
        if (const auto st = doc.editable(slot); !st) {
            ctx.refuse(st.error());
            return false;
        }
        const core::KindId kind = doc.entities().kind[slot];
        const std::optional<core::CurvePath> path =
            core::path_of(doc, slot, core::PathScope::Curves);
        if (!path) {
            ctx.refuse(core::ErrorCode::Unsupported,
                         "Nesne " + std::to_string(key) +
                             " bu komutun işleyebileceği bir tür değil; " + verb +
                             " çizgi, yay, daire, elips ve spline'da çalışır.");
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
        const std::vector<core::CurvePath> edges = core::cutting_edges(doc, slot, guide);
        if (edges.empty()) {
            ctx.refuse(core::ErrorCode::InvalidArgument,
                         std::string(verb) +
                             (extend ? " için ulaşılacak sınır yok: " : " için kesecek sınır yok: ") +
                             (guide.every ? "tıklanan nesnenin yakınında başka bir çizgi, yay, "
                                            "daire, elips ya da spline yok."
                                          : "sınır olarak verilen nesneler tıklanan nesnenin kendisi "
                                            "ya da çizgi, yay, daire, elips veya spline değil."));
            return false;
        }

        if (extend) {
            auto reach = core::extend_curve(*path, edges, pick);
            if (!reach) {
                ctx.refuse(reach.error());
                return false;
            }
            if (!write_path(ctx, slot, reach.value().extended)) return false;
            ++done;
        } else {
            auto cut = core::trim_curve(*path, edges, pick, guide.keep);
            if (!cut) {
                ctx.refuse(cut.error());
                return false;
            }
            if (!apply_trim(ctx, slot, cut.value())) return false;
            done += cut.value().removed.size();
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
        const char* ask = "Atılacak parçaya tıklayın — Enter: bitir";
        if (extend)
            ask = "Uzatılacak uca tıklayın — Enter: bitir";
        else if (guide.keep)
            ask = "Kalacak parçaya tıklayın — Enter: bitir";
        while (auto at = co_await ctx.point(
                   "nokta", ask,
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

    // Each click with the object it edited, so a replay acts on the same ones.
    record_edges();
    ctx.record("nesne", Value::ids(targets));
    ctx.record("nokta", Value::points(picks));
    ctx.echo(extend ? std::to_string(done) + " uç sınıra uzatıldı."
                    : std::to_string(done) + " parça budandı.");
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
                Param::points("noktalar", Arity{0, 0xFFFFFFFFu},
                              "cizgi: kesme çizgisinin iki noktası · nokta: nesnenin üstündeki "
                              "bölme noktaları")
                    .en("points"),
                // THE LEGACY FORM, kept because it is written into journals and
                // into scripts already: one open line split at a point on it.
                // Nothing collects it interactively any more.
                Param{"nokta", ParamKind::Point, Arity::optional(),
                      "Bölme noktası (tek çizgi; eski biçim)"}
                    .en("point"),
                Param::choice("yontem", Arity::optional(),
                              {"cizgi", "nokta", "kesisim", "mesafe", "esit"},
                              "cizgi: çizilen kesme çizgisinden · nokta: nesnenin üstündeki "
                              "noktalardan · kesisim: seçilenlerin birbirini kestiği yerlerden · "
                              "mesafe: baştan verilen uzaklıktan · esit: eşit parçalara")
                    .en("method"),
                Param::number("mesafe", Arity::optional(), "mesafe: baştan uzaklık (m)")
                    .measured_in("m")
                    .en("distance"),
                Param::integer_range("sayi", Arity::optional(), 2, 10000, "esit: kaç eşit parça")
                    .en("count"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Nesneleri bir kesme çizgisiyle, üstündeki noktalardan, kesişimlerinden, "
                   "baştan bir uzaklıktan ya da eşit parçalara böler; yaylar yay kalır.",
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
                Param::choice("yontem", Arity::optional(), {"tıkla", "çit"},
                              "Parçalar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da "
                              "çizilen bir çitle")
                    .en("method"),
                Param::points("cit", Arity{0, 0xFFFFFFFFu},
                              "Çitin köşeleri; çitin geçtiği her parça budanır")
                    .en("fence"),
                Param::boolean("tut", Arity::optional(),
                               "Gösterilen parça kalır; iki yanındaki kesimlerin dışında kalan "
                               "gider")
                    .en("keep"),
                Param::boolean("uzanti", Arity::optional(),
                               "Sınırlar kendi yolunda uzatılmış sayılır; nesneye yetişmeyen bir "
                               "sınır da keser")
                    .en("carry_edges"),
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
                Param::choice("yontem", Arity::optional(), {"tıkla", "çit"},
                              "Uçlar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da "
                              "çizilen bir çitle")
                    .en("method"),
                Param::points("cit", Arity{0, 0xFFFFFFFFu},
                              "Çitin köşeleri; çitin yanından geçtiği her uç uzatılır")
                    .en("fence"),
                Param::boolean("uzanti", Arity::optional(),
                               "Sınırlar kendi yolunda uzatılmış sayılır; ucun doğrultusuna "
                               "yetişmeyen bir sınıra da ulaşılır")
                    .en("carry_edges"),
            },
        .undo    = UndoPolicy::SingleTransaction,
        .flags   = Flags::Interactive | Flags::Scriptable | Flags::AiAccessible,
        .summary = "Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, "
                   "yayınkini çemberi boyunca.",
        .run     = &run_extend,
    };
}

} // namespace kentos::command
