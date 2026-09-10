// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/snap.hpp"

#include "kentos_cad/core/trig.hpp"

#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/pick.hpp"

#include <cmath>
#include <vector>

namespace kentos::core {
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
constexpr std::uint32_t kPriority[] = {
    SnapNode,
    SnapInsertion,
    SnapEndpoint,
    SnapIntersection,
    SnapMidpoint,
    SnapCenter,

    // A CENTROID IS DERIVED, so it ranks under every point the drawing states
    // outright. A parcel's centre of area is a number this engine computed; its
    // corner is a number a surveyor measured, and the two must never compete on
    // equal terms.
    SnapCentroid,
    SnapPerpendicular,
    SnapNearest,
    SnapApparent,
    SnapParallel,
    SnapExtension,

    // A guide is a line the USER drew for themselves, so it sits below every
    // point the drawing actually contains — the same rule the constructed modes
    // follow, and for the same reason.
    SnapGuide,
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
/// How wide the surface-normal aid catches, as the TANGENT of its half-angle.
///
/// tan(20°). Twenty degrees either side of the perpendicular is wide enough that
/// a deliberate pull lands in it without aiming, and narrow enough that four
/// fifths of every direction stays the user's own — which is the bargain a snap
/// makes and a lock does not.
constexpr double kNormalCone = 0.36397023426620234;

/// The unit normal is carried as an integer pair scaled by this, because core
/// stores no floating point and a normal has to survive being handed about.
constexpr double kNormalScale = 1000000.0;

/// Whether `kind` is drawn from a TESSELLATED outline rather than from its own
/// vertices or an exact centre-and-radius rule. Today that is the ellipse, whose
/// ring holds a centre and two axis ends — a definition, not a shape — and it is
/// every kind a later phase adds (spline, hatch, dimension). A polyline and a
/// point ARE their vertices; a circle and an arc are answered exactly, from
/// centre and radius, wherever they occur below.
bool outline_kind(KindId kind) noexcept
{
    return kind != kPolylineKind && kind != kPointKind && kind != kCircleKind && kind != kArcKind;
}

/// One drawn chain of an entity: a polyline's ring, or one run of a tessellated
/// outline. `closed` says the segment back to the first vertex is implied.
struct Chain
{
    std::span<const Mm> xs;
    std::span<const Mm> ys;
    bool closed;
};

/// Calls `fn(const Chain&)` for every drawn chain of `e`. A polyline yields its
/// rings; a kind drawn from an outline yields the runs `curve_outline` builds
/// into `scratch` — the SAME tessellation the picture and the pick test use, so
/// what a snap lands on is what the user sees. Snapping to an ellipse used to
/// walk its three stored vertices as if they were edges, and landed on the axes
/// of a shape whose axes are not drawn.
template<class Fn>
void for_each_chain(const Document& doc, EntityId e, EmitBuffer& scratch, Fn&& fn)
{
    const EntityTable& entities  = doc.entities();
    const RingGeometry& geometry = doc.geometry();
    const std::uint32_t slot     = entities.slot[e];

    if (outline_kind(entities.kind[e])) {
        scratch.clear();
        if (entity_outline(doc, e, scratch)) {
            for (std::size_t r = 0; r < scratch.run_total(); ++r)
                fn(Chain{scratch.run_xs(r), scratch.run_ys(r), scratch.run_closed[r] != 0});
            return;
        }
        // A kind this build does not know has no outline of its own and is
        // walked as its stored rings — which is how it is drawn (model.md R26).
    }

    const RingSpan span = geometry.rings_of(slot);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
        fn(Chain{geometry.ring_xs(r), geometry.ring_ys(r),
                 geometry.ring_role[r] != RingRole::Open});
}

/// Where the ray from `base` through `toward` first meets a drawn edge, looking
/// only within `window_radius` of `toward`. False when the ray runs into nothing.
///
/// WHY THIS EXISTS. A perpendicular is almost never drawn into empty space: it is
/// drawn from one boundary ACROSS to another, and where it lands is the answer —
/// a çekme mesafesi ends on the building line, a section runs wall to wall. With
/// only a direction constraint the run stayed perpendicular but stopped wherever
/// the pixel fell; with only an object snap it landed on the far edge and stopped
/// being perpendicular. Neither alone is the measurement.
///
/// CIRCLES AND ARCS ARE NOT WALKED HERE. They store a centre and a handle, not
/// a chord, so treating their two vertices as an edge would intersect a line
/// nobody drew. An ellipse is walked as the outline it is drawn with.
bool ray_meets_edge(const Document& doc, Point2 base, Point2 toward, Mm window_radius, Point2& out,
                    EntityId& hit_entity)
{
    if (base == toward) return false;

    const Box2 window{toward.x - window_radius, toward.y - window_radius, toward.x + window_radius,
                      toward.y + window_radius};

    std::vector<EntityId> candidates;
    pick_candidates(doc, window, candidates);

    const EntityTable& entities = doc.entities();

    const auto limit = static_cast<double>(window_radius) * static_cast<double>(window_radius);
    double best      = limit;
    bool found       = false;
    EmitBuffer outline;

    for (const EntityId e : candidates) {
        if (!doc.alive(e)) continue;
        if (entities.kind[e] == kCircleKind || entities.kind[e] == kArcKind) continue;

        for_each_chain(doc, e, outline, [&](const Chain& chain) {
            const std::size_t n = chain.xs.size();
            if (n < 2) return;
            const std::size_t segments = chain.closed ? n : n - 1;

            for (std::size_t v = 0; v < segments; ++v) {
                const std::size_t w = (v + 1) % n;
                const Point2 a{chain.xs[v], chain.ys[v]};
                const Point2 b{chain.xs[w], chain.ys[w]};
                if (!segment_touches_box(a, b, window)) continue;

                Point2 crossing{};
                double t = 0.0;
                double u = 0.0;
                if (!line_intersection(base, toward, a, b, crossing, t, u)) continue;

                // ON the drawn edge, and AHEAD of the base. A crossing behind the
                // run is the boundary it left, not the one it is going to.
                if (u < 0.0 || u > 1.0 || t <= 0.0) continue;

                const double dx = static_cast<double>(crossing.x - toward.x);
                const double dy = static_cast<double>(crossing.y - toward.y);
                const double d2 = (dx * dx) + (dy * dy);
                if (d2 > best) continue;

                best       = d2;
                out        = crossing;
                hit_entity = e;
                found      = true;
            }
        });
    }

    return found;
}

/// The unit normal of the edge nearest `at`, scaled by `kNormalScale`.
///
/// SEARCHED THE WAY EVERY OTHER SNAP SEARCHES: the same candidate narrowing, the
/// same aperture discipline. False when nothing is within reach, which is what
/// switches the lock off rather than guessing a direction — a perpendicular to no
/// surface is not a constraint, it is an invention.
///
/// A CURVE'S NORMAL IS ITS RADIUS. On a circle or an arc the perpendicular to the
/// surface runs through the centre, and computing it from the two stored vertices
/// would give the normal of a line nobody drew.
bool surface_normal(const Document& doc, Point2 at, Mm reach, Point2& out)
{
    const Box2 box{at.x - reach, at.y - reach, at.x + reach, at.y + reach};

    std::vector<EntityId> candidates;
    pick_candidates(doc, box, candidates);

    const EntityTable& entities  = doc.entities();
    const RingGeometry& geometry = doc.geometry();

    double best = -1.0;
    double bx = 0.0, by = 0.0;
    EmitBuffer outline;

    const auto consider = [&](double dx, double dy, double distance) {
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0) return;
        if (best >= 0.0 && distance >= best) return;
        best = distance;
        bx   = dx / len;
        by   = dy / len;
    };

    for (EntityId e : candidates) {
        if (!entities.visible(e)) continue;

        if (entities.kind[e] == kCircleKind || entities.kind[e] == kArcKind) {
            const std::uint32_t slot = entities.slot[e];
            const Point2 centre = entities.kind[e] == kCircleKind ? circle_centre_of(geometry, slot)
                                                                  : arc_centre_of(geometry, slot);
            const double dx     = static_cast<double>(at.x - centre.x);
            const double dy     = static_cast<double>(at.y - centre.y);
            const double radius = static_cast<double>(entities.kind[e] == kCircleKind
                                                          ? circle_radius_of(geometry, slot)
                                                          : arc_radius_of(geometry, slot));
            const double len    = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0) consider(dx, dy, std::abs(len - radius));
            continue;
        }

        // A polyline's own edges, or the drawn outline of a kind that is not its
        // vertices (an ellipse): the normal of a chord of the picture, never of a
        // definition line.
        for_each_chain(doc, e, outline, [&](const Chain& chain) {
            const std::size_t n = chain.xs.size();
            if (n < 2) return;
            const std::size_t segments = chain.closed ? n : n - 1;

            for (std::size_t v = 0; v < segments; ++v) {
                const std::size_t w = (v + 1) % n;
                const Point2 a{chain.xs[v], chain.ys[v]};
                const Point2 b{chain.xs[w], chain.ys[w]};

                const Point2 foot = closest_point_on_segment(a, b, at);
                const double d    = std::sqrt(distance_squared(foot, at));
                if (d > static_cast<double>(reach)) continue;

                // The normal is the edge turned a quarter turn; the SIGN is taken
                // from which side `at` is on, so the ray leaves the surface the
                // way the user is standing.
                double nx = -static_cast<double>(b.y - a.y);
                double ny = static_cast<double>(b.x - a.x);
                if (static_cast<double>(at.x - foot.x) * nx +
                        static_cast<double>(at.y - foot.y) * ny <
                    0.0) {
                    nx = -nx;
                    ny = -ny;
                }
                consider(nx, ny, d);
            }
        });
    }

