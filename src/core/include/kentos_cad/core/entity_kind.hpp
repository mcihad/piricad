// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: entity kinds.
//
// .claude/model.md R22–R26. An entity kind is six free function pointers over
// spans and nothing else. No vptr, no capture, no std::function, no virtual
// (R22, P1): a vptr per entity costs eight bytes and an indirect load per
// entity per frame on a five-million-parcel layer, and that layer is already
// the slowest measured scenario.
//
// Dispatch is per (kind, layer, style) BATCH, never per entity (R23, P2). Every
// function below therefore takes a span of slots and fills a span of results;
// there is deliberately no single-entity overload to reach for by accident.
//
// The table is owned by the Document and passed by reference (R24). Core has no
// process-wide mutable registry (core.md P8), because bbox_of(entity) must not
// depend on global state — two documents open at once would otherwise be able to
// disagree about what "core.polyline" means.
#pragma once

#include "kentos_cad/core/geometry.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/result.hpp"
#include "kentos_cad/core/units.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

// `KindId`, `kNoKind` and the built-in kind ids live in identity.hpp, so a caller
// who only asks "is this a circle?" need not include this header.

/// The slots handed to one kind function call. One call per batch (R23).
using SlotSpan = std::span<const std::uint32_t>;

/// A run that takes its style from the entity it belongs to (`EmitBuffer::run_style`).
inline constexpr std::uint32_t kInheritRunStyle = 0xFFFFFFFFu;
/// A run drawn on the entity's own layer (`EmitBuffer::run_layer`).
inline constexpr std::uint32_t kInheritRunLayer = 0xFFFFFFFFu;
/// A run that carries no caption (`EmitBuffer::run_text`).
inline constexpr std::uint32_t kNoRunText = 0xFFFFFFFFu;

/// Where a kind writes the geometry it wants drawn: flat coordinate arrays plus
/// a run table, which is what a DrawList wants and what a GPU buffer upload
/// wants. Deliberately not an interface with a virtual push_vertex(): the whole
/// point of batch dispatch is that no per-entity indirect call survives.
struct EmitBuffer
{
    /// Flat coordinate arrays, in DOCUMENT millimetres. The screen-space
    /// conversion happens later, in the scene builder, because a kind must not
    /// need to know the view to describe itself.
    std::vector<Mm> xs;
    std::vector<Mm> ys;

    std::vector<std::uint32_t> run_start; ///< first vertex of the run
    std::vector<std::uint32_t> run_count; ///< how many vertices the run holds
    std::vector<std::uint8_t> run_closed; ///< 1 = the closing segment is implied
    /// 1 = the run is a VOID in its entity rather than an outline — a hatch's
    /// island, a face's interior ring. The fill pass reads it the way it reads
    /// `RingRole::Interior` on a stored ring; a stroke pass ignores it.
    std::vector<std::uint8_t> run_hole;

    /// THE STYLE, LAYER AND CAPTION OF A RUN, when they are not the entity's own.
    /// A block reference draws its definition's members, and a member keeps its
    /// own colour, its own layer and its own text; the scene builder reads these
    /// three columns and batches the run where THEY say. `kInheritRunStyle`,
    /// `kInheritRunLayer` and `kNoRunText` — what every other kind writes — mean
    /// "the entity's", which is what the columns meant before they existed.
    std::vector<std::uint32_t> run_style; ///< a StyleId, or kInheritRunStyle
    std::vector<std::uint32_t> run_layer; ///< a LayerId, or kInheritRunLayer
    std::vector<std::uint32_t>
        run_text; ///< a text-table slot whose caption this run is the baseline of, or kNoRunText

    /// How many runs have been emitted.
    std::size_t run_total() const noexcept { return run_start.size(); }

    /// Starts a run. `closed` says the segment back to the first vertex is
    /// implied rather than stored — the ring convention the geometry uses;
    /// `hole` says the run bounds a void rather than the shape (see `run_hole`);
    /// `style`, `layer` and `text` are the run's own or the inherit sentinels.
    void begin_run(bool closed, bool hole = false, std::uint32_t style = kInheritRunStyle,
                   std::uint32_t layer = kInheritRunLayer, std::uint32_t text = kNoRunText)
    {
        run_start.push_back(static_cast<std::uint32_t>(xs.size()));
        run_count.push_back(0);
        run_closed.push_back(closed ? std::uint8_t{1} : std::uint8_t{0});
        run_hole.push_back(hole ? std::uint8_t{1} : std::uint8_t{0});
        run_style.push_back(style);
        run_layer.push_back(layer);
        run_text.push_back(text);
    }

    /// The eastings of run `i`.
    std::span<const Mm> run_xs(std::size_t i) const noexcept
    {
        return {xs.data() + run_start[i], run_count[i]};
    }

    /// The northings of run `i`.
    std::span<const Mm> run_ys(std::size_t i) const noexcept
    {
        return {ys.data() + run_start[i], run_count[i]};
    }

