#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a new user can start from a sample, edit it and print it at the right scale (TODOS U-06).
#
# The sample projects are data (data/ornekler); the command that opens them is ÖRNEKPROJE. This holds
# the half only the real shell can show, with the probe driving each of the five the way a person
# would:
#
#   * THE FIRST CLICK ASKS NOTHING: a program that has just started is not "unsaved work". It used
#     to be — the coordinate system was resolved at startup and moved the revision — so the first
#     sample, `Aç` and the close button all asked to save changes nobody had made.
#   * EACH PROJECT OPENS (no refusal, the transcript says what it is and prints its "Deneyin"
#     lines), fills the drawing, and EVERY line it offers runs as printed.
#   * THE SHEET PRINTS: a PDF is written for each project that ends in one, at A4 landscape.
#   * The application menu lists all five (`PIRICAD_MENU_PROBE`, ci-gate-menu / ctest menu-bar).
#
# `PIRICAD_SAMPLE_PROBE` drives the shell offscreen; with a directory in the variable it also
# photographs each project, before and after its first edit, and keeps the PDFs.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "ornek-proje: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

profil="${XDG_CONFIG_HOME:-$HOME/.config}/PiriCAD/PiriCAD.conf"
once=""
[[ -f "$profil" ]] && once="$(cksum <"$profil")"

cd "$kok"
set +e
cikti="$(QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" PIRICAD_DATA="$kok/data" PIRICAD_SAMPLE_PROBE=1 \
             timeout 300 "$exe" 2>&1)"
rc=$?
set -e

if [[ $rc -ge 124 ]]; then
    echo "ornek-proje: sonda bitmedi ya da öldü (çıkış kodu $rc)" >&2
    exit 1
fi

if grep -q 'BAŞARISIZ' <<<"$cikti"; then
    echo "ornek-proje: sonda başarısız:" >&2
    grep 'BAŞARISIZ' <<<"$cikti" >&2
    exit 1
fi

for gerekli in 'açılışta çizim temiz' \
               'Ölçüden harita: açıldı, hata yok' 'Ölçüden harita: PDF A4 yatay' \
               'Parsel düzenleme: açıldı, hata yok' 'Parsel düzenleme: PDF A4 yatay' \
               'Plan çizimi: açıldı, hata yok' 'Plan çizimi: PDF A4 yatay' \
               'GIS analizi: açıldı, hata yok' "GIS analizi: 'TAMPON nesneler=1 mesafe=15' çalıştı" \
               'Arazi işi — aplikasyon: açıldı, hata yok' "Arazi işi — aplikasyon: 'APLİKASYON istasyon=485270,4310180' çalıştı"; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "ornek-proje: sondanın şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

if [[ -n "$once" && "$once" != "$(cksum <"$profil")" ]]; then
    echo "ornek-proje: sonda koşusu kişinin kendi profilini değiştirdi: $profil" >&2
    exit 1
fi

echo "ornek-proje: OK — beş örnek proje menüden/komuttan açılıyor, her 'Deneyin' satırı çalışıyor,"
echo "ornek-proje:   pafta A4 yatay PDF olarak yazılıyor, açılışta çizim temiz"
