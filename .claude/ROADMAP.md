# Roadmap — where KentOSCad stands

> This is a HAND-OFF, not a rulebook. It records what is finished, what is
> measured, what is written but unproven, and what is left — so the work can be
> picked up on another machine without re-deriving any of it.
>
> English, like everything else under `.claude/` (CLAUDE.md 11.9). It is written
> for whoever continues the work, not for a user of the program.
>
> Last updated after the drawing/editing toolset landed (49 commands). When an
> item here is finished, delete the line — a roadmap nobody prunes stops being read.

## Where things stand

Article 8.1's QRhi canvas is real and complete as a PICTURE. Both optional
halves are still OFF by default.

| | State |
|---|---|
| Vector + raster symbology | **All eleven** symbol layer types of `/data/catalogs/mpyy-vektor` draw on the GPU |
| Text | SDF atlas (msdfgen over FreeType outlines, shaped with HarfBuzz) — rulers, scale bar, north arrow, captions |
| Frame budget (§10.1) | **Measured, and the GPU wins by 20×** — see below |
| Defaults | **Both ON wherever the toolchain is found** — probed in `cmake/KentOSCadOptions.cmake` |

### The measurement

40 000 parcels (a 200 × 200 grid of 20 m rings, 200 000 vertices), same scene,
same machine, median of 20 frames, backend share only:

| Backend | Draw |
|---|---|
| QRhi (GPU) | **1.10 ms** |
| Built-in QPainter | 22.41 ms |
| QGIS | 72.63 ms |

The §10.1 budget is 16 ms. Reproduce with:

```bash
KENTOS_FRAME_TIMES=20 QT_QPA_PLATFORM=xcb ./build/dev/bin/kentos_cad --betik <yük.json>
```

`KENTOS_BACKEND=dahili` selects the built-in painter on a non-RHI build.

## What this machine needed

None of these are in the repository; a fresh machine needs them before
`KENTOS_WITH_RHI` or `KENTOS_WITH_TEXT` will configure.

```bash
sudo apt install qt6-base-private-dev      # <rhi/qrhi.h> — Qt Gui's PRIVATE headers
sudo apt install qt6-shadertools-dev       # qsb, for baking the shader packs
sudo apt install libfreetype-dev libharfbuzz-dev
```

Lua, sol2, msdfgen and stb_rect_pack are fetched from pinned commits, so the
first configure with `KENTOS_WITH_LUA=ON` or `KENTOS_WITH_TEXT=ON` needs the
network. `qsb` is NOT on `PATH` on any platform — `scripts/doctor.sh` asks Qt
where its own tools live.

```bash
cmake --preset dev -DKENTOS_WITH_RHI=ON -DKENTOS_WITH_TEXT=ON -DKENTOS_WITH_LUA=ON
cmake --build --preset dev
```

## The entity model is no longer polyline-only

`entities.kind` was a column of zeros that nothing read; the kind system was
declared, unit-tested and wired to nothing. It is live now, and four kinds ship:

| Kind | id | Stored as |
|---|---|---|
| `core.polyline` | 1 | its rings, as before |
| `core.circle` | 2 | centre + a radius handle due east — the radius is exact |
| `core.arc` | 3 | centre + radius handle + the two measured ends, swept CCW |
| `core.point` | 4 | one vertex |

`core::curve_outline` is the ONE place that answers "what shape is this really".
The renderer, the pick test, the box select and the canvas all ask it, so the next
curve kind becomes visible, selectable and snappable by being registered. That is
not a claim — the arc was added after the circle and needed no edit to any of them.

Three things this cost, which are worth not re-deriving:

- **`KindSpec::emit` was named `emit`, which is a Qt macro.** Including
  `entity_kind.hpp` from a Qt translation unit failed with `expected
  unqualified-id` on a line that looked perfectly good. The member is `outline`
  now, and `KindId` lives in `identity.hpp` so a caller who only asks "is this a
  circle?" needs neither header.
- **The project reader refused any file whose kind column was not 0.** Filling the
  column correctly would have made every saved project unloadable. It accepts 0
  (legacy = polyline) and every declared kind now.
- **A circle and a two-point line are the same two vertices.** Only the kind column
  separates them; a reader that guessed from the geometry turned every saved circle
  into a short line pointing east. There is a round-trip test for exactly that.

Curves never call `std::cos`/`std::sin`: libm is not required to agree between
platforms and §7.3 promises the drawing does. Tessellation bisects from the four
exact axis points using only `+`, `*` and `sqrt`, and `core::sin_cos_udeg` reduces
an integer micro-degree angle by quadrant before a Taylor series — so a right angle
is exactly a right angle and four 90° rotations return a parcel bit-for-bit.

## What the drawing and editing toolset now covers

Draw: `ÇİZGİ` `ÇOKLUÇİZGİ` `ALAN` `DİKDÖRTGEN` `DAİRE` `YAY` `NOKTA` `METİN`
Modify: `TAŞI` `KOPYALA` `DÖNDÜR` `ÖLÇEKLE` `AYNALA` `DİZİ` `BÖL` `BUDA` `UZAT`
`PAH` `YUVARLA` `KÖŞETAŞI` `KÖŞEEKLE` `ALANAÇEVİR` `KATMANAT` `STİLKOPYALA` `SİL`
Query: `ÖLÇ` `ALANÖLÇ` `SEÇ`

