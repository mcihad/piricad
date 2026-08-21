#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# GATE: the document model stays the shape .claude/model.md settled on.
#
# piricad.md §10.2 / §10.3 / §12 — every rule below costs something measurable, and
# every one of them is cheaper than the failure it prevents:
#
#   R21/P8  No stored field is floating point. A coordinate, a width, an angle or a
#           scale that round-trips through a double is not bit-identical across x86
#           and Apple Silicon, and an alan hesabı is a legal output (§7.3, §12).
#   R20/P9  A line width is PAPER micrometres, never pixels. MPYY prescribes çizgi
#           kalınlığı in mm on the pafta and DXF group 370 is 1/100 mm on paper; a
#           width stored in pixels round-trips to neither.
#   R22/P1  No vptr, no std::function, no owning pointer in an entity, style, layer
#           or attribute record. Five million parcels pay for every one of them.
#   R1/R5/P4  Every id that reaches a file, a journal line, a selection or an AI tool
#           call is a persistent KEY; every id inside a frame is a dense SLOT. A slot
#           in a journal line is a document that replays onto the wrong parcel.
#   R24/P10 No process-wide mutable registry in /src/core. The kind table is owned by
#           the Document and passed by reference, like every other piece of core state
#           (core.md P8: a mutable global breaks determinism and reentrancy).
#   R6/P3   The cull block is closed: the per-frame cull test reads the four bbox
#           arrays and one flags byte, nothing else. Adding a column is a memory-
#           traffic regression on the frame path that is already the slowest measured
#           scenario (§10.1: pan/zoom over 5M polygons, 16 ms).
#   R9/P6   Geometry is entity -> rings -> vertices. A single (start, count) vertex run
#           cannot express a parcel with a hole, and yola terk and irtifak routinely
#           produce one.
#
# Two checks are deliberately named heuristics rather than parsers, because a shell
# gate that guesses at C++ scope must fail towards silence, never towards a false
# positive that blocks every build:
#   * the R6 cull region is the enclosing brace-block above the bbox read;
#   * the P4 signature test reads one declaration line, not a continuation.
#
# ---------------------------------------------------------------------------
# PHASE-0 CARVE-OUTS. Three files predate model.md and are superseded by it. Each
# is named here with its removal condition so the exemption cannot go unnoticed;
# the gate reports the count on every run. Nothing else may be added without a
# matching entry in CLAUDE.md Article 8.
#
#   src/core/include/piricad/core/document.hpp
#       LayerStyle::width_px (P9, P8) and PolylineStore's (start, count) run (P6).
#       Superseded by style.hpp's Appearance::width_um, layer.hpp's Layer and
#       geometry.hpp's RingGeometry. Dies when Document is rebuilt on them.
#   src/render/src/scene.cpp
#       Reads poly.layer[e] before the bbox test (R6). The Phase-0 PolylineStore
#       carries no R7 flags byte, so layer_hidden cannot be mirrored and the layer
#       visibility test cannot fold into flags. Dies with the flags byte.
#   src/core/src/log.cpp
#       A mutable process-wide sink and level (P10). core.md P9 bans a logging sink
#       in core outright; dies when logging moves out of /src/core.
# ---------------------------------------------------------------------------
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fail=0
core_inc="$root/src/core/include/piricad/core"

legacy_doc="$core_inc/document.hpp"
legacy_scene="$root/src/render/src/scene.cpp"
legacy_log="$root/src/core/src/log.cpp"

# A shell gate cannot parse C++, but it can refuse to read a comment as code.
# Strips a // tail; a line that is nothing but comment comes back empty.
strip() { local s="${1%%//*}"; printf '%s' "$s"; }

