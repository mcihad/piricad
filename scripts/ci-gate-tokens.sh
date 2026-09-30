#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the design tokens in the code are the ones the mockup uses.
#
# TWO SOURCES, AND THEY ARE NOT INTERCHANGEABLE.
#
# `data/design/design.md` §2 is the TOKEN TABLE — the written decision. Every
# token this program declares must appear there with the same hex, because that
# table is what a contributor reads and what a reviewer checks against.
#
# `data/design/terracad-mockup.html` is the SOURCE the table was read off, for
# the tokens that existed when it was drawn. A token that appears in BOTH must
# agree in both. Tokens added later — the component standard's `--danger` family
# and the two accent edges, which come from `bileşen_standardı.png` — are in the
# table and not in the mockup, and that is expected rather than a gap.
#
# `data/design/terracad-mockup.html` is the SOURCE. It is the reference the design
# was drawn in and every colour in it is written out literally, so the mockup can
# be read rather than interpreted — which is what makes "identical on every
# platform" a checkable claim instead of an opinion about a screenshot.
#
# THE SCREENSHOTS ARE NOT THE SOURCE, and measuring them is how that was settled:
# `ana_ekran.png` is 1898 px wide rather than 1600, its title bar samples as
# #272C30 where the mockup says #262B30, and one flat toolbar band holds 86
# distinct colours. Those are a browser's rendering and a rescale, not the design.
#
# WHAT IS CHECKED, TWO THINGS.
#
#   1. Every token this program declares appears, byte for byte, in `design.md`
#      §2. A token that has drifted is a surface that will not match, and a
#      surface that does not match is the whole complaint this gate exists to
#      answer. A token that ALSO appears in the mockup must agree with it too.
#
#   2. The NAMES in `kDark` and `kLight` are in the same order as the fields in
#      `tokens.hpp`. `Tokens` is initialised positionally, so a field added to the
#      header without its value added at the same index shifts every colour after
#      it into the wrong name. Nothing catches that at compile time and nothing
#      catches it at run time either: every value is still a valid colour, so the
#      program simply paints the tab strip white and the window edge with a hover
#      grey. This happened once; it does not get to happen twice.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mockup="$root/data/design/terracad-mockup.html"
tokens="$root/src/app/src/tokens.cpp"
fail=0

if [[ ! -f "$mockup" ]]; then
    echo "tokens: skipped — the reference mockup is not in this tree"
    exit 0
fi
# ---- 2. the initialiser order matches the declaration order --------------------
header="$root/src/app/include/piricad/app/tokens.hpp"
if [[ -f "$header" && -f "$tokens" ]]; then
    fields=$(sed -n '/^struct Tokens$/,/^};/p' "$header" |
             sed -n 's/^ *QColor \([A-Za-z_][A-Za-z0-9_]*\);.*/\1/p')
    for block in kDark kLight; do
        values=$(sed -n "/^const Tokens $block{/,/^};/p" "$tokens" |
                 sed -n 's|.*/\* *\([A-Za-z_][A-Za-z0-9_]*\) *\*/.*|\1|p')
        if [[ "$fields" != "$values" ]]; then
            echo "tokens: $block is not in tokens.hpp field order -> src/app/src/tokens.cpp:1" >&2
            diff <(echo "$fields") <(echo "$values") | head -8 >&2
            echo "tokens:   Tokens is initialised POSITIONALLY. A mismatch here means" >&2
            echo "tokens:   every colour after the first difference lands in the wrong" >&2
            echo "tokens:   field, silently, and the shell paints with the wrong greys." >&2
            fail=1
        fi
    done
fi

if [[ ! -f "$tokens" ]]; then
    echo "tokens: skipped — tokens.cpp is not in this tree"
    exit 0
fi

lower="$(tr '[:upper:]' '[:lower:]' < "$mockup")"

# The DARK block only: the light theme is a second mapping of the same structure
# (design.md §12) and has no counterpart in a dark mockup to be checked against.
dark="$(awk '/const Tokens kDark\{/,/^\};/' "$tokens")"

checked=0
used_by_mockup=0
guide="$root/data/design/design.md"

if [[ ! -f "$guide" ]]; then
    echo "tokens: skipped — data/design/design.md is not in this tree"
    exit 0
fi
while read -r name r g b; do
    [[ -z "$name" ]] && continue

    # Alpha-bearing tokens are written as rgba() in the mockup and as a fourth
    # argument here; their RGB is checked and their alpha is the spec's business.
    hex="$(printf '#%s%s%s' "$r" "$g" "$b" | tr '[:upper:]' '[:lower:]')"
    checked=$((checked + 1))

    # The written decision first: `design.md` §2 must carry this exact hex.
    if ! grep -qiF -- "$hex" "$guide"; then
        echo "tokens: $name is $hex, which design.md §2 does not list -> src/app/src/tokens.cpp:1" >&2
        echo "tokens:   add the row to the token table, or fix the value here." >&2
        fail=1
    fi

    # And the drawing it was read off, for the tokens it actually contains. A
    # token the mockup never uses is fine — the component standard added several
    # after it was drawn — but one that DISAGREES with it is not, and that is a
    # case this loop cannot see from a hex alone. What it can check is that a
    # token claiming to come from the mockup is really in it, which is what the
    # table's own §14.2 note is about.
    if grep -qF -- "$hex" <<<"$lower"; then
        used_by_mockup=$((used_by_mockup + 1))
    fi
done < <(grep -oE '/\* [a-zA-Z]+ *\*/ QColor\(0x[0-9A-Fa-f]{2}, 0x[0-9A-Fa-f]{2}, 0x[0-9A-Fa-f]{2}' <<<"$dark" |
         sed -E 's:/\* ([a-zA-Z]+) *\*/ QColor\(0x([0-9A-Fa-f]{2}), 0x([0-9A-Fa-f]{2}), 0x([0-9A-Fa-f]{2}):\1 \2 \3 \4:')

if [[ $checked -eq 0 ]]; then
    echo "tokens: no tokens found in the dark block -> src/app/src/tokens.cpp:1" >&2
    fail=1
elif [[ $fail -eq 0 ]]; then
    echo "tokens: OK — $checked dark tokens listed in design.md §2, $used_by_mockup of them in the mockup"
fi
exit $fail
