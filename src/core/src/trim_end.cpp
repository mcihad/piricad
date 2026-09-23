// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: trimming and extending a line's end. See trim_end.hpp.
#include "kentos_cad/core/trim_end.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <cmath>
#include <cstring>

namespace kentos::core {
namespace {

/// Where the infinite line through `a`-`b` crosses the segment `p0`-`p1`'s own
/// line, as a fraction along `p0`-`p1`. False when they are parallel.
bool cut_fraction(Point2 p0, Point2 p1, Point2 a, Point2 b, double& t)
{
    const double dx = mm_to_metres(p1.x - p0.x);
    const double dy = mm_to_metres(p1.y - p0.y);
    const double ex = mm_to_metres(b.x - a.x);
    const double ey = mm_to_metres(b.y - a.y);

    const double denom = dx * ey - dy * ex;
    if (std::abs(denom) < 1e-12) return false; // parallel: no crossing to move to

    const double wx = mm_to_metres(a.x - p0.x);
    const double wy = mm_to_metres(a.y - p0.y);

    t = (wx * ey - wy * ex) / denom;
    return true;
}

/// The squared distance from `pick` to the nearest point of `run`.
double reach_of(std::span<const Point2> run, Point2 pick)
{
    double best = -1.0;
    for (std::size_t i = 0; i + 1 < run.size(); ++i) {
        const Point2 f = closest_point_on_segment(run[i], run[i + 1], pick);
        const double d = distance_squared(f, pick);
        if (best < 0.0 || d < best) best = d;
    }
    return best;
}

} // namespace

Result<TrimEnd> trim_end(std::span<const Point2> target, std::span<const Point2> boundary,
                         Point2 pick, bool extend)
{
    if (target.size() < 2 || boundary.size() < 2)
        return err(ErrorCode::InvalidArgument, "Çizgilerden birinin kenarı yok.");

    const bool at_start =
        distance_squared(target.front(), pick) <= distance_squared(target.back(), pick);

    // The segment that moves is the one at that end.
    const std::size_t seg = at_start ? 0 : target.size() - 2;
    const Point2 p0       = at_start ? target[1] : target[seg];     // the anchored end
    const Point2 p1       = at_start ? target[0] : target[seg + 1]; // the end that moves

    // Every boundary segment is a candidate; the crossing nearest the moving end
    // is the one meant, because that is the first boundary the line reaches.
    bool found  = false;
    double best = 0.0;
    Point2 cut{};
    for (std::size_t i = 0; i + 1 < boundary.size(); ++i) {
        double t = 0.0;
        if (!cut_fraction(p0, p1, boundary[i], boundary[i + 1], t)) continue;

        const Point2 hit{p0.x + mm_round(static_cast<double>(p1.x - p0.x) * t),
                         p0.y + mm_round(static_cast<double>(p1.y - p0.y) * t)};

        // The crossing has to be ON the boundary segment, not merely on its line:
        // a line trimmed to where two boundaries would have met had they been
        // longer is a line trimmed to nothing that exists.
        const Point2 foot = closest_point_on_segment(boundary[i], boundary[i + 1], hit);
        if (distance_squared(foot, hit) > 1.0) continue;

        // Trimming pulls the end back (t < 1), extending pushes it out (t > 1).
        if (extend ? (t <= 1.0) : (t >= 1.0 || t <= 0.0)) continue;

        if (!found || std::abs(t - 1.0) < std::abs(best - 1.0)) {
            found = true;
            best  = t;
            cut   = hit;
        }
    }

    if (!found)
        return err(ErrorCode::InvalidArgument,
                   extend ? "Bu uç, sınır çizgisine uzatılarak ulaşamıyor: kesişme yok."
                          : "Bu uç sınır çizgisini kesmiyor; budanacak bir şey yok.");

    TrimEnd out;
    out.run.assign(target.begin(), target.end());
    (at_start ? out.run.front() : out.run.back()) = cut;
    out.cut                                       = cut;
    out.at_start                                  = at_start;
    out.changed = extend ? std::vector<Point2>{p1, cut} : std::vector<Point2>{cut, p1};
    return out;
}

bool picks_first(std::span<const Point2> a, std::span<const Point2> b, Point2 pick)
{
    return reach_of(a, pick) <= reach_of(b, pick);
}

std::vector<std::uint8_t> encode_trim_guide(const TrimGuide& guide)
{
    // version, flags, the two keys — little-endian as the machine writes them,
    // because the bytes never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(2 + 2 * sizeof(std::int64_t));
    bytes[0] = 1;
    bytes[1] = static_cast<std::uint8_t>((guide.paired ? 1U : 0U) | (guide.extend ? 2U : 0U));
    std::memcpy(bytes.data() + 2, &guide.first, sizeof(guide.first));
    std::memcpy(bytes.data() + 2 + sizeof(guide.first), &guide.second, sizeof(guide.second));
    return bytes;
}

Result<TrimGuide> decode_trim_guide(std::span<const std::uint8_t> bytes)
{
    TrimGuide guide;
    if (bytes.size() != 2 + 2 * sizeof(std::int64_t) || bytes[0] != 1 || bytes[1] > 3)
        return err(ErrorCode::InvalidArgument, "Budama önizlemesinin baytları tanınmıyor.");
    guide.paired = (bytes[1] & 1U) != 0;
    guide.extend = (bytes[1] & 2U) != 0;
    std::memcpy(&guide.first, bytes.data() + 2, sizeof(guide.first));
    std::memcpy(&guide.second, bytes.data() + 2 + sizeof(guide.first), sizeof(guide.second));
    return guide;
}

} // namespace kentos::core
