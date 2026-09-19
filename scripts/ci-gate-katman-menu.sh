#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the layer panel's right-click menu reaches what it promises.
#
# TWO ENTRIES, AND BOTH ARE ABOUT ONE LAYER. "Tümünü seç" selects that layer's
# objects and nothing else; "Öznitelik tablosu" opens the table on that layer's
# rows and nothing else. The second one is the reason this gate exists: the table
# already accepted a layer name and used it for the WINDOW TITLE alone, so it
# announced a layer and listed the whole drawing.
#
# WHY IT IS A SHELL GATE. /tests links no Qt (Article 3.4), so a menu, a signal
# and a dialog cannot be constructed there. `KENTOS_LAYER_PROBE` opens the real
# menu — built by the same function a right-click builds it with — finds the
# entry by the text on it and fires it, then prints what the document and the
# table did. A test that called the handler would prove the handler works and say
# nothing about the route from the menu to it, which is the defect this panel has
# actually shipped before.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "katman-menu: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

# NO SCREEN IS NOT NO TEST, and this replaces the BEKLEMEDE this gate used to
# print. What is measured here is what the menu DOES — which entries it has, and
# what firing one leaves behind in the document — and Qt's offscreen platform
# builds a real menu and fires real actions. Requiring a display made the gate
# dormant in exactly the three places it was written for: no CI runner has one,
# and neither does a terminal on macOS. The app-level ctests next door
# (`shell-starts`, `canvas-edits`, `print-pdf`) have run offscreen all along.
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    export QT_QPA_PLATFORM=offscreen
    echo "katman-menu: ekranda pencere yok — menü offscreen açılıyor (ölçülen şey"
    echo "katman-menu:   menünün çizimi değil, davranışı)"
fi

# TWO LAYERS WITH DIFFERENT COUNTS, which is the whole test: a scope that is
# quietly ignored shows five rows where two belong.
gecici="$(mktemp -d)"
trap 'rm -rf "$gecici"' EXIT

cat > "$gecici/sahne.json" <<'JSON'
[
 {"cmd": "KATMAN", "args": {"ad": "PARSEL"}},
 {"cmd": "ALAN", "args": {"noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]]}},
 {"cmd": "ALAN", "args": {"noktalar": [[20000,0],[30000,0],[30000,10000],[20000,10000]]}},
 {"cmd": "KATMAN", "args": {"ad": "YOL"}},
 {"cmd": "ÇİZGİ", "args": {"noktalar": [[0,20000],[30000,20000]]}},
 {"cmd": "ÇİZGİ", "args": {"noktalar": [[0,24000],[30000,24000]]}},
 {"cmd": "ÇİZGİ", "args": {"noktalar": [[0,28000],[30000,28000]]}}
]
JSON

cd "$kok"

set +e
cikti="$(KENTOS_DATA="$kok/data" KENTOS_LAYER_PROBE=1 \
         "$exe" --betik "$gecici/sahne.json" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "katman-menu: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

# The layer the first object sits on is PARSEL, and PARSEL holds two of the five.
bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "katman-menu: beklenen satır yok -> $1" >&2
        grep '^\[katman\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

bekle "[katman] Tümünü seç · PARSEL → 2 nesne seçili"
bekle "[katman] Öznitelik tablosu · PARSEL → 2 satır"

# THE SHAPE OF THE MENU, in order, separators included. Two entries were taken
# out of it — `Stili düzenle…` and `Stili temizle`, both pieces of the Katman
# Özellikleri window shown as items in a list beside `Gizle` and `Gruba taşı…` —
# and one was put in: `Katman Özellikleri…`, last, after a rule, where every
# desktop program puts the entry that OPENS something rather than doing it.
#
# Checked as a whole line rather than entry by entry, because the order is part
# of the claim: a properties entry in the middle of the list is the thing this
# replaced.
bekle "[katman] menü · PARSEL: Yeni katman… | — | Tümünü seç | Öznitelik tablosu | Aktif katman yap | Özniteliklerden etiketle… | — | Görünüm | Kilidi aç | — | Gruba taşı… | — | Katman Özellikleri…"

# THE GÖRÜNÜM SUBMENU. In the line above a submenu is its title and nothing more,
# so its own shape is a line of its own. `Gizle` used to be a top-level entry
# whose word flipped with the row's state; it is inside this menu now, beside the
# five things that are about the OTHER layers.
bekle "[katman] görünüm · PARSEL: Göster | Gizle | Yalnız bunu göster | — | Tümünü göster | Gösterimi ters çevir"

# AND THE SUBMENU USED, on two rows at once. The panel takes several rows now, so
# the claim to check is that something ACTS on the set: two layers picked, one
# entry fired, two layers down. The undo line after it is the other half of the
# claim — the set went out as one command per layer inside one batch, so one
# Ctrl+Z brings all of them back (CLAUDE.md 1.5).
bekle "[katman] tümünü göster → 0 katman gizli"
bekle "[katman] çoklu gizle · PARSEL + YOL → 2 katman gizli"
bekle "[katman] çoklu gizle geri alındı → 0 katman gizli"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "katman-menu: OK — sağ tuş menüsü 'Tümünü seç' ve 'Öznitelik tablosu' girişlerini"
echo "katman-menu:   açtığı katmana bağlıyor: beşin ikisi, her ikisinde de;"
echo "katman-menu:   stil kalemleri çıktı, en altta 'Katman Özellikleri…' duruyor;"
echo "katman-menu:   Görünüm alt menüsü iki katmanı birlikte gizliyor, tek geri alma"
