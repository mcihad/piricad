#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# THE TURKISH EVALUATION SET: how big it is, and how far that is from the target.
#
# CLAUDE.md Article 8.9 allows the set to be a starter rather than the 200–300
# cases `.claude/ai.md` R21 asks for — on one condition, stated in the article
# itself: "`make check` reports the case count so the shortfall is VISIBLE rather
# than assumed."
#
# That condition was not met. There was no set, no harness and no count; the
# article described machinery the tree did not have. This gate is the half that
# makes the sentence true. It does NOT fail on a small set — failing would only
# get the gate disabled, and the deviation is sanctioned. It fails when the set
# is missing, unreadable, or has been emptied, because those are the states the
# article does not cover.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
set_file="$root/tests/ai-eval/senaryolar.json"

if [[ ! -f "$set_file" ]]; then
    echo "eval: değerlendirme seti yok -> tests/ai-eval/senaryolar.json:1" >&2
    exit 1
fi

read -r count terms < <(python3 - "$set_file" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    data = json.load(f)
cases = data.get("senaryolar", [])
terms = set()
for one in cases:
    terms.update(one.get("terimler", []))
print(len(cases), len(terms))
PY
)

if [[ "$count" -lt 10 ]]; then
    echo "eval: set boşaltılmış ($count vaka) -> tests/ai-eval/senaryolar.json:1" >&2
    exit 1
fi

# THE TARGET AND THE GAP, in one line, every run. `ai.md` R21 asks for 200–300
# real Turkish requests paired with their expected command sequences; R22 lists
# the domain vocabulary they must cover.
target=200
short=$((target - count))
if [[ "$short" -gt 0 ]]; then
    echo "eval: $count vaka, $terms terim — hedefe $short vaka VAR (ai.md R21: 200-300)"
    echo "eval:   Article 8.9 bu açığı sanctioned sayıyor; kapatmak alan işidir, mühendislik değil."
else
    echo "eval: $count vaka, $terms terim — R21 hedefi karşılandı"
fi

echo "eval: OK — set okunuyor, her dizi 'unit' içinde canlı kütüğe karşı sınanıyor"
