#!/usr/bin/env bash
# Drives KENTOS_OSCLICK_PROBE with REAL window-system events. macOS only, and it
# needs Accessibility permission for the terminal that runs it (System Settings →
# Privacy & Security → Accessibility).
#
# WHY A C HELPER AND NOT `osascript`. System Events' `click at` does NOT send a
# mouse event: it performs an accessibility press on the element under the point.
# Against this program that checks a tool button without running its command, so
# a whole run can look like "the toolbox is broken" when no mouse event was ever
# delivered. CGEventPost is the path a physical mouse takes, and it is the only
# one that tests what a user does.
#
# Usage: scripts/os-tikla.sh <dir> [seconds]
# The probe prints every button's global position, then reports what real clicks
# do to it; this script reads those positions back and clicks them.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"
dir="${1:?kullanım: os-tikla.sh <dizin> [saniye]}"
seconds="${2:-90}"
[[ "$(uname)" == "Darwin" ]] || { echo "os-tikla: yalnız macOS"; exit 0; }
mkdir -p "$dir"
driver="$dir/os-tikla"
cc -O1 -o "$driver" "$here/os-tikla.c" -framework ApplicationServices
"$driver" || { echo "os-tikla: Erişilebilirlik izni yok — Sistem Ayarları → Gizlilik ve Güvenlik → Erişilebilirlik"; exit 1; }
app="$root/build/dev/bin/KentOSCad.app/Contents/MacOS/KentOSCad"
KENTOS_OSCLICK_PROBE="$dir" KENTOS_OSCLICK_SECONDS="$seconds" KENTOS_DATA="$root/data" "$app" > "$dir/app.log" 2>&1 &
pid=$!
for _ in $(seq 1 60); do grep -q '^\[os\] hazır' "$dir/app.log" && break; osascript -e 'delay 0.5'; done
grep -q '^\[os\] hazır' "$dir/app.log" || { echo "os-tikla: probe hazır olmadı"; kill "$pid"; exit 1; }
osascript -e "tell application \"System Events\" to set frontmost of (first process whose unix id is $pid) to true"
echo "os-tikla: pencere hazır, pid=$pid, konumlar $dir/app.log içinde."
echo "os-tikla: sür → $driver tik <x> <y> <ms> | sagtik <x> <y> | git <x> <y> | tus <kod> | bekle <ms>"
echo "os-tikla: bitirmek için → touch $dir/dur"
wait "$pid"
