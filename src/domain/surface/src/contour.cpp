// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/domain/surface/contour.hpp"

#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <map>
#include <utility>

#if KENTOS_HAVE_CDT
#include "CDT.h"
#endif

namespace kentos::domain::surface {

bool available() noexcept
{
#if KENTOS_HAVE_CDT
    return true;
#else
    return false;
#endif
}

#if !KENTOS_HAVE_CDT

core::Result<std::vector<Contour>> trace_contours(const std::vector<Level>&, core::Mm)
{
    return core::err(core::ErrorCode::Unsupported,
                     "Üçgenleme bu yapıda yok; eş yükselti eğrisi çizilemez. "
                     "KENTOS_WITH_CDT=ON ile derleyin.");
}

#else

namespace {

/// A point's exact identity, for the maps below. Millimetres are integers, so
/// this is an exact key and needs no tolerance.
using Key = std::pair<core::Mm, core::Mm>;

Key key_of(core::Point2 p)
{
    return {p.x, p.y};
}

/// The point where `level` crosses the edge from `a` to `b`.
///
/// COMPUTED FROM THE EDGE, NOT FROM THE TRIANGLE. The two triangles sharing an
/// edge must produce the identical millimetre for it or the runs will not chain,
/// so the caller orders the two endpoints canonically before calling and this
/// does nothing that depends on which triangle asked.
core::Point2 crossing(const Level& a, const Level& b, core::Mm level)
{
    const core::Mm span = b.height - a.height;
    if (span == 0) return a.at;

    // Integer-first: the ratio is formed once, in double, and applied to the two
    // integer offsets. Rounding is `mm_round`, the same one every other coordinate
    // in the program goes through.
    const double t = static_cast<double>(level - a.height) / static_cast<double>(span);
    return core::Point2{a.at.x + core::mm_round(static_cast<double>(b.at.x - a.at.x) * t),
                        a.at.y + core::mm_round(static_cast<double>(b.at.y - a.at.y) * t)};
}

/// One contour segment, keyed by its two ends so the chainer can join them.
struct Segment
{
    core::Point2 from{};
    core::Point2 to{};
};

} // namespace

core::Result<std::vector<Contour>> trace_contours(const std::vector<Level>& points,
                                                  core::Mm interval)
{
    if (interval <= 0)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Eş yükselti aralığı sıfırdan büyük olmalı.");
    if (points.size() < 3)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Yüzey en az üç kotlu nokta ister. Verilen: " +
                             std::to_string(points.size()) + ".");

    // ---- duplicates first, and the heights follow them ----
    //
    // Two levelling points at the same coordinate is an ordinary field mistake —
    // a station occupied twice, a point read twice — and a triangulator handed
    // one produces a degenerate mesh rather than an error. Removed here rather
    // than by the library so the height column stays aligned with the vertices.
    //
    // The FIRST reading wins: a later duplicate is the one somebody re-entered,
    // and choosing by rule rather than by whichever the container returns keeps
    // the answer reproducible.
    std::vector<Level> unique;
    unique.reserve(points.size());
    {
        std::map<Key, std::size_t> seen;
        for (const Level& p : points)
            if (seen.emplace(key_of(p.at), unique.size()).second) unique.push_back(p);
    }