# ------------------------------------------------------- R21 / P8: no floats ---
#
# Scope is the records model.md settles: identity, geometry, style, layer,
# attribute, settings, entity_kind, crs, units. json.hpp is out — a Json is a
# transient parse tree, not a stored record, and JSON numbers are doubles by
# definition. A transient double local and an mm_to_metres-style conversion are
# explicitly allowed (core.md R3), so a line carrying '(' is never a member.
fp_member='^[[:space:]]*(mutable[[:space:]]+)?(float|double|long[[:space:]]+double)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*[{=;[]'
fp_container='(vector|array|span|deque|optional|pair|tuple)[[:space:]]*<[^;]*\b(float|double)\b'

for h in identity geometry style layer attribute settings entity_kind crs units; do
    f="$core_inc/$h.hpp"
    [[ -f "$f" ]] || continue
    [[ "$f" == "$legacy_doc" ]] && continue
    while IFS= read -r hit; do
        ln="${hit%%:*}"
        code="$(strip "${hit#*:}")"
        [[ "$code" == *'('* ]] && continue
        if grep -qE "$fp_member|$fp_container" <<<"$code"; then
            echo "model: floating-point member in a stored record (R21/P8) -> $f:$ln" >&2
            fail=1
        fi
    done < <(grep -nE "$fp_member|$fp_container" "$f" || true)
done

# --------------------------------------------------------- R20 / P9: no pixels ---
#
# Core stores paper micrometres. Screen pixels are derived per frame in /src/render
# and never persisted, so a *_px identifier anywhere in a core header is a stored
# pixel by construction.
while IFS= read -r hit; do
    f="${hit%%:*}"; rest="${hit#*:}"; ln="${rest%%:*}"
    [[ "$f" == "$legacy_doc" ]] && continue
    code="$(strip "${rest#*:}")"
    if grep -qE '\b[A-Za-z_][A-Za-z0-9_]*_px\b' <<<"$code"; then
        echo "model: line width stored in pixels, not paper µm (R20/P9) -> $f:$ln" >&2
        fail=1
    fi
done < <(grep -rnE '\b[A-Za-z_][A-Za-z0-9_]*_px\b' "$core_inc" || true)

# ------------------------------------- R22 / P1: no vptr, functor or owning ptr ---
#
# The records themselves. entity_kind.hpp is checked for virtual/std::function only:
# R22 mandates free FUNCTION POINTERS there, so a pointer check would flag the rule
# it is enforcing.
ptr_member='^[[:space:]]*(const[[:space:]]+)?[A-Za-z_][A-Za-z0-9_:]*([[:space:]]*<[^>]*>)?[[:space:]]*\*[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*[{=;]'
own_ptr='\b(unique_ptr|shared_ptr)[[:space:]]*<'

for h in geometry style layer attribute; do
    f="$core_inc/$h.hpp"
    [[ -f "$f" ]] || continue
    while IFS= read -r hit; do
        ln="${hit%%:*}"
        code="$(strip "${hit#*:}")"
        [[ -z "$code" ]] && continue
        if grep -qE '\bvirtual\b' <<<"$code"; then
            echo "model: virtual function in an entity/style/layer/attribute record (R22/P1) -> $f:$ln" >&2
            fail=1
        fi
        if grep -qE 'std::function' <<<"$code"; then
            echo "model: std::function in an entity/style/layer/attribute record (R22/P1) -> $f:$ln" >&2
            fail=1
        fi
        if grep -qE "$own_ptr" <<<"$code"; then
            echo "model: owning pointer in an entity/style/layer/attribute record (R22/P1) -> $f:$ln" >&2
            fail=1
        fi
        if [[ "$code" != *'('* ]] && grep -qE "$ptr_member" <<<"$code"; then
            echo "model: pointer member in an entity/style/layer/attribute record (R22/P1) -> $f:$ln" >&2
            fail=1
        fi
    done < <(grep -nE "\bvirtual\b|std::function|$own_ptr|\*" "$f" || true)
done

f="$core_inc/entity_kind.hpp"
if [[ -f "$f" ]]; then
    while IFS= read -r hit; do
        ln="${hit%%:*}"
        code="$(strip "${hit#*:}")"
        if grep -qE '\bvirtual\b|std::function' <<<"$code"; then
            echo "model: virtual or std::function in the kind table — R22 wants free function pointers -> $f:$ln" >&2
            fail=1
        fi
    done < <(grep -nE '\bvirtual\b|std::function' "$f" || true)
fi

# ------------------------------------------- R1 / R5 / P4: keys cross boundaries ---
#
# Persistence and selection speak EntityKey/LayerKey. A dense slot is valid only
# inside one in-memory Document, so a slot in a journal line, a serialised record or
# a selection replays onto whatever entity happens to hold that index next.
# Heuristic: one declaration line at a time — a signature broken across lines is not
# reassembled, and the gate stays silent rather than guessing.
persist_name='[A-Za-z_]*(journal|serial|select|to_json|from_json)[A-Za-z0-9_]*[[:space:]]*\('
while IFS= read -r hit; do
    f="${hit%%:*}"; rest="${hit#*:}"; ln="${rest%%:*}"
    code="$(strip "${rest#*:}")"
    base="$(basename "$f")"
    named=0
    grep -qiE "$persist_name" <<<"$code" && named=1
    [[ "$base" =~ (journal|serial|select) ]] && [[ "$code" == *'('* ]] && named=1
    [[ $named -eq 1 ]] || continue
    if grep -qE '\b(core::)?(EntityId|LayerId)\b' <<<"$code"; then
        echo "model: dense slot in a persistence or selection signature; use EntityKey/LayerKey (R1/R5/P4) -> $f:$ln" >&2
        fail=1
    fi
done < <(grep -rnE '\b(core::)?(EntityId|LayerId)\b' --include='*.hpp' --include='*.cpp' \
             "$root/src/core" "$root/src/command" || true)

# ------------------------------------------ R24 / P10: no mutable global in core ---
#
# Two shapes: a function-local `static` that is not const, and a namespace-scope
# variable that is not const. A `static const` table built once from a factory list
# is the sanctioned idiom (entity_kind.cpp, settings.cpp) and is left alone.
local_static='\bstatic\b[[:space:]]+[^;()]*\b[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=|;|\{)'
ns_var='^[A-Za-z_][A-Za-z0-9_:<>,*&[:space:]]*[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=|;|\{)'
ns_keyword='^[[:space:]]*(using|namespace|struct|class|enum|template|typedef|friend|return|extern|export|import|public|private|protected)\b'

while IFS= read -r f; do
    [[ "$f" == "$legacy_log" ]] && continue
    while IFS= read -r hit; do
        ln="${hit%%:*}"
        code="$(strip "${hit#*:}")"
        [[ -z "$code" ]] && continue
        grep -qE '\b(const|constexpr)\b' <<<"$code" && continue
        grep -qE "$ns_keyword" <<<"$code" && continue
        if grep -qE '\bstatic\b' <<<"$code" && ! grep -qE 'static_cast|static_assert' <<<"$code" \
           && grep -qE "$local_static" <<<"$code"; then
            echo "model: mutable function-local static in /src/core (R24/P10) -> $f:$ln" >&2
            fail=1
            continue
        fi
        # Namespace scope only: a member lives indented inside its class.
        [[ "$code" =~ ^[[:space:]] ]] && continue
        [[ "$code" == *'('* ]] && continue
        if grep -qE "$ns_var" <<<"$code"; then
            echo "model: mutable namespace-scope variable in /src/core (R24/P10) -> $f:$ln" >&2
            fail=1
        fi
    done < <(grep -nE '\bstatic\b|^[A-Za-z_]' "$f" || true)
done < <(find "$root/src/core" -name '*.hpp' -o -name '*.cpp' | sort)

# ---------------------------------------------------- R6 / P3: the cull block ---
#
# HEURISTIC, and deliberately so. The cull region is taken to be the enclosing
# brace-block above the line that reads the bounding box: scan back to the nearest
# line that opens a block at a strictly smaller indent. Inside that region only the
# four bbox arrays and the flags/alive byte may be read from the entity store; layer,
# kind, style, order and key are read only for the entities the index returned (R6).
scene="$legacy_scene"
cull_allowed='^(min_x|min_y|max_x|max_y|flags|alive|layer)$'   # `layer`: Phase-0 carve-out
if [[ -f "$scene" ]]; then
    bbox_line="$(grep -nE '\.(min_x|min_y|max_x|max_y)[[:space:]]*\[' "$scene" | head -1 | cut -d: -f1 || true)"
    if [[ -z "${bbox_line:-}" ]]; then
        # Never let the check vanish because the scene builder was restructured.
        echo "model: R6 cull-block check found no bounding-box read in $scene — the heuristic no longer locates the cull region; re-anchor it" >&2
        fail=1
    else
        # Nearest enclosing block: walk back to the last line that opens a brace at
        # a strictly smaller indent than the bbox read. That is the cull function or
        # lambda; anything nested tighter is already past the decision.
        region_start="$(awk -v end="$bbox_line" '
            NR == end { want = match($0, /[^ \t]/) - 1 }
            NR > end  { exit }
            {
                line = $0
                sub(/\/\/.*/, "", line)
                if (line !~ /\{[[:space:]]*$/) next
                indent = match(line, /[^ \t]/) - 1
                if (indent < 0) indent = 0
                lines[NR] = indent
            }
            END {
                best = 0
                for (n = 1; n < end; n++)
                    if (n in lines && lines[n] < want) best = n
                print best
            }' "$scene")"
        [[ "${region_start:-0}" -gt 0 ]] || region_start=1
        while IFS= read -r hit; do
            ln="${hit%%:*}"
            code="$(strip "${hit#*:}")"
            while read -r col; do
                [[ -z "$col" ]] && continue
                grep -qE "$cull_allowed" <<<"$col" && continue
                echo "model: cull block reads '$col' before the index query — R6 closes it to the four bbox arrays and flags -> $scene:$ln" >&2
                fail=1
            done < <(grep -oE '\.[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(\[|_of[[:space:]]*\()' <<<"$code" \
                     | sed -E 's/^\.//; s/[[:space:]]*(\[|_of.*)$//' | sort -u)
        done < <(sed -n "${region_start},${bbox_line}p" "$scene" | grep -nE '\.[A-Za-z_]' |
                 awk -v off="$((region_start - 1))" -F: '{ $1 = $1 + off; print $0 }' OFS=:)
    fi
fi

# ------------------------------------------------- R9 / P6: rings, not one run ---
#
# A store carrying a bare `start` AND a bare `count` member is a single vertex run
# per entity. RingGeometry spells its ring arrays ring_start/ring_count/first_ring/
# ring_total precisely so that this check has a distinctive shape to look for.
while IFS= read -r f; do
    [[ "$f" == "$legacy_doc" ]] && continue
    s="$(grep -nE '^[[:space:]]+[A-Za-z_][A-Za-z0-9_:<>,[:space:]]*[[:space:]]start[[:space:]]*[{;=]' "$f" | head -1 || true)"
    [[ -n "$s" ]] || continue
    grep -qE '^[[:space:]]+[A-Za-z_][A-Za-z0-9_:<>,[:space:]]*[[:space:]]count[[:space:]]*[{;=]' "$f" || continue
    echo "model: (start, count) single vertex run on an entity-indexed store — a parcel with a hole needs rings (R9/P6) -> $f:${s%%:*}" >&2
    fail=1
done < <(find "$root/src/core" -name '*.hpp' | sort)

if [[ $fail -eq 0 ]]; then
    echo "model: OK — no floating-point or *_px field in a stored record, no vptr/std::function/owning pointer in an entity, style, layer or attribute record, no dense slot in a persistence or selection signature, no mutable global in /src/core, cull block closed to the R6 columns (enclosing-block heuristic), no (start, count) vertex run outside RingGeometry"
    echo "model: note — 3 Phase-0 carve-outs exempted, each named with its removal condition in this script's header: document.hpp, render/src/scene.cpp, core/src/log.cpp"
fi
exit $fail
