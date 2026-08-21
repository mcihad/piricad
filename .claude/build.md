# Build & Release Engine — Rules

> Scope: `/CMakeLists.txt`, `/cmake`, `/CMakePresets.json`, `/Makefile`, `/vcpkg.json`, `/packaging`, `/.github/workflows`, `/scripts/ci-gate-*.sh`  |  Depends on: every `piricad_*` target consumes this, it consumes none  |  Source: piricad.md §7.1, §7.2, §7.3, §8, §9.11, §13, §14

## Hard Rules

R1. `cmake_minimum_required(VERSION 3.28)`; every build entry point MUST be a `CMakePresets.json` preset (§8).
R2. Ninja MUST be the only generator on all three platforms (§8).
R3. Presets MUST be named `<os>-<config>`: `linux-debug`, `linux-release`, `linux-asan`, `windows-debug`, `windows-release`, `macos-debug`, `macos-release`. CI invokes these names and no ad-hoc `cmake -S -B` line.
R4. The target set is exactly `piricad_core`, `piricad_command`, `piricad_io`, `piricad_domain_geodesy`, `piricad_domain_cadastre`, `piricad_domain_planning`, `piricad_domain_surface`, `piricad_script`, `piricad_ai`, `piricad_render`, `piricad_plugin_api`, `piricad_app` (executable `piricad`), `piricad_tests` (CLAUDE.md 3.2–3.4). Phase 0 builds only `piricad_core`, `piricad_command`, `piricad_script`, `piricad_render`, `piricad_app`, `piricad_tests`; the rest land with their modules. A target outside this list requires an edit to this rule first.
R5. Every target not in CLAUDE.md 3.4's Qt-linked list (`piricad_render`, `piricad_app`) MUST NOT list any `Qt6::*` in `target_link_libraries` (see `.claude/core.md`).
R6. The standard MUST be set once at top level: `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`. No target may raise or lower it (§7.1).
R7. `/cmake/Compilers.cmake` MUST `message(FATAL_ERROR)` below the floor: MSVC 19.40 (VS 2022 17.10+), GCC 13 (prefer 14), Clang 17, AppleClang Xcode 15.3 (§7.2).
R8. One compiler and one standard library per platform; all dependencies MUST be built with the same toolchain as PiriCAD (§7.2).
R9. Release flags MUST be these exact strings, defined once in `/cmake/Flags.cmake` — MSVC: `/O2 /Oi /Gy /GL /fp:precise /DNDEBUG`; GCC/Clang/AppleClang: `-O2 -fno-fast-math -ffp-contract=off -flto=thin -fvisibility=hidden` (§7.3).
R10. `-ffp-contract=off` (`/fp:precise` on MSVC) MUST apply to every translation unit in every config, Debug and ASan included (FMA does not round the intermediate result, so the same code gives different answers on x86 and Apple Silicon; official documents require bit-identical area, intersection and adjustment results across platforms) (§7.3).
R11. Dependencies MUST come from vcpkg manifest mode and nowhere else: `/vcpkg.json` with a pinned `builtin-baseline` commit SHA plus explicit `version>=` / `overrides` per port (§8, CLAUDE.md 5.12).
R12. Every OPTIONAL external dependency MUST sit behind a `PIRICAD_WITH_<NAME>` option defaulting to `OFF` (GDAL, PROJ, GEOS, CGAL, LUA, PYTHON, TRACY, IMGUI, …). ON but missing MUST `message(FATAL_ERROR)` naming the package, the vcpkg port and the install command.
R12a. The mandatory dependencies — Qt 6, Qt Advanced Docking System (§9.4), HarfBuzz, FreeType, msdfgen — MUST be `find_package`d unconditionally with a pinned minimum version and MUST hard-fail when absent; they carry no `PIRICAD_WITH_*` option.
R13. ccache (Linux/macOS) and sccache (Windows) MUST be wired through `CMAKE_<LANG>_COMPILER_LAUNCHER` in the presets and cached in CI (§8).
R14. All `piricad_*` targets MUST build with `/W4 /WX` (MSVC) or `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang).
R15. clang-tidy, clang-format and IWYU MUST run as blocking CI jobs against `.clang-tidy`, `.clang-format` and the IWYU mapping file committed at the repo root (§9.11).
R16. The CI matrix MUST be 3 OS × {Debug, Release} plus a separate ASan+UBSan job, and MUST finish under 20 minutes per PR (§14).
R17. Packaging MUST go through CPack only — MSI (WiX), DMG, DEB, RPM, AppImage, Flatpak — with recipes under `/packaging/{windows,macos,linux}` (§8, §14).
R18. Every release job MUST emit a CycloneDX SBOM artifact `piricad-<version>-sbom.cdx.json` listing each linked dependency with resolved version and SPDX license id (GPL compliance requires it) (§9.11, §13).
R19. Release builds MUST be reproducible: `SOURCE_DATE_EPOCH` set, `-ffile-prefix-map` stripping absolute paths, no `__DATE__`/`__TIME__` in sources; two builds of one tag MUST hash identically (§13).
R20. Shipped binaries MUST be signed — Windows EV certificate, Apple Developer ID plus notarization and stapling. Unsigned artifacts MUST NOT be published (§13).
R21. Versioning MUST be SemVer from the single source `project(piricad VERSION …)`, and `/docs/api-stability.md` MUST be updated in the same PR that changes `/src/plugin-api` (§13).
R22. `/Makefile` MUST be a thin wrapper: one documented line per target calling `cmake --preset`, `cmake --build --preset`, `ctest --preset`, or a `/scripts` script. `make check` MUST run format + tidy + IWYU + every `scripts/ci-gate-*.sh` + `piricad_tests`.
R23. Qt MUST ship bundled in the base package (windeployqt / macdeployqt / AppImage payload) at the version pinned in `/vcpkg.json`. The embedded CPython 3.12 runtime MUST ship only inside the separate, optional Python module package — never in the base installer (§4.2; `.claude/script.md` R7, P13).
R24. Every blocking gate MUST be a script named `scripts/ci-gate-<job>.sh`; the R22 glob is the complete gate set. A gate implemented in another language MUST be invoked from such a wrapper, and a rulebook MUST cite gates only by these names.
R25. `/SECURITY.md` MUST be published, naming the security contact, the coordinated-disclosure process and a stated response SLA in days; a release job MUST fail if the file or the SLA figure is absent (§13).
R26. CI MUST publish Doxygen API documentation and MkDocs user documentation as blocking jobs, and the repository MUST carry `/CONTRIBUTING.md`, `/CODE_OF_CONDUCT.md`, plus a documented RFC process, a public roadmap with a release calendar, and sample datasets with tutorial content under `/docs` (§13).

## Absolute Prohibitions

P1. NEVER `-ffast-math`, `/fp:fast`, `-ffp-contract=fast`, or any per-target override of the R9 flag strings.
P2. NEVER C++20 modules — no `import`, no `export module`, no `FILE_SET CXX_MODULES`. Header + PCH only (§7.1).
P3. NEVER an unpinned dependency: no floating baseline, no branch refs, no `FetchContent` without a commit SHA, no `find_package` without a minimum version.
P4. NEVER link a system-provided Qt or Python into a shipped build.
P5. NEVER add a dependency whose `LICENSE` has not been read and recorded in the SBOM and `/NOTICE`.
P6. NEVER add Triangle (Shewchuk) or the ODA Drawings SDK — GPL-incompatible; Shewchuk `predicates.c` alone is allowed and required. NEVER a GPLv2-only dependency.
P7. NEVER put build logic in `/Makefile`: no compiler flags, no OS conditionals, no source lists, no dependency resolution.
P8. NEVER silence a warning — banned: `#pragma warning(disable)`, `-Wno-*`, `/WX-`, `COMPILE_OPTIONS -w`. Fix the code.
P9. NEVER merge with a red CI, and NEVER use `continue-on-error: true` or `|| true` on a gate step.
P10. NEVER hardcode absolute paths, machine names, or secrets in CMake, presets, or workflows; secrets come only from encrypted CI secrets.
P11. NEVER default a `PIRICAD_WITH_*` option to ON, and NEVER silently fall back to a stub when the package is missing.
P12. NEVER rename a target, a preset, or a packaged file layout without the version bump required by R21.

