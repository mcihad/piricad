// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/document.hpp"

#include "kentos_cad/core/block_reference.hpp"

#include "kentos_cad/core/pick.hpp"

#include "kentos_cad/core/entity_kind.hpp"
#include "kentos_cad/core/spatial_index.hpp"
#include "kentos_cad/core/text.hpp"

#include <algorithm>
#include <limits>

namespace kentos::core {
namespace {

/// The flag bits that are CONTENT. `FlagLayerHidden` is not one of them: it is
/// mirrored from the layer and refreshed on every toggle, so folding it in would
/// make hiding a layer change the drawing's fingerprint (model.md R6).
constexpr std::uint8_t kFlagsHashed = FlagAlive | FlagHidden | FlagInBlock;

/// Grows geometrically, so appending N entities is O(N) rather than O(N²).
/// Calling reserve(size() + n) unconditionally pins capacity to the exact size
/// and turns a bulk load into quadratic copying; that cost was measured once.
template<class T> void reserve_for(std::vector<T>& v, std::size_t extra)
{
    const std::size_t needed = v.size() + extra;
    if (needed <= v.capacity()) return;
    v.reserve(std::max(needed, v.capacity() * 2));
}

} // namespace

Document::~Document()                              = default;
Document::Document(Document&&) noexcept            = default;
Document& Document::operator=(Document&&) noexcept = default;

Document::Document()
{
    // LayerTable creates layer "0" itself — the default every CAD lineage opens
    // with — so the per-layer counters are sized from the table rather than
    // assumed empty. Getting this wrong wrote past the end of layer_live_.
    layer_live_.assign(layers_.size(), 0);
}

// ------------------------------------------------------------------ read ----

LayerId Document::find_layer(std::string_view name) const
{
    return layers_.find(name);
}

std::size_t Document::layer_entity_count(LayerId l) const noexcept
{
    return l < layer_live_.size() ? layer_live_[l] : 0;
}

Box2 Document::entity_extent(EntityId e) const
{
    return (e < entities_.size() && entities_.alive(e)) ? entities_.box_of(e) : Box2{};
}

Box2 Document::extent() const
{
    Box2 b{};
    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (!entities_.visible(e)) continue;

        if (b.empty()) {
            b = entities_.box_of(e);
            continue;
        }
        if (entities_.min_x[e] < b.min_x) b.min_x = entities_.min_x[e];
        if (entities_.min_y[e] < b.min_y) b.min_y = entities_.min_y[e];
        if (entities_.max_x[e] > b.max_x) b.max_x = entities_.max_x[e];
        if (entities_.max_y[e] > b.max_y) b.max_y = entities_.max_y[e];
    }
    return b;
}

Mm2 Document::entity_area(EntityId e) const
{
    if (e >= entities_.size() || !entities_.alive(e)) return 0;

    // THE KIND ANSWERS, for the reason `AreaFn` gives: a circle's rings are a
    // centre and a handle, and the shoelace over those is zero. The polyline
    // reads straight from the arena, which is the same answer without the
    // table lookup, and it is every entity on the five-million-parcel sheet.
    const KindId kind = entities_.kind[e];
    if (kind != kPolylineKind) {
        if (const KindSpec* spec = builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{entities_.slot[e]};
            Mm2 out = 0;
            spec->area(geometry_, SlotSpan(one, 1), std::span<Mm2>(&out, 1));
            return out;
        }
    }
    return geometry_.area_of(entities_.slot[e]);
}

Mm Document::entity_perimeter(EntityId e) const
{
    if (e >= entities_.size() || !entities_.alive(e)) return 0;
    const KindId kind = entities_.kind[e];
    if (kind != kPolylineKind) {
        if (const KindSpec* spec = builtin_kinds().find(kind);
            spec != nullptr && spec->perimeter != nullptr) {
            const std::uint32_t one[1]{entities_.slot[e]};
            Mm out = 0;
            spec->perimeter(geometry_, SlotSpan(one, 1), std::span<Mm>(&out, 1));
            return out;
        }
    }
    return geometry_.perimeter_of(entities_.slot[e]);
}

bool Document::kind_known(EntityId e) const noexcept
{
    if (e >= entities_.size()) return false;
    const KindId kind = entities_.kind[e];
    return kind == kPolylineKind || builtin_kinds().find(kind) != nullptr;
}

std::uint64_t Document::content_hash() const
{
    // Keys are deliberately excluded: two documents built the same way from the
    // same input must agree, and key assignment is an allocation detail (§7.3).
    std::uint64_t h = fnv1a(crs_.id());
    h               = layers_.fold(h);
    h               = styles_.fold(h);

    // THE ROWS THAT ARE THE DOCUMENT'S: all of them, less the members an
    // external reference loaded (R45a) — those are its file's, remade on every
    // load, and the project file holds none of them. Where a row stands among
    // the others is what names it below, so a caption drawn after a reference
    // was loaded fingerprints the same once the file is read back without it.
    const std::vector<bool> external = external_rows();
    std::vector<std::uint32_t> position;
    if (!external.empty()) {
        position.assign(entities_.size(), std::numeric_limits<std::uint32_t>::max());
        std::uint32_t next = 0;
        for (EntityId e = 0; e < entities_.size(); ++e)
            if (!external[e]) position[e] = next++;
    }

    // Attributes ARE document content: an ada number is not a view preference,
    // it is what the parcel is. Folding the table once here rather than per
    // entity keeps the loop below reading only the hot columns, and folds the
    // SCHEMA too — declaring a column changes what the document says it holds
    // even before a single cell is written.
    //
    // THE SLOT TABLES FOLD BY ROW: each entity's caption, cells and foreign
    // data at the slot its geometry holds now, in row order. A geometry edit
    // appends a slot and leaves the old one behind for undo; folded by slot,
    // that history made a drawing whose caption had been moved fingerprint
    // differently from the same drawing read back from its file — which holds
    // no history. A drawing never edited has one slot per row, in row order,
    // and folds exactly as it did when these folded every slot.
    const std::vector<std::uint32_t> rows = row_slots();
    h                                     = attributes_.fold(h, rows);
    h                                     = texts_.fold(h, rows);
    h                                     = images_.fold(h);
    h                                     = dashes_.fold(h);
    h                                     = foreign_.fold(h, rows);
    h                                     = blocks_.fold(h);
    h                                     = attachments_.fold(h, position);
    h                                     = dim_links_.fold(h, position);
    h                                     = hatch_links_.fold(h, position);
    h                                     = lineage_.fold(h, position);

    // THE PAFTA IS CONTENT. A drawing whose sheet layout differs is a different
    // deliverable, even when every parcel in it is identical. Folding an EMPTY
    // store returns the seed untouched, so every fingerprint written before
    // layouts existed still stands (the same bargain the kind fold makes below).
    h = layouts_.fold(h);

    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (!entities_.alive(e) || (!external.empty() && external[e])) continue;

        h = fnv1a(layers_.all()[entities_.layer[e]].folded, h);
        h = fnv1a_int(static_cast<std::int64_t>(entities_.style[e]), h);
        h = fnv1a_int(entities_.flags[e] & kFlagsHashed, h);

        // THE KIND IS CONTENT: a circle and a two-vertex line hold the same two
        // vertices and are not the same drawing. Folded only for a kind other
        // than the polyline, so every file written before kinds were hashed —
        // all of them parcels and lines — keeps the fingerprint in its header.
        // The payload is folded byte for byte, for the same reason and with the
        // same bargain: a slot without one adds nothing (model.md R9a).
        if (entities_.kind[e] != kPolylineKind)
            h = fnv1a_int(static_cast<std::int64_t>(entities_.kind[e]), h);
        if (const auto bytes = geometry_.payload_of(entities_.slot[e]); !bytes.empty())
            h = fnv1a(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                      h);

        const RingSpan span = geometry_.rings_of(entities_.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            h = fnv1a_int(static_cast<std::int64_t>(geometry_.ring_role[r]), h);
            h = fnv1a_int(geometry_.ring_part[r], h);

            const auto xs = geometry_.ring_xs(r);
            const auto ys = geometry_.ring_ys(r);
            for (std::size_t i = 0; i < xs.size(); ++i) {
                h = fnv1a_int(xs[i], h);
                h = fnv1a_int(ys[i], h);
            }
            h = fnv1a_int(-2, h); // ring terminator
        }
        h = fnv1a_int(-1, h); // entity terminator
    }
    return h;
}

