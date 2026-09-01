# Roadmap — where the GPU canvas work stands

> This is a HAND-OFF, not a rulebook. It records what is finished, what is
> measured, what is written but unproven, and what is left — so the work can be
> picked up on another machine without re-deriving any of it.
>
> English, like everything else under `.claude/` (CLAUDE.md 11.9). It is written
> for whoever continues the work, not for a user of the program.
>
> Last updated at commit `98235c4`. When an item here is finished, delete the
> line — a roadmap nobody prunes stops being read.

## Where things stand

Article 8.1's QRhi canvas is real and complete as a PICTURE. Both optional
halves are still OFF by default.

| | State |
|---|---|
| Vector + raster symbology | **All eleven** symbol layer types of `/data/catalogs/mpyy-vektor` draw on the GPU |
| Text | SDF atlas (msdfgen over FreeType outlines, shaped with HarfBuzz) — rulers, scale bar, north arrow, captions |
| Frame budget (§10.1) | **Measured, and the GPU wins by 20×** — see below |
| Defaults | `PIRICAD_WITH_RHI=OFF`, `PIRICAD_WITH_TEXT=OFF` |

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
PIRICAD_FRAME_TIMES=20 QT_QPA_PLATFORM=xcb ./build/dev/bin/piricad --betik <yük.json>
```

`PIRICAD_BACKEND=dahili` selects the built-in painter on a non-RHI build.

## What this machine needed

None of these are in the repository; a fresh machine needs them before
`PIRICAD_WITH_RHI` or `PIRICAD_WITH_TEXT` will configure.

```bash
sudo apt install qt6-base-private-dev      # <rhi/qrhi.h> — Qt Gui's PRIVATE headers
sudo apt install qt6-shadertools-dev       # qsb, for baking the shader packs
sudo apt install libfreetype-dev libharfbuzz-dev
```

Lua, sol2, msdfgen and stb_rect_pack are fetched from pinned commits, so the
first configure with `PIRICAD_WITH_LUA=ON` or `PIRICAD_WITH_TEXT=ON` needs the
network. `qsb` is NOT on `PATH` on any platform — `scripts/doctor.sh` asks Qt
where its own tools live.

```bash
cmake --preset dev -DPIRICAD_WITH_RHI=ON -DPIRICAD_WITH_TEXT=ON -DPIRICAD_WITH_LUA=ON
cmake --build --preset dev
```

## Written but NOT COMPILED

Committed for the hand-off, unverified. Compile these first.

- `src/app/src/style_designer.cpp` / `.hpp` — the unit segmented control:
  a `QButtonGroup` so exactly one unit is lit, the click converting all five of
  the layer's measures rather than three, and the mixed state shown deliberately.
- `src/app/src/map_canvas.cpp` — `timeFrames()` renders through `grabCanvas()`
  instead of `repaint()`. Repainting a widget the window system has not exposed
  does nothing, so the harness was reporting `0 us` for backends that had not
  drawn.

## Open, in the order they are worth doing

1. **Default both options ON.** The measurement supports it and Article 8.1's
   removal condition names it. Keep the QPainter backend reachable while the
   port settles; deleting it is the end of Phase 1.
2. **Style designer, two visible faults.** The properties panel ends on a
   half-drawn row at the bottom of the window (it is inside a `QScrollArea`, so
   the fix is to stop the page being squashed rather than to add scrolling), and
   a very long published name still elides at the second line — the tooltip
   carries the full name, the cell does not.
3. **Interactive drawing is unproven.** Every drawing check so far went through
   a script. Driving a click with XTEST is unreliable while the screen is locked;
   this needs an unlocked session or a nested compositor.
4. **`make check` is red at clang-tidy, and was before this work.** 19 findings
   over the tree; 17 are in files this work never touched — `painter_backend`,
   `database`, `postgis`, `theme`, `project_writer`, `image_store`,
   `style_library`, `drawlist`, `attribute` — plus two third-party headers.
   Until they are cleared, `make check`'s exit code cannot be trusted as a gate.
   **Read its exit status directly**: piping it through `tail` reports `tail`'s
   status, which is how three green reports were given for a red run.
5. **`ci-gate-render-desen.py` reports PENDING on a GPU build.** Its ratios are
   calibrated against the QGIS picture. Now that `MapCanvas::grabCanvas()` can
   read a GPU frame back, the gate can be taught to measure the QRhi path too.
6. **Renderer work Article 8.1 still owes**: precomputed LOD (`render.md` R4),
   a persistent mapped ring buffer with fences (R6), the < 100 draw-call budget
   asserted in `/tests/bench` (R7), label placement on its own thread (R9), the
   render thread (R10). None of these is needed for the picture; all of them are
   needed for the 5M-polygon scene the budget is written against.

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
- **`PIRICAD_RHI_DEBUG` and `PIRICAD_RHI_ONLY`** bisect a frame. A batch that
  never reached the buffer, a batch drawn off screen, and a pipeline that
  corrupts the state of the draws after it look identical in a screenshot.

## House rules learned on this machine

- The machine is shared. Ask before building, and never start a full
  `make check` or a `FetchContent` download without being told to.
- Screenshots come from the real binary: `PIRICAD_FRAME_DUMP=<png>` for one
  frame, `PIRICAD_SHOT_DIR=<dir>` for every window.
