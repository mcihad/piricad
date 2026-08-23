// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: the document model.
//
// Built on .claude/model.md. The three tiers, and the frame path only ever
// touches tier 1:
//
//   Tier 1  EntityTable    one flat SoA row per entity. No kind-specific data,
//                          no pointers. Read by cull, index build, hit-test
//                          prefilter and the layer panel.
//   Tier 2  RingGeometry   the per-kind store. Referenced from tier 1 by `slot`.
//                          Read by vertex emit, geometry operations and I/O.
//   Tier 3  attributes     named typed columns declared from /data. Read by
//                          domain rules, reports and export. NEVER by the frame.
//
// The Document exposes only primitive, inverse-producing mutators. Higher layers
// reach them through a Transaction, which the command bus owns (Article 1).
#pragma once

#include "piricad/core/attribute.hpp"
#include "piricad/core/crs.hpp"
#include "piricad/core/geometry.hpp"
#include "piricad/core/identity.hpp"
#include "piricad/core/layer.hpp"
#include "piricad/core/result.hpp"
#include "piricad/core/style.hpp"
#include "piricad/core/text_store.hpp"
#include "piricad/core/units.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

/// The STR-packed R-tree the document rebuilds lazily; see spatial_index.hpp.
class SpatialIndex;

/// Per-entity flag bits. The cull test reads this byte and the four bbox arrays,
/// and nothing else (model.md R6, R7).
enum EntityFlag : std::uint8_t {
    FlagAlive       = 1u << 0,
    FlagHidden      = 1u << 1, ///< hidden on its own
    FlagLayerHidden = 1u << 2, ///< mirrored from the layer, refreshed on toggle
};

/// Tier 1. One row per entity: POD, mmap-able, no pointers, no kind-specific data.
class EntityTable
{
public:
    // ---- cull block: the ONLY arrays the frame path reads (R6) ----
    std::vector<Mm> min_x;
    std::vector<Mm> min_y;
    std::vector<Mm> max_x;
    std::vector<Mm> max_y;
    std::vector<std::uint8_t> flags;

    // ---- resolution block: read only for entities the index returned ----
    std::vector<LayerId> layer;
    std::vector<StyleId> style; ///< interned; kByLayerStyle means inherit
    /// Entity kind index, dense and not persisted — `KindSpec::stable_id` is what
    /// reaches the file (R22-R26).
    std::vector<std::uint16_t> kind;
    std::vector<std::uint32_t> slot; ///< row in the per-kind store

    // ---- identity: cold. Culling never reads it, and it is 8 bytes (R1) ----
    std::vector<EntityKey> key;

    std::size_t size() const noexcept { return flags.size(); }

    bool alive(EntityId e) const noexcept { return (flags[e] & FlagAlive) != 0; }

    /// Drawn only when alive and hidden by neither itself nor its layer. One byte,
    /// one test — that is the whole point of mirroring the layer bit.
    bool visible(EntityId e) const noexcept
    {
        return (flags[e] & (FlagAlive | FlagHidden | FlagLayerHidden)) == FlagAlive;
    }

    /// The cached bounding box. Assembled from the four columns rather than
    /// stored as a struct, because the CULL TEST reads the columns and a
    /// struct-of-boxes layout would pull cache lines it does not need (R6, R7).
    Box2 box_of(EntityId e) const noexcept { return Box2{min_x[e], min_y[e], max_x[e], max_y[e]}; }
};

/// A reversible primitive edit. Produced by Document mutators, consumed by
/// Transaction. Trivially copyable except for the CRS string, which is the one
/// variable-length payload the document has today.
struct Op
{
    enum class Kind : std::uint8_t {
        None,
        SetEntityAlive,     ///< entity, bool_arg
        SetEntityHidden,    ///< entity, bool_arg
        SetEntityStyle,     ///< entity, style_arg
        SetLayerVisible,    ///< layer,  bool_arg
        SetLayerLocked,     ///< layer,  bool_arg
        SetLayerAppearance, ///< layer,  appearance_arg
        SetCrs,             ///< crs_arg
        SetAttribute,       ///< attr_col, entity (as the row), attr_arg
        SetText,            ///< entity, str_arg, text_height, text_anchor
    };