    if (best < 0.0) return false;
    out = Point2{mm_round(bx * kNormalScale), mm_round(by * kNormalScale)};
    return true;
}

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

std::size_t priority_index(std::uint32_t bit)
{
    for (std::size_t i = 0; i < sizeof(kPriority) / sizeof(kPriority[0]); ++i)
        if (kPriority[i] == bit) return i;
    return sizeof(kPriority) / sizeof(kPriority[0]);
}

} // namespace

// ------------------------------------------------------------------ names ---

const std::uint32_t* snap_mode_bits()
{
    static const std::uint32_t bits[] = {
        SnapEndpoint,     SnapMidpoint,      SnapCenter,    SnapCentroid,
        SnapIntersection, SnapPerpendicular, SnapNearest,   SnapNode,
        SnapGrid,         SnapPolar,         SnapExtension, SnapParallel,
        SnapApparent,     SnapGuide,         SnapInsertion, SnapNone,
    };
    return bits;
}

const char* snap_mode_id(std::uint32_t single_bit)
{
    switch (single_bit) {
    case SnapEndpoint: return "uc";
    case SnapMidpoint: return "orta";
    case SnapCenter: return "merkez";
    case SnapCentroid: return "agirlik_merkezi";
    case SnapIntersection: return "kesisim";
    case SnapPerpendicular: return "dik";
    case SnapNearest: return "yakin";
    case SnapNode: return "dugum";
    case SnapGrid: return "izgara";
    case SnapPolar: return "kutupsal";
    case SnapExtension: return "uzanti";
    case SnapParallel: return "paralel";
    case SnapApparent: return "uzatilmis_kesisim";
    case SnapOrtho: return "dik_mod";
    case SnapNormal: return "yuzey_normali";
    case SnapStep: return "adim";
    case SnapGuide: return "kilavuz";
    case SnapInsertion: return "ekleme";
    default: return "yok";
    }
}

