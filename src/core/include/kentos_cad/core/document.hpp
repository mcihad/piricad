// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: the document model.
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

#include "kentos_cad/core/attach.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/block.hpp"
#include "kentos_cad/core/crs.hpp"
#include "kentos_cad/core/dash_store.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/foreign_table.hpp"
#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/guide.hpp"
#include "kentos_cad/core/hatch_link.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/image_store.hpp"
#include "kentos_cad/core/layer.hpp"
#include "kentos_cad/core/layout.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/text_store.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// The STR-packed R-tree the document rebuilds lazily; see spatial_index.hpp.
class SpatialIndex;

/// Per-entity flag bits. The cull test reads this byte and the four bbox arrays,
/// and nothing else (model.md R6, R7). The enumerators are `unsigned` so that a
/// mask folded out of them stays unsigned — the BYTE is `EntityTable::flags`,
/// which the file format writes as `u8[]`, and that is unchanged.
enum EntityFlag : unsigned {
    FlagAlive       = 1u << 0,
    FlagHidden      = 1u << 1, ///< hidden on its own
    FlagLayerHidden = 1u << 2, ///< mirrored from the layer, refreshed on toggle
    /// Part of a block DEFINITION (model.md R45): drawn only through a reference,
    /// never on its own, so the cull test, the index and the pick skip it — one
    /// more bit in the byte R6 already reads, and no new column.
    FlagInBlock = 1u << 3,
};

/// `FlagAlive` and nothing else: what `visible()` requires the byte to equal.
inline constexpr std::uint8_t kFlagsDrawn = FlagAlive | FlagHidden | FlagLayerHidden | FlagInBlock;

