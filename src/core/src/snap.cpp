// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/snap.hpp"

#include "piricad/core/document.hpp"
#include "piricad/core/pick.hpp"

#include <cmath>
#include <vector>

namespace piricad::core {
namespace {

/// One segment near the aim, kept with the entity it belongs to so the marker can
/// name it and so two segments of the same ring are not mistaken for a crossing.
struct NearSegment
{
    Point2 a{};
    Point2 b{};
    EntityId entity{kNoEntity};
};

/// How many nearby segments the intersection search will consider. The pairwise
/// test is O(n²) and runs on every mouse move, so it is bounded rather than
/// trusted to stay small: 48 segments is 1128 pairs, well inside the frame budget
/// (§10.1), and no aperture a user can set holds more real segments than that.
constexpr std::size_t kMaxNearSegments = 48;

/// Priority, highest first. A corner beats a crossing beats a middle: this is the
/// order every CAD user already has in their fingers, and it is what makes an
/// aperture that covers several features still land where they meant.
constexpr std::uint16_t kPriority[] = {
    SnapEndpoint, SnapIntersection, SnapMidpoint, SnapCenter, SnapPerpendicular, SnapNearest,
};

Mm abs_mm(Mm v) noexcept
{
    return v < 0 ? -v : v;
}

/// Round-half-away-from-zero for an angle in micro-degrees. `mm_round` is the
/// coordinate helper (core.md R20) and is deliberately not reused for an angle:
/// same arithmetic, different unit, and confusing the two is how a bug hides.
std::int64_t round_udeg(double v) noexcept
{
    return v >= 0.0 ? static_cast<std::int64_t>(v + 0.5) : static_cast<std::int64_t>(v - 0.5);
}

Mm snap_axis(Mm v, Mm step) noexcept
{
    if (step <= 0) return v;
    const Mm q = v >= 0 ? (v + step / 2) / step : -((-v + step / 2) / step);
    return q * step;
}

/// Area centroid of one ring, or false for a ring that encloses nothing.
///
/// Accumulated in `double` rather than int64: the cross products of a hundred
/// vertices of a 20 m parcel fit, but a 5 km ring in TM3 does not, and an
/// overflowing centroid would put the snap marker in another province. The
/// summation order is the ring's own vertex order, which is fixed by model.md
/// R11, so the result is the same on every platform (`-ffp-contract=off`).
bool ring_centroid(const RingGeometry& geometry, std::uint32_t ring, Point2& out)
{
    if (geometry.ring_role[ring] == RingRole::Open) return false;

    const auto xs = geometry.ring_xs(ring);
    const auto ys = geometry.ring_ys(ring);
    if (xs.size() < 3) return false;

    // Translated to the first vertex, exactly as RingGeometry::ring_area does.
    const Mm ox = xs[0];
    const Mm oy = ys[0];

    double twice_area = 0.0;
    double cx         = 0.0;
    double cy         = 0.0;

    for (std::size_t i = 0; i < xs.size(); ++i) {
        const std::size_t j = (i + 1) % xs.size();
        const double x0     = static_cast<double>(xs[i] - ox);
        const double y0     = static_cast<double>(ys[i] - oy);
        const double x1     = static_cast<double>(xs[j] - ox);
        const double y1     = static_cast<double>(ys[j] - oy);
        const double cross  = x0 * y1 - x1 * y0;
        twice_area += cross;
        cx += (x0 + x1) * cross;
        cy += (y0 + y1) * cross;
    }

    if (twice_area == 0.0) return false;

    const double scale = 1.0 / (3.0 * twice_area);
    out                = Point2{ox + mm_round(cx * scale), oy + mm_round(cy * scale)};
    return true;
}

/// Best candidate found for each priority level, plus the entity that produced it.
struct Best
{
    Point2 point{};
    EntityId entity{kNoEntity};
    double distance{-1.0};
};

void offer(Best& best, Point2 candidate, EntityId owner, Point2 aim, double limit)
{
    const double d = distance_squared(candidate, aim);
    if (d > limit) return;
    if (best.distance >= 0.0 && d >= best.distance) return;
    best.point    = candidate;
    best.entity   = owner;
    best.distance = d;
}

std::size_t priority_index(std::uint16_t bit)
{
    for (std::size_t i = 0; i < sizeof(kPriority) / sizeof(kPriority[0]); ++i)
        if (kPriority[i] == bit) return i;
    return sizeof(kPriority) / sizeof(kPriority[0]);
}

} // namespace

// ------------------------------------------------------------------ names ---

const std::uint16_t* snap_mode_bits()
{
    static const std::uint16_t bits[] = {
        SnapEndpoint, SnapMidpoint, SnapCenter, SnapIntersection, SnapPerpendicular,
        SnapNearest,  SnapGrid,     SnapPolar,  SnapNone,
    };
    return bits;
}

const char* snap_mode_id(std::uint16_t single_bit)
{
    switch (single_bit) {
    case SnapEndpoint: return "uc";
    case SnapMidpoint: return "orta";
    case SnapCenter: return "merkez";
    case SnapIntersection: return "kesisim";
    case SnapPerpendicular: return "dik";
    case SnapNearest: return "yakin";
    case SnapGrid: return "izgara";
    case SnapPolar: return "kutupsal";
    case SnapOrtho: return "dik_mod";
    default: return "yok";
    }
}

const char* snap_mode_label(std::uint16_t single_bit)
{
    switch (single_bit) {
    case SnapEndpoint: return "uç nokta";
    case SnapMidpoint: return "orta nokta";
    case SnapCenter: return "merkez";
    case SnapIntersection: return "kesişim";
    case SnapPerpendicular: return "dik ayak";
    case SnapNearest: return "en yakın";
    case SnapGrid: return "ızgara";
    case SnapPolar: return "kutupsal";
    case SnapOrtho: return "dik mod";
    default: return "yok";
    }
}

// ------------------------------------------------------------------ rules ---

Point2 apply_grid(Point2 p, Mm step) noexcept
{
    if (step <= 0) return p;
    return Point2{snap_axis(p.x, step), snap_axis(p.y, step)};
}

Point2 apply_ortho(Point2 base, Point2 p) noexcept
{
    const Mm dx = p.x - base.x;
    const Mm dy = p.y - base.y;
    // Ties go to the horizontal, so the choice is total and reproducible.
    return abs_mm(dx) >= abs_mm(dy) ? Point2{p.x, base.y} : Point2{base.x, p.y};
}

Point2 apply_polar(Point2 base, Point2 p, std::int64_t step_udeg) noexcept
{
    if (step_udeg <= 0) return p;

    const double dx       = static_cast<double>(p.x - base.x);
    const double dy       = static_cast<double>(p.y - base.y);
    const double distance = std::sqrt(dx * dx + dy * dy);
    if (distance == 0.0) return p;

    // std::atan2/sin/cos are the one place in this engine that is not exact. They
    // are within one ulp on every conforming libm, and one ulp of a direction
    // cosine over a ten-kilometre radius is a nanometre — far below the stored
    // millimetre, so the rounded result agrees across platforms in practice. It
    // is stated here rather than hidden because §7.3 asks for bit-identity and
    // this is the single approximation the input aids contain. Polar tracking is
    // off by default, and the point it produces is journalled as an exact
    // integer, so a replay never re-derives it.
    const double degrees = std::atan2(dy, dx) * (180.0 / 3.14159265358979323846);

    std::int64_t udeg = round_udeg(degrees * static_cast<double>(kUDegPerDegree));
    udeg %= kUDegFullCircle;
    if (udeg < 0) udeg += kUDegFullCircle;

    const std::int64_t steps   = (udeg + step_udeg / 2) / step_udeg;
    const std::int64_t snapped = (steps * step_udeg) % kUDegFullCircle;

    const double radians = udeg_to_radians(snapped);
    return Point2{base.x + mm_round(distance * std::cos(radians)),
                  base.y + mm_round(distance * std::sin(radians))};
}

// ----------------------------------------------------------------- engine ---

SnapResult snap(const Document& doc, const SnapQuery& q)
{
    SnapResult result;
    result.point = q.aim;

    // ---- 1. object snap ----
    const std::uint16_t object_modes = q.modes & SnapObjectMask;
    if (q.radius > 0 && object_modes != 0) {
        const Box2 box{q.aim.x - q.radius, q.aim.y - q.radius, q.aim.x + q.radius,
                       q.aim.y + q.radius};
        const double limit = static_cast<double>(q.radius) * static_cast<double>(q.radius);

        std::vector<EntityId> candidates;
        pick_candidates(doc, box, candidates);

        Best best[sizeof(kPriority) / sizeof(kPriority[0])]{};
        std::vector<NearSegment> near;
        near.reserve(kMaxNearSegments);

        const EntityTable& entities  = doc.entities();
        const RingGeometry& geometry = doc.geometry();

        for (EntityId e : candidates) {
            const RingSpan span = geometry.rings_of(entities.slot[e]);

            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                const auto xs = geometry.ring_xs(r);
                const auto ys = geometry.ring_ys(r);
                if (xs.empty()) continue;

                if ((object_modes & SnapCenter) != 0) {
                    Point2 centre{};
                    if (ring_centroid(geometry, r, centre))
                        offer(best[priority_index(SnapCenter)], centre, e, q.aim, limit);
                }

                if ((object_modes & SnapEndpoint) != 0)
                    for (std::size_t v = 0; v < xs.size(); ++v)
                        offer(best[priority_index(SnapEndpoint)], Point2{xs[v], ys[v]}, e, q.aim,
                              limit);

                if (xs.size() < 2) continue;

                const bool closed          = geometry.ring_role[r] != RingRole::Open;
                const std::size_t n        = xs.size();
                const std::size_t segments = closed ? n : n - 1;

                for (std::size_t v = 0; v < segments; ++v) {
                    const std::size_t w = (v + 1) % n;
                    const Point2 a{xs[v], ys[v]};
                    const Point2 b{xs[w], ys[w]};

                    if (!segment_touches_box(a, b, box)) continue;

                    if ((object_modes & SnapMidpoint) != 0)
                        offer(best[priority_index(SnapMidpoint)],
                              Point2{(a.x + b.x) / 2, (a.y + b.y) / 2}, e, q.aim, limit);

                    if ((object_modes & SnapNearest) != 0)
                        offer(best[priority_index(SnapNearest)],
                              closest_point_on_segment(a, b, q.aim), e, q.aim, limit);

                    if ((object_modes & SnapPerpendicular) != 0 && q.has_base) {
                        const Point2 foot = closest_point_on_segment(a, b, q.base);
                        offer(best[priority_index(SnapPerpendicular)], foot, e, q.aim, limit);
                    }

                    if ((object_modes & SnapIntersection) != 0 && near.size() < kMaxNearSegments)
                        near.push_back(NearSegment{a, b, e});
                }
            }
        }

        if ((object_modes & SnapIntersection) != 0) {
            for (std::size_t i = 0; i + 1 < near.size(); ++i) {
                for (std::size_t j = i + 1; j < near.size(); ++j) {
                    // Two segments meeting at a shared vertex are adjacent, not
                    // crossing; that point is already an endpoint and outranks
                    // an intersection anyway.
                    if (near[i].a == near[j].a || near[i].a == near[j].b ||
                        near[i].b == near[j].a || near[i].b == near[j].b)
                        continue;

                    Point2 crossing{};
                    if (!segment_intersection(near[i].a, near[i].b, near[j].a, near[j].b, crossing))
                        continue;
                    offer(best[priority_index(SnapIntersection)], crossing, near[i].entity, q.aim,
                          limit);
                }
            }
        }

        for (std::uint16_t bit : kPriority) {
            const Best& b = best[priority_index(bit)];
            if (b.distance < 0.0) continue;
            result.point  = b.point;
            result.mode   = bit;
            result.entity = b.entity;
            return result;
        }
    }

    // ---- 2. direction constraint from the previous point ----
    if (q.has_base) {
        if (q.ortho) {
            result.point       = apply_ortho(q.base, q.aim);
            result.mode        = SnapOrtho;
            result.constrained = true;
            return result;
        }
        if ((q.modes & SnapPolar) != 0 && q.polar_step > 0) {
            result.point       = apply_polar(q.base, q.aim, q.polar_step);
            result.mode        = SnapPolar;
            result.constrained = true;
            return result;
        }
    }

    // ---- 3. the lattice ----
    if ((q.modes & SnapGrid) != 0 && q.grid_step > 0) {
        result.point = apply_grid(q.aim, q.grid_step);
        result.mode  = SnapGrid;
        return result;
    }

    return result;
}

} // namespace piricad::core
