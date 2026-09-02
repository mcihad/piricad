# Domain (Geodesy / Cadastre / Planning / Surface) — Rules

> Scope: `/src/domain/geodesy`, `/src/domain/cadastre`, `/src/domain/planning`, `/src/domain/surface`, plus the data they read in `/data/catalogs` and `/data/crs`  |  Depends on: `kentos_core`, `kentos_command` only  |  Source: kentoscad.md §2.5, §2.6, §7.3, §8, §9.2, §9.7, §9.9, §10.5, §12, §15

## Hard Rules

R1. Every regulatory constant, code list, threshold, symbol and annex table MUST live as versioned data under `/data/catalogs` (BÖHHBÜY detail codes and object catalogue, MPYY EK-1a/1b/1c/1ç/1d symbology, EK-1e detail catalogue) or `/data/crs` (geoid grids, transformation parameters). C++ in `/src/domain` only *interprets* them (§8 repo layout, §15 "data-driven catalogue architecture").
R2. Every catalogue file MUST carry the header block defined in `.claude/data.md` R2 and MUST validate against its schema in `/data/catalogs/schema/`. That rule is the single definition; domain code MUST NOT assume any other field set.
R3. Every domain catalogue MUST be loadable and replaceable at runtime without recompiling; a legislation update SHALL be a data package swap, not a patch release (§15).
R4. Every state mutation MUST be a `CommandSpec` registered in `Registry` via `KENTOS_COMMAND`, dispatched through `Bus`. Domain code receives `Context` and returns `Result<T>`; see `.claude/command.md`.
R5. Domain command ids MUST use the namespaces `geodesy.`, `cadastre.`, `planning.`, `surface.` and MUST declare, in `.claude/command.md` R7 order, Turkish primary + ASCII-folded Turkish + English + abbreviations — `İFRAZ, IFRAZ, SUBDIVIDE, İFR, IFR` (§5.6).
R6. Topology checks, geometry validity and regulatory rule evaluation MUST run inside the `Bus` validate stage, so mouse, command line, script and AI clients hit identical checks (§2.6).
R7. One legal operation MUST be one `Transaction` and one undo step. A validation failure MUST roll back completely; a half-applied ifraz, tevhit or 18. madde parcelation is a build-blocking defect (§2.5).
R8. All stored coordinates MUST be `Mm` / `Point2` fixed-point int64 from `kentos_cad/core/units.hpp`. Floating point is permitted only inside a computation, never as persisted cadastral geometry.
R9. Cadastral area computation MUST use exact integer or explicitly ordered accumulation with a documented summation order, and MUST produce bit-identical results on Linux, Windows and macOS (§7.3, §10.5).
R10. Every domain source file MUST compile under `-fno-fast-math -ffp-contract=off` (`/fp:precise`) and MUST use Shewchuk `predicates.c` for orientation/incircle rather than raw comparisons (§7.3, §9.2).
R11. `geodesy` MUST implement TUREF/ITRF96 with explicit epoch and velocity field, TM 3° zones 27/30/33/36/39/42/45 at scale factor 1.0, ED50/UTM 6°, and regional ITRF↔ED50 transformation (§12 Jeodezik).
R12. Every coordinate carried across a transformation MUST name its `Crs` (`kentos_cad/core/crs.hpp`, e.g. `TUREF/TM30`) and its epoch; an unlabelled coordinate MUST be rejected with `Error`.
R13. Orthometric heights MUST be derived by Helmert reduction using the currently published Türkiye geoid model grid from `/data/crs`; the grid file name and version MUST appear in the output report.
R14. `geodesy` MUST support TUSAGA-Aktif / CORS-TR workflows, RINEX ingest and NTRIP streams, and MUST run least-squares network adjustment for polygon, triangulation and GNSS baselines, emitting error ellipses with every adjusted point (§12, §9.7 Eigen/Ceres).
R15. `geodesy` MUST produce aplikasyon and röper krokisi outputs, the kontrol işleri belge ve çizelgeleri set, and MUST implement pafta subdivision and naming as catalogue-driven rules (§12 Jeodezik).
R16. `cadastre` MUST implement ifraz, tevhit, yola terk, ihdas, irtifak and cins değişikliği as commands, each emitting a tescil bildirimi, an area calculation table (alan hesap cetveli) and a control report, and MUST cover the LİHKAB iş akışları end to end through the same commands (§12 Kadastro).
R17. MEGSİS / TAKBİS integration MUST be a read/exchange adapter behind an interface; parcel data fetched from them MUST be validated and versioned before it enters `Document`. Transport and format handling belong to `/src/io` — see `.claude/io.md`.
R18. `planning` MUST generate PlanGML and validate it against the official XSD in-process before export, and MUST emit the incompatible-land-use report (§12, §9.9).
R19. `planning` MUST produce the complete e-Plan file set (PlanGML, vector, GeoTIFF, KML, plan notu, rapor) as one atomic command result; a partial set MUST fail (§12).
R20. `planning` MUST implement 3194 madde 18 parcelation with an explicit düzenleme sınırı, DOP computation and distribution tables (dağıtım cetvelleri), kamulaştırma under law 2942, and değer artış payı computation from the Planlı Alanlar İmar Yönetmeliği catalogue (§12 İmar ve Planlama).
R21. TAKS/KAKS and çekme mesafesi checks MUST read their limits from the Planlı Alanlar İmar Yönetmeliği catalogue; setback/offset geometry MUST use CGAL straight skeleton or Clipper2 integer offset, never an ad-hoc buffer (§9.2, §12).
R22. Exported themes MUST map onto the TUCBS 32 themes / 53 sub-themes and MUST ship ISO 19115/19139 metadata; unmapped themes MUST fail export (§12 Veri ve Kurumsal).
R23. Every regulatory rejection MUST return an `Error` whose message names the legislation article and the catalogue version used, in Turkish.
R24. Turkish text case conversion MUST use the shared Turkish folding table exposed by `kentos_command` (`.claude/command.md` R7); `std::toupper`/`std::tolower` on Turkish strings is banned (CLAUDE.md 5.6, §13).
R25. Every domain rule change MUST land with a `/tests/golden` case whose expected values come from a TKGM/official reference dataset, and MUST be signed off by the surveying-engineer or urban-planner reviewer (§16.7, §16.9).
R26. Domain sub-modules MUST build as Qt-free static targets `kentos_domain_geodesy`, `kentos_domain_cadastre`, `kentos_domain_planning`, `kentos_domain_surface`, linking only `kentos_core` and `kentos_command`.