    Kind kind{Kind::None};
    EntityId entity{kNoEntity};
    LayerId layer{kNoLayer};
    bool bool_arg{false};
    StyleId style_arg{kByLayerStyle};
    Appearance appearance_arg{};
    std::string str_arg;

    // R28 is why there is ONE attribute variant and not one per type: an
    // attribute write is a column, a row and a value. A new column adds no Op
    // kind, no journal case and no migration — which is the whole point of a
    // schema that lives in /data rather than in this enum.
    AttrId attr_col{kNoAttr};
    AttrValue attr_arg{};

    /// The whole previous CRS, not just its id: undoing a CRS change has to
    /// restore the metadata the geodesy module resolved, and re-resolving it here
    /// is impossible because core does not know what a zone is.
    Crs crs_arg{};

    Mm text_height{0}; ///< 0 = the slot carries no text
    TextAnchor text_anchor{TextAnchor::BaselineLeft};
};

class Document
{
public:
    /// An empty document with layer 0 already present, a CRS of `TUREF/TM30` and
    /// the key allocators at their start.
    Document();
    ~Document();

    /// Movable, so a file reader can build a document and hand it over.
    Document(Document&&) noexcept;
    Document& operator=(Document&&) noexcept;

    /// NOT copyable. A document is tens of columns and, at cadastral scale,
    /// hundreds of megabytes; an accidental copy would be a pause the user feels.
    /// A caller that genuinely wants a second one replays the journal.
    Document(const Document&)            = delete;
    Document& operator=(const Document&) = delete;

    // ---- read API: rich and direct (performance), never mutating ----
    const Crs& crs() const noexcept { return crs_; }

    const EntityTable& entities() const noexcept { return entities_; }

    const RingGeometry& geometry() const noexcept { return geometry_; }

    const LayerTable& layer_table() const noexcept { return layers_; }

    const StyleTable& styles() const noexcept { return styles_; }

    /// The attribute columns of this document, indexed by entity SLOT.
    ///
    /// R29/P29: **the frame path never reads these.** The renderer reads one
    /// StyleId per entity, resolved at commit time by the command that changed
    /// the data (R14). A draw call that needs a value from here is a missing
    /// style class, not a lookup — the hot/cold split is why five million
    /// parcels pan inside 16 ms.
    const AttrTable& attributes() const noexcept { return attributes_; }

    /// The catalogues a CodeRef column validates against (R34).
    const CatalogueSet& catalogues() const noexcept { return catalogues_; }

    /// The text carried by entity slots. Read by the frame path, unlike attribute
    /// columns — drawing a caption means reading its string every frame, so text
    /// lives here rather than in a column R29 forbids the renderer to touch.
    const TextTable& texts() const noexcept { return texts_; }

    CatalogueSet& catalogues() noexcept { return catalogues_; }

    std::uint64_t revision() const noexcept { return revision_; }

    const std::vector<Layer>& layers() const noexcept { return layers_.all(); }

    const Layer* layer(LayerId slot) const noexcept { return layers_.at(slot); }

    LayerId find_layer(std::string_view name) const;

    bool alive(EntityId e) const noexcept { return e < entities_.size() && entities_.alive(e); }

    /// Maintained incrementally: the layer panel asks on every document change and
    /// must not walk five million rows to answer.
    std::size_t live_entity_count() const noexcept { return live_count_; }

    std::size_t layer_entity_count(LayerId l) const noexcept;

    Box2 extent() const;
    Box2 entity_extent(EntityId e) const;

    /// Net area of one entity: exterior rings add, interior rings subtract.
    /// This is `alan hesabı` (model.md R12).
    Mm2 entity_area(EntityId e) const;
    Mm entity_perimeter(EntityId e) const;

    /// Order-independent, platform-independent content fingerprint. Covers CRS,
    /// layers, styles and geometry — not keys, so two documents built the same way
    /// from the same input agree (§7.3).
    std::uint64_t content_hash() const;

    // ---- identity: translation happens at the bus boundary only (R2) ----
    EntityKey key_of(EntityId e) const noexcept;
    EntityId slot_of(EntityKey k) const noexcept;
    LayerKey layer_key_of(LayerId l) const noexcept;
    LayerId layer_slot_of(LayerKey k) const noexcept;

    /// Highest key handed out so far, for the file writer.
    const KeyAllocator& keys() const noexcept { return keys_; }

    // ---- spatial index: a cache, rebuilt lazily (R6, §10.5) ----
    const SpatialIndex& spatial_index() const;