    /// Appends one vertex to the run in progress. Undefined before `begin_run`,
    /// which is a programming error rather than a data condition.
    void push_vertex(Mm x, Mm y)
    {
        xs.push_back(x);
        ys.push_back(y);
        ++run_count.back();
    }

    /// Empties the buffer and KEEPS its capacity: this is reused every frame and
    /// the draw path must not allocate (§10.4).
    void clear()
    {
        xs.clear();
        ys.clear();
        run_start.clear();
        run_count.clear();
        run_closed.clear();
        run_hole.clear();
        run_style.clear();
        run_layer.clear();
        run_text.clear();
    }
};

// ------------------------------------------------------------ the six fns ----

/// Bounding box per slot. `out.size()` must equal `slots.size()`.
using BboxFn = void (*)(const RingGeometry& geom, SlotSpan slots, std::span<Box2> out);

/// Drawable geometry for the batch, appended to `into` in slot order.
using OutlineFn = void (*)(const RingGeometry& geom, SlotSpan slots, EmitBuffer& into);

/// 1 for every slot within `tolerance` millimetres of `probe`, 0 otherwise.
using HitFn = void (*)(const RingGeometry& geom, SlotSpan slots, Point2 probe, Mm tolerance,
                       std::span<std::uint8_t> out);

/// Net area per slot in square millimetres — `alan hesabı`, the legal output (R12).
using AreaFn = void (*)(const RingGeometry& geom, SlotSpan slots, std::span<Mm2> out);

/// The length around one entity, in millimetres — a closed shape's perimeter and
/// an open one's length.
///
/// THE KIND HAS TO ANSWER THIS, for the same reason it answers for the area. A
/// circle stores its centre and its radius, so summing the segments of its stored
/// run gives the RADIUS, and that is what `ÖLÇ` reported as an eight-metre
/// circle's circumference. What a shape measures is the shape's own business
/// (model.md R22-R26).
///
/// May be null: `KindSpec` grows by appending and a kind registered against an
/// older layout has no such member, so a caller falls back to
/// `RingGeometry::perimeter_of` — which is exactly right for a polyline and wrong
/// only for the curves that now declare one.
using PerimeterFn = void (*)(const RingGeometry& geom, SlotSpan slots, std::span<Mm> out);

/// Appends one slot decoded from `payload` and returns its index. Payloads come
/// off disk and are untrusted: every length in them is checked against the
/// bytes actually present (io.md).
using ReadFn = Result<std::uint32_t> (*)(RingGeometry& geom, std::span<const std::uint8_t> payload);

/// Appends each slot's payload to `bytes` and each payload's END offset to
/// `ends`, so the reader frames the batch without a per-entity length prefix.
/// Little-endian on every platform, because a file written on one machine is
/// read on another and R26 promises byte-identity.
using WriteFn = void (*)(const RingGeometry& geom, SlotSpan slots, std::vector<std::uint8_t>& bytes,
                         std::vector<std::uint32_t>& ends);

/// Whether `rings` and `payload` describe a well-formed entity of the kind,
/// asked BEFORE anything is appended so a refusal leaves no trace. This is where
/// a kind's own floor lives — a circle wants exactly a centre and a handle due
/// east, an ellipse two axes of non-zero length, an arc-polyline as many bulges
/// as it has segments — and where a payload's lengths are checked against the
/// bytes present, because a payload comes off disk (io.md). The message is what
/// the user reads when a command or a file is refused. May be null: a kind with
/// nothing to check beyond what `RingGeometry::append` already enforces.
using ValidateFn = Status (*)(std::span<const RingGeometry::RingInput> rings,
                              std::span<const std::uint8_t> payload);

/// Where a kind's key points are collected. `modes` holds one snap-mode bit per
/// point (`core/snap.hpp`: `SnapCenter`, `SnapEndpoint`, `SnapInsertion`…), so
/// the snap engine offers each under the mode a user expects rather than one
/// mode for everything.
struct KeyPointSink
{
    std::vector<Point2>& points;       ///< the points, in document millimetres
    std::vector<std::uint32_t>& modes; ///< one snap-mode bit per point
};

/// The points a snap offers on one slot BEYOND its outline: an ellipse's centre
/// and axis ends, a block reference's insertion point, a dimension's definition
/// points. May be null for a kind whose outline is all there is to reach for.
using KeyPointsFn = void (*)(const RingGeometry& geom, std::uint32_t slot, KeyPointSink& into);

/// One entity kind, declared once (R22).
///
/// `size` leads the struct so a later ABI boundary can grow it by appending
/// fields only, exactly as `kentos_host_vN` does (.claude/plugin-api.md).
/// Names are `const char*` rather than std::string so the record stays a POD an
/// out-of-process plugin can hand over by value.
struct KindSpec
{
    /// How many aliases one kind may declare — Turkish, ASCII-folded, English and
    /// abbreviations. Fixed so the record stays a POD with no allocation.
    static constexpr std::size_t kMaxNames = 6;

