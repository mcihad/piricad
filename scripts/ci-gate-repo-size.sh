#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the repository stays cloneable.
# .claude/data.md R15 / P8, kentoscad.md §12: any file over 10 MB belongs in Git LFS
# or in an external artefact store referenced by URL + SHA-256 from
# /data/MANIFEST.json; the working-tree cap is one named value in that manifest.
# Geoid grids, LAZ tiles and corpus PDFs are how a survey repo becomes a 3 GB
# clone, so the total is printed on every run, passing or not.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
file_cap=$((10 * 1024 * 1024))
tree_cap_mb=250
manifest="$root/data/MANIFEST.json"
if [[ -f "$manifest" ]]; then
    declared="$(grep -oE '"worktree_size_cap_mb"[^0-9]+[0-9]+' "$manifest" | grep -oE '[0-9]+$' || true)"
    [[ -n "$declared" ]] && tree_cap_mb="$declared"
fi

# git ls-files excludes build output and every untracked scratch file for free.
# Before the first commit nothing is tracked, so fall back to the working tree
# with build/ and .git/ pruned explicitly.
mode="tracked files"
if ! git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
   [[ "$(git -C "$root" ls-files 2>/dev/null | wc -l)" -eq 0 ]]; then
    mode="working tree, nothing tracked by git yet"
fi
count=0; total=0; oversized=""
while IFS= read -r -d '' f; do
    abs="$f"; [[ "$abs" == /* ]] || abs="$root/$f"
    [[ -f "$abs" ]] || continue
    n="$(wc -c <"$abs" 2>/dev/null || echo 0)"
    count=$((count + 1)); total=$((total + n))
    [[ $n -gt $file_cap ]] && oversized+="$n ${abs#"$root"/}"$'\n'
done < <(if [[ "$mode" == "tracked files" ]]; then git -C "$root" ls-files -z;
         else find "$root" \( -path "$root/build" -o -path "$root/.git" \) -prune -o -type f -print0; fi)

if [[ $count -eq 0 ]]; then
    echo "repo-size: skipped — no files to measure under $root"
    exit 0
fi
# Largest first, so the CI log names the worst offender on its first line.
while IFS=' ' read -r bytes path; do
    [[ -n "${path:-}" ]] || continue
    if git -C "$root" check-attr filter -- "$path" 2>/dev/null | grep -q 'filter: lfs'; then continue; fi
    echo "repo-size: $((bytes / 1024 / 1024)) MB file is over the 10 MB cap; move it to Git LFS or to the external artefact store recorded in /data/MANIFEST.json -> $path:$bytes" >&2
    fail=1
done < <(printf '%s' "$oversized" | sort -rn)

if [[ $total -gt $((tree_cap_mb * 1024 * 1024)) ]]; then
    echo "repo-size: tree is $((total / 1024 / 1024)) MB, past the ${tree_cap_mb} MB cap; move data to Git LFS or the external artefact store -> $manifest:0" >&2
    fail=1
fi

if [[ $fail -eq 0 ]]; then
    echo "repo-size: OK — $count $mode, $((total / 1024)) KB total of ${tree_cap_mb} MB cap, no file over 10 MB outside Git LFS"
fi
exit $fail
