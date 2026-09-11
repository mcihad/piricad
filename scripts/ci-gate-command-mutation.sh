#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: nothing mutates the Document outside a Transaction.
# kentoscad.md §8: "/src/domain içinde doğrudan geometri mutasyonu (komut dışı)
# görülürse build kırılsın." Constitution Article 1 extends this to every module
# above /src/command: only Transaction may call a Document mutator.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

# The Document's primitive mutators. Only src/core (the definitions) and
# src/command/src/transaction.cpp (the sanctioned wrapper) may name them.
mutators='add_polyline|set_entity_alive|set_layer_visible|set_layer_locked|set_layer_style|set_crs'

for dir in src/domain src/io src/processing src/render src/script src/ai src/app; do
    [[ -d "$root/$dir" ]] || continue
    while IFS= read -r hit; do
        # A call through a Transaction object is exactly what is required.
        if grep -qE '(transaction\(\)|tx_?\.|tx->)' <<<"$hit"; then continue; fi
        echo "command-mutation: direct Document mutation outside a Transaction -> $hit" >&2
        fail=1
    done < <(grep -rn --include='*.cpp' --include='*.hpp' \
                 -E "(document\(\)|doc_?)\.($mutators)\(" "$root/$dir" || true)
done

# Nothing outside src/core and src/command may name Document::apply, which is the
# undo/rollback primitive.
while IFS= read -r hit; do
    echo "command-mutation: Document::apply called outside core/command -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.cpp' --include='*.hpp' -E '\.apply\(\s*op' \
             "$root/src/domain" "$root/src/io" "$root/src/render" "$root/src/script" \
             "$root/src/ai" "$root/src/app" 2>/dev/null || true)

if [[ $fail -eq 0 ]]; then
    echo "command-mutation: OK — every mutation goes through a Transaction"
fi
exit $fail
