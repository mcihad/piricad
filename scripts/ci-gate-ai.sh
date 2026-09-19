#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the invariants the AI layer's rulebook promises but no compiler checks.
#
# Five claims, each of which has a way of quietly becoming false:
#
#   1. THE LICENCE SPLIT IS REAL. CLAUDE.md 2.1 puts the server component under
#      AGPLv3 while the rest of the tree is GPLv3-or-later. Two lists therefore
#      exist — the SPDX line in each file, and the list in /NOTICE that tells a
#      distributor which files those are — and two lists drift. This compares them.
#
#   2. NOBODY CAN FORGE AN APPROVAL. `.claude/ai.md` P1 forbids any claim standing
#      in for a decision, and P15 makes that structural: `ai::Gate::approve` is the
#      only factory for an `Approval` and it is called from exactly TWO places —
#      the suggestion card, where a person clicked, and the policy path, which
#      acts on permission that person gave beforehand. A THIRD caller anywhere is
#      the trust mode arriving by the back door.
#
#      THE COUNT WENT FROM ONE TO TWO ON 20 SEPTEMBER 2026, with §5.2.1 and
#      CLAUDE.md 5.7. It did NOT become "anyone may ask": the list below is
#      closed, and the policy path is on it because the policy is the user's and
#      nothing else can reach it (CLAUDE.md 5.23, `ai::escalates`).
#
#   3. THE CATALOGUE IS GENERATED. ai.md P7 bans a checked-in tool schema, and
#      CLAUDE.md 5.20 bans hand-editing the two generated documents. The deleted
#      projections (`Registry::ai_tool_schema`, `CommandSpec::to_schema`) must not
#      come back either.
#
#   4. THE AI LAYER STAYS SANS-IO. ai.md P10 keeps Qt out of /src/ai;
#      ci-gate-layering.sh checks the includes, and this checks the other half —
#      no socket, no file handle, no environment read in a module whose whole
#      testability rests on having none.
#
#   5. THE BLOCKLIST HOLDS. ai.md P4 lists the words an AI surface may never
#      render, because "onaylandı" beside a machine's output is a signature the
#      machine cannot give (§5.2.4).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

# ---- 1. the AGPL split -------------------------------------------------------
# The files that carry the AGPL line, found rather than assumed.
mapfile -t agpl_files < <(grep -rl "SPDX-License-Identifier: AGPL-3.0-or-later" \
                              "$root/src" "$root/tests" 2>/dev/null | sed "s|^$root/||" | sort)

