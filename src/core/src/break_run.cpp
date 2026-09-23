// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: breaking an open run. See break_run.hpp.
#include "kentos_cad/core/break_run.hpp"

#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/units.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace kentos::core {
namespace {

/// Where along `run` the point `at` falls: the segment, the fraction along it
/// and the foot itself. False when the run has no segments.
bool locate(std::span<const Point2> run, Point2 at, std::size_t& segment, double& along,
            Point2& foot)
{
    if (run.size() < 2) return false;
    double best = 0.0;
    bool found  = false;
    for (std::size_t i = 0; i + 1 < run.size(); ++i) {
        Point2 candidate{};
        double t = 0.0;
        if (!closest_point_on_line(run[i], run[i + 1], at, candidate, t)) continue;
        const double clamped = std::clamp(t, 0.0, 1.0);
        if (clamped != t) {
            const auto dx = static_cast<double>(run[i + 1].x - run[i].x);
            const auto dy = static_cast<double>(run[i + 1].y - run[i].y);
            candidate =
                Point2{run[i].x + mm_round(clamped * dx), run[i].y + mm_round(clamped * dy)};
        }
        const double d = distance_squared(at, candidate);
        if (!found || d < best) {
            found   = true;
            best    = d;
            segment = i;
            along   = clamped;
            foot    = candidate;
        }
    }
    return found;
}

/// The run up to `(segment, along)`, and the run from it onwards.
void cut_at(std::span<const Point2> run, std::size_t segment, double along, Point2 foot,
            std::vector<Point2>& head, std::vector<Point2>& rest)
{
    head.clear();
    rest.clear();
    for (std::size_t i = 0; i <= segment; ++i)
        head.push_back(run[i]);
    if (along > 0.0) head.push_back(foot);

    if (along < 1.0) rest.push_back(foot);
    for (std::size_t i = segment + 1; i < run.size(); ++i)
        rest.push_back(run[i]);
}

} // namespace

Result<BreakCut> break_run(std::span<const Point2> run, Point2 a, Point2 b)
{
    std::size_t seg_a = 0;
    std::size_t seg_b = 0;
    double at_a       = 0.0;
    double at_b       = 0.0;
    Point2 foot_a{};
    Point2 foot_b{};
    if (!locate(run, a, seg_a, at_a, foot_a) || !locate(run, b, seg_b, at_b, foot_b))
        return err(ErrorCode::InvalidArgument, "Kırılma noktası çizgi üzerinde bulunamadı.");

    // ORDERED ALONG THE LINE, not in the order they were given.
    if (seg_b < seg_a || (seg_b == seg_a && at_b < at_a)) {
        std::swap(seg_a, seg_b);
        std::swap(at_a, at_b);
        std::swap(foot_a, foot_b);
    }

    BreakCut cut;
    cut.first  = foot_a;
    cut.second = foot_b;

    std::vector<Point2> scrap;
    cut_at(run, seg_a, at_a, foot_a, cut.head, scrap);

    // The tail is cut from the SCRAP, whose own indices start at the first cut.
    const std::size_t seg_in_scrap = seg_b - seg_a;
    double at_in_scrap             = at_b;
    if (at_a > 0.0 && seg_b == seg_a) {
        // Both cuts on one segment: the scrap's first segment is what is left of
        // it, so the second fraction is re-measured against that stub.
        const double left = 1.0 - at_a;
        at_in_scrap       = left > 0.0 ? (at_b - at_a) / left : 0.0;
    }
    cut_at(scrap, seg_in_scrap, at_in_scrap, foot_b, cut.gap, cut.tail);

    if (cut.head.size() < 2 && cut.tail.size() < 2)
        return err(ErrorCode::InvalidArgument,
                   "Kırılma çizginin tamamını götürüyor; parça bırakmıyor. Silmek için SİL "
                   "kullanın.");
    return cut;
}

std::vector<std::uint8_t> encode_break_guide(const BreakGuide& guide)
{
    // version, key — little-endian as the machine writes it, because the bytes
    // never leave the process (a prompt to the canvas).
    std::vector<std::uint8_t> bytes(1 + sizeof(guide.key));
    bytes[0] = 1;
    std::memcpy(bytes.data() + 1, &guide.key, sizeof(guide.key));
    return bytes;
}

Result<BreakGuide> decode_break_guide(std::span<const std::uint8_t> bytes)
{
    BreakGuide guide;
    if (bytes.size() != 1 + sizeof(guide.key) || bytes[0] != 1)
        return err(ErrorCode::InvalidArgument, "Kırma önizlemesinin baytları tanınmıyor.");
    std::memcpy(&guide.key, bytes.data() + 1, sizeof(guide.key));
    return guide;
}

} // namespace kentos::core
