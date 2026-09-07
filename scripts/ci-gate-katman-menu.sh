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

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "katman-menu: BEKLEMEDE — ortamda ekran yok. Bir menü ancak açıldığı yerde"
    echo "katman-menu:   ölçülür; açılmamış bir menü hiçbir şeyi kanıtlamaz."
    exit 0
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

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "katman-menu: OK — sağ tuş menüsü 'Tümünü seç' ve 'Öznitelik tablosu' girişlerini"
echo "katman-menu:   açtığı katmana bağlıyor: beşin ikisi, her ikisinde de"
