#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs include-what-you-use over the compile database.
#
# THIS EXISTS BECAUSE OF PLATFORM DETAILS, and Article 10 keeps those details out
# of the Makefile. There are two.
#
# 1. On macOS, IWYU's bundled clang does not know where the SDK keeps the
#    standard library, so `iwyu_tool.py -p build src` fails on the first file
#    with "'type_traits' file not found" — a toolchain message that reads like a
#    broken checkout. `xcrun --show-sdk-path` is the answer, and it is a macOS
#    answer, so it belongs in a script rather than in a conditional in /Makefile.
#
# 2. libc++ from LLVM 20 on has a SIMD fast path in `std::find` and
#    `std::mismatch` written with clang's `ext_vector_type` extension
#    (`__algorithm/simd_utils.h`). IWYU 0.26 has no case for that type and dies
#    on the spot — `iwyu.cc:1967: Assertion failed: TODO(csilvers): for objc and
#    clang lang extensions` — which reaches the Makefile as the unreadable
#    `Error 250` (SIGABRT, 256-6) and takes `make check` down with it. One
#    ordinary translation unit that calls `std::find` over 64-bit integers is
#    enough to trigger it.
#
#    `__OPTIMIZE_SIZE__` is the switch libc++ itself reads: with it defined,
#    `_LIBCPP_VECTORIZE_ALGORITHMS` is 0 and the scalar path is compiled, so
#    IWYU never meets the vector type. It is defined here for the ANALYSIS only
#    and cannot reach the build — `make build` never calls this script, and the
#    flag lives on IWYU's command line, not in any preset. What IWYU reports
#    about this project's own headers is unchanged: the fast path is inside a
#    system header, which IWYU does not make suggestions about either way.
#    Passing `-Os` instead does not work, because IWYU drops optimisation flags
#    before the macro is derived from them.
#
#    Removed when an IWYU release handles `ExtVectorType`; until then the
#    alternative is a red gate that names none of this.
#
# `make check` calls this only when IWYU is installed; a missing IWYU is reported
# by the Makefile and never silently passed (CLAUDE.md 6.2).
set -uo pipefail

build="${1:-build/dev}"

if ! command -v include-what-you-use >/dev/null 2>&1; then
    echo "iwyu: include-what-you-use not installed — SKIPPED"
    exit 0
fi
if ! command -v iwyu_tool.py >/dev/null 2>&1; then
    echo "iwyu: iwyu_tool.py not on PATH — SKIPPED (install the IWYU tools)"
    exit 0
fi
if [[ ! -f "$build/compile_commands.json" ]]; then
    echo "iwyu: no compile database at $build — run 'make setup' first" >&2
    exit 1
fi

# Everything after `--` goes to IWYU's own command line; see the notes above.
extra=(-- -D__OPTIMIZE_SIZE__=1)
if [[ "$(uname -s)" == "Darwin" ]]; then
    if sdk="$(xcrun --show-sdk-path 2>/dev/null)" && [[ -n "$sdk" ]]; then
        extra+=(-isysroot "$sdk")
    else
        echo "iwyu: macOS SDK not found (xcrun --show-sdk-path) — SKIPPED" >&2
        exit 0
    fi
fi

iwyu_tool.py -p "$build" src "${extra[@]}"
