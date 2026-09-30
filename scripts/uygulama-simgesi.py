#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The application icon, made from the logo.

`data/images/piricad_logo.png` is the whole mark — the compass emblem over the
`PiriCAD` word. An icon is the EMBLEM alone: at 16 and 32 px the word is a grey
smear, and the ring and the star are what a dock or a task bar shows. The emblem
is navy, which disappears on a dark dock, so it sits on a white plate with the
rounded corners and the size macOS's icon grid gives an app (824 of 1024).

Writes, from that one master:

    packaging/icon/piricad-1024.png   the master; also the window icon at run time
    packaging/macos/PiriCAD.icns    the bundle's icon
    packaging/windows/piricad.ico   16 … 256, for the executable's resource
    packaging/linux/piricad.png     512, for the desktop entry

Run it again only when the logo changes; the outputs are committed. Needs
Python 3 and Pillow. Deterministic: the same logo gives the same bytes.
"""

from __future__ import annotations

import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOGO = ROOT / "data" / "images" / "piricad_logo.png"

CANVAS = 1024
PLATE = 824  # macOS: the app's body inside the 1024 canvas
RADIUS = 185  # the corner radius Apple's template draws at that size
ART = 640  # the emblem's box on the plate
INK = 30  # alpha below this is antialiasing noise, not the mark


def emblem(logo: Image.Image) -> Image.Image:
    """The first band of ink rows — the emblem — cut out of the logo."""
    alpha = logo.getchannel("A").point(lambda v: 255 if v > INK else 0)
    width, height = alpha.size
    rows = [alpha.crop((0, y, width, y + 1)).getbbox() is not None for y in range(height)]
    top = rows.index(True)
    bottom = top
    while bottom < height and rows[bottom]:
        bottom += 1
    band = alpha.crop((0, top, width, bottom))
    left, _, right, _ = band.getbbox()
    return logo.crop((left, top, right, bottom))


def master(art: Image.Image) -> Image.Image:
    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    edge = (CANVAS - PLATE) // 2

    # A SOFT SHADOW under the plate, the way the dock draws every app's.
    shadow = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (edge, edge + 12, edge + PLATE, edge + PLATE + 12), RADIUS, fill=(0, 0, 0, 70)
    )
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(14)))

    plate = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    ImageDraw.Draw(plate).rounded_rectangle(
        (edge, edge, edge + PLATE, edge + PLATE), RADIUS, fill=(255, 255, 255, 255)
    )
    canvas.alpha_composite(plate)

    scale = ART / max(art.size)
    size = (round(art.width * scale), round(art.height * scale))
    fitted = art.resize(size, Image.LANCZOS)
    canvas.alpha_composite(fitted, ((CANVAS - size[0]) // 2, (CANVAS - size[1]) // 2))
    return canvas


def main() -> int:
    if not LOGO.exists():
        print(f"uygulama-simgesi: logo yok -> {LOGO.relative_to(ROOT)}", file=sys.stderr)
        return 1
    art = emblem(Image.open(LOGO).convert("RGBA"))
    icon = master(art)

    out = {
        "master": ROOT / "packaging" / "icon" / "piricad-1024.png",
        "icns": ROOT / "packaging" / "macos" / "PiriCAD.icns",
        "ico": ROOT / "packaging" / "windows" / "piricad.ico",
        "png": ROOT / "packaging" / "linux" / "piricad.png",
    }
    for path in out.values():
        path.parent.mkdir(parents=True, exist_ok=True)

    icon.save(out["master"], optimize=True)
    icon.save(out["icns"])
    icon.save(out["ico"], sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    icon.resize((512, 512), Image.LANCZOS).save(out["png"], optimize=True)
    for name, path in out.items():
        print(f"uygulama-simgesi: {name} -> {path.relative_to(ROOT)} ({path.stat().st_size} bayt)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
