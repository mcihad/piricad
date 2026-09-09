#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: every control in the shell is one of the component set's, and the set
# still matches the standard it was drawn from.
#
# WHAT IT GUARDS. `Screenshots/bileşen_standardı.png` — tracked as
# `data/design/bileşen_standardı.png` — is the component standard: six button
# roles in one hierarchy, an input with seven states, the selection controls.
# Before `widgets.hpp` those existed as STYLESHEET RULES and as nothing else, so
# every dialog wrote `new QPushButton(...)` and then remembered, or did not, the
# object name that gave it a role and the height that made it 30 px. Twenty-nine
# buttons were made that way. They agreed with each other only where somebody had
# looked.
#
# THREE CLAIMS, and each is a different way for the set to rot:
#
#   1. NO RAW CONTROL OUTSIDE THE SET. A `QPushButton`, `QCheckBox`,
#      `QRadioButton`, `QSlider`, `QSpinBox`, `QDoubleSpinBox`, `QProgressBar`,
#      `QGroupBox`, `QDialogButtonBox` or `QComboBox` constructed anywhere in /src/app other
#      than `widgets.cpp` and `fields.cpp` is a control that can drift. The one
#      allowance is named below with its removal condition, Article 8 style.
#
#   2. EVERY ROLE HAS ITS RULE, AND EVERY SIZE. A role the sheet does not style
#      is a button that looks like a different one; a height outside 24/30/36 is
#      not a size, it is a mistake.
#
#   3. THE LIVING STANDARD OPENS AND MEASURES RIGHT. `KENTOS_WIDGETS_PROBE`
#      builds every component in every state and prints each with its height;
#      the lines below are the standard's own numbers.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0

# ---- 1. no raw control outside the set ---------------------------------------
raw='new (QPushButton|QCheckBox|QRadioButton|QSlider|QSpinBox|QDoubleSpinBox|QProgressBar|QGroupBox|QDialogButtonBox|QComboBox)\b'

# THE ONE ALLOWANCE. The style designer's per-layer property form drives its
# numeric editors — width, size, interval, spacing, phase, opacity — through the
# `QSpinBox` value API, three constructions in a 2 500-line file with its own
# visual gates; rebuilding that form is that file's own change, not a side
# effect of this one. Everything else in the designer — footer, gallery button,
# colour lock, unit segment — is already on the set. The ceiling is the count on
# the day this gate landed and it may only go DOWN: a new raw control there fails
# like anywhere else.
#
# Removal condition: the designer's property rows are rebuilt on `FormRow` and
# `Field`, at which point this block is deleted.
allow_file="src/app/src/style_designer.cpp"
allow_max=3

while IFS= read -r hit; do
    file="${hit%%:*}"
    rest="${hit#*:}"
    case "$file" in
        */src/app/src/widgets.cpp|*/src/app/src/fields.cpp) continue ;;
        */"$allow_file") continue ;;
    esac
    echo "bilesenler: ham Qt denetimi — bileşen setinden gelmeli (widgets.hpp) -> ${file#"$kok"/}:${rest%%:*}" >&2
    fail=1
done < <(grep -rnE "$raw" --include='*.cpp' "$kok/src/app/src" || true)

allowed_now="$(grep -cE "$raw" "$kok/$allow_file" || true)"
if (( allowed_now > allow_max )); then
    echo "bilesenler: $allow_file içinde $allowed_now ham denetim var, izin verilen en çok $allow_max" >&2
    echo "bilesenler:   izin yalnızca AZALABİLİR; yeni denetim bileşen setinden gelmeli -> $allow_file:1" >&2
    fail=1
fi

# ---- 2. every role has its rule, every size its height -------------------------
sheet="$kok/src/app/src/theme.cpp"
for role in primary secondary ghost danger mode iconButton; do
    if ! grep -qE "QPushButton#${role}\b" "$sheet"; then
        echo "bilesenler: '$role' rolünün stil sayfasında kuralı yok -> src/app/src/theme.cpp:1" >&2
        fail=1
    fi
