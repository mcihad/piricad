// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/text_store.hpp"

#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <array>

namespace kentos::core {
namespace {
// THE SEED DOES NOT FOLLOW THE PRODUCT'S NAME, and must not. It is folded into
// every content hash this program has ever computed — golden fixtures, journal
// fingerprints, the equality proof — so renaming it would silently change what
// every stored drawing hashes to. The string is an arbitrary constant that
// happens to read as the old name; that is all it has ever been.

constexpr std::uint64_t kTextSeed = fnv1a("piricad.core.text");

/// A height of zero would draw nothing and a negative one is not a height. The
/// upper bound is the same coordinate limit every stored length obeys.
constexpr Mm kMaxTextHeight = static_cast<Mm>(std::uint64_t{1} << 40U);

} // namespace

const char* text_anchor_name(TextAnchor a) noexcept
{
    switch (a) {
    case TextAnchor::BaselineLeft: return "sol";
    case TextAnchor::BaselineCentre: return "orta";
    case TextAnchor::BaselineRight: return "sag";
    case TextAnchor::MiddleCentre: return "merkez";
    case TextAnchor::TopLeft: return "ust_sol";
    case TextAnchor::TopCentre: return "ust_orta";
    case TextAnchor::TopRight: return "ust_sag";
    case TextAnchor::MiddleLeft: return "orta_sol";
    case TextAnchor::MiddleRight: return "orta_sag";
    }
    return "?";
}

int text_anchor_column(TextAnchor a) noexcept
{
    switch (a) {
    case TextAnchor::BaselineCentre:
    case TextAnchor::MiddleCentre:
    case TextAnchor::TopCentre: return 1;
    case TextAnchor::BaselineRight:
    case TextAnchor::TopRight:
    case TextAnchor::MiddleRight: return 2;
    default: return 0;
    }
}

int text_anchor_row(TextAnchor a) noexcept
{
    switch (a) {
    case TextAnchor::MiddleCentre:
    case TextAnchor::MiddleLeft:
    case TextAnchor::MiddleRight: return 1;
    case TextAnchor::TopLeft:
    case TextAnchor::TopCentre:
    case TextAnchor::TopRight: return 2;
    default: return 0;
    }
}

TextAnchor text_anchor_at(int column, int row) noexcept
{
    // Rows bottom-up, columns left to right: the grid the nine values name.
    constexpr std::array<std::array<TextAnchor, 3>, 3> kGrid{{
        {TextAnchor::BaselineLeft, TextAnchor::BaselineCentre, TextAnchor::BaselineRight},
        {TextAnchor::MiddleLeft, TextAnchor::MiddleCentre, TextAnchor::MiddleRight},
        {TextAnchor::TopLeft, TextAnchor::TopCentre, TextAnchor::TopRight},
    }};
    const auto at = [](int v) { return static_cast<std::size_t>(std::clamp(v, 0, 2)); };
    return kGrid[at(row)][at(column)];
}

std::size_t text_characters(std::string_view utf8) noexcept
{
    std::size_t n = 0;
    for (const char c : utf8)
        n += (static_cast<unsigned char>(c) & 0xC0U) != 0x80U ? 1 : 0; // not a continuation byte
    return n;
}

namespace {

/// A line's width in thousandths of its height, by what KIND of letter each is:
/// the bundled face's own averages, measured and rounded up a little — a
/// capital 0,89, a lower-case letter 0,70, a digit 0,86, a space 0,34. A count
/// of letters at one width made every box of capitals too short to click at
/// its end, and every box of lower case too long.
std::int64_t line_width_permille(std::string_view line) noexcept
{
    std::int64_t w = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const auto c = static_cast<unsigned char>(line[i]);
        if ((c & 0xC0U) == 0x80U) continue; // a continuation byte: counted with its lead
        if (c < 0x80U) {
            if (c >= 'A' && c <= 'Z')
                w += 900;
            else if (c >= '0' && c <= '9')
                w += 860;
            else if (c == ' ')
                w += 340;
            else if (c >= 'a' && c <= 'z')
                w += 720;
            else
                w += 500; // punctuation
            continue;
        }
        // Two bytes: the Turkish capitals Ç Ö Ü (C3 87/96/9C) and Ğ İ Ş (C4 9E/B0,
        // C5 9E) are capitals; every other letter reads as a lower-case one.
        const auto next    = i + 1 < line.size() ? static_cast<unsigned char>(line[i + 1]) : 0U;
        const bool capital = (c == 0xC3U && (next == 0x87U || next == 0x96U || next == 0x9CU)) ||
                             (c == 0xC4U && (next == 0x9EU || next == 0xB0U)) ||
                             (c == 0xC5U && next == 0x9EU);
        w += capital ? 900 : 720;
    }
    return w;
}

} // namespace

