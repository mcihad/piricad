// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io (internal): the validated view over a mapped project file.
//
// EVERY NUMBER IN A FILE IS HOSTILE. .claude/io.md R18 and P6 say it plainly:
// header-declared extents, counts, offsets and lengths are untrusted hints and
// are bounds-checked against the REAL file size before any allocation or seek.
// This class is where that happens, once, so that no decoding code further down
// has to remember to do it — every span it hands out is already inside the file.
//
// It is also where io.md R10 lives: a block whose id this build does not know is
// skipped by its declared length and is never fatal. That is the whole forward
// compatibility story, and it is four lines.
#pragma once

#include "kentos_cad/core/result.hpp"
#include "kentos_cad/io/format.hpp"

#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace kentos::io {

/// A parsed, fully validated directory over a mapping. Holds no ownership: the
/// `MappedFile` that produced the bytes must outlive it.
class BlockView
{
public:
    /// Validates the header and every directory entry against `bytes.size()`.
    ///
    /// Refuses, in this order, so the message names the first thing that is
    /// actually wrong: too small for a header, wrong magic, a reader version this
    /// build cannot satisfy (io.md R9), a header size it cannot navigate, a
    /// directory outside the file, an entry outside the file, an entry whose
    /// declared length disagrees with count × stride, an overlapping entry.
    static core::Result<BlockView> parse(std::span<const std::byte> bytes, const std::string& path);

    const FileHeader& header() const noexcept { return header_; }

    /// Number of directory entries this build did not recognise. Reported to the
    /// user, never fatal (R10): it is how a file written by a newer KentOSCad says
    /// "there is more here than you can see".
    std::size_t unknown_blocks() const noexcept { return unknown_; }

    bool has(BlockId id) const noexcept { return find(id) != nullptr; }

    /// Element count of `id`, or 0 when the block is absent. An absent column is
    /// the correct encoding of an empty document, so it is not an error.
    std::uint64_t count_of(BlockId id) const noexcept
    {
        const BlockEntry* e = find(id);
        return e ? e->count : 0;
    }

    /// A typed column over the mapping — no copy, no parse (io.md R5).
    ///
    /// `expect_count` is the count the DocumentRecord promised. A column that
    /// disagrees with it is a corrupt file, not a hint: the two numbers come from
    /// different places in the same file and a mismatch means one of them is a
    /// lie. An absent block is accepted only when `expect_count` is 0.
    template<class T>
    core::Result<std::span<const T>> column(BlockId id, std::uint64_t expect_count,
                                            const char* what) const
    {
        const BlockEntry* e = find(id);
        if (!e) {
            if (expect_count == 0) return std::span<const T>{};
            return core::err(core::ErrorCode::ParseError, std::string(kErrConsist) + ": " + what +
                                                              " sütunu eksik; belge kaydı " +
                                                              std::to_string(expect_count) +
                                                              " öğe bildiriyor. Dosya bozuk.");
        }
        if (e->elem_bytes != sizeof(T))
            return core::err(core::ErrorCode::ParseError,
                             std::string(kErrBlock) + ": " + what + " sütununun öğe boyu " +
                                 std::to_string(e->elem_bytes) + ", beklenen " +
                                 std::to_string(sizeof(T)) +
                                 ". Dosya bozuk ya da başka bir "
                                 "sürümden.");
        if (e->count != expect_count)
            return core::err(core::ErrorCode::ParseError,
                             std::string(kErrConsist) + ": " + what + " sütunu " +
                                 std::to_string(e->count) + " öğe taşıyor, belge kaydı " +
                                 std::to_string(expect_count) + " bildiriyor. Dosya bozuk.");

        // Safe by construction: parse() proved offset is 8-aligned and that
        // [offset, offset + bytes) lies inside the mapping, and the static_asserts
        // in format.hpp fix every record's layout. This reinterpret_cast is the
        // point of the whole format — it is what "mmap with zero parsing" means.
        const auto* first = reinterpret_cast<const T*>(bytes_.data() + e->offset);
        return std::span<const T>{first, static_cast<std::size_t>(e->count)};
    }

    /// Raw bytes of an opaque block (the string pool). Empty when absent.
    std::span<const std::byte> blob(BlockId id) const noexcept
    {
        const BlockEntry* e = find(id);
        if (!e) return {};
        return bytes_.subspan(static_cast<std::size_t>(e->offset),
                              static_cast<std::size_t>(e->bytes));
    }

    /// The one document-level record, copied out rather than mapped: it is 88
    /// bytes read once, and a copy costs nothing while keeping the caller free of
    /// any lifetime tie to the mapping.
    core::Result<DocumentRecord> document_record() const;

private:
    const BlockEntry* find(BlockId id) const noexcept
    {
        for (const auto& e : entries_)
            if (e.id == static_cast<std::uint32_t>(id)) return &e;
        return nullptr;
    }

    std::span<const std::byte> bytes_{};
    FileHeader header_{};
    std::vector<BlockEntry> entries_{};
    std::size_t unknown_{0};
};

/// Reads a fixed-layout wire record out of the mapping by value.
///
/// `std::memcpy` rather than a cast: these are one-off records, the copy is free
/// at this size, and it sidesteps every object-lifetime question that reading a
/// struct out of mapped bytes otherwise raises. The bulk columns, where the copy
/// would NOT be free, go through `BlockView::column` instead.
template<class T> T read_record(std::span<const std::byte> bytes, std::uint64_t offset) noexcept
{
    T out{};
    std::memcpy(&out, bytes.data() + offset, sizeof(T));
    return out;
}

} // namespace kentos::io