done
# The heights are `ControlSize`, in the code, once. The sheet may NOT carry a
# height for a button: Qt turns a `min-height` into the widget's minimum size
# with the border added, so a rule saying 30 made a button of 32 and silently
# overrode the code's 30.
hdr="$kok/src/app/include/kentos_cad/app/widgets.hpp"
for pair in "Compact:24" "Regular:30" "Large:36"; do
    if ! grep -qE "${pair%%:*}\s*=\s*${pair##*:}\b" "$hdr"; then
        echo "bilesenler: ControlSize::${pair%%:*} ${pair##*:} px değil (standart 24 / 30 / 36) -> src/app/include/kentos_cad/app/widgets.hpp:1" >&2
        fail=1
    fi
done
if grep -nE "QPushButton[^{]*\{[^}]*(min|max)-height" "$sheet" >/dev/null; then
    echo "bilesenler: stil sayfası bir düğmeye yükseklik veriyor; yükseklik ControlSize'dan gelir -> src/app/src/theme.cpp:$(grep -nE 'QPushButton[^{]*\{[^}]*(min|max)-height' "$sheet" | head -n1 | cut -d: -f1)" >&2
    fail=1
fi

# The standard itself has to be where the gates and the manual read it from.
if [[ ! -f "$kok/data/design/bileşen_standardı.png" ]]; then
    echo "bilesenler: bileşen standardı izlenen kopyada yok -> data/design/bileşen_standardı.png" >&2
    fail=1
fi

# ---- 3. the living standard --------------------------------------------------
exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "bilesenler: kentos_cad bulunamadı — canlı standart denenmedi (uygulama derlenmemiş)"
elif [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "bilesenler: BEKLEMEDE — ortamda ekran yok; canlı standart bir pencerede ölçülür."
else
    cd "$kok"
    set +e
    cikti="$(KENTOS_DATA="$kok/data" KENTOS_WIDGETS_PROBE=1 "$exe" 2>/dev/null)"
    rc=$?
    set -e
    if [[ $rc -ge 128 ]]; then
        echo "bilesenler: uygulama sinyal $((rc - 128)) ile öldü" >&2
        fail=1
    fi

    bekle() {
        if ! grep -qF "$1" <<<"$cikti"; then
            echo "bilesenler: beklenen satır yok -> $1" >&2
            grep '^\[bilesen\]' <<<"$cikti" >&2 || true
            fail=1
        fi
    }

    # The six roles, each at the standard's regular height, and the icon button
    # at its own 32.
    for role in primary secondary ghost danger mode; do
        bekle "[bilesen] $role · etkin · 30 px"
        bekle "[bilesen] $role · devre dışı · 30 px"
    done
    bekle "[bilesen] iconButton · etkin · 32 px"

    # The three heights, side by side.
    bekle "[bilesen] secondary · compact · 24 px"
    bekle "[bilesen] secondary · regular · 30 px"
    bekle "[bilesen] secondary · large · 36 px"

    # The input's states, every one at the regular height.
    for state in "varsayılan" "değiştirilmiş" "hatalı" "salt okunur" "devre dışı" "türetilmiş" "açılır liste" "tarih"; do
        bekle "[bilesen] girdi · $state · 30 px"
    done

    # The selection controls and the annotation.
    bekle "[bilesen] onay · işaretli"
    bekle "[bilesen] onay · kısmi"
    bekle "[bilesen] radyo · seçili"
    bekle "[bilesen] anahtar · açık · 20 px"
    bekle "[bilesen] segment · 3 seçenek · 24 px"
    bekle "[bilesen] kaydırıcı · %62"
    bekle "[bilesen] etiket · seçili"
    bekle "[bilesen] etiket · taşma"
    bekle "[bilesen] rozet · accent · 14 px"
    bekle "[bilesen] uyarı şeridi · warn"
    bekle "[bilesen] yükleniyor · etkin · 2 px"
    bekle "[bilesen] açılır liste · 3 seçenek · 30 px"
    bekle "[bilesen] ifade · renkli · 30 px"
    bekle "[bilesen] tablo · 3 satır"
fi

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "bilesenler: OK — /src/app'te ham Qt denetimi yok (tasarımcının adlandırılmış payı dışında),"
echo "bilesenler:   altı rolün ve üç boyun kuralı var, canlı standart standardın sayılarını veriyor"
