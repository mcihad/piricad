#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the design reference has exactly one tracked copy, and no drift.
#
# The reference — `design.md`, the mockup and the four screenshots — is DATA the
# program reads at run time (`data/design`) and the gates check against. Article
# 3.1 fixes the repository tree and `Screenshots/` is not in it, so that folder is
# the local drop box and `/data/design` is the tracked copy.
#
# The risk is obvious and this gate exists for it: an edit made to
# `Screenshots/design.md` — the file the reference arrived as — would never reach
# the build. When both copies are present they must be identical, byte for byte.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tracked="$root/data/design"
inbox="$root/Screenshots"
fail=0

if [[ ! -d "$tracked" ]]; then
    echo "design: skipped — /data/design is not in this tree"
    exit 0
fi

for want in design.md terracad-mockup.html ana_ekran.png stil.png bileşen_standardı.png; do
    if [[ ! -f "$tracked/$want" ]]; then
        echo "design: missing -> data/design/$want" >&2
        echo "design:   the gates and the shell read the reference from here." >&2
        fail=1
    fi
done

if [[ -d "$inbox" ]]; then
    while IFS= read -r -d '' file; do
        name="$(basename "$file")"
        # The mockup arrived under its export name and is tracked under a tidier
        # one; compare it by content rather than by filename.
        [[ "$name" == *.dc.html ]] && name="terracad-mockup.html"
        [[ -f "$tracked/$name" ]] || continue

        if ! cmp -s "$file" "$tracked/$name"; then
            echo "design: the drop copy and the tracked copy differ -> Screenshots/$(basename "$file")" >&2
            echo "design:   copy it over data/design/$name — only the tracked copy" >&2
            echo "design:   ships, and only the tracked copy is what the gates read." >&2
            fail=1
        fi
    done < <(find "$inbox" -maxdepth 1 -type f -print0)
fi

if [[ $fail -eq 0 ]]; then
    echo "design: OK — one tracked reference in data/design, no drift"
fi
exit $fail
