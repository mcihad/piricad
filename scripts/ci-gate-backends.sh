#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every canvas backend draws the OVERLAY.
#
# A backend draws the document. The grid, the ruler, the scale bar, the north
# arrow, the snap marker, the crosshair, the selection box and the drawing's own
# captions are not the document — they are the program's own furniture, and every
# backend owes the user all of them.
#
# WHY THIS IS A GATE AND NOT A UNIT TEST. `/tests` links no Qt, and a backend is
# Qt by definition (Article 3.4 keeps `piricad_render` Qt-free, so both backends
# live in `/src/app`). There is nowhere in the test binary to construct one. What
# CAN be checked without Qt is that each backend's `render` reaches the one shared
# function that paints the aids — which is the thing that was missing.
#
# THE DEFECT THIS EXISTS FOR: the QGIS backend read `overlay.background_rgba` and
# nothing else. The moment it became the default engine the grid, the ruler, the
# scale bar, the north arrow and the snap marker all vanished from the canvas at
# once; the drawing was still there and everything around it was gone. Nothing
# failed, because nothing was watching.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/src/app/src"
fail=0

# Every file that implements render::Backend, found rather than listed: a backend
# added and forgotten here would be a backend nobody checks.
mapfile -t backends < <(grep -ln "public render::Backend" "$app"/*.cpp 2>/dev/null || true)

if [[ ${#backends[@]} -eq 0 ]]; then
    echo "backends: skipped — no render::Backend implementation found under /src/app"
    exit 0
fi

for file in "${backends[@]}"; do
    name="$(basename "$file")"

    # BOTH halves. The overlay is drawn in two places — the grid under the
    # document, everything else over it — and a backend that calls only the
    # second draws the grid on top of the map. A backend that calls only the
    # first loses the ruler, the selection, the snap marker and the crosshair.
    if ! grep -q "paint_frame_ground\|paint_ground" "$file"; then
        echo "backends: $(basename "$file") never draws the overlay's GROUND -> $file:1" >&2
        echo "backends:   the grid goes UNDER the document. Call paint_frame_ground()" >&2
        echo "backends:   before the passes, or the grid covers the drawing." >&2
        fail=1
    fi

    if ! grep -q "paint_frame_aids\|paint_aids" "$file"; then
        echo "backends: $name implements render::Backend but never paints the overlay -> ${file#$root/}:1" >&2
        echo "backends:   the grid, ruler, scale bar, north arrow, snap marker and crosshair" >&2
        echo "backends:   are every backend's job. Call paint_frame_aids() at the end of render()." >&2
        fail=1
    fi
done

# ---- what a backend claims it can draw ---------------------------------------
#
# A backend that cannot draw every layer type must say which ones it CAN, and say
# it as a whitelist. The QGIS backend first listed what it could NOT draw and let
# everything else fall through to "yes"; every type nobody had taught it was then
# silently claimed and silently skipped. Three raster types went out that door —
# which is most of what MPYY publishes — so a hatched lekesi came out as a flat
# colour and a published line type as a plain stroke.
#
# A whitelist cannot fail that way: a type nobody has translated reaches the
# default and is refused, which sends the frame to a backend that can draw it.
for file in "${backends[@]}"; do
    name="$(basename "$file")"
    grep -q "handles" "$file" || continue

    # The `default:` of the switch inside handles() has to REFUSE. Read from the
    # last default: in the function, whichever way the cases are ordered.
    if ! awk '/bool .*::handles/,/^\}/' "$file" | grep -qE "default:[[:space:]]*return false"; then
        echo "backends: $name decides what it can draw by exclusion -> ${file#$root/}:1" >&2
        echo "backends:   handles() must list what it CAN draw and refuse the rest," >&2
        echo "backends:   or a layer type nobody taught it is claimed and then skipped." >&2
        fail=1
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "backends: OK — ${#backends[@]} backend(s), each paints the overlay and refuses by default"
fi
exit $fail
