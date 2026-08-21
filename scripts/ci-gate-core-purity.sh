#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: /src/core is Qt-free.
# piricad.md §8: "CI kapısı: /src/core içinde #include <Q görülürse build kırılsın."
# Constitution Article 3.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

while IFS= read -r hit; do
    echo "core-purity: Qt include inside /src/core -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.hpp' --include='*.cpp' --include='*.h' \
             -E '#[[:space:]]*include[[:space:]]*[<"]Q' "$root/src/core" || true)

while IFS= read -r hit; do
    echo "core-purity: Qt symbol inside /src/core -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.hpp' --include='*.cpp' \
             -E '\b(QString|QObject|QWidget|qDebug|Q_OBJECT|QVariant)\b' "$root/src/core" || true)

# core may not depend on any module above it.
while IFS= read -r hit; do
    echo "core-purity: upward dependency from /src/core -> $hit" >&2
    fail=1
done < <(grep -rn --include='*.hpp' --include='*.cpp' \
             -E '#include "piricad/(command|io|render|script|ai|domain|app)/' \
             "$root/src/core" || true)

if [[ $fail -eq 0 ]]; then
    echo "core-purity: OK — /src/core is Qt-free and has no upward dependency"
fi
exit $fail
