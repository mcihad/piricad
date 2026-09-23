#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the support matrix is a measurement, and it is the current one.
#
# WHY THIS GATE EXISTS.
#
# `docs/nesneler/destek-matrisi.md` answers "what does each editing command do to
# each kind" — and it answers it by RUNNING them (`kentos_kapsam`): one fresh
# document per cell, the entity made through the commands a user types, the edit
# run through the same bus, the result classified. A table like that is only
# worth anything while it matches the program, so this regenerates it and fails
# on any difference. A change that makes TRIM work on an arc therefore changes
# the table in the same commit, and a change that quietly breaks it shows up as a
# cell that moved (TODOS F-01: "the table is updated first; a done feature is not
# re-tasked as missing").
#
# A MISSING TOOL IS A FAILURE, not a skip. A freshness check that skips on a cold
# tree has never run — `ci-gate-docs.sh` learned that about four artefacts.
#
# HOW TO SATISFY IT
#
#   make kapsam
#
# and commit the regenerated page with the change that moved it.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

page="docs/nesneler/destek-matrisi.md"
if [[ ! -f "$page" ]]; then
    echo "kapsam: $page yok -> $page:1  (make kapsam)" >&2
    exit 1
fi

tool=""
for candidate in dev release debug asan; do
    if [[ -x "$root/build/$candidate/bin/kentos_kapsam" ]]; then
        tool="$root/build/$candidate/bin/kentos_kapsam"
        break
    fi
done
if [[ -z "$tool" ]]; then
    echo "kapsam: kentos_kapsam derlenmemiş; destek matrisi doğrulanamadı." >&2
    echo "kapsam:   Önce derleyin (make build). Atlayan bir tazelik denetimi hiç koşmamış demektir." >&2
    exit 1
fi

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
if ! "$tool" "$scratch/matris.md" "$root" "$scratch/is" >/dev/null 2>"$scratch/hata"; then
    echo "kapsam: kentos_kapsam çalışmadı:" >&2
    cat "$scratch/hata" >&2
    exit 1
fi

if ! diff -q "$page" "$scratch/matris.md" >/dev/null; then
    echo "kapsam: $page güncel değil -> $page:1  (make kapsam)" >&2
    diff -u "$page" "$scratch/matris.md" | head -40 >&2
    exit 1
fi

summary="$(grep -m1 ' hücre: ' "$page")"
echo "kapsam: OK — $summary"