const char* snap_mode_label(std::uint32_t single_bit)
{
    switch (single_bit) {
    case SnapEndpoint: return "uç nokta";
    case SnapMidpoint: return "orta nokta";
    case SnapCenter: return "merkez";
    case SnapCentroid: return "ağırlık merkezi";
    case SnapIntersection: return "kesişim";
    case SnapPerpendicular: return "dik ayak";
    case SnapNearest: return "en yakın";
    case SnapNode: return "düğüm";
    case SnapGrid: return "ızgara";
    case SnapPolar: return "kutupsal";
    case SnapExtension: return "uzantı";
    case SnapParallel: return "paralel";
    case SnapApparent: return "uzatılmış kesişim";
    case SnapOrtho: return "dik mod";
    case SnapNormal: return "yüzey normali";
    case SnapStep: return "adım";
    case SnapGuide: return "kılavuz";
    case SnapInsertion: return "ekleme noktası";
    default: return "yok";
    }
}

// ------------------------------------------------------------------ rules ---

Mm grid_step_in_force(Mm declared, bool adaptive, double mm_per_pixel) noexcept
{
    if (mm_per_pixel <= 0.0) return declared;

    double step_mm = static_cast<double>(declared);
    if (adaptive) {
        // The 1-2-5 ladder, so the reading beside a line stays a round number a
        // surveyor can hold in their head. 90 px is the target spacing.
        constexpr double kTargetPx = 90.0;
        step_mm                    = kTargetPx * mm_per_pixel;

        const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(step_mm, 1.0))));
        const double norm      = step_mm / magnitude;
        step_mm                = (norm < 2.0 ? 1.0 : norm < 5.0 ? 2.0 : 5.0) * magnitude;
    }

    if (step_mm < 1.0) return 0;

    // Below a couple of pixels the lines merge into a flat wash that hides the
    // drawing, so nothing is drawn — and nothing is snapped to either. Snapping
    // to a lattice the user cannot see is how a click lands somewhere they did
    // not ask for.
    if (step_mm / mm_per_pixel < 2.0) return 0;

    return mm_round(step_mm);
}

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

