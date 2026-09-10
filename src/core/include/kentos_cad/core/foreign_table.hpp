// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: bytes that belong to another program.
//
// model.md R26a. A DXF carries XDATA — extended entity data another application
// attached to an entity, in that application's own vocabulary. This program
// cannot interpret it and must not lose it (io.md P11): a drawing that came from
// a municipality's plotter software goes back to it with the bytes it arrived
// with. So they are kept HERE, opaque: per slot, per tag, a byte string that no
// command reads, no panel shows beyond its count, no frame path touches, and the
// file writes exactly as it was given.
//
// This is NOT a property bag (P12). Nothing is queried by name, nothing is typed,
// nothing is edited. The encoding inside the bytes is the source format's own —
// the DXF reader lays XDATA down with `/src/io`'s codec — and the IEEE doubles it
// may contain are the FILE's numbers, never the document's (P8 binds model
// fields; a foreign byte string is not a field).
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// The tag of DXF extended entity data. Stable; it reaches the file.
inline constexpr std::string_view kForeignDxfXdata = "dxf.xdata";

/// Foreign byte strings attached to entity slots, each under a tag.
///
/// Sorted by slot, then by tag NAME, so two documents holding the same facts
/// fold to the same hash whatever order they were built in (core.md P11: no
/// hash-order iteration). Small by nature — a cadastral DXF attaches XDATA to a
/// few hundred entities, not to five million — so a sorted vector and a binary
/// search is the right structure, and a slot without any costs nothing.
class ForeignTable
{
public:
    /// One attached byte string.
    struct Record
    {
        std::uint32_t slot;  ///< the entity slot it belongs to
        std::uint32_t tag;   ///< index into `tags()`
        std::uint64_t start; ///< offset into `pool()`
        std::uint32_t bytes; ///< length in bytes
    };

    /// Attaches `bytes` to `slot` under `tag`. Refuses a second attachment under
    /// the same (slot, tag): the source had one, and two would be an invention.
    /// Empty bytes are refused too — "attached nothing" is not a record.
    Status attach(std::uint32_t slot, std::string_view tag, std::span<const std::uint8_t> bytes);

    /// Removes the attachment under (slot, tag). The pool is never compacted, so
    /// the bytes stay where they were and only the record goes; false when there
    /// was none.
    bool detach(std::uint32_t slot, std::string_view tag);

    /// The bytes under (slot, tag), or an empty span.
    std::span<const std::uint8_t> bytes(std::uint32_t slot, std::string_view tag) const noexcept;

    /// How many attachments `slot` carries.
    std::size_t count_for(std::uint32_t slot) const noexcept;

    /// Every record, in (slot, tag name) order.
    const std::vector<Record>& records() const noexcept { return records_; }

    /// The interned tag names, indexed by `Record::tag`.
    const std::vector<std::string>& tags() const noexcept { return tags_; }

    /// The byte pool `Record::start` indexes.
    const std::vector<std::uint8_t>& pool() const noexcept { return pool_; }

    bool empty() const noexcept { return records_.empty(); }

    std::size_t size() const noexcept { return records_.size(); }

    /// Folds into the document hash, record by record in stored order. A table
    /// with no records folds to the seed unchanged, so every file written before
    /// foreign data existed keeps its fingerprint.
    std::uint64_t fold(std::uint64_t seed) const;

    void clear();

private:
    /// Where (slot, tag) sits or would sit; `found` says which.
    std::size_t locate(std::uint32_t slot, std::string_view tag, bool& found) const noexcept;
    std::uint32_t intern(std::string_view tag);

    std::vector<Record> records_;
    std::vector<std::string> tags_;
    std::vector<std::uint8_t> pool_;
};

} // namespace kentos::core
