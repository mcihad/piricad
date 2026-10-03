#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the digitiser's pen works in the real shell (TODOS G-04).
#
# A feature class is picked, drawn with, bound to and checked — here with the ribbon's own controls,
# the way a person would:
#
#   * THE PEN BOX lists the package's classes and starts with none picked; picking one makes its
#     layer the active one and the box shows it.
#   * WHAT IS DRAWN STARTS WITH THE CLASS'S DEFAULTS (a parcel carries its class code, a building
#     its floor count and structure type).
#   * A SHAPE THAT IS NOT THE CLASS'S IS REFUSED, in the transcript and in the document: a loose line
#     on the building layer never gets in.
#   * "SEÇİMİ BAĞLA" brings a selected object from an old layer under the class in hand.
#   * "SINIFI DENETLE" reports a value outside a field's own list.
#   * The layer property panel says which class a layer follows.
#
# `PIRICAD_KALEM_PROBE` drives the shell; with a directory in the variable it also photographs the
# sheet. The probe runs with its own configuration directory, so a developer's own PiriCAD.conf is
# never the thing under test and never the thing overwritten.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "kalem: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

cd "$kok"
yapilandirma="$(mktemp -d)"
trap 'rm -rf "$yapilandirma"' EXIT

set +e
cikti="$(env XDG_CONFIG_HOME="$yapilandirma" QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
             PIRICAD_DATA="$kok/data" PIRICAD_KALEM_PROBE=1 timeout 300 "$exe" 2>&1)"
cikis=$?
set -e

if [[ $cikis -ge 124 ]]; then
    echo "kalem: sonda bitmedi ya da öldü (çıkış kodu $cikis)" >&2
    exit 1
fi
if grep -q 'BAŞARISIZ' <<<"$cikti"; then
    echo "kalem: sonda başarısız:" >&2
    grep 'BAŞARISIZ' <<<"$cikti" >&2
    exit 1
fi

for gerekli in 'kalem kutusu paketin sınıflarını sayıyor' 'henüz kalem seçilmedi' \
               'Parsel kalemi seçildi' 'kutu seçilen kalemi gösteriyor' 'PARSEL katmanı etkin oldu' \
               'çizilen parsel sınıfın kodunu taşıyor' 'çizilen bina sınıfın başlangıç değerlerini taşıyor' \
               'açık çizgi Bina katmanında reddedildi' 'reddedilen nesne belgeye girmedi' \
               "'Seçimi bağla' düğmesi şeritte" 'düğme seçili nesneyi sınıfa bağladı' \
               'nesne BINA katmanına geldi' "'Sınıfı denetle' düğmesi şeritte" \
               'denetim listede olmayan değeri söyledi' "katman paneli 'sinif' satırını gösteriyor"; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "kalem: sondanın şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

echo "kalem: OK — kalem kutusu sınıfı seçtiriyor, çizilen nesne sınıfın değerleriyle başlıyor,"
echo "kalem:   sınıfa uymayan şekil reddediliyor, düğmeler bağlıyor ve denetliyor"