/// Rounds the DISTANCE from `base` to `p` to a whole multiple of `step`, keeping
/// the direction.
///
/// What a surveyor means by "12 cm adım": the line stops at 12, 24, 36 cm and
/// nowhere between. It constrains the LENGTH, not the position — a lattice would
/// constrain both and is what `apply_grid` is for — so it composes with ortho and
/// polar rather than fighting them: ortho picks the axis, polar picks the ray,
/// and this picks how far along it the point sits.
///
/// Deterministic, and by the same rule as everything else in this file: the
/// length comes from `segment_length`, which is exact integer arithmetic, and the
/// direction is that exact length divided into the integer offsets. No libm.
Point2 apply_step(Point2 base, Point2 p, Mm step) noexcept
{
    if (step <= 0) return p;

    const Mm length = segment_length(base, p);
    if (length <= 0) return p;

    // Round half away from zero on a positive quantity, in integers: no double
    // rounding mode can change what this answers.
    const Mm steps = (length + step / 2) / step;
    if (steps <= 0) {
        // Nearer than half a step: the point collapses onto the base, which is
        // what "the shortest line this step allows is one step" would deny. A zero
        // length is a line the command refuses anyway, and refusing is honest.
        return base;
    }

    const Mm wanted = steps * step;
    if (wanted == length) return p; // already on a step; no rounding drift

    const double scale = static_cast<double>(wanted) / static_cast<double>(length);
    return Point2{base.x + mm_round(static_cast<double>(p.x - base.x) * scale),
                  base.y + mm_round(static_cast<double>(p.y - base.y) * scale)};
}

// ----------------------------------------------------------------- engine ---

