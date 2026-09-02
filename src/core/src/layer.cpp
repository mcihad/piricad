// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/layer.hpp"

#include "kentos_cad/core/text.hpp"

namespace kentos::core {

// Defined in style.cpp. The appearance mixing has to exist exactly once: a second
// copy would drift from the first and silently change every golden fixture that
// contains a layer. It belongs in style.hpp as a free function — see the note in
// the review — and is declared here only to leave that settled header untouched.
std::uint64_t fold_appearance(const Appearance& a, std::uint64_t seed);

namespace {

/// The key reserved for layer "0". A KeyAllocator starts minting at 1, so the
/// default layer occupies exactly the key a fresh allocator would hand out first
/// — and `add()` tells the allocator about it before minting, so the reservation
/// can never turn into a reused key (R4).
constexpr std::uint64_t kDefaultLayerKey = 1;
// THE SEED DOES NOT FOLLOW THE PRODUCT'S NAME, and must not. It is folded into
// every content hash this program has ever computed — golden fixtures, journal
// fingerprints, the equality proof — so renaming it would silently change what
// every stored drawing hashes to. The string is an arbitrary constant that
// happens to read as the old name; that is all it has ever been.

constexpr std::uint64_t kLayerSeed = fnv1a("piricad.core.layer");

/// Linear, because the table holds hundreds of layers and `add` is not a hot
/// path; a scan over the vector is also order-deterministic, which a scan over
/// `by_key_` would only accidentally be (.claude/core.md P11).
std::uint64_t highest_key(const std::vector<Layer>& layers)
{
    std::uint64_t highest = 0;
    for (const auto& l : layers)
        if (raw(l.key) > highest) highest = raw(l.key);
    return highest;
}

} // namespace

LayerTable::LayerTable()
{
    // "0" is the layer every CAD lineage the users come from opens with, and an
    // empty layer table would make the first drawn entity homeless.
    Layer zero;
    zero.key    = static_cast<LayerKey>(kDefaultLayerKey);
    zero.name   = "0";
    zero.folded = turkish_upper(zero.name);

    layers_.push_back(std::move(zero));
    by_folded_.emplace(layers_.front().folded, LayerId{0});
    by_key_.emplace(raw(layers_.front().key), LayerId{0});
}

Result<LayerId> LayerTable::add(Layer layer, KeyAllocator& keys)
{
    layer.folded = turkish_upper(layer.name);
    if (layer.folded.empty())
        return err(ErrorCode::InvalidArgument, "Katman adı boş olamaz; en az bir karakter girin.");

    if (const auto it = by_folded_.find(layer.folded); it != by_folded_.end())
        return err(ErrorCode::ValidationFailed,
                   "'" + layer.name + "' adı, mevcut '" + layers_[it->second].name +
                       "' katmanıyla aynı ada karşılık geliyor (büyük/küçük harf ayrımı "
                       "yapılmaz). Farklı bir ad verin ya da mevcut katmanı kullanın.");

    // The table may already hold keys this allocator never minted: the default
    // layer "0" above, or every layer of a file that has just been loaded.
    // adopt_layer is the declared way to say so, and it is what keeps "a retired
    // or live key is never handed out twice" true no matter which allocator the
    // caller brings (R4, identity.hpp).
    keys.adopt_layer(static_cast<LayerKey>(highest_key(layers_)));

    const LayerKey minted = keys.mint_layer();
    if (minted == LayerKey::None)
        return err(ErrorCode::Internal,
                   "Katman anahtarı alanı tükendi; belge yeni katman kabul edemiyor.");
    layer.key = minted;

    const auto slot = static_cast<LayerId>(layers_.size());
    layers_.push_back(std::move(layer));
    by_folded_.emplace(layers_[slot].folded, slot);
    by_key_.emplace(raw(layers_[slot].key), slot);
    return slot;
}

LayerId LayerTable::find(std::string_view name) const
{
    // Folded, so "parsel", "PARSEL" and "Parsel" are one layer — and so that
    // "ışık" and "IŞIK" are one while "isik" is not, which std::toupper would get
    // backwards (CLAUDE.md 5.6).
    const auto it = by_folded_.find(turkish_upper(name));
    return it != by_folded_.end() ? it->second : kNoLayer;
}

LayerId LayerTable::slot_of(LayerKey key) const
{
    const auto it = by_key_.find(raw(key));
    return it != by_key_.end() ? it->second : kNoLayer;
}

LayerKey LayerTable::key_of(LayerId slot) const noexcept
{
    return slot < layers_.size() ? layers_[slot].key : LayerKey::None;
}

const Layer* LayerTable::at(LayerId slot) const noexcept
{
    return slot < layers_.size() ? &layers_[slot] : nullptr;
}

Layer* LayerTable::at(LayerId slot) noexcept
{
    return slot < layers_.size() ? &layers_[slot] : nullptr;
}

Status LayerTable::rename(LayerId slot, std::string name)
{
    if (slot >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(slot));

    std::string folded = turkish_upper(name);
    if (folded.empty())
        return err(ErrorCode::InvalidArgument, "Katman adı boş olamaz; en az bir karakter girin.");

    // A layer may be renamed onto its own folded name — "parsel" -> "Parsel" is a
    // presentation change and must be allowed.
    if (const auto it = by_folded_.find(folded); it != by_folded_.end() && it->second != slot)
        return err(ErrorCode::ValidationFailed,
                   "'" + name + "' adı, mevcut '" + layers_[it->second].name +
                       "' katmanıyla aynı ada karşılık geliyor (büyük/küçük harf ayrımı "
                       "yapılmaz). Farklı bir ad verin.");

    // Name and folded name, and nothing else. The key does not move, no entity is
    // touched and no index over entities is invalidated, because nothing outside
    // this table ever stores a layer by name — that is the whole content of R30.
    by_folded_.erase(layers_[slot].folded);
    layers_[slot].name   = std::move(name);
    layers_[slot].folded = std::move(folded);
    by_folded_.emplace(layers_[slot].folded, slot);
    return ok();
}

bool LayerTable::visible_at(LayerId slot, ScaleDenominator scale) const
{
    const Layer* l = at(slot);
    if (l == nullptr) return false;
    if (!l->visible) return false;

    // A denominator grows as the view zooms OUT: 1:25000 is further out than
    // 1:1000. min_scale is therefore the zoomed-out limit and bounds the
    // denominator from ABOVE; max_scale is the zoomed-in limit and bounds it from
    // below. Zero means unbounded on that side.
    //
    // Both limits are INCLUSIVE: a layer with min_scale 25000 is still drawn at
    // exactly 1:25000 and disappears at 1:25001.
    if (l->min_scale != 0 && scale > l->min_scale) return false;
    if (l->max_scale != 0 && scale < l->max_scale) return false;
    return true;
}

std::uint64_t LayerTable::fold(std::uint64_t seed) const
{
    // Every stored field of every record, in slot order. The rule is deliberately
    // "all of them": a stored field left out of the fingerprint is a field a save
    // can change without the document noticing (.claude/model.md, Definitions of
    // Done). `name` is folded alongside `folded` because the file keeps the casing
    // the user typed, so "parsel" -> "Parsel" really is a document change.
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(kLayerSeed), seed);
    for (const auto& l : layers_) {
        h = fnv1a_int(static_cast<std::int64_t>(raw(l.key)), h);
        h = fnv1a(l.name, h);
        h = fnv1a(l.folded, h);
        h = fnv1a(l.description, h);
        h = fnv1a_int(l.visible ? 1 : 0, h);
        h = fnv1a_int(l.locked ? 1 : 0, h);
        h = fnv1a_int(l.plottable ? 1 : 0, h);
        h = fold_appearance(l.appearance, h);
        // The zero sentinel predates full layer symbols. Leaving it out preserves
        // every legacy document fingerprint; a real layer symbol is content and
        // must change it.
        if (l.style != kByLayerStyle) h = fnv1a_int(static_cast<std::int64_t>(l.style), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.min_scale), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.max_scale), h);
        h = fnv1a_int(static_cast<std::int64_t>(l.opacity), h);
        h = fnv1a(l.catalog_ref, h);
        h = fnv1a(l.group, h);
    }
    return h;
}

} // namespace kentos::core
