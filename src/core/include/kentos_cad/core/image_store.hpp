// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the raster images a drawing carries with it.
//
// WHY A DRAWING EMBEDS ITS PICTURES INSTEAD OF POINTING AT THEM.
//
// MPYY publishes its symbology as PICTURES. The regulation's EK-1 annexes are
// Word documents, and 608 of the images lifted out of them sit in
// /data/catalogs/mpyy/semboller — a hatch for `orman`, a glyph for `cami`, a
// line type for `il sınırı`. Turning each one into a vector definition is a
// regulatory interpretation of a printed picture, and 476 of those need a
// harita mühendisi's signature (CLAUDE.md 6.11), not a guess by this program.
// Drawing the published picture is the only faithful answer available today, and
// it is what this store makes possible.
//
// The picture then has to be FOUND when the drawing is opened, and a path is the
// wrong way to find it. A path breaks when the file moves, when the data package
// is not installed, when the drawing is emailed to the belediye that has to check
// it. So the bytes travel inside the document, exactly as DXF and DWG have always
// carried their embedded rasters, and exactly as this project already claims of
// its symbols: "a drawing carries the symbols it uses in its own style table, so
// it opens the same on a machine with no library at all."
//
// The cost is small and bounded. A sheet using fifty gösterim carries fifty
// images of a few kilobytes each; the store deduplicates by CONTENT, so a hatch
// shared by nine plan types is stored once.
//
// WHAT THIS IS NOT: an image cache, a texture atlas or a decoder. It holds bytes
// and hands them back. Decoding belongs to the backend, which is the only layer
// that knows what a GPU or a QPainter wants (core.md P9: core does no I/O and
// links nothing).
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kentos::core {

/// The encodings a drawing may carry.
///
/// Two, and deliberately not more: these are what MPYY's own annexes contain
/// after extraction, and every format added here is a decoder the reader has to
/// have and a parser /tests/fuzz has to cover (io.md R19).
enum class ImageFormat : std::uint8_t {
    Png = 0,
    Jpeg,

    /// A vector picture. What a published gösterim SHOULD be: it recolours, it
    /// scales without resampling, it carries no scanner's paper, and a marker
    /// drawn from one is the same shape at 1/1000 and at 1/5000. The annex
    /// publishes rasters, so these are authored against the printed symbol
    /// rather than traced off it — a traced JPEG circle is a wobbly polygon and
    /// the regulation says circle.
    Svg,
};

/// Stable machine name, for a file, a message or a test.
const char* image_format_name(ImageFormat f) noexcept;

/// The format a byte sequence declares in its own header, or nothing.
///
/// Read from the BYTES, never from a file extension: an extension is a claim by
/// whoever named the file and a header is a claim by whoever wrote it, and only
/// the second one is checkable. A sequence matching neither signature is refused
/// rather than guessed at.
Result<ImageFormat> sniff_image_format(std::span<const std::byte> bytes);

/// Index into a Document's ImageStore. Slot 0 always means "no image".
using ImageId = std::uint16_t;

inline constexpr ImageId kNoImage = 0;

/// The raster images one document carries, deduplicated by content.
class ImageStore
{
public:
    /// Builds a store whose slot 0 is already the "no image" sentinel, so a
    /// symbol layer that references nothing costs a zero.
    ImageStore();

    /// Adds an image, or returns the id an identical one already has.
    ///
    /// `origin` records WHERE the picture came from — the catalogue row and the
    /// package file it was published in. It is provenance, not a path to load
    /// from: the bytes are here, and a plan sheet that cannot say which annex its
    /// symbols came from is a plan sheet that cannot be checked (model.md R35,
    /// CLAUDE.md 11.7).
    ///
    /// Deduplication is by BYTES, not by origin: one hatch shared by nine plan
    /// types is stored once, and the first origin recorded for it is the one kept.
    Result<ImageId> intern(std::span<const std::byte> bytes, std::string_view origin);

    bool contains(ImageId id) const noexcept { return id < images_.size(); }

    /// The bytes behind `id`, or an empty span for the sentinel and for an id
    /// this store does not hold. Empty rather than an error: a renderer asking
    /// for a picture that is not there draws nothing, which is what the drawing
    /// says, and throwing at frame time would take the whole window down.
    std::span<const std::byte> bytes(ImageId id) const;

    ImageFormat format(ImageId id) const noexcept;

    /// Where this picture was published. Empty for the sentinel.
    std::string_view origin(ImageId id) const;

    /// A stable key for the CONTENT of `id`, 0 for the sentinel.
    ///
    /// What a backend caches a decoded picture under. Keying on the byte address
    /// instead would be a use-after-free waiting to happen: a document closes,
    /// its memory is reused by the next one, and the cache serves the old
    /// picture for the new address. Keying on the id alone is wrong for the same
    /// reason across documents. This is the hash the store already computed to
    /// deduplicate, so it costs nothing and two documents holding the same hatch
    /// share one decode.
    std::uint64_t content_key(ImageId id) const noexcept;

    /// How many entries, including the sentinel at 0.
    std::size_t size() const noexcept { return images_.size(); }

    /// Total bytes held, sentinel excluded. What a size report and a warning
    /// about an oversized document both need.
    std::size_t total_bytes() const noexcept { return total_bytes_; }

    /// Folds every image into the document fingerprint, IN ID ORDER.
    ///
    /// The bytes themselves are folded, not just their count: two drawings that
    /// reference the same number of pictures but different pictures are two
    /// different drawings. An empty store folds to the seed unchanged, so every
    /// file and fixture written before images existed keeps its fingerprint.
    std::uint64_t fold(std::uint64_t seed) const;

private:
    struct Entry
    {
        std::vector<std::byte> bytes;
        std::string origin;
        ImageFormat format{ImageFormat::Png};
        std::uint64_t key{0}; ///< content hash, computed once at intern time
    };

    std::vector<Entry> images_;
    std::unordered_map<std::uint64_t, std::vector<ImageId>> by_content_;
    std::size_t total_bytes_{0};
};

} // namespace kentos::core
