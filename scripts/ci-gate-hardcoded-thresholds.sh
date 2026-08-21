#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: no regulatory value hard-coded in C++.
# piricad.md §15: "Mevzuat sık değişiyor -> Veri odaklı katalog mimarisi" — a
# legislation update must be a data package swap, never a rebuild (§12, §8).
# Constitution 5.13, .claude/domain.md R1/P1, .claude/data.md P1: every TAKS/KAKS
# row, gösterim, detay kodu, çekme mesafesi, asgari/azami threshold and TUCBS
# theme id lives under /data/catalogs; C++ only interprets it.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

# The legislation vocabulary — extend this one list as the catalogue grows.
# Matched case-insensitively anywhere in an identifier (kMaxTaks, taks_limit,
# TAKS_ORANI); a token that also occurs inside an English word carries its own
# left boundary, as DOP does against "adopt".
tokens='TAKS|KAKS|MPYY|TUCBS|ASGARI|AZAMI|MADDE|DETAY|EK-1|(^|[^[:alnum:]_])DOP'
tokens+='|B(Ö|ö|O|o)HHB(Ü|ü|U|u)Y|G(Ö|ö|O|o)STERIM|(Ç|ç|C|c)EKME[ _]?MESAFES'
tokens+='|TEMA[ _]?ID|IMAR[ _]?Y(Ö|ö|O|o)N|PLANLI[ _]?ALAN'
regulatory="(${tokens})"
# What "hard-coded" looks like: numeric literal, constexpr, enum entry, string table.
literal='(^|[^[:alnum:]_])[-+]?[0-9]+(\.[0-9]+)?|\bconstexpr\b|\benum\b|"'

scanned=()
for dir in src/core src/command src/domain src/ai src/app; do
    if [[ -z "$(find "$root/$dir" -name '*.[ch]pp' -o -name '*.h' 2>/dev/null)" ]]; then
        echo "hardcoded-thresholds: skipped — /$dir is empty in Phase 0"
        continue
    fi
    scanned+=("/$dir")
    while IFS= read -r hit; do
        code="${hit#*:*:}"
        code="${code%%//*}"   # a comment is allowed to name the regulation
        if grep -qE '#[[:space:]]*include' <<<"$code"; then continue; fi
        # .claude/domain.md: an annotated catalogue lookup key is the sanctioned form.
        if grep -q 'catalog-key' <<<"$hit"; then continue; fi
        if ! grep -qiE "$regulatory" <<<"$code"; then continue; fi
        if grep -qE "$literal" <<<"$code"; then
            echo "hardcoded-thresholds: regulatory value baked into C++ -> $hit" >&2
            fail=1
        fi
    done < <(grep -rniE "$regulatory" --include='*.cpp' --include='*.hpp' --include='*.h' \
                 "$root/$dir" || true)
done

if [[ $fail -eq 0 && ${#scanned[@]} -gt 0 ]]; then
    echo "hardcoded-thresholds: OK — no regulatory literal, constexpr, enum or string table in ${scanned[*]}"
fi
exit $fail
