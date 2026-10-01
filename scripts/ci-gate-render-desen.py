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
import json
from collections import deque
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

# THE SAME TWO DEFECTS, MEASURED ON THE GPU. Wider, and deliberately so: the
# bands above are a QGIS-shaped refinement of a much simpler claim, and reusing
# them on another engine would fail a correct picture for drawing the same
# gösterim at a slightly different size. What this gate actually asserts is that
# the pattern did NOT wash its face (ink ~1.00) and that the ground WAS painted
# (green ~0.00), and both of those stay an order of magnitude away here.
#
# Not calibrated to whatever the binary happens to produce — the module docstring
# warns about exactly that. The bounds are the failure modes, loosened only
# enough that engine geometry cannot reach them.
MUREKKEP_RHI = (0.02, 0.45)
ZEMIN_RHI    = (0.10, 0.70)


def yapi_secenegi(exe, ad):
    """Reads one PIRICAD_WITH_<AD> out of the CMakeCache that produced `exe`.

    Walk up to the build tree rather than counting directories: the executable
    sits at <build>/bin/piricad on Linux and Windows but three levels deeper
    inside <build>/bin/piricad.app on macOS.
    """
    kok = os.path.dirname(os.path.abspath(exe))
    while True:
        onbellek = os.path.join(kok, "CMakeCache.txt")
        if os.path.isfile(onbellek):
            with open(onbellek, encoding="utf-8") as f:
                for satir in f:
                    if satir.startswith(f"PIRICAD_WITH_{ad}:"):
                        return satir.strip().rsplit("=", 1)[-1] == "ON"
            return None
        ust = os.path.dirname(kok)
        if ust == kok:
            return None
        kok = ust


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
                     os.path.join(kok_ad, "PiriCAD.app", "Contents", "MacOS", "PiriCAD")):
            mutlak = os.path.join(KOK, aday)
            if os.path.isfile(mutlak) and os.access(mutlak, os.X_OK):
                exe = mutlak
                break
        if exe:
            break
    return exe


