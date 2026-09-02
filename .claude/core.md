# Core — Rules

> Scope: `/src/core` (CMake target `kentos_core`), plus its tests in `/tests/unit`, `/tests/golden`, `/tests/bench`  |  Depends on: **nothing** — not Qt, not any other KentOSCad module  |  Source: kentoscad.md §7.1, §7.3, §8, §9.1, §9.2, §10.2, §10.5, §13

## Hard Rules

R1. `kentos_core` MUST stay a static, Qt-free library; its `target_link_libraries` list SHALL contain only third-party libraries from §9.1/§9.2 — no `Qt6::*`, no other `kentos_*` target (§8).
R2. Every stored coordinate MUST be `Mm` (`int64` fixed-point millimetres) from `kentos_cad/core/units.hpp`; `Point2` and `Box2` SHALL hold `Mm` members only (§10.2).
R3. `double` MAY appear only as a transient local computation type inside a function body, and MUST be converted back to `Mm` before it reaches any struct member, container, or serialized field (§10.2).
R4. Vertex storage MUST be struct-of-arrays — separate `xs` and `ys` arrays per block, never one interleaved point array (§10.2).
R5. Entity and vertex references inside `Document` MUST be 32-bit indices, never raw or smart pointers into the store (§10.2).
R6. Each `Layer` MUST own one arena allocator; layer teardown SHALL be a single arena release, not per-entity destruction (§10.2).
R7. Geometry blocks and attribute blocks MUST live in separate allocations, so a draw traversal reads no attribute memory (hot/cold split, §10.2).
R8. Orientation, incircle and segment-side tests MUST route through the Shewchuk `predicates.c` wrapper compiled into `kentos_core` (§9.2).
R9. Core output MUST be bit-identical on Linux, Windows and macOS for identical input; any new geometry, index or CRS routine SHALL land together with a fixture in `/tests/golden` (§7.3, §10.5).
R10. Every fallible function MUST return `Result<T>` from `kentos_cad/core/result.hpp` with a populated `Error{code,message}`; the happy path SHALL NOT be signalled by a sentinel value (§7.1).
R11. Exceptions MUST be reserved for programmer errors (broken precondition, allocation failure); no public core function SHALL use `throw` as an expected outcome of valid input.
R12. C++20 is the baseline; any C++23 facility MUST be guarded by `#if __cpp_lib_*` with a C++20 fallback that is also compiled and tested in CI (§7.1).
R13. Coordinate and identity types (`Mm`, `Point2`, `Box2`, `EntityId`) MUST define `operator<=>` and `operator==` with total, value-based ordering (§7.1).
R14. SoA arrays MUST cross API boundaries as `std::span` (non-owning); a core function SHALL NOT take ownership of a caller's vertex buffer (§7.1, §10.2).
R15. All mutable state MUST be reachable only through an explicit `Document&`/`Layer&` parameter; a core function's result SHALL depend on nothing but its arguments.
R16. The spatial index MUST be built by bulk-load (STR packing); one-by-one `insert` in a loop is banned for initial construction (§10.5).
R17. Every public header under `/src/core/include/kentos_cad/core` MUST compile standalone under `-Wall -Wextra -Werror` on GCC 13+, Clang 17+ and MSVC 19.40+ (§7.2).
R18. Core translation units MUST compile with `-fno-fast-math -ffp-contract=off` (MSVC `/fp:precise`); the flags are set on the target, not per-file (§7.3).
R19. `Crs` in `kentos_cad/core/crs.hpp` MUST remain a value type (identifier plus parameters, e.g. `TUREF/TM30`); executing a transformation requires PROJ and belongs outside core — see `.claude/io.md` and `.claude/domain.md`.
R20. Rounding of a transient `double` to `Mm` MUST use the one shared half-away-from-zero rounding helper declared in `kentos_cad/core/units.hpp`; `static_cast<int64_t>`/`static_cast<Mm>` on a coordinate `double` is banned everywhere under `/src` outside that helper's own definition (§7.3).

## Absolute Prohibitions

