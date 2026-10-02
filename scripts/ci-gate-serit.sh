#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the ribbon keeps its grammar, its width and its look — on every platform.
#
# WHAT IT GUARDS (`.claude/ui.md` R46, R48, R55). The ribbon was reorganised
# because it had grown into a mixture: some tools large and some small with no
# rule behind either, a bare picture here and a two-row button there, editor tabs
# carrying commands that had nothing to do with the object picked. The grammar is
# two sizes and one order — a panel's leads large at its left, everything else a
# labelled row — and `PIRICAD_RIBBON_SHEET` measures it instead of trusting an
# eye: no icon-only button, no row two rows tall, every tab inside the 1440 px
# window it is laid out for, a picture for every button.
#
# TWO MORE THINGS THE MAC NEVER SHOWED, both found on a Linux/KDE session by the
# person who runs this program there:
#
#   * THE FACE. Plasma names Noto Sans for QToolButton, QMenu and QLabel before
#     any sheet is read, so the ribbon's buttons came out in it — 5% wider than the
#     design's IBM Plex, and five tabs no longer fit. The application sheet now
#     declares the family. The probe builds a widget of each of those classes and
#     asks which face it got.
#   * THE PRINTER'S ARROW. Fusion paints the arrow half of a split button in a
#     button group from a transparent `Button` role, which Qt turns into opaque
#     black. The probe crops the arrow out of the bar's own picture, in the light
#     theme, where black cannot hide.
#
# TWO RUNS. Offscreen, always: it needs no display, honours the window size to the
# pixel and has no desktop theme, so it is the same on every machine. And on the
# real platform when there is a display — the run that sees what the desktop does
# to the program, which the offscreen one cannot.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "serit: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cd "$kok"
fail=0

# run <ad> <ortam değişkeni>...  — one pass of the probe, its findings or its OK.
run() {
    local ad="$1"; shift
    local cikti rc=0
    set +e
    cikti="$(env "$@" PIRICAD_DATA="$kok/data" PIRICAD_RIBBON_SHEET="$tmp/$ad" \
                 timeout 300 "$exe" 2>/dev/null)"
    rc=$?
    set -e

    if [[ $rc -ge 124 ]]; then
        echo "serit[$ad]: şerit sondası bitmedi ya da öldü (çıkış kodu $rc)" >&2
        fail=1
        return
    fi

    if ! grep -qE '^\[şerit\] [0-9]+ düğme, [0-9]+ bulgu' <<<"$cikti"; then
        echo "serit[$ad]: sonda özet satırını yazmadı; çıktının sonu:" >&2
        tail -5 <<<"$cikti" >&2
        fail=1
        return
    fi

    if ! grep -qE '^\[şerit\] [0-9]+ düğme, 0 bulgu' <<<"$cikti"; then
        echo "serit[$ad]: şeridin bulguları var:" >&2
        grep -E '^\[şerit\] (pencereye sığmıyor|boy kuralı dışında|yazdır okunun yanı siyah|yazı tipi|resimsiz|genel işaret)' \
            <<<"$cikti" >&2 || true
        grep -E '^\[şerit\] [0-9]+ düğme' <<<"$cikti" >&2
        fail=1
        return
    fi

    echo "serit[$ad]: $(grep -E '^\[şerit\] [0-9]+ düğme' <<<"$cikti" | sed 's/^\[şerit\] //')"
}

run cikti-ekransiz QT_QPA_PLATFORM=offscreen

if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
    # The platform Qt picks for this session, with the desktop's own theme on it.
    run gercek-ekran
else
    echo "serit: gerçek ekran koşusu BEKLEMEDE — ortamda ekran yok. Masaüstünün"
    echo "serit:   programa yaptığını yalnız bir masaüstü gösterebilir."
fi

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "serit: OK — iki boy ve tek sıra, her sekme 1440 pikselde, yazdır okunun yanı"
echo "serit:   siyah değil, düğmeler çizim yazı tipinde"