SnapResult snap(const Document& doc, const SnapQuery& q)
{
    SnapResult result;
    result.point = q.aim;

    // ---- 0. the surface's own perpendicular ----
    //
    // Dik mod squares a line to the SHEET. What a survey needs is square to the
    // THING: a çekme mesafesi runs perpendicular to the boundary it is measured
    // from, a building line to the road it faces, an offset to the edge it
    // offsets. On a boundary running at 37 degrees dik mod is exactly the wrong
    // answer, and there was no right one — the user read the bearing, added
    // ninety and typed it.
    //
    // AHEAD OF THE OBJECT SNAP, and the placement is the rule. Behind it, the
    // aid held only until the run reached something: the cursor came up to the
    // boundary across the way, an endpoint or a nearest-point won as it should in
    // every other case, and the line stopped being square to the surface it left
    // — in the one gesture whose entire purpose was to stay square. So inside the
    // cone the perpendicular wins outright, and what it lands ON is the crossing
    // rather than an arbitrary point of the far edge. Outside the cone it is not
    // engaged at all and every ordinary snap works exactly as before, which is
    // what keeps this from being a mode that swallows the drawing.
    //
    // The surface is the edge nearest the BASE, because that is the one the run
    // is leaving; and both directions along the normal are live, so a
    // perpendicular can be struck inwards or outwards without aiming precisely.
    // `apply_step` composes with it exactly as it does with the other two: the
    // normal picks the ray, the step picks how far.
    if (q.has_base && q.normal_lock && q.normal_reach > 0) {
        Point2 unit{};
        if (surface_normal(doc, q.base, q.normal_reach, unit)) {
            const double nx = static_cast<double>(unit.x) / kNormalScale;
            const double ny = static_cast<double>(unit.y) / kNormalScale;

            const double dx = static_cast<double>(q.aim.x - q.base.x);
            const double dy = static_cast<double>(q.aim.y - q.base.y);

            // ALONG the normal, and how far OFF it. Both rays are live, so a
            // perpendicular can be struck inwards or outwards without aiming
            // precisely: the sign of `along` chooses the side.
            const double along = (dx * nx) + (dy * ny);
            const double off   = (dx * -ny) + (dy * nx);
            const double reach = std::sqrt((dx * dx) + (dy * dy));

            // A TRACKING AID, NOT A JAIL, and this is the whole difference.
            //
            // Held down as an absolute lock it did exactly what it was told
            // and nothing else was drawable: with the mode on, every line and
            // every measurement came out perpendicular no matter where the
            // user aimed, so turning it on meant giving up the drawing. That
            // is not what a snap is. Every other rule in this engine offers a
            // point when the aim is NEAR it and stands aside when it is not,
            // and the normal now does the same — inside the cone it lands
            // exactly on the perpendicular, outside it the aim is the user's.
            //
            // The cone is angular rather than a distance, because the further
            // along a perpendicular the user pulls, the further sideways the
            // same intent wanders. `kNormalCone` is its tangent.
            const bool inside_cone = std::abs(off) <= std::abs(along) * kNormalCone;

            // Right on top of the base there is no direction to be near, so
            // the aperture stands in for the cone: the first millimetres of a
            // pull must not be decided by an angle measured on nothing.
            const bool at_the_base = reach <= static_cast<double>(q.normal_reach);

            if (reach > 0.0 && (inside_cone || at_the_base)) {
                const Point2 on{q.base.x + mm_round(nx * along), q.base.y + mm_round(ny * along)};

                // AND IT LANDS ON WHAT IT RUNS INTO. A perpendicular is drawn
                // from one boundary ACROSS to another, and the far boundary is
                // where the measurement ends. Reaching it used to cost the
                // perpendicular: the object snap won, the point went onto the
                // far edge at whatever spot the cursor was nearest, and the
                // run stopped being square to the surface it left. Both hold
                // now — the direction is the normal's, the point is the
                // crossing.
                //
                // The step is NOT applied to a crossing. A step rounds how far
                // along the ray the point sits, and the whole value of landing
                // on an edge is that it sits exactly there.
                Point2 crossing{};
                EntityId crossed = kNoEntity;
                if (q.radius > 0 && ray_meets_edge(doc, q.base, on, q.radius, crossing, crossed)) {
                    result.point       = crossing;
                    result.entity      = crossed;
                    result.mode        = SnapNormal;
                    result.constrained = true;
                    return result;
                }

                result.point       = apply_step(q.base, on, q.step);
                result.mode        = SnapNormal;
                result.constrained = true;
                return result;
            }
        }
    }

    // ---- 1. object snap ----
    std::uint32_t object_modes = q.modes & SnapObjectMask;

    // A constructed mode with no reach is a mode that cannot see the edge it
    // would build from, so it is switched off here rather than searching for
    // nothing — the same contract `grid_step` and `polar_step` already keep.
    if (q.reach <= 0)
        object_modes = static_cast<std::uint32_t>(object_modes & ~SnapConstructedMask);

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

        // The drawn outline of a kind that is not its vertices, rebuilt per
        // candidate. One buffer for the whole query: an ellipse is 128 vertices
        // and a query sees a handful of candidates. The key-point buffers are
        // reused the same way.
        EmitBuffer outline;
        std::vector<Point2> key_points;
        std::vector<std::uint32_t> key_modes;

        const EntityTable& entities  = doc.entities();
        const RingGeometry& geometry = doc.geometry();

        for (EntityId e : candidates) {
            const RingSpan span = geometry.rings_of(entities.slot[e]);

            // A SURVEYED POINT IS ITS OWN MODE. A monument is what every boundary
            // on a cadastral sheet was measured from, so it is offered separately
            // and ranked ABOVE a corner: when a röper and a parcel corner sit a
            // millimetre apart, the röper is the one that was measured and the
            // corner is the one that was derived from it.
            if (entities.kind[e] == kPointKind) {
                if ((object_modes & SnapNode) != 0 && span.count > 0) {
                    const auto xs = geometry.ring_xs(span.first);
                    const auto ys = geometry.ring_ys(span.first);
                    if (!xs.empty())
                        offer(best[priority_index(SnapNode)], Point2{xs[0], ys[0]}, e, q.aim,
                              limit);
                }
                // Nothing else applies: a point has no segment to be the middle,
                // the nearest part or the perpendicular foot of.
                continue;
            }

            // A CURVE IS NOT ITS STORED VERTICES, and treating it as if it were
            // was the whole of this bug. A circle keeps its centre and a handle
            // due east at the radius; an arc keeps its centre and its two ends.
            // The loop below walks a ring as a chain of SEGMENTS, so on a circle
            // it offered the middle of a line nobody drew, the nearest point of a
            // line nobody drew, and the east handle as though it were a corner —
            // while the one point a surveyor actually reaches for, the CENTRE,
            // was unreachable, because `ring_centroid` wants three vertices and a
            // closed ring and a circle's is neither.
            //
            // `pick.cpp` and the selection outline already know this and rebuild
            // the shape from centre and radius; snapping now reads the same way.
            if (entities.kind[e] == kCircleKind || entities.kind[e] == kArcKind) {
                const bool full          = entities.kind[e] == kCircleKind;
                const std::uint32_t slot = entities.slot[e];
                const Point2 centre =
                    full ? circle_centre_of(geometry, slot) : arc_centre_of(geometry, slot);
                const Mm radius =
                    full ? circle_radius_of(geometry, slot) : arc_radius_of(geometry, slot);

                if ((object_modes & SnapCenter) != 0)
                    offer(best[priority_index(SnapCenter)], centre, e, q.aim, limit);

                if (!full) {
                    // An ARC has two real ends and a real middle; a full circle
                    // has neither, and offering one would be inventing a corner.
                    const Point2 from = arc_start_of(geometry, slot);
                    const Point2 to   = arc_end_of(geometry, slot);
                    if ((object_modes & SnapEndpoint) != 0) {
                        offer(best[priority_index(SnapEndpoint)], from, e, q.aim, limit);
                        offer(best[priority_index(SnapEndpoint)], to, e, q.aim, limit);
                    }
                    if ((object_modes & SnapMidpoint) != 0) {
                        const Point2 mid = arc_midpoint(centre, radius, from, to);
                        offer(best[priority_index(SnapMidpoint)], mid, e, q.aim, limit);
                    }
                }

                // THE CURVE ITSELF, computed rather than tessellated: centre plus
                // the radius along the aim's own direction is EXACT, and it costs
                // one square root where walking 64 chords would cost 64 segments
                // and still land beside the curve.
                if ((object_modes & SnapNearest) != 0 && radius > 0) {
                    const double dx  = static_cast<double>(q.aim.x - centre.x);
                    const double dy  = static_cast<double>(q.aim.y - centre.y);
                    const double len = std::sqrt(dx * dx + dy * dy);
                    if (len > 0.0) {
                        const double scale = static_cast<double>(radius) / len;
                        Point2 on{centre.x + mm_round(dx * scale), centre.y + mm_round(dy * scale)};
                        // An arc is only the part of the circle it sweeps; a point
                        // beyond its ends belongs to the circle it was cut from
                        // and not to the thing that is drawn.
                        if (full || on_arc(centre, arc_start_of(geometry, slot),
                                           arc_end_of(geometry, slot), on))
                            offer(best[priority_index(SnapNearest)], on, e, q.aim, limit);
                    }
                }

                // NOT the segment walk below: there is no segment. Every mode a
                // curve can answer has been answered.
                continue;
            }

            // A KIND DRAWN FROM AN OUTLINE — the ellipse today — answers from the
            // outline the picture is drawn with, never from its stored definition
            // points. Its centre is MERKEZ; its four axis ends are offered as UÇ,
            // because they are the points ELİPS was drawn from and the points a
            // surveyor measures it by. The chords carry YAKIN, DİK and KESİŞİM.
            // ORTA, UZANTI and PARALEL are NOT offered on a chord of an
            // approximation: the middle of a chord is a place the curve does not
            // pass, and the extension of one is a line nobody drew.
            if (outline_kind(entities.kind[e])) {
                // The kind names its own key points and the mode each is offered
                // under (`KindSpec::key_points`): an ellipse's centre as MERKEZ
                // and its four axis ends as UÇ, a block reference's insertion
                // point as EKLEME. Offered only under a mode that is on, at that
                // mode's own priority.
                if (const KindSpec* spec = builtin_kinds().find(entities.kind[e]);
                    spec != nullptr && spec->key_points != nullptr) {
                    key_points.clear();
                    key_modes.clear();
                    KeyPointSink sink{key_points, key_modes};
                    spec->key_points(geometry, entities.slot[e], sink);
                    for (std::size_t k = 0; k < key_points.size(); ++k)
                        if ((object_modes & key_modes[k]) != 0)
                            offer(best[priority_index(key_modes[k])], key_points[k], e, q.aim,
                                  limit);
                }

                for_each_chain(doc, e, outline, [&](const Chain& chain) {
                    const std::size_t n = chain.xs.size();
                    if (n < 2) return;
                    const std::size_t segments = chain.closed ? n : n - 1;

                    for (std::size_t v = 0; v < segments; ++v) {
                        const std::size_t w = (v + 1) % n;
                        const Point2 a{chain.xs[v], chain.ys[v]};
                        const Point2 b{chain.xs[w], chain.ys[w]};
                        if (!segment_touches_box(a, b, aperture)) continue;

                        if ((object_modes & SnapNearest) != 0)
                            offer(best[priority_index(SnapNearest)],
                                  closest_point_on_segment(a, b, q.aim), e, q.aim, limit);

                        if ((object_modes & SnapPerpendicular) != 0 && q.has_base)
                            offer(best[priority_index(SnapPerpendicular)],
                                  closest_point_on_segment(a, b, q.base), e, q.aim, limit);

                        if ((object_modes & SnapIntersection) != 0 &&
                            near.size() < kMaxNearSegments)
                            near.push_back(NearSegment{a, b, e});
                    }
                });
                continue;
            }

            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                const auto xs = geometry.ring_xs(r);
                const auto ys = geometry.ring_ys(r);
                if (xs.empty()) continue;

                if ((object_modes & SnapCentroid) != 0) {
                    Point2 centre{};
                    if (ring_centroid(geometry, r, centre))
                        offer(best[priority_index(SnapCentroid)], centre, e, q.aim, limit);
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

        for (std::uint32_t bit : kPriority) {
            const Best& b = best[priority_index(bit)];
            if (b.distance < 0.0) continue;
            result.point  = b.point;
            result.mode   = bit;
            result.entity = b.entity;
            return result;
        }
    }

    // ---- 1b. the drafting guides ----
    //
    // AFTER the object block, which returns as soon as it finds anything, so a
    // parcel corner under the same aperture always wins. A guide is a line the
    // USER drew for themselves and it must never take a measured point away from
    // them — the same rule the constructed modes follow.
    //
    // Two guides crossing give a POINT, which is what makes a pair of them usable
    // for setting one out; a single guide gives the foot of the perpendicular
    // onto it, so the cursor slides along the line.
    if ((q.modes & SnapGuide) != 0 && q.radius > 0) {
        const GuideStore& guides = doc.guides();

        Mm best_h = 0, best_v = 0;
        bool have_h = false, have_v = false;
        Mm dh = q.radius, dv = q.radius;

        for (std::size_t i = 0; i < guides.size(); ++i) {
            const Mm c = guides.coordinate(i);
            if (guides.axis(i) == GuideAxis::Horizontal) {
                const Mm d = abs_mm(c - q.aim.y);
                // `<=` so the FIRST guide of two at one coordinate wins and the
                // answer does not depend on iteration order.
                if (d < dh || (!have_h && d <= dh)) {
                    dh     = d;
                    best_h = c;
                    have_h = true;
                }
            } else {
                const Mm d = abs_mm(c - q.aim.x);
                if (d < dv || (!have_v && d <= dv)) {
                    dv     = d;
                    best_v = c;
                    have_v = true;
                }
            }
        }

        if (have_h || have_v) {
            result.point  = Point2{have_v ? best_v : q.aim.x, have_h ? best_h : q.aim.y};
            result.mode   = SnapGuide;
            result.entity = kNoEntity;
            return result;
        }
    }

    // ---- 2. direction constraint from the previous point ----
    //
    // The step is applied AFTER the direction, and to the result of it: ortho
    // chooses the axis, polar chooses the ray, and the step chooses how far along
    // it the point lands. It is applied on its own too, so "12 cm adım" works with
    // no direction lock at all.
    //
    // The surface normal is NOT here. It is stage 0, ahead of the object snap,
    // because it is the one direction aid whose whole job is to survive reaching
    // something — see the note there.
    if (q.has_base) {
        if (q.ortho) {
            result.point       = apply_step(q.base, apply_ortho(q.base, q.aim), q.step);
            result.mode        = SnapOrtho;
            result.constrained = true;
            return result;
        }
        if ((q.modes & SnapPolar) != 0 && q.polar_step > 0) {
            result.point = apply_step(q.base, apply_polar(q.base, q.aim, q.polar_step), q.step);
            result.mode  = SnapPolar;
            result.constrained = true;
            return result;
        }
        if (q.step > 0) {
            result.point       = apply_step(q.base, q.aim, q.step);
            result.mode        = SnapStep;
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

} // namespace kentos::core