std::uint64_t Document::content_revision(EntityId e) const
{
    // A NEW seed, carrying the program's present name (CLAUDE.md 0.5a froze
    // only the seeds already folded into fixtures).
    static constexpr std::uint64_t kRevisionSeed = fnv1a("kentos.core.content_revision");
    if (e >= entities_.size()) return kRevisionSeed;
    const std::uint32_t slot = entities_.slot[e];
    std::uint64_t h = fnv1a_int(static_cast<std::int64_t>(entities_.kind[e]), kRevisionSeed);
    if (const auto bytes = geometry_.payload_of(slot); !bytes.empty()) {
        h = fnv1a_int(static_cast<std::int64_t>(bytes.size()), h);
        h = fnv1a(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), h);
    }
    const RingSpan span = geometry_.rings_of(slot);
    h                   = fnv1a_int(static_cast<std::int64_t>(span.count), h);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        h             = fnv1a_int(static_cast<std::int64_t>(geometry_.ring_role[r]), h);
        h             = fnv1a_int(geometry_.ring_part[r], h);
        const auto xs = geometry_.ring_xs(r);
        const auto ys = geometry_.ring_ys(r);
        h             = fnv1a_int(static_cast<std::int64_t>(xs.size()), h);
        for (std::size_t i = 0; i < xs.size(); ++i) {
            h = fnv1a_int(xs[i], h);
            h = fnv1a_int(ys[i], h);
        }
    }
    h = texts_.fold(h, std::span<const std::uint32_t>(&slot, 1));
    return attributes_.fold_cells(h, slot);
}

std::vector<std::uint32_t> Document::row_slots() const
{
    const std::vector<bool> external = external_rows();
    if (external.empty()) return {entities_.slot.begin(), entities_.slot.end()};
    std::vector<std::uint32_t> out;
    out.reserve(entities_.size());
    for (EntityId e = 0; e < entities_.size(); ++e)
        if (!external[e]) out.push_back(entities_.slot[e]);
    return out;
}

bool Document::skip_entity_keys_to(EntityKey next) noexcept
{
    if (raw(next) == 0) return false;
    return keys_.adopt_entity(static_cast<EntityKey>(raw(next) - 1));
}

std::vector<bool> Document::external_rows() const
{
    std::vector<bool> out;
    for (const BlockDef& def : blocks_.all()) {
        if (!def.external() || def.members.empty()) continue;
        if (out.empty()) out.assign(entities_.size(), false);
        for (const EntityKey k : def.members)
            if (const EntityId e = slot_of(k); e != kNoEntity) out[e] = true;
    }
    return out;
}

// -------------------------------------------------------------- identity ----

EntityKey Document::key_of(EntityId e) const noexcept
{
    return e < entities_.size() ? entities_.key[e] : EntityKey::None;
}

EntityId Document::slot_of(EntityKey k) const noexcept
{
    if (k == EntityKey::None || entities_.key.empty()) return kNoEntity;

    if (!keys_sorted_) {
        // Only reachable if a loader appended out of order; linear rather than
        // wrong, and the loader is expected to restore the invariant.
        for (EntityId e = 0; e < entities_.size(); ++e)
            if (entities_.key[e] == k) return e;
        return kNoEntity;
    }

    const auto begin = entities_.key.begin();
    const auto end   = entities_.key.end();
    const auto it =
        std::lower_bound(begin, end, k, [](EntityKey a, EntityKey b) { return raw(a) < raw(b); });
    if (it == end || *it != k) return kNoEntity;
    return static_cast<EntityId>(it - begin);
}

LayerKey Document::layer_key_of(LayerId l) const noexcept
{
    return layers_.key_of(l);
}

LayerId Document::layer_slot_of(LayerKey k) const noexcept
{
    return layers_.slot_of(k);
}

// ----------------------------------------------------------------- index ----

const SpatialIndex& Document::spatial_index() const
{
    if (!index_) index_ = std::make_unique<SpatialIndex>();

    const std::size_t total = entities_.size();
    const std::size_t added = total - indexed_upto_;

    // Rebuild when the unindexed tail has grown past 5% of what is indexed, or
    // when enough entities have been erased that the tree mostly points at
    // corpses. Otherwise the tail is short and scanning it beats repacking.
    const std::size_t growth_limit = std::max<std::size_t>(4096, indexed_upto_ / 20);
    const bool grew                = added > growth_limit;
    const bool shrank              = indexed_live_ > 1024 && live_count_ * 2 < indexed_live_;
    const bool virgin              = index_->empty() && total > 0;

    // `index_stale_` is not one of the heuristics above and is not allowed to be:
    // the others trade a stale tree for a cheaper rebuild, and a tree that is
    // merely over-inclusive costs a rejected candidate. An edited entity makes it
    // UNDER-inclusive, and no amount of scanning the tail finds an entity the tree
    // filed under a box it has left.
    if (grew || shrank || virgin || index_stale_) {
        index_->build(entities_);
        indexed_upto_ = static_cast<EntityId>(total);
        indexed_live_ = live_count_;
        index_stale_  = false;
    }
    return *index_;
}

// ------------------------------------------------------------- mutators -----

LayerId Document::ensure_layer(std::string_view name)
{
    if (const LayerId existing = layers_.find(name); existing != kNoLayer) return existing;

    Layer l;
    l.name   = std::string(name);
    l.folded = turkish_upper(name);

    auto added = layers_.add(std::move(l), keys_);
    if (!added) return kNoLayer;

    // Resized from the table, never appended blind: LayerTable owns how many
    // layers exist and the counters follow it.
    layer_live_.resize(layers_.size(), 0);
    ++revision_;
    return added.value();
}

Result<EntityId> Document::push_entity(LayerId lyr, std::uint32_t geometry_slot, KindId kind)
{
    const EntityKey key = keys_.mint_entity();
    if (key == EntityKey::None)
        return err(ErrorCode::Internal,
                   "Nesne anahtarı alanı tükendi; bu belgeye yeni nesne eklenemez.");

    const Box2 box = geometry_.bounds_of(geometry_slot);
    const auto id  = static_cast<EntityId>(entities_.size());

    reserve_for(entities_.min_x, 1);
    reserve_for(entities_.min_y, 1);
    reserve_for(entities_.max_x, 1);
    reserve_for(entities_.max_y, 1);
    reserve_for(entities_.flags, 1);
    reserve_for(entities_.layer, 1);
    reserve_for(entities_.style, 1);
    reserve_for(entities_.kind, 1);
    reserve_for(entities_.slot, 1);
    reserve_for(entities_.key, 1);

    entities_.min_x.push_back(box.min_x);
    entities_.min_y.push_back(box.min_y);
    entities_.max_x.push_back(box.max_x);
    entities_.max_y.push_back(box.max_y);

    // The layer bit is mirrored at creation and refreshed by the command that
    // toggles the layer, so the cull test stays one byte (R7).
    std::uint8_t flags = FlagAlive;
    if (const Layer* l = layers_.at(lyr); l && !l->visible) flags |= FlagLayerHidden;
    entities_.flags.push_back(flags);

    entities_.layer.push_back(lyr);
    entities_.style.push_back(kByLayerStyle);
    entities_.kind.push_back(kind);
    entities_.slot.push_back(geometry_slot);

    // The binary search in slot_of() depends on this staying true.
    if (!entities_.key.empty() && raw(entities_.key.back()) >= raw(key)) keys_sorted_ = false;
    entities_.key.push_back(key);

    // The slot-indexed side tables follow the geometry, ALWAYS.
    //
    // Growing them lazily — on the first write to each — made content_hash()
    // depend on the ORDER of a session rather than on its result: a drawing whose
    // ada number was typed before its caption hashed differently from the same
    // drawing typed the other way round, and reloading either of them produced a
    // third answer. Two documents that hold the same facts must fingerprint the
    // same, or the fingerprint is not about the document.
    attributes_.resize(geometry_.slot_count());
    texts_.resize(geometry_.slot_count());
    attachments_.resize(entities_.size());

    ++live_count_;
    ++layer_live_[lyr];
    ++revision_;
    return id;
}

namespace {

/// The bounding box of a slot AS ITS KIND SEES IT.
///
/// `RingGeometry::bounds_of` bounds the stored vertices, which for a curve are its
/// definition and not its shape: a circle's are a centre and a handle due east, so
/// the arena's answer is a flat line. Every cull, every pick prefilter and every
/// zoom-to-extents reads this box, so it is asked of the kind.
Box2 kind_bounds(const RingGeometry& geom, KindId kind, std::uint32_t slot)
{
    if (kind != kPolylineKind) {
        if (const KindSpec* spec = builtin_kinds().find(kind); spec != nullptr) {
            const std::uint32_t one[1]{slot};
            Box2 box{};
            spec->bbox(geom, SlotSpan(one, 1), std::span<Box2>(&box, 1));
            return box;
        }
    }
    return geom.bounds_of(slot);
}

} // namespace

