# Document Model — Rules

> Scope: `kentos_cad/core/{units,document,geometry,identity,style,layer,attribute,entity_kind,settings}.hpp` and every file that persists or renders them  |  Depends on: nothing  |  Source: kentoscad.md §2.2, §9.3, §10.2, §10.3, §10.5, §12, §13

These rules were settled after a six-way study of AutoCAD/ObjectARX, the ODA DWG object model,
FreeCAD, LibreCAD, QGIS, GDAL/OGR, ArcGIS, FlatGeobuf, GeoPackage and Blender, and an adversarial
review of the result. They are **closed for modification**: the shapes below may gain fields, never
change meaning. Where a rule cost something, the cost is stated so a later reader does not "fix" it.

## Hard Rules

### Identity

R1. Two id kinds exist and MUST NOT be confused.
  * `EntityId` / `LayerId` — dense `u32` **slot**. Valid only inside one in-memory `Document`. Used by the cull path, the spatial index, `DrawList`, `Op`.
  * `EntityKey` / `LayerKey` — persistent `u64` **key**. Monotonic, never reused, survives save, load, reorder and compaction. Used by the journal, selection, external references, and every cross-document link.
R2. Translation between slot and key happens at the **command bus boundary only**. A `u64` key MUST NOT appear in the cull path, the index, or `DrawList`.
R3. The key space is capped at 2⁶³−1, because `command::Value::Kind::IdList` is `std::vector<std::int64_t>` and a key above that silently becomes negative in the journal.
R4. A retired key is never reused, even after the entity is erased and the file is compacted. Reuse makes "which parcel was this?" unanswerable, and that question is a legal one (§12).
R5. Every id that reaches a user, a file, a journal line or an AI tool call is a **key**. Every id inside a frame is a **slot**.

### The cull block is closed

R6. The per-frame cull test reads exactly two things: the four bbox arrays (`min_x, min_y, max_x, max_y`) and one `flags` byte. Nothing else. `layer`, `kind`, `style`, `slot`, `order` and `key` are read **only for entities the index returned**.
R7. `flags` carries `alive`, `hidden` (per entity), `layer_hidden` (mirrored from the layer, refreshed by the command that changes layer visibility) and `in_block` (the entity belongs to a block definition and is drawn only through a reference, R45). Four bits, one byte; R6 and R8 are unchanged. Mirroring costs one O(n) pass per visibility toggle and saves an indirect load per entity per frame; that trade is deliberate.
R8. Adding a column to the cull block requires amending this rule. The measured cost of the full-extent path is recorded in `tests/bench/temel-degerler.json`; a new cull column is a memory-traffic regression on the frame path that is already the slowest measured scenario.

### Geometry

R9. Geometry is stored as **entity → rings → vertices**, not as a single vertex run. A cadastral parcel is a ring, may have interior rings, and may be multipart. `(start, count)` cannot express a parcel with a hole, and yola terk and irtifak routinely produce one.
R10. Each ring carries `start`, `count`, `part` and `role`. `role` is `Open`, `Exterior` or `Interior`. A polyline is one `Open` ring; a parcel is one `Exterior` ring; a parcel with an exclusion is `Exterior` + `Interior` in the same part; a multipart parcel uses distinct `part` values.
R11. Ring order within an entity is `part` ascending, then `Exterior` before its `Interior` rings. This ordering is part of the content hash and therefore of every golden fixture.
R12. Area is computed over rings with sign by role, never over a raw vertex run. Alan hesabı is the legal output (§12).
R9a. A kind may store BESIDE its rings a **payload**: fixed-width little-endian integer fields laid down with `core/wire.hpp`, interpreted only by the kind's own `KindSpec`, never by the arena, the renderer or a command. It is slot-indexed in `RingGeometry` (`payload_ref` per slot, records, one pool) and in file blocks `0x0080–0x0083`, written only when a payload exists; it is folded byte for byte into `content_hash()`; an unknown kind's payload is kept unchanged (R26). Changing a payload appends a new slot and repoints the entity (`set_kind_payload`), so the inverse is `Op::SetGeometry` and no Op variant exists for it. A polyline has none and pays nothing.
R9b. An entity's **KIND may change in place**, together with its rings and payload in one step (`Document::set_kind_geometry` with a kind), when an edit needs another kind to hold what it made: a polyline whose straight edge became an arc is a `core.arc_polyline` now, and one whose last arc went straight is a polyline again. The key, layer, style, attributes, caption and every dependent (R46) stay — which is the point: a parcel does not lose its title or its edge lengths because one of its boundaries was bent. The new kind validates the pair first; a kind this build does not know is refused either way (R26). The inverse is `Op::SetKindGeometry`, the old slot and the old kind together, and `settle_attachments` reads it as a geometry change. This amendment adds a capability and changes no stored shape: the kind column and the slots were always per entity, and nothing on disk moves.

