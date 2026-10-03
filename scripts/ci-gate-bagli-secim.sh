#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the table and the map hold ONE selection, and the property panel speaks for every selected
# object (TODOS U-04).
#
# FIVE CLAIMS, and a transcript of commands can keep none of them:
#
#   * MAP -> TABLE. What is selected on the drawing is the row the table holds.
#   * TABLE -> MAP. The rows chosen in the table are what the drawing selects, through the SEÇ line
#     a typed one would be.
#   * COMMON OR MIXED. A column read across a selection shows the value the objects share, or says
#     `karışık` (KARIŞIK) instead of showing the first object's as everyone's; a column only some
#     of them carry is marked `3/4`.
#   * THE TABLE LEAVES THE SELECTION ALONE. Its reload after an edit or an undo used to put its cursor
#     row back AS A SELECTION and send `SEÇ` for it, so one GERİAL collapsed three selected parcels
#     into one.
#   * ONE EDIT, ALL OF THEM, ONE STEP. Editing the mixed row writes the value onto every selected
#     object through the command (`ÖZNİTELİK … nesneler=…`) and a single GERİAL takes all of it back.
#
# `PIRICAD_LINK_PROBE` drives the real shell offscreen; the lines below are what it must print.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "bagli-secim: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

profil="${XDG_CONFIG_HOME:-$HOME/.config}/PiriCAD/PiriCAD.conf"
once=""
[[ -f "$profil" ]] && once="$(cksum <"$profil")"

cd "$kok"
set +e
cikti="$(QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" PIRICAD_DATA="$kok/data" PIRICAD_LINK_PROBE=1 \
             timeout 300 "$exe" 2>&1)"
rc=$?
set -e

if [[ $rc -ge 124 ]]; then
    echo "bagli-secim: sonda bitmedi ya da öldü (çıkış kodu $rc)" >&2
    exit 1
fi

if grep -q 'BAŞARISIZ' <<<"$cikti"; then
    echo "bagli-secim: sonda başarısız:" >&2
    grep 'BAŞARISIZ' <<<"$cikti" >&2
    exit 1
fi

# A probe that stopped early prints no failure either: the claims have to be SEEN.
for gerekli in 'haritadan seçilen nesne tabloda satır oldu' 'tablodan seçilen satırlar haritada seçildi' \
               'iki parselde ortak değer gösteriliyor' 'farklı değerler karışık diye gösteriliyor' \
               'sütunu yalnız 3/4 nesne taşıyorsa satır bunu söylüyor' \
               'toplu değişiklik tek geri alma adımı' 'tek GERİAL üçünü birden eski değerlerine döndürdü' \
               'tablo açıkken GERİAL çizimdeki seçimi bozmadı'; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "bagli-secim: sondanın şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

if [[ -n "$once" && "$once" != "$(cksum <"$profil")" ]]; then
    echo "bagli-secim: sonda koşusu kişinin kendi profilini değiştirdi: $profil" >&2
    exit 1
fi

echo "bagli-secim: OK — harita ve tablo tek seçim, panel ortak/karışık değeri söylüyor,"
echo "bagli-secim:   toplu düzenleme tek adım ve tek GERİAL ile geri dönüyor"