Result<EntityId> Document::add_kind(LayerId lyr, KindId kind,
                                    std::span<const RingGeometry::RingInput> rings,
                                    std::span<const std::uint8_t> payload, Op& undo_out,
                                    BlockId in_block)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (kind == kNoKind) return err(ErrorCode::InvalidArgument, "Nesne türü atanmamış (kNoKind).");
    if (in_block != kNoBlock && in_block >= blocks_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(in_block));

    // The kind's own floor, before a byte is appended. An unknown kind has no
    // floor here and is taken as given: the arena still enforces R9–R12 on its
    // rings, and its payload is opaque by definition (R26).
    if (const KindSpec* spec = builtin_kinds().find(kind);
        spec != nullptr && spec->validate != nullptr)
        if (auto st = spec->validate(rings, payload); !st) return st.error();

    // A block reference names a definition, and the document is the one who
    // knows whether it exists and whether placing it inside `in_block` would
    // let a definition contain itself (R45).
    BlockId uses = kNoBlock;
    if (kind == kBlockReferenceKind) {
        auto ref = decode_block_reference(payload);
        if (!ref) return ref.error();
        uses = ref.value().block;
        if (uses >= blocks_.size())
            return err(ErrorCode::NotFound,
                       "Blok referansı tanımsız bir bloğu gösteriyor: " + std::to_string(uses));
        if (in_block != kNoBlock && blocks_.would_cycle(in_block, uses))
            return err(ErrorCode::ValidationFailed,
                       "'" + blocks_.at(in_block).name +
                           "' bloğu kendini içerecekti; bir blok tanımı kendine referans veremez.");
    }

    auto slot = geometry_.append(rings, payload);
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kind);
    if (!id) return id;

    // The box of the SHAPE, not of the definition vertices: a circle's are a
    // centre and a handle due east, an ellipse's a triangle strictly inside it.
    // Every cull, pick prefilter and zoom-to-extents reads this box.
    const EntityId e   = id.value();
    const Box2 box     = kind_bounds(geometry_, kind, slot.value());
    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    // A definition member is never drawn on its own: the flag keeps it out of
    // the cull, the index and the pick, and the block remembers it by key.
    if (in_block != kNoBlock) {
        entities_.flags[e] |= FlagInBlock;
        if (auto st = blocks_.add_member(in_block, entities_.key[e], uses); !st) return st.error();
    }

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Status Document::editable(EntityId e) const
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument, "Silinmiş nesne düzenlenemez: " + std::to_string(e));
    if (!kind_known(e))
        return err(ErrorCode::Unsupported,
                   "Bu yapının tanımadığı türdeki nesne düzenlenemez; olduğu gibi korunur.");
    if ((entities_.flags[e] & FlagInBlock) != 0) {
        // A REFERENCE'S MEMBER IS NOT THIS DRAWING'S TO EDIT: it comes from its
        // file and is read again from there, so the answer names the file and
        // the two ways out — not BLOKDÜZENLE, which refuses a reference.
        const EntityKey key = entities_.key[e];
        for (BlockId b = 0; b < blocks_.size(); ++b) {
            const BlockDef& def = blocks_.at(b);
            if (!def.external() || std::ranges::find(def.members, key) == def.members.end())
                continue;
            const std::string_view file = file_name_of(def.path);
            return err(ErrorCode::ValidationFailed,
                       "Bu nesne '" + def.name + "' dış referansının parçası" +
                           (file.empty() ? std::string() : " ('" + std::string(file) + "')") +
                           "; kendi dosyasında düzenlenir ve yenilenince oradan yeniden okunur. "
                           "Burada düzenlemek için bir yerel kopyasını alın (YERELKOPYA) ya da "
                           "referansı çizime bağlayın: DIŞREFERANS islem=bagla ad=" +
                           def.name + ".",
                       "YERELKOPYA nesneler=" + std::to_string(raw(key)));
        }
        return err(ErrorCode::ValidationFailed,
                   "Blok tanımındaki nesne doğrudan düzenlenemez; tanımı BLOKDÜZENLE ile "
                   "açıp düzenleyin.");
    }

    // AND THE LAYER'S LOCK, which was missing and made `kilitli` mean almost
    // nothing. The lock was checked on every `add_*` and on nothing else, so a
    // locked layer stopped a user DRAWING a new parcel on it and let TAŞI,
    // KÖŞETAŞI, ESNET and PATLAT reshape every parcel already there. In a
    // cadastral drawing that is the wrong way round: what is on the sheet is what
    // the lock is for.
    //
    // Here rather than in each setter, because this is the one question every
    // in-place edit already asks. Undo and redo do NOT come through it —
    // `restore_geometry` and the other inverse paths are unguarded on purpose —
    // so locking a layer never traps an earlier edit inside the undo stack.
    const LayerId lyr = entities_.layer[e];
    if (lyr < layers_.size() && layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed, "'" + layers_.all()[lyr].name +
                                                    "' katmanı kilitli; üzerindeki nesne "
                                                    "düzenlenemez. Kilidi KATMAN ad=" +
                                                    layers_.all()[lyr].name +
                                                    " kilitli=hayır ile açın.");
    return ok();
}

Status Document::attach_foreign(EntityId e, std::string_view tag,
                                std::span<const std::uint8_t> bytes, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (auto st = foreign_.attach(entities_.slot[e], tag, bytes); !st) return st;
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::DetachForeign;
    undo_out.entity  = e;
    undo_out.str_arg = std::string(tag);
    return ok();
}

Status Document::detach_foreign(EntityId e, std::string_view tag, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    const auto was = foreign_.bytes(entities_.slot[e], tag);
    if (was.empty())
        return err(ErrorCode::NotFound, std::to_string(e) + ". nesnede '" + std::string(tag) +
                                            "' etiketli yabancı veri yok.");

    // The inverse is built BEFORE the write: it needs the bytes being removed.
    undo_out         = Op{};
    undo_out.kind    = Op::Kind::AttachForeign;
    undo_out.entity  = e;
    undo_out.str_arg = std::string(tag);
    undo_out.bytes_arg.assign(was.begin(), was.end());

    (void)foreign_.detach(entities_.slot[e], tag);
    ++revision_;
    return ok();
}

Result<BlockId> Document::add_block(std::string_view name, std::string_view description,
                                    Point2 base)
{
    auto id = blocks_.add(name, description, base);
    if (id) ++revision_;
    return id;
}

Status Document::add_block_use(BlockId block, BlockId uses)
{
    if (block >= blocks_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(block));
    if (uses >= blocks_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(uses));
    if (blocks_.would_cycle(block, uses))
        return err(ErrorCode::ValidationFailed,
                   "'" + blocks_.at(block).name + "' bloğu '" + blocks_.at(uses).name +
                       "' bloğunu içeremez: kendini içeren bir tanım sonsuza dek açılır.");
    // Recorded through the member path with no member: only the edge is new.
    BlockDef& def = const_cast<BlockDef&>(blocks_.at(block));
    if (std::find(def.uses.begin(), def.uses.end(), uses) == def.uses.end())
        def.uses.push_back(uses);
    ++revision_;
    return ok();
}

Status Document::set_kind_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                                   std::span<const std::uint8_t> payload, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Silinmiş nesnenin geometrisi değiştirilemez: " + std::to_string(e));
    if (auto st = editable(e); !st) return st;

    if (const KindSpec* spec = builtin_kinds().find(entities_.kind[e]);
        spec != nullptr && spec->validate != nullptr)
        if (auto st = spec->validate(rings, payload); !st) return st;

    auto slot = geometry_.append(rings, payload);
    if (!slot) return slot.error();

    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot.value();
    carry_side_tables(was, slot.value());
    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

Status Document::set_kind_geometry(EntityId e, KindId kind,
                                   std::span<const RingGeometry::RingInput> rings,
                                   std::span<const std::uint8_t> payload, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Silinmiş nesnenin geometrisi değiştirilemez: " + std::to_string(e));
    if (auto st = editable(e); !st) return st;
    const KindSpec* spec = builtin_kinds().find(kind);
    if (spec == nullptr)
        return err(ErrorCode::Unsupported,
                   "Bu yapının tanımadığı bir türe dönüştürülemez: " + std::to_string(kind));
    if (spec->validate != nullptr)
        if (auto st = spec->validate(rings, payload); !st) return st;

    auto slot = geometry_.append(rings, payload);
    if (!slot) return slot.error();

    const std::uint32_t was = entities_.slot[e];
    const KindId was_kind   = entities_.kind[e];
    entities_.slot[e]       = slot.value();
    entities_.kind[e]       = kind;
    carry_side_tables(was, slot.value());
    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetKindGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    undo_out.kind_arg      = was_kind;
    return ok();
}

Status Document::set_kind_payload(EntityId e, std::span<const std::uint8_t> payload, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (auto st = editable(e); !st) return st;
    return write_payload(e, payload, undo_out);
}

