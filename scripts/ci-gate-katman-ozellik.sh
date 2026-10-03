#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a layer's properties DO something (TODOS U-05).
#
# Five of a layer's fields — plottable, selectable, the two ends of the scale window, the opacity —
# were stored, hashed and saved while no command could set them and nothing drew by them. This holds
# the other half, which only the real shell can show:
#
#   * THE SCALE WINDOW HIDES A LAYER where the view is outside it, from either side, on the very
#     builder the canvas draws with — and a layer inside its window stays.
#   * A LAYER THAT DOES NOT PRINT is on the screen and not on the sheet; its opacity is the screen's
#     alone.
#   * THE PROPERTY PANEL speaks for the layer: its rows are the commands that write them (one undo
#     step each), and an empty scale window is refused there too.
#   * A LAYER THE PICK PASSES OVER is not taken by a box.
#   * A THOUSAND LAYERS stay fluid (U-05's acceptance): the list, the search box and the bulk
#     commands (flip every visibility, save and apply a state of all of them) are timed in the real
#     shell and held to generous bounds; the figures are printed.
#   * SAVED LAYER STATES (`KATMANDURUM`) are in the layer list's own menu, each entry is the command
#     that applies it, and applying one is a single undo step that brings every layer back as saved.
#
# `PIRICAD_LAYERPROPS_PROBE` drives the shell offscreen; with a directory in the variable it also
# photographs the far and near views and the panel.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "katman-ozellik: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

profil="${XDG_CONFIG_HOME:-$HOME/.config}/PiriCAD/PiriCAD.conf"
once=""
[[ -f "$profil" ]] && once="$(cksum <"$profil")"

cd "$kok"
set +e
cikti="$(QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" PIRICAD_DATA="$kok/data" PIRICAD_LAYERPROPS_PROBE=1 \
             timeout 300 "$exe" 2>&1)"
rc=$?
set -e

if [[ $rc -ge 124 ]]; then
    echo "katman-ozellik: sonda bitmedi ya da öldü (çıkış kodu $rc)" >&2
    exit 1
fi

if grep -q 'BAŞARISIZ' <<<"$cikti"; then
    echo "katman-ozellik: sonda başarısız:" >&2
    grep 'BAŞARISIZ' <<<"$cikti" >&2
    exit 1
fi

for gerekli in 'uzak görünümde INCE ölçek aralığıyla gizli' 'paftada ayrıca KILAVUZ yok' \
               'yakın görünümde GENEL kayboldu, INCE göründü' 'panel KILAVUZ' \
               'panelden yazılan basilir=evet katmanda' 'tek geri alma adımı' \
               'boş ölçek aralığı panelden de reddediliyor' 'seçilemez katmandaki çizgi kutuyla seçilmedi' \
               'alt menü kayıtlı durumları ve kaydetme satırını sunuyor' \
               'menüden Uygula — TUMU: katmanlar kayıttaki gibi geri geldi' 'durumu uygulamak tek geri alma adımı' \
               '1000 katmanda liste ve arama akıcı' '1000 katmanda toplu işlemler akıcı'; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "katman-ozellik: sondanın şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

if [[ -n "$once" && "$once" != "$(cksum <"$profil")" ]]; then
    echo "katman-ozellik: sonda koşusu kişinin kendi profilini değiştirdi: $profil" >&2
    exit 1
fi

echo "katman-ozellik: OK — ölçek aralığı iki yandan gizliyor, basılmayan katman paftada yok,"
echo "katman-ozellik:   panel satırları komut, boş aralık reddediliyor, seçilemez katman seçilmiyor"
