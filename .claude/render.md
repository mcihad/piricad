# Render Engine — Rules

> Scope: `/src/render` (target `piricad_render`) and the canvas half of `/src/app` (`MapCanvas` + backend factory) | Depends on: `piricad_core`, Qt 6 Gui/RHI | Source: piricad.md §6.1, §6.3, §9.4, §10.1, §10.3, §10.5

## Phase-0 Deviation

The `QPainter`-backed `MapCanvas` stand-in, its reason and its removal condition are CLAUDE.md 8.1. Every rule below is written against the `QRhiWidget` target and binds the stand-in (CLAUDE.md 8.4).

## Hard Rules

R1. All drawing MUST go through `render::Backend`, INCLUDING the overlay — grid, selection, rubber band, snap glyph, crosshair, developer HUD. The canvas widget decides WHAT is on screen and fills a `render::Overlay` in widget pixels; the backend decides how any of it is drawn. Everything else in this rule follows from that: Only the backend factory may name a backend implementation type; the `MapCanvas` widget's own header and source may name nothing beyond its `QRhiWidget` base class. Verify: `grep -rE 'QPainter|QRhiCommandBuffer|QSGNode' /src/app` returns hits only in the factory file.
R2. **Origin offset (§10.3).** Subtract the view-centre origin from every `Point2`/`Mm` value in `double`, and only then narrow to `float`. Every float vertex attribute reaching the GPU SHALL be origin-relative.
R3. The render frame state MUST carry its origin explicitly, and MUST re-anchor when the view centre drifts more than 1 km from it; all live GPU buffers are rebuilt or re-offset on re-anchor.
R4. LOD MUST be precomputed: 4-5 Douglas-Peucker levels baked into quadtree tiles at load time (§10.3). Level selection per frame is a lookup, never a simplification pass.
R5. Line widening MUST happen on the GPU: instanced quads with screen-space expansion in the vertex shader. Width, dash pattern and cap style SHALL be shader-side (instance attributes / uniforms), never CPU-emitted outline geometry.
R6. Dynamic vertex/instance data MUST use a persistent mapped ring buffer with fences (§10.3); no per-frame buffer creation.
R7. Draw calls MUST be batched into shared atlases; the 5M-polygon cadastral bench scene of R13 SHALL render in **< 100 draw calls/frame**, asserted in `/tests/bench`, and the frame-stats struct MUST expose the live draw-call count.
R8. Labels and MPYY/BÖHHBÜY symbols MUST render from an SDF atlas built with msdfgen (single texture, scale independent); Turkish text is shaped with HarfBuzz + FreeType (§9.4).
R9. Label placement MUST run on its own thread and publish its result by atomic pointer swap; the render thread never blocks on placement (§10.3).
R10. A dedicated render thread MUST own geometry preparation and GPU submission. The UI thread does event handling and widget painting only.
R11. Frustum culling MUST run against the quadtree **before** tile acquisition — an invisible tile is never opened, decoded or uploaded.
R12. Raster layers MUST be COG with overview pyramids, loaded asynchronously, and cached in an LRU whose hard byte cap is one named constant in `/src/render`; the frame-stats struct MUST expose current cache bytes.
R13. Pan/zoom on a 5M-polygon cadastral layer MUST stay **≤ 16 ms/frame** (§10.1). This is a CI gate, not a target.
R14. Every function in the frame path under `/src/render/src` MUST open a named Tracy zone (`ZoneScopedN`), checked by `scripts/ci-gate-render.sh` (§10.5).
R15. Render MUST read the `Document` through a read-only snapshot and consume its SoA polyline store directly; state changes only ever come back as commands (`core.zoom` and friends) — see `.claude/command.md`.
R16. The GPU canvas MUST be a `QRhiWidget` embedded in a Qt Widgets shell (§6.3). QML is allowed only for selected side panels via `QQuickWidget` — see `.claude/ui.md` R3.
R17. The Dear ImGui overlay MUST be compiled behind `PIRICAD_WITH_IMGUI`, defaulting OFF and hard-forced OFF for `CMAKE_BUILD_TYPE=Release` (§6.1, §9.4).
R18. `piricad_render` MUST build with `-fno-fast-math -ffp-contract=off`, exactly like the rest of the tree.
R19. Every pixel-space constant (line width, hit radius, label padding, atlas margin) MUST be scaled by `devicePixelRatio`.
R20. The draw loop MUST be allocation-free: all buffers are sized at resize/tile-load time; transient per-frame data comes from a frame arena that is reset, never grown.

