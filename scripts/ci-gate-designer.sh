#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the style designer offers every property its renderer actually reads.
#
# THE DEFECT THIS EXISTS FOR. The designer decides which property rows to show
# from a table of layer types written by hand beside the rows; the backend decides
# which properties to read from its own switch. Nothing kept the two in step and
# they drifted: `gorsel-dolgu` rotates its brush by `angle_udeg` and the dialog
# offered no angle row for it, so a value the renderer reads was unreachable — the
# drawing could hold a rotation the user could neither see nor change. The user
# saw it as the form "going wrong" after picking a gallery row.
#
# WHAT IS CHECKED, and how. Each row of the table below names one property, the
# designer list that governs it, and the layer types the BACKEND reads it for. The
# third column is the only thing restated here, and it is restated because a
# reliable way to derive it from C++ needs a C++ parser — which is a bigger thing
# than the drift it would catch. It is short, it sits beside the reason, and it
# fails loudly when the designer's list stops covering it.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
designer="$root/src/app/src/style_designer.cpp"
fail=0

if [[ ! -f "$designer" ]]; then
    echo "designer: skipped — style_designer.cpp is not in this tree"
    exit 0
fi

# property | designer list | types whose draw path reads it
checks=(
    "angle_udeg|angled|MarkerLine HashLine LinePatternFill PointPatternFill SimpleMarker RasterFill RasterLine RasterMarker CentroidFill"
    "interval_px|spaced|MarkerLine HashLine LinePatternFill PointPatternFill RasterLine"
    "phase_px|phased|MarkerLine HashLine RasterLine"
    "size_px|sized|MarkerLine HashLine PointPatternFill CentroidFill SimpleMarker RasterFill RasterMarker RasterLine TextMarker"
)

# THE SYMBOL PARAMETERS, AND WHAT IS BEING GUARDED NOW.
#
# A symbol layer's `bindings` name the attribute columns the layer takes from the
# object. This gate used to require a ROW for them in the dialog. That row has
# been removed on purpose: it wrote the bindings into the in-memory symbol and
# the preview redrew, but `applyToDocument` emits one `STİL` per symbol layer and
# never emitted `alan=` — so the parameters reached the preview and never reached
# the document. A control that reports success and changes nothing is worse than
# no control, and a colon-separated line was a programmer's answer to a
# plan-maker's question besides. The replacement is being designed.
#
# So what is guarded is the other half, and it is the half that matters: the
# CAPABILITY must still exist while the dialog has no row for it. `STİL` declares
# `alan`, `ETİKET` reads the text bindings, and the document model carries them.
# If any of those goes, the feature has been deleted rather than postponed — and
# a postponed row with no command behind it is just a deletion nobody announced.
declares_alan="$root/src/command/src/commands/style.cpp"
if ! grep -q '"alan"' "$declares_alan"; then
    echo "designer: the parameter row was removed from the dialog on the promise that" >&2
    echo "designer:   STİL alan= still declares symbol parameters, and it no longer does" >&2
    echo "designer:   -> ${declares_alan#$root/}:1" >&2
    fail=1
fi

if ! grep -q 'bindings' "$root/src/core/include/kentos_cad/core/style.hpp"; then
    echo "designer: SymbolLayer no longer carries bindings -> src/core/include/kentos_cad/core/style.hpp:1" >&2
    fail=1
fi

for row in "${checks[@]}"; do
    IFS='|' read -r field list types <<<"$row"

    declared="$(awk "/const std::vector<T> ${list}\{/,/\};/" "$designer" |
                grep -oE "T::[A-Za-z]+" | sed 's/T:://' | sort -u)"

    if [[ -z "$declared" ]]; then
        echo "designer: list '$list' not found -> ${designer#$root/}:1" >&2
        fail=1
        continue
    fi

    for t in $types; do
        if ! grep -qx -- "$t" <<<"$declared"; then
            echo "designer: $t is drawn using $field but '$list' does not list it -> ${designer#$root/}:1" >&2
            echo "designer:   the renderer reads a property the dialog cannot set." >&2
            fail=1
        fi
    done
done

if [[ $fail -eq 0 ]]; then
    echo "designer: OK — every property the backend reads is reachable from the dialog"
fi
exit $fail
