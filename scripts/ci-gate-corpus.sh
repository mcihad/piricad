#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every legislation corpus chunk is citable.
# kentoscad.md §5.5: "Cevap her zaman kaynak madde referansı ile verilmeli. Madde
# numarası ve yayım tarihi olmadan verilen cevap bu alanda değersizdir, hatta
# tehlikelidir." data.md R11/P3 and ai.md R15/P5 turn that into metadata: no chunk
# without `madde` and `published`/`yayim_tarihi`. data.md R15 keeps bulk out of git.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
corpus="$root/data/corpus"
fail=0
chunks=0

if [[ ! -d "$corpus" ]] || [[ -z "$(find "$corpus" -type f \
        ! -name '.gitkeep' ! -name 'README.md' -print -quit)" ]]; then
    echo "corpus: skipped — /data/corpus is empty in Phase 0"
    exit 0
fi

# $1 = one chunk's JSON text, $2 = file (repo-relative), $3 = line
check_chunk() {
    chunks=$((chunks + 1))
    grep -qE '"madde"[[:space:]]*:' <<<"$1" \
        || { echo "corpus: chunk carries no madde number -> $2:$3" >&2; fail=1; }
    grep -qE '"(published|yayim_tarihi)"[[:space:]]*:[[:space:]]*"[0-9]{4}-[0-9]{2}-[0-9]{2}' <<<"$1" \
        || { echo "corpus: chunk carries no ISO-8601 publication date -> $2:$3" >&2; fail=1; }
    grep -qE '"source_ref"[[:space:]]*:' <<<"$1" \
        || { echo "corpus: chunk carries no source_ref -> $2:$3" >&2; fail=1; }
}

while IFS= read -r f; do
    rel="${f#"$root"/}"
    if [[ "$f" == *.jsonl ]]; then
        n=0
        while IFS= read -r line || [[ -n "$line" ]]; do
            n=$((n + 1))
            [[ -z "${line//[[:space:]]/}" ]] && continue
            check_chunk "$line" "$rel" "$n"
        done < "$f"
    else
        check_chunk "$(cat "$f")" "$rel" 1
    fi
done < <(find "$corpus" -type f \( -name '*.json' -o -name '*.jsonl' \) | sort)

# R15: bulk corpus text and embedding indexes belong in Git LFS, not in the tree.
while IFS= read -r big; do
    echo "corpus: file over 50 MB belongs in Git LFS, not the repository -> ${big#"$root"/}:1" >&2
    fail=1
done < <(find "$corpus" -type f -size +50M | sort)

if [[ $fail -eq 0 ]]; then
    echo "corpus: OK — $chunks chunk(s) carry madde + publication date + source_ref, none over 50 MB"
fi
exit $fail
