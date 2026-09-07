#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: holding a key on the canvas draws PERPENDICULAR TO THE SURFACE.
#
# WHAT IT GUARDS. Dik mod squares a line to the SHEET. What a survey needs is
# square to the THING — a çekme mesafesi runs perpendicular to the boundary it is
# measured from, a cephe hattı to the road it faces. On a boundary running at 45
# degrees, dik mod gives 0 and the right answer is 315; before this existed the
# only way to draw it was to read the bearing, add ninety and type it, and a
# figure arrived at that way ends up in a document somebody signs.
#
# WHY IT IS A SHELL GATE. The engine half has its own unit test in
# /tests/unit/test_snap.cpp, and it passed while the feature did nothing: the
# lock was read from the settings, cached in `AidSettings` and then never copied
# into the `SnapQuery`. What no Qt-free test can reach is the road from a key
# held on a real canvas to a point in the document, and that road is where the
# break was. It starts at the key event for the same reason the pick gate starts
# at a click.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "yuzey-normali: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "yuzey-normali: BEKLEMEDE — ortamda ekran yok. Basılı tutulan bir tuş ancak"
    echo "yuzey-normali:   bir tuvalde olur; tuvalsiz bir çalıştırma hiçbir şeyi kanıtlamaz."
    exit 0
fi

cd "$kok"

set +e
cikti="$(KENTOS_DATA="$kok/data" KENTOS_NORMAL_PROBE=1 "$exe" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "yuzey-normali: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "yuzey-normali: beklenen satır yok -> $1" >&2
        grep '^\[normal\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

# THE FREE RUN FIRST, because it is what makes the rest mean something: the same
# two clicks with no aid give the raw bearing of the cursor.
bekle "[normal] serbest: 322.125°"

# THE CONTROL. Dik mod on the same click squares the line to the sheet — 0
# degrees — which on a 45 degree boundary is exactly the wrong answer.
bekle "[normal] dik mod: 0.000°"

# THE ANSWER. Perpendicular to the EDGE, and the edge runs at 45. The aim was 7
# degrees off it, which is how a hand aims.
bekle "[normal] kilitli: 315.000°"

# AND IT IS A SNAP, NOT A JAIL — the line this gate exists for. Aimed 38 degrees
# off the perpendicular the aid stands aside and the bearing is the user's own.
# Held as an absolute lock it answered 315 here too, and with it on there was no
# other direction left to draw or to measure in.
bekle "[normal] koni dışı: 353.660°"

# THE OTHER HELD LOCK, on the same road and broken the same way: Ctrl sends
# `MOD köşegen`, which is polar tracking at 45 degrees. A second aim, 33.7
# degrees off, so 45 is a different answer from both the free bearing and dik
# mod's.
bekle "[normal] köşegen: 45.000°"

# AND THE KEY LET GO OF IT. A held lock that stayed on would steer every line
# drawn afterwards, which is the one failure a latched toggle cannot make.
bekle "[normal] bırakınca: kapalı"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "yuzey-normali: OK — 45°'lik kenara 7° yakın nişan tam 315°'ye oturuyor, 38° uzakta"
echo "yuzey-normali:   imleç serbest (353.660°), dik modda 0°, Ctrl basılıyken 45°;"
echo "yuzey-normali:   tuş bırakılınca yardım kapanıyor"