    EntityId indexed_upto() const noexcept { return indexed_upto_; }

    // ---- mutators: PRIMITIVE ONLY, each returns the Op that undoes it ----
    // Reaching these outside a Transaction violates Article 1 and is caught by
    // scripts/ci-gate-command-mutation.sh.

    /// Creates the layer if absent. Not undoable by design: an empty layer is
    /// inert, and removing it would invalidate stored slots.
    LayerId ensure_layer(std::string_view name);

    /// One open ring — a polyline. The common CAD case.
    Result<EntityId> add_polyline(LayerId lyr, std::span<const Point2> pts, Op& undo_out);

    /// A face: one exterior ring, optionally with interior rings, optionally
    /// multipart. This is what a parcel is, and what `(start, count)` could not
    /// express (R9).
    Result<EntityId> add_area(LayerId lyr, std::span<const RingGeometry::RingInput> rings,
                              Op& undo_out);

    Status set_entity_alive(EntityId e, bool alive, Op& undo_out);
    Status set_entity_hidden(EntityId e, bool hidden, Op& undo_out);
    Status set_entity_style(EntityId e, StyleId style, Op& undo_out);

    Status set_layer_visible(LayerId l, bool visible, Op& undo_out);
    Status set_layer_locked(LayerId l, bool locked, Op& undo_out);
    Status set_layer_appearance(LayerId l, const Appearance& a, Op& undo_out);
    /// Sets the document's CRS. Takes a whole `Crs` rather than an id, because a
    /// resolved CRS carries the epoch and the zone meridian and dropping them here
    /// would leave the document naming a system it cannot describe (R36).
    Status set_crs(Crs crs, Op& undo_out);

    /// Declares a column. NOT undoable and deliberately so, for the same reason
    /// a layer is not: the schema is what rows are addressed against, and undoing
    /// a declaration would invalidate every row index the journal already holds.
    /// A schema comes from /data, and reloading a package is its own command.
    Result<AttrId> declare_attribute(AttrSpec spec);

    /// R28's one generic write: column, row, value in; the previous value out,
    /// which is exactly what undo needs and all it needs.
    Status set_attribute(AttrId col, EntityId e, const AttrValue& v, Op& undo_out);

    Result<AttrValue> attribute(AttrId col, EntityId e) const;

    /// Attaches or replaces the text on an entity. Height is ground millimetres.
    /// An empty `content` detaches it.
    Status set_text(EntityId e, std::string content, Mm height, TextAnchor anchor, Op& undo_out);

    /// Interns an appearance and returns its id, for a command building a style.
    StyleId intern_style(const Appearance& a);

    StyleId intern_symbol(const Symbol& sym);

    /// Applies a previously produced Op. Used only by Transaction rollback and by
    /// the undo stack; `undo_out` receives the Op that reverses this one.
    Status apply(const Op& op, Op* undo_out = nullptr);

private:
    Result<EntityId> push_entity(LayerId lyr, std::uint32_t geometry_slot);
    void mirror_layer_visibility(LayerId l, bool visible);

    Crs crs_{};
    EntityTable entities_{};
    RingGeometry geometry_{};
    LayerTable layers_{};
    StyleTable styles_{};
    AttrTable attributes_{};
    CatalogueSet catalogues_{};
    TextTable texts_{};
    KeyAllocator keys_{};

    /// INVARIANT: `entities_.key` is strictly increasing in slot order, because
    /// keys are minted monotonically and rows are only ever appended. `slot_of`
    /// is therefore a binary search over a column that already exists.
    ///
    /// The obvious alternative, an unordered_map<u64, EntityId>, measured at
    /// ~240 MB on the five-million-parcel benchmark — a third of the document —
    /// to answer a question the key column already answers.
    ///
    /// A future file reader that inserts keys out of order must restore the
    /// invariant (sort on load) or replace this with an explicit index.
    bool keys_sorted_{true};

    std::vector<std::size_t> layer_live_{};

    std::uint64_t revision_{0};
    std::size_t live_count_{0};

    // A cache, not state: rebuilding it never changes what the document contains.
    mutable std::unique_ptr<SpatialIndex> index_{};
    mutable std::size_t indexed_live_{0};
    mutable EntityId indexed_upto_{0};
};

} // namespace piricad::core
