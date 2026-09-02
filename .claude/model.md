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
R7. `flags` carries `alive`, `hidden` (per entity) and `layer_hidden` (mirrored from the layer, refreshed by the command that changes layer visibility). Mirroring costs one O(n) pass per visibility toggle and saves an indirect load per entity per frame; that trade is deliberate.
R8. Adding a column to the cull block requires amending this rule. The measured cost of the full-extent path is recorded in `tests/bench/temel-degerler.json`; a new cull column is a memory-traffic regression on the frame path that is already the slowest measured scenario.

### Geometry

R9. Geometry is stored as **entity → rings → vertices**, not as a single vertex run. A cadastral parcel is a ring, may have interior rings, and may be multipart. `(start, count)` cannot express a parcel with a hole, and yola terk and irtifak routinely produce one.
R10. Each ring carries `start`, `count`, `part` and `role`. `role` is `Open`, `Exterior` or `Interior`. A polyline is one `Open` ring; a parcel is one `Exterior` ring; a parcel with an exclusion is `Exterior` + `Interior` in the same part; a multipart parcel uses distinct `part` values.
R11. Ring order within an entity is `part` ascending, then `Exterior` before its `Interior` rings. This ordering is part of the content hash and therefore of every golden fixture.
R12. Area is computed over rings with sign by role, never over a raw vertex run. Alan hesabı is the legal output (§12).

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

R22. An entity kind is declared once as a `KindSpec` carrying free function pointers over spans — `bbox`, `emit`, `hit`, `area`, `read`, `write`. No `this`, no capture, no `std::function`, no virtual.
R23. Kind function pointers are dispatched **once per (kind, layer, style) batch**, never per entity. The batch count is bounded by kinds × layers × styles, never by entity count.
R24. The kind table is **owned by the `Document`** and passed by reference, exactly like every other piece of core state. There is no process-wide mutable registry in core (`core.md` P8). `/src/command` owns the registration list and the docgen/AI projection.
R25. Registration mirrors `commands/builtin.cpp` exactly: one factory per kind, one X-macro list, integer ids, generated docs. One idiom in the codebase, not two.
R26. An entity of an unknown kind loads as **visible, preserved and non-editable**, and its payload round-trips byte-identically. Dropping it is data destruction with legal consequences (`domain.md` P9).

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
- [ ] New entity kind: one `KindSpec` factory, one line in the X-macro list, a `/docs/nesneler/<slug>.md` page, a golden fixture, and no change to the renderer, the index or the undo stack.
- [ ] New attribute: declared in `/data`, zero new `Op` variants, not read by the frame path.
- [ ] New setting: one `SettingSpec`, correct scope by the R40 test, generated documentation, and a migration entry if it replaces an older id.
- [ ] Anything touching the cull block: `make bench` run, `render.pan_zoom_5m` still inside 16 ms, baseline re-recorded in the same commit.

## Enforcement

- `scripts/ci-gate-model.sh` — fails on a floating-point member in a stored record, a `virtual`/`std::function`/raw pointer in an entity or style record, an `EntityId` in a journal or selection signature, a `width_px` field, and a cull-block column outside the R6 list.
- `/tests/unit` — key monotonicity and non-reuse, slot/key translation at the bus boundary only, ring/hole area correctness, style resolution order, setting scope enforcement.
- `/tests/golden` — ring ordering, content hash stability, unknown-kind round trip.
- `/tests/bench` — the R8 cull-block budget.
