#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the command search opens on what a person reaches for, and a probe never
# touches the person's own profile.
#
# TWO CLAIMS, and a transcript can keep neither:
#
#   * THE PALETTE OPENS ON THE STARRED AND THE RECENT (`.claude/ui.md` R56, TODOS U-01).
#     `PIRICAD_HELP_PROBE` runs commands by hand, through the door a typed line and a
#     ribbon button use, and asks the palette what it shows: the last one run on top,
#     a view change, an undo, a batch's lines and the shell's own set-up commands left
#     out, Ctrl+D starring the row under the cursor with the cursor staying on it, a
#     typed word giving the section way to the ranked answers. Offscreen, always: it
#     paints nothing the probe needs to see.
#
#   * A PROBE RUN LEAVES THE PROFILE AS IT FOUND IT. Every probe runs the real shell,
#     and the shell saves its window geometry, dock layout and preferences on the way
#     out; on Linux Qt's test mode moved the standard paths but not `QSettings`, so a
#     probe wrote an 800 px offscreen window over the real
#     `~/.config/PiriCAD/PiriCAD.conf` and the next ordinary start opened a postage
#     stamp. Where such a file exists the gate compares it before and after.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/piricad build/release/bin/piricad build/debug/bin/piricad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "komut-paleti: piricad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

profil="${XDG_CONFIG_HOME:-$HOME/.config}/PiriCAD/PiriCAD.conf"
once=""
[[ -f "$profil" ]] && once="$(cksum <"$profil")"

cd "$kok"
set +e
cikti="$(QT_QPA_PLATFORM=offscreen PIRICAD_DATA="$kok/data" PIRICAD_HELP_PROBE=1 \
             timeout 300 "$exe" 2>&1)"
rc=$?
set -e

if [[ $rc -ge 124 ]]; then
    echo "komut-paleti: sonda bitmedi ya da öldü (çıkış kodu $rc)" >&2
    exit 1
fi

if grep -q 'BAŞARISIZ' <<<"$cikti"; then
    echo "komut-paleti: sonda başarısız:" >&2
    grep '^\[yardim\] BAŞARISIZ' <<<"$cikti" >&2
    exit 1
fi

# A probe that stopped early would also print no failure; the claims have to be SEEN.
for gerekli in 'yıldızlayınca Favoriler üste çıktı' 'ikinci Ctrl+D yıldızı kaldırdı' \
               'yalnız elle başlatılan üç araç var' 'arama sırasında Favoriler ve Son kullanılanlar yok'; do
    if ! grep -qF "$gerekli" <<<"$cikti"; then
        echo "komut-paleti: sondanın şu denetimi hiç yazılmadı: $gerekli" >&2
        exit 1
    fi
done

if [[ -n "$once" && "$once" != "$(cksum <"$profil")" ]]; then
    echo "komut-paleti: sonda koşusu kişinin kendi profilini değiştirdi: $profil" >&2
    echo "komut-paleti:   src/app/src/probe_env.cpp'deki liste ve main.cpp'deki QSettings::setPath'e bakın." >&2
    exit 1
fi

echo "komut-paleti: OK — palet favoriler ve son kullanılanlarla açılıyor, Ctrl+D yıldızlıyor,"
echo "komut-paleti:   sonda kişinin profiline dokunmuyor"
