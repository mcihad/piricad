#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a click that hits several objects asks which one, and the answer holds.
#
# THE PROBLEM IT GUARDS. `core::pick_nearest` answers a click with ONE entity,
# by distance and then by the lower slot. That is correct and, on a plan sheet,
# unhelpful: a click lands on a parcel, on the boundary that closes it and on the
# ada boundary drawn over that, all at zero distance, and the user has no way to
# say they meant the second one. The shell opens a chooser; this checks that the
# chooser opens, describes what is under the cursor, and that taking a row other
# than the first actually selects that object.
#
# WHY IT IS A SHELL GATE. /tests links no Qt (Article 3.4), so a canvas, a mouse
# event and a modal dialog cannot be built there. What IS tested there is the
# half that is Qt-free — `core::pick_all`'s order and `SEÇ mod=NOKTA sira=` —
# and this gate covers the half that only exists on a screen. It starts at a real
# click for the reason the layer probe does: calling the handler proves the
# handler works and says nothing about the route from the click to it.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "secim-listesi: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
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
cikti="$(KENTOS_DATA="$kok/data" KENTOS_PICK_PROBE=1 \
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

# All three, nearest first — which for three shapes the point is INSIDE is slot
# order, the same tie `pick_nearest` breaks.
bekle "[secim] liste: 3 satır"

# AND WHAT EACH ROW SAYS. The columns are the reason the window is a table and
# not a menu of ids: a layer, a type and a measurement are what tell two parcels
# apart, and a row that lost its measurement would still look like a row.
# The thousands space is part of the check: `1 600.00` is what the attribute
# table prints and `1600.00` is what a column of areas cannot be scanned in.
bekle "[secim] satır 1: ALAN · PARSEL · 1 600.00 m² · 1"
bekle "[secim] satır 2: ALAN · ADA · 3 600.00 m² · 2"
bekle "[secim] satır 3: ÇOKLUÇİZGİ · YOL · 60.000 m · 3"

# THE SECOND ROW, AND THE DOCUMENT AGREES. The probe takes row 2; a chooser that
# opened, listed correctly and then selected the top object anyway would pass
# every check above.
bekle "[secim] seçim: 1 nesne, kimlik 2"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "secim-listesi: OK — üç nesnenin üstüne yapılan tıklama listeyi açıyor, satırlar"
echo "secim-listesi:   tür · katman · ölçü · kimlik yazıyor, ikinci satır ikinci nesneyi seçiyor"
