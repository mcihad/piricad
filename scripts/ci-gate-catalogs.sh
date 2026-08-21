#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every regulatory catalogue is a self-describing, versioned data package.
# piricad.md §15 "veri odaklı katalog mimarisi", §12: a legislation change is a
# data release, never a rebuild — so the file itself must say which regulation,
# annex/madde and publication date it encodes, under which licence.
# .claude/data.md R2 (header completeness), R3 (schema exists), R5 (ids stable,
# never reused). Constitution Article 9 "To change a regulatory rule".
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

mapfile -t catalogues < <(find "$root/data/catalogs" -type f -name '*.json' \
                              -not -path '*/schema/*' 2>/dev/null | sort)

if [[ ${#catalogues[@]} -eq 0 ]]; then
    echo "catalogs: skipped — /data/catalogs holds no catalogue *.json outside schema/ (empty in Phase 0)"
    exit 0
fi

for f in "${catalogues[@]}"; do
    rel="${f#"$root"/}"

    # R2: the header block. A missing field is a CI failure.
    for key in schema_version package_version source published licence; do
        grep -qE "\"$key\"[[:space:]]*:" "$f" && continue
        echo "catalogs: required header field \"$key\" missing (data.md R2) -> $rel:1" >&2
        fail=1
    done

    # R3: a JSON Schema must exist for it under data/catalogs/schema/.
    base="$(basename "$f" .json)"
    if [[ ! -f "$root/data/catalogs/schema/$base.schema.json" \
       && ! -f "$root/data/catalogs/schema/$base.json" ]]; then
        echo "catalogs: no schema/$base.schema.json for this catalogue (data.md R3) -> $rel:1" >&2
        fail=1
    fi

    # R5: an entry id is stable forever — it may never appear twice in one file.
    while IFS= read -r dup; do
        line="$(grep -nF "$dup" "$f" | grep '"id"' | sed -n 2p | cut -d: -f1)"
        echo "catalogs: duplicate catalogue entry id $dup (data.md R5) -> $rel:${line:-1}" >&2
        fail=1
    done < <(grep -oE '"id"[[:space:]]*:[[:space:]]*"[^"]*"' "$f" \
                 | grep -oE '"[^"]*"$' | sort | uniq -d)
done

if [[ $fail -eq 0 ]]; then
    echo "catalogs: OK — ${#catalogues[@]} catalogue(s) carry the R2 header, have a schema and no duplicate id (full JSON Schema validation is the Phase-1 job of the data-catalogs CI job)"
fi
exit $fail
