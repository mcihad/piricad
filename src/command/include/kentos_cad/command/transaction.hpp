// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — command: transaction and undo.
//
// kentoscad.md §2.5:
//   * one command  = one undo step (default)
//   * one script block or one AI suggestion = ONE merged undo step
//   * a validation failure inside a transaction = full rollback, no partial apply
//   * a half-applied edit on cadastral or zoning data is never acceptable
#pragma once

#include "kentos_cad/core/document.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::command {

using core::Appearance;
using core::Document;
using core::EntityId;
using core::LayerId;
using core::Op;
using core::Point2;
using core::Result;
using core::RingGeometry;
using core::Status;
using core::StyleId;

/// Collects the inverse of every primitive edit made through it.
/// The ONLY sanctioned route to Document mutation (Constitution Article 1).
class Transaction
{
public:
    /// Opens a transaction over a document. `label` is what the undo entry will
    /// be called — the user reads it in the Düzen menu, so it names the ACTION
    /// rather than the command id.
    Transaction(Document& doc, std::string label);

    /// Returns the slot of the layer with this name, creating it if absent.
    ///
    /// NOT undoable, and deliberately so — the same decision `Document` records:
    /// an empty layer is inert, and removing it on undo would invalidate every
    /// stored slot in `entity.layer`. It lives here anyway because a caller
    /// outside /src/command must have ONE sanctioned handle for document work
    /// (Article 5.9) rather than reaching past the transaction for this one call.
    LayerId ensure_layer(std::string_view name);

    /// Interns an appearance and returns its id, for a command — or a file reader
    /// — that resolves a style at commit time (model.md R14).
    ///
    /// Also not undoable, for two reasons that reinforce each other. The style
    /// table is a deduplicated pool: adding to it changes nothing that is drawn
    /// until an entity's style column points at the new entry, and that write IS
    /// undoable (`set_entity_style`). And the table only ever grows, so an id once
    /// handed out stays valid for the document's lifetime — rolling an intern back
    /// would renumber ids that other entities, and the journal's recorded previous
    /// values, already point at.
    StyleId intern_style(const Appearance& a);

    /// Interns a full symbol stack. A one-layer stroke stack with no scale window
    /// returns the same id `intern_style` would, so nothing changes for a drawing
    /// that uses neither.
    StyleId intern_symbol(const core::Symbol& sym);

    /// Adds a picture to the drawing. Additive only, like style interning: an id
    /// handed out stays valid for the document's lifetime, so there is nothing to
    /// undo and no inverse Op is recorded.
    core::Result<core::ImageId> intern_image(std::span<const std::byte> bytes,
                                             std::string_view origin);

    /// Adds a line type to the drawing. Additive for the same reason and with the
    /// same consequence: no inverse Op, and an id stays valid for the document.
    core::Result<core::DashId> intern_dash(const core::DashPattern& pattern,
                                           std::string_view origin);

    Result<EntityId> add_polyline(LayerId layer, std::span<const Point2> pts);