/// The two bits `standalone()` reads: alive, and not inside a block definition.
inline constexpr std::uint8_t kFlagsStandalone = FlagAlive | FlagInBlock;

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
    /// Which KIND of thing this entity is: `KindSpec::id`, and the reason two
    /// entities holding the same two vertices can be a line and a circle.
    ///
    /// The id is DECLARED by each kind rather than handed out in registration
    /// order (`core.polyline` is 1, `core.circle` is 2), which is what lets the
    /// project writer store the number itself and still keep R26's promise that a
    /// file round-trips byte-identically.
    std::vector<KindId> kind;
    std::vector<std::uint32_t> slot; ///< row in the per-kind store

    // ---- identity: cold. Culling never reads it, and it is 8 bytes (R1) ----
    std::vector<EntityKey> key;

    std::size_t size() const noexcept { return flags.size(); }

    bool alive(EntityId e) const noexcept { return (flags[e] & FlagAlive) != 0; }

    /// Drawn only when alive and hidden by neither itself nor its layer. One byte,
    /// one test — that is the whole point of mirroring the layer bit.
    bool visible(EntityId e) const noexcept { return (flags[e] & kFlagsDrawn) == FlagAlive; }

    /// Alive and not inside a block definition: what the spatial index packs and
    /// what a pick or a snap may reach directly.
    bool standalone(EntityId e) const noexcept
    {
        return (flags[e] & kFlagsStandalone) == FlagAlive;
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
        SetLayerStyle,      ///< layer,  style_arg
        SetLayerGroup,      ///< layer,  str_arg
        SetCrs,             ///< crs_arg
        SetAttribute,       ///< attr_col, entity (as the row), attr_arg
        SetText,            ///< entity, str_arg, text_height, text_anchor
        SetGeometry,        ///< entity, geometry_slot
        SetKindGeometry,    ///< entity, geometry_slot, kind_arg — the kind it had before
        SetEntityLayer,     ///< entity, layer
        AttachForeign,      ///< entity, str_arg (the tag), bytes_arg
        DetachForeign,      ///< entity, str_arg (the tag)
        SetAttachment,      ///< entity, has_attach, attach_arg — what it followed before
        SetDimensionLinks,  ///< entity, bytes_arg — the links it had before (encode_dim_links)
        SetHatchLinks, ///< entity, bytes_arg — the sources it had before (encode_hatch_links)

        /// The WHOLE guide list, restored as it was.
        ///
        /// Not "add this one" / "remove that one": a guide has no key, so an
        /// inverse that named an index would be wrong the moment an earlier
        /// guide was removed. The list is a handful of numbers — a drawing has
        /// a dozen guides, not a million — so replacing it whole is both correct
        /// and cheaper than the bookkeeping any finer record would need.
        SetGuides, ///< guides

        /// The WHOLE layout list, restored as it was — the same bargain
        /// `SetGuides` makes, for the same reason. An item is named by an id
        /// inside a layout and a layout by a name, so a finer record would have
        /// to describe a rename, a reorder and a delete separately; a drawing
        /// carries a handful of sheets, so putting the previous list back is
        /// both correct and smaller than that bookkeeping would be.
        SetLayouts, ///< layouts_arg
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

    /// The geometry slot to put back. A slot number and not a vertex list, because
    /// the arena never drops one: the rings this names are still exactly where the
    /// entity left them (see `Document::set_geometry`).
    std::uint32_t geometry_slot{0};

    /// The kind to put back with `geometry_slot`; see `Kind::SetKindGeometry`.
    KindId kind_arg{0};

    /// The guide list as it was before the change; see `Kind::SetGuides`.
    ///
    /// Rows rather than parallel columns: the store is SoA because that is how it
    /// is stored, and an undo record is a value because that is what it is.
    std::vector<GuideRow> guides;

    /// The layout list as it was before the change; see `Kind::SetLayouts`.
    std::vector<Layout> layouts_arg;

    /// The foreign bytes to put back; see `Kind::AttachForeign`.
    std::vector<std::uint8_t> bytes_arg;

    /// The attachment to put back, when `has_attach`; a false `has_attach` puts
    /// back "attached to nothing" (see `Document::set_attachment`).
    bool has_attach{false};
    Attachment attach_arg{};
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

    /// Bytes another program attached to entities (model.md R26a). Opaque: no
    /// command reads them, no panel shows more than their count.
    const ForeignTable& foreign() const noexcept { return foreign_; }

    /// The block definitions this document holds (model.md R45).
    const BlockTable& blocks() const noexcept { return blocks_; }

    /// Which entities FOLLOW which (core/attach.hpp), keyed by the dependent's
    /// row. Never read by the frame path: a dependent is re-placed by the
    /// command that moved its source, at that command's commit.
    const AttachTable& attachments() const noexcept { return attachments_; }

    /// Which geometry each linked dimension measures (core/dimension_link.hpp),
    /// keyed by the dimension's row. Read at commit, never by the frame path.
    const DimLinkTable& dimension_links() const noexcept { return dim_links_; }

    /// Which objects each linked hatch's boundary comes from
    /// (core/hatch_link.hpp), keyed by the hatch's row. Read at commit, never by
    /// the frame path.
    const HatchLinkTable& hatch_links() const noexcept { return hatch_links_; }

    /// The drafting guides this document carries. Furniture, not geometry: saved
    /// with the file and invisible to selection, culling, export and area sums
    /// (see `core/guide.hpp`).
    const GuideStore& guides() const noexcept { return guides_; }

    /// The sheet layouts this drawing carries.
    ///
    /// CONTENT, NOT FURNITURE, unlike guides: a pafta is part of the submitted
    /// work, so it is in `content_hash`, it travels in the file, and every edit
    /// to it is a transaction like any other (see `core/layout.hpp`).
    const LayoutStore& layouts() const noexcept { return layouts_; }

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

    /// A circle, from its centre and radius. Stored as its DEFINING numbers and
    /// drawn by the kind (see `core.circle`), so its area is pi*r^2 and not the
    /// area of whatever polygon happened to be drawn for it.
    Result<EntityId> add_circle(LayerId lyr, Point2 centre, Mm radius, Op& undo_out);

    /// Adds an ellipse from its centre and its two axis ENDPOINTS.
    ///
    /// Endpoints rather than lengths and an angle: the two vectors carry the
    /// rotation, so no trigonometry is stored and none is needed to read it back.
    Result<EntityId> add_ellipse(LayerId lyr, Point2 centre, Point2 major, Point2 minor,
                                 Op& undo_out);
    /// An arc, from its centre, radius and the two measured ends. The sweep runs
    /// counter-clockwise from `start` to `end` (see `core.arc`).
    Result<EntityId> add_arc(LayerId lyr, Point2 centre, Mm radius, Point2 start, Point2 end,
                             Op& undo_out);

    /// A surveyed point: a control point, a traverse station, a benchmark
    /// (see `core.point`).
    Result<EntityId> add_point(LayerId lyr, Point2 at, Op& undo_out);

    /// THE GENERAL MUTATOR every `add_*` above is a spelling of: an entity of
    /// `kind` from its stored rings and its kind payload (model.md R9a).
    ///
    /// The kind's own `validate` runs first, so a circle handed three vertices or
    /// an arc-polyline handed too few bulges is refused before a byte is
    /// appended; the bounding box is the KIND's (`bbox`), never the arena's box
    /// of the definition vertices. A kind this build does not know is accepted
    /// exactly as given — visible through its rings, preserved byte for byte,
    /// and refused by every edit (R26) — which is what lets a file reader and an
    /// import hand over whatever the source held without a switch per kind.
    ///
    /// `in_block` names the block definition the entity belongs to, or kNoBlock
    /// for an ordinary entity. A member is flagged `FlagInBlock` and recorded in
    /// the block's member list; it is drawn only through a reference.
    Result<EntityId> add_kind(LayerId lyr, KindId kind,
                              std::span<const RingGeometry::RingInput> rings,
                              std::span<const std::uint8_t> payload, Op& undo_out,
                              BlockId in_block = kNoBlock);

    /// Attaches bytes another program owns to `e` under `tag` (model.md R26a).
    /// The inverse detaches them. Refused for a (entity, tag) already attached.
    Status attach_foreign(EntityId e, std::string_view tag, std::span<const std::uint8_t> bytes,
                          Op& undo_out);

    /// Removes the bytes under (e, tag); the inverse re-attaches them.
    Status detach_foreign(EntityId e, std::string_view tag, Op& undo_out);

    /// Adds a block definition (model.md R45). Append-only and not undoable, for
    /// the reason `ensure_layer` is not: an empty definition is inert, and an id
    /// once handed out reaches the file and every reference's payload.
    Result<BlockId> add_block(std::string_view name, std::string_view description, Point2 base);

    /// Records that `block`'s members reference `uses`, refusing a cycle. What a
    /// block reference created inside a definition calls, and what the file
    /// reader restores.
    Status add_block_use(BlockId block, BlockId uses);

    /// Whether `e` may be edited in place: alive, of a kind this build knows
    /// (model.md R26), not inside a block definition (R45), and NOT ON A LOCKED
    /// LAYER. The message is what the refusing command says.
    ///
    /// The lock belongs here and was missing: it was checked on every `add_*` and
    /// nowhere else, so a locked layer stopped a new parcel being drawn on it and
    /// let every edit verb reshape the parcels already on it. Undo and redo do not
    /// come through this, so locking a layer never traps an earlier edit.
    Status editable(EntityId e) const;

    /// Replaces an entity's kind payload, keeping its rings and its identity.
    ///
    /// A new slot is appended with the same rings and the new bytes, and the
    /// entity is repointed at it, exactly as `set_geometry` does for the rings —
    /// so the inverse is the old slot number (`Op::Kind::SetGeometry`) and no new
    /// Op variant exists. The kind validates the pair before anything is written.
    Status set_kind_payload(EntityId e, std::span<const std::uint8_t> payload, Op& undo_out);

    /// Replaces an entity's rings AND its kind payload together, keeping its
    /// identity — what a transform of a kind whose payload holds coordinates
    /// needs (an arc polyline's centres, a block reference's turn), since the
    /// rings alone or the payload alone would fail the kind's validate against
    /// the half not yet changed. One new slot; the inverse is `Op::SetGeometry`.
    Status set_kind_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                             std::span<const std::uint8_t> payload, Op& undo_out);

    /// The same, when the edit needs ANOTHER KIND to hold it: a polyline whose
    /// straight edge became an arc is an arc polyline now, and back (model.md
    /// R9b). The entity keeps its key, layer, style, attributes, caption and
    /// the objects that follow it — the new kind validates the pair first, and
    /// the inverse is `Op::SetKindGeometry`, which puts the old kind back with
    /// the old slot. Refused into or out of a kind this build does not know.
    Status set_kind_geometry(EntityId e, KindId kind,
                             std::span<const RingGeometry::RingInput> rings,
                             std::span<const std::uint8_t> payload, Op& undo_out);

    /// Whether `e` is of a kind this build understands. An entity of an unknown
    /// kind is preserved and drawn, and refused by every edit (model.md R26).
    bool kind_known(EntityId e) const noexcept;

    /// Moves an entity to another layer, keeping its identity.
    ///
    /// The live counters and the mirrored `FlagLayerHidden` bit follow it, because
    /// the cull test reads that bit and nothing else (R6, R7): an object moved
    /// onto a hidden layer has to disappear without the frame path asking a
    /// question.
    Status set_entity_layer(EntityId e, LayerId layer, Op& undo_out);

    Status set_entity_alive(EntityId e, bool alive, Op& undo_out);
    Status set_entity_hidden(EntityId e, bool hidden, Op& undo_out);
    Status set_entity_style(EntityId e, StyleId style, Op& undo_out);

    /// Replaces an entity's geometry, keeping its identity.
    ///
    /// The rings are APPENDED to the arena and the entity is repointed at them;
    /// the slot it used to hold is left where it is. That is what makes this
    /// undoable in O(1) — the inverse is the old slot number, not a copy of the
    /// vertices — and it is why the arena is append-only to begin with. The cost
    /// is a dead slot per edit, which the file writer drops on save.
    ///
    /// IDENTITY SURVIVES, and that is the whole reason this exists rather than
    /// erase-then-add: the key, the layer, the style, the attributes and the text
    /// all hang off `EntityId`. A parsel whose corner was dragged is the same
    /// parsel, with the same ada/parsel numbers, and re-adding it would silently
    /// mint a new key and drop every attribute the surveyor had entered.
    Status set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings, Op& undo_out);

    Status set_layer_visible(LayerId l, bool visible, Op& undo_out);
    Status set_layer_locked(LayerId l, bool locked, Op& undo_out);
    Status set_layer_appearance(LayerId l, const Appearance& a, Op& undo_out);
    Status set_layer_style(LayerId l, StyleId style, Op& undo_out);

    /// Moves a layer in the layer tree. An empty path puts it at the root.
    Status set_layer_group(LayerId l, std::string group, Op& undo_out);
    /// Sets the document's CRS. Takes a whole `Crs` rather than an id, because a
    /// resolved CRS carries the epoch and the zone meridian and dropping them here
    /// would leave the document naming a system it cannot describe (R36).
    Status set_crs(Crs crs, Op& undo_out);

    /// Replaces the whole layout list, recording the previous one for undo.
    ///
    /// WHOLE-LIST, and every layout command goes through it: add, remove,
    /// rename, move an item, retype a title. The caller reads `layouts().all()`,
    /// changes its copy and hands it back, so one mutator and one Op kind cover
    /// a subsystem that would otherwise need a dozen of each.
    Status set_layouts(std::vector<Layout> layouts, Op& undo_out);

    /// Puts a layout list back without recording anything — what undo calls.
    void load_layouts(std::vector<Layout> layouts);

    /// Declares a column. NOT undoable and deliberately so, for the same reason
    /// a layer is not: the schema is what rows are addressed against, and undoing
    /// a declaration would invalidate every row index the journal already holds.
    /// A schema comes from /data, and reloading a package is its own command.
    Result<AttrId> declare_attribute(AttrSpec spec);

    /// Drops a column and every cell in it. NOT undoable, for the same reason.
    ///
    /// The caller above this is expected to have asked the user first: this is
    /// the one edit in the program that destroys entered data with no record of
    /// it anywhere.
    Status drop_attribute(std::string_view id);

    /// Changes what a column says about itself; see `AttrColumn::amend`. NOT
    /// undoable, and it cannot change the id or the type.
    Status amend_attribute(std::string_view id, const AttrSpec& next);

    /// R28's one generic write: column, row, value in; the previous value out,
    /// which is exactly what undo needs and all it needs.
    Status set_attribute(AttrId col, EntityId e, const AttrValue& v, Op& undo_out);

    /// Adds a drafting guide. `undo_out` carries the whole previous list.
    Status add_guide(GuideAxis axis, Mm coordinate, Op& undo_out);

    /// Adds a guide through `through` running at `angle` micro-degrees. `ray`
    /// makes it one-sided: forward from the point only (`KILAVUZ tur=isin`).
    Status add_angled_guide(Point2 through, std::int64_t angle, bool ray, Op& undo_out);

    /// Removes the guide at `index`. Refuses an index past the end by name.
    Status remove_guide(std::size_t index, Op& undo_out);

    /// Replaces the whole guide list — how `SetGuides` is undone, and how a file
    /// reader installs what it read.
    void set_guides(std::vector<GuideRow> rows);

    Result<AttrValue> attribute(AttrId col, EntityId e) const;

    /// Attaches or replaces the text on an entity. Height is ground millimetres.
    /// An empty `content` detaches it.
    Status set_text(EntityId e, std::string content, Mm height, TextAnchor anchor, Op& undo_out);

    /// Makes `e` follow `a->source` as `a` says, or follow nothing when `a` is
    /// null. Refused for a dead or non-editable dependent, a source that does not
    /// exist or is dead, a self-reference, and a chain that would come back to
    /// `e` — a dependent may itself be followed, a cycle may not. The inverse
    /// restores what `e` followed before.
    Status set_attachment(EntityId e, const Attachment* a, Op& undo_out);

    /// Replaces the links of dimension `dim` (an empty list unlinks it).
    /// Refused for anything but a live, editable dimension, a definition point
    /// the dimension does not have, and a live link to a source that does not
    /// exist — a BROKEN link keeps the key of the object that is gone, which is
    /// what it is for. The inverse restores the links it had before.
    Status set_dimension_links(EntityId dim, std::span<const DimLink> links, Op& undo_out);

    /// Replaces the boundary sources of hatch `hatch` (an empty list unlinks
    /// it). Refused for anything but a live, editable hatch, and a live source
    /// that does not exist or is the hatch itself — a BROKEN source keeps the
    /// key of the object that is gone. The inverse restores what it had before.
    Status set_hatch_links(EntityId hatch, std::span<const HatchSource> sources, Op& undo_out);

    /// Interns an appearance and returns its id, for a command building a style.
    StyleId intern_style(const Appearance& a);

    StyleId intern_symbol(const Symbol& sym);

    /// Adds a picture to the drawing and returns its id.
    ///
    /// Like style interning and unlike everything else on this class, this is
    /// ADDITIVE ONLY: an image is never removed, because an id handed out stays
    /// valid for the document's lifetime and the journal already holds it. A
    /// drawing that stops using a hatch keeps the bytes, which is a few kilobytes
    /// and the price of an id that never lies.
    Result<ImageId> intern_image(std::span<const std::byte> bytes, std::string_view origin);

    /// Adds a line type to the drawing, or returns the id an identical one has.
    /// Additive only, exactly like image and style interning.
    Result<DashId> intern_dash(const DashPattern& pattern, std::string_view origin);

    /// The pictures this drawing carries. Read by the renderer, which needs the
    /// bytes, and by a size report.
    const ImageStore& images() const noexcept { return images_; }

    /// The line types this drawing carries; `Appearance.dash` indexes it.
    const DashStore& dashes() const noexcept { return dashes_; }

    DashStore& dashes() noexcept { return dashes_; }

    /// Applies a previously produced Op. Used only by Transaction rollback and by
    /// the undo stack; `undo_out` receives the Op that reverses this one.
    Status apply(const Op& op, Op* undo_out = nullptr);

