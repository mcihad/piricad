#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the two frame budgets render.md names, measured on a real frame.
#
#   R13 / kentoscad.md 10.1 — pan/zoom stays inside 16 ms a frame.
#   R7                      — the scene draws in fewer than 100 draw calls.
#
# WHY IT IS A SHELL GATE AND NOT A BENCH CASE. `/tests` links no Qt (Article 3.4)
# and every backend is Qt by definition, so the bench binary has nowhere to build
# one: it can time the SCENE BUILDER and nothing else. A budget about what
# reaches the screen has to be measured where the screen is, which is the
# application. `KENTOS_BUDGET_PROBE=<kare>` runs the real paint path the real
# number of times and exits non-zero when either budget is exceeded.
#
# A machine with no display reports PENDING rather than passing: a QRhiWidget
# needs a GL context, and a frame that never drew is not a fast frame.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "butce: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "butce: BEKLEMEDE — ortamda ekran yok. Kare ölçümü bir yüzey ister ve"
    echo "butce:   çizilmemiş bir kare hızlı bir kare değildir. Ölçüm yapılmadı."
    exit 0
fi

# The heaviest scene the repository carries: patterned fills over a full sheet,
# which is where both budgets are actually at risk.
sahne="tests/bench/sahne/desen-yuku.json"
if [[ ! -f "$kok/$sahne" ]]; then
    echo "butce: $sahne yok — ATLANDI"
    exit 0
fi

cd "$kok"
KENTOS_DATA="$kok/data" KENTOS_BUDGET_PROBE=20 "$exe" --betik "$sahne" 2>&1 | grep '^\[butce\]' || true
KENTOS_DATA="$kok/data" KENTOS_BUDGET_PROBE=20 "$exe" --betik "$sahne" >/dev/null 2>&1
