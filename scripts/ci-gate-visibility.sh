#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: symbols are hidden by default; only the plugin ABI has an export surface.
# kentoscad.md §7.3 / §4.3 — build.md R9 fixes the GCC/Clang flag string
# (-O2 -fno-fast-math -ffp-contract=off -flto=thin -fvisibility=hidden); MSVC does
# not auto-export, and CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS would undo that posture.
# plugin-api.md R13: kentos_plugin_api exports exactly the symbols listed in
# kentos_plugin_api.exports — no other module may declare an exported symbol.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
flags="$root/cmake/KentOSCadFlags.cmake"

# 1. The default-hidden flag must stay in the shared flag interface.
if ! grep -q -- '-fvisibility=hidden' "$flags"; then
    echo "visibility: cmake/KentOSCadFlags.cmake no longer sets -fvisibility=hidden -> $flags:1" >&2
    fail=1
fi

# 2. MSVC posture: nothing may switch auto-export of every symbol back on.
while IFS= read -r hit; do
    echo "visibility: MSVC auto-export defeats hidden visibility -> $hit" >&2
    fail=1
done < <(grep -rn --include='CMakeLists.txt' --include='*.cmake' --include='*.json' \
             -E 'WINDOWS_EXPORT_ALL_SYMBOLS[^#]*(ON|TRUE|YES|[^A-Z_]1)' \
             "$root" --exclude-dir=build --exclude-dir=.git || true)

# 3. Only /src/plugin-api may declare an explicit export. Comments are allowed to
#    name the attribute — that is how the ban is documented — so each hit is
#    re-tested with the // tail removed.
export_re='visibility[[:space:]]*\([[:space:]]*"(default|protected)"|dllexport|Q_DECL_EXPORT'
if [[ -z "$(find "$root/src" -path "$root/src/plugin-api" -prune -o \
               -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' -o -name '*.c' \) -print \
            | head -n 1)" ]]; then
    scanned=""
    echo "visibility: skipped — /src has no C/C++ sources outside /src/plugin-api in Phase 0"
else
    scanned=", no explicit export outside /src/plugin-api"
    while IFS= read -r hit; do
        code="${hit#*:*:}"
        code="${code%%//*}"
        if grep -qE "$export_re" <<<"$code"; then
            echo "visibility: explicit symbol export outside /src/plugin-api -> $hit" >&2
            fail=1
        fi
    done < <(grep -rn --include='*.hpp' --include='*.cpp' --include='*.h' --include='*.c' \
                 -E "$export_re" "$root/src" --exclude-dir=plugin-api || true)
fi

if [[ $fail -eq 0 ]]; then
    [[ -n "$scanned" && -z "$(find "$root/src/plugin-api" -type f -name '*.h' | head -n 1)" ]] &&
        scanned="$scanned (empty in Phase 0 — no export surface exists yet)"
    echo "visibility: OK — -fvisibility=hidden set in cmake/KentOSCadFlags.cmake, no auto-export$scanned"
fi
exit $fail
