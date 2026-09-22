// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/transform.hpp"

#include <cmath>
#include <cstring>

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
    }
    return p;
}

std::vector<std::uint8_t> encode_ghost_spec(const GhostSpec& spec)
{
    std::vector<std::uint8_t> bytes(9);
    bytes[0] = static_cast<std::uint8_t>(spec.kind);
    std::memcpy(bytes.data() + 1, &spec.copies, sizeof(spec.copies));
    return bytes;
}

std::optional<GhostSpec> decode_ghost_spec(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != 9) return std::nullopt;
    if (bytes[0] > static_cast<std::uint8_t>(GhostKind::Mirror)) return std::nullopt;

    GhostSpec spec{};
    spec.kind = static_cast<GhostKind>(bytes[0]);
    std::memcpy(&spec.copies, bytes.data() + 1, sizeof(spec.copies));
    if (spec.copies < 1) return std::nullopt;
    return spec;
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
    }
    return x;
}

} // namespace kentos::core
