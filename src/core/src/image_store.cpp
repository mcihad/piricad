// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/image_store.hpp"

#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <cstring>
#include <string_view>
#include <utility>

namespace kentos::core {
namespace {
// THE SEED DOES NOT FOLLOW THE PRODUCT'S NAME, and must not. It is folded into
// every content hash this program has ever computed — golden fixtures, journal
// fingerprints, the equality proof — so renaming it would silently change what
// every stored drawing hashes to. The string is an arbitrary constant that
// happens to read as the old name; that is all it has ever been.

constexpr std::uint64_t kImageSeed = fnv1a("piricad.core.image_store");

/// The first bytes of a PNG file, fixed by the format.
constexpr unsigned char kPngSignature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

/// A JPEG starts with SOI and its next marker always begins 0xFF.
constexpr unsigned char kJpegSignature[] = {0xFF, 0xD8, 0xFF};

/// The largest single picture a drawing will carry.
///
/// Sixteen megabytes is far above anything a symbol needs — the whole MPYY set is
/// eight, across six hundred files — and far below what would make a document
/// unopenable. The bound exists because these bytes arrive from a file on disk,
/// which is untrusted input: without it a corrupt header could ask the reader to
/// allocate whatever a 64-bit length says (io.md R17).
constexpr std::size_t kMaxImageBytes = std::size_t{16} * 1024 * 1024;

bool starts_with(std::span<const std::byte> bytes, const unsigned char* signature,
                 std::size_t length)
{
    if (bytes.size() < length) return false;
    for (std::size_t i = 0; i < length; ++i)
        if (static_cast<unsigned char>(bytes[i]) != signature[i]) return false;
    return true;
}

std::uint64_t hash_bytes(std::span<const std::byte> bytes)
{
    std::uint64_t h = kImageSeed;
    for (const std::byte b : bytes) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(b));
        h *= 1099511628211ULL;
    }
    return h;
}

} // namespace

const char* image_format_name(ImageFormat f) noexcept
{
    switch (f) {
    case ImageFormat::Png: return "png";
    case ImageFormat::Jpeg: return "jpeg";
    case ImageFormat::Svg: return "svg";
    }
    return "?";
}

namespace {

/// True when the bytes open an SVG document.
///
/// SVG has no magic number — it is XML — so the check is for the root element,
/// skipping whatever prolog, doctype or comment precedes it. Read from the BYTES
/// like the other two: a file named `.svg` that holds a JPEG is a JPEG, and the
/// renderer that trusted the name would draw nothing and say nothing.
///
/// Bounded to the first kilobyte. A hostile file with a megabyte of comments
/// before its root element is not a picture this program has to accept.
bool looks_like_svg(std::span<const std::byte> bytes)
{
    const std::size_t look = bytes.size() < 1024 ? bytes.size() : 1024;
    const std::string_view head(reinterpret_cast<const char*>(bytes.data()), look);

    const std::size_t at = head.find("<svg");
    if (at == std::string_view::npos) return false;

    // Only a prolog may precede it. Anything else means the `<svg` found is a
    // string inside some other document rather than this document's root.
    const std::string_view before = head.substr(0, at);
    return before.find_first_not_of(" \t\r\n") == std::string_view::npos ||
           before.find("<?xml") != std::string_view::npos ||
           before.find("<!DOCTYPE") != std::string_view::npos ||
           before.find("<!--") != std::string_view::npos;
}

} // namespace

Result<ImageFormat> sniff_image_format(std::span<const std::byte> bytes)
{
    if (starts_with(bytes, kPngSignature, sizeof kPngSignature)) return ImageFormat::Png;
    if (starts_with(bytes, kJpegSignature, sizeof kJpegSignature)) return ImageFormat::Jpeg;
    if (looks_like_svg(bytes)) return ImageFormat::Svg;

    return err(ErrorCode::ValidationFailed,
               "Görsel tanınmadı: baytlar ne PNG, ne JPEG, ne de SVG imzasıyla "
               "başlıyor. Dosya uzantısına bakılmaz; imza dosyayı yazanın "
               "beyanıdır, uzantı ise adını koyanın.");
}

ImageStore::ImageStore()
{
    // Slot 0 is the "no image" sentinel and exists before anything is interned,
    // so a symbol layer that references no picture costs a zero.
    images_.push_back(Entry{});
}

Result<ImageId> ImageStore::intern(std::span<const std::byte> bytes, std::string_view origin)
{
    if (bytes.empty())
        return err(ErrorCode::ValidationFailed, "Görsel boş: '" + std::string(origin) + "'.");

    if (bytes.size() > kMaxImageBytes)
        return err(ErrorCode::ValidationFailed, "Görsel çok büyük: '" + std::string(origin) +
                                                    "', " + std::to_string(bytes.size()) +
                                                    " bayt. Üst sınır " +
                                                    std::to_string(kMaxImageBytes) + " bayt.");

    auto format = sniff_image_format(bytes);
    if (!format) return format.error();

    // Deduplicated by CONTENT: one hatch shared by nine plan types is stored once.
    // The hash buckets candidates and the comparison is exact, because a hash
    // collision must not make two different pictures the same picture.
    const std::uint64_t key = hash_bytes(bytes);
    if (const auto it = by_content_.find(key); it != by_content_.end()) {
        for (const ImageId candidate : it->second) {
            const Entry& existing = images_[candidate];
            if (existing.bytes.size() != bytes.size()) continue;
            if (std::memcmp(existing.bytes.data(), bytes.data(), bytes.size()) == 0)
                return candidate;
        }
    }

    if (images_.size() > 0xFFFEu)
        return err(ErrorCode::Internal,
                   "Bir çizim en çok 65534 görsel taşıyabilir; bu sınır aşıldı.");

    const auto id = static_cast<ImageId>(images_.size());

    Entry entry;
    entry.bytes.assign(bytes.begin(), bytes.end());
    entry.origin = std::string(origin);
    entry.format = format.value();
    entry.key    = key;

    total_bytes_ += entry.bytes.size();
    images_.push_back(std::move(entry));
    by_content_[key].push_back(id);
    return id;
}

std::span<const std::byte> ImageStore::bytes(ImageId id) const
{
    if (id == kNoImage || id >= images_.size()) return {};
    return images_[id].bytes;
}

ImageFormat ImageStore::format(ImageId id) const noexcept
{
    return id < images_.size() ? images_[id].format : ImageFormat::Png;
}

std::string_view ImageStore::origin(ImageId id) const
{
    if (id == kNoImage || id >= images_.size()) return {};
    return images_[id].origin;
}

std::uint64_t ImageStore::content_key(ImageId id) const noexcept
{
    return id < images_.size() ? images_[id].key : 0;
}

std::uint64_t ImageStore::fold(std::uint64_t seed) const
{
    // A store holding only the sentinel folds to the seed UNCHANGED, so every
    // file and fixture written before images existed keeps its fingerprint.
    if (images_.size() <= 1) return seed;

    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(images_.size()), seed ^ kImageSeed);
    for (std::size_t i = 1; i < images_.size(); ++i) {
        h = fnv1a_int(static_cast<std::int64_t>(i), h);
        h = fnv1a_int(static_cast<std::int64_t>(images_[i].format), h);
        h = fnv1a(images_[i].origin, h);

        // The BYTES, not just their length: two drawings referencing the same
        // number of pictures but different pictures are different drawings.
        h = images_[i].key ^ (h * 1099511628211ULL);
    }
    return h;
}

} // namespace kentos::core