Status Document::refresh_reference_bounds(EntityId e, Op& undo_out)
{
    undo_out = Op{};
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e) || entities_.kind[e] != kBlockReferenceKind)
        return err(ErrorCode::InvalidArgument,
                   "Kutusu yenilenecek nesne yaşayan bir blok referansı değil: " +
                       std::to_string(e));
    auto ref = block_reference_of(geometry_, entities_.slot[e]);
    if (!ref) return ref.error();
    BlockReference next = ref.value();
    next.bounds         = block_reference_bounds(
        *this, block_reference_insertion(geometry_, entities_.slot[e]), next);
    if (next.bounds == ref.value().bounds) return ok();
    return write_payload(e, encode_block_reference(next), undo_out);
}

Status Document::set_block_base(BlockId block, Point2 base, Op& undo_out)
{
    undo_out = Op{};
    if (block >= blocks_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(block));
    const Point2 was = blocks_.at(block).base;
    if (auto st = blocks_.set_base(block, base); !st) return st;
    ++revision_;
    undo_out.kind      = Op::Kind::SetBlockBase;
    undo_out.block_arg = block;
    undo_out.point_arg = was;
    return ok();
}

Status Document::set_block_external(BlockId block, std::string path, std::uint8_t flags,
                                    Op& undo_out)
{
    undo_out = Op{};
    if (block >= blocks_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen blok kimliği: " + std::to_string(block));
    std::string was_path        = blocks_.at(block).path;
    const std::uint8_t was_flag = blocks_.at(block).flags;
    if (auto st = blocks_.set_external(block, std::move(path), flags); !st) return st;
    ++revision_;
    undo_out.kind      = Op::Kind::SetBlockExternal;
    undo_out.block_arg = block;
    undo_out.str_arg   = std::move(was_path);
    undo_out.byte_arg  = was_flag;
    return ok();
}

Status Document::move_reference(EntityId e, Point2 insertion, Op& undo_out)
{
    undo_out = Op{};
    if (e >= entities_.size() || !entities_.alive(e) || entities_.kind[e] != kBlockReferenceKind)
        return err(ErrorCode::InvalidArgument,
                   "Taşınacak nesne yaşayan bir blok referansı değil: " + std::to_string(e));
    auto ref = block_reference_of(geometry_, entities_.slot[e]);
    if (!ref) return ref.error();
    BlockReference next                     = ref.value();
    next.bounds                             = block_reference_bounds(*this, insertion, next);
    const std::vector<std::uint8_t> payload = encode_block_reference(next);
    const Point2 at[1]{insertion};
    const RingGeometry::RingInput ring{std::span<const Point2>(at, 1), RingRole::Open, 0};
    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1), payload);
    if (!slot) return slot.error();
    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot.value();
    carry_side_tables(was, slot.value());
    refresh_box(e);
    ++revision_;
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

Status Document::write_payload(EntityId e, std::span<const std::uint8_t> payload, Op& undo_out)
{
    // The same rings, re-described for `append`: the arena stores eastings and
    // northings apart, and a RingInput wants points, so they meet in one buffer
    // for the length of this call.
    const std::uint32_t was = entities_.slot[e];
    const RingSpan span     = geometry_.rings_of(was);
    std::vector<Point2> points;
    std::vector<RingGeometry::RingInput> rings;
    std::size_t total = 0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
        total += geometry_.ring_count[r];
    points.reserve(total);
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        const auto xs = geometry_.ring_xs(r);
        const auto ys = geometry_.ring_ys(r);
        for (std::size_t i = 0; i < xs.size(); ++i)
            points.push_back(Point2{xs[i], ys[i]});
    }
    std::size_t cursor = 0;
    for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
        rings.push_back(RingGeometry::RingInput{
            std::span<const Point2>(points.data() + cursor, geometry_.ring_count[r]),
            geometry_.ring_role[r], geometry_.ring_part[r]});
        cursor += geometry_.ring_count[r];
    }

    if (const KindSpec* spec = builtin_kinds().find(entities_.kind[e]);
        spec != nullptr && spec->validate != nullptr)
        if (auto st = spec->validate(rings, payload); !st) return st;

    auto slot = geometry_.append(rings, payload);
    if (!slot) return slot.error();

    entities_.slot[e] = slot.value();
    carry_side_tables(was, slot.value());
    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

Result<EntityId> Document::add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out)
{
    // THE FLOOR LIVES HERE, where the kind is known. `RingGeometry::append` allows
    // a one-vertex open ring because a `core.point` is exactly that; a polyline
    // with one vertex is a line that goes nowhere and is refused.
    if (pts.size() < 2)
        return err(ErrorCode::ValidationFailed,
                   "Bir çizgi en az iki nokta ister, verilen: " + std::to_string(pts.size()) + ".");

    const RingGeometry::RingInput ring{pts, RingRole::Open, 0};
    return add_area(lyr, std::span<const RingGeometry::RingInput>(&ring, 1), undo_out);
}

Result<EntityId> Document::add_circle(LayerId lyr, Point2 centre, Mm radius, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (radius <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Daire yarıçapı sıfırdan büyük olmalı: " + std::to_string(radius));

    // The two DEFINING vertices, not the circle's picture: centre, and a handle
    // due east of it so the radius reads back as an exact integer subtraction
    // (see core.circle in entity_kind.cpp). The drawn form is the kind's `emit`.
    const Point2 pts[2]{centre, Point2{centre.x + radius, centre.y}};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 2), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kCircleKind);
    if (!id) return id;

    // The bounding box of a circle is NOT the box of its two stored vertices: the
    // arena bounded the centre and the handle, which is a flat line to the east.
    // Every cull, every pick prefilter and every zoom-to-extents reads this, so it
    // is corrected here rather than left for the kind to answer later.
    const EntityId e   = id.value();
    entities_.min_x[e] = centre.x - radius;
    entities_.min_y[e] = centre.y - radius;
    entities_.max_x[e] = centre.x + radius;
    entities_.max_y[e] = centre.y + radius;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_ellipse(LayerId lyr, Point2 centre, Point2 major, Point2 minor,
                                       Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (centre.x == major.x && centre.y == major.y)
        return err(ErrorCode::InvalidArgument, "Elipsin birinci ekseni sıfır uzunlukta olamaz.");
    if (centre.x == minor.x && centre.y == minor.y)
        return err(ErrorCode::InvalidArgument, "Elipsin ikinci ekseni sıfır uzunlukta olamaz.");

    // The three DEFINING vertices: centre and the two axis endpoints. The
    // endpoints carry the rotation as vectors, so nothing stores an angle.
    const Point2 pts[3]{centre, major, minor};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 3), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kEllipseKind);
    if (!id) return id;

    // As for the circle: the arena bounded the three stored vertices, which for a
    // rotated ellipse is a triangle strictly inside the shape. Every cull, pick
    // prefilter and zoom-to-extents reads this box, so it is corrected here.
    const EntityId e = id.value();
    Box2 box{};
    const std::uint32_t one[1]{slot.value()};
    if (const KindSpec* spec = builtin_kinds().find(kEllipseKind); spec != nullptr)
        spec->bbox(geometry_, SlotSpan(one, 1), std::span<Box2>(&box, 1));

    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_point(LayerId lyr, Point2 at, Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");

    const Point2 pts[1]{at};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 1), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kPointKind);
    if (!id) return id;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id.value();
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_arc(LayerId lyr, Point2 centre, Mm radius, Point2 start, Point2 end,
                                   Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");
    if (radius <= 0)
        return err(ErrorCode::InvalidArgument,
                   "Yay yarıçapı sıfırdan büyük olmalı: " + std::to_string(radius));

    const Point2 pts[4]{centre, Point2{centre.x + radius, centre.y}, start, end};
    const RingGeometry::RingInput ring{std::span<const Point2>(pts, 4), RingRole::Open, 0};

    auto slot = geometry_.append(std::span<const RingGeometry::RingInput>(&ring, 1));
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kArcKind);
    if (!id) return id;

    // The box of an ARC, not of its four defining vertices: a sweep crossing due
    // north reaches higher than either end does, and the arena bounded the centre
    // and the radius handle along with them. Cull, pick and zoom-to-extents all
    // read this.
    const EntityId e     = id.value();
    const KindSpec* spec = builtin_kinds().find(kArcKind);
    if (spec != nullptr) {
        const std::uint32_t one[1]{slot.value()};
        Box2 box{};
        spec->bbox(geometry_, SlotSpan(one, 1), std::span<Box2>(&box, 1));
        entities_.min_x[e] = box.min_x;
        entities_.min_y[e] = box.min_y;
        entities_.max_x[e] = box.max_x;
        entities_.max_y[e] = box.max_y;
    }

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = false;
    return id;
}

