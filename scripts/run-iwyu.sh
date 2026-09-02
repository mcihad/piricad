#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs include-what-you-use over the compile database.
#
# THIS EXISTS BECAUSE OF A PLATFORM DETAIL, and Article 10 keeps that detail out
# of the Makefile: on macOS, IWYU's bundled clang does not know where the SDK
# keeps the standard library, so `iwyu_tool.py -p build src` fails on the first
# file with "'type_traits' file not found" — a toolchain message that reads like a
# broken checkout. `xcrun --show-sdk-path` is the answer, and it is a macOS
# answer, so it belongs in a script rather than in a conditional in /Makefile.
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

extra=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    if sdk="$(xcrun --show-sdk-path 2>/dev/null)" && [[ -n "$sdk" ]]; then
        extra=(-- -isysroot "$sdk")
    else
        echo "iwyu: macOS SDK not found (xcrun --show-sdk-path) — SKIPPED" >&2
        exit 0
    fi
fi

iwyu_tool.py -p "$build" src "${extra[@]}"