    /// The record's own size, FIRST, so a later version can append fields and an
    /// older reader can still tell how much of the struct it understands.
    std::uint32_t size{sizeof(KindSpec)};

    /// The kind's number, DECLARED by the kind itself rather than handed out in
    /// registration order — see `kPolylineKind`. It reaches the file: the project
    /// writer stores this column, so a value once used can never be re-meant
    /// (R26). `stable_id` is the name a human and a plugin use for the same thing.
    KindId id{kNoKind};
    const char* stable_id{""};  ///< "core.polyline", "cadastre.parsel" — never renamed
    const char* summary_tr{""}; ///< one line, Turkish, shown in help and docs

    /// Turkish primary, ASCII-folded Turkish, English, abbreviations — the
    /// CLAUDE.md 2.6 `.names` order. Unused entries are null.
    const char* names[kMaxNames]{};

    BboxFn bbox{nullptr};
    OutlineFn outline{nullptr};
    HitFn hit{nullptr};
    AreaFn area{nullptr};
    ReadFn read{nullptr};
    WriteFn write{nullptr};

    /// APPENDED, and that is the point of `size` being first: a kind compiled
    /// against the older layout has no such member, and a reader that checks
    /// `size` can tell. Optional for the same reason — see `PerimeterFn`.
    PerimeterFn perimeter{nullptr};

    /// Appended later still, both optional (see their typedefs).
    ValidateFn validate{nullptr};
    KeyPointsFn key_points{nullptr};
};

/// The kinds one Document understands. Owned by the Document, passed by
/// reference (R24). A document loaded from a file carries the kinds that file
/// used; a plugin's kind is added to that document's table by the command that
/// loaded the plugin, never to a global.
class KindTable
{
public:
    /// Fails on a duplicate id, a duplicate name, a missing stable id or a null
    /// function pointer — a half-declared kind would crash at frame time, and
    /// "it dispatched into a null" is not a diagnosable field report.
    Status add(const KindSpec& spec);

    const KindSpec* find(KindId id) const noexcept;

    /// Turkish-folded lookup: ÇOKLUÇİZGİ, cokluçizgi and POLYLINE all resolve.
    const KindSpec* find_name(std::string_view name) const;

    std::size_t size() const noexcept { return specs_.size(); }

    /// Registration order, which is the X-macro order and therefore stable.
    const std::vector<KindSpec>& all() const noexcept { return specs_; }

private:
    std::vector<KindSpec> specs_;
    std::vector<std::pair<std::string, KindId>> folded_; ///< sorted; no hash-order iteration
};

/// The kinds every document starts with.
///
/// This is a function-local `static const` table: it is built once from the
/// X-macro list below and is immutable from the first read onward, so it is
/// const state, which core.md P8 permits — it is NOT a registry. Nothing can
/// add to it; a document that needs more copies it and adds to its own table.
/// Deleting it as "a singleton" would force every caller to rebuild the same
/// immutable table.
const KindTable& builtin_kinds();

/// Declares a kind factory. One factory per kind, one line in the X-macro list,
/// exactly like command/commands/builtin.cpp — one idiom in this codebase, not
/// two (R25).
#define KENTOS_KIND(sym) ::kentos::core::KindSpec kentos_kind_##sym()

/// The drawable, pickable outline of ONE slot, for a kind whose stored vertices
/// are not its outline.
///
/// Returns false for `core.polyline`, whose rings ARE its outline and are read
/// straight out of the arena with no copy — that is every entity on the
/// five-million-parcel sheet the frame budget is written against (§10.1).
/// Returns true for a curve, having filled `into` with the runs to walk.
///
/// THIS IS THE ONE PLACE that answers "what shape is this really". The renderer,
/// the pick test, the snap engine and the canvas all ask it, so a new curve kind
/// becomes visible, selectable and snappable by being registered — not by four
/// separate edits that have to agree.
bool curve_outline(KindId kind, const RingGeometry& geom, std::uint32_t slot, EmitBuffer& into);

/// The built-in kinds, one factory each (R25). The first five live in
/// entity_kind.cpp; every Phase 2 kind has its own file beside its geometry.
KENTOS_KIND(polyline);
KENTOS_KIND(circle);
KENTOS_KIND(arc);
KENTOS_KIND(point);
KENTOS_KIND(ellipse);
KENTOS_KIND(arc_polyline);
KENTOS_KIND(spline);
KENTOS_KIND(hatch);
KENTOS_KIND(block_reference);
KENTOS_KIND(dimension);
KENTOS_KIND(leader);

/// Where a `core.point` slot sits.
Point2 point_position_of(const RingGeometry& geom, std::uint32_t slot);

} // namespace kentos::core
