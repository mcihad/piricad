#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the capability inventory, and the gap between what a person can do and
# what an agent can do.
#
# CLAUDE.md 2.8: the command set is the AI's TRAINING SURFACE — "a capability
# that exists only as a mouse gesture is a capability the AI can never be
# taught". Article 1.2 says every client is equal. So a command WITHOUT
# `Flags::AiAccessible` is a decision, and this gate makes the decision be
# written down instead of assumed.
#
# The inventory is taken from the LIVE registries by `kentos_envanter`, not by
# grepping the tree for `return CommandSpec{...}`: that grep misses every command
# a module registers through a loop or a helper, and it cannot see a parameter's
# arity, its word list or its range. It reported 81 commands where the program
# has 93.
#
# Failing conditions:
#   * a command is closed to agents and has no row in `tests/support/ai-kapsam.json`
#   * a row names a command that no longer exists
#   * a row names a command that is now open (delete the row)
#
# The gate REGENERATES rather than trusting a checked-in copy, and it FAILS
# rather than skipping when the generator is not built — a freshness check that
# skips has never run (CLAUDE.md 6.14).
set -euo pipefail

kok="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
liste="$kok/tests/support/ai-kapsam.json"

# IN PRESET ORDER, not in glob order — `build/*` sorts `asan` before `dev`, and a
# sanitiser tree left over from an earlier session would answer with a registry
# from before the change being tested (the same trap `ci-gate-docs.sh` fell into).
arac=""
if [[ -n "${1:-}" && -x "$1/bin/kentos_envanter" ]]; then
    arac="$1/bin/kentos_envanter"
else
    for aday in dev release debug asan headless; do
        if [[ -x "$kok/build/$aday/bin/kentos_envanter" ]]; then
            arac="$kok/build/$aday/bin/kentos_envanter"
            break
        fi
    done
fi

if [[ -z "$arac" ]]; then
    echo "envanter: kentos_envanter derlenmemiş, dolayısıyla ajan kapsamı ÖLÇÜLEMEDİ." >&2
    echo "envanter:   Önce derleyin (make build). Ölçemeyen bir kapı sessizce geçemez." >&2
    exit 1
fi
if [[ ! -f "$liste" ]]; then
    echo "envanter: $liste yok" >&2
    exit 1
fi

cikti="$(mktemp)"
trap 'rm -f "$cikti"' EXIT
"$arac" > "$cikti"

KENTOS_ENVANTER="$cikti" KENTOS_KAPSAM="$liste" python3 - <<'PY'
import json, os, sys

envanter = json.load(open(os.environ["KENTOS_ENVANTER"], encoding="utf-8"))
kapsam = json.load(open(os.environ["KENTOS_KAPSAM"], encoding="utf-8"))

komutlar = envanter["komutlar"]
kapali_kayit = kapsam["kapali"]

acik = {c["kimlik"] for c in komutlar if c["bayraklar"]["yapay_zeka"]}
kapali = {c["kimlik"] for c in komutlar if not c["bayraklar"]["yapay_zeka"]}
var = {c["kimlik"] for c in komutlar}

hata = 0

# A command closed to agents with nobody having said why.
for kimlik in sorted(kapali - set(kapali_kayit)):
    print(f"envanter: '{kimlik}' ajana kapalı ve gerekçesi yazılı değil -> "
          f"tests/support/ai-kapsam.json", file=sys.stderr)
    hata = 1

# A row that outlived its command, or one that was opened and left behind.
for kimlik in sorted(set(kapali_kayit) - var):
    print(f"envanter: '{kimlik}' artık kayıtlı bir komut değil; satırı silin", file=sys.stderr)
    hata = 1
for kimlik in sorted(set(kapali_kayit) & acik):
    print(f"envanter: '{kimlik}' artık ajana AÇIK; ai-kapsam.json'daki satırı silin", file=sys.stderr)
    hata = 1

# Every row says why, and either names the work package that will open it or
# says out loud that it is meant to stay shut.
for kimlik, satir in sorted(kapali_kayit.items()):
    if not satir.get("sebep"):
        print(f"envanter: '{kimlik}' satırında 'sebep' yok", file=sys.stderr)
        hata = 1
    if not satir.get("plan") and not satir.get("kalici"):
        print(f"envanter: '{kimlik}' ne bir plana bağlı ne de kalıcı; birini yazın",
              file=sys.stderr)
        hata = 1

toplam = len(komutlar)
gecici = sum(1 for s in kapali_kayit.values() if s.get("plan"))
kalici = sum(1 for s in kapali_kayit.values() if s.get("kalici"))
oran = 100.0 * len(acik) / toplam if toplam else 0.0

if hata:
    sys.exit(1)

print(f"envanter: {toplam} komut, {len(acik)} ajana açık (%{oran:.0f}), "
      f"{len(kapali)} kapalı — {gecici} plana bağlı, {kalici} bilerek kalıcı")
PY
