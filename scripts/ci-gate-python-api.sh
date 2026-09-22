#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every declared parameter carries its English name for the Python surface.
#
# WHY THIS GATE EXISTS.
#
# `kentos.cad` is PROJECTED from `Registry` (CLAUDE.md 5.10, 5.20): a command
# registered today is a Python callable today, with no second list to update. The
# one thing the projection cannot work out for itself is what to call a parameter
# in English — `noktalar` is `points`, and no rule of grammar gets you there.
#
# So `command::Param::english` is declared beside the Turkish name, at the
# declaration site, because only that site knows WHICH English word: `kenar` is a
# page MARGIN in `core.layout`, a measured DISTANCE in `geodesy.traverse` and an
# EDGE index in `islem.alan_duzenle`. A lookup table keyed by the Turkish word
# would have to answer once and be wrong twice.
#
# Without this gate the failure is silent and late: a new command lands, the
# projection has no keyword for one of its parameters, and the Python user meets
# it as a `TypeError` months later — or worse, the projection falls back to the
# Turkish word and the API is half-Turkish, which is the outcome §4.2 amended
# itself to avoid.
#
# WHAT IS CHECKED
#
#   Every `Param::<factory>("...")`, `Param{"..."` and `ToolParam::<factory>("...")`
#   declaration under /src, for a chained `.en("...")`. The chain may sit after
#   `.measured_in(...)` or `.renamed_from(...)`; only its presence is checked.
#
#   The English name itself is checked for shape, not for taste: lowercase ASCII
#   with underscores, and never a Python keyword — `cad.line(class=...)` is a
#   SyntaxError, not a bad name.
#
# HOW TO SATISFY IT
#
#   Chain it: `Param::point("merkez", "Dairenin merkezi").en("center")`.
#   Pick the word a Python programmer would reach for, and when the Turkish word
#   means two things in two commands, give each site its own word.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# `class`, `lambda`, `from`, `import`, `is`, `in`, `for`, `if`, `else`, `def`,
# `return`, `and`, `or`, `not`, `None`, `True`, `False`, `pass`, `with`, `as`,
# `global`, `del`, `try`, `raise`, `while`, `assert`, `yield`, `break`,
# `continue`, `elif`, `except`, `finally`, `nonlocal`, `async`, `await`.
readonly PY_KEYWORDS='^(False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield|match|case)$'

fail=0
declared=0

# The declarations, flattened: a parameter declaration may wrap over several
# lines, so the sources are joined and the pairs are read off the joined text.
while IFS= read -r hit; do
    file="${hit%%|*}"
    rest="${hit#*|}"
    name="${rest%%|*}"
    english="${rest#*|}"

    declared=$((declared + 1))

    if [[ -z "$english" ]]; then
        echo "python-api: '${name}' parametresinin İngilizce adı yok -> ${file}" >&2
        echo "python-api:   .en(\"...\") ile bildirin; kentos.cad anahtar kelimesi odur." >&2
        fail=1
        continue
    fi
    if [[ ! "$english" =~ ^[a-z][a-z0-9_]*$ ]]; then
        echo "python-api: '${english}' geçerli bir Python anahtar kelimesi değil -> ${file} (${name})" >&2
        echo "python-api:   küçük harf ASCII ve alt çizgi; rakamla başlamaz." >&2
        fail=1
        continue
    fi
    if [[ "$english" =~ $PY_KEYWORDS ]]; then
        echo "python-api: '${english}' Python'un ayrılmış sözcüğü -> ${file} (${name})" >&2
        echo "python-api:   cad.komut(${english}=...) bir SyntaxError olur; başka bir ad seçin." >&2
        fail=1
    fi
done < <(
    python3 - <<'PY'
import io, os, re, sys

FACTORY = re.compile(r'\b(?:Tool)?Param::(?:points|point|number|integer|integer_optional|integer_range'
                     r'|text|boolean|choice|object)\(\s*"([^"]+)"')
BRACED  = re.compile(r'\bParam\{\s*"([^"]+)"')
CHAIN   = re.compile(r'\s*\.\s*(measured_in|renamed_from|en)\s*\(')

def strip_comments(src):
    """Blanks out comments, keeping every byte offset.

    A COMMENT IS NOT A DECLARATION. This rule is explained in prose inside the
    very header it guards — `spec.hpp` shows `Param::text("yerlesim", ...)` as
    the example of how to chain a name — and a gate that could not tell an
    example from a declaration would forbid explaining itself.
    """
    out = list(src)
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            quote = c
            i += 1
            while i < n:
                if src[i] == '\\':
                    i += 2
                    continue
                if src[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            while i < n and src[i] != '\n':
                out[i] = ' '
                i += 1
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            while i < n and not (src[i] == '*' and i + 1 < n and src[i + 1] == '/'):
                if src[i] != '\n':
                    out[i] = ' '
                i += 1
            for k in range(i, min(i + 2, n)):
                out[k] = ' '
            i += 2
            continue
        i += 1
    return ''.join(out)


def close(s, i):
    depth = 0
    in_str = False
    while i < len(s):
        c = s[i]
        if in_str:
            if c == '\\':
                i += 2
                continue
            if c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c in '({':
                depth += 1
            elif c in ')}':
                depth -= 1
                if depth == 0:
                    return i
        i += 1
    return -1

for dirpath, _, names in os.walk('src'):
    for fn in sorted(names):
        if not fn.endswith(('.cpp', '.hpp')):
            continue
        path = os.path.join(dirpath, fn)
        s = strip_comments(io.open(path, encoding='utf-8').read())
        for pat, opener in ((FACTORY, '('), (BRACED, '{')):
            for m in pat.finditer(s):
                name = m.group(1)
                op = s.index(opener, m.start())
                cp = close(s, op)
                if cp < 0:
                    continue
                end = cp + 1
                english = ''
                while True:
                    cm = CHAIN.match(s, end)
                    if not cm:
                        break
                    co = s.index('(', cm.end() - 1)
                    ce = close(s, co)
                    if ce < 0:
                        break
                    if cm.group(1) == 'en':
                        q = re.match(r'\(\s*"([^"]*)"', s[co:ce + 1])
                        english = q.group(1) if q else '?'
                    end = ce + 1
                print('%s|%s|%s' % (path, name, english))
PY
)

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "python-api: OK — ${declared} parametre bildirimi, hepsinin İngilizce adı var"
