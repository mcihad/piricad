// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/snap.hpp"

#include "piricad/core/trig.hpp"

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
///
/// The three CONSTRUCTED modes sit at the bottom, below even YAKIN, and that
/// placement is the rule that keeps them safe: a point this engine invented must
/// never win against a point the drawing actually contains.
constexpr std::uint16_t kPriority[] = {
    SnapEndpoint, SnapIntersection, SnapMidpoint, SnapCenter,    SnapPerpendicular,
    SnapNearest,  SnapApparent,     SnapParallel, SnapExtension,
};

Mm abs_mm(Mm v) noexcept
{
    return v < 0 ? -v : v;
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
        SnapEndpoint, SnapMidpoint, SnapCenter,    SnapIntersection, SnapPerpendicular, SnapNearest,
        SnapGrid,     SnapPolar,    SnapExtension, SnapParallel,     SnapApparent,      SnapNone,
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
    case SnapExtension: return "uzanti";
    case SnapParallel: return "paralel";
    case SnapApparent: return "uzatilmis_kesisim";
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
    case SnapExtension: return "uzantı";
    case SnapParallel: return "paralel";
    case SnapApparent: return "uzatılmış kesişim";
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

    const Mm dx_mm = p.x - base.x;
    const Mm dy_mm = p.y - base.y;
    if (dx_mm == 0 && dy_mm == 0) return p;

    // NO atan2, NO libm. §7.3 and core.md R9 ask for bit-identical results across
    // three platforms, and IEEE-754 does not specify the transcendentals: two
    // conforming libms may answer one ulp apart, which is invisible on screen and
    // fatal to a golden fixture.
    //
    // The candidate directions depend only on `step_udeg`, never on where the user
    // aimed, so the nearest one can be found by COMPARING against each candidate
    // rather than by measuring the aim's own angle. The comparison is a dot
    // product: the candidate whose unit vector has the largest projection onto the
    // aim is the nearest one, and that is a multiply and an add.
    //
    // `core::sin_cos_udeg` supplies the unit vectors and uses nothing but the four
    // operations IEEE-754 does specify exactly, so every machine walks the same
    // comparisons in the same order and reaches the same candidate.
    const double dx = static_cast<double>(dx_mm);
    const double dy = static_cast<double>(dy_mm);

    const std::int64_t candidates = kUDegFullCircle / step_udeg;
    if (candidates <= 0) return p;

    double best_projection = -1.0e308;
    SinCos best_direction{};

    for (std::int64_t k = 0; k < candidates; ++k) {
        const std::int64_t angle = k * step_udeg;
        const SinCos dir         = sin_cos_udeg(angle);

        // Projection of the aim onto this direction. Largest wins; ties go to the
        // lower angle because the loop runs upward and the comparison is strict,
        // which makes the choice reproducible rather than dependent on order.
        const double projection = dx * dir.cos + dy * dir.sin;
        if (projection > best_projection) {
            best_projection = projection;
            best_direction  = dir;
        }
    }

    // The distance is preserved exactly along the chosen direction: the point
    // slides around the circle rather than moving toward or away from the base,
    // which is what polar tracking means to a surveyor holding a measured length.
    //
    // Distance uses the exact integer length the geometry module computes, not a
    // sqrt of doubles, so a 100 m aim lands at exactly 100 m.
    const double distance = static_cast<double>(segment_length(base, p));

    return Point2{base.x + mm_round(distance * best_direction.cos),
                  base.y + mm_round(distance * best_direction.sin)};
}

// ----------------------------------------------------------------- engine ---

