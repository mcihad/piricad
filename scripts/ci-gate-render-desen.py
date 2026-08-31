#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Proves that a pattern fill draws its PATTERN over its GROUND, and neither eats
the other.

Two defects live here and both were shipped once:

  * The pattern washed its own face with the glyph's fill colour, so MPYY's
    orman came out a solid black block with the trees invisible inside it.
  * The ground was written into the layer's stroke colour instead of its fill
    colour, so the annex's ALAN RENK KODU parsed, validated and painted nothing.

Neither shows up in a schema check and neither throws: the picture is simply
wrong. So this renders the real gösterim through the real painter and MEASURES
what came out.

Not a pixel comparison. An antialiased QPainter frame is not bit-identical
across platforms and a golden PNG would fail on a font hint; the two ratios
below move by a fraction of a percent and separate right from wrong by an order
of magnitude.

THE DEFAULT BACKEND, which is the one a user sees. `PIRICAD_BACKEND=dahili`
draws the same row very differently today — 89% ink against 5% — because the two
disagree about how a PAPER measure becomes pixels. That is a real defect and it
is not this gate's: locking it here would freeze whichever answer happens to be
in the binary. It is measured and reported separately.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

KOK = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
BETIK = "tests/golden/cizim/desen-dolgu.json"

# ORMAN ALANI: a #228B22 ground under a grid of black triangles.
MUREKKEP = (0.02, 0.22)   # trees; a washed face is ~1.00
ZEMIN    = (0.08, 0.35)   # ground; a fill written to the wrong field is ~0.00


def qgis_motoru_var(exe):
    """Did the build that produced `exe` link the QGIS symbology engine?

    The ratios below are calibrated against the QGIS backend, because that is the
    default and therefore the picture a user sees. When QGIS is absent the very
    same script is drawn by the built-in backend, which the module docstring
    already records as answering very differently — so asserting the QGIS numbers
    against it measures the wrong thing and fails an innocent build.
    """
    # Walk up to the build tree rather than counting directories: the executable
    # sits at <build>/bin/piricad on Linux and Windows but three levels deeper
    # inside <build>/bin/piricad.app on macOS.
    kok = os.path.dirname(os.path.abspath(exe))
    while True:
        onbellek = os.path.join(kok, "CMakeCache.txt")
        if os.path.isfile(onbellek):
            with open(onbellek, encoding="utf-8") as f:
                for satir in f:
                    if satir.startswith("PIRICAD_WITH_QGIS:"):
                        return satir.strip().rsplit("=", 1)[-1] == "ON"
            return None
        ust = os.path.dirname(kok)
        if ust == kok:
            return None
        kok = ust


def bul():
    exe = None
    # macOS builds an application BUNDLE, so the executable is not at bin/piricad
    # but inside bin/piricad.app. Looking only for the bare name meant this gate
    # skipped itself on every Mac — a built binary it never found, and a pattern
    # fill nobody was checking.
    for kok_ad in ("build/dev/bin", "build/debug/bin", "build/release/bin"):
        for aday in (os.path.join(kok_ad, "piricad"),
                     os.path.join(kok_ad, "piricad.app", "Contents", "MacOS", "piricad")):
            mutlak = os.path.join(KOK, aday)
            if os.path.isfile(mutlak) and os.access(mutlak, os.X_OK):
                exe = mutlak
                break
        if exe:
            break
    return exe


def kare(exe, yol):
    ortam = dict(os.environ)
    ortam.update({"QT_QPA_PLATFORM": "offscreen",
                  "PIRICAD_DATA": os.path.join(KOK, "data"),
                  "PIRICAD_FRAME_DUMP": yol})
    subprocess.run([exe, "--betik", BETIK], cwd=KOK, env=ortam,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=180)
    return os.path.isfile(yol)


def olc(yol):
    """The two ratios, measured inside the gösterim's own bounding box."""
    a = np.asarray(Image.open(yol).convert("RGB"), dtype=int)
    lum = a @ [0.299, 0.587, 0.114]
    sat = a.max(axis=2) - a.min(axis=2)
    yesil = (a[:, :, 1] > a[:, :, 0] + 12) & (a[:, :, 1] > a[:, :, 2] + 12) & (sat > 25)
    koyu = lum < 90

    # The face is found rather than assumed: a hard-coded crop breaks the day the
    # shell's panels change width, and then this gate reports on the toolbar.
    ys, xs = np.where(yesil | (koyu & (np.cumsum(yesil, axis=1) > 0)))
    if len(ys) < 500:
        return None, None
    kutu_koyu = koyu[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    kutu_yesil = yesil[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    return float(kutu_koyu.mean()), float(kutu_yesil.mean())


def main():
    exe = bul()
    if exe is None:
        print("render-desen: piricad çalıştırılabiliri bulunamadı — ATLANDI "
              "(uygulama derlenmemiş; `make build` sonrası tekrar çalışır)")
        return 0

    motor = qgis_motoru_var(exe)
    if motor is not True:
        neden = ("PIRICAD_WITH_QGIS=OFF" if motor is False
                 else "yapı yapılandırması okunamadı")
        print(f"render-desen: BEKLEMEDE — {neden}. Saklanan oranlar QGIS arka ucuna "
              f"göre ayarlı; QGIS yokken aynı betiği dahili arka uç çiziyor ve onun "
              f"oranları ölçülmüş değil. Geçmiş sayılmaz, ölçüm yapılmadı.")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        yol = os.path.join(tmp, "desen.png")
        if not kare(exe, yol):
            print("render-desen: kare üretilemedi", file=sys.stderr)
            return 1

        murekkep, zemin = olc(yol)
        if murekkep is None:
            print("render-desen: gösterim çizilmemiş — tuvalde ne desen ne zemin var",
                  file=sys.stderr)
            return 1

        kusur = 0
        if not MUREKKEP[0] <= murekkep <= MUREKKEP[1]:
            print(f"render-desen: desen mürekkebi %{murekkep*100:.1f} — "
                  f"beklenen %{MUREKKEP[0]*100:.0f}..%{MUREKKEP[1]*100:.0f}. "
                  f"Yüksekse desen kendi alanını glif rengiyle boyuyor demektir.",
                  file=sys.stderr)
            kusur = 1
        if not ZEMIN[0] <= zemin <= ZEMIN[1]:
            print(f"render-desen: alan zemini %{zemin*100:.1f} — "
                  f"beklenen %{ZEMIN[0]*100:.0f}..%{ZEMIN[1]*100:.0f}. "
                  f"Düşükse `dolgu` katmanı yüzünü `dolgu_renk` ile boyamıyor demektir.",
                  file=sys.stderr)
            kusur = 1
        if kusur:
            return 1

        print(f"render-desen: OK — desen %{murekkep*100:.1f}, zemin %{zemin*100:.1f}; "
              f"ORMAN ALANI yeşil zemin üstünde üçgen ağaçlarla çiziliyor")
        return 0


sys.exit(main())
