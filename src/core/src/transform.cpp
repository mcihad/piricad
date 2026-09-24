// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/transform.hpp"

#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/arc_polyline.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/hatch.hpp"
#include "kentos_cad/core/identity.hpp"

#include <cmath>
#include <cstring>
#include <utility>

namespace kentos::core {
Point2 rotated_about(Point2 p, Point2 base, SinCos t)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * t.cos - dy * t.sin),
                  base.y + mm_round(dx * t.sin + dy * t.cos)};
}

Point2 scaled_about(Point2 p, Point2 base, double factor)
{
    const auto dx = static_cast<double>(p.x - base.x);
    const auto dy = static_cast<double>(p.y - base.y);

    return Point2{base.x + mm_round(dx * factor), base.y + mm_round(dy * factor)};
}

Point2 mirrored_in_line(Point2 p, Point2 a, Point2 b)
{
    const Mm ax = b.x - a.x;
    const Mm ay = b.y - a.y;

    // THE TWO AXES A SURVEYOR ACTUALLY PICKS, handled exactly. A reflection in a
    // horizontal or vertical line is an integer negation; routing it through the
    // general formula would round a coordinate that had an exact answer.
    if (ay == 0 && ax != 0) return Point2{p.x, 2 * a.y - p.y};
    if (ax == 0 && ay != 0) return Point2{2 * a.x - p.x, p.y};

    const auto vx     = static_cast<double>(ax);
    const auto vy     = static_cast<double>(ay);
    const double len2 = vx * vx + vy * vy;
    if (len2 <= 0.0) return p; // a line through one point reflects nothing

    const auto px = static_cast<double>(p.x - a.x);
    const auto py = static_cast<double>(p.y - a.y);

    // The reflection of a vector in a line through the origin, written so the
    // only division is by the axis's squared length.
    const double t  = (px * vx + py * vy) / len2;
    const double rx = 2.0 * t * vx - px;
    const double ry = 2.0 * t * vy - py;

    return Point2{a.x + mm_round(rx), a.y + mm_round(ry)};
}

Point2 transformed(const Xform& x, Point2 p)
{
    switch (x.kind) {
    case Xform::Kind::Translate: return translated(p, x.dx, x.dy);
    case Xform::Kind::Rotate: return rotated_about(p, x.base, x.turn);
    case Xform::Kind::Scale: return scaled_about(p, x.base, x.factor);
    case Xform::Kind::Mirror: return mirrored_in_line(p, x.base, x.axis_b);
    case Xform::Kind::Align: {
        // `p -> axis_b + factor · turn(flip(p - base))`, the offset from the
        // source taken first so the magnitudes multiplied are the object's, not
        // a TUREF coordinate's (core.md R3).
        const double dx = static_cast<double>(p.x - x.base.x) * x.factor;
        const double dy = static_cast<double>(p.y - x.base.y) * x.factor * (x.flip ? -1.0 : 1.0);
        return Point2{x.axis_b.x + mm_round(dx * x.turn.cos - dy * x.turn.sin),
                      x.axis_b.y + mm_round(dx * x.turn.sin + dy * x.turn.cos)};
    }
    case Xform::Kind::Stretch: {
        const auto dx = static_cast<double>(p.x - x.base.x);
        const auto dy = static_cast<double>(p.y - x.base.y);
        return Point2{x.base.x + mm_round(dx * x.factor), x.base.y + mm_round(dy * x.factor_y)};
    }
    case Xform::Kind::Place: {
        // THE REFERENCE'S OWN ARITHMETIC, not a second spelling of it: the grid
        // step is one copy of a reference spaced by that step, so a member taken
        // out by PATLAT lands where `expand_block_definition` drew it.
        BlockReference ref;
        ref.sx             = x.place_sx;
        ref.sy             = x.place_sy;
        ref.rotation_udeg  = x.place_udeg;
        ref.column_spacing = x.dx;
        ref.row_spacing    = x.dy;
        return place_block_point(ref, x.axis_b, x.base, p, 1, 1);
    }
    }
    return p;
}

Xform block_placement(const BlockReference& ref, Point2 insertion, Point2 base, int column,
                      int row) noexcept
{
    Xform x;
    x.kind       = Xform::Kind::Place;
    x.base       = base;
    x.axis_b     = insertion;
    x.dx         = static_cast<Mm>(column) * ref.column_spacing;
    x.dy         = static_cast<Mm>(row) * ref.row_spacing;
    x.place_sx   = ref.sx;
    x.place_sy   = ref.sy;
    x.place_udeg = ref.rotation_udeg;
    return x;
}

