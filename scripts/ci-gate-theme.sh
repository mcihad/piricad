#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: one stylesheet, one palette, and Fusion on every platform.
#
# `design.md` §12: `QApplication::setStyle("Fusion")` plus a single stylesheet,
# with no native theme inherited anywhere. That is what makes Windows, macOS and
# Linux draw the same program, and it only holds if it holds EVERYWHERE — a single
# widget left to the platform style is a single widget that looks like three
# different applications.
#
# THREE THINGS ARE CHECKED.
#
#   1. Fusion is forced. Without it Qt takes the desktop's own style.
#   2. No widget outside `theme.cpp` calls `setStyleSheet`. The settings window
#      and the style designer each used to carry their own, with their own greys
#      and their own radii, and matched neither each other nor the shell.
#   3. No colour literal outside `tokens.cpp`. A `#1C2024` written where it is
#      used is a colour nobody can find and nobody will update.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/src/app"
fail=0

if [[ ! -d "$app" ]]; then
    echo "theme: skipped — /src/app is not in this tree"
    exit 0
fi

# ---- 1. Fusion --------------------------------------------------------------
# Fusion may be installed bare or wrapped in the shell's QProxyStyle; both count,
# and nothing else does — a build that reaches Qt's platform default is a build
# that looks different on every desktop.
if ! grep -rq 'QStyleFactory::create(QStringLiteral("Fusion"))\|setStyle(QStringLiteral("Fusion"))' "$app/src"; then
    echo "theme: the Fusion style is not forced -> src/app/src/main.cpp:1" >&2
    echo "theme:   without it every platform inherits its own theme and the" >&2
    echo "theme:   same build looks like three different applications." >&2
    fail=1
fi

# ---- 2. one stylesheet ------------------------------------------------------
while IFS= read -r hit; do
    file="${hit%%:*}"
    case "$file" in
        */theme.cpp) continue ;;                      # the one sheet lives here
        */main_window.cpp) continue ;;                # ...and is applied here
    esac
    echo "theme: $(basename "$file") sets its own stylesheet -> ${file#$root/}:${hit#*:}" >&2
    echo "theme:   there is one sheet (theme.cpp). Ask for a role by object name" >&2
    echo "theme:   instead — sectionTitle, quiet, mono, toolBoxBody." >&2
    fail=1
done < <(grep -rn "setStyleSheet" "$app/src" | cut -d: -f1,2)

# ---- 3. colours live in tokens.cpp ------------------------------------------
while IFS= read -r hit; do
    file="${hit%%:*}"
    [[ "$(basename "$file")" == "tokens.cpp" ]] && continue
    echo "theme: $(basename "$file") writes a colour literal -> ${file#$root/}:${hit#*:}" >&2
    echo "theme:   every colour comes from tokens.hpp; see design.md §2." >&2
    fail=1
done < <(grep -rnE "QColor\(0x[0-9A-Fa-f]{2}|#[0-9A-Fa-f]{6}\b" "$app/src" "$app/include" |
         grep -viE "ui-label|design\.md|//" | cut -d: -f1,2)

if [[ $fail -eq 0 ]]; then
    echo "theme: OK — Fusion forced, one stylesheet, every colour from tokens.hpp"
fi
exit $fail