Result<EntityId> Document::add_area(LayerId lyr, std::span<const RingGeometry::RingInput> rings,
                                    Op& undo_out)
{
    if (lyr >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(lyr));
    if (layers_.all()[lyr].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[lyr].name + "' katmanı kilitli.");

    // RingGeometry::append owns every geometric rule — ring ordering, closure,
    // minimum vertex counts, zero area, holes escaping their exterior, and the
    // coordinate limit. The document does not restate them (model.md R9–R12).
    auto slot = geometry_.append(rings);
    if (!slot) return slot.error();

    auto id = push_entity(lyr, slot.value(), kPolylineKind);
    if (!id) return id;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = id.value();
    undo_out.bool_arg = false;
    return id;
}

Status Document::set_entity_layer(EntityId e, LayerId layer, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (layer >= layers_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(layer));
    if (layers_.all()[layer].locked)
        return err(ErrorCode::ValidationFailed,
                   "'" + layers_.all()[layer].name + "' katmanı kilitli.");

    const LayerId was = entities_.layer[e];
    if (was == layer) {
        undo_out        = Op{};
        undo_out.kind   = Op::Kind::SetEntityLayer;
        undo_out.entity = e;
        undo_out.layer  = was;
        return ok();
    }

    // The per-layer live counts follow the object, or the layer panel would go on
    // reporting it where it no longer is.
    if (entities_.alive(e)) {
        --layer_live_[was];
        ++layer_live_[layer];
    }

    entities_.layer[e] = layer;

    // The mirrored visibility bit is the cull test's whole input (R7), so it is
    // re-taken from the NEW layer rather than carried over from the old one.
    if (const Layer* l = layers_.at(layer); l != nullptr && !l->visible)
        entities_.flags[e] |= FlagLayerHidden;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagLayerHidden);

    ++revision_;

    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetEntityLayer;
    undo_out.entity = e;
    undo_out.layer  = was;
    return ok();
}

Status Document::set_entity_alive(EntityId e, bool alive, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const bool was = entities_.alive(e);
    if (was != alive) {
        const std::size_t delta = alive ? 1 : static_cast<std::size_t>(-1);
        live_count_ += delta;
        layer_live_[entities_.layer[e]] += delta;
    }
    if (alive)
        entities_.flags[e] |= FlagAlive;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagAlive);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityAlive;
    undo_out.entity   = e;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_entity_hidden(EntityId e, bool hidden, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const bool was = (entities_.flags[e] & FlagHidden) != 0;
    if (hidden)
        entities_.flags[e] |= FlagHidden;
    else
        entities_.flags[e] &= static_cast<std::uint8_t>(~FlagHidden);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetEntityHidden;
    undo_out.entity   = e;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_entity_style(EntityId e, StyleId style, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!styles_.contains(style))
        return err(ErrorCode::InvalidArgument, "Bilinmeyen stil: " + std::to_string(style));

    const StyleId was  = entities_.style[e];
    entities_.style[e] = style;
    ++revision_;

    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetEntityStyle;
    undo_out.entity    = e;
    undo_out.style_arg = was;
    return ok();
}

Status Document::set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                              Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Silinmiş nesnenin geometrisi değiştirilemez: " + std::to_string(e));
    if (auto st = editable(e); !st) return st;

    // Validation is the arena's, exactly as it is for a new entity: ring order,
    // minimum vertex counts and closure are the same rules whether the geometry is
    // being created or replaced (R11) — and the kind's own, over the rings and
    // the payload it keeps (R9a): moving an arc polyline's vertices without its
    // arcs is refused where the arcs no longer fit. A refusal here writes
    // nothing, so the entity keeps the geometry it had.
    const std::span<const std::uint8_t> payload = geometry_.payload_of(entities_.slot[e]);
    if (const KindSpec* spec = builtin_kinds().find(entities_.kind[e]);
        spec != nullptr && spec->validate != nullptr)
        if (auto st = spec->validate(rings, payload); !st) return st;
    auto slot = geometry_.append(rings, payload);
    if (!slot) return slot.error();

    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot.value();
    carry_side_tables(was, slot.value());

    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

void Document::carry_attributes(std::uint32_t from, std::uint32_t to)
{
    // THE ROW MOVES WITH THE GEOMETRY. Cells are indexed by geometry slot, and a
    // geometry edit gives the entity a new slot; carried like the caption, or a
    // moved parcel, a dragged corner and a trimmed road lost every attribute
    // they had (model.md P11). The old slot keeps its row, so undoing the edit
    // brings the old values back with the old geometry.
    if (attributes_.columns() == 0 || from >= attributes_.rows()) return;
    attributes_.resize(geometry_.slot_count());
    for (std::size_t c = 0; c < attributes_.columns(); ++c) {
        const auto col = static_cast<AttrId>(c);
        auto had       = attributes_.get(col, from);
        if (!had || !had.value().present) continue;
        (void)attributes_.set(col, to, had.value());
    }
}

void Document::carry_side_tables(std::uint32_t from, std::uint32_t to)
{
    carry_text(from, to);
    carry_attributes(from, to);
    // The tag was accepted once and the target slot is new, so the copy cannot
    // be refused; a failure here would be an allocation, like the two above.
    (void)foreign_.copy_slot(from, to);
}

void Document::carry_text(std::uint32_t from, std::uint32_t to)
{
    if (!texts_.has(from)) return;
    texts_.resize(geometry_.slot_count());
    (void)texts_.set(to, texts_.text(from), texts_.height(from), texts_.anchor(from),
                     texts_.lines(from));
}

Status Document::restore_geometry(EntityId e, std::uint32_t slot, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (slot >= geometry_.slot_count())
        return err(ErrorCode::InvalidArgument,
                   "Bilinmeyen geometri yuvası: " + std::to_string(slot));

    const std::uint32_t was = entities_.slot[e];
    entities_.slot[e]       = slot;

    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    return ok();
}

Status Document::restore_kind_geometry(EntityId e, std::uint32_t slot, KindId kind, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (slot >= geometry_.slot_count())
        return err(ErrorCode::InvalidArgument,
                   "Bilinmeyen geometri yuvası: " + std::to_string(slot));

    const std::uint32_t was = entities_.slot[e];
    const KindId was_kind   = entities_.kind[e];
    entities_.slot[e]       = slot;
    entities_.kind[e]       = kind;

    refresh_box(e);
    ++revision_;

    undo_out               = Op{};
    undo_out.kind          = Op::Kind::SetKindGeometry;
    undo_out.entity        = e;
    undo_out.geometry_slot = was;
    undo_out.kind_arg      = was_kind;
    return ok();
}

// ------------------------------------------------------------ attributes ----

Result<AttrId> Document::declare_attribute(AttrSpec spec)
{
    // A CodeRef column must name a catalogue this document actually holds, or
    // its codes cannot be validated — and an unvalidatable regulatory reference
    // on a parcel is worse than no reference at all (R34).
    if (spec.type == AttrType::CodeRef && catalogues_.find(spec.catalog) == nullptr)
        return err(ErrorCode::InvalidArgument,
                   "'" + spec.id + "' özniteliği '" + spec.catalog +
                       "' kataloğuna dayanıyor ama belge o kataloğu taşımıyor.");

    auto col = attributes_.add(std::move(spec));
    if (!col) return col;

    // Rows follow slots, always. A column declared after the drawing was made
    // still has a cell for every entity in it, all of them absent.
    attributes_.resize(geometry_.slot_count());
    ++revision_;
    return col;
}

Status Document::drop_attribute(std::string_view id)
{
    const AttrId col = attributes_.find(id);
    if (col == kNoAttr)
        return err(ErrorCode::NotFound, "Bilinmeyen öznitelik: '" + std::string(id) + "'");

    if (!attributes_.remove(col))
        return err(ErrorCode::Internal, "Öznitelik silinemedi: '" + std::string(id) + "'");
    ++revision_;
    return ok();
}

Status Document::amend_attribute(std::string_view id, const AttrSpec& next)
{
    const AttrId col = attributes_.find(id);
    if (col == kNoAttr)
        return err(ErrorCode::NotFound, "Bilinmeyen öznitelik: '" + std::string(id) + "'");

    // Same rule as a declaration: a CodeRef column must name a catalogue this
    // document actually holds (R34).
    if (next.type == AttrType::CodeRef && catalogues_.find(next.catalog) == nullptr)
        return err(ErrorCode::InvalidArgument,
                   "'" + next.id + "' özniteliği '" + next.catalog +
                       "' kataloğuna dayanıyor ama belge o kataloğu taşımıyor.");

    auto amended = attributes_.amend(col, next);
    if (!amended) return amended;
    ++revision_;
    return ok();
}

