#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: no command body branches on WHICH CLIENT is asking.
#
# WHY THIS GATE EXISTS.
#
# Article 1.2 says the GUI, the command line, a script, the AI, a plugin, a batch
# job and a test are EQUAL clients of the command bus: none gets a privilege, a
# fast path or a private entry point. The way that rule dies is not by a
# declaration — it dies one `if` at a time, in a command body that peeks at where
# the input came from and does something slightly different. Each such `if` is
# reasonable on its own ("the GUI already asked, so skip the prompt") and together
# they are two programs wearing one name, and only one of them is tested.
#
# `.claude/command.md` P10 states it for the body: a command MUST NOT read
# `InputSource`, and the coroutine input model is what makes that possible —
# `co_await ctx.point(...)` is answered by a click, a typed coordinate or a JSON
# value through the same awaiter, so a body has no reason to ask.
#
# WHAT DECIDES BEHAVIOUR INSTEAD is the EMPTINESS OF AN ARGUMENT. `POLİGON` asks
# for its readings when `aci=` was not supplied and does not when it was; that is
# not "am I interactive", it is "was I told". A script that supplied everything is
# never asked anything, and the same body serves both without knowing which is
# which.
#
# WHAT IS CHECKED
#
#   Command BODIES only — `/src/command/src/commands`, `/src/domain/*/src` and
#   `/src/ai/src/commands` — for `InputSource`, `input().origin()` and a literal
#   `Origin::` comparison. The BUS may read the origin and does: it journals it
#   (`Bus::journal_entry`) and the host names it when it dispatches. That is the
#   record saying who asked, which is the opposite of behaviour changing because
#   of who asked.
#
# HOW TO SATISFY IT
#
#   Branch on the argument, not on the client. If a body genuinely needs to know,
#   the design is wrong at a level a gate cannot fix — say so in the PR and amend
#   `command.md` P10 rather than working around this script.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

dirs=(src/command/src/commands src/ai/src/commands)
for d in src/domain/*/src; do
    [[ -d "$d" ]] && dirs+=("$d")
done

fail=0
for dir in "${dirs[@]}"; do
    while IFS= read -r line; do
        file="${line%%:*}"
        rest="${line#*:}"
        lineno="${rest%%:*}"
        text="${rest#*:}"

        # A COMMENT IS NOT CODE. These rules are explained in prose all over this
        # tree — including in the files this gate guards — and a gate that could
        # not tell an explanation from a violation would forbid explaining itself.
        trimmed="${text#"${text%%[![:space:]]*}"}"
        case "$trimmed" in
            '//'*|'/*'*|'*'*) continue ;;
        esac

        echo "tek-yol: komut gövdesi istemciye bakıyor -> ${file#"$root"/}:${lineno}  ${text}" >&2
        echo "tek-yol:   Article 1.2 / command.md P10: davranış ARGÜMANIN BOŞLUĞUNA bakar," >&2
        echo "tek-yol:   istemciye değil. Bir betiğe hiçbir şey sorulmaz çünkü her şeyi" >&2
        echo "tek-yol:   verdi, bir ele sorulur çünkü vermedi — ikisi de aynı gövde." >&2
        fail=1
    done < <(grep -rn -E 'InputSource|input\(\)\.origin\(\)|Origin::[A-Za-z]+ *==|== *Origin::' \
                 "$dir" 2>/dev/null || true)
done

if [[ $fail -ne 0 ]]; then
    exit 1
fi

counted=$(find "${dirs[@]}" -name '*.cpp' 2>/dev/null | wc -l | tr -d ' ')
echo "tek-yol: OK — ${counted} komut gövdesi, hiçbiri hangi istemcinin sorduğuna bakmıyor"