private:
    Result<EntityId> push_entity(LayerId lyr, std::uint32_t geometry_slot, KindId kind);
    /// Recomputes `e`'s bounding box from its geometry, and then grows it over
    /// the letters when it carries text.
    ///
    /// THE BOX IS WHAT THE CULL AND THE PICK PREFILTER READ (R6), and a caption's
    /// geometry is the hairline under its letters — so without this an imported
    /// ada number reported a bounding box zero millimetres tall, fell out of
    /// every query whose box did not cross that one line, and could not be
    /// clicked on. `pick.hpp::text_quad` owns the shape; this stores its extent.
    void refresh_box(EntityId e);

    /// Copies the caption of geometry slot `from` onto slot `to`, when there is
    /// one. A caption lives on the SLOT (`TextTable`), and every edit that
    /// replaces an entity's geometry appends a new slot — so without this a
    /// corner moved on a labelled parsel, or a definition point moved on a
    /// dimension, silently dropped the label. Undo repoints to the old slot,
    /// which keeps its own caption, so nothing is written back on the way out.
    void carry_text(std::uint32_t from, std::uint32_t to);

    /// The attribute row of slot `from` copied to slot `to`, for the same
    /// reason as `carry_text`: cells are slot-indexed, and without the copy
    /// every geometry edit silently dropped the entity's attributes (P11).
    void carry_attributes(std::uint32_t from, std::uint32_t to);

    void mirror_layer_visibility(LayerId l, bool visible);

    /// Points an entity back at a slot the arena already holds. The undo half of
    /// `set_geometry`: it appends nothing, because the rings being restored were
    /// never thrown away.
    Status restore_geometry(EntityId e, std::uint32_t slot, Op& undo_out);

    /// The undo half of the kind-changing `set_kind_geometry`: the slot and the
    /// kind it had, both put back.
    Status restore_kind_geometry(EntityId e, std::uint32_t slot, KindId kind, Op& undo_out);

    /// The undo half of `set_dimension_links`: the links a dimension had, put
    /// back as they were. Nothing about the sources is checked, because undo
    /// runs backwards — the links of a dimension whose source a command erased
    /// come back one op BEFORE the source does.
    Status restore_dimension_links(EntityId dim, std::vector<DimLink> links, Op& undo_out);

    /// The undo half of `set_hatch_links`, for the same reason: nothing about
    /// the sources is checked on the way back.
    Status restore_hatch_links(EntityId hatch, std::vector<HatchSource> sources, Op& undo_out);

    Crs crs_{};
    EntityTable entities_{};
    RingGeometry geometry_{};
    LayerTable layers_{};
    StyleTable styles_{};
    AttrTable attributes_{};
    CatalogueSet catalogues_{};
    TextTable texts_{};
    ForeignTable foreign_{};
    BlockTable blocks_{};
    AttachTable attachments_{};
    DimLinkTable dim_links_{};
    HatchLinkTable hatch_links_{};
    GuideStore guides_{};
    LayoutStore layouts_{};
    ImageStore images_{};
    DashStore dashes_{};
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

    /// An indexed entity's box has MOVED, so the tree no longer bounds it.
    ///
    /// Erasing an entity leaves the tree over-inclusive, which costs a wasted
    /// candidate and nothing else. Editing geometry is the one mutation that can
    /// make it UNDER-inclusive: the entity moves out of the box the tree filed it
    /// under, and a query at its new position stops finding it — which shows up as
    /// a corner that has visibly moved but will not snap. Nothing else in this
    /// class needs this, so it is set in exactly one place.
    mutable bool index_stale_{false};
};

/// The symbol an object on `layer` that inherits everything is drawn with: the
/// layer's symbol stack when it has one, its plain appearance otherwise — the
/// two branches the scene builder takes for the ByLayer sentinel.
///
/// THE VALUES ARE WHAT IS DRAWN. The renderer reads a style's colours and
/// widths as they stand and evaluates no cascade (model.md R14), so a command
/// that gives one object its own style starts from THIS and changes what it was
/// asked to change; starting from the bare sentinel would draw the object black
/// and hairline the moment it stopped inheriting.
Symbol layer_symbol(const Document& doc, LayerId layer);

/// The symbol `e` is drawn with: its own style, or its layer's (above) when it
/// carries the ByLayer sentinel.
Symbol drawn_symbol(const Document& doc, EntityId e);

} // namespace kentos::core
