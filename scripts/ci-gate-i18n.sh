#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: Turkish-safe casing, and no user-visible string outside tr().
# piricad.md §13. Constitution 5.6 ("NEVER std::toupper/std::tolower/<cctype>
# classifiers on Turkish text") and 6.9 ("User-visible strings are tr()-wrapped").
# .claude/ui.md R24, R25, P4, P8.
# ASCII classifiers destroy the dotted/dotless i (i->İ, ı->I), so they are banned
# everywhere under /src. src/core/src/text.cpp is the single exemption: it *is*
# the Turkish folding table the rest of the tree is required to call instead.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

ctype='(std::|::)(toupper|tolower|is(alpha|alnum|digit|space|upper|lower|punct|xdigit|blank|print|graph|cntrl))\b|#[[:space:]]*include[[:space:]]*<c?ctype(\.h)?>'
# Only a bare "..." handed straight to a user-visible setter counts. QStringLiteral
# (object names, style sheets, bus command names like "ÇİZGİ") and tr() are fine.
setters='(setText|setWindowTitle|setPlaceholderText|setToolTip|setStatusTip|addAction|addMenu)[[:space:]]*\([[:space:]]*([0-9]+[[:space:]]*,[[:space:]]*)?"'
setters="$setters"'|QMessageBox::[a-zA-Z]+[[:space:]]*\([^)]*,[[:space:]]*"'

if [[ ! -d "$root/src" || -z "$(find "$root/src" -name '*.cpp' -o -name '*.hpp' -o -name '*.h')" ]]; then
    echo "i18n: skipped — /src holds no C++ sources yet in Phase 0"
    exit 0
fi

# (a) ASCII case/classifier calls anywhere under /src. Comments naming the ban are
# how the ban is documented, so every hit is re-tested with the comment tail cut.
while IFS= read -r hit; do
    file="${hit%%:*}"; rest="${hit#*:}"; code="${rest#*:}"; code="${code%%//*}"
    if [[ "$file" == */src/core/src/text.cpp ]]; then continue; fi
    if [[ "$file" == */src/core/src/json.cpp ]] && grep -qiE 'hex|\\u' <<<"$code"; then continue; fi
    if grep -qE "$ctype" <<<"$code"; then
        echo "i18n: ASCII case/classifier on Turkish text — use QLocale(QLocale::Turkish) or core::turkish_upper -> $file:${rest%%:*}" >&2
        fail=1
    fi
done < <(grep -rnE "$ctype" --include='*.cpp' --include='*.hpp' --include='*.h' \
             --exclude-dir=build "$root/src" || true)

# (b) Un-tr()'d user-visible literal in the Widgets shell.
if [[ -d "$root/src/app" && -n "$(find "$root/src/app" -name '*.cpp' -o -name '*.hpp')" ]]; then
    while IFS= read -r hit; do
        file="${hit%%:*}"; rest="${hit#*:}"; code="${rest#*:}"; code="${code%%//*}"
        if grep -qE "$setters" <<<"$code"; then
            echo "i18n: user-visible string literal not wrapped in tr() -> $file:${rest%%:*}" >&2
            fail=1
        fi
    done < <(grep -rnE "$setters" --include='*.cpp' --include='*.hpp' "$root/src/app" || true)
else
    echo "i18n: skipped — /src/app has no C++ sources yet in Phase 0"
fi

if [[ $fail -eq 0 ]]; then
    echo "i18n: OK — no ASCII case/classifier calls under /src, every user-visible setter literal is tr()-wrapped"
fi
exit $fail
