// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/text_store.hpp"

#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/text.hpp"

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
constexpr Mm kMaxTextHeight = Mm{1} << 40;

} // namespace

const char* text_anchor_name(TextAnchor a) noexcept
{
    switch (a) {
    case TextAnchor::BaselineLeft: return "sol";
    case TextAnchor::BaselineCentre: return "orta";
    case TextAnchor::BaselineRight: return "sag";
    case TextAnchor::MiddleCentre: return "merkez";
    }
    return "?";
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

Status TextTable::set(std::size_t slot, std::string_view content, Mm height, TextAnchor anchor)
{
    if (slot >= count_)
        return err(ErrorCode::NotFound, "Bilinmeyen metin yuvası: " + std::to_string(slot));

    if (height <= 0 || height > kMaxTextHeight)
        return err(ErrorCode::ValidationFailed,
                   "Yazı yüksekliği " + std::to_string(height) +
                       " mm geçersiz; sıfırdan büyük olmalı ve zemin sınırını aşmamalı.");

    materialise();
    ref_[slot]    = intern(content);
    height_[slot] = height;
    anchor_[slot] = static_cast<std::uint8_t>(anchor);
    return ok();
}

void TextTable::clear(std::size_t slot)
{
    if (slot >= ref_.size()) return;
    ref_[slot]    = kNoText;
    height_[slot] = 0;
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

std::uint64_t TextTable::fold(std::uint64_t seed) const
{
    // A document with no text folds to the seed UNCHANGED, the same decision the
    // attribute table makes: a drawing that carries no text is the drawing it was
    // before text existed, so every file and fixture written before this keeps its
    // fingerprint.
    bool any = false;
    for (std::uint32_t r : ref_)
        if (r != kNoText) {
            any = true;
            break;
        }
    if (!any) return seed;

    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(count_), seed ^ kTextSeed);
    for (std::size_t i = 0; i < ref_.size(); ++i) {
        if (ref_[i] == kNoText) {
            // Folded distinctly from an empty string: "this entity is not text"
            // and "this text entity says nothing" are different drawings.
            h = fnv1a_int(-1, h);
            continue;
        }
        h = fnv1a(pool_[ref_[i]], h);
        h = fnv1a_int(height_[i], h);
        h = fnv1a_int(anchor_[i], h);
    }
    return h;
}

} // namespace kentos::core