Mm text_width_estimate(std::string_view utf8, Mm height) noexcept
{
    std::int64_t widest = 0;
    std::size_t at      = 0;
    while (true) {
        const std::size_t nl = utf8.find('\n', at);
        const std::size_t to = nl == std::string_view::npos ? utf8.size() : nl;
        widest = std::max(widest, line_width_permille(std::string_view(utf8.data() + at, to - at)));
        if (nl == std::string_view::npos) break;
        at = nl + 1;
    }
    return static_cast<Mm>(widest) * height / 1000;
}

std::size_t text_line_estimate(std::string_view utf8, Mm height, TextLines lines, Mm width) noexcept
{
    std::size_t count = 0;
    std::size_t at    = 0;
    while (true) {
        const std::size_t nl = utf8.find('\n', at);
        const std::size_t to = nl == std::string_view::npos ? utf8.size() : nl;
        std::size_t here     = 1;
        if (lines.wrap && width > 0) {
            const Mm wide =
                text_width_estimate(std::string_view(utf8.data() + at, to - at), height);
            here = std::max<std::size_t>(1, static_cast<std::size_t>((wide + width - 1) / width));
        }
        count += here;
        if (nl == std::string_view::npos) break;
        at = nl + 1;
    }
    return count;
}

void TextTable::resize(std::size_t count)
{
    count_ = count;

    // Nothing is stored yet, so there is nothing to grow. This branch is the whole
    // saving: `resize` runs once per entity created, and a document with no text
    // now pays a comparison instead of three vector resizes and thirteen bytes.
    if (ref_.empty()) return;

    ref_.resize(count, kNoText);
    height_.resize(count, 0);
    anchor_.resize(count, static_cast<std::uint8_t>(TextAnchor::BaselineLeft));
    if (!spacing_.empty()) {
        spacing_.resize(count, TextLines{}.spacing);
        wrap_.resize(count, 0);
    }
}

void TextTable::materialise()
{
    if (!ref_.empty() || count_ == 0) return;
    ref_.assign(count_, kNoText);
    height_.assign(count_, 0);
    anchor_.assign(count_, static_cast<std::uint8_t>(TextAnchor::BaselineLeft));
}

std::uint32_t TextTable::intern(std::string_view s)
{
    const std::string key(s);
    if (const auto it = intern_.find(key); it != intern_.end()) return it->second;

    // First-seen order, never hash order: the same sequence of writes has to
    // produce the same pool on every platform, because golden fixtures record it
    // (.claude/core.md P11).
    const auto id = static_cast<std::uint32_t>(pool_.size());
    pool_.push_back(key);
    intern_.emplace(key, id);
    return id;
}

