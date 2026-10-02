#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a light/dark switch stays fast on a real desktop.
#
# WHAT IT GUARDS (`.claude/ui.md` R55). On a KDE Plasma session one switch took
# 4.4 s — a second per widget class is how it felt — against 0.4 s today. The cause
# was invisible from anywhere else: Qt builds the DESKTOP'S style (Breeze) while the
# application object is constructed, the program swaps in its own Fusion afterwards,
# and the replaced style's hooks stayed in the event path — 65% of the time was spent
# inside `breeze6.so`, called from application-wide event filters, once per widget
# event of a stylesheet re-polish. A Mac, a Windows machine and a bare X session have
# no such style to load, so the offscreen run takes 0.7 s either way and cannot
# see it. Only a run on the real platform can.
#
# THE MEASURE is `PIRICAD_THEME_PROBE`: the Koyu Tema action toggled the way a click
# toggles it — command and all — and timed until the window has nothing left to lay
# out or paint. The probe's own exit code is the verdict (a worst switch over its
# 1500 ms budget fails it).
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "tema-gecisi: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "tema-gecisi: BEKLEMEDE — ortamda ekran yok. Ekransız koşu masaüstünün stilini"
    echo "tema-gecisi:   yüklemez, yani ölçmek istediği şeyi ölçemez."
    exit 0
fi

cd "$kok"

set +e
cikti="$(PIRICAD_DATA="$kok/data" PIRICAD_THEME_PROBE=4 timeout 300 "$exe" 2>&1)"
rc=$?
set -e

if [[ $rc -ge 124 ]]; then
    echo "tema-gecisi: sonda bitmedi ya da öldü (çıkış kodu $rc)" >&2
    exit 1
fi

if ! grep -q '^\[tema\] en yavaş geçiş' <<<"$cikti"; then
    echo "tema-gecisi: sonda sonuç satırını yazmadı; çıktının sonu:" >&2
    tail -5 <<<"$cikti" >&2
    exit 1
fi

if [[ $rc -ne 0 ]]; then
    echo "tema-gecisi: açık/koyu geçiş bütçeyi aşıyor:" >&2
    grep '^\[tema\]' <<<"$cikti" >&2
    echo "tema-gecisi:   Önce 'QT_STYLE_OVERRIDE'ın uygulama nesnesi kurulmadan ayarlandığına" >&2
    echo "tema-gecisi:   bakın (src/app/src/main.cpp): masaüstünün stili kalıyorsa geçiş saniyeler sürer." >&2
    exit 1
fi

echo "tema-gecisi: OK — $(grep '^\[tema\] en yavaş geçiş' <<<"$cikti" | sed 's/^\[tema\] //')"
