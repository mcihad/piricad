#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the plugin boundary is pure C.
# kentoscad.md §4.3, §13 — .claude/plugin-api.md R1, R3, P1, P9 and its Enforcement
# row "No C++/Qt in header": every header under /src/plugin-api/include is C99 —
# extern "C", fixed-width integers, opaque handles — and carries the ABI version
# macro the §13 handshake is built on. A C++ class, template, reference, STL type,
# exception or <Q...> include on that line breaks the ABI for every compiler that
# is not the one that built the host.
# (The abidiff half of the Enforcement table needs a previous release tag and a
# built kentos_plugin_api; it lands with the first release.)
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
inc="$root/src/plugin-api/include"
fail=0

# Bash 3.2 has no `mapfile` (it arrived in 4.0) and macOS ships 3.2 as
# /bin/bash, which is what `env bash` finds there. Read the list instead.
headers=()
while IFS= read -r line; do headers+=("$line"); done \
    < <(find "$inc" -type f \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null | sort)
if [[ ${#headers[@]} -eq 0 ]]; then
    echo "abi: skipped — /src/plugin-api/include holds no header yet (Phase 0)"
    exit 0
fi

cpp='\b(class|template|namespace|throw|try|catch|new|delete|typename|virtual)\b|\bstd::|\bkentos::|\b(Result|Task)[[:space:]]*<|#[[:space:]]*include[[:space:]]*<Q|#[[:space:]]*include[[:space:]]*<(vector|string|memory|map|set|functional|optional|variant|array|utility|algorithm)>'
cond='#[[:space:]]*(if|ifdef)[[:space:]].*KENTOS_'
ref='[A-Za-z_0-9)][[:space:]]*&[[:space:]]*[A-Za-z_]'

# Comments may name the banned constructs — that is how the ban is documented —
# so every hit is re-tested with the // tail removed and comment bodies skipped.
while IFS= read -r hit; do
    code="${hit#*:*:}"
    code="${code%%//*}"
    if grep -qE '^[[:space:]]*(\*|/\*)' <<<"$code"; then
        continue
    elif grep -qE "$cpp" <<<"$code"; then
        echo "abi: C++ construct on the plugin C ABI -> $hit" >&2; fail=1
    elif grep -qE "$cond" <<<"$code"; then
        echo "abi: #if/#ifdef on a KENTOS_ macro (P9) -> $hit" >&2; fail=1
    elif grep -qE "$ref" <<<"$code" && ! grep -qE '&&|#[[:space:]]*define' <<<"$code"; then
        echo "abi: C++ reference on the plugin C ABI -> $hit" >&2; fail=1
    fi
done < <(grep -rnH -E "$cpp|$cond|$ref" "${headers[@]}" || true)

for h in "${headers[@]}"; do
    if ! grep -qE 'extern[[:space:]]*"C"' "$h"; then
        echo "abi: header is not wrapped in extern \"C\" -> $h:1" >&2
        fail=1
    fi
done

if ! grep -qhE '#[[:space:]]*define[[:space:]]+KENTOS_[A-Z0-9_]*ABI[A-Z0-9_]*' "${headers[@]}"; then
    echo "abi: no KENTOS_*ABI* version macro declared (R3) -> ${headers[0]}:1" >&2
    fail=1
fi

if [[ $fail -eq 0 ]]; then
    echo "abi: OK — ${#headers[@]} plugin header(s) are C99, extern \"C\", ABI-versioned"
fi
exit $fail