    /// A face: one exterior ring, optionally with holes, optionally multipart.
    /// This is what a parcel is (model.md R9).
    Result<EntityId> add_area(LayerId layer, std::span<const RingGeometry::RingInput> rings);
    /// Replaces an entity's geometry, keeping its key, layer, style, attributes
    /// and text. This is what a corner being dragged is: the same parsel with a
    /// different boundary, never a new one (see `Document::set_geometry`).
    Status set_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings);

    /// A circle, from its centre and radius (model.md R22-R26: `core.circle`).
    Result<EntityId> add_circle(LayerId layer, Point2 centre, core::Mm radius);

    /// Adds an ellipse from its centre and its two axis endpoints.
    Result<EntityId> add_ellipse(LayerId layer, Point2 centre, Point2 major, Point2 minor);
    /// An arc: centre, radius and the two ends, swept counter-clockwise.
    Result<EntityId> add_arc(LayerId layer, Point2 centre, core::Mm radius, Point2 start,
                             Point2 end);

    /// A surveyed point (model.md R22-R26: `core.point`).
    Result<EntityId> add_point(LayerId layer, Point2 at);

    /// An entity of any kind from its rings and kind payload — the general form
    /// every `add_*` above spells for one kind (`Document::add_kind`). This is
    /// how a file reader and an import hand over what the source held, unknown
    /// kinds included (model.md R26).
    Result<EntityId> add_kind(LayerId layer, core::KindId kind,
                              std::span<const RingGeometry::RingInput> rings,
                              std::span<const std::uint8_t> payload,
                              core::BlockId in_block = core::kNoBlock);

    /// Attaches bytes another program owns to `e` (`Document::attach_foreign`).
    Status attach_foreign(EntityId e, std::string_view tag, std::span<const std::uint8_t> bytes);

    /// Adds a block definition (`Document::add_block`). Append-only, no inverse.
    core::Result<core::BlockId> add_block(std::string_view name, std::string_view description,
                                          Point2 base);

    /// Records that `block` references `uses` (`Document::add_block_use`).
    Status add_block_use(core::BlockId block, core::BlockId uses);

    /// Replaces an entity's kind payload, keeping its rings and its identity
    /// (`Document::set_kind_payload`).
    Status set_kind_payload(EntityId e, std::span<const std::uint8_t> payload);

    /// Replaces rings and payload together (`Document::set_kind_geometry`).
    Status set_kind_geometry(EntityId e, std::span<const RingGeometry::RingInput> rings,
                             std::span<const std::uint8_t> payload);

    /// Replaces the kind, rings and payload together, keeping the entity's
    /// identity (`Document::set_kind_geometry` with a kind, model.md R9b).
    Status set_kind_geometry(EntityId e, core::KindId kind,
                             std::span<const RingGeometry::RingInput> rings,
                             std::span<const std::uint8_t> payload);

    /// What `adopt_from` brought over.
    struct AdoptSummary
    {
        std::uint64_t entities{0};      ///< live entities copied, members of definitions included
        std::uint64_t standalone{0};    ///< of those, the ones on the sheet rather than in a block
        std::uint64_t layers{0};        ///< layers this document did not have before
        std::uint64_t columns{0};       ///< attribute columns declared here for the first time
        std::vector<std::string> notes; ///< what could not be carried, in Turkish, at most a few
    };

    /// Copies EVERYTHING a scratch document holds into this transaction's
    /// document: layers (by name, with appearance, style, visibility, lock and
    /// group), the styles, dash patterns and pictures those need, block
    /// definitions, every live entity of every kind — known or not, with its
    /// payload — its hidden bit, own style, caption, attribute cells and foreign
    /// data. Layers are matched by name and columns by id, so a re-import lands
    /// on the same layers and columns rather than making new ones.
    ///
    /// GENERAL, NOT A SWITCH PER KIND: an entity goes through `add_kind` with its
    /// rings and payload as stored, so a kind added next month is adopted by the
    /// code written this month. This is the second half of a two-phase import —
    /// a reader fills a scratch document on a worker thread, this copies it into
    /// the real one on the bus thread, inside the command's one transaction
    /// (io.md P3, R17). Any failure is returned; the transaction rolls back whole.
    /// `only` names the entities to bring over, by persistent key; empty brings
    /// every live one, which is what an import wants. A block reference named
    /// in `only` brings the definition it draws, whole. A block whose name this
    /// document already has keeps this document's definition: the incoming
    /// references draw it and the incoming members stay behind (TODOS C-13).
    ///
    /// A FILTER RATHER THAN A SECOND COPIER. `PANOYAKOPYALA` needs exactly this
    /// function over a SELECTION, and the alternative was a second walk over the
    /// entity table that would have to learn dash patterns, pictures, interned
    /// styles, layers, blocks and attribute columns all over again — and would
    /// fall behind the day a kind gains one of them (CLAUDE.md 5.10).
    ///
    /// `blocks` names definitions of `scratch` to bring WHOLE besides — a block
    /// taken from a library file (`BLOKEKLE dosya=`); with `only` empty and
    /// `blocks` given, nothing on the scratch sheet comes. `into` puts what does
    /// come from the scratch SHEET into that definition of this document instead
    /// of onto the sheet — a whole drawing inserted as a block.
    core::Result<AdoptSummary> adopt_from(const core::Document& scratch,
                                          std::span<const core::EntityKey> only = {},
                                          std::span<const core::BlockId> blocks = {},
                                          core::BlockId into                    = core::kNoBlock);

    /// How `adopt_from` brings a scratch document across, beyond what to bring.
    struct AdoptOptions
    {
        std::span<const core::EntityKey> only; ///< as `adopt_from`'s `only`
        std::span<const core::BlockId> blocks; ///< as `adopt_from`'s `blocks`
        core::BlockId into{core::kNoBlock};    ///< as `adopt_from`'s `into`

        /// AN EXTERNAL REFERENCE'S LOAD (model.md R45a), when not empty — the
        /// reference's name and a bar, `ALTLIK|`. The scratch's layers and
        /// blocks arrive under `<prefix><name>`, so nothing of the file lands on
        /// a name the drawing uses; layer `0` stays `0`, whose members take the
        /// reference's layer when drawn. A layer made here is grouped under the
        /// reference, and one already here keeps what the user set on it — its
        /// colour, its visibility, its lock. The blocks made are dependents of
        /// the reference (`core::kBlockDependent`), and a dependent already here
        /// is filled again: its caller took its old members out first. Ties are
        /// not carried, since nothing a reference holds is ever edited.
        std::string prefix;
    };

    core::Result<AdoptSummary> adopt_from(const core::Document& scratch,
                                          const AdoptOptions& options);

    /// Whether this transaction may change a PROPERTY of `e` — its layer,
    /// style, text or a cell (TODOS F-02). An entity born inside this
    /// transaction always may: a copy, a piece, a member being assembled, a
    /// reference's file being read in. One that was already there may only
    /// where the document allows an edit (`Document::editable`): not on a
    /// locked layer, not a member of a block or an external reference. The
    /// geometry setters ask the document the same question themselves; these
    /// four did not, so a locked parcel's ada number could be retyped, moved to
    /// another layer or restyled, and a reference's member edited until the
    /// next reload quietly undid it.
    Status may_change(EntityId e) const;

    /// Moves an entity to another layer, keeping its identity.
    Status set_entity_layer(EntityId e, LayerId layer);

    Status erase_entity(EntityId e);
    Status restore_entity(EntityId e);

    /// Takes a MEMBER out of its block definition — what BLOKDÜZENLE does to a
    /// member the edit removed, and what reading a file does to a member row
    /// written dead. `erase_entity` is the road a command takes to an object ON
    /// the sheet and refuses a member by design; this one is for the definition
    /// and refuses anything that is not a member.
    Status erase_member(EntityId e);

    /// Moves the base point of `block` (`Document::set_block_base`).
    Status set_block_base(core::BlockId block, Point2 base);

    /// Makes `block` an external reference to `path`, or a drawn block again
    /// (`Document::set_block_external`).
    Status set_block_external(core::BlockId block, std::string path, std::uint8_t flags);

    /// Takes every live member out of `block` (`erase_member` on each) — what
    /// reloading or unloading an external reference does before its file's
    /// members come in again (model.md R45a). How many went.
    core::Result<std::size_t> clear_block_members(core::BlockId block);

    /// Brings the stored box of every block reference that draws `block` —
    /// itself, or through a block inside a block, on the sheet or inside
    /// another definition — up to date with what it draws now. How many boxes
    /// changed.
    core::Result<std::size_t> refresh_block_references(core::BlockId block);

    /// Steps the entity key counter up to `next` (`Document::skip_entity_keys_to`):
    /// the reader's way over keys a file left out. No inverse is recorded — a
    /// key once passed is never handed out, whether or not the step is undone.
    Status skip_entity_keys_to(core::EntityKey next);

    /// Stands block reference `e` at `insertion`, box refreshed
    /// (`Document::move_reference`): the reference's half of keeping a picture
    /// in place while its definition's base moves.
    Status move_reference(EntityId e, Point2 insertion);

    /// Brings the drawn box a block reference stores up to date with what its
    /// definition draws now (`Document::refresh_reference_bounds`). A cache, not
    /// an edit of the reference: a reference on a locked layer, or inside
    /// another definition, is refreshed too.
    Status refresh_reference_bounds(EntityId e);
    Status set_layer_visible(LayerId l, bool visible);
    Status set_layer_locked(LayerId l, bool locked);
    Status set_layer_appearance(LayerId l, const Appearance& a);
    Status set_layer_style(LayerId l, StyleId style);

    /// Moves a layer in the layer tree. An empty path puts it at the root.
    Status set_layer_group(LayerId l, std::string group);
    Status set_entity_style(EntityId e, StyleId style);

    Status set_entity_hidden(EntityId e, bool hidden);

    /// Adds a drafting guide (`core/guide.hpp`). Furniture rather than geometry,
    /// but a document change all the same: journalled, undone in one step, and
    /// saved with the file.
    Status add_guide(core::GuideAxis axis, core::Mm coordinate);

    /// Adds an angled guide through `at`, running at `angle` micro-degrees;
    /// `ray` makes it one-sided (`KILAVUZ tur=isin`).
    Status add_angled_guide(core::Point2 at, std::int64_t angle, bool ray);

    /// Adds whatever `row` describes — how the file reader restores a guide list
    /// without having to branch on its kind.
    Status add_guide_row(const core::GuideRow& row);

    /// Removes the guide at `index`.
    Status remove_guide(std::size_t index);

    /// Replaces the whole layout list (`core/layout.hpp`).
    ///
    /// WHOLE-LIST, and every layout command goes through it: the caller reads
    /// `document().layouts().all()`, changes its copy and hands it back. One
    /// mutator covers adding a sheet, deleting one, renaming it, moving an item
    /// and retyping a title — and each is one transaction, one journal line and
    /// one Ctrl+Z, like every other edit (Article 1.5).
    Status set_layouts(std::vector<core::Layout> layouts);
    /// Sets the document's CRS. The whole record, so undo restores the metadata
    /// the geodesy module resolved along with the id.
    Status set_crs(core::Crs crs);

    /// R28's generic attribute write, and the only sanctioned way to reach one.
    /// Undoable: an ada number typed wrong is exactly the kind of mistake Ctrl+Z
    /// exists for, and the previous value is what the document hands back.
    Status set_attribute(core::AttrId col, EntityId e, const core::AttrValue& v);

    /// Declares a column. NOT undoable — see Document::declare_attribute.
    core::Result<core::AttrId> declare_attribute(core::AttrSpec spec);

    /// Drops a column and everything in it. NOT undoable — see
    /// `Document::drop_attribute`, and ask the user before calling it.
    core::Status drop_attribute(std::string_view id);

    /// Changes a column's name, description, requiredness, catalogue or scale.
    /// NOT undoable; cannot change the id or the type.
    core::Status amend_attribute(std::string_view id, const core::AttrSpec& next);

    /// Attaches or replaces the text on an entity. Height is ground millimetres;
    /// an empty `content` detaches it. Undoable like any other edit. Without
    /// `lines` the text keeps the line layout it has (a new text, the default).
    Status set_text(EntityId e, std::string content, core::Mm height, core::TextAnchor anchor);
    Status set_text(EntityId e, std::string content, core::Mm height, core::TextAnchor anchor,
                    core::TextLines lines);

    /// Makes `e` FOLLOW `a.source` (`core/attach.hpp`, `Document::set_attachment`).
    Status set_attachment(EntityId e, const core::Attachment& a);

    /// Makes `e` follow nothing. Undoable: the previous attachment comes back.
    Status clear_attachment(EntityId e);

    /// Replaces the links of dimension `dim` (`Document::set_dimension_links`).
    Status set_dimension_links(EntityId dim, std::span<const core::DimLink> links);

    /// Replaces the boundary sources of hatch `hatch` (`Document::set_hatch_links`).
    Status set_hatch_links(EntityId hatch, std::span<const core::HatchSource> sources);

    /// Records where `e` came from (`Document::set_lineage`, core/lineage.hpp).
    /// Undoable with the edit that made it.
    Status set_lineage(EntityId e, core::Lineage origin);

    /// What `settle_attachments` did.
    struct SettleReport
    {
        std::size_t followed{0};        ///< dependents re-placed after their source moved
        std::size_t relabelled{0};      ///< dependents whose derived text changed
        std::size_t erased{0};          ///< dependents erased because their source was
        std::size_t reoffset{0};        ///< dependents moved by hand, offset re-measured
        std::size_t left{0};            ///< dependents that could not follow: not editable (locked)
        std::size_t dims_followed{0};   ///< linked dimensions re-laid out after their source moved
        std::size_t dims_broken{0};     ///< links broken: the object measured was erased
        std::size_t dims_cornerless{0}; ///< links broken: the corner measured was removed
        std::size_t dims_relinked{0};   ///< links moved to the object that replaced theirs
        std::size_t dims_released{0};   ///< links released: the dimension's own point was moved off
        std::size_t dims_left{0};       ///< linked dimensions that could not follow: locked
        std::size_t dims_manual{0}; ///< followed and re-measured, but the caption is typed by hand
        std::size_t hatches_followed{0};   ///< linked hatches rebuilt from their moved boundary
        std::size_t hatches_broken{0};     ///< sources broken: the boundary object was erased
        std::size_t hatches_open{0};       ///< sources broken: the boundary object no longer closes
        std::size_t hatches_released{0};   ///< hatches moved on their own, unlinked
        std::size_t hatches_left{0};       ///< linked hatches that could not follow: locked
        std::size_t results_stale{0};      ///< results a source of which this changed: out of date
        std::size_t results_sourceless{0}; ///< results a source of which this erased
        std::size_t results_released{0};   ///< results reshaped on their own: released
        std::size_t results_carried{0};    ///< results reshaped with their sources: still current
        std::size_t results_asked{0};      ///< shared origins compared: the cost, for a test
        std::vector<std::string> stale_by; ///< what made the stale and sourceless ones, once
        std::vector<std::string> released_by; ///< what made the released ones, once
        std::size_t caught_up{0}; ///< followers unlocked that caught up with their source
    };

    /// Keeps every TEXT this transaction touched standing on a baseline exactly
    /// as long as its words are wide (TODOS C-18): a text that does not wrap
    /// gets the length `core::text_width` gives its words at its height, in the
    /// direction its baseline already points — whatever put it there: a new
    /// text, a rewrite, a grip dragged along it, a height scaled, a file read.
    /// Its box is then the words, never longer or shorter, for the cull and the
    /// pick alike. Within a millimetre is already so — a turned baseline's end
    /// is rounded to one — and nothing is written for it. A caption that
    /// follows a source is placed by `settle_attachments`, by the same measure,
    /// and left to it. Runs first at commit, on its own cursor; silent, because
    /// it moves nothing a user can see.
    void settle_texts();

    /// Brings every dependent up to date with what this transaction did to its
    /// source — THE ONE PLACE it happens, at commit, never per frame (model.md
    /// R14's pattern applied to geometry). Reads the inverse record since the
    /// last call: a source whose geometry changed has its dependents re-placed
    /// and, for a derived text, re-worded; a source that was erased takes its
    /// dependents with it; a dependent moved on its own keeps that offset. Every
    /// write lands in this same transaction, so one undo step covers the command
    /// and what followed from it. Idempotent: a second call finds nothing to do.
    SettleReport settle_attachments();

    /// Brings caption `d` to where its rule puts it now, and its words up to
    /// date (TODOS F-04): what a caption that could not follow — it was locked —
    /// needs once it may. Refused for one that follows nothing, may not be
    /// edited, or whose feature is gone; counted into `rep` like the settle's.
    Status follow_caption(EntityId d, SettleReport& rep);

    /// Keeps a tie naming the same feature of a changed source, for a dependent
    /// that may not itself be edited (`Document::retie_attachment` and its two
    /// siblings): the settle's road only.
    Status retie_attachment(EntityId e, const core::Attachment& a);
    Status retie_dimension(EntityId dim, std::span<const core::DimLink> links);
    Status retie_hatch(EntityId hatch, std::span<const core::HatchSource> sources);

    /// Brings every LINKED DIMENSION up to date with what this transaction did
    /// to what it measures (core/dimension_link.hpp): a moved source re-lays
    /// the dimension out around the moved feature and re-words its figure in
    /// `unit`; an erased one breaks the link and leaves the dimension where it
    /// was; a dimension whose own point was moved off its feature, the source
    /// standing still, releases that link. A link names a CORNER, not a number:
    /// a corner inserted or removed before it, or a line reversed, renumbers
    /// the link and moves nothing, and a corner within `tolerance` of where the
    /// measured one was is still that corner; an object erased and replaced in
    /// the same command (a join, an explode) hands its links to the new object
    /// at the same point. Its own cursor, so the writes it makes are never read
    /// back as a dimension the user moved.
    SettleReport settle_dimensions(core::DrawingUnit unit, core::Mm tolerance);

    /// Brings dimension `dim` back onto the features its links name, re-laid
    /// out and re-measured in `unit` (TODOS F-04): for one that could not
    /// follow. Refused for one that has no links or may not be edited.
    Status follow_dimension(EntityId dim, core::DrawingUnit unit, SettleReport& rep);

    /// Brings every LINKED HATCH up to date with what this transaction did to
    /// its boundary (core/hatch_link.hpp): the loops built again from the
    /// sources as they are now, holes by nesting, the pattern's origin carried
    /// along when every source moved by one offset; a source erased, or no
    /// longer closed, is kept as broken and the hatch stays as it was; a hatch
    /// moved on its own is released. Its own cursor, like the dimensions'.
    SettleReport settle_hatches();

    /// Builds hatch `hatch` again from its boundary objects as they are now
    /// (TODOS F-04): for one that could not follow. Refused for one with no
    /// sources, a source lost, or a lock.
    Status follow_hatch(EntityId hatch, SettleReport& rep);

    /// LETS A FOLLOWER CATCH UP (TODOS F-04): a caption, a dimension or a hatch
    /// that could not follow its source while it was locked, and that this
    /// transaction set free — its layer unlocked, or it moved to another layer
    /// — is brought to its source as the settle would have, in the same undo
    /// step. Only the ones behind: a current follower is left untouched. Runs
    /// after the three settles above, and advances their cursors past its own
    /// writes, so none of them reads a caught-up follower as one moved by hand.
    SettleReport settle_unlocked(core::DrawingUnit unit);

    /// Says which RESULTS this transaction put out of date (TODOS F-04,
    /// core/lineage.hpp): of the objects it reshaped, re-worded, re-valued or
    /// erased, the ones a result was computed from — and only those results
    /// are compared with their sources, so an edit far from any result costs
    /// nothing. Whether a result is current is read from the drawing, never
    /// stored. What it writes is for a result that was ITSELF reshaped: carried
    /// with every source it has, it is recorded against them as they are now;
    /// reshaped on its own, it is released — history kept, claim dropped — in
    /// the same transaction. Its own cursor, like the dimensions'.
    SettleReport settle_results();

    /// The objects this transaction created after `mark` — a `size()` read
    /// earlier — in the order it made them: what a nested run left behind.
    std::vector<EntityId> created_since(std::size_t mark) const;

    /// Reverts every edit made through this transaction, newest first.
    void rollback();

    /// Reverts the edits made after `mark` — a `size()` read earlier — newest
    /// first, and keeps the ones before it. A command inside a batch writes into
    /// the batch's transaction, and when it fails, what goes back is that
    /// command's own edits: the ones before it belong to commands that succeeded
    /// and are already in the journal (CLAUDE.md 1.6 is about the failed command
    /// being whole, not about taking its neighbours with it).
    void rollback_to(std::size_t mark);

    /// Hands the inverse record over to the undo stack and clears it.
    std::vector<Op> release();

    bool empty() const noexcept { return inverse_.empty(); }

    std::size_t size() const noexcept { return inverse_.size(); }

    const std::string& label() const noexcept { return label_; }

    void set_label(std::string l) { label_ = std::move(l); }

    Document& document() noexcept { return doc_; }