if [[ ${#agpl_files[@]} -eq 0 ]]; then
    echo "ai: no AGPL-licensed file yet — the server component has not landed"
else
    for file in "${agpl_files[@]}"; do
        # THE NOTICE MUST NAME IT. A distributor reads /NOTICE to learn which
        # terms apply to which part of the program; a file whose licence is
        # stated only in its own header is a file whose licence nobody finds.
        if ! grep -qF "$file" "$root/NOTICE"; then
            echo "ai: AGPL file is not listed in /NOTICE -> $file:1" >&2
            fail=1
        fi
        # And it must not ALSO claim GPL: two SPDX lines in one file is a file
        # whose licence is a question rather than an answer.
        if grep -q "SPDX-License-Identifier: GPL-3.0-or-later" "$root/$file"; then
            echo "ai: file carries both an AGPL and a GPL SPDX line -> $file:1" >&2
            fail=1
        fi
    done

    # The other direction: /NOTICE naming a file that is not AGPL any more.
    while IFS= read -r listed; do
        [[ -f "$root/$listed" ]] || {
            echo "ai: /NOTICE lists an AGPL file that does not exist -> $listed:1" >&2
            fail=1
            continue
        }
        grep -q "SPDX-License-Identifier: AGPL-3.0-or-later" "$root/$listed" || {
            echo "ai: /NOTICE lists it as AGPL but the file does not say so -> $listed:1" >&2
            fail=1
        }
        # ONLY THE INDENTED LIST, not every path /NOTICE happens to mention: the
        # dependency entries below name source files too (the qpdf entry names
        # the file that calls it), and treating those as licence claims made the
        # first cut of this gate report four files that were never AGPL.
    done < <(grep -oE '^    (src|tests)/[a-zA-Z0-9_/.-]+\.(cpp|hpp)' "$root/NOTICE" \
                 | sed 's/^ *//' | sort -u)

    [[ -f "$root/LICENSES/AGPL-3.0-or-later.txt" ]] || {
        echo "ai: AGPL files exist but LICENSES/AGPL-3.0-or-later.txt does not -> NOTICE:1" >&2
        fail=1
    }
fi

# ---- 2. two callers for the approval factory, and no third ------------------
if [[ -f "$root/src/ai/src/gate.cpp" ]]; then
    # `Gate::approve` is the definition; a CALL is `.approve(` or `->approve(`
    # on a gate. Two callers are sanctioned and the list is CLOSED:
    #   · the suggestion card — a person clicked;
    #   · the policy path — the person gave permission beforehand, and only they
    #     could have (CLAUDE.md 5.23).
    mapfile -t callers < <(grep -rlnE '(\.|->)approve\(' "$root/src" "$root/tests" 2>/dev/null \
                               | sed "s|^$root/||" | sort -u)
    for caller in "${callers[@]}"; do
        case "$caller" in
            src/app/src/suggestion_card.cpp | src/ai/src/policy_path.cpp \
                | tests/unit/test_ai_*.cpp | src/ai/src/gate.cpp) ;;
            *)
                echo "ai: only the suggestion card and the policy path may ask for an" >&2
                echo "ai:   approval (ai.md P15) -> $caller:1" >&2
                fail=1
                ;;
        esac
    done
fi

# ---- 3. the catalogue is generated, not kept --------------------------------
while IFS= read -r hit; do
    echo "ai: a deleted catalogue projection is back (ai.md P7, R32) -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.cpp' --include='*.hpp' -E '\b(ai_tool_schema|to_schema)\s*\(' \
             "$root/src" "$root/tests" 2>/dev/null | grep -v '// ' || true)

for generated in docs/llms.txt docs/llms-full.txt; do
    [[ -f "$root/$generated" ]] || continue
    head -n1 "$root/$generated" | grep -q 'ÜRETİLMİŞ DOSYA' || {
        echo "ai: generated document lost its do-not-edit header -> $generated:1" >&2
        fail=1
    }
done

# A checked-in JSON tool catalogue is exactly what P7 bans.
while IFS= read -r suspect; do
    echo "ai: a checked-in tool catalogue duplicates the registry (ai.md P7) -> ${suspect#"$root/"}:1" >&2
    fail=1
done < <(find "$root/data" "$root/src/ai" -name '*tool*catalog*.json' -o -name '*tools*.json' \
             2>/dev/null || true)

# ---- 4. sans-IO ---------------------------------------------------------------
if [[ -d "$root/src/ai/src" ]]; then
    while IFS= read -r hit; do
        echo "ai: /src/ai must not do I/O — the transports live in /src/app (ai.md P10) -> $hit" >&2
        fail=1
    done < <(grep -rnE '#[[:space:]]*include[[:space:]]*<(fstream|filesystem|cstdio\.h|sys/socket\.h|netinet/)' \
                 "$root/src/ai" 2>/dev/null || true)
    while IFS= read -r hit; do
        echo "ai: /src/ai must not read the environment or open a socket -> $hit" >&2
        fail=1
        # COMMENTS ARE PROSE, not code. This module's headers explain at length
        # why they open no socket, and a grep that could not tell the
        # explanation from the thing being explained failed on its own rationale.
    done < <(grep -rnE '\b(getenv|::socket|::bind|::listen|::accept)[[:space:]]*\(' \
                 --include='*.cpp' --include='*.hpp' "$root/src/ai" 2>/dev/null \
                 | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|\*|/\*)' || true)
fi

# ---- 5. the P4 blocklist -----------------------------------------------------
# Only in the AI surfaces: the words are perfectly ordinary elsewhere (a print
# dialog's "onay" button, a database prompt), and it is beside a MODEL's output
# that they become a claim the model cannot make.
surfaces=("$root/src/ai")
for extra in "$root/src/app/src/chat_dock.cpp" "$root/src/app/src/suggestion_card.cpp" \
             "$root/src/app/include/kentos_cad/app/chat_dock.hpp" \
             "$root/src/app/include/kentos_cad/app/suggestion_card.hpp"; do
    [[ -f "$extra" ]] && surfaces+=("$extra")
done
while IFS= read -r hit; do
    echo "ai: a P4 blocklist word in an AI surface -> $hit" >&2
    fail=1
done < <(grep -rnE '"[^"]*(onaylandı|kontrol edildi|uygundur|mevzuata uygundur|imzalandı)' \
             "${surfaces[@]}" 2>/dev/null || true)

if [[ $fail -ne 0 ]]; then exit 1; fi

echo "ai: OK — licence split matches /NOTICE, one approval factory with two closed callers,"
echo "ai:   catalogue generated (no checked-in schema, no revived projection),"
echo "ai:   /src/ai does no I/O, and no P4 blocklist word reaches an AI surface"
