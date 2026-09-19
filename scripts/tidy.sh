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

# WHICH ONE, AND IT IS NOT "whatever is on PATH". The tool carries its own clang
# front end, so a clang-tidy OLDER than the compiler that built the database
# cannot parse the system headers that compiler ships with. On this machine
# AppleClang 21's libc++ uses `__builtin_clzg`, `__builtin_ctzg` and
# `__builtin_popcountg`, which LLVM 18 does not have: tidy 18 reported 2599
# `clang-diagnostic-error`s from inside <bit> and <type_traits> and every real
# finding was buried under them. `CLANG_TIDY` names one explicitly; otherwise the
# newest Homebrew LLVM is preferred over the bare name, and the bare name is the
# last resort so a machine with only one still works.
tidy_bul() {
    local aday
    if [[ -n "${CLANG_TIDY:-}" ]]; then printf '%s' "$CLANG_TIDY"; return 0; fi
    for aday in /opt/homebrew/opt/llvm/bin/clang-tidy /usr/local/opt/llvm/bin/clang-tidy; do
        if [[ -x "$aday" ]]; then printf '%s' "$aday"; return 0; fi
    done
    if command -v clang-tidy >/dev/null; then printf 'clang-tidy'; return 0; fi
    return 1
}

# `--var` — the Makefile asking whether the tool exists at all. It used to ask
# `command -v clang-tidy`, which answers "no" on every machine where Homebrew's
# LLVM is keg-only, that is, on this one: `make check` reported the gate SKIPPED
# while the tool sat installed two directories away, and a gate that skips has
# never run (CLAUDE.md 6.2). The search lives here, so the question does too
# (Article 10: no build logic in the Makefile).
if [[ "${1:-}" == "--var" ]]; then
    tidy_bul >/dev/null 2>&1 || exit 1
    exit 0
fi

# `--bir <dosya>` — one translation unit, run and filtered.
#
# A FINDING IN SOMEBODY ELSE'S HEADER IS NOT A FINDING WE CAN ACT ON, and this is
# where that policy stops being a comment. `HeaderFilterRegex` and
# `ExcludeHeaderFilterRegex` do not reach clang-analyzer diagnostics: the analyser
# treats every inline function in the translation unit as its own entry point, so
# a header-defined iterator in a fetched dependency is analysed and reported on
# its own — `-isystem` and both filters notwithstanding. libpqxx's `result_iter`
# is the live example: its end iterator holds a null `m_home` by design and never
# reads through it, which the analyser cannot know.
#
# A diagnostic is kept only when its own file is under /src. Notes follow the
# diagnostic they belong to, so they are kept or dropped with it.
if [[ "${1:-}" == "--bir" ]]; then
    tidy="$(tidy_bul)" || { echo "tidy: clang-tidy bulunamadı" >&2; exit 2; }
    ekstra=()
    [[ -n "${KENTOS_TIDY_SYSROOT:-}" ]] && ekstra+=("--extra-arg=-isysroot${KENTOS_TIDY_SYSROOT}")
    # clang-tidy answers nonzero for a finding it reported — including one this
    # filter then drops — so its status is discarded and awk's is the answer.
    set +e
    "$tidy" -p "${KENTOS_TIDY_DB:?}" --quiet "${ekstra[@]}" "$2" 2>&1 |
        awk -v src="${KENTOS_TIDY_SRC:?}/" '
            /^\/[^ :]*:[0-9]+:[0-9]+: (error|warning): / {
                yol = $0
                sub(/:[0-9]+:[0-9]+: .*/, "", yol)
                bizim = (index(yol, src) == 1)
                if (bizim && $0 ~ /: error: /) hata = 1
            }
            bizim { print }
            END { exit (hata ? 1 : 0) }
        '
    durum=${PIPESTATUS[1]}
    set -e
    exit "$durum"
fi

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

tidy="$(tidy_bul)" || { echo "tidy: clang-tidy bulunamadı" >&2; exit 2; }

# AND THE SDK, on macOS. A Homebrew clang does not know where Apple keeps its
# headers, so without this it cannot find <cstdint> — the compile database's own
# flags do not carry a sysroot because AppleClang does not need one told.
sysroot=""
if [[ "$(uname -s)" == "Darwin" ]]; then
    sysroot="$(xcrun --show-sdk-path 2>/dev/null || true)"
fi

echo "tidy: $("$tidy" --version 2>/dev/null | sed -n 's/.*LLVM version \(.*\)/LLVM \1/p')"

# xargs answers 123 when any child failed, and a finding IS a failure here, so the
# status is mapped rather than passed through. Each child is this same script in
# `--bir` mode, so it filters its own output rather than several of them writing
# interleaved diagnostics into one stream to be sorted out afterwards.
export KENTOS_TIDY_DB="$yapi" KENTOS_TIDY_SYSROOT="$sysroot" KENTOS_TIDY_SRC="$kok/src"
if printf '%s\0' "${dosyalar[@]}" |
        xargs -0 -n 1 -P "$is" "${BASH_SOURCE[0]}" --bir; then
    exit 0
fi
exit 1
