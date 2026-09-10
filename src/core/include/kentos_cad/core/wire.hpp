// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the byte order of a kind payload.
//
// model.md R9a. A kind stores what its rings cannot say — an arc-polyline's
// bulges, a spline's knots, a block reference's transform — as fixed-width
// integers in a byte string the kind alone interprets. These helpers are the one
// way those bytes are laid down and read back: little-endian on every platform,
// assembled byte by byte, so a file written on x86 comes back byte-identical on
// Apple Silicon (R26), and `memcpy` for the signed/unsigned hop so no coordinate
// ever meets a `static_cast<Mm>` outside the units.hpp rounding helper
// (core.md R20).
//
// A reader NEVER trusts a length it has not checked against the bytes present:
// a payload comes off disk, and disk is untrusted input (io.md).
#pragma once

#include "kentos_cad/core/units.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace kentos::core {

/// Appends one byte.
inline void put_u8(std::vector<std::uint8_t>& out, std::uint8_t v)
{
    out.push_back(v);
}

/// Appends a 16-bit unsigned integer, little-endian.
inline void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    for (int i = 0; i < 2; ++i)
        out.push_back(
            static_cast<std::uint8_t>((static_cast<std::uint32_t>(v) >> (i * 8)) & 0xFFu));
}

/// Appends a 32-bit unsigned integer, little-endian.
inline void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}

/// Appends a 64-bit unsigned integer, little-endian.
inline void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
}

/// Appends a signed 64-bit integer — a millimetre, a micro-degree, a scaled
/// ratio — through its two's-complement bit pattern.
inline void put_i64(std::vector<std::uint8_t>& out, std::int64_t v)
{
    std::uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    put_u64(out, u);
}

/// Appends a coordinate. The same bytes as `put_i64`; the name says what it is.
inline void put_mm(std::vector<std::uint8_t>& out, Mm v)
{
    put_i64(out, v);
}

/// Reads the bytes `put_*` wrote, checking every length against what is there.
///
/// The `remaining*` questions are the whole contract: a caller asks before every
/// read, and a read past the end is a programming error rather than a data
/// condition, exactly as `EmitBuffer::push_vertex` before `begin_run` is.
class WireReader
{
public:
    /// Reads from `bytes`, which must outlive the reader.
    explicit WireReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    /// Bytes not yet read.
    std::size_t left() const noexcept { return bytes_.size() - at_; }

    /// Whether `n` more bytes are present.
    bool remaining(std::size_t n) const noexcept { return left() >= n; }

    /// `count` records of `stride` bytes each, without ever computing the
    /// product: a hostile file declares four billion rings, and on a 32-bit
    /// build `count * stride` wraps to a number the payload happily satisfies.
    bool remaining_records(std::uint64_t count, std::size_t stride) const noexcept
    {
        return stride != 0 && count <= left() / stride;
    }

    /// Where the next read starts, as an offset into the bytes.
    std::size_t position() const noexcept { return at_; }

    /// One byte.
    std::uint8_t u8() { return bytes_[at_++]; }

    /// A 16-bit unsigned integer.
    std::uint16_t u16()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 2; ++i)
            v |= static_cast<std::uint32_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 2;
        return static_cast<std::uint16_t>(v);
    }

    /// A 32-bit unsigned integer.
    std::uint32_t u32()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 4;
        return v;
    }

    /// A 64-bit unsigned integer.
    std::uint64_t u64()
    {
        std::uint64_t u = 0;
        for (int i = 0; i < 8; ++i)
            u |= static_cast<std::uint64_t>(bytes_[at_ + static_cast<std::size_t>(i)]) << (i * 8);
        at_ += 8;
        return u;
    }

    /// A signed 64-bit integer, through its bit pattern.
    std::int64_t i64()
    {
        const std::uint64_t u = u64();
        std::int64_t v        = 0;
        std::memcpy(&v, &u, sizeof(v));
        return v;
    }

    /// A coordinate. The same bytes as `i64`; the name says what it is.
    Mm mm() { return i64(); }

    /// Skips `n` bytes the caller has already checked are present.
    void skip(std::size_t n) noexcept { at_ += n; }

    /// The byte pointer at `offset`, for a caller that has checked `remaining`.
    const std::uint8_t* bytes_at(std::size_t offset) const noexcept
    {
        return bytes_.data() + offset;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t at_{0};
};

} // namespace kentos::core
