#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: nothing in the QRhi backend outlives the resource it names.
#
# THE DEFECT THIS EXISTS FOR. `RhiBackend` grows its uniform buffer to a high-
# water mark: the branch is taken only when a scene wants more uniform slots than
# every scene before it did. Three sets of shader resource bindings name that
# buffer BY ADDRESS — the shared one, the text one, and one cached per published
# picture — and only the first was re-pointed at the replacement. The other two
# went on naming a buffer that had been destroyed, and the driver read it:
#
#     QRhiWidget::paintEvent -> QRhi::endOffscreenFrame -> libgallium -> SIGSEGV
#
# It reached the screen as an occasional crash on a big drawing and nothing at
# all on a small one, which is the shape of every use-after-free.
#
# WHY IT IS A SHELL GATE. Same reason as `ci-gate-butce.sh`: /tests links no Qt
# (Article 3.4) and a QRhi backend is Qt by definition, so the only place this can
# be exercised is the application. And the only thing that turns "sometimes
# crashes" into a test that fails every time is a sanitizer, so this gate wants an
# ASan build and reports PENDING without one rather than passing (data.md
# Enforcement).
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="$kok/build/asan/bin/kentos_cad"

if [[ ! -x "$exe" ]]; then
    echo "omur: BEKLEMEDE — build/asan/bin/kentos_cad yok. 'cmake --build --preset asan'"
    echo "omur:   sonrasi bu kapi gercek olcum yapar; sanitizer'siz kosmak"
    echo "omur:   'bu sefer cokmedi' demektir, 'dogru' demek degil."
    exit 0
fi

# Counted rather than matched: `grep -q` stops reading at the first hit, `nm`
# dies of SIGPIPE, and `pipefail` then calls a successful test a failure.
asan_semboller="$(nm -D "$exe" 2>/dev/null | grep -c '__asan' || true)"
if [[ "$asan_semboller" -eq 0 ]]; then
    echo "omur: BEKLEMEDE — build/asan/bin/kentos_cad sanitizer'siz derlenmis."
    exit 0
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "omur: BEKLEMEDE — ortamda ekran yok. Kaynak omru ancak cizilen bir karede"
    echo "omur:   olculur; cizilmemis bir kare hicbir seyi kanitlamaz."
    exit 0
fi

# The heaviest scene the repository carries, and heavy is the point: the growth
# branch is only reached when a drawing outgrows the initial 64 uniform slots.
sahne="tests/bench/sahne/desen-yuku.json"
if [[ ! -f "$kok/$sahne" ]]; then
    echo "omur: $sahne yok — ATLANDI"
    exit 0
fi

cd "$kok"

# The budget probe draws a real frame the real number of times. Its EXIT CODE is
# deliberately ignored: an ASan build is far outside the 16 ms budget and saying
# so is `ci-gate-butce.sh`'s job, not this one's. A death by signal is not
# ignored — that is exactly the failure being tested for.
set +e
cikti="$(ASAN_OPTIONS=detect_leaks=0:abort_on_error=0 \
         UBSAN_OPTIONS=print_stacktrace=1 \
         KENTOS_DATA="$kok/data" KENTOS_RHI_OMUR=1 KENTOS_BUDGET_PROBE=8 \
         "$exe" --betik "$sahne" 2>&1)"
rc=$?
set -e

fail=0

if [[ $rc -ge 128 ]]; then
    echo "omur: sinyal $((rc - 128)) ile oldu — bir kaynak kendisini adlandiran seyden" >&2
    echo "omur:   once serbest birakildi." >&2
    fail=1
fi

if grep -q "AddressSanitizer\|runtime error:" <<<"$cikti"; then
    echo "omur: sanitizer bir bellek hatasi bildirdi:" >&2
    grep -m 20 -A 8 "AddressSanitizer\|runtime error:" <<<"$cikti" >&2
    fail=1
fi

# THE TEST MUST HAVE RUN. Without this the gate would go green on a scene that
# never grew its uniform buffer, and would keep going green after the branch it
# guards stopped being reachable at all.
buyume="$(grep -c '^\[omur\] tekduzen tampon' <<<"$cikti" || true)"
if [[ "$buyume" -eq 0 ]]; then
    echo "omur: tekduzen tampon hic buyumedi — bu kapi hicbir seyi olcmedi." >&2
    echo "omur:   Sahne kuculdu ya da yuksek su seviyesi mantigi degisti; ikisi de" >&2
    echo "omur:   kapinin kendisini duzeltmeyi gerektirir." >&2
    fail=1
fi

if [[ $fail -ne 0 ]]; then
    exit 1
fi

echo "omur: OK — tampon $buyume kez buyudu, her buyumede baglamalar takip etti; sanitizer sessiz"