## Definitions of Done

- [ ] `cmake --preset <os>-release && cmake --build --preset <os>-release` is clean on Linux, Windows and macOS.
- [ ] `make check` is green (format, tidy, IWYU, all `ci-gate-*`, `piricad_tests`).
- [ ] Zero warnings; no new `-Wno-*` or pragma disable in the diff.
- [ ] New dependency: pinned in `/vcpkg.json`, gated by `PIRICAD_WITH_*` default OFF, license recorded in `/NOTICE`, SBOM regenerated.
- [ ] Flag diff checked character-by-character against R9.
- [ ] Benchmark gate reports no regression above 10%.

## Enforcement

- `scripts/ci-gate-core-purity.sh` — fails on `#include <Q` or a Qt link in `/src/core`, and on the R20 rounding violation (§8; see `.claude/core.md`).
- `scripts/ci-gate-layering.sh` — fails on `#include <Q...>` in any Qt-free target, on any reverse/lateral module include, and on a `/data` path included from `/src/**` (R5, CLAUDE.md 3.2–3.4).
- `scripts/ci-gate-command-mutation.sh` — fails on geometry mutation outside a command in `/src/domain` (§8; see `.claude/command.md`).
- `scripts/ci-gate-fp-flags.sh` — greps generated `compile_commands.json`: fails on any banned fp flag (P1) and on any TU missing `-ffp-contract=off` / `/fp:precise` (R9, R10).
- The complete gate set is the `scripts/ci-gate-*.sh` glob (R24): `core-purity`, `layering`, `command-mutation`, `fp-flags`, `hardcoded-thresholds`, `catalogs`, `corpus`, `repo-size`, `data-permits`, `render`, `i18n`, `abi`, `visibility`.
- Benchmark gate — `tests/bench` against the stored baseline; >10% regression fails the build (§10.1).
- SBOM job — CycloneDX generation plus license diff against `/NOTICE`; fails on an unrecorded component (R18, P5).
- clang-format / clang-tidy / IWYU jobs, the ASan+UBSan job, and `make check` are required status checks on the default branch (R15, R16, P9).
