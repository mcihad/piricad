#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: a widget that paints from tokens declares that it does.
#
# `theme.hpp` defines `Themed`, and `applyThemeToChildren` hands the theme to
# every descendant that implements it. A class with an `applyTheme` member that
# does NOT declare the interface is invisible to that walk: the window changes
# theme and the widget does not, silently, in one theme only.
#
# That is not hypothetical. `SettingsDialog` themed itself and not its
# `SectionList`, so the settings window kept a black sidebar in the light theme —
# reported by a user, not by a test. This is that test.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/src/app/include/piricad/app"
fail=0

if [[ ! -d "$app" ]]; then
    echo "themed: skipped — /src/app is not in this tree"
    exit 0
fi

checked=0
while IFS= read -r header; do
    # Every class in the file, with the body that follows it, so the interface
    # declaration is looked for in the SAME class rather than anywhere nearby.
    python3 - "$header" <<'PY' || fail=1
import re, sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
bad  = []

for match in re.finditer(r"^class\s+(\w+)\s*:([^{]*)\{(.*?)^\};", text, re.S | re.M):
    name, bases, body = match.group(1), match.group(2), match.group(3)

    if "applyTheme(ThemeMode" not in body:
        continue
    # A class that inherits a Themed base already answers the walk.
    if "DialogFrame" in bases:
        continue
    if "public Themed" in bases and "Q_INTERFACES(piricad::app::Themed)" in body:
        continue

    line = text[: match.start()].count("\n") + 1
    bad.append((line, name))

for line, name in bad:
    print(f"themed: {name} paints from tokens but does not declare Themed"
          f" -> {path}:{line}", file=sys.stderr)
    print("themed:   add `, public Themed` to the bases and"
          " `Q_INTERFACES(piricad::app::Themed)` to the body;", file=sys.stderr)
    print("themed:   without it `applyThemeToChildren` skips it and the widget"
          " keeps one theme's colours.", file=sys.stderr)

sys.exit(1 if bad else 0)
PY
    checked=$((checked + 1))
done < <(find "$app" -name '*.hpp')

if [[ $fail -eq 0 ]]; then
    echo "themed: OK — every token-painting widget in $checked headers declares Themed"
fi
exit $fail