### Style and the cascade

R13. Every entity carries one `StyleId` (`u32`) indexing a document-owned, deduplicated `StyleTable` of POD `Appearance` records. `StyleId{0}` means "inherit everything from my layer".
R14. **A GIS renderer is a command that writes the style column. Style is NEVER derived at frame time.** Resolution runs inside a `Transaction` at commit, in this order:
  1. explicit `style[e]` if not `kByLayer`
  2. else the layer's classification rule over the entity's attributes (the GIS renderer)
  3. else the layer's own appearance (ByLayer)
  The *result* is written back into `style[e]`. The renderer then reads one `u32`.
R15. Recomputation is triggered by exactly four commands — an entity's classifying attribute changed, a layer's rule changed, a catalogue package was reloaded, entities were imported. Each is one transaction, one undo step, one journal line, over the dirty set the command already knows.
R16. Scale-dependent symbology is baked into the `StyleTable` **once per frame**, never per entity. The table is O(hundreds); the entity column does not change with scale.
R17. Continuous data-driven properties are quantised to at most 256 classes, each a `StyleId`. If a genuinely continuous per-entity value is ever needed it becomes its own optional numeric column feeding a GPU instance attribute — never a CPU evaluation per entity.
R18. `Appearance` carries line **and area** symbology: `rgba`, `width_um`, `dash`, `symbol`, `fill_rgba`, `hatch`, `z_order`. MPYY EK-1 plan gösterim is overwhelmingly area symbology; a line-only `Appearance` cannot represent an imar planı.
R19. The ByLayer/ByBlock cascade is an explicit `Source` enum in memory. The wire sentinels (`62 == 256`, `370 == -1`, `"BYLAYER"`) exist **only** inside `/src/io`.

### Units

R20. Plot width is **paper micrometres**, `i32`, 1 µm = 1/1000 mm. Screen pixels are derived per frame from paper width × scale × dpi. MPYY prescribes çizgi kalınlığı in mm on the pafta and DXF group 370 is 1/100 mm on paper; a width stored in pixels round-trips to neither.
R21. No stored field is floating point. Coordinates are `Mm`, widths are µm, angles are micro-degrees, scales are rationals, epochs are fixed-point years.

### Kinds and extension

