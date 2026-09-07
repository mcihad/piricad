#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the line tool button holds a family, and taking a member arms it.
#
# WHAT IT GUARDS. The tool column is 46 px wide and eleven creation tools do not
# fit down it as eleven buttons. They are grouped: the button shows whichever
# member was used last and holds the rest one press away. Three things have to
# hold for that to be a grouping and not a hiding place — the card opens, it
# lists the whole family with the command word each member sends, and choosing a
# member other than the one on the face actually arms THAT command.
#
# THE COMMAND WORD IS PART OF THE CHECK. It is the only place a user is told that
# the button they pressed sends `ÇOKLUÇİZGİ`, which is what they will type
# tomorrow — a palette that grouped tools and hid their names would make the
# grouping a mouse-only capability (CLAUDE.md 5.15).
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
    echo "arac-ailesi: BEKLEMEDE — ortamda ekran yok. Açılmayan bir kart hiçbir şeyi"
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

bekle "[aile] kart açıldı"

# AND IT SURVIVES THE HAND LETTING GO. The press that opened the card is still
# down; its release arrives at the card, through the popup grab, with the pointer
# still on the button and therefore on no row. Closing on it made the card open
# and vanish in the same motion, so "hold, look, then choose" was impossible.
bekle "[aile] bırakınca: açık"

# The whole family, in order, by the command each member sends. TWO members, and
# the two it does NOT have are the check: DİKDÖRTGEN and ÇOKGEN were in here once,
# grouped as "things drawn with straight edges", and a rectangle is not a kind of
# line — it is a face, with an area and a fill, and it sits under its own button.
bekle "[aile] üyeler: ÇİZGİ · ÇOKLUÇİZGİ"

# THE SECOND MEMBER, because taking the first would prove nothing the button did
# not already do: the face follows the choice...
bekle "[aile] düğmenin yüzü: ÇOKLUÇİZGİ"

# ...and the choice actually armed that command, not merely repainted a button.
bekle "[aile] çalışan komut: core.polyline"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "arac-ailesi: OK — çizgi düğmesi kendi ailesini açıyor, ikinci üye seçilince"
echo "arac-ailesi:   düğmenin yüzü de çalışan komut da ÇOKLUÇİZGİ oluyor"
