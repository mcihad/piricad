#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the field calculator and the attribute table hold up over a million rows (TODOS G-03).
#
# Two probes drive the real shell, the way a person would:
#
#   PIRICAD_CALC_PROBE     the calculator WINDOW over six parcels: the lists are filled from the schema
#                          and from the parser's own function table, a refused expression writes
#                          nothing, a preview writes nothing and opens no undo step, "Uygula" stays
#                          shut until the fields say what was previewed, one apply is one undo step,
#                          and one undo takes a whole calculation back.
#   PIRICAD_BIGTABLE_PROBE a million points read from a point list: the calculator over every row, the
#                          table opened on them, the scroll bar dragged to the end, a sort, a filter —
#                          each timed, each against a budget (a gate, not an aspiration; Article 7).
#
# The probes run with their own configuration directory, so a developer's own PiriCAD.conf is never
# the thing under test and never the thing overwritten.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "alan-hesaplayici: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

cd "$kok"
yapilandirma="$(mktemp -d)"
trap 'rm -rf "$yapilandirma"' EXIT

kos() {
    local degisken="$1" sure="$2"
    set +e
    cikti="$(env XDG_CONFIG_HOME="$yapilandirma" QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
                 PIRICAD_DATA="$kok/data" "$degisken"=1 timeout "$sure" "$exe" 2>&1)"
    cikis=$?
    set -e
    if [[ $cikis -ge 124 ]]; then
        echo "alan-hesaplayici: $degisken sondası bitmedi ya da öldü (çıkış kodu $cikis)" >&2
        exit 1
    fi
    if grep -q 'BAŞARISIZ' <<<"$cikti"; then
        echo "alan-hesaplayici: $degisken sondası başarısız:" >&2
        grep 'BAŞARISIZ' <<<"$cikti" >&2
        exit 1
    fi
}

# ---- the window ----
kos PIRICAD_CALC_PROBE 300
for gerekli in 'pencere açıldı' 'sütun ve işlev listeleri dolu' 'işleve çift tıklamak ifadeye ekler' \
               "hatalı ifade Uygula'yı açmaz" 'önizleme hiçbir şey yazmadı' \
               'önizleme geri alma adımı açmadı' 'hesap tek geri alma adımı' \
               'önizlenmemiş ifade Uygula ile yazılmadı' 'kod iki sütundan kuruldu' \
               'tek GERİAL kodu bütün satırlarda geri aldı' 'alan hesabı yerinde kaldı'; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "alan-hesaplayici: pencere sondasının şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

# ---- a million rows ----
kos PIRICAD_BIGTABLE_PROBE 900
for gerekli in 'bir milyon nesne var' 'bir milyon satırın hesabı sınırın altında' \
               'kot sütunu bir milyon satırda değişti' 'tablo bir milyon satırı sayıyor' \
               'tablo bir milyon satırla sınırın altında açılıyor' 'son satıra kaydırıldı' \
               'en büyük numara en üstte' 'bir milyon satırın sıralaması sınırın altında' \
               'süzgeç 1000 satır bıraktı' 'bir milyon satırın süzülmesi sınırın altında' \
               'bileşik süzgeç satır buldu'; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "alan-hesaplayici: büyük tablo sondasının şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done
olcum="$(grep -E 'ölçüm: (hesaplayıcı: kot|tabloyu aç|sona kaydır|fid sütununa göre|süz: "nokta_no")' <<<"$cikti" \
            | sed 's/^\[büyüktablo\] ölçüm: /alan-hesaplayici:   /')"

echo "alan-hesaplayici: OK — hesaplayıcı penceresi önizleyip tek adımda yazıyor, hatalı ifade yazmıyor;"
echo "alan-hesaplayici:   bir milyon satırda (ölçülen):"
echo "$olcum"