## Absolute Prohibitions

P1. NEVER hard-code a regulatory value in C++ (CLAUDE.md 5.13) — in `/src/domain` this is enforced by `scripts/ci-gate-hardcoded-thresholds.sh`.
P2. NEVER mutate `Document`, `Layer` or any geometry outside a command executing inside a `Transaction`. Direct geometry mutation in `/src/domain` BREAKS THE BUILD (§8 CI gate).
P3. NEVER accumulate a cadastral area in floating point with unspecified summation order, and NEVER use `-ffast-math`, `/fp:fast` or `-ffp-contract=fast` (§7.3).
P4. NEVER emit a transformed coordinate without naming, in the result, the grid file or parameter-set id used (R13); if that file or id is absent the call MUST return `Error`, never a computed value.
P5. NEVER ship or reference a symbol set, code list or annex table that has not been checked against the currently published MPYY / BÖHHBÜY annex; a catalogue whose `valid_until` has passed, or whose `published` is absent, BANS the release (`.claude/data.md` R2, R5).
P6. NEVER implement a domain rule only in the UI layer, a dialog, or an AI prompt. If it is law, it lives on the bus (§2.6) — see `.claude/ui.md` and `.claude/ai.md`.
P7. NEVER let AI or script clients bypass validation, and NEVER take a coordinate from model-generated text.
P8. NEVER `#include <Q...>` in `/src/domain`, and NEVER depend on `/src/io`, `/src/render`, `/src/script`, `/src/ai` or `/src/app` (§8 dependency direction).
P9. NEVER commit an official output (tescil bildirimi, cetvel, PlanGML, kroki) that is not reproducible byte-for-byte from the journalled command sequence.
P10. NEVER link Triangle (Shewchuk triangulator) or ODA Drawings SDK; use CDT/CGAL and Clipper2 (§9.2, §9.8).
P11. NEVER write a catalogue lookup that silently falls back to a default when the key is missing — return `Error`.

## Definitions of Done

- [ ] New/changed regulatory values live in `/data/catalogs` or `/data/crs` with the `.claude/data.md` R2 header complete; nothing new in C++ literals.
- [ ] Operation is a registered command with Turkish + English names, one `Transaction`, one undo step, rollback tested.
- [ ] Validation runs on the bus and is proven from a script/JSON client, not only the GUI.
- [ ] `/tests/golden` case added with TKGM/official reference values; three-platform bit-identical run is green.
- [ ] Output documents (cetvel, tescil bildirimi, PlanGML, e-Plan set, kroki) regenerate identically from the journal.
- [ ] Error messages cite article + catalogue version.
- [ ] Domain-expert (harita mühendisi / şehir plancısı) review recorded on the PR.

## Enforcement

- `scripts/ci-gate-command-mutation.sh` — fails the build on geometry mutation outside a command in `/src/domain`.
- `scripts/ci-gate-layering.sh` — fails on `#include <Q` and on forbidden module dependencies in `/src/domain` (P8).
- `scripts/ci-gate-hardcoded-thresholds.sh` — fails on any integer or float literal under `/src/domain` outside the allowlist `0, 1, -1, 2` that is not on a line annotated `// catalog-key`, and on any occurrence of the banned tokens `TAKS`, `KAKS`, `detay_kodu`, `gosterim_kodu`, `EK-1`, `tema_id` (P1).
- `scripts/ci-gate-catalogs.sh` — the single catalogue validator, shared with `.claude/data.md`: schema-validates every file in `/data/catalogs` and rejects a missing `published` or a past `valid_until` (R2, P5).
- `/tests/golden` — three-platform bit-identical comparison against TKGM reference coordinates and reference area tables (R9, R25).
- `/tests/journal` — replay of the command journal must reproduce official outputs byte-for-byte (P9).
- Review convention (not a gate): domain-expert sign-off on any change under `/src/domain` or `/data/catalogs` (CLAUDE.md 6.11).
