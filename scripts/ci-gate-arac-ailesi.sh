#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a split button on the ribbon holds a family, and taking a member arms it.
#
# WHAT IT GUARDS. Tools that stand in for one another share one split button:
# the face runs whichever member was used last and the arrow lists the rest
# (`.claude/ui.md` R47). Four things have to hold for that to be a grouping and
# not a hiding place — the arrow opens the list, the list survives the hand
# letting go of the press that opened it, it lists the whole family with the
# line each member sends, and choosing a member other than the one on the face
# actually arms THAT method.
#
# THE CIRCLE'S BUTTON, because it is the family a hand meets first: the tab the
# window opens on, the drawing panel, the third big button. Until the ribbon
# this gate pressed the old tool column's line family; the ribbon gives Çizgi,
# Çoklu Çizgi and Spline a button each, and a family of one is not a family.
#
# THE COMMAND LINE IS PART OF THE CHECK. It is the only place a user is told that
# the circle through the two ends of a diameter is `DAİRE yontem=2n`, which is
# what they will type tomorrow — a list that grouped methods and hid their words
# would make the grouping a mouse-only capability (CLAUDE.md 5.15).
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "arac-ailesi: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "arac-ailesi: BEKLEMEDE — ortamda ekran yok. Açılmayan bir liste hiçbir şeyi"
    echo "arac-ailesi:   kanıtlamaz."
    exit 0
fi

cd "$kok"

set +e
cikti="$(KENTOS_DATA="$kok/data" KENTOS_FAMILY_PROBE=1 "$exe" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "arac-ailesi: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "arac-ailesi: beklenen satır yok -> $1" >&2
        grep '^\[aile\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

# The whole family, in order, by the line each member sends. Four members — the
# four ways to say where a circle is — and the two it does NOT have are part of
# the check: Elips and Halka were in here once, and an ellipse is not a way to
# draw a circle; each has a place of its own.
bekle "[aile] üyeler: DAİRE · DAİRE yontem=2n · DAİRE yontem=3n · DAİRE yontem=ttr"

# A real press on the arrow half of the button opens the list...
bekle "[aile] liste açıldı"

# ...AND IT SURVIVES THE HAND LETTING GO. The release of the press that opened the
# list arrives with the pointer still on the button and therefore on no row.
# Closing on it would make the list open and vanish in the same motion, so
# "press, look, then choose" would be impossible.
bekle "[aile] bırakınca: açık"

# THE SECOND MEMBER, because taking the first would prove nothing the face did
# not already do: the face follows the choice...
bekle "[aile] düğmenin yüzü: DAİRE yontem=2n"

# ...and the choice armed that command WITH ITS METHOD, not merely repainted a
# button: the circle asks for an end of the diameter, not for a centre.
bekle "[aile] çalışan komut: core.circle_draw"
bekle "[aile] istem: Çapın bir ucu"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "arac-ailesi: OK — Daire düğmesinin oku ailesini açıyor, bırakınca açık kalıyor;"
echo "arac-ailesi:   ikinci üye seçilince düğmenin yüzü DAİRE yontem=2n oluyor ve komut"
echo "arac-ailesi:   çapın ucunu soruyor"
