#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: no geographic data without a Coğrafi Veri İzin Belgesi record.
# kentoscad.md §12 "Veri ve Kurumsal": "Coğrafi Veri İzin Belgesi süreci".
# .claude/data.md R16 / P5: a dataset that ships or is redistributed MUST have a
# permit row (permit id, covered data, scope, expiry, legal sign-off) in
# /data/LICENCES.md. Exempt: files directly in /data (MANIFEST.json, LICENCES.md)
# and the JSON Schemas under */schema/ — those are structure, not data.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
licences="$root/data/LICENCES.md"
# Bash 3.2 has no `mapfile` (it arrived in 4.0) and macOS ships 3.2 as
# /bin/bash, which is what `env bash` finds there. Read the list instead.
datafiles=()
while IFS= read -r line; do datafiles+=("$line"); done \
    < <(find "$root/data" -mindepth 2 -type f \
        \( -iname '*.json' -o -iname '*.geojson' -o -iname '*.gpkg' -o -iname '*.tif' \
           -o -iname '*.gml' -o -iname '*.csv' -o -iname '*.gsb' -o -iname '*.grid' \) \
        -not -path '*/schema/*' | sort)
if [[ ${#datafiles[@]} -eq 0 ]]; then
    state="present"; [[ -f "$licences" ]] || state="not created yet"
    echo "data-permits: skipped — /data holds no data file yet (Phase-0 placeholders only); data/LICENCES.md $state"
    exit 0
fi

if [[ ! -f "$licences" ]]; then
    echo "data-permits: data files present but /data/LICENCES.md does not exist -> ${datafiles[0]#"$root"/}:1" >&2
    exit 1
fi
if ! grep -qiE '(kaynak|source)' "$licences"; then
    echo "data-permits: /data/LICENCES.md declares no source column -> data/LICENCES.md:1" >&2
    fail=1
fi

# The row must name the directory, state a permit status and carry an ISO-8601
# date (expiry / sign-off). Status tokens stay ASCII so -i folding is locale-safe:
# "zin belgesi" matches both "İzin Belgesi" and "izin belgesi".
status='zin[- ]belgesi|permit|kamuya|public[- ]domain|redistribut'
for dir in $(printf '%s\n' "${datafiles[@]}" | xargs -n1 dirname | sort -u); do
    rel="${dir#"$root"/}"
    key="${rel#data/}"   # rows are written relative to /data, e.g. `crs/tg03/`
    first="$(printf '%s\n' "${datafiles[@]}" | grep -m1 -E "^${dir}/[^/]+$" || true)"
    first="${first#"$root"/}"
    row="$(grep -n -F -- "$key" "$licences" | head -n1 || true)"
    if [[ -z "$row" ]]; then
        echo "data-permits: data files with no İzin Belgesi row in /data/LICENCES.md for $rel -> $first:1" >&2
        fail=1
        continue
    fi
    if ! grep -qiE "$status" <<<"$row"; then
        echo "data-permits: permit row for $rel states no redistribution permit status -> data/LICENCES.md:${row%%:*}" >&2
        fail=1
    fi
    if ! grep -qE '[0-9]{4}-[0-9]{2}-[0-9]{2}' <<<"$row"; then
        echo "data-permits: permit row for $rel has no ISO-8601 expiry/sign-off date -> data/LICENCES.md:${row%%:*}" >&2
        fail=1
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "data-permits: OK — every /data subdirectory holding data files has a dated LICENCES.md permit row"
fi
exit $fail
