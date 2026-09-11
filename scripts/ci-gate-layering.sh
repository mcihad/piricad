#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the module dependency graph is one-way.
# kentoscad.md §8 / Constitution Article 3.2-3.3: core -> nothing (not even Qt),
# command -> core, io/domain -> core+command, render -> core+Qt Gui, script/ai ->
# command, app -> everything; a reverse or lateral dependency is a build failure,
# and Article 3.4 lists the Qt-free targets. script.md R10/P4/P9 adds: no
# Document&/Document*/Layer* in a /src/script header. /src/core has its own gate.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
checked=""; skipped=""

# module : modules it may NOT include — upward or lateral in the Article 3.2 graph.
rules=("command:io|render|script|ai|domain|app|plugin-api"
       "io:render|script|ai|domain|app"
       "processing:render|script|ai|domain|app|plugin-api"
       "domain:io|render|script|ai|app"
       "render:command|io|script|ai|domain|app"
       "script:io|render|ai|domain|app"
       "ai:io|render|script|domain|app"
       "plugin-api:command|io|render|script|ai|domain|app")
sources=(--include='*.hpp' --include='*.cpp' --include='*.h' --include='*.c')

for rule in "${rules[@]}"; do
    mod="${rule%%:*}"
    dir="$root/src/$mod"
    if [[ ! -d "$dir" || -z "$(find "$dir" \( -name '*.[hc]pp' -o -name '*.[hc]' \) -print -quit)" ]]; then
        skipped="$skipped $mod"
        continue
    fi
    checked="$checked $mod"
    # Article 3.4: every module here except render is a Qt-free target.
    if [[ "$mod" != render ]]; then
        while IFS= read -r hit; do
            echo "layering: Qt inside the Qt-free target kentos_$mod -> $hit" >&2
            fail=1
        done < <(grep -rn "${sources[@]}" -E \
                     '#[[:space:]]*include[[:space:]]*[<"]Q|\b(QString|QObject|QWidget|QVariant|QByteArray|Q_OBJECT|qDebug)\b' \
                     "$dir" || true)
    fi
    while IFS= read -r hit; do
        echo "layering: reverse or lateral dependency out of /src/$mod -> $hit" >&2
        fail=1
    done < <(grep -rn "${sources[@]}" -E \
                 "#[[:space:]]*include[[:space:]]*\"kentos_cad/(${rule#*:})/" "$dir" || true)
done

# script.md R10/P4: a script binding never sees a raw Document or Layer handle.
while IFS= read -r hit; do
    echo "layering: raw Document/Layer handle in a /src/script header -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.hpp' --include='*.h' -E '\b(Document|Layer)[[:space:]]*[*&]' \
             "$root/src/script" 2>/dev/null || true)

if [[ $fail -eq 0 && -z "$checked" ]]; then echo "layering: skipped — every module above /src/core is empty in Phase 0"
elif [[ $fail -eq 0 ]]; then echo "layering: OK —${checked} obey the Article 3.2 graph${skipped:+ (empty in Phase 0:${skipped})}"
fi
exit $fail