Status TextTable::set(std::size_t slot, std::string_view content, Mm height, TextAnchor anchor,
                      TextLines lines)
{
    if (slot >= count_)
        return err(ErrorCode::NotFound, "Bilinmeyen metin yuvası: " + std::to_string(slot));

    if (height <= 0 || height > kMaxTextHeight)
        return err(ErrorCode::ValidationFailed,
                   "Yazı yüksekliği " + std::to_string(height) +
                       " mm geçersiz; sıfırdan büyük olmalı ve zemin sınırını aşmamalı.");
    if (static_cast<std::uint8_t>(anchor) >= kTextAnchorCount)
        return err(ErrorCode::ValidationFailed,
                   "Bilinmeyen yazı hizası: " + std::to_string(static_cast<int>(anchor)));
    if (lines.spacing < kTextSpacingMin || lines.spacing > kTextSpacingMax)
        return err(ErrorCode::ValidationFailed, "Satır aralığı " +
                                                    std::to_string(lines.spacing / 1000.0) +
                                                    " geçersiz; 0,25 ile 4 arasında olmalı.");

    materialise();
    ref_[slot]    = intern(content);
    height_[slot] = height;
    anchor_[slot] = static_cast<std::uint8_t>(anchor);
    // The layout columns come into being with the first text that needs them.
    if (spacing_.empty() && lines != TextLines{}) {
        spacing_.assign(count_, TextLines{}.spacing);
        wrap_.assign(count_, 0);
    }
    if (!spacing_.empty()) {
        spacing_[slot] = lines.spacing;
        wrap_[slot]    = lines.wrap ? 1 : 0;
    }
    return ok();
}

void TextTable::clear(std::size_t slot)
{
    if (slot >= ref_.size()) return;
    ref_[slot]    = kNoText;
    height_[slot] = 0;
    if (slot < spacing_.size()) {
        spacing_[slot] = TextLines{}.spacing;
        wrap_[slot]    = 0;
    }
}

bool TextTable::has(std::size_t slot) const noexcept
{
    return slot < ref_.size() && ref_[slot] != kNoText;
}

std::string_view TextTable::text(std::size_t slot) const
{
    if (!has(slot)) return {};
    return pool_[ref_[slot]];
}

Mm TextTable::height(std::size_t slot) const noexcept
{
    return slot < height_.size() ? height_[slot] : 0;
}

TextAnchor TextTable::anchor(std::size_t slot) const noexcept
{
    return slot < anchor_.size() ? static_cast<TextAnchor>(anchor_[slot])
                                 : TextAnchor::BaselineLeft;
}

TextLines TextTable::lines(std::size_t slot) const noexcept
{
    if (slot >= spacing_.size()) return TextLines{};
    return TextLines{spacing_[slot], wrap_[slot] != 0};
}

std::uint64_t TextTable::fold(std::uint64_t seed) const
{
    std::vector<std::uint32_t> every(ref_.size());
    for (std::size_t i = 0; i < every.size(); ++i)
        every[i] = static_cast<std::uint32_t>(i);
    // `count_` rather than the list's length: a table not yet materialised has
    // slots and no storage, and folds to the seed either way.
    if (every.size() != count_) return seed;
    return fold(seed, every);
}

std::uint64_t TextTable::fold(std::uint64_t seed, std::span<const std::uint32_t> slots) const
{
    // A document with no text folds to the seed UNCHANGED, the same decision the
    // attribute table makes: a drawing that carries no text is the drawing it was
    // before text existed, so every file and fixture written before this keeps its
    // fingerprint.
    bool any = false;
    for (const std::uint32_t s : slots)
        if (s < ref_.size() && ref_[s] != kNoText) {
            any = true;
            break;
        }
    if (!any) return seed;

    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(slots.size()), seed ^ kTextSeed);
    for (const std::uint32_t i : slots) {
        if (i >= ref_.size() || ref_[i] == kNoText) {
            // Folded distinctly from an empty string: "this entity is not text"
            // and "this text entity says nothing" are different drawings.
            h = fnv1a_int(-1, h);
            continue;
        }
        h = fnv1a(pool_[ref_[i]], h);
        h = fnv1a_int(height_[i], h);
        h = fnv1a_int(anchor_[i], h);
        // A default layout folds to nothing, so every text written before line
        // spacing and wrapping existed keeps its fingerprint.
        if (const TextLines l = lines(i); l != TextLines{}) {
            h = fnv1a_int(l.spacing, h);
            h = fnv1a_int(l.wrap ? 1 : 0, h);
        }
    }
    return h;
}

} // namespace kentos::core