## Absolute Prohibitions

P1. NEVER write world coordinates (TUREF/TM30, 7-digit TM3) directly into a `float` vertex attribute. This is the metre-scale jitter bug and it is a build-breaking defect.
P2. NEVER perform geometry work or disk I/O on the UI thread — no tessellation, no simplification, no tile decode, no file read (§10.3).
P3. NEVER let `QPainter` reach the production render path outside the Phase-0 stand-in backend; `scripts/ci-gate-render.sh` fails on `QPainter` in `/src/render` or `/src/app` once CLAUDE.md 8.1 is discharged.
P4. NEVER expose the ImGui debug layer in a release build, behind a preference, or through any user-facing menu. Developer overlay only, inside the canvas.
P5. NEVER use Qt's scene graph or Qt painting inside the canvas — no `QGraphicsScene`, no `QSGNode`, no `QPainter` calls in the QRhi path (§6.3: "Qt's drawing system does not interfere here").
P6. NEVER allocate in the draw loop: no `new`/`malloc`, no `std::vector` growth, no `QString`/`QByteArray` construction per frame.
P7. NEVER mutate the `Document` (or any core type) from render code. Render is a consumer — see `.claude/core.md`.
P8. NEVER build the application shell in QML. Widgets shell, QML panels only (§6.3).
P9. NEVER include Qt Widgets headers inside `/src/render`; the dependency direction is `render -> core, Qt Gui` and nothing more.
P10. NEVER upload or draw an unculled full layer — every upload path SHALL be culled and LOD-selected first.
P11. NEVER compile or patch shader source at runtime. Shaders are baked to `.qsb` at build time; a missing `qsb` fails the QRhi backend configure step with an actionable message.
P12. NEVER call `std::toupper`/`std::tolower` on label text (Turkish i/I). Use `QLocale(QLocale::Turkish)` — render is a Qt layer (CLAUDE.md 5.6).

## Definitions of Done

- [ ] New drawing capability is reachable only through `render::Backend`; no caller was edited to name a backend.
- [ ] Every new float vertex path is origin-relative and covered by the jitter test.
- [ ] New stage has a Tracy zone and appears in the frame-stats breakdown.
- [ ] Draw-call count and frame time on the 5M-polygon bench are unchanged or better.
- [ ] No new allocation in the draw loop (checked under a heap profiler or ASan allocation counter).
- [ ] Debug-only code is inside `PIRICAD_WITH_IMGUI` or an equivalent OFF-by-default guard.

## Enforcement

- `/tests/bench` frame-time gate: 5M-polygon cadastral pan/zoom, ≤ 16 ms/frame; **> 10% regression breaks the build** (§10.1).
- `/tests/unit` jitter regression test: 30th-zone TM3 (`TUREF/TM30`) coordinates, deep zoom, asserts sub-pixel screen error — fails if any world coordinate reaches a `float` unoffset (§11 Phase 0, §10.3).
- `scripts/ci-gate-layering.sh` — greps `/src/render` for Qt Widgets includes and `/src/app` for backend implementation type names (R1, P9).
- `scripts/ci-gate-render.sh` — fails on a frame-path file under `/src/render/src` with no `ZoneScoped*` (R14) and on `QPainter` outside the stand-in backend (P3).
- Release packaging check: `PIRICAD_WITH_IMGUI=OFF` asserted in the packaged build config (P4).
- clang-tidy + ASan/TSan CI jobs cover P6 and the render-thread rules.