bool place_uniform(const Xform& x) noexcept
{
    if (x.kind != Xform::Kind::Place) return false;
    const auto mag = [](std::int64_t v) { return static_cast<Int128>(v < 0 ? -v : v); };
    return mag(x.place_sx.num) * static_cast<Int128>(x.place_sy.den) ==
           mag(x.place_sy.num) * static_cast<Int128>(x.place_sx.den);
}

Mm place_length(const Xform& x, Mm v) noexcept
{
    if (!place_uniform(x)) return v;
    const std::int64_t num = x.place_sx.num < 0 ? -x.place_sx.num : x.place_sx.num;
    return mul_div_round(v, num, x.place_sx.den);
}

std::vector<RingGeometry::RingInput> TranslatedRecord::inputs() const
{
    std::vector<RingGeometry::RingInput> out;
    out.reserve(rings.size());
    for (std::size_t i = 0; i < rings.size(); ++i)
        out.push_back(RingGeometry::RingInput{rings[i], roles[i], parts[i]});
    return out;
}

Result<TranslatedRecord> translated_record(const Document& doc, EntityId e, Mm dx, Mm dy)
{
    const EntityTable& ents  = doc.entities();
    const RingGeometry& geom = doc.geometry();
    const std::uint32_t slot = ents.slot[e];
    TranslatedRecord out;
    const RingSpan span = geom.rings_of(slot);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = geom.ring_xs(r);
        const auto ys = geom.ring_ys(r);
        std::vector<Point2> pts;
        pts.reserve(xs.size());
        for (std::size_t v = 0; v < xs.size(); ++v)
            pts.push_back(Point2{xs[v] + dx, ys[v] + dy});
        out.rings.push_back(std::move(pts));
        out.roles.push_back(geom.ring_role[r]);
        out.parts.push_back(geom.ring_part[r]);
    }
    const auto bytes = geom.payload_of(slot);
    out.payload.assign(bytes.begin(), bytes.end());
    switch (ents.kind[e]) {
    case kArcPolylineKind: {
        auto def = arc_polyline_of(geom, slot);
        if (!def) return def.error();
        for (ArcPolyline::Arc& a : def.value().arcs)
            a.centre = Point2{a.centre.x + dx, a.centre.y + dy};
        out.payload = encode_arc_polyline(def.value());
        break;
    }
    case kBlockReferenceKind: {
        auto ref = block_reference_of(geom, slot);
        if (!ref) return ref.error();
        Box2& b     = ref.value().bounds;
        b           = Box2{b.min_x + dx, b.min_y + dy, b.max_x + dx, b.max_y + dy};
        out.payload = encode_block_reference(ref.value());
        break;
    }
    case kHatchKind: {
        auto def = hatch_of(geom, slot);
        if (!def) return def.error();
        def.value().origin = Point2{def.value().origin.x + dx, def.value().origin.y + dy};
        out.payload        = encode_hatch(def.value());
        break;
    }
    default: break;
    }
    return out;
}

bool xform_reverses(const Xform& x) noexcept
{
    return x.kind == Xform::Kind::Mirror || (x.kind == Xform::Kind::Align && x.flip) ||
           (x.kind == Xform::Kind::Stretch && (x.factor < 0.0) != (x.factor_y < 0.0)) ||
           (x.kind == Xform::Kind::Place && (x.place_sx.num < 0) != (x.place_sy.num < 0));
}

namespace {

/// A turn in whole micro-degrees into [0, 360°).
std::int64_t wrapped(std::int64_t udeg) noexcept
{
    udeg %= kUDegFullCircle;
    return udeg < 0 ? udeg + kUDegFullCircle : udeg;
}

} // namespace