    if (unique.size() < 3)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Yüzey en az üç FARKLI kotlu nokta ister; verilen noktaların " +
                             std::to_string(unique.size()) + " tanesi ayrı yerde.");

    // ---- triangulate ----
    CDT::Triangulation<double> cdt;
    std::vector<CDT::V2d<double>> vertices;
    vertices.reserve(unique.size());
    for (const Level& p : unique)
        vertices.push_back(CDT::V2d<double>{static_cast<double>(p.at.x),
                                            static_cast<double>(p.at.y)});

    cdt.insertVertices(vertices);
    cdt.eraseSuperTriangle();

    if (cdt.triangles.empty())
        return core::err(core::ErrorCode::ValidationFailed,
                         "Bu noktalardan yüzey kurulamadı: hepsi aynı doğru üzerinde olabilir.");

    // ---- the range of levels to trace ----
    core::Mm lowest  = points.front().height;
    core::Mm highest = points.front().height;
    for (const Level& p : points) {
        lowest  = std::min(lowest, p.height);
        highest = std::max(highest, p.height);
    }

    // Whole multiples of the interval, so a 1 m contour set lands on 845, 846,
    // 847 — the numbers a plan sheet prints — rather than on the lowest point
    // plus one metre.
    const core::Mm first = ((lowest / interval) + (lowest % interval > 0 ? 1 : 0)) * interval;
    if (first > highest) return std::vector<Contour>{};

    const auto levels = static_cast<std::size_t>((highest - first) / interval) + 1;
    if (levels > kMaxContours)
        return core::err(core::ErrorCode::InvalidArgument,
                         "Bu aralık " + std::to_string(levels) +
                             " kot verir; çok küçük bir aralık verilmiş olabilir.");

    // ---- cut every triangle at every level that crosses it ----
    std::map<core::Mm, std::vector<Segment>> by_level;

    for (const CDT::Triangle& tri : cdt.triangles) {
        const std::size_t ia = tri.vertices[0];
        const std::size_t ib = tri.vertices[1];
        const std::size_t ic = tri.vertices[2];
        if (ia >= unique.size() || ib >= unique.size() || ic >= unique.size()) continue;

        const Level* v[3]{&unique[ia], &unique[ib], &unique[ic]};
        const std::size_t index[3]{ia, ib, ic};

        core::Mm lo = std::min({v[0]->height, v[1]->height, v[2]->height});
        core::Mm hi = std::max({v[0]->height, v[1]->height, v[2]->height});

        for (core::Mm level = first; level <= highest; level += interval) {
            if (level < lo || level > hi) continue;

            // Where the level crosses each of the three edges. An edge whose two
            // ends straddle the level contributes one point.
            std::vector<core::Point2> hits;
            for (int e = 0; e < 3; ++e) {
                const int j = (e + 1) % 3;

                const core::Mm ha = v[e]->height;
                const core::Mm hb = v[j]->height;
                if ((ha < level && hb < level) || (ha > level && hb > level)) continue;
                if (ha == hb) continue; // an edge lying IN the level plane

                // CANONICAL ORDER, so the neighbouring triangle interpolates the
                // same edge from the same end and lands on the same millimetre.
                const bool forward = index[e] < index[j];
                hits.push_back(forward ? crossing(*v[e], *v[j], level)
                                       : crossing(*v[j], *v[e], level));
            }

            if (hits.size() == 2 && (hits[0].x != hits[1].x || hits[0].y != hits[1].y))
                by_level[level].push_back(Segment{hits[0], hits[1]});
        }
    }

    // ---- chain the segments into runs ----
    std::vector<Contour> out;
    for (auto& [level, segments] : by_level) {
        // An index from each endpoint to the segments that touch it. Exact,
        // because the interpolation above is.
        std::multimap<Key, std::size_t> ends;
        std::vector<bool> used(segments.size(), false);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            ends.emplace(key_of(segments[i].from), i);
            ends.emplace(key_of(segments[i].to), i);
        }

        for (std::size_t seed = 0; seed < segments.size(); ++seed) {
            if (used[seed]) continue;

            Contour run;
            run.height = level;
            used[seed] = true;
            run.path.push_back(segments[seed].from);
            run.path.push_back(segments[seed].to);

            // Grow from both ends until nothing joins.
            for (int direction = 0; direction < 2; ++direction) {
                bool grew = true;
                while (grew) {
                    grew                 = false;
                    const core::Point2 tip = direction == 0 ? run.path.back() : run.path.front();
                    const auto range       = ends.equal_range(key_of(tip));

                    for (auto it = range.first; it != range.second; ++it) {
                        const std::size_t s = it->second;
                        if (used[s]) continue;

                        const Segment& seg = segments[s];
                        const bool from_tip =
                            seg.from.x == tip.x && seg.from.y == tip.y;
                        const core::Point2 next = from_tip ? seg.to : seg.from;

                        used[s] = true;
                        if (direction == 0)
                            run.path.push_back(next);
                        else
                            run.path.insert(run.path.begin(), next);
                        grew = true;
                        break;
                    }
                }
            }

            if (run.path.size() >= 2) {
                run.closed = run.path.front().x == run.path.back().x &&
                             run.path.front().y == run.path.back().y;
                if (run.closed) run.path.pop_back(); // the closing vertex is implied
                if (run.path.size() >= 2) out.push_back(std::move(run));
            }

            if (out.size() > kMaxContours)
                return core::err(core::ErrorCode::InvalidArgument,
                                 "Bu aralık çok fazla eğri veriyor (" +
                                     std::to_string(kMaxContours) +
                                     " üstü); daha büyük bir aralık verin.");
        }
    }

    return out;
}

#endif

} // namespace kentos::domain::surface