Status Document::set_attribute(AttrId col, EntityId e, const AttrValue& v, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    const AttrColumn* column = attributes_.column(col);
    if (column == nullptr)
        return err(ErrorCode::NotFound, "Bilinmeyen öznitelik sütunu: " + std::to_string(col));

    // A CodeRef cell may only carry a code the catalogue knows. This is where a
    // detay kodu is checked, and it is checked against /data rather than against
    // anything written here (CLAUDE.md 5.13).
    if (column->type() == AttrType::CodeRef && v.present) {
        const Catalogue* cat = catalogues_.find(column->spec().catalog);
        if (cat == nullptr || !cat->contains(v.text))
            return err(ErrorCode::InvalidArgument,
                       "'" + v.text + "' kodu '" + column->spec().catalog +
                           "' kataloğunda yok. Katalog sürümünü ve kodu denetleyin.");
    }

    attributes_.resize(geometry_.slot_count());

    auto was = attributes_.set(col, entities_.slot[e], v);
    if (!was) return was.error();

    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetAttribute;
    undo_out.entity   = e;
    undo_out.attr_col = col;
    undo_out.attr_arg = was.value();
    undo_out.str_arg  = column->spec().id; // the column by NAME: see `apply`
    return ok();
}

Status Document::add_guide(GuideAxis axis, Mm coordinate, Op& undo_out)
{
    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetGuides;
    undo_out.guides = guides_.rows();

    guides_.add(axis, coordinate);
    ++revision_;
    return ok();
}

Status Document::add_angled_guide(Point2 through, std::int64_t angle, bool ray, Op& undo_out)
{
    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetGuides;
    undo_out.guides = guides_.rows();

    guides_.add_angled(through, angle, ray);
    ++revision_;
    return ok();
}

Status Document::remove_guide(std::size_t index, Op& undo_out)
{
    if (index >= guides_.size())
        return err(ErrorCode::NotFound, "Kılavuz yok: " + std::to_string(index) + ". Çizimde " +
                                            std::to_string(guides_.size()) + " kılavuz var.");

    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetGuides;
    undo_out.guides = guides_.rows();

    guides_.remove(index);
    ++revision_;
    return ok();
}

Status Document::set_layouts(std::vector<Layout> layouts, Op& undo_out)
{
    // VALIDATED BEFORE ANYTHING IS RECORDED. `upsert` is the floor — a name, a
    // page with area, unique item ids — and a list that fails it must leave the
    // document exactly as it was, or a refused edit would still have produced an
    // undo entry (Article 1.6).
    LayoutStore next;
    for (Layout& one : layouts)
        if (Status held = next.upsert(std::move(one)); !held) return held;

    undo_out             = Op{};
    undo_out.kind        = Op::Kind::SetLayouts;
    undo_out.layouts_arg = layouts_.all();

    layouts_ = std::move(next);
    ++revision_;
    return ok();
}

void Document::load_layouts(std::vector<Layout> layouts)
{
    layouts_.load(std::move(layouts));
    ++revision_;
}

void Document::set_guides(std::vector<GuideRow> rows)
{
    guides_.load(std::move(rows));
    ++revision_;
}

Result<AttrValue> Document::attribute(AttrId col, EntityId e) const
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    return attributes_.get(col, entities_.slot[e]);
}

Status Document::set_text(EntityId e, std::string content, Mm height, TextAnchor anchor,
                          Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    return set_text(e, std::move(content), height, anchor, texts_.lines(entities_.slot[e]),
                    undo_out);
}

Status Document::set_text(EntityId e, std::string_view content, Mm height, TextAnchor anchor,
                          TextLines lines, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));

    texts_.resize(geometry_.slot_count());
    const std::uint32_t slot = entities_.slot[e];

    // The inverse is built BEFORE the write, from what is there now. An empty
    // string with a zero height is how "there was no text here" is expressed, and
    // undo therefore detaches rather than writing an empty caption.
    undo_out             = Op{};
    undo_out.kind        = Op::Kind::SetText;
    undo_out.entity      = e;
    undo_out.str_arg     = std::string(texts_.text(slot));
    undo_out.text_height = texts_.has(slot) ? texts_.height(slot) : 0;
    undo_out.text_anchor = texts_.anchor(slot);
    undo_out.text_lines  = texts_.lines(slot);

    if (content.empty() || height <= 0) {
        texts_.clear(slot);
        refresh_box(e);
        ++revision_;
        return ok();
    }

    if (auto st = texts_.set(slot, content, height, anchor, lines); !st) return st;

    // THE BOX FOLLOWS THE TEXT, in both directions. Attaching a caption grows the
    // entity over its letters; detaching one shrinks it back to the line. A box
    // left behind either way is an entity the cull and the pick disagree with the
    // drawing about.
    refresh_box(e);
    ++revision_;
    return ok();
}

