// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: what every kind implementation shares.
//
// Not a public header. A kind's six functions are free functions over spans
// (model.md R22), and the pieces they have in common — the payload header every
// payload-carrying kind starts with, the snap-mode bits a key point is offered
// under, the segment distance the hit tests measure with — live here so eleven
// kinds do not carry eleven copies that drift apart.
#pragma once

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/wire.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kentos::core::kind {

/// Snap-mode bits, the values `core/snap.hpp` declares. Written as numbers
/// because a kind sits below the snap engine and must not include it.
inline constexpr std::uint32_t kKeyEndpoint  = 1u << 0;  ///< core::SnapEndpoint
inline constexpr std::uint32_t kKeyMidpoint  = 1u << 1;  ///< core::SnapMidpoint
inline constexpr std::uint32_t kKeyCenter    = 1u << 2;  ///< core::SnapCenter
inline constexpr std::uint32_t kKeyNode      = 1u << 9;  ///< core::SnapNode
inline constexpr std::uint32_t kKeyInsertion = 1u << 17; ///< core::SnapInsertion

/// Offers one key point under one mode.
inline void offer(KeyPointSink& into, Point2 p, std::uint32_t mode)
{
    into.points.push_back(p);
    into.modes.push_back(mode);
}

/// Squared distance from `p` to segment `a`-`b`, in square METRES: the square of
/// a TM coordinate difference in millimetres overflows a double's mantissa long
/// before it overflows int64, so the operands are translated to the probe first.
double segment_distance2_m(Point2 a, Point2 b, Point2 p);

/// Refuses a payload for a kind that carries none.
Status refuse_payload(const char* what, std::span<const std::uint8_t> payload);

/// Requires exactly one Open ring of `vertices` points.
Status one_open_ring(const char* what, std::span<const RingGeometry::RingInput> rings,
                     std::size_t vertices);

/// The header every payload-carrying kind starts with (model.md R9a): a layout
/// version, so a later build can read an older record and say what it lacks; a
/// flags word the kind interprets; four reserved bytes that keep what follows
/// 8-byte aligned. Little-endian, like everything in `wire.hpp`.
inline constexpr std::size_t kHeaderBytes = 8;

/// A decoded header.
struct Header
{
    std::uint16_t version{0}; ///< the kind's layout version
    std::uint16_t flags{0};   ///< the kind's flag bits
};

/// Appends a header.
inline void put_header(std::vector<std::uint8_t>& out, std::uint16_t version, std::uint16_t flags)
{
    put_u16(out, version);
    put_u16(out, flags);
    put_u32(out, 0);
}

/// Reads a header; false when the bytes do not hold one.
inline bool read_header(WireReader& in, Header& h)
{
    if (!in.remaining(kHeaderBytes)) return false;
    h.version = in.u16();
    h.flags   = in.u16();
    (void)in.u32();
    return true;
}

/// Appends a length-prefixed UTF-8 string (u32 length, then the bytes).
inline void put_string(std::vector<std::uint8_t>& out, const std::string& s)
{
    put_u32(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

/// Reads a length-prefixed string of at most `most` bytes; false when the bytes
/// are not there or the length is beyond `most`.
inline bool read_string(WireReader& in, std::string& s, std::size_t most)
{
    if (!in.remaining(4)) return false;
    const std::uint32_t n = in.u32();
    if (n > most || !in.remaining(n)) return false;
    s.assign(reinterpret_cast<const char*>(in.bytes_at(in.position())), n);
    in.skip(n);
    return true;
}

/// The bounds of a run of points; empty for none.
Box2 box_of_points(std::span<const Mm> xs, std::span<const Mm> ys);

/// The bounds of every run in a buffer; empty for none.
Box2 box_of_runs(const EmitBuffer& runs);

/// Whether `probe` is within `tolerance` of any run's segments, or — when
/// `inside_counts` — inside a closed run that is not a hole and outside every
/// hole. The polyline rule (`polyline_hit`), applied to a drawn form.
bool runs_hit(const EmitBuffer& runs, Point2 probe, Mm tolerance, bool inside_counts);

/// The length of a run's segments, in millimetres; the closing segment when closed.
Mm run_length(std::span<const Mm> xs, std::span<const Mm> ys, bool closed);

/// Twice the signed area of a run (counter-clockwise positive), in a double,
/// translated to its first vertex. For a kind whose stored area is an
/// approximation over its drawn form (spline), never for one with an exact answer.
double run_area2(std::span<const Mm> xs, std::span<const Mm> ys);

} // namespace kentos::core::kind