def kare(exe, yol, gpu=False, betik=BETIK):
    """Draws the gösterim once and writes the window to `yol`.

    OFFSCREEN IS NOT AN OPTION ON THE GPU PATH. Qt's offscreen platform has no
    GL context, so a `QRhiWidget` composites as a black rectangle and every ratio
    below reads zero — which is indistinguishable from "the backend drew nothing"
    and would fail an innocent build. On the GPU the ambient platform is used
    when the environment has a display, and the caller reports PENDING when it
    does not.
    """
    ortam = dict(os.environ)
    ortam.update({"PIRICAD_DATA": os.path.join(KOK, "data"),
                  "PIRICAD_FRAME_DUMP": yol})
    if gpu:
        ortam.pop("QT_QPA_PLATFORM", None)
    else:
        ortam["QT_QPA_PLATFORM"] = "offscreen"
    subprocess.run([exe, "--betik", betik], cwd=KOK, env=ortam,
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


def ortusme(exe, tmp):
    """Separate same-style faces must add coverage, while real holes stay empty.

    Commands build the scene; a cyan frame locates its coordinates in the real
    window. Test the stencil for solids, line hatches and stamped markers. The
    concave fan has negative triangles; B is clockwise, A counter-clockwise,
    a nested face overlaps A, and a small face covers part of A's actual hole.
    """
    for tip in ("dolgu", "cizgi-desen-dolgu", "nokta-desen-dolgu"):
        commands = [
            {"cmd": "KATMAN", "args": {"ad": "ORTUSME"}},
            {"cmd": "STİL", "args": {"katman": "ORTUSME", "tip": tip,
             "renk": 0xffcc3377, "dolgu": 0xffcc3377, "kalinlik": 500,
             "aralik": 12, "aralik_birim": "piksel", "boyut": 6,
             "boyut_birim": "piksel", "sekil": "kare", "aci": 0}},
            {"cmd": "ALAN", "args": {"noktalar": [
             [60000, 45000], [40000, 45000], [40000, 60000], [0, 60000],
             [0, 0], [60000, 0], [10000, 10000], [25000, 10000],
             [25000, 25000], [10000, 25000]], "bolum": [6, 4]}},
            {"cmd": "ALAN", "args": {"noktalar": [
             [30000, 20000], [30000, 40000], [90000, 40000], [90000, 20000]]}},
            {"cmd": "ALAN", "args": {"noktalar": [
             [17000, 13000], [23000, 13000], [23000, 22000], [17000, 22000]]}},
            {"cmd": "ALAN", "args": {"noktalar": [
             [5000, 35000], [15000, 35000], [15000, 45000], [5000, 45000]]}},
            {"cmd": "KATMAN", "args": {"ad": "OLCU"}},
            {"cmd": "STİL", "args": {"katman": "OLCU", "tip": "cizgi",
             "renk": 0xff19d6d5, "kalinlik": 500}},
            {"cmd": "ÇOKLUÇİZGİ", "args": {"noktalar": [
             [0, 0], [90000, 0], [90000, 80000], [0, 80000], [0, 0]]}},
        ]
        betik = os.path.join(tmp, f"ortusme-{tip}.json")
        yol = os.path.join(tmp, f"ortusme-{tip}.png")
        with open(betik, "w", encoding="utf-8") as f:
            json.dump(commands, f, ensure_ascii=False)
        if not kare(exe, yol, gpu=True, betik=betik):
            print(f"render-desen: örtüşme karesi üretilemedi ({tip})", file=sys.stderr)
            return 1
        a = np.asarray(Image.open(yol).convert("RGB"), dtype=int)
        cyan = (a[:, :, 1] > 150) & (a[:, :, 2] > 150) & (a[:, :, 0] < 80)
        # The largest connected ink component is the frame, independent of UI
        # icons, window size, sidebar widths, antialiasing or display density.
        points = set(zip(*np.where(cyan)))
        largest = []
        while points:
            start = points.pop()
            queue = deque([start])
            component = [start]
            while queue:
                y, x = queue.popleft()
                for p in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if p in points:
                        points.remove(p)
                        queue.append(p)
                        component.append(p)
            if len(component) > len(largest):
                largest = component
        if len(largest) < 500:
            print(f"render-desen: örtüşme ölçü çerçevesi yok ({tip})", file=sys.stderr)
            return 1
        ys, xs = zip(*largest)
        left, right, top, bottom = min(xs), max(xs), min(ys), max(ys)
        ink = (a[:, :, 0] > a[:, :, 1] + 40) & (a[:, :, 2] > a[:, :, 1] + 20)

        def ratio(x, y, side):
            x0 = round(left + (x - side / 2) / 90 * (right - left))
            x1 = round(left + (x + side / 2) / 90 * (right - left))
            y0 = round(bottom - (y + side / 2) / 80 * (bottom - top))
            y1 = round(bottom - (y - side / 2) / 80 * (bottom - top))
            return float(ink[y0:y1, x0:x1].mean())

        checks = (("tek alan", 20, 35, 6, True),
                  ("kesişim", 45, 30, 8, True),
                  ("ikinci alan", 75, 30, 8, True),
                  ("iç içe alan", 10, 40, 6, True),
                  ("gerçek delik", 12.5, 17, 3, False),
                  ("deliği örten alan", 20, 17, 3, True),
                  ("içbükey boşluk", 50, 52, 6, False),
                  ("dış bölge", 75, 10, 8, False))
        for name, x, y, side, filled in checks:
            measured = ratio(x, y, side)
            if (filled and measured < 0.02) or (not filled and measured > 0.005):
                print(f"render-desen: BAŞARISIZ — {tip} / {name}: "
                      f"mürekkep %{measured * 100:.1f}", file=sys.stderr)
                return 1
        print(f"render-desen: OK — {tip}: kesişim, iç içe alan, gerçek delik, "
              "deliği örten alan ve içbükey sınır")
    return 0


def main():
    exe = bul()
    if exe is None:
        print("render-desen: piricad çalıştırılabiliri bulunamadı — ATLANDI "
              "(uygulama derlenmemiş; `make build` sonrası tekrar çalışır)")
        return 0

    # THE GPU PATH IS MEASURED NOW. Both reasons this used to report PENDING are
    # gone: the QRhi backend draws all eleven symbol layer types including the
    # pattern fills, and `MapCanvas::grabCanvas()` reads the frame back off the
    # GPU so `PIRICAD_FRAME_DUMP` composites a real picture. What it still cannot
    # do is make a GL context out of nothing, so a headless machine is PENDING —
    # reported as such, never as passing (`data.md` Enforcement).
    if yapi_secenegi(exe, "RHI") is True:
        if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
            print("render-desen: BEKLEMEDE — PIRICAD_WITH_RHI=ON ve ortamda ekran yok. "
                  "QRhiWidget bir GL bağlamı ister; offscreen platformu siyah bir "
                  "dikdörtgen verir ve o 'çizmedi' ile ayırt edilemez. Ölçüm "
                  "yapılmadı; geçmiş sayılmaz.")
            return 0

        with tempfile.TemporaryDirectory() as tmp:
            yol = os.path.join(tmp, "desen.png")
            if not kare(exe, yol, gpu=True):
                print("render-desen: kare üretilemedi (GPU)", file=sys.stderr)
                return 1

            murekkep, zemin = olc(yol)
            if murekkep is None:
                print("render-desen: GPU karesinde ne desen ne zemin var — "
                      "gösterim çizilmemiş", file=sys.stderr)
                return 1

            kusur = 0
            if not MUREKKEP_RHI[0] <= murekkep <= MUREKKEP_RHI[1]:
                print(f"render-desen: GPU desen mürekkebi %{murekkep*100:.1f} — "
                      f"beklenen %{MUREKKEP_RHI[0]*100:.0f}..%{MUREKKEP_RHI[1]*100:.0f}. "
                      f"Yüksekse desen kendi alanını glif rengiyle boyuyor demektir.",
                      file=sys.stderr)
                kusur = 1
            if not ZEMIN_RHI[0] <= zemin <= ZEMIN_RHI[1]:
                print(f"render-desen: GPU alan zemini %{zemin*100:.1f} — "
                      f"beklenen %{ZEMIN_RHI[0]*100:.0f}..%{ZEMIN_RHI[1]*100:.0f}. "
                      f"Düşükse `dolgu` katmanı yüzünü `dolgu_renk` ile boyamıyor "
                      f"demektir.", file=sys.stderr)
                kusur = 1
            if kusur:
                return 1

            print(f"render-desen: OK (QRhi) — desen %{murekkep*100:.1f}, "
                  f"zemin %{zemin*100:.1f}; ORMAN ALANI yeşil zemin üstünde üçgen "
                  f"ağaçlarla çiziliyor")
            return ortusme(exe, tmp)

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
