// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: BUDA and UZAT on lines, arcs and circles. See trim_curve.hpp.
#include "kentos_cad/core/trim_curve.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>

namespace kentos::core {
namespace {

constexpr std::int64_t kTurn = kUDegFullCircle;

std::int64_t wrap(std::int64_t a) noexcept
{
    a %= kTurn;
    return a < 0 ? a + kTurn : a;
}

std::int64_t angle_of(Point2 centre, Point2 p) noexcept
{
    return atan2_udeg(p.y - centre.y, p.x - centre.x);
}

/// Every meet of `target` with every edge, in order along the target, one per
/// point — two edges meeting the target where they meet each other are one cut.
std::vector<PathCrossing> all_crossings(const CurvePath& target, std::span<const CurvePath> edges)
{
    std::vector<PathCrossing> all;
    for (const CurvePath& edge : edges) {
        std::vector<PathCrossing> some = path_crossings(target, edge);
        all.insert(all.end(), some.begin(), some.end());
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

bool empty(const CurvePath& p) noexcept
{
    return p.pieces.empty();
}

} // namespace

Result<CurveTrim> trim_curve(const CurvePath& target, std::span<const CurvePath> edges, Point2 pick)
{
    if (target.pieces.empty())
        return err(ErrorCode::InvalidArgument, "Budanacak nesnenin çizilecek bir parçası yok.");

    const std::vector<PathCrossing> cuts = all_crossings(target, edges);
    const PathPlace at                   = place_of(target, pick);
    const PathPlace start                = path_start(target);
    const PathPlace end                  = path_end(target);

    // A CLICK ON A CUT NAMES NO PIECE: the two either side of it both touch it,
    // and taking one of them would be a guess.
    const Point2 clicked = point_at(target, at);
    for (const PathCrossing& c : cuts)
        if (distance_squared(c.point, clicked) <= 1.0)
            return err(ErrorCode::InvalidArgument,
                       "Tıklanan nokta bir kesişimin tam üstünde; hangi parçanın atılacağını "
                       "söylemek için parçanın içine tıklayın.");

    // The cuts either side of the click.
    const PathCrossing* prev = nullptr;
    const PathCrossing* next = nullptr;
    for (const PathCrossing& c : cuts) {
        if (comes_before(c.at, at))
            prev = &c;
        else if (comes_before(at, c.at) && next == nullptr)
            next = &c;
    }

    CurveTrim out;
    if (!target.closed) {
        if (prev == nullptr && next == nullptr)
            return err(ErrorCode::InvalidArgument,
                       "Bu nesneyi sınırlardan hiçbiri kesmiyor; atılacak bir parça yok.");
        out.removed =
            sub_path(target, prev != nullptr ? prev->at : start, next != nullptr ? next->at : end);
        if (prev != nullptr) out.kept.push_back(sub_path(target, start, prev->at));
        if (next != nullptr) out.kept.push_back(sub_path(target, next->at, end));
    } else {
        // A CLOSED SHAPE IS CUT IN TWO PLACES OR NOT AT ALL: one cut opens a
        // ring without taking anything out of it.
        if (cuts.size() < 2)
            return err(ErrorCode::InvalidArgument,
                       "Kapalı bir şekil ancak iki yerinden kesilince budanır; sınırlar onu " +
                           std::to_string(cuts.size()) + " yerinden kesiyor.");
        // Round the seam: before the first cut is after the last.
        if (prev == nullptr) prev = &cuts.back();
        if (next == nullptr) next = &cuts.front();
        out.removed = sub_path(target, prev->at, next->at);
        out.kept.push_back(sub_path(target, next->at, prev->at));
    }

    std::erase_if(out.kept, empty);
    if (empty(out.removed) || out.kept.empty())
        return err(ErrorCode::InvalidArgument,
                   "Tıklanan parça bütün nesne; budanınca geriye bir şey kalmıyor. Silmek için SİL "
                   "kullanın.");
    return out;
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

    // AN ARC'S END ROUND ITS CIRCLE, to the nearest edge past it — never so far
    // that the arc would come round onto itself.
    const std::int64_t room = kTurn - tip.sweep_udeg;
    std::optional<std::int64_t> best;
    Point2 reached{};
    for (const CurvePath& edge : edges)
        for (const PathPiece& piece : edge.pieces)
            for (const Point2 q : circle_meets(tip, piece)) {
                const std::int64_t d =
                    at_start ? wrap(angle_of(tip.centre, tip.from) - angle_of(tip.centre, q))
                             : wrap(angle_of(tip.centre, q) - angle_of(tip.centre, tip.to));
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
    reach.sweep_udeg = *best;
    if (at_start) {
        reach.from  = reached;
        reach.to    = tip.from;
        edited.from = reached;
    } else {
        reach.from = tip.to;
        reach.to   = reached;
        edited.to  = reached;
    }
    edited.sweep_udeg += *best;
    out.added.pieces.push_back(reach);
    return out;
}

std::vector<CurvePath> cutting_edges(const Document& doc, EntityId target, bool every,
                                     std::span<const std::int64_t> keys)
{
    std::vector<CurvePath> out;
    const auto add = [&out, &doc, target](EntityId e) {
        if (e == target || e == kNoEntity) return;
        if (auto path = path_of(doc, e)) out.push_back(std::move(*path));
    };

    if (!every) {
        for (const std::int64_t key : keys)
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

std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide)
{
    // version, flags, key count, keys — little-endian as the machine writes them,
    // because the bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(2 + sizeof(std::uint32_t) +
                                    guide.keys.size() * sizeof(std::int64_t));
    bytes[0] = 2;
    bytes[1] = static_cast<std::uint8_t>((guide.extend ? 1U : 0U) | (guide.every ? 2U : 0U));
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
    if (bytes.size() < head || bytes[0] != 2 || bytes[1] > 3)
        return err(ErrorCode::InvalidArgument, "Budama önizlemesinin baytları tanınmıyor.");
    TrimGuide guide;
    guide.extend        = (bytes[1] & 1U) != 0;
    guide.every         = (bytes[1] & 2U) != 0;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 2, sizeof(count));
    if (bytes.size() != head + count * sizeof(std::int64_t))
        return err(ErrorCode::InvalidArgument, "Budama önizlemesinin baytları tanınmıyor.");
    guide.keys.resize(count);
    if (count != 0)
        std::memcpy(guide.keys.data(), bytes.data() + head, count * sizeof(std::int64_t));
    return guide;
}

} // namespace kentos::core