Every one has its Turkish page under `/docs`, a regression test, and works from the
command line, a script and the GUI alike. `Del` deletes the selection and
`core.duzenleme.silme_onayi` asks first when it is on.

Two behaviours worth knowing before changing them:

- **A fillet BREAKS the line in two** and puts a `core.arc` between the pieces.
  Leaving both tangent points in one run draws a chord AND the arc — a lens where a
  rounded corner should be. For the same reason `YUVARLA` refuses a CLOSED ring:
  the result is a boundary partly made of a curve, which this ring cannot hold.
- **A mirror reverses winding**, so ring vertex order is reversed to keep the area
  positive, and an arc's two ends are swapped to keep the sweep counter-clockwise.

## Open, in the order they are worth doing

> Pruned as items land. DONE and deleted from this list: `ÖTELE`/offset (Clipper2
> is linked and `OFSET`, `BİRLEŞTİR` and `BÖL` all run on it), the GPU backend's
> lone-vertex point marker, defaulting `KENTOS_WITH_RHI` and `KENTOS_WITH_TEXT`
> ON, every published point gösterim previewing (the last one was a WORD, and the
> preview drew no captions), the style designer's two visible faults, and
> `ci-gate-render-desen.py`, which now MEASURES the GPU path instead of
> reporting PENDING against two reasons that had both stopped being true. And
> `make check` is GREEN: every gate, the format check, and clang-tidy — whose
> harness was itself a defect (it analysed files the build never compiled, with
> no flags, and reported the parse failures as findings). R7's draw-call budget
> is asserted too, by `scripts/ci-gate-butce.sh`.

1. **The probes are the only thing that catches interaction defects.** Three now:
   `KENTOS_EDIT_PROBE` (grip dragging), `KENTOS_TOOL_PROBE` (every column button,
   two passes — select-then-press and press-then-select) and `KENTOS_HAND_PROBE`
   (real mouse and key events, which button is lit at each step, and a PNG of
   every step when given a directory). Every interaction bug in this session was
   found by one of them and none was findable by a unit test: the transcript said
   a command ran while the screen showed nothing. Extend them rather than testing
   the canvas by eye.
2. **Renderer work Article 8.1 still owes — four subsystems, no defects.**
   R7 is DONE: the live draw-call count exists (`render::FrameStats`, counted
   where the draws are submitted), `KENTOS_FRAME_TIMES` prints it beside the
   frame time, and `scripts/ci-gate-butce.sh` ASSERTS both budgets on the
   heaviest scene in the repository —

       [butce] TAMAM — kare ortancasi 4055 us (butce 16000)
                     · cizim cagrisi 82 (butce 100)

   It is a shell gate rather than a bench case because of Article 3.4 and not
   taste: `/tests` links no Qt and every backend is Qt by definition, so the
   bench binary can time the SCENE BUILDER and nothing else. A budget about what
   reaches the screen is measured where the screen is.

   What remains is architecture, and each is a subsystem rather than a fix:
   precomputed LOD baked into quadtree tiles at load time (R4), a persistent
   mapped ring buffer with fences (R6), label placement on its own thread
   publishing by atomic pointer swap (R9), and a dedicated render thread owning
   geometry preparation and submission (R10).

   NONE IS NEEDED FOR THE PICTURE and none is a defect a user can hit today: the
   GPU canvas draws every symbol layer type and meets both budgets on the scenes
   above. All four are needed before the 5M-polygon sheet the budgets are
   actually written against, and R10 in particular is not a local change —
   `QRhiWidget` renders on the GUI thread, so moving submission off it is a
   question about which surface the canvas is, not about where a loop lives.
   Take them one at a time, each with its own bench case in the gate above.

## Things that cost an afternoon each — do not re-derive them

- **`cb->draw(..., firstInstance)` is not portable.** It needs
  `QRhi::BaseInstance`, which OpenGL ES and GL without `ARB_base_instance` do not
  have — and where it is missing the draw is silently WRONG rather than refused.
  Bind the vertex buffer at a byte offset instead.
- **Every shader stage of every pipeline must declare `ubuf` identically.** GLSL
  refuses to link a program whose two stages give one uniform block name two
  member lists, and the picture pipeline pairs `text.vert` with `image.frag`.
- **A pipeline built against a layout-only bindings set, with the real set
  swapped in per draw, did not work here.** Nothing drew and the draws that
  followed came out as a fan of stretched quads. Each picture owns its own
  pipeline now.
- **`QWidget::grab()` cannot see a `QRhiWidget`'s frame** — it walks the backing
  store and the frame is on the GPU. `MapCanvas::grabCanvas()` exists for this.
- **A stale `ui/state` in QSettings survives a rebuild and looks exactly like a
  new bug.** `kLayoutVersion` is the way to decline one.
- **`KENTOS_RHI_DEBUG` and `KENTOS_RHI_ONLY`** bisect a frame. A batch that
  never reached the buffer, a batch drawn off screen, and a pipeline that
  corrupts the state of the draws after it look identical in a screenshot.

## House rules learned on this machine

- The machine is shared. Ask before building, and never start a full
  `make check` or a `FetchContent` download without being told to.
- Screenshots come from the real binary: `KENTOS_FRAME_DUMP=<png>` for one
  frame, `KENTOS_SHOT_DIR=<dir>` for every window.