private:
    Document& doc_;
    std::string label_;
    std::vector<Op> inverse_; ///< newest last

    /// How far `settle_attachments` has read `inverse_`; the ops before it were
    /// already answered.
    std::size_t settled_upto_{0};
    std::size_t dims_settled_upto_{0};     ///< how far `settle_dimensions` has read
    std::size_t hatches_settled_upto_{0};  ///< how far `settle_hatches` has read
    std::size_t texts_settled_upto_{0};    ///< how far `settle_texts` has read
    std::size_t results_settled_upto_{0};  ///< how far `settle_results` has read
    std::size_t unlocked_settled_upto_{0}; ///< how far `settle_unlocked` has read

    /// The first entity row this transaction can have created: rows are
    /// append-only, so everything at or past it was born inside it.
    EntityId first_born_{0};

    /// Whether `hatch`'s rings are exactly what `sources` give it now.
    bool fills_boundary(EntityId hatch, std::span<const core::HatchSource> sources) const;

    /// Re-lays dimension `dim` out with its points moved as `moves` say, re-worded
    /// in `unit`, and counts it; false when it could not be rebuilt.
    bool rebuild_dimension(EntityId dim,
                           std::span<const std::pair<std::size_t, core::Point2>> moves,
                           core::DrawingUnit unit, SettleReport& rep);

    /// Writes caption `d` where `a` puts it, and its words, only what changed;
    /// `stored` is the attachment it has, written over when `a` differs.
    bool place_caption(EntityId d, const core::Attachment& stored, const core::Attachment& a,
                       SettleReport& rep);
};