R22. An entity kind is declared once as a `KindSpec` carrying free function pointers over spans — `bbox`, `emit`, `hit`, `area`, `read`, `write`, and the appended optional `perimeter`, `validate` (the kind's own floor, asked before a byte is appended) and `key_points` (what a snap offers beyond the outline, each under its own mode). No `this`, no capture, no `std::function`, no virtual. `core::entity_outline` (outline.hpp) is the ONE document-aware entry the renderer, the pick test, the snap engine and the canvas call for a kind's drawn shape; its `EmitBuffer` runs carry `closed`, `hole`, and — for a run that is not the entity's own, a block member's — `style`, `layer` and `text` (the inherit sentinels mean the entity's). Every payload-carrying kind starts its payload with the 8-byte header `u16 layout_version, u16 flags, u32 reserved` (`kind_common.hpp`), so a later layout is refused by version, never misread.
R23. Kind function pointers are dispatched **once per (kind, layer, style) batch**, never per entity. The batch count is bounded by kinds × layers × styles, never by entity count.
R24. The kind table is **owned by the `Document`** and passed by reference, exactly like every other piece of core state. There is no process-wide mutable registry in core (`core.md` P8). `/src/command` owns the registration list and the docgen/AI projection.
R25. Registration mirrors `commands/builtin.cpp` exactly: one factory per kind, one X-macro list, integer ids, generated docs. One idiom in the codebase, not two.
R26. An entity of an unknown kind loads as **visible, preserved and non-editable**, and its payload round-trips byte-identically. Dropping it is data destruction with legal consequences (`domain.md` P9). `Document::editable` is the one test every in-place edit asks; it refuses an unknown kind and a block member (R45).
R26a. **Foreign data** — bytes another program attached to an entity (DXF XDATA) — lives in `core::ForeignTable`: per slot, per tag, schemaless. It is never queried by name, never shown beyond its count, edited by no command, never read by the frame path, folded into `content_hash()`, written to file blocks `0x0084/0x0085` and returned to the source format as it came. This is not a property bag (P12): nothing is typed, looked up or edited. P8 binds model fields; an IEEE value inside a foreign byte string is the FILE's number, not the document's.

### Attributes

R27. The attribute schema belongs to the **collection**, not the object: dense typed columns declared from `/data`, dictionary-encoded for text, in the shape of `OGRFeatureDefn` and Blender's `CustomData`. Never a per-entity property bag, never XDATA.
R28. An attribute write is one generic `Op` — column, row, previous value — so adding BÖHHBÜY detay kodu, TUCBS teması or ada/parsel numarası adds **zero** `Op` variants.
R29. Attribute columns are never read by the frame path.

### Layers

R30. A layer is identified by its `LayerKey`, never by name or slot. Renaming a layer MUST NOT touch a single entity.
R31. The layer **tree** (grouping, ordering, presentation) is a separate structure from the layer **table** (the records). Merging them makes a rename or a regroup a data migration; QGIS separates them for this reason.
R32. Layer record: key, name, folded name, description, `visible`, `locked`, `plottable`, default `Appearance`, min/max scale denominators, opacity, classification rule reference, catalogue default reference.
R33. The layer name is free text; a catalogue link is a separate field. Export validation refuses unmapped layers rather than the editor refusing the name (`domain.md` R22).

### Catalogue provenance

R34. Regulatory identity has one canonical home: a **per-entity catalogue reference column**. The layer carries a *default* applied at draw time; the document carries the catalogue **package version** those references are read against. Three cardinalities, one authority.
R35. The package version stamp is a **critical** part of the document. A file whose regulatory basis is unknown MUST NOT open silently.

### CRS

R36. `Crs` carries id, **epoch**, zone central meridian, geoid model and vertical datum. A bare id string is not a CRS: `domain.md` R12 makes an unlabelled coordinate an error, and a TM30/TM33 mix-up is the classic field blunder.
R37. CRS is document-level. Per-layer CRS arrives with reprojection, not before, and its absence is documented rather than implied.
R37a. **Axis naming follows Turkish surveying convention, which is the inverse of the mathematical one.** EPSG:5254 itself defines `AXIS["northing (X)", ORDER 1]` and `AXIS["easting (Y)", ORDER 2]`, and BÖHHBÜY practice matches: **Y is sağa değer (easting), X is yukarı değer (northing)**.
  * Storage is unaffected: `Point2::x` holds the easting and `Point2::y` the northing, because those are field names, not axis names.
  * Every user-facing label MUST read `Sağa (Y)` for the easting and `Yukarı (X)` for the northing. Printing `X` beside a 485 km value tells a harita mühendisi something false.
  * Command-line and script input order is **easting first**, matching common Turkish CAD practice.
  * At the PROJ boundary the order MUST be swapped explicitly, because `EPSG:5254` is northing-first. Getting this wrong produces a coordinate that is silently, plausibly wrong — the worst failure mode this product has.

### Settings

R38. A setting is declared once as a `SettingSpec` — id, type, default, scope, range, summary — and the persistence, the validation, the user interface and the documentation are all generated from that declaration.
R39. Three scopes, and the boundary is a rule, not a habit:
  * `App` — per user and machine. Not undoable, not hashed, not in the file.
  * `Project` — travels with the document. **Undoable, journalled, part of `content_hash()`.**
  * `Session` — transient. Not persisted, not hashed, not undoable.
R40. **Anything that can change a byte of an exported legal document is `Scope::Project`.** That is the test; apply it literally.
R41. One command per scope, never one command per setting: `AYAR` writes project settings through a transaction, `TERCİH` writes application settings.
R42. A value out of range in a file written by another version is clamped to the declared range with a recorded warning, never silently accepted and never a hard failure that makes the file unopenable.

### Blocks

R45. Block definitions live in `core::BlockTable`, **append-only**, names unique under Turkish folding, ids reaching the file (blocks `0x0086–0x0088`) and every reference's payload. A member is fixed at its creation (`add_kind(..., in_block)`), flagged `in_block` (R7), kept out of the index, the pick and the cull, and refused by every in-place edit. `BLOKDÜZENLE` is what changes a definition, and it changes it without editing a member: a member leaves by dying (`Transaction::erase_member`; its key stays in the list), a member joins by `add_kind(..., in_block)`, and the base moves by `Op::SetBlockBase` with every reference re-stood where it drew (`Document::move_reference`) — the table stays append-only and R26's refusal stands. (This amendment supersedes "`BLOKDÜZENLE` (Phase 2) is what changes a definition", written before the command existed.) A definition that would contain itself, directly or through the blocks its members reference (`uses`), is refused — `Document::add_kind` records the `uses` edge when the member is a `core.block_reference`. A reference (`core.block_reference`, `block_reference.hpp`) is drawn by expanding the definition through `entity_outline`: members placed by `mul_div_round` and `rotate_udeg`, each run carrying the member's style, layer and caption, a member on layer `0` or with a ByBlock source inheriting the reference's; its stored `bounds` is the drawn form's box and is recomputed by every command that moves it.

R45b. A reference may carry a **CLIP** (`BlockReference::clip`, `BLOKKIRP`, TODOS C-14): a closed boundary of three or more corners in the DEFINITION's own coordinates, so it moves, turns and scales with the reference and crops every copy of a grid alike, and the definition is never touched. An unclipped reference's payload is layout 1 byte for byte; a clipped one is layout 2 (`kBlockReferenceClipLayout`: layout 1's fields, then a `u32` corner count bounded against the bytes present, then the corners), and a file holding one raises `min_reader_version` (`io::kMinReaderVersionClip`). The crop happens ONCE, in `expand_block_definition`, per member in definition space before placement, through Clipper2: an open run keeps its pieces inside; a closed run wholly inside is kept as it is; a closed run the boundary cuts becomes its outline's pieces inside plus its face inside marked FILL-ONLY (`EmitBuffer::run_fill_only`) — filled by the fill pass, never stroked, never offered to the snap or measured by the pick's edge test (its face test still hits it); a caption shows whole or not at all by its first vertex. `bounds` is the crop's box. The snap's key points of a member are offered only where `core::inside_clip` shows them at every level of the placement chain, a middle and a centroid being tested themselves. `PATLAT` sets a clip aside, as AutoCAD's EXPLODE does, and says so; the DXF writer cannot carry it (AutoCAD's `SPATIAL_FILTER`) and says so.

R45a. An **EXTERNAL REFERENCE** (TODOS C-14) is a block definition flagged `core::kBlockExternal`, carrying the `path` of a project, DXF or DWG file. The blocks that file defines arrive beside it as its DEPENDENTS (`core::kBlockDependent`) named `NAME|BLOCK`, its layers as `NAME|LAYER` (layer `0` stays `0`, whose members take the reference's layer when drawn); a layer made for it keeps what the user sets on it across reloads. Its members are loaded from the file — when the drawing opens, inside the read transaction, and on `DIŞREFERANS islem=yenile` — and are NEVER written to the project file: the file holds the definition's name, flags and path (relative to the project file's directory where one exists), leaves a gap in the entity key sequence where the members stood, and raises `min_reader_version` (`io::kMinReaderVersionExternal`). `Document::external_rows()` names those rows; `row_slots()` and `content_hash()` leave them out, and the link tables fold a row by its position among the rest, so a document fingerprints the same whether its references are loaded, missing or reloaded. The block fold holds an external definition's name, flags and the FILE NAME of its path — not its members, and not where the file lives, which is location like the project's own path (R43). No command edits an external definition in place: `BLOKDÜZENLE` and `PATLAT` refuse it by name. Binding (`islem=bagla`) clears the flags and makes it an ordinary block whose members are written from then on. `core::kBlockUnloaded` is persisted user state (not loaded at open); `core::kBlockDetached` keeps the append-only record for a later attach under the same name. A source that cannot be read is a warning when the drawing opens, never a failed open. A source drawn in another coordinate system is carried into the drawing's on load by `DÖNÜŞTÜR` itself, run on the scratch through the host's registry — the one reprojection this program has; `DÖNÜŞTÜR` on the drawing skips external references and reloads them afterwards, in the same transaction, in the new system. A source's own external references are loaded into it before it is adopted and arrive as its dependents (`A|B`); a loop through the files being read, or through the drawing's own file, is refused and said, and nesting stops at eight.

### Attachments

R46. An entity may FOLLOW another: `core::AttachTable` (`core/attach.hpp`), keyed by the DEPENDENT's entity row, holds one `Attachment` per dependent — the source by `EntityKey` (R1: a stored link is a key), the anchored feature (`ring`, `index`, vertex or edge), the placement (`side`, `gap`), the words (`Keep`, or `Length` with unit, precision, separator and format) and the hand's offset (`along`, `across`, in the reading frame). No field is floating point (R21). Entity-indexed, not slot-indexed, because a caption's slot changes on every edit and its row never does; allocated lazily, so a sheet with no attachment pays nothing (R6 is untouched: the frame path never reads it).
R46a. **Dependents are re-placed at COMMIT, never per frame** — R14's pattern applied to geometry. `Transaction::settle_attachments()` runs once in `Bus::finish` (and `end_batch`): a source whose geometry changed re-places and re-words its dependents, a source erased takes them with it, a dependent moved on its own keeps that as its offset, a vertex count that changed re-anchors by proximity (`attach_reanchor`, exact integer distances, ties to the lower index). Every write lands in the same transaction, so one undo step covers the command and what followed; the journal holds only the command, and a replay reproduces the follow.
R46b. The placement arithmetic has ONE home, `core::attach_place` / `attach_text`: the tool that writes a caption and the settle that re-places it call the same function, so the two cannot disagree. A cycle is refused at `Document::set_attachment` (a chain is allowed); a dependent that is not editable (R26, R45) is left alone.
R46c. File block `0x0089` (`AttachRecord`, both ends by key, format through the string pool), written only when an attachment exists; `content_hash()` folds the table only when it is non-empty — a drawing without attachments keeps its fingerprint and its bytes.
R46d. A derived entity carries its **LINEAGE** (TODOS F-02): `core::LineageTable` (`core/lineage.hpp`), keyed by the entity's row like R46, holds one `Lineage` per derived object — the `operation` (the stable id of the command or processing tool that made it, never a display name) and the `sources` it was made from, by `EntityKey`, sorted, none twice, never itself. A source need not be alive: a key is never reused (R1), so an ifraz's pieces name the parcel the same command erased, and a dead row keeps its lineage too. It is HISTORY, not a tie: nothing re-places a derived object when its sources move (contrast R46a). It is recorded through `Context::derive` by the command or the processing runner that makes the object — never by a client — so every client leaves the same record; it is undone with that command (`Op::Kind::SetLineage`), folded into `content_hash()` only when non-empty, and written as file block `0x0096` (`LineageRecord`, one row per source, the operation through the string pool), dead rows included, only when a lineage exists — an older reader steps over it (io.md R10), losing the history and never the drawing. A copy through `adopt_from` (paste, import) does not carry it: its sources are keys of another document. The object that keeps its key through an edit — a trim's first piece, a join done in place — has no lineage; it is the same object.

### Drafting guides

R47. A guide is document **FURNITURE**, not an entity: no geometry the document owns, no style, no layer, no attributes, and it MUST NOT appear in a selection, a cull pass, an export or an area sum. `core::GuideStore` (`core/guide.hpp`) holds it, `Document` owns the store, and it travels in the file. Putting it in the entity table would mean teaching every one of those to ignore it, which is how a construction line ends up in a tapu.
R47a. A guide is a CARDINAL one — an axis (`GuideAxis::Horizontal`, `Vertical`) and a coordinate — or an **ANGLED** one: a point it passes through, a direction, and whether it runs both ways or forward only. The store is six parallel columns (R6's SoA rule); `core::GuideRow` carries one across the seams where six parameters in a row is how a caller comes to pass the northing where the easting goes.
R47b. The stored direction is **MATHEMATICAL** micro-degrees — counter-clockwise from east, `core::atan2_udeg`'s own unit — so nothing converts between the stored number and the snap test. What a user types and what a listing prints go through the session's convention (`core.aci.birim`, `core.aci.kural`) at the edge, once, in `core::math_udeg_from_angle` and `core::direction_turns`. Never a `double` angle in the store (R21).
R47c. The undo record is **THE WHOLE LIST**, because a guide has no key: `Op::Kind::SetGuides` restores the list as it was, and its inverse is the list that is there now — which is what makes a guide change redoable as well as undoable.
R47d. On disk the cardinal columns are `0x0039`/`0x003A` and the angled ones `0x0092`–`0x0095`, **written only when an angled guide exists** (io.md R10). An angled guide is the one thing in this format that raises `min_reader_version` (`io::kMinReaderVersionAngledGuide`), because `kBlkGuideAxis` is an OLD block whose value 2 is new: a reader that has never heard of it would refuse the guide column as corrupt, which is a true refusal with a misleading reason. A drawing of ordinary ruler guides keeps writing 1 and still opens everywhere.

### Not document state

R43. Selection, snap state, view state, active layer and the command line's history are **not** document state. They MUST NOT touch `content_hash()` or `revision()`, and they are not journalled as document mutations.
R44. Selection is stored as `EntityKey`s, because a journalled selection must survive a save, a reorder and a reload.

## Absolute Prohibitions

P1. NEVER put a virtual function, a vptr, a `std::function`, a pointer or a heap allocation in an entity record.
P2. NEVER dispatch per entity. Dispatch per batch.
P3. NEVER add a column to the cull block without amending R6 and re-recording the benchmark baseline.
P4. NEVER let a persistent `EntityKey` into the cull path, or a dense `EntityId` into the journal, a file, a selection or an AI tool call.
P5. NEVER reuse a retired key.
P6. NEVER represent a parcel as a single vertex run. Rings, always.
P7. NEVER evaluate a style, an expression or a classification rule inside the frame path.
P8. NEVER store a floating-point value in the document, the journal or the file.
P9. NEVER store a line width in pixels.
P10. NEVER put a process-wide mutable registry in `/src/core`.
P11. NEVER drop an entity, an attribute or a file chunk that the running build does not understand.
P12. NEVER give an entity a property bag, an XDATA blob or a `QVariant` map.
P13. NEVER identify a layer by name or by slot in anything that is stored.
P14. NEVER make a natural key — ada/parsel, detay kodu, TAKBİS tuple — the entity identity. Ifraz destroys one parcel and creates two; tevhit does the reverse; 18. madde reissues numbers wholesale. The identity is opaque and monotonic; the natural key is a validated attribute with `valid_from`/`valid_until` (`data.md` R4).
P15. NEVER let selection, snapping or the view change `content_hash()`.
P16. NEVER write a file format version until the entity and layer records below are settled. A frozen floor freezes whatever is in it.

## Definitions of Done

- [ ] New stored field: fixed width, no floating point, documented unit, added to `content_hash()` deliberately, golden fixtures regenerated in their own reviewed commit.
- [ ] New entity kind: one `KindSpec` factory (with `validate`), one line in the X-macro list, a `/docs/nesneler/<slug>.md` page with the fixed skeleton and a row in the generated `docs/nesneler/referans.md` (`make reference`), a golden fixture, and no change to the renderer, the index or the undo stack — the renderer sees a new kind only as `entity_outline` runs.
- [ ] New attribute: declared in `/data`, zero new `Op` variants, not read by the frame path.
- [ ] New setting: one `SettingSpec`, correct scope by the R40 test, generated documentation, and a migration entry if it replaces an older id.
- [ ] New dependent kind or rule (R46): the placement in `core/attach.hpp`, the settle unchanged, a `tests/unit/test_processing.cpp` case proving follow, re-word, cascade erase, hand offset and one undo step, and an io round trip.
- [ ] Anything touching the cull block: `make bench` run, `render.pan_zoom_5m` still inside 16 ms, baseline re-recorded in the same commit.

## Enforcement

- `scripts/ci-gate-model.sh` — fails on a floating-point member in a stored record, a `virtual`/`std::function`/raw pointer in an entity or style record, an `EntityId` in a journal or selection signature, a `width_px` field, and a cull-block column outside the R6 list.
- `/tests/unit` — key monotonicity and non-reuse, slot/key translation at the bus boundary only, ring/hole area correctness, style resolution order, setting scope enforcement.
- `/tests/golden` — ring ordering, content hash stability, unknown-kind round trip.
- `/tests/bench` — the R8 cull-block budget.
