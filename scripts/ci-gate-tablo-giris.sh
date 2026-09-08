#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the attribute grid is read-only until asked, uses this program's editors,
# and Enter walks the row the way a ledger is filled in.
#
# FOUR CLAIMS, and none of them can be read out of a transcript:
#
#   * A CELL DOES NOT OPEN while the edit mode is off. A grid of a thousand
#     parcels is read far more often than it is written, and a double click that
#     starts editing a cadastral value somebody only meant to look at is a change
#     nobody intended and nobody notices. The refusal is in the MODEL's `flags`,
#     not in the view's edit triggers, so it holds however the cell is reached —
#     double click, F2, a paste, `edit()` called from code.
#
#   * ENTER GOES ACROSS, then down. The last column wraps to the NEXT ROW's first
#     editable column, which is column 1: `fid` is identity and never opens. The
#     last cell of the last row stays put rather than wrapping to the top, because
#     "I have reached the end" is information and a silent jump to row one is a
#     value entered in the wrong place.
#
#   * THE VALUE IS CHECKED BEFORE IT IS SENT, with the parser the command itself
#     uses. The command would refuse it anyway — but a refusal that arrives after
#     the dispatch is a failed line in the transcript and a cell the user has
#     already left behind.
#
#   * AND WHAT LANDS IS WHAT THE COLUMN DECLARED: `0,40` typed with a comma comes
#     back as the fixed-point `0.40`, a date as ISO 8601.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "tablo-giris: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "tablo-giris: BEKLEMEDE — ortamda ekran yok. Açılmayan bir hücre hiçbir şeyi"
    echo "tablo-giris:   kanıtlamaz."
    exit 0
fi

cd "$kok"

set +e
cikti="$(KENTOS_DATA="$kok/data" KENTOS_TABLE_PROBE=1 "$exe" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "tablo-giris: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "tablo-giris: beklenen satır yok -> $1" >&2
        grep '^\[tablo\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

# The gate, and it is the first line for a reason: everything below is about
# editing, and none of it should be possible before this.
bekle "[tablo] kip kapalıyken: açılmadı"
bekle "[tablo] kip: açık"
bekle "[tablo] kip açıkken: açıldı"

# Across, across, then wrap to the next row's first editable column.
bekle "[tablo] enter 1 -> 0,2"
bekle "[tablo] enter 2 -> 0,3"
bekle "[tablo] enter 3 -> 1,1"

# Refused before it was sent, with the column named.
bekle "[tablo] reddedilen: 'ada' özniteliği tam sayı bekliyor. Girilen: 'abc'"

# And each type landed as its own type: a comma typed into a fixed-point column
# comes back as the declared two digits, a date as ISO 8601.
bekle "[tablo] tam sayı: 128"
bekle "[tablo] ondalık: 0.40"
bekle "[tablo] tarih: 2026-09-08"

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "tablo-giris: OK — kip kapalıyken hücre açılmıyor, Enter kolonları geçip satır"
echo "tablo-giris:   sonunda alt satırın ilk kolonuna sarıyor, hatalı değer gönderilmeden"
echo "tablo-giris:   reddediliyor, her tür kendi biçiminde iniyor"
