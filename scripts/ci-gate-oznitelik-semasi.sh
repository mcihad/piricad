#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the attribute schema can be built, edited and pruned from the window.
#
# WHAT IT GUARDS. Before this page, a column could only be declared by typing
# `SÜTUN` at the prompt — so the schema, which is the thing every attribute value
# in the drawing is addressed against, was reachable only by someone who already
# knew the command. That is the mouse-only rule (CLAUDE.md 5.15) standing on its
# head: a capability that exists only for the keyboard is just as much a hole in
# the equality of clients, because the panel that shows the values could not
# declare the column they go in.
#
# WHAT IS CHECKED, and each line is a different failure:
#
#   * EVERY TYPE lands. A page that offered a type word the command does not know
#     would give the user a form they can fill in and not send.
#   * The QUALIFIERS survive. A decimal's digits and a column's requiredness are
#     the two fields that are easy to build a form for and forget to put on the
#     command line.
#   * An EDIT changes what a column says and not what it is.
#   * A DROP removes exactly one column.
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exe=""
for aday in build/dev/bin/kentos_cad build/release/bin/kentos_cad build/debug/bin/kentos_cad; do
    if [[ -x "$kok/$aday" ]]; then exe="$kok/$aday"; break; fi
done

if [[ -z "$exe" ]]; then
    echo "oznitelik-semasi: kentos_cad bulunamadı — ATLANDI (uygulama derlenmemiş)"
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "oznitelik-semasi: BEKLEMEDE — ortamda ekran yok. Açılmayan bir pencere hiçbir"
    echo "oznitelik-semasi:   şeyi kanıtlamaz."
    exit 0
fi

cd "$kok"

set +e
cikti="$(KENTOS_DATA="$kok/data" KENTOS_SCHEMA_PROBE=1 "$exe" 2>/dev/null)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "oznitelik-semasi: uygulama sinyal $((rc - 128)) ile öldü" >&2
    fail=1
fi

bekle() {
    if ! grep -qF "$1" <<<"$cikti"; then
        echo "oznitelik-semasi: beklenen satır yok -> $1" >&2
        grep '^\[sema\]' <<<"$cikti" >&2 || true
        fail=1
    fi
}

# SIX COLUMNS WERE DECLARED AND FIVE ARE LISTED, which is the whole of the
# layer/project split. The sixth was declared with `katman=ENERJİ` and this page
# belongs to another layer — so it is not here, and before the split it would
# have been, along with an empty row on every object in the drawing.
bekle "[sema] sütun sayısı: 5"

# Five types, five rows, and the columns that only some of them carry. Each is
# marked `proje sütunu` because the page is a LAYER's and these belong to the
# project: listed as context, not editable from here.
bekle "[sema] satır: ada · Ada No · tam_sayi ·  · evet · proje sütunu"
bekle "[sema] satır: oran · Oran · ondalik · 2 basamak ·  · proje sütunu"
bekle "[sema] satır: onay · Onay Tarihi · tarih ·  ·  · proje sütunu"
bekle "[sema] satır: tescilli · Tescilli · evet_hayir ·  ·  · proje sütunu"
bekle "[sema] satır: cephe · Cephe · uzunluk ·  ·  · proje sütunu"

# And the layer column is nowhere on this page.
if grep -qF "direk" <<<"$cikti"; then
    echo "oznitelik-semasi: 'ENERJİ' katmanına tanımlanan sütun başka bir katmanın" >&2
    echo "oznitelik-semasi:   sayfasında görünüyor -> src/app/src/schema_page.cpp:1" >&2
    fail=1
fi

# THE EDIT CHANGED WHAT IT SAYS. Name, digits and requiredness moved; the id and
# the type did not, and `AttrColumn::amend` is what refuses those.
bekle "[sema] düzenlendi: oran · Ölçülen Oran · ondalik · 3 basamak · evet · proje sütunu"

bekle "[sema] silindikten sonra: 4 sütun"

# THE PROJECT HALF OF THE STORY, and both halves have to exist for either to make
# sense: a layer page that declares layer columns is only honest if there is
# somewhere else to declare the project's. It is its OWN WINDOW — `Dosya ▸ Proje
# Ayarları…` — with the settings that travel in the file beside the schema that
# does.
bekle "[sema] proje penceresi: Ayarlar | Öznitelikler"

# AND `Seçenekler` DOES NOT ALSO CARRY THEM. Two windows both holding the
# project's pages would be two places to look and one of them wrong; the options
# window ends on its last declared topic section and nothing after it.
bekle "[sema] seçenekler son bölüm: Ağ ve Kimlik"

# EVERY PROJECT-SCOPED SETTING, and the page is generated from the catalogue —
# so this count moves when a setting is declared, never because somebody
# remembered to add a row (CLAUDE.md 5.10).
bekle "[sema] proje ayarı: 11"

# The two nobody would think to look for outside their own topic page, which is
# the reason the gathered page exists at all.
if ! grep -q "core.crs.id" <<<"$cikti" || ! grep -q "core.plan.olcek" <<<"$cikti"; then
    echo "oznitelik-semasi: 'Proje Ayarları' sayfası proje kapsamlı ayarları toplamıyor" >&2
    echo "oznitelik-semasi:   -> src/app/src/settings_dialog.cpp:1" >&2
    fail=1
fi

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "oznitelik-semasi: OK — beş türde sütun pencereden tanımlanıyor, ondalık basamağı ve"
echo "oznitelik-semasi:   zorunluluk komut satırına geçiyor, düzenleme ve silme çalışıyor;"
echo "oznitelik-semasi:   başka bir katmana tanımlanan sütun bu sayfada görünmüyor"
