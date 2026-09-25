// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: BUDA and UZAT on lines, arcs and circles. See trim_curve.hpp.
#include "kentos_cad/core/trim_curve.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>

namespace kentos::core {
namespace {

constexpr std::int64_t kTurn = kUDegFullCircle;

/// How far a carried-on edge runs past its end: a thousand kilometres, more
/// than any TM3 zone is wide, so an implied boundary reaches whatever in the
/// drawing it could be meant to cut.
constexpr double kCarryMm = 1.0e9;

std::int64_t wrap(std::int64_t a) noexcept
{
    a %= kTurn;
    return a < 0 ? a + kTurn : a;
}

std::int64_t angle_of(Point2 centre, Point2 p) noexcept
{
    return atan2_udeg(p.y - centre.y, p.x - centre.x);
}

bool empty(const CurvePath& p) noexcept
{
    return p.pieces.empty();
}

/// `b` carried on past itself, away from `a`, by `kCarryMm`.
Point2 beyond(Point2 a, Point2 b) noexcept
{
    const auto dx    = static_cast<double>(b.x - a.x);
    const auto dy    = static_cast<double>(b.y - a.y);
    const double len = std::sqrt((dx * dx) + (dy * dy));
    if (len <= 0.0) return b;
    return Point2{b.x + mm_round(dx / len * kCarryMm), b.y + mm_round(dy / len * kCarryMm)};
}

/// An arc piece as its whole circle, seamed at its east point; an ellipse
/// piece as its whole ellipse, seamed where it starts. A spline has no curve
/// past its ends and is left as it is.
void whole_circle(PathPiece& p) noexcept
{
    if (p.kind == PathPiece::Kind::Spline) return;
    if (p.kind == PathPiece::Kind::Ellipse) {
        p.sweep_udeg = kTurn;
        p.to         = p.from;
        return;
    }
    p.from       = Point2{p.centre.x + p.radius, p.centre.y};
    p.to         = p.from;
    p.sweep_udeg = kTurn;
}

/// `edge` running on past both its ends (`TrimGuide::carry`). A closed edge has
/// no end and is left as it is.
CurvePath carried(CurvePath edge)
{
    if (edge.closed || edge.pieces.empty()) return edge;
    PathPiece& first = edge.pieces.front();
    PathPiece& last  = edge.pieces.back();
    if (&first == &last && first.kind == PathPiece::Kind::Segment) {
        const Point2 a = first.from;
        const Point2 b = first.to;
        first.from     = beyond(b, a);
        first.to       = beyond(a, b);
        return edge;
    }
    if (first.kind == PathPiece::Kind::Segment)
        first.from = beyond(first.to, first.from);
    else
        whole_circle(first);
    if (last.kind == PathPiece::Kind::Segment)
        last.to = beyond(last.from, last.to);
    else
        whole_circle(last);
    return edge;
}

} // namespace

std::vector<PathCrossing> cuts_of(const CurvePath& target, std::span<const CurvePath> edges,
                                  bool* unresolved)
{
    std::vector<PathCrossing> all;
    for (const CurvePath& edge : edges) {
        const PathMeets some = path_meets(target, edge);
        all.insert(all.end(), some.crossings.begin(), some.crossings.end());
        if (unresolved != nullptr && some.unresolved) *unresolved = true;
    }
    std::ranges::sort(
        all, [](const PathCrossing& a, const PathCrossing& b) { return comes_before(a.at, b.at); });
    std::vector<PathCrossing> unique;
    for (const PathCrossing& c : all)
        if (unique.empty() || distance_squared(unique.back().point, c.point) > 1.0)
            unique.push_back(c);
    if (target.closed && unique.size() >= 2 &&
        distance_squared(unique.front().point, unique.back().point) <= 1.0)
        unique.pop_back();
    return unique;
}

Result<CurveTrim> cut_pieces(const CurvePath& target, std::vector<PathCrossing> cuts,
                             std::span<const PathPlace> marks, bool keep)
{
    if (target.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Budanacak nesnenin çizilecek bir parçası yok.");

    // THE BOUNDS OF THE PIECES, in order. An open path runs from its start
    // through every cut to its end; a closed one from cut to cut and round
    // again, and it needs two of them — one cut opens a ring without taking
    // anything out of it.
    std::vector<PathPlace> bounds;
    if (!target.closed) {
        if (cuts.empty())
            return err(ErrorCode::InvalidArgument,
                       "Bu nesneyi sınırlardan hiçbiri kesmiyor; atılacak bir parça yok.");
        bounds.push_back(path_start(target));
        for (const PathCrossing& c : cuts)
            bounds.push_back(c.at);
        bounds.push_back(path_end(target));
    } else {
        if (cuts.size() < 2)
            return err(ErrorCode::InvalidArgument,
                       "Kapalı bir şekil ancak iki yerinden kesilince budanır; sınırlar onu " +
                           std::to_string(cuts.size()) + " yerinden kesiyor.");
        for (const PathCrossing& c : cuts)
            bounds.push_back(c.at);
        bounds.push_back(cuts.front().at);
    }
    const std::size_t pieces = bounds.size() - 1;

    // WHICH PIECES A MARK FALLS IN. Strictly inside at a cut — a mark on a cut
    // belongs to neither piece either side of it — but an open path's own start
    // and end belong to its first and last pieces: a click on a line's very end
    // names the end piece. The last piece of a closed path wraps past the seam.
    std::vector<bool> marked(pieces, false);
    for (const PathPlace m : marks)
        for (std::size_t i = 0; i < pieces; ++i) {
            const PathPlace a = bounds[i];
            const PathPlace b = bounds[i + 1];
            if (target.closed && i + 1 == pieces) {
                if (!comes_before(a, m) && !comes_before(m, b)) continue;
            } else {
                const bool after_a  = comes_before(a, m) || (!target.closed && i == 0);
                const bool before_b = comes_before(m, b) || (!target.closed && i + 1 == pieces);
                if (!after_a || !before_b) continue;
            }
            marked[i] = true;
            break;
        }
    if (std::ranges::none_of(marked, [](bool on) { return on; }))
        return err(ErrorCode::InvalidArgument, "Gösterilen yerde atılacak bir parça yok.");

    // WHAT GOES: the marked pieces, or with `keep` every other one. Runs of
    // pieces with one fate are one path each. A closed path is walked from a
    // piece whose fate differs from the one before it, so no run is split at the
    // seam.
    std::vector<bool> goes(pieces);
    for (std::size_t i = 0; i < pieces; ++i)
        goes[i] = marked[i] != keep;

    CurveTrim out;
    std::size_t from = 0;
    if (target.closed) {
        while (from < pieces && goes[from] == goes[(from + pieces - 1) % pieces])
            ++from;
        if (from == pieces) from = 0; ///< one fate for every piece: said below
    }
    for (std::size_t n = 0; n < pieces;) {
        const std::size_t head = (from + n) % pieces;
        std::size_t length     = 1;
        while (n + length < pieces && goes[(from + n + length) % pieces] == goes[head])
            ++length;
        // An open path's last run ends at its end; a closed one's wraps back to
        // the first cut, which `bounds` holds at both ends.
        const std::size_t tail = target.closed ? (head + length) % pieces : head + length;
        CurvePath piece = length == pieces ? target : sub_path(target, bounds[head], bounds[tail]);
        if (!empty(piece)) (goes[head] ? out.removed : out.kept).push_back(std::move(piece));
        n += length;
    }

    if (out.removed.empty())
        return err(ErrorCode::InvalidArgument, "Tıklanan parçanın dışında atılacak bir şey yok.");
    if (out.kept.empty())
        return err(ErrorCode::InvalidArgument,
                   "Tıklanan parça bütün nesne; budanınca geriye bir şey kalmıyor. Silmek için SİL "
                   "kullanın.");
    out.cuts = std::move(cuts);
    return out;
}

Result<CurveTrim> trim_curve(const CurvePath& target, std::span<const CurvePath> edges, Point2 pick,
                             bool keep)
{
    if (target.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Budanacak nesnenin çizilecek bir parçası yok.");

    bool unsettled                 = false;
    std::vector<PathCrossing> cuts = cuts_of(target, edges, &unsettled);
    // A CUT THE SOLVE COULD NOT SETTLE is a cut that may be missing: trimming
    // without it could take a piece the user did not point at. Said, not guessed.
    if (unsettled)
        return err(ErrorCode::ValidationFailed,
                   "Sınırların bu nesneyi nerede kestiği sayısal olarak kesinleştirilemedi; "
                   "budama yapılmadı. Sınırı ya da eğriyi sadeleştirip yeniden deneyin.");
    const PathPlace at = place_of(target, pick);

    // A CLICK ON A CUT NAMES NO PIECE: the two either side of it both touch it,
    // and taking one of them would be a guess.
    const Point2 clicked = point_at(target, at);
    for (const PathCrossing& c : cuts)
        if (distance_squared(c.point, clicked) <= 1.0)
            return err(ErrorCode::InvalidArgument,
                       "Tıklanan nokta bir kesişimin tam üstünde; hangi parçanın atılacağını "
                       "söylemek için parçanın içine tıklayın.");

    const std::array<PathPlace, 1> marks{at};
    return cut_pieces(target, std::move(cuts), marks, keep);
}

Result<CurveExtension> extend_curve(const CurvePath& target, std::span<const CurvePath> edges,
                                    Point2 pick)
{
    if (target.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Uzatılacak nesnenin çizilecek bir parçası yok.");
    if (target.closed)
        return err(ErrorCode::InvalidArgument,
                   "Kapalı bir şeklin ucu yok; uzatılacak bir şey yok.");

    const bool at_start = distance_squared(target.pieces.front().from, pick) <=
                          distance_squared(target.pieces.back().to, pick);
    const PathPiece& tip = at_start ? target.pieces.front() : target.pieces.back();

    CurveExtension out;
    out.extended      = target;
    PathPiece& edited = at_start ? out.extended.pieces.front() : out.extended.pieces.back();

    if (tip.kind == PathPiece::Kind::Segment) {
        // THE LINE PAST ITS END: from the anchored end through the moving one,
        // and the first edge beyond (t > 1).
        const Point2 anchor = at_start ? tip.to : tip.from;
        const Point2 moving = at_start ? tip.from : tip.to;
        std::optional<LineMeet> best;
        for (const CurvePath& edge : edges)
            for (const PathPiece& piece : edge.pieces)
                for (const LineMeet& m : line_meets(anchor, moving, piece))
                    if (m.t > 1.0 + 1e-9 && (!best || m.t < best->t)) best = m;
        if (!best)
            return err(ErrorCode::InvalidArgument,
                       "Bu uç, sınırlara uzatılarak ulaşamıyor: kesişme yok.");
        (at_start ? edited.from : edited.to) = best->point;
        PathPiece reach;
        reach.from = moving;
        reach.to   = best->point;
        out.added.pieces.push_back(reach);
        return out;
    }

    // A SPLINE HAS NO CURVE PAST ITS ENDS: its last knot span ends there, and
    // a continuation is a guess this program does not make.
    if (tip.kind == PathPiece::Kind::Spline)
        return err(ErrorCode::Unsupported,
                   "Spline'ın ucu uzatılamaz: eğri son düğümünde biter, ötesi tanımsızdır. "
                   "Ucundan sınıra bir çizgi çizin.");

    // AN ELLIPTIC ARC'S END ROUND ITS ELLIPSE, as an arc's round its circle:
    // measured in the ellipse's own parameter, the way the arc is walked.
    if (tip.kind == PathPiece::Kind::Ellipse) {
        const bool ccw          = tip.sweep_udeg >= 0;
        const std::int64_t room = kTurn - (ccw ? tip.sweep_udeg : -tip.sweep_udeg);
        const std::int64_t from = tip.start_udeg;
        const std::int64_t to   = tip.start_udeg + tip.sweep_udeg;
        std::optional<std::int64_t> best;
        Point2 reached{};
        for (const CurvePath& edge : edges)
            for (const PathPiece& piece : edge.pieces)
                for (const Point2 q : ellipse_meets(tip, piece)) {
                    const std::int64_t at   = ellipse_parameter_of(tip, q);
                    const std::int64_t back = from - at;
                    const std::int64_t on   = at - to;
                    const std::int64_t d =
                        at_start ? wrap(ccw ? back : -back) : wrap(ccw ? on : -on);
                    if (d <= 0 || d >= room) continue;
                    if (!best || d < *best) {
                        best    = d;
                        reached = q;
                    }
                }
        if (!best)
            return err(ErrorCode::InvalidArgument,
                       "Bu elips yayının ucu, sınırlara elipsi boyunca uzatılarak ulaşamıyor: "
                       "kesişme yok.");
        const std::int64_t step = ccw ? *best : -*best;
        PathPiece reach         = tip;
        if (at_start) {
            reach.start_udeg  = wrap(from - step);
            reach.sweep_udeg  = step;
            reach.from        = reached;
            reach.to          = tip.from;
            edited.start_udeg = wrap(from - step);
            edited.from       = reached;
        } else {
            reach.start_udeg = wrap(to);
            reach.sweep_udeg = step;
            reach.from       = tip.to;
            reach.to         = reached;
            edited.to        = reached;
        }
        edited.sweep_udeg += step;
        out.added.pieces.push_back(reach);
        return out;
    }

    // AN ARC'S END ROUND ITS CIRCLE, to the nearest edge past it — never so far
    // that the arc would come round onto itself — and ON the way it is walked:
    // an arc-polyline edge that bends clockwise carries its end clockwise.
    const bool ccw          = tip.sweep_udeg >= 0;
    const std::int64_t room = kTurn - (ccw ? tip.sweep_udeg : -tip.sweep_udeg);
    std::optional<std::int64_t> best;
    Point2 reached{};
    for (const CurvePath& edge : edges)
        for (const PathPiece& piece : edge.pieces)
            for (const Point2 q : circle_meets(tip, piece)) {
                const std::int64_t back = angle_of(tip.centre, tip.from) - angle_of(tip.centre, q);
                const std::int64_t on   = angle_of(tip.centre, q) - angle_of(tip.centre, tip.to);
                const std::int64_t d = at_start ? wrap(ccw ? back : -back) : wrap(ccw ? on : -on);
                if (d <= 0 || d >= room) continue;
                if (!best || d < *best) {
                    best    = d;
                    reached = q;
                }
            }
    if (!best)
        return err(ErrorCode::InvalidArgument,
                   "Bu yayın ucu, sınırlara çemberi boyunca uzatılarak ulaşamıyor: kesişme yok.");

    PathPiece reach  = tip;
    reach.sweep_udeg = ccw ? *best : -*best;
    if (at_start) {
        reach.from  = reached;
        reach.to    = tip.from;
        edited.from = reached;
    } else {
        reach.from = tip.to;
        reach.to   = reached;
        edited.to  = reached;
    }
    edited.sweep_udeg += ccw ? *best : -*best;
    out.added.pieces.push_back(reach);
    return out;
}

std::vector<CurvePath> cutting_edges(const Document& doc, EntityId target, const TrimGuide& run)
{
    std::vector<CurvePath> out;
    const auto add = [&out, &doc, target, &run](EntityId e) {
        if (e == target || e == kNoEntity) return;
        if (auto path = path_of(doc, e, PathScope::Curves))
            out.push_back(run.carry ? carried(std::move(*path)) : *path);
    };

    if (!run.every) {
        for (const std::int64_t key : run.keys)
            add(doc.slot_of(static_cast<EntityKey>(static_cast<std::uint64_t>(key))));
        return out;
    }

    // EVERY VISIBLE OBJECT the target could reach: its own box, grown by its own
    // size, so an extension that has somewhere to go finds it and a trim is not
    // handed the whole sheet.
    if (target >= doc.entities().size()) return out;
    const Box2 box   = doc.entities().box_of(target);
    const Mm grow    = std::max(box.max_x - box.min_x, box.max_y - box.min_y);
    const Box2 reach = Box2{box.min_x - grow, box.min_y - grow, box.max_x + grow, box.max_y + grow};
    std::vector<EntityId> near;
    pick_candidates(doc, reach, near);
    for (const EntityId e : near)
        add(e);
    return out;
}

FencePlan plan_fence(const Document& doc, std::span<const Point2> fence, const TrimGuide& run)
{
    FencePlan plan;
    if (fence.size() < 2) return plan;

    CurvePath line;
    for (std::size_t i = 0; i + 1 < fence.size(); ++i)
        if (fence[i] != fence[i + 1])
            line.pieces.push_back(PathPiece{.from = fence[i], .to = fence[i + 1]});
    if (line.pieces.empty()) return plan;

    std::vector<EntityId> crossed;
    pick_along_fence(doc, fence, crossed);
    for (const EntityId e : crossed) {
        const std::optional<CurvePath> path = path_of(doc, e, PathScope::Curves);
        const bool area = path && path->closed && doc.entities().kind[e] == kPolylineKind;
        if (!path || area || !doc.editable(e)) {
            ++plan.passed_over;
            continue;
        }
        const std::vector<PathCrossing> meets = path_crossings(*path, line);
        if (meets.empty()) {
            ++plan.passed_over;
            continue;
        }
        const std::vector<CurvePath> edges = cutting_edges(doc, e, run);

        if (!run.extend) {
            std::vector<PathPlace> marks;
            marks.reserve(meets.size());
            for (const PathCrossing& m : meets)
                marks.push_back(m.at);
            bool unsettled = false;
            auto cut       = cut_pieces(*path, cuts_of(*path, edges, &unsettled), marks, run.keep);
            if (unsettled) {
                ++plan.passed_over;
                continue;
            }
            if (!cut) {
                ++plan.passed_over;
                continue;
            }
            plan.edits.push_back(FenceEdit{.target = e, .cut = std::move(cut.value())});
            continue;
        }

        // UZAT: EACH END THE FENCE CROSSES NEAR, the start first. The end is the
        // one nearer the crossing, as a click there would choose it.
        if (path->closed) {
            ++plan.passed_over;
            continue;
        }
        bool start = false;
        bool end   = false;
        for (const PathCrossing& m : meets) {
            const bool nearer_start = distance_squared(path->pieces.front().from, m.point) <=
                                      distance_squared(path->pieces.back().to, m.point);
            (nearer_start ? start : end) = true;
        }
        CurveExtension total{.extended = *path, .added = {}};
        for (const bool at_start : {true, false}) {
            if (!(at_start ? start : end)) continue;
            const Point2 tip =
                at_start ? total.extended.pieces.front().from : total.extended.pieces.back().to;
            auto reach = extend_curve(total.extended, edges, tip);
            if (!reach) continue;
            total.extended = std::move(reach.value().extended);
            total.added.pieces.insert(total.added.pieces.end(), reach.value().added.pieces.begin(),
                                      reach.value().added.pieces.end());
        }
        if (total.added.pieces.empty()) {
            ++plan.passed_over;
            continue;
        }
        plan.edits.push_back(FenceEdit{.target = e, .cut = {}, .reach = std::move(total)});
    }
    return plan;
}

std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide)
{
    // version, flags, key count, keys — little-endian as the machine writes them,
    // because the bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(2 + sizeof(std::uint32_t) +
                                    (guide.keys.size() * sizeof(std::int64_t)));
    bytes[0] = 2;
    bytes[1] = static_cast<std::uint8_t>((guide.extend ? 1U : 0U) | (guide.every ? 2U : 0U) |
                                         (guide.keep ? 4U : 0U) | (guide.carry ? 8U : 0U));
    const auto count = static_cast<std::uint32_t>(guide.keys.size());
    std::memcpy(bytes.data() + 2, &count, sizeof(count));
    if (count != 0)
        std::memcpy(bytes.data() + 2 + sizeof(count), guide.keys.data(),
                    guide.keys.size() * sizeof(std::int64_t));
    return bytes;
}

Result<TrimGuide> decode_trim_guide(std::span<const std::uint8_t> bytes)
{
    const std::size_t head = 2 + sizeof(std::uint32_t);
    if (bytes.size() < head || bytes[0] != 2 || bytes[1] > 15)
        return err(ErrorCode::InvalidArgument, "Budama önizlemesinin baytları tanınmıyor.");
    TrimGuide guide;
    guide.extend        = (bytes[1] & 1U) != 0;
    guide.every         = (bytes[1] & 2U) != 0;
    guide.keep          = (bytes[1] & 4U) != 0;
    guide.carry         = (bytes[1] & 8U) != 0;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 2, sizeof(count));
    if (bytes.size() != head + (count * sizeof(std::int64_t)))
        return err(ErrorCode::InvalidArgument, "Budama önizlemesinin baytları tanınmıyor.");
    guide.keys.resize(count);
    if (count != 0)
        std::memcpy(guide.keys.data(), bytes.data() + head, count * sizeof(std::int64_t));
    return guide;
}

} // namespace kentos::core
