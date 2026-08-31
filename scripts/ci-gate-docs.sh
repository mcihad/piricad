#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: everything a user can do is documented in /docs — the command system included.
# CLAUDE.md Article 11 and 5.16/5.17, .claude/docs.md R1–R17.
# piricad.md §13 lists user documentation published from CI as a world-standard
# acceptance criterion; §2.3 forbids a second, hand-maintained command list.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
docs="$root/docs"
fail=0

# The eight sections every command page must carry, in .claude/docs.md R7 order.
sections=("Ne yapar" "Adlar" "Sözdizimi" "Parametreler" "Örnekler" "Geri alma"
          "Betikten kullanım" "Hatalar")

if [[ ! -d "$docs" ]]; then
    echo "docs: /docs is missing entirely" >&2
    exit 1
fi

# ---- R4: the index links every page ------------------------------------------
index="$docs/README.md"
if [[ ! -f "$index" ]]; then
    echo "docs: docs/README.md index is missing -> docs/README.md:1" >&2
    fail=1
fi

while IFS= read -r page; do
    rel="${page#"$docs"/}"
    [[ "$rel" == "README.md" ]] && continue
    if ! grep -qF -- "$rel" "$index" 2>/dev/null; then
        echo "docs: page not linked from the index -> docs/$rel:1" >&2
        fail=1
    fi
done < <(find "$docs" -name '*.md' -type f | sort)

# ---- R5: every page opens with a level-1 heading -----------------------------
while IFS= read -r page; do
    if ! grep -qE '^# .' "$page"; then
        echo "docs: page has no level-1 heading -> ${page#"$root"/}:1" >&2
        fail=1
    fi
done < <(find "$docs" -name '*.md' -type f | sort)

# ---- P7: no placeholders in a merged page ------------------------------------
while IFS= read -r hit; do
    echo "docs: placeholder left in a merged page -> ${hit#"$root"/}" >&2
    fail=1
done < <(grep -rnE '\b(TODO|TBD|XXX|FIXME|Lorem ipsum)\b' "$docs" --include='*.md' || true)

# ---- R6/R7/R8: a page per registered command, with all eight sections ---------
# Bash 3.2 has no `mapfile` (it arrived in 4.0) and macOS ships 3.2 as
# /bin/bash, which is what `env bash` finds there. Read the list instead.
ids=()
while IFS= read -r line; do ids+=("$line"); done \
    < <(grep -rhoE '\.id[[:space:]]*=[[:space:]]*"[a-z0-9_.]+"' \
            "$root/src/command/src/commands" | grep -oE '"[^"]+"' | tr -d '"' | sort -u)

if [[ ${#ids[@]} -eq 0 ]]; then
    echo "docs: no command ids found under src/command/src/commands — gate cannot verify coverage" >&2
    fail=1
fi

for id in "${ids[@]}"; do
    slug="${id##*.}"
    page="$docs/komutlar/$slug.md"

    if [[ ! -f "$page" ]]; then
        echo "docs: command '$id' has no page -> docs/komutlar/$slug.md:1  (CLAUDE.md 11.4)" >&2
        fail=1
        continue
    fi
    for section in "${sections[@]}"; do
        if ! grep -qF -- "$section" "$page"; then
            echo "docs: command page is missing the '$section' section -> docs/komutlar/$slug.md:1" >&2
            fail=1
        fi
    done
    # R8: all three clients must appear — command line, GUI, script.
    for client in "Komut satırı" "Arayüz" "Betik"; do
        if ! grep -qF -- "$client" "$page"; then
            echo "docs: command page does not show the '$client' path -> docs/komutlar/$slug.md:1" >&2
            fail=1
        fi
    done
done

# ---- R9/P2: the generated reference must match the registry ------------------
reference="$docs/komutlar/referans.md"
if [[ ! -f "$reference" ]]; then
    echo "docs: generated reference missing -> docs/komutlar/referans.md:1  (run: make reference)" >&2
    fail=1
elif ! head -n1 "$reference" | grep -q 'ÜRETİLMİŞ DOSYA'; then
    echo "docs: generated reference lost its do-not-edit header -> docs/komutlar/referans.md:1" >&2
    fail=1
else
    docgen=""
    for candidate in "$root"/build/*/bin/piricad_docgen; do
        [[ -x "$candidate" ]] && docgen="$candidate" && break
    done
    if [[ -n "$docgen" ]]; then
        tmp="$(mktemp)"
        trap 'rm -f "$tmp"' EXIT
        if "$docgen" "$tmp" >/dev/null 2>&1 && ! diff -q "$reference" "$tmp" >/dev/null; then
            echo "docs: docs/komutlar/referans.md is stale or hand-edited -> docs/komutlar/referans.md:1  (run: make reference)" >&2
            fail=1
        fi
    else
        echo "docs: note — piricad_docgen is not built, so referans.md freshness was not verified"
    fi
    # Every command in the reference must resolve to a page.
    while IFS= read -r slug; do
        [[ -f "$docs/komutlar/$slug.md" ]] || {
            echo "docs: reference links a page that does not exist -> docs/komutlar/$slug.md:1" >&2
            fail=1
        }
    done < <(grep -oE '\]\([a-z0-9_-]+\.md\)' "$reference" \
                 | sed -E 's/^\]\(//; s/\.md\)$//' | sort -u || true)
fi

# ---- P10: no dead relative link ----------------------------------------------
while IFS= read -r page; do
    dir="$(dirname "$page")"
    while IFS= read -r target; do
        target="${target%%#*}"
        [[ -z "$target" ]] && continue
        [[ "$target" =~ ^(https?:|mailto:) ]] && continue
        if [[ "$target" == /* ]]; then
            resolved="$root${target}"
        else
            resolved="$dir/$target"
        fi
        if [[ ! -e "$resolved" ]]; then
            echo "docs: dead link '$target' -> ${page#"$root"/}:$(grep -nF "($target)" "$page" | head -n1 | cut -d: -f1)" >&2
            fail=1
        fi
    done < <(grep -oE '\]\([^)]+\)' "$page" | sed -E 's/^\]\(//; s/\)$//' | sort -u)
done < <(find "$docs" -name '*.md' -type f | sort)

if [[ $fail -eq 0 ]]; then
    echo "docs: OK — ${#ids[@]} komut belgelendi, dizin eksiksiz, ölü bağlantı yok, referans üretilmiş halde"
fi
exit $fail