Status Document::set_attachment(EntityId e, const Attachment* a, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (!entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Silinmiş nesne bir nesneye bağlanamaz: " + std::to_string(e));
    if (a != nullptr) {
        if (auto st = editable(e); !st) return st;
        const EntityId src = slot_of(a->source);
        if (src == kNoEntity || !entities_.alive(src))
            return err(ErrorCode::NotFound, "Bağlanılacak nesne bulunamadı veya silinmiş: " +
                                                std::to_string(raw(a->source)));
        if (src == e) return err(ErrorCode::InvalidArgument, "Bir nesne kendisine bağlanamaz.");
        // A chain is allowed — a caption may follow a line that itself follows
        // something — but a cycle would make the commit-time update chase its
        // own tail. Walk up from the source; coming back to `e` is the refusal.
        EntityId cur = src;
        for (int hops = 0; hops < 64 && attachments_.has(cur); ++hops) {
            cur = slot_of(attachments_.get(cur)->source);
            if (cur == e || cur == kNoEntity)
                return err(ErrorCode::InvalidArgument,
                           "Bağ döngüsü: " + std::to_string(raw(entities_.key[e])) +
                               " zaten dolaylı olarak " + std::to_string(raw(a->source)) +
                               " tarafından izleniyor.");
        }
    }

    return restore_attachment(e, a, undo_out);
}

Status Document::restore_attachment(EntityId e, const Attachment* a, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetAttachment;
    undo_out.entity = e;
    if (const Attachment* was = attachments_.get(e); was != nullptr) {
        undo_out.has_attach = true;
        undo_out.attach_arg = *was;
    }

    if (a == nullptr) {
        if (!attachments_.clear(e)) {
            undo_out = Op{}; // followed nothing, follows nothing: no change, no inverse
            return ok();
        }
    } else {
        attachments_.set(e, *a);
    }
    ++revision_;
    return ok();
}

Status Document::retie_attachment(EntityId e, const Attachment& a, Op& undo_out)
{
    const Attachment* was = e < entities_.size() ? attachments_.get(e) : nullptr;
    if (was == nullptr || !entities_.alive(e))
        return err(ErrorCode::InvalidArgument,
                   "Bağı yenilenecek nesne bir şeyi izlemiyor: " + std::to_string(e));
    // THE SAME TIE, RENUMBERED: which object it follows is not the settle's to
    // change behind a lock, nor how it reads.
    Attachment same = a;
    same.anchor     = was->anchor;
    same.ring       = was->ring;
    same.index      = was->index;
    if (a.source != was->source || same != *was)
        return err(ErrorCode::InvalidArgument,
                   "Kilitli bir nesnenin bağında yalnız kaynağın köşe numarası değişebilir: " +
                       std::to_string(raw(entities_.key[e])));
    return restore_attachment(e, &a, undo_out);
}

Status Document::retie_dimension(EntityId dim, std::span<const DimLink> links, Op& undo_out)
{
    const std::vector<DimLink>* was = dim < entities_.size() ? dim_links_.get(dim) : nullptr;
    if (was == nullptr || was->size() != links.size())
        return err(ErrorCode::InvalidArgument,
                   "Bağı yenilenecek ölçünün bağları aynı değil: " + std::to_string(dim));
    for (std::size_t i = 0; i < links.size(); ++i)
        if (links[i].source != (*was)[i].source || links[i].point != (*was)[i].point)
            return err(
                ErrorCode::InvalidArgument,
                "Kilitli bir ölçünün bağında yalnız köşe numarası ve kopukluk değişebilir: " +
                    std::to_string(raw(entities_.key[dim])));
    return restore_dimension_links(dim, std::vector<DimLink>(links.begin(), links.end()), undo_out);
}

Status Document::retie_hatch(EntityId hatch, std::span<const HatchSource> sources, Op& undo_out)
{
    const std::vector<HatchSource>* was =
        hatch < entities_.size() ? hatch_links_.get(hatch) : nullptr;
    if (was == nullptr || was->size() != sources.size())
        return err(ErrorCode::InvalidArgument,
                   "Bağı yenilenecek taramanın sınırları aynı değil: " + std::to_string(hatch));
    for (std::size_t i = 0; i < sources.size(); ++i)
        if (sources[i].source != (*was)[i].source || (!sources[i].broken && (*was)[i].broken))
            return err(ErrorCode::InvalidArgument,
                       "Kilitli bir taramanın bağında yalnız kopukluk değişebilir: " +
                           std::to_string(raw(entities_.key[hatch])));
    return restore_hatch_links(hatch, std::vector<HatchSource>(sources.begin(), sources.end()),
                               undo_out);
}

Status Document::set_dimension_links(EntityId dim, std::span<const DimLink> links, Op& undo_out)
{
    if (dim >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(dim));
    if (!entities_.alive(dim) || entities_.kind[dim] != kDimensionKind)
        return err(ErrorCode::InvalidArgument,
                   "Yalnız canlı bir ölçü bir nesneye bağlanabilir: " + std::to_string(dim));
    if (auto st = editable(dim); !st) return st;
    const RingSpan span        = geometry_.rings_of(entities_.slot[dim]);
    const std::uint32_t points = span.count >= 2 ? geometry_.ring_count[span.first + 1] : 0;
    for (const DimLink& l : links) {
        if (l.point >= points)
            return err(ErrorCode::InvalidArgument,
                       "Ölçünün " + std::to_string(l.point + 1) + ". tanım noktası yok.");
        if (l.broken) continue;
        const EntityId src = slot_of(l.source);
        if (src == kNoEntity || !entities_.alive(src))
            return err(ErrorCode::NotFound, "Ölçünün bağlanacağı nesne bulunamadı veya silinmiş: " +
                                                std::to_string(raw(l.source)));
        if (src == dim) return err(ErrorCode::InvalidArgument, "Bir ölçü kendisine bağlanamaz.");
    }
    return restore_dimension_links(dim, std::vector<DimLink>(links.begin(), links.end()), undo_out);
}

Status Document::set_hatch_links(EntityId hatch, std::span<const HatchSource> sources, Op& undo_out)
{
    if (hatch >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(hatch));
    if (!entities_.alive(hatch) || entities_.kind[hatch] != kHatchKind)
        return err(ErrorCode::InvalidArgument,
                   "Yalnız canlı bir tarama sınırına bağlanabilir: " + std::to_string(hatch));
    if (auto st = editable(hatch); !st) return st;
    for (const HatchSource& s : sources) {
        if (s.broken) continue;
        const EntityId src = slot_of(s.source);
        if (src == kNoEntity || !entities_.alive(src))
            return err(ErrorCode::NotFound,
                       "Taramanın bağlanacağı nesne bulunamadı veya silinmiş: " +
                           std::to_string(raw(s.source)));
        if (src == hatch) return err(ErrorCode::InvalidArgument, "Bir tarama kendi sınırı olamaz.");
    }
    // NOR THROUGH ANOTHER HATCH (TODOS F-04): a hatch whose boundary is a hatch
    // that is filled, one or more hatches along, from this one would make the
    // commit-time rebuild chase its own tail. Walk every source's own sources;
    // coming back to `hatch` is the refusal.
    std::vector<EntityId> frontier;
    std::vector<EntityId> seen;
    for (const HatchSource& s : sources)
        if (!s.broken) frontier.push_back(slot_of(s.source));
    for (int hops = 0; hops < 4096 && !frontier.empty(); ++hops) {
        const EntityId at = frontier.back();
        frontier.pop_back();
        if (at == kNoEntity || std::ranges::find(seen, at) != seen.end()) continue;
        seen.push_back(at);
        const std::vector<HatchSource>* further = hatch_links_.get(at);
        if (further == nullptr) continue;
        for (const HatchSource& s : *further) {
            const EntityId next = slot_of(s.source);
            if (next == hatch)
                return err(ErrorCode::InvalidArgument,
                           "Tarama bağ döngüsü: " + std::to_string(raw(entities_.key[hatch])) +
                               " zaten dolaylı olarak " + std::to_string(raw(entities_.key[at])) +
                               " taramasının sınırını veriyor.");
            frontier.push_back(next);
        }
    }
    return restore_hatch_links(hatch, std::vector<HatchSource>(sources.begin(), sources.end()),
                               undo_out);
}

Status Document::restore_hatch_links(EntityId hatch, std::vector<HatchSource> sources, Op& undo_out)
{
    if (hatch >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(hatch));
    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetHatchLinks;
    undo_out.entity = hatch;
    if (const std::vector<HatchSource>* was = hatch_links_.get(hatch); was != nullptr) {
        undo_out.bytes_arg = encode_hatch_links(*was);
    } else if (sources.empty()) {
        undo_out = Op{}; // followed nothing, follows nothing: no change, no inverse
        return ok();
    }
    hatch_links_.set(hatch, std::move(sources));
    ++revision_;
    return ok();
}

Status Document::set_lineage(EntityId e, Lineage origin, Op& undo_out)
{
    if (e >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(e));
    if (origin.revisions.empty()) {
        origin.sources = lineage_sources(origin.sources);
    } else {
        // A RESULT'S EVIDENCE STAYS BESIDE ITS SOURCE: sorted as pairs, a key
        // given twice kept once with the revision it was first given.
        if (origin.revisions.size() != origin.sources.size())
            return err(ErrorCode::InvalidArgument,
                       "Bir sonucun her kaynağının bir sürümü olmalı: " + origin.operation);
        std::vector<std::pair<EntityKey, std::uint64_t>> pairs;
        pairs.reserve(origin.sources.size());
        for (std::size_t i = 0; i < origin.sources.size(); ++i)
            pairs.emplace_back(origin.sources[i], origin.revisions[i]);
        std::ranges::stable_sort(pairs, {}, &std::pair<EntityKey, std::uint64_t>::first);
        pairs.erase(
            std::ranges::unique(pairs, {}, &std::pair<EntityKey, std::uint64_t>::first).begin(),
            pairs.end());
        origin.sources.clear();
        origin.revisions.clear();
        for (const auto& [k, r] : pairs) {
            origin.sources.push_back(k);
            origin.revisions.push_back(r);
        }
    }
    if (!origin.operation.empty()) {
        if (origin.sources.empty())
            return err(ErrorCode::InvalidArgument,
                       "Bir kökenin en az bir kaynak nesnesi olmalı: " + origin.operation);
        const std::uint64_t issued = keys_.peek_entity(); // the next key to be handed out
        for (const EntityKey k : origin.sources)
            if (k == EntityKey::None || raw(k) >= issued)
                return err(ErrorCode::NotFound,
                           "Kökendeki kaynak bu çizimin verdiği bir kimlik değil: " +
                               std::to_string(raw(k)));
        if (std::ranges::binary_search(origin.sources, entities_.key[e]))
            return err(ErrorCode::InvalidArgument, "Bir nesne kendi kökeni olamaz.");
    }
    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetLineage;
    undo_out.entity    = e;
    const Lineage* was = lineage_.get(e);
    if (was == nullptr && origin.operation.empty()) {
        undo_out = Op{}; // had none, gets none: no change, no inverse
        return ok();
    }
    undo_out.bytes_arg = encode_lineage(was);
    lineage_.set(e, std::move(origin));
    ++revision_;
    return ok();
}

Status Document::restore_dimension_links(EntityId dim, std::vector<DimLink> links, Op& undo_out)
{
    if (dim >= entities_.size())
        return err(ErrorCode::NotFound, "Bilinmeyen nesne kimliği: " + std::to_string(dim));
    undo_out        = Op{};
    undo_out.kind   = Op::Kind::SetDimensionLinks;
    undo_out.entity = dim;
    if (const std::vector<DimLink>* was = dim_links_.get(dim); was != nullptr) {
        undo_out.bytes_arg = encode_dim_links(*was);
    } else if (links.empty()) {
        undo_out = Op{}; // linked to nothing, links to nothing: no change, no inverse
        return ok();
    }
    dim_links_.set(dim, std::move(links));
    ++revision_;
    return ok();
}

void Document::refresh_box(EntityId e)
{
    if (e >= entities_.size()) return;

    Box2 box = kind_bounds(geometry_, entities_.kind[e], entities_.slot[e]);

    if (std::array<Point2, 4> quad; text_quad(*this, e, quad))
        for (const Point2 corner : quad)
            box.extend(corner);

    entities_.min_x[e] = box.min_x;
    entities_.min_y[e] = box.min_y;
    entities_.max_x[e] = box.max_x;
    entities_.max_y[e] = box.max_y;

    // The tree filed this entity under the box it no longer has.
    index_stale_ = true;
}

void Document::mirror_layer_visibility(LayerId l, bool visible)
{
    // One pass per toggle instead of an indirect load per entity per frame (R7).
    for (EntityId e = 0; e < entities_.size(); ++e) {
        if (entities_.layer[e] != l) continue;
        if (visible)
            entities_.flags[e] &= static_cast<std::uint8_t>(~FlagLayerHidden);
        else
            entities_.flags[e] |= FlagLayerHidden;
    }
}

Status Document::set_layer_visible(LayerId l, bool visible, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was = layer->visible;
    layer->visible = visible;
    mirror_layer_visibility(l, visible);
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerVisible;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_locked(LayerId l, bool locked, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const bool was = layer->locked;
    layer->locked  = locked;
    ++revision_;

    undo_out          = Op{};
    undo_out.kind     = Op::Kind::SetLayerLocked;
    undo_out.layer    = l;
    undo_out.bool_arg = was;
    return ok();
}

Status Document::set_layer_appearance(LayerId l, const Appearance& a, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));

    const Appearance was = layer->appearance;
    layer->appearance    = a;
    ++revision_;

    undo_out                = Op{};
    undo_out.kind           = Op::Kind::SetLayerAppearance;
    undo_out.layer          = l;
    undo_out.appearance_arg = was;
    return ok();
}

