#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the sanctioned presets DEMAND the full canvas, they do not hope for it.
#
# WHAT WENT WRONG WITHOUT THIS. `KENTOS_WITH_RHI` and `KENTOS_WITH_TEXT` default
# to ON *where their toolchain is found* — a probe, and a probe that fails is
# silent by construction. So a fresh checkout on a machine missing Qt Shader
# Tools or HarfBuzz configured happily, built happily, ran happily, and drew no
# captions at all. The person building it was never told; they found out by
# looking at a drawing and seeing empty parcels where the ada numbers should be.
#
# A build that cannot draw text is not a build of this program. So `dev`, `debug`,
# `release` and `asan` set both to ON explicitly, which turns the silent
# degradation into the hard error `src/app/CMakeLists.txt` and
# `cmake/KentOSCadDependencies.cmake` already write — with the apt, dnf, brew and
# vcpkg names in it.
#
# `headless` is the exception and it is a real one: it builds no application and
# exists to prove the Qt-free targets stand on their own (Article 3.3). Demanding
# FreeType there would add a dependency to the one preset whose whole point is
# not having any.
#
# THE ESCAPE IS STILL THERE, and it has to be: `-DKENTOS_WITH_RHI=OFF` or
# `-DKENTOS_WITH_TEXT=OFF` on the command line still overrides a preset. What is
# gone is getting there by accident.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
presets="$kok/CMakePresets.json"

if [[ ! -f "$presets" ]]; then
    echo "preset-kanvas: CMakePresets.json yok -> CMakePresets.json:1" >&2
    exit 1
fi

fail=0

deger() { # preset, variable -> the value it resolves to, following `inherits`
    python3 - "$presets" "$1" "$2" <<'PY'
import json, sys

presets = {p["name"]: p for p in json.load(open(sys.argv[1]))["configurePresets"]}
want    = sys.argv[3]

def resolve(name, seen=()):
    if name in seen or name not in presets:
        return None
    node = presets[name]
    got  = node.get("cacheVariables", {}).get(want)
    if got is not None:
        return got
    parents = node.get("inherits") or []
    if isinstance(parents, str):
        parents = [parents]
    for parent in parents:
        got = resolve(parent, seen + (name,))
        if got is not None:
            return got
    return None

print(resolve(sys.argv[2]) or "")
PY
}

for preset in dev debug release asan; do
    for option in KENTOS_WITH_RHI KENTOS_WITH_TEXT; do
        got="$(deger "$preset" "$option")"
        if [[ "$got" != "ON" ]]; then
            echo "preset-kanvas: '$preset' ön ayarı $option değerini ON istemiyor" >&2
            echo "preset-kanvas:   (bulunan: '${got:-yok}') -> CMakePresets.json:1" >&2
            echo "preset-kanvas:   Sonda kalan bir sonda sessizdir: eksik paket, yazısız bir" >&2
            echo "preset-kanvas:   yapı olarak değil, paket adını söyleyen bir hata olarak" >&2
            echo "preset-kanvas:   çıkmalı." >&2
            fail=1
        fi
    done
done

# And the exception stays an exception: headless says OFF out loud rather than
# inheriting a demand it cannot meet.
for option in KENTOS_WITH_RHI KENTOS_WITH_TEXT; do
    got="$(deger headless "$option")"
    if [[ "$got" != "OFF" ]]; then
        echo "preset-kanvas: 'headless' ön ayarı $option için OFF demiyor (bulunan: '${got:-yok}')" >&2
        echo "preset-kanvas:   Bu ön ayar uygulama derlemez; Qt'siz hedeflerin kendi başına" >&2
        echo "preset-kanvas:   ayakta durduğunu kanıtlamak için vardır -> CMakePresets.json:1" >&2
        fail=1
    fi
done

# The failure the demand produces has to be ACTIONABLE, or the demand only moves
# the confusion from run time to configure time.
for needle in "qt6-shadertools-dev" "libharfbuzz-dev" "freetype-devel" "brew install freetype"; do
    if ! grep -rqF -- "$needle" "$kok/src/app/CMakeLists.txt" "$kok/cmake/KentOSCadDependencies.cmake"; then
        echo "preset-kanvas: eksik bağımlılık mesajında '$needle' geçmiyor" >&2
        fail=1
    fi
done

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "preset-kanvas: OK — dev/debug/release/asan tam tuvali talep ediyor, headless"
echo "preset-kanvas:   bilerek dışarıda, eksik paketin adı hata mesajında yazılı"
