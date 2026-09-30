# Plugin API — Rules

> Scope: `/src/plugin-api`, `/tests/unit/plugin_*`, `/scripts/ci-gate-abi.sh`, `/scripts/ci-gate-visibility.sh`  |  Depends on: `piricad_command` (via the bus only), `piricad_core` types by value only — never Qt, never `piricad_app`  |  Source: piricad.md §4.3, §2.5, §2.6, §1, §8, §11 (Faz 5), §13

The public surface is ONE header: `/src/plugin-api/include/piricad_plugin.h`, built as CMake target
`piricad_plugin_api`. It does not exist in Phase 0; these rules bind the first commit that creates it.

## Hard Rules

**R1** — The whole boundary SHALL be C99: `extern "C"`, fixed-width integers, `const char*` UTF-8,
opaque handle typedefs (`piricad_host*`, `piricad_plugin*`), and POD structs with explicit padding.

**R2** — Coordinates SHALL cross as `int64_t` millimetres, bit-identical to `Mm` in
`piricad/core/units.hpp`. Doubles MUST NOT be used for stored geometry on the boundary.

**R3** — Every plugin SHALL export `uint32_t piricad_plugin_abi_version(void)` and
`int piricad_plugin_init(const piricad_host_v1*, piricad_plugin**)`. The host SHALL run the
handshake before any other call: major mismatch → refuse to load; plugin minor > host minor →
refuse to load; plugin minor < host minor → load (§13 API stability policy).

**R4** — Within one major version the ABI SHALL be additive only: new function pointers appended to
the END of `piricad_host_vN`, never inserted; every versioned struct SHALL carry `uint32_t size` as
its first field and callers SHALL check it before touching an appended member.

**R5** — All state mutation SHALL go through the command bus, exactly as a script does (§4.3):
`host->submit(cmd_id, argv, argc, piricad_error*)` where arguments serialise to `Value`
(`piricad/command/value.hpp`). Read APIs may be rich and direct, but read-only (§4.3).

**R6** — Plugin-submitted commands SHALL traverse the identical `Bus` path — dispatch → validate →
transact → journal (§2.6) — with the same `Transaction` and single-undo-step semantics as §2.5.
Plugins get no privileged path; see `.claude/command.md`.

**R7** — Every entry point SHALL be `noexcept` and report failure as `piricad_error {int32_t code;
const char* message;}` mapping 1:1 onto `Error{code,message}` from `piricad/core/result.hpp`. The
host SHALL wrap every inbound call in `try/catch(...)` and every outbound callback likewise.

**R8** — Each plugin SHALL ship `plugin.json` declaring: `id`, `version` (semver), `abi_major`,
`license` (SPDX id), `capabilities` (explicit list), `commands` (ids it registers), `signature`.
A missing or malformed field SHALL fail the load with an actionable message.

**R9** — Capabilities SHALL be granted only from that declared list and mapped to the same sandbox
levels as scripts (§4.3): `güvenli` (no filesystem, no network), `proje` (project directory only),
`tam` (explicit user approval). Default on load is `güvenli`. See `.claude/script.md`.

**R10** — Plugins SHALL load from the signed plugin repository. An unsigned or signature-invalid
plugin SHALL load only after a blocking, explicitly worded user consent dialog, and the host SHALL
record `plugin_id`, hash, and the user decision as a `{kind:"meta"}` `Journal` line
(`.claude/command.md` R20) (§4.3).

**R11** — A plugin fault SHALL NOT terminate the host: `tam`-capability plugins SHALL run
out-of-process; in-process plugins SHALL be loaded under a guarded call that on fault unloads the
plugin, rolls back the open `Transaction` (§2.5), flushes the `Journal`, and keeps the session alive.

**R12** — Plugin command ids SHALL be namespaced `plugin.<vendor>.<name>`, and registration SHALL be
refused if the id or any Turkish/English alias collides with an entry already in `Registry`.

