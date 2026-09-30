#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: no fast-math anywhere.
# piricad.md §7.3: -ffast-math / /fp:fast break NaN checks, reorder addition and
# destroy robust predicate correctness. -ffp-contract=off is mandatory because FMA
# does not round the intermediate result, so x86 and Apple Silicon would disagree.
# Software producing official survey documents must be bit-identical everywhere.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

# Comments are allowed to name the banned flags — that is how the ban is
# documented. Only live configuration counts, so each hit is re-tested with the
# comment tail removed.
while IFS= read -r hit; do
    code="${hit#*:*:}"
    code="${code%%#*}"
    if grep -qE '(-ffast-math|/fp:fast|-ffp-contract=fast|-funsafe-math-optimizations)' \
            <<<"$code"; then
        echo "fp-flags: banned fast-math flag -> $hit" >&2
        fail=1
    fi
done < <(grep -rn --include='CMakeLists.txt' --include='*.cmake' --include='Makefile' \
             --include='*.json' --include='*.yml' --include='*.yaml' \
             -E '(-ffast-math|/fp:fast|-ffp-contract=fast|-funsafe-math-optimizations)' \
             "$root" --exclude-dir=build --exclude-dir=.git || true)

# The GCC/Clang release flags must actually carry -ffp-contract=off.
if ! grep -q -- '-ffp-contract=off' "$root/cmake/PiriCADFlags.cmake"; then
    echo "fp-flags: cmake/PiriCADFlags.cmake no longer sets -ffp-contract=off" >&2
    fail=1
fi
if ! grep -q -- '-fno-fast-math' "$root/cmake/PiriCADFlags.cmake"; then
    echo "fp-flags: cmake/PiriCADFlags.cmake no longer sets -fno-fast-math" >&2
    fail=1
fi

if [[ $fail -eq 0 ]]; then
    echo "fp-flags: OK — no fast-math, -ffp-contract=off enforced"
fi
exit $fail