EllipseImage transformed_ellipse(const Xform& x, const EllipseImage& e)
{
    EllipseImage out = e;
    out.centre       = transformed(x, e.centre);
    // The axis VECTORS under the transform: the images of the axis ends less
    // the image of the centre, in metres so their products stay in range.
    const auto metres_of = [&out](Point2 end) {
        return std::pair{mm_to_metres(end.x - out.centre.x), mm_to_metres(end.y - out.centre.y)};
    };
    auto [ax, ay]      = metres_of(transformed(x, e.major));
    auto [bx, by]      = metres_of(transformed(x, e.minor));
    std::int64_t shift = 0; ///< how far the parameter moved, micro-degrees

    if (x.kind == Xform::Kind::Stretch || (x.kind == Xform::Kind::Place && !place_uniform(x))) {
        // CONJUGATE, NOT PERPENDICULAR: the principal axes are the conjugate
        // pair turned by φ, where tan 2φ = 2 a·b / (|a|² − |b|²). `atan2_udeg`
        // and `sin_cos_udeg`, never libm (§7.3), on integers in micro-metres².
        const double dot  = ax * bx + ay * by;
        const double diff = ax * ax + ay * ay - (bx * bx + by * by);
        const std::int64_t twice =
            atan2_udeg(static_cast<std::int64_t>(std::llround(2.0 * dot * 1.0e6)),
                       static_cast<std::int64_t>(std::llround(diff * 1.0e6)));
        const std::int64_t phi = twice / 2;
        const SinCos t         = sin_cos_udeg(phi);
        const double nax       = ax * t.cos + bx * t.sin;
        const double nay       = ay * t.cos + by * t.sin;
        const double nbx       = -ax * t.sin + bx * t.cos;
        const double nby       = -ay * t.sin + by * t.cos;
        ax = nax, ay = nay, bx = nbx, by = nby;
        shift = phi;
        // The longer first, as the name of the first axis says: turning the
        // frame a quarter keeps the curve and swaps the roles.
        if (bx * bx + by * by > ax * ax + ay * ay) {
            const double oax = ax;
            const double oay = ay;
            ax               = bx;
            ay               = by;
            bx               = -oax;
            by               = -oay;
            shift += kUDegFullCircle / 4;
        }
    }
    // COUNTER-CLOCKWISE, second of first: after a reflection the second axis
    // is the other side, and the parameter runs the other way.
    const bool mirrored = ax * by - ay * bx < 0.0;
    if (mirrored) {
        bx = -bx;
        by = -by;
    }
    const auto end_of = [&out](double vx, double vy) {
        return Point2{out.centre.x + mm_round(vx * static_cast<double>(kMmPerMetre)),
                      out.centre.y + mm_round(vy * static_cast<double>(kMmPerMetre))};
    };
    out.major = end_of(ax, ay);
    out.minor = end_of(bx, by);
    if (e.partial) {
        std::int64_t s = wrapped(e.start_udeg - shift);
        std::int64_t f = wrapped(e.end_udeg - shift);
        if (mirrored) {
            const std::int64_t ns = wrapped(-f);
            f                     = wrapped(-s);
            s                     = ns;
        }
        out.start_udeg = s;
        out.end_udeg   = f;
    }
    return out;
}

std::vector<std::uint8_t> encode_ghost_spec(const GhostSpec& spec)
{
    const std::size_t head =
        1 + sizeof(spec.copies) + 6 * sizeof(Mm) + 1 + 2 * sizeof(Mm) + sizeof(std::uint32_t);
    std::vector<std::uint8_t> bytes(head + spec.keys.size() * sizeof(std::int64_t));
    std::size_t at = 0;
    const auto put = [&bytes, &at](const void* from, std::size_t n) {
        std::memcpy(bytes.data() + at, from, n);
        at += n;
    };
    bytes[at++] = static_cast<std::uint8_t>(spec.kind);
    put(&spec.copies, sizeof(spec.copies));
    for (const Point2 p : {spec.from1, spec.to1, spec.from2}) {
        put(&p.x, sizeof(p.x));
        put(&p.y, sizeof(p.y));
    }
    bytes[at++] = spec.scale ? 1 : 0;
    put(&spec.reference_udeg, sizeof(spec.reference_udeg));
    put(&spec.reference_length, sizeof(spec.reference_length));
    const auto count = static_cast<std::uint32_t>(spec.keys.size());
    put(&count, sizeof(count));
    if (count != 0) put(spec.keys.data(), spec.keys.size() * sizeof(std::int64_t));
    return bytes;
}

std::optional<GhostSpec> decode_ghost_spec(std::span<const std::uint8_t> bytes)
{
    GhostSpec spec{};
    const std::size_t head =
        1 + sizeof(spec.copies) + 6 * sizeof(Mm) + 1 + 2 * sizeof(Mm) + sizeof(std::uint32_t);
    if (bytes.size() < head) return std::nullopt;
    if (bytes[0] > static_cast<std::uint8_t>(GhostKind::Align)) return std::nullopt;

    std::size_t at = 1;
    const auto get = [&bytes, &at](void* into, std::size_t n) {
        std::memcpy(into, bytes.data() + at, n);
        at += n;
    };
    spec.kind = static_cast<GhostKind>(bytes[0]);
    get(&spec.copies, sizeof(spec.copies));
    if (spec.copies < 1) return std::nullopt;
    for (Point2* p : {&spec.from1, &spec.to1, &spec.from2}) {
        get(&p->x, sizeof(p->x));
        get(&p->y, sizeof(p->y));
    }
    if (bytes[at] > 1) return std::nullopt;
    spec.scale = bytes[at++] == 1;
    get(&spec.reference_udeg, sizeof(spec.reference_udeg));
    get(&spec.reference_length, sizeof(spec.reference_length));
    std::uint32_t count = 0;
    get(&count, sizeof(count));
    if (bytes.size() != head + count * sizeof(std::int64_t)) return std::nullopt;
    spec.keys.resize(count);
    if (count != 0) get(spec.keys.data(), count * sizeof(std::int64_t));
    return spec;
}