Status Document::set_layer_style(LayerId l, StyleId style, Op& undo_out)
{
    Layer* layer = layers_.at(l);
    if (!layer) return err(ErrorCode::NotFound, "Bilinmeyen katman kimliği: " + std::to_string(l));
    if (style != kByLayerStyle && !styles_.contains(style))
        return err(ErrorCode::NotFound, "Bilinmeyen stil kimliği: " + std::to_string(style));

    const StyleId was = layer->style;
    layer->style      = style;
    ++revision_;

    undo_out           = Op{};
    undo_out.kind      = Op::Kind::SetLayerStyle;
    undo_out.layer     = l;
    undo_out.style_arg = was;
    return ok();
}

Status Document::set_layer_group(LayerId l, std::string group, Op& undo_out)
{
    Layer* record = layers_.at(l);
    if (record == nullptr)
        return err(ErrorCode::NotFound, "Katman bulunamadı: " + std::to_string(l) + ".");

    std::string was = record->group;
    record->group   = std::move(group);
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::SetLayerGroup;
    undo_out.layer   = l;
    undo_out.str_arg = std::move(was);
    return ok();
}

Status Document::set_crs(Crs crs, Op& undo_out)
{
    if (crs.id().empty())
        return err(ErrorCode::InvalidArgument, "Koordinat sistemi kimliği boş olamaz.");

    Crs was = crs_;
    crs_    = std::move(crs);
    ++revision_;

    undo_out         = Op{};
    undo_out.kind    = Op::Kind::SetCrs;
    undo_out.crs_arg = std::move(was);
    return ok();
}

StyleId Document::intern_style(const Appearance& a)
{
    return styles_.intern(a);
}

Result<ImageId> Document::intern_image(std::span<const std::byte> bytes, std::string_view origin)
{
    return images_.intern(bytes, origin);
}

Result<DashId> Document::intern_dash(const DashPattern& pattern, std::string_view origin)
{
    return dashes_.intern(pattern, origin);
}

StyleId Document::intern_symbol(const Symbol& sym)
{
    return styles_.intern(sym);
}

Status Document::apply(const Op& op, Op* undo_out)
{
    Op scratch;
    Op& inverse = undo_out ? *undo_out : scratch;

    switch (op.kind) {
    case Op::Kind::None: return ok();
    case Op::Kind::SetEntityAlive: return set_entity_alive(op.entity, op.bool_arg, inverse);
    case Op::Kind::SetEntityHidden: return set_entity_hidden(op.entity, op.bool_arg, inverse);
    case Op::Kind::SetEntityStyle: return set_entity_style(op.entity, op.style_arg, inverse);
    case Op::Kind::SetAttribute: {
        // A COLUMN IS FOUND BY ITS NAME, not its number (TODOS F-02). Dropping a
        // column renumbers every one after it, and a record that addressed
        // its column by number wrote a parcel's old value into the NEIGHBOURING
        // column on undo. A column that is gone — dropped, or declared again
        // as another type — has no cell to put the value back into: nothing
        // is written, and nothing is there to redo.
        AttrId col = op.attr_col;
        if (!op.str_arg.empty()) {
            col                       = attributes_.find(op.str_arg);
            const AttrColumn* current = attributes_.column(col);
            if (current == nullptr || current->type() != op.attr_arg.type) {
                inverse = Op{};
                return ok();
            }
        }
        return set_attribute(col, op.entity, op.attr_arg, inverse);
    }
    case Op::Kind::SetText:
        return set_text(op.entity, op.str_arg, op.text_height, op.text_anchor, op.text_lines,
                        inverse);
    case Op::Kind::SetLayerVisible: return set_layer_visible(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerLocked: return set_layer_locked(op.layer, op.bool_arg, inverse);
    case Op::Kind::SetLayerAppearance:
        return set_layer_appearance(op.layer, op.appearance_arg, inverse);
    case Op::Kind::SetLayerStyle: return set_layer_style(op.layer, op.style_arg, inverse);
    case Op::Kind::SetLayerGroup: return set_layer_group(op.layer, op.str_arg, inverse);
    case Op::Kind::SetCrs: return set_crs(op.crs_arg, inverse);
    case Op::Kind::SetGuides: {
        // The inverse of "restore this list" is "restore the list that is here
        // now", which is what makes a guide change redoable as well as undoable.
        inverse        = Op{};
        inverse.kind   = Op::Kind::SetGuides;
        inverse.guides = guides_.rows();
        set_guides(op.guides);
        return ok();
    }
    case Op::Kind::SetLayouts: {
        // The inverse of "restore this list" is "restore the one that is here
        // now", which is what makes a layout edit redoable as well as undoable.
        inverse             = Op{};
        inverse.kind        = Op::Kind::SetLayouts;
        inverse.layouts_arg = layouts_.all();
        load_layouts(op.layouts_arg);
        return ok();
    }
    case Op::Kind::SetGeometry: return restore_geometry(op.entity, op.geometry_slot, inverse);
    case Op::Kind::SetKindGeometry:
        return restore_kind_geometry(op.entity, op.geometry_slot, op.kind_arg, inverse);
    case Op::Kind::SetEntityLayer: return set_entity_layer(op.entity, op.layer, inverse);
    case Op::Kind::AttachForeign:
        return attach_foreign(op.entity, op.str_arg, op.bytes_arg, inverse);
    case Op::Kind::DetachForeign: return detach_foreign(op.entity, op.str_arg, inverse);
    case Op::Kind::SetAttachment:
        return restore_attachment(op.entity, op.has_attach ? &op.attach_arg : nullptr, inverse);
    case Op::Kind::SetDimensionLinks: {
        auto links = decode_dim_links(op.bytes_arg);
        if (!links) return links.error();
        return restore_dimension_links(op.entity, std::move(links.value()), inverse);
    }
    case Op::Kind::SetLineage: {
        auto origin = decode_lineage(op.bytes_arg);
        if (!origin) return origin.error();
        return set_lineage(op.entity, std::move(origin.value()), inverse);
    }
    case Op::Kind::SetHatchLinks: {
        auto sources = decode_hatch_links(op.bytes_arg);
        if (!sources) return sources.error();
        return restore_hatch_links(op.entity, std::move(sources.value()), inverse);
    }
    case Op::Kind::SetBlockBase: return set_block_base(op.block_arg, op.point_arg, inverse);
    case Op::Kind::SetBlockExternal:
        return set_block_external(op.block_arg, op.str_arg, op.byte_arg, inverse);
    }
    return err(ErrorCode::Internal, "İşlenmemiş Op::Kind");
}

Symbol layer_symbol(const Document& doc, LayerId layer)
{
    const Layer* l = doc.layer_table().at(layer);
    if (l == nullptr) return Symbol::of(Appearance{});
    if (l->style != kByLayerStyle && doc.styles().contains(l->style)) {
        Symbol stack = doc.styles().symbol_at(l->style);
        if (!stack.layers.empty()) return stack;
    }
    return Symbol::of(l->appearance);
}

Symbol drawn_symbol(const Document& doc, EntityId e)
{
    const StyleId own = doc.entities().style[e];
    if (own == kByLayerStyle || !doc.styles().contains(own))
        return layer_symbol(doc, doc.entities().layer[e]);

    // An id interned as a bare appearance reports a one-layer stack; one that
    // somehow reports none is drawn from its appearance, as the scene does.
    Symbol stack = doc.styles().symbol_at(own);
    if (stack.layers.empty()) return Symbol::of(doc.styles().at(own));
    return stack;
}

} // namespace kentos::core