P1. NEVER write `#include <Q...>`, `Q_OBJECT`, or any Qt type anywhere under `/src/core` — the CI gate breaks the build (§8).
P2. NEVER include a header from `/src/command`, `/src/io`, `/src/render`, `/src/script`, `/src/ai`, `/src/domain`, `/src/app`, or `/src/plugin-api`. The dependency direction is one-way (§8).
P3. NEVER add `-ffast-math`, `/fp:fast`, or `-ffp-contract=fast` to any core target, file, or pragma (§7.3).
P4. NEVER use `double`, `float` or `long double` as a *storage* format for a coordinate — no such member, container element, or file field (§10.2).
P5. NEVER introduce an array-of-structs vertex container as the primary geometry layout (§10.2).
P6. NEVER call `std::toupper`, `std::tolower`, or the `<cctype>` classifiers on Turkish text; core SHALL NOT case-fold user-facing text at all — the shared Turkish folding table lives in `/src/command` (`.claude/command.md` R7) and `QLocale` casing in the Qt layers (`.claude/ui.md` R25) (CLAUDE.md 5.6, §13).
P7. NEVER use C++20 modules (`export module`, `import`) — header + PCH only (§7.1).
P8. NEVER add a singleton, mutable global, function-local `static` non-const state, or thread-local cache to core (breaks determinism and reentrancy).
P9. NEVER perform I/O from core: no `<fstream>`, no `std::cout`/`std::cerr`, no sockets, no `std::getenv`, no logging sink. Persistence belongs to `/src/io`, see `.claude/io.md`.
P10. NEVER link or vendor the **Triangle** library (GPL-incompatible); use CDT or CGAL. Shewchuk `predicates.c` alone is permitted and required (§9.2).
P11. NEVER let a result depend on `rand()`, wall-clock time, pointer/address values, hash-order iteration of unordered containers, or uninitialized memory — cross-platform bit-identity is a legal requirement (§7.3).
P12. NEVER return a pointer or reference into arena memory from a public core API; return an index or a `std::span` whose validity ends at the next mutation (§10.2).
P13. NEVER put a mutex, condition variable, or self-synchronizing atomic inside a core type; concurrency and transaction boundaries are the caller's job, see `.claude/command.md` (§2.5).
P14. NEVER mutate `Document` outside a command path at higher layers; core exposes mutators, but calling them directly from `/src/domain` breaks the build (§8) — see `.claude/command.md`.

## Definitions of Done

- [ ] `scripts/ci-gate-core-purity.sh` exits 0 locally.
- [ ] Changed public headers compile standalone, `-Werror` clean, on all three compilers.
- [ ] `/tests/unit` covers at least one `Error` path per new `Result<T>` function, not only the success path.
- [ ] `/tests/golden` fixture added or updated, and its hash matches on the Linux, Windows and macOS CI runners.
- [ ] A `Mm` → transient `double` → `Mm` round-trip test exists for any new numeric routine.
- [ ] `/tests/bench` shows no regression above 10% against the recorded baseline (§10.1).
- [ ] `nm`/`dumpbin` on `kentos_core` shows no Qt symbol and no symbol from another `kentos_*` target.

## Enforcement

- **`scripts/ci-gate-core-purity.sh`** — greps `/src/core` for `<Q`/`Q_OBJECT`, upward `#include` paths, `export module`/`import `, `-ffast-math`/`/fp:fast`/`-ffp-contract=fast`, `std::toupper`/`std::tolower`, `<fstream>`/`std::cout`/`getenv`, `rand()`, AoS point containers, and `static_cast<int64_t>`/`static_cast<Mm>` outside the R20 rounding helper. Non-zero exit breaks the build (§8, R20).
- **`/tests/golden`** — bit-identical hash comparison of the same fixtures across the three-platform CI matrix (§7.3, §10.5).
- **`/tests/unit`** — built into the `kentos_tests` executable; covers `Result<T>` error paths, `operator<=>` totality, arena reuse, index bulk-load.
- **`/tests/bench`** — benchmark gate; >10% regression breaks the build (§10.1).
- **CMake** — `kentos_core` fails to configure if a `Qt6::*` or upward `kentos_*` target appears in its link interface, or if the float flags of §7.3 are absent.
