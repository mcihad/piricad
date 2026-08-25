#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Every `%(name)s` in the stylesheet template has a `.replace` that fills it.

Called by `ci-gate-theme.sh`; see the comment there for why this matters.
"""
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
holes = set(re.findall(r"%\((\w+)\)s", text))
fills = set(re.findall(r'\.replace\(QStringLiteral\("%\((\w+)\)s"\)', text))

for name in sorted(holes - fills):
    print(f"theme: %({name})s has no .replace — Qt refuses the whole sheet"
          f" -> {sys.argv[1]}:1", file=sys.stderr)
for name in sorted(fills - holes):
    print(f"theme: %({name})s is substituted but never used -> {sys.argv[1]}:1",
          file=sys.stderr)

sys.exit(1 if (holes - fills) or (fills - holes) else 0)