SnapResult snap(const Document& doc, const SnapQuery& q)
{
    SnapResult result;
    result.point = q.aim;

    // ---- 1. object snap ----
    std::uint16_t object_modes = q.modes & SnapObjectMask;

    // A constructed mode with no reach is a mode that cannot see the edge it
    // would build from, so it is switched off here rather than searching for
    // nothing — the same contract `grid_step` and `polar_step` already keep.
    if (q.reach <= 0)
        object_modes = static_cast<std::uint16_t>(object_modes & ~SnapConstructedMask);

    if (q.radius > 0 && object_modes != 0) {
        // The APERTURE is what a snap must land inside; the SEARCH BOX is how far
        // the engine looks for the geometry that implies a point. They are the
        // same box for a real feature and a wider one for a constructed point,
        // whose edge is by definition somewhere the cursor is not.
        const Mm look = (object_modes & SnapConstructedMask) != 0 ? q.radius + q.reach : q.radius;

        const Box2 aperture{q.aim.x - q.radius, q.aim.y - q.radius, q.aim.x + q.radius,
                            q.aim.y + q.radius};
        const Box2 box{q.aim.x - look, q.aim.y - look, q.aim.x + look, q.aim.y + look};
        const double limit = static_cast<double>(q.radius) * static_cast<double>(q.radius);

        std::vector<EntityId> candidates;
        pick_candidates(doc, box, candidates);

        Best best[sizeof(kPriority) / sizeof(kPriority[0])]{};
        std::vector<NearSegment> near;
        near.reserve(kMaxNearSegments);

        // Edges kept for the constructed modes. Separate from `near`, which holds
        // only edges that actually cross the aperture, because an extension and an
        // apparent corner are built from edges that do not.
        std::vector<NearSegment> reachable;
        if ((object_modes & SnapConstructedMask) != 0) reachable.reserve(kMaxNearSegments);

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

                    if ((object_modes & SnapConstructedMask) != 0 &&
                        reachable.size() < kMaxNearSegments)
                        reachable.push_back(NearSegment{a, b, e});

                    // UZANTI: the aim dropped onto the edge's LINE, accepted only
                    // where the edge itself is not. Inside the span it would be
                    // the same point YAKIN already offers, at a priority that
                    // would then depend on which mode happened to be on.
                    if ((object_modes & SnapExtension) != 0) {
                        Point2 on_line{};
                        double t = 0.0;
                        if (closest_point_on_line(a, b, q.aim, on_line, t) && (t < 0.0 || t > 1.0))
                            offer(best[priority_index(SnapExtension)], on_line, e, q.aim, limit);
                    }

                    // Everything below is about the edge itself, so an edge that
                    // only the widened box reached has nothing more to say.
                    if (!segment_touches_box(a, b, aperture)) continue;

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

        // PARALEL: a ray leaving the last point in the direction of a nearby edge.
        //
        // This is how a çekme mesafesi, a road edge and an ifraz cut are actually
        // drawn — "the same bearing as that boundary, from here" — and the whole
        // value of it is that the user never has to read the bearing off anything.
        // The distance from the base is preserved, exactly as polar tracking
        // preserves it, so a measured length typed after the direction is the
        // length that lands.
        if ((object_modes & SnapParallel) != 0 && q.has_base) {
            for (const NearSegment& edge : reachable) {
                const double dx   = static_cast<double>(edge.b.x - edge.a.x);
                const double dy   = static_cast<double>(edge.b.y - edge.a.y);
                const double len2 = dx * dx + dy * dy;
                if (len2 <= 0.0) continue;

                const double px = static_cast<double>(q.aim.x - q.base.x);
                const double py = static_cast<double>(q.aim.y - q.base.y);

                const double t = (px * dx + py * dy) / len2;
                offer(best[priority_index(SnapParallel)],
                      Point2{q.base.x + mm_round(t * dx), q.base.y + mm_round(t * dy)}, edge.entity,
                      q.aim, limit);
            }
        }

        // UZATILMIŞ KESİŞİM: the corner two boundaries WOULD make.
        //
        // Accepted only where the crossing is off at least one of the two edges;
        // on both it is a real crossing and KESİŞİM owns it at a higher priority.
        // This is the mode an ifraz needs when the corner monument is gone and the
        // two surviving edges are all there is to rebuild it from.
        if ((object_modes & SnapApparent) != 0) {
            for (std::size_t i = 0; i + 1 < reachable.size(); ++i) {
                for (std::size_t j = i + 1; j < reachable.size(); ++j) {
                    // Two edges meeting at a shared vertex already have their
                    // corner, and it is an endpoint.
                    if (reachable[i].a == reachable[j].a || reachable[i].a == reachable[j].b ||
                        reachable[i].b == reachable[j].a || reachable[i].b == reachable[j].b)
                        continue;

                    Point2 corner{};
                    double t = 0.0;
                    double u = 0.0;
                    if (!line_intersection(reachable[i].a, reachable[i].b, reachable[j].a,
                                           reachable[j].b, corner, t, u))
                        continue;

                    const bool on_first  = t >= 0.0 && t <= 1.0;
                    const bool on_second = u >= 0.0 && u <= 1.0;
                    if (on_first && on_second) continue;

                    offer(best[priority_index(SnapApparent)], corner, reachable[i].entity, q.aim,
                          limit);
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