struct UndoEntry
{
    std::string label;       ///< what the user reads on the GERİAL menu item
    std::vector<Op> inverse; ///< the Ops that undo the command, newest last
};

class UndoStack
{
public:
    /// Puts a completed transaction on the stack and CLEARS the redo side: once
    /// the user edits after undoing, the branch they undid is gone, which is what
    /// every editor does and what any other answer would make unpredictable.
    void push(UndoEntry e);

    /// Whether there is anything to undo or redo, for the menu items.
    bool can_undo() const noexcept { return !undo_.empty(); }

    bool can_redo() const noexcept { return !redo_.empty(); }

    /// Applies the top inverse record and moves it to the redo stack.
    Status undo(Document& doc, std::string* label_out = nullptr);
    Status redo(Document& doc, std::string* label_out = nullptr);

    void clear();

    std::size_t undo_depth() const noexcept { return undo_.size(); }

    std::size_t redo_depth() const noexcept { return redo_.size(); }

    std::string next_undo_label() const
    {
        return undo_.empty() ? std::string{} : undo_.back().label;
    }

    /// The label of what would be redone, or empty. Shown in the menu so the item
    /// reads "Yinele: Katman ekle" rather than a bare "Yinele".
    std::string next_redo_label() const
    {
        return redo_.empty() ? std::string{} : redo_.back().label;
    }

private:
    std::vector<UndoEntry> undo_;
    std::vector<UndoEntry> redo_;
};

} // namespace kentos::command
