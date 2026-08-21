#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the MPYY gösterim catalogues are what the official annexes say, and they
# are REPRODUCIBLE.
#
# piricad.md §15 / §12: a legislation change is a data release, not a rebuild —
# so the data must be regenerable from the source annex, not hand-typed. A
# hand-edited catalogue is undetectable in review and unfalsifiable afterwards;
# a stored digest makes both impossible.
#
# Three things are checked, in this order:
#   1. the shipped catalogues match the stored golden summary, value for value
#      (row counts per annex, reference colours, the belirsiz roster);
#   2. their SHA-256 matches the stored digest — this is what catches a hand-edit
#      of a generated file (.claude/data.md P6, CLAUDE.md 5.13);
#   3. when the source annexes are present, the extractor is re-run into a scratch
#      directory and the output is compared BYTE FOR BYTE — the determinism proof
#      (.claude/test.md, CLAUDE.md 6.5). Without the sources this step reports as
#      skipped; a skipped check is reported, never silently passed.
#
# The golden summary is data and is reviewed as data. Regenerate it, after reading
# the diff, with:  PIRICAD_GOLDEN_UPDATE=1 bash scripts/ci-gate-mpyy.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mpyy="$root/data/catalogs/mpyy"
fixture="$root/tests/golden/mpyy/beklenen.txt"
kaynak="${PIRICAD_MPYY_KAYNAK:-$root/.mpyy-kaynak}"
fail=0

if [[ ! -d "$mpyy" ]]; then
    echo "mpyy: skipped — data/catalogs/mpyy does not exist"
    exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "mpyy: skipped — python3 is not installed, so the catalogues cannot be summarised"
    exit 0
fi

summary="$(python3 "$root/scripts/mpyy-ozet.py" "$mpyy")"

if [[ "${PIRICAD_GOLDEN_UPDATE:-}" == "1" ]]; then
    mkdir -p "$(dirname "$fixture")"
    printf '%s\n' "$summary" >"$fixture"
    echo "mpyy: golden özeti güncellendi -> ${fixture#"$root"/}"
    exit 0
fi

if [[ ! -f "$fixture" ]]; then
    echo "mpyy: golden summary missing -> ${fixture#"$root"/}:1  (run: PIRICAD_GOLDEN_UPDATE=1 bash scripts/ci-gate-mpyy.sh)" >&2
    exit 1
fi

if ! diff -u "$fixture" <(printf '%s\n' "$summary") >/tmp/mpyy-gate-diff.$$ 2>&1; then
    echo "mpyy: shipped catalogue does not match the golden summary -> ${fixture#"$root"/}:1" >&2
    sed -n '1,60p' /tmp/mpyy-gate-diff.$$ >&2
    rm -f /tmp/mpyy-gate-diff.$$
    fail=1
else
    rm -f /tmp/mpyy-gate-diff.$$
    echo "mpyy: OK — katalog özeti golden ile birebir"
fi

# ---- determinism: same source in, same bytes out -----------------------------
if [[ -d "$kaynak" ]]; then
    scratch="$(mktemp -d)"
    trap 'rm -rf "$scratch"' EXIT
    mkdir -p "$scratch/data/catalogs/schema"
    cp "$root"/data/catalogs/schema/*.json "$scratch/data/catalogs/schema/"
    if python3 "$root/scripts/mpyy-cikar.py" --kaynak "$kaynak" --depo "$scratch" \
                                             --gorsel-yok >/dev/null 2>&1; then
        for f in plan-gosterim.json detay-katalogu.json asgari-standartlar.json; do
            if ! cmp -s "$mpyy/$f" "$scratch/data/catalogs/mpyy/$f"; then
                echo "mpyy: re-extraction does not reproduce the shipped file byte for byte -> data/catalogs/mpyy/$f:1" >&2
                fail=1
            fi
        done
        [[ $fail -eq 0 ]] && echo "mpyy: OK — kaynaktan yeniden çıkarım üç katalogu da bayt birebir üretiyor"
    else
        echo "mpyy: re-extraction failed; the source directory is present but unreadable -> $kaynak:1" >&2
        fail=1
    fi
else
    echo "mpyy: note — determinism re-run skipped, source annexes are not present ($kaynak). The stored SHA-256 in the golden summary still pins the shipped bytes."
fi

exit $fail
