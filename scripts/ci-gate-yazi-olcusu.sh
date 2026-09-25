#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the core's text measure is the face the program draws with.
#
# WHY THIS GATE EXISTS (TODOS C-18).
#
# The box a text is picked by, whether a dimension's figure fits between its
# extension lines and the baseline a caption stands on all come from
# `core::text_width` — and the core cannot open a font (core.md R1, P9), so the
# drawing face's advances are CONSTANT DATA of the core library,
# `src/core/src/text_metrics_table.cpp`, written by `kentos_yazi_olcusu` from
# `data/fonts/IBMPlexSans-Regular.ttf` with the atlas's own shaper. A table
# like that is only worth anything while it is the face's, so this regenerates
# it and fails on any difference: a new font release changes the table in the
# same commit, and a hand-edited number is caught the day it is made.
#
# A MISSING TOOL IS A FAILURE, not a skip. A freshness check that skips on a
# cold tree has never run — `ci-gate-docs.sh` learned that about four
# artefacts. The tool needs the text engine (KENTOS_WITH_TEXT), which every
# sanctioned preset but `headless` turns on (Article 8.1); a headless tree has
# no font engine to ask and says so.
#
# HOW TO SATISFY IT
#
#   make yazi-olcusu
#
# and commit the regenerated table with the change that moved it.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

table="src/core/src/text_metrics_table.cpp"
if [[ ! -f "$table" ]]; then
    echo "yazi-olcusu: $table yok -> $table:1  (make yazi-olcusu)" >&2
    exit 1
fi
if ! head -n1 "$table" | grep -q '^// GENERATED FILE - do not edit'; then
    echo "yazi-olcusu: $table üretilmiş dosya satırıyla başlamıyor -> $table:1  (make yazi-olcusu)" >&2
    exit 1
fi

# NO SECOND MEASURE (core.md R21): a per-letter guess of a width is how the box
# and the sheet came to disagree in the first place.
if guesses="$(grep -rnE 'text_width_estimate|height \* 6 \* ' src --include='*.cpp' --include='*.hpp' 2>/dev/null)" && [[ -n "$guesses" ]]; then
    echo "yazi-olcusu: yazı genişliği tahmini var; core::text_width kullanın ->" >&2
    echo "$guesses" | head -5 >&2
    exit 1
fi

tool=""
for candidate in dev release debug asan; do
    if [[ -x "$root/build/$candidate/bin/kentos_yazi_olcusu" ]]; then
        tool="$root/build/$candidate/bin/kentos_yazi_olcusu"
        break
    fi
done
if [[ -z "$tool" ]]; then
    if [[ -f "$root/build/headless/CMakeCache.txt" && ! -d "$root/build/dev" && \
          ! -d "$root/build/release" && ! -d "$root/build/debug" && ! -d "$root/build/asan" ]]; then
        echo "yazi-olcusu: yalnız headless ağaç var; yazı motoru derlenmediği için tablo doğrulanamadı (Article 8.1)."
        exit 0
    fi
    echo "yazi-olcusu: kentos_yazi_olcusu derlenmemiş; yazı ölçüsü tablosu doğrulanamadı." >&2
    echo "yazi-olcusu:   Önce derleyin (make build). Atlayan bir tazelik denetimi hiç koşmamış demektir." >&2
    exit 1
fi

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
if ! "$tool" "$root/data/fonts" "$scratch/tablo.cpp" >/dev/null 2>"$scratch/hata"; then
    echo "yazi-olcusu: kentos_yazi_olcusu çalışmadı:" >&2
    cat "$scratch/hata" >&2
    exit 1
fi

if ! diff -q "$table" "$scratch/tablo.cpp" >/dev/null; then
    echo "yazi-olcusu: $table yazı tipiyle aynı değil -> $table:1  (make yazi-olcusu)" >&2
    diff -u "$table" "$scratch/tablo.cpp" | head -40 >&2
    exit 1
fi

runs="$(grep -m1 -oE 'std::array<Run, [0-9]+>' "$table" | grep -oE '[0-9]+')"
echo "yazi-olcusu: OK — IBMPlexSans-Regular, $runs sıra ve doğrudan U+0000–U+017F; tablo yazı tipiyle aynı"