**R13** — `piricad_plugin_api` SHALL build with `-fvisibility=hidden -fvisibility-inlines-hidden`
(MSVC: no auto-export) and export exactly the symbols listed in
`/src/plugin-api/piricad_plugin_api.exports`.

**R14** — A plugin that links the application is a derivative work: its declared `license` MUST be
GPLv3-compatible (§1) and the host SHALL refuse to load a plugin whose `license` field is absent or
outside the allowed SPDX set.

**R15** — ABI changes SHALL follow semantic versioning with a published deprecation window (§13): a
deprecated entry point stays functional for at least 2 minor releases or 12 months, whichever is
longer, and is marked `PIRICAD_DEPRECATED` from the release that deprecates it.

## Absolute Prohibitions

**P1** — NEVER put a C++ class, template, reference, STL container/string, `Result<T>`, `Task<T>`, or
any `<Q...>` type in `piricad_plugin.h`. BANNED without exception.

**P2** — NEVER let an exception, `std::terminate`, or a C++ ABI detail (name mangling, RTTI, vtable
layout, `new`/`delete` across the line) cross the boundary.

**P3** — NEVER break ABI inside a major version: no removing, reordering, resizing, or changing the
signature of an existing struct member or function pointer. Add a new major instead.

**P4** — NEVER hand a plugin a `Document`, `Layer`, `Registry`, `Bus`, `Transaction`, or `UndoStack`
pointer, const or otherwise. Direct mutation of the Document from a plugin is BANNED.

**P5** — NEVER load an unsigned plugin silently, from a CLI flag alone, or with a remembered
"always allow". Consent is per plugin, per version.

**P6** — NEVER expose an API that skips validation, the transaction boundary, or the journal — no
"fast path", no "trusted plugin" flag, no direct geometry writer.

**P7** — NEVER let a plugin register, override, or shadow a `core.` command id or any of its aliases
(`ÇİZGİ`, `GERİAL`, `KATMAN`, `SİL`, …). Shadowing is a load-time rejection, not a warning.

**P8** — NEVER grant a capability not present in `plugin.json`, and NEVER escalate a sandbox level at
runtime without a fresh user approval.

**P9** — NEVER link Qt, GDAL, or any GPL-incompatible library into `piricad_plugin_api`, and NEVER
put an `#if`/`#ifdef` on a `PIRICAD_*` macro inside `piricad_plugin.h`.

## Definitions of Done

- [ ] `piricad_plugin.h` compiles clean as C99 with `-Wall -Wextra -Werror -pedantic`.
- [ ] `abidiff` against the previous release tag reports additive-only changes, or the major bumped.
- [ ] Version handshake test covers: major mismatch, newer minor, older minor, missing export.
- [ ] Sandbox test proves `güvenli` blocks filesystem + network and `proje` blocks paths outside it.
- [ ] Crash-isolation test: a plugin that faults leaves the host running with the journal intact.
- [ ] Reference plugin registers `plugin.test.echo`; a shadow attempt on `core.line` is rejected.
- [ ] `nm -D --defined-only` matches the export list; deprecations dated in `/docs/api-stability.md` (`.claude/build.md` R21).

## Enforcement

| Gate | Mechanism |
|---|---|
| ABI compatibility | `scripts/ci-gate-abi.sh` — `abidiff` current `piricad_plugin_api` vs previous release tag; non-additive diff fails CI |
| Symbol visibility | `scripts/ci-gate-visibility.sh` — `-fvisibility=hidden` asserted in CMake, `nm -D` diffed against `piricad_plugin_api.exports` |
| No C++/Qt in header | `scripts/ci-gate-abi.sh` greps `piricad_plugin.h` for `class`, `template`, `std::`, `#include <Q`, and `#if`/`#ifdef PIRICAD_`; any hit fails (P1, P9) |
| Sandbox, capabilities, crash isolation | `tests/unit/plugin_sandbox_test.cpp`, `tests/unit/plugin_crash_test.cpp` |
| Bus/journal path | `tests/journal/` — replaying a plugin session reproduces identical state |
| Signature + license | `tests/unit/plugin_manifest_test.cpp` |
