#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# clang-tidy over THE COMPILE DATABASE, not over every .cpp on disk.
#
# `find src -name '*.cpp' | xargs clang-tidy` analyses files the build never
# compiled — `script/lua_runner.cpp` with `KENTOS_WITH_LUA=OFF`, every backend
# behind an option that is off — and clang-tidy then runs them with no flags at
# all. The result is not a finding, it is a parse failure dressed as one:
#     lua_runner.cpp:6:10: error: 'sol/sol.hpp' file not found
# followed by whatever garbage the half-parsed file produces. Two of the findings
# this gate reported were that, and chasing them costs an afternoon.
#
# The database lists exactly the translation units this configuration builds, so
# that is the list.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
yapi="${1:-$kok/build/dev}"
veritabani="$yapi/compile_commands.json"

if [[ ! -f "$veritabani" ]]; then
    echo "tidy: $veritabani yok — önce yapılandırın" >&2
    exit 2
fi

# Our own translation units only. The pinned dependencies are fetched sources and
# a finding in somebody else's header is not a finding we can act on.
dosyalar=()
while IFS= read -r yol; do dosyalar+=("$yol"); done < <(
    python3 - "$veritabani" "$kok" <<'PY'
import json, os, sys
veritabani, kok = sys.argv[1], os.path.realpath(sys.argv[2])
kaynak = os.path.join(kok, "src") + os.sep
gorulen = []
for giris in json.load(open(veritabani, encoding="utf-8")):
    yol = os.path.realpath(os.path.join(giris.get("directory", ""), giris["file"]))
    if yol.startswith(kaynak) and yol not in gorulen:
        gorulen.append(yol)
for yol in sorted(gorulen):
    print(yol)
PY
)

if [[ ${#dosyalar[@]} -eq 0 ]]; then
    echo "tidy: derleme veritabanında /src altında hiçbir birim yok" >&2
    exit 2
fi

# PARALLEL, and not as a nicety. clang-tidy re-parses the whole translation unit
# for every file it is handed, so 135 of them run for HOURS in one process — on a
# machine somebody else is also building on, that is the difference between a gate
# people run and a gate people skip. One job per core minus two, the same
# reservation the rest of this repository's tooling makes.
cekirdek=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
is=$(( cekirdek > 3 ? cekirdek - 2 : 1 ))

echo "tidy: ${#dosyalar[@]} çeviri birimi (derleme veritabanından), $is koşut iş"

# xargs answers 123 when any child failed, and a finding IS a failure here, so the
# status is mapped rather than passed through.
if printf '%s\0' "${dosyalar[@]}" |
        xargs -0 -n 1 -P "$is" clang-tidy -p "$yapi" --quiet; then
    exit 0
fi
exit 1
