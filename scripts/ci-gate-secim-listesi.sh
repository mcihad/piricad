#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a click that hits several objects takes one AND LETS THE HAND WALK TO THE
# OTHERS IN PLACE — a framed non-modal list — and the one kept holds.
#
# THE PROBLEM IT GUARDS. A click on a plan sheet lands on a parcel, on the ada
# round it and on a road drawn through, all at zero distance, and the user has
# to be able to say which one. It used to be a modal list; the user found that
# clumsy and chose the walk other programs use (Netcad's Space through nested
# areas, MicroStation's reset, AutoCAD's cycling): the first candidate is taken
# at once. The framed, non-modal list now also offers direct row choice, search
# and Home/End for large candidate sets (1 October 2026 user request). Space and
# Shift+Space still walk on and back, Enter keeps, Esc puts the selection back.
#
# THE ORDER IS `core::pick_all`'s: nearest first, and among what the point is on
# or inside, the SMALLEST first — the road (no area), the parcel, then the ada.
#
# WHY IT IS A SHELL GATE. /tests links no Qt (Article 3.4), so a canvas, a mouse
# event and a key cannot be built there. What IS tested there is the half that is
# Qt-free — `core::pick_all`'s order and `SEÇ mod=NOKTA sira=` — and this gate
# covers the half that only exists on a screen, starting at a real click.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "secim-listesi: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "secim-listesi: BEKLEMEDE — ortamda ekran yok. Bir tıklama ancak bir tuvalde"
    echo "secim-listesi:   olur; tıklanmamış bir tuval hiçbir şeyi kanıtlamaz."
    exit 0
fi

# THREE THINGS UNDER ONE POINT, on three layers, with three different measures —
# an area, a bigger area and a length. `--betik` fits the drawing to the canvas,
# so the middle of the widget is the middle of all three.
gecici="$(mktemp -d)"
trap 'rm -rf "$gecici"' EXIT

cat > "$gecici/sahne.json" <<'JSON'
[
 {"cmd": "KATMAN", "args": {"ad": "PARSEL"}},
 {"cmd": "ALAN", "args": {"noktalar": [[-20000,-20000],[20000,-20000],[20000,20000],[-20000,20000]]}},
 {"cmd": "KATMAN", "args": {"ad": "ADA"}},
 {"cmd": "ALAN", "args": {"noktalar": [[-30000,-30000],[30000,-30000],[30000,30000],[-30000,30000]]}},
 {"cmd": "KATMAN", "args": {"ad": "YOL"}},
 {"cmd": "ÇİZGİ", "args": {"noktalar": [[-30000,0],[30000,0]]}}
]
JSON

cd "$kok"

set +e
cikti="$(PIRICAD_DATA="$kok/data" PIRICAD_PICK_PROBE=1 \
         "$exe" --betik "$gecici/sahne.json" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "secim-listesi: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "secim-listesi: beklenen satır yok -> $1" >&2
        grep '^\[secim\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

# THREE UNDER THE POINT, AND NO WINDOW: the first taken, the badge saying so.
bekle "[secim] aday: 3"
bekle "[secim] pencere: yok"
bekle "[secim] rozet: 1/3 · ÇOKLUÇİZGİ · YOL · 60.000 m — Boşluk: sıradaki"
bekle "[secim] seçim: 1 nesne, kimlik 3"

# SPACE WALKS ON — the parcel, then the ada round it, then round to the road —
# and each step selects what it names: a badge that moved while the selection
# stayed would pass the badge lines alone.
bekle "[secim] boşluk: 2/3 · ALAN · PARSEL · 1 600.00 m² — Boşluk: sıradaki | 1 nesne, kimlik 1"
bekle "[secim] boşluk: 3/3 · ALAN · ADA · 3 600.00 m² — Boşluk: sıradaki | 1 nesne, kimlik 2"
bekle "[secim] boşluk: 1/3 · ÇOKLUÇİZGİ · YOL · 60.000 m — Boşluk: sıradaki | 1 nesne, kimlik 3"
bekle "[secim] geri: 3/3 · ALAN · ADA · 3 600.00 m² — Boşluk: sıradaki | 1 nesne, kimlik 2"

# ENTER KEEPS, AND ESC PUTS BACK what was selected before the click.
bekle "[secim] enter: 1 nesne, kimlik 2 | rozet yok"
bekle "[secim] esc: 1 nesne, kimlik 2 | rozet yok"

# The large candidate set is chosen directly, searched and clicked. An empty
# search cannot commit an invisible candidate; Esc restores the prior choice.
bekle "[secim] liste: 20 | görünür 1 | çerçeve 1 | tuval içinde 1"
bekle "[secim] son: 20 | 1 nesne, kimlik 20"
bekle "[secim] ilk: 1 | 1 nesne, kimlik 3"
bekle "[secim] arama: 1 satır | 1 nesne, kimlik 20"
bekle "[secim] satır: 1 nesne, kimlik 20 | liste kapalı"
bekle "[secim] boş arama: -1 | seçim bekliyor 1"
bekle "[secim] arama esc: 1 nesne, kimlik 20 | liste kapalı"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "secim-listesi: OK — çerçeveli liste, 20 adayda doğrudan seçim/arama/tıklama;"
echo "secim-listesi:   Boşluk, Shift+Boşluk, Home/End, Enter ve Esc yerinde çalışıyor"
