#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: no header makes a class's SIZE depend on the token struct.
#
# `Tokens` gains a field every time the design does. A class that holds one BY
# VALUE in a header therefore changes size on that day, and every translation
# unit that includes the header has to be rebuilt. When one is not — a stale
# object, a warm build directory, a second build tree — two translation units
# disagree about how big the object is and write past each other. The heap is
# corrupted silently and glibc notices at shutdown, in an unrelated destructor,
# with nothing in the backtrace pointing at the cause.
#
# That happened. It cost an afternoon and three wrong hypotheses, and none of the
# candidates was a real bug. `darkTokens()` returns a reference to a static that
# outlives everything, so a pointer costs nothing and cannot go stale.
#
# A `.cpp` may hold one by value — a private class in one translation unit has no
# ABI to break.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/src/app/include"
fail=0

if [[ ! -d "$app" ]]; then
    echo "token-abi: skipped — /src/app/include is not in this tree"
    exit 0
fi

while IFS= read -r hit; do
    echo "token-abi: a header holds Tokens by value -> $hit" >&2
    echo "token-abi:   hold a const Tokens* instead: darkTokens() returns a" >&2
    echo "token-abi:   reference to a static, so there is nothing to copy, and a" >&2
    echo "token-abi:   value member makes the class change size whenever a token" >&2
    echo "token-abi:   is added — which a stale object turns into heap corruption." >&2
    fail=1
done < <(grep -rn --include='*.hpp' -E '^\s*Tokens\s+[A-Za-z_][A-Za-z0-9_]*\s*[{;=]' "$app" || true)

if [[ $fail -eq 0 ]]; then
    echo "token-abi: OK — no header's object size depends on Tokens"
fi
exit $fail
