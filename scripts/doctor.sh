#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Reports what this machine can build, and what each missing piece costs.
set -uo pipefail

say() { printf '  %-26s %s\n' "$1" "$2"; }
probe() {
    local label="$1" cmd="$2" note="${3:-}"
    if out=$(eval "$cmd" 2>/dev/null); then
        say "$label" "${out:-present}"
    else
        say "$label" "MISSING${note:+  — $note}"
    fi
}

echo "PiriCAD — build environment"
echo
probe "CMake"        "cmake --version | head -1 | cut -d' ' -f3"
probe "Ninja"        "ninja --version"
probe "C++ compiler" "\${CXX:-c++} --version | head -1"
probe "git"          "git --version | cut -d' ' -f3"
probe "Qt 6"         "qmake6 -query QT_VERSION"
echo
echo "Optional dependencies (all gated OFF by default — CLAUDE.md Article 8)"
probe "qsb (GPU canvas)" "command -v qsb || command -v qsb6" "QRhi backend unavailable; QPainter is used"
probe "GDAL"             "gdal-config --version"               "no format I/O"
probe "PROJ"             "pkg-config --modversion proj"        "no coordinate transformation"
probe "GEOS"             "geos-config --version"               "no overlay operations"
probe "Lua 5.4"          "pkg-config --modversion lua5.4"      "no hot-path script layer"
probe "Python 3"         "python3 --version | cut -d' ' -f2"   "no ecosystem script layer"
probe "clang-format"     "clang-format --version | head -1"    "make format unavailable"
probe "clang-tidy"       "clang-tidy --version | head -1"      "make tidy unavailable"
echo