std::optional<Xform> align_xform(Point2 from1, Point2 to1, Point2 from2, Point2 to2, bool scale,
                                 bool flip)
{
    const Mm was     = segment_length(from1, from2);
    const Mm becomes = segment_length(to1, to2);
    if (was == 0 || becomes == 0) return std::nullopt;

    // The turn as the difference of two directions, in turns, rounded once to
    // whole micro-degrees: `sin_cos_udeg` rather than libm, so a quarter turn
    // is exact and every platform agrees (§7.3).
    // Reflected first, the source direction is its mirror image: the turn is
    // then taken from that.
    const double source = direction_turns(from1, from2, AngleRule::Matematik);
    const double turns =
        direction_turns(to1, to2, AngleRule::Matematik) - (flip ? -source : source);

    Xform x;
    x.kind   = Xform::Kind::Align;
    x.base   = from1;
    x.axis_b = to1;
    x.turn   = sin_cos_udeg(mm_round(turns * static_cast<double>(kUDegFullCircle)));
    x.factor = scale ? static_cast<double>(becomes) / static_cast<double>(was) : 1.0;
    x.flip   = flip;
    return x;
}

UDeg ghost_turn_udeg(Point2 base, Point2 cursor) noexcept
{
    // `atan2_udeg` is the integer one the snap engine uses, so a right angle is
    // exactly a right angle and the same drag gives the same turn on every
    // platform (§7.3).
    return atan2_udeg(static_cast<std::int64_t>(cursor.y - base.y),
                      static_cast<std::int64_t>(cursor.x - base.x));
}

double ghost_factor(Point2 base, Point2 cursor) noexcept
{
    // Metres before squaring: the square of a TM3 coordinate difference in
    // millimetres leaves the 53-bit mantissa long before it leaves int64
    // (core.md R3).
    const double dx = mm_to_metres(cursor.x - base.x);
    const double dy = mm_to_metres(cursor.y - base.y);
    return std::sqrt(dx * dx + dy * dy);
}

Xform ghost_xform(const GhostSpec& spec, Point2 base, Point2 cursor)
{
    // BY REFERENCE: the turn from the reference direction to the cursor's, and
    // the factor from the reference length to the cursor's distance — what
    // DÖNDÜR and ÖLÇEKLE compute with the click (TODOS C-08).
    if (spec.kind == GhostKind::Rotate && spec.reference_udeg != 0) {
        Xform x = ghost_xform(spec.kind, base, cursor);
        x.turn  = sin_cos_udeg(ghost_turn_udeg(base, cursor) - spec.reference_udeg);
        return x;
    }
    if (spec.kind == GhostKind::Scale && spec.reference_length > 0) {
        Xform x  = ghost_xform(spec.kind, base, cursor);
        x.factor = static_cast<double>(segment_length(base, cursor)) /
                   static_cast<double>(spec.reference_length);
        return x;
    }
    if (spec.kind != GhostKind::Align) return ghost_xform(spec.kind, base, cursor);
    return align_xform(spec.from1, spec.to1, spec.from2, cursor, spec.scale).value_or(Xform{});
}

Xform ghost_xform(GhostKind kind, Point2 base, Point2 cursor)
{
    Xform x;
    switch (kind) {
    case GhostKind::Translate:
        x.kind = Xform::Kind::Translate;
        x.dx   = cursor.x - base.x;
        x.dy   = cursor.y - base.y;
        return x;
    case GhostKind::Rotate:
        x.kind = Xform::Kind::Rotate;
        x.base = base;
        x.turn = sin_cos_udeg(ghost_turn_udeg(base, cursor));
        return x;
    case GhostKind::Scale:
        x.kind   = Xform::Kind::Scale;
        x.base   = base;
        x.factor = ghost_factor(base, cursor);
        return x;
    case GhostKind::Mirror:
        x.kind   = Xform::Kind::Mirror;
        x.base   = base;
        x.axis_b = cursor;
        return x;
    case GhostKind::Align:
        // Without its fixed points an align is the move of its first pair,
        // which is what the cursor completes before the second pair exists.
        x.kind = Xform::Kind::Translate;
        x.dx   = cursor.x - base.x;
        x.dy   = cursor.y - base.y;
        return x;
    }
    return x;
}

} // namespace kentos::core
