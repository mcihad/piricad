#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the render path never narrows an absolute world coordinate to float.
# piricad.md §10.3: TUREF/TM3 eastings are seven digits, so a world coordinate
# written straight into a float vertex attribute shimmers by metres on screen.
# render.md R2/P1 — ViewTransform::offset_x_f / offset_y_f (src/render/src/view.cpp)
# are the ONLY sanctioned narrowing. Also R20/P6 (no per-frame allocation) and
# R17/P4 (ImGui is developer-only). Scope: /src/render + the canvas half of /src/app.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
narrow='(static_cast<[[:space:]]*float[[:space:]]*>|\bfloat)[[:space:]]*\([^)]*(\bMm\b|xs\[|ys\[|\.x\b|\.y\b)'
alloc='(\bnew\b|make_unique|make_shared|\.resize\(|\.reserve\()'
imgui='(ImGui::|ImGui_Impl|include[^;]*imgui)'
# Bash 3.2 has no `mapfile` (it arrived in 4.0) and macOS ships 3.2 as
# /bin/bash, which is what `env bash` finds there. Read the list instead.
files=()
while IFS= read -r line; do files+=("$line"); done \
    < <(find "$root/src/render" "$root/src/app" \
        \( -path '*/render/*' -o -name '*canvas*' -o -name '*overlay*' \) \
        \( -name '*.cpp' -o -name '*.hpp' \) 2>/dev/null || true)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "render: skipped — /src/render and the /src/app canvas hold no sources yet"
    exit 0
fi

# Nearest enclosing function name for a line: the last column-0 definition above it.
enclosing() { sed -n "1,${2}p" "$1" | grep -E '^[A-Za-z_][A-Za-z0-9_:<>&*, ]*\(' | tail -1 |
                  sed -E 's/\(.*//; s/.*[ *&]//'; }

for f in "${files[@]}"; do
    # R2/P1 — an absolute world coordinate must never reach a float.
    while IFS= read -r hit; do
        ln="${hit%%:*}"; code="${hit#*:}"; code="${code%%//*}"
        if ! grep -qE "$narrow" <<<"$code"; then continue; fi   # comment only
        if grep -qE 'offset_[xy]_f' <<<"$code"; then continue; fi
        if [[ "$f" == */render/src/view.cpp ]]; then continue; fi   # the definitions
        echo "render: world coordinate narrowed to float outside ViewTransform::offset_*_f -> $f:$ln" >&2
        fail=1
    done < <(grep -nE "$narrow" "$f" || true)
    # R20/P6 — the draw loop must not allocate (enclosing-name heuristic).
    while IFS= read -r hit; do
        ln="${hit%%:*}"; code="${hit#*:}"; code="${code%%//*}"
        if ! grep -qE "$alloc" <<<"$code"; then continue; fi
        fn="$(enclosing "$f" "$ln" || true)"
        if ! grep -qiE 'paint|render|draw' <<<"$fn"; then continue; fi
        echo "render: allocation in the draw loop ($fn) -> $f:$ln" >&2
        fail=1
    done < <(grep -nE "$alloc" "$f" || true)
    # R17/P4 — the ImGui overlay is developer-only.
    while IFS= read -r hit; do
        ln="${hit%%:*}"; code="${hit#*:}"; code="${code%%//*}"
        if ! grep -qE "$imgui" <<<"$code"; then continue; fi
        guard="$(sed -n "1,${ln}p" "$f" |
                 grep -E '^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|endif)' | tail -1 || true)"
        if grep -qE '#[[:space:]]*if.*(IMGUI|DEBUG)' <<<"$guard"; then continue; fi
        echo "render: Dear ImGui outside a debug/NDEBUG guard -> $f:$ln" >&2
        fail=1
    done < <(grep -nE "$imgui" "$f" || true)
done

if [[ $fail -eq 0 ]]; then
    echo "render: OK — no unoffset float narrowing, no allocation inside a paint/render/draw function (name heuristic), no unguarded ImGui"
fi
exit $fail
