# I/O Engine — Rules

> Scope: `/src/io` (GDAL/OGR wrapper, native mmap-able format, DXF/DWG, GML/XML, LAS/LAZ, OGC services), `/tests/fuzz`, `/tests/golden` format cases  |  Depends on: `piricad_core`, `piricad_command` only (canon dependency table)  |  Source: piricad.md §9.2, §9.3, §9.8, §9.9, §9.10, §9.11, §10.1–§10.3, §10.5, §13, §15

## Hard Rules

R1. Every format access MUST go through a thin wrapper in `/src/io`. Public headers under `piricad/io/` MUST expose only core/command types (`Document`, `Layer`, `EntityId`, `Mm`, `Point2`, `Box2`, `Crs`, `Result<T>`, `Task<T>`) and MUST return `Result<T>` on failure.
R2. GDAL/OGR/LibreDWG/libdxfrw/libxml2/PDAL headers MUST appear only in `.cpp` files under `/src/io`. Wrapper classes MUST hide them behind pimpl or forward declarations (§9.2).
R3. `/src/io` MUST link `piricad_core` + `piricad_command` + format libraries only. No `#include <Q...>` anywhere in `/src/io`.
R4. Every import and export MUST be a command registered in `Registry` via `PIRICAD_COMMAND`/`CommandSpec`, with Turkish primary + English names; readers MUST NOT mutate `Document` directly (see `.claude/command.md`).
R5. The native project format MUST be a single file that is columnar (SoA blocks mirroring the `Document` polyline store), 8-byte aligned, offset-addressed with `u64`, and usable by `mmap` with zero parsing of geometry blocks (§9.3, §10.2).
R6. The native format MUST embed 4–5 precomputed Douglas–Peucker LOD levels written into quadtree tiles (§10.3) and a bulk-loaded STR R-tree (§10.5). Rebuilding either on open is a bug.
R7. All coordinates in the native format MUST be stored as `Mm` (int64 fixed-point millimetres); the file MUST NOT contain floating-point geometry (canon).
R8. Every format PiriCAD writes MUST begin with a magic string, a `u32 format_version`, and a `u32 min_reader_version` inside the first 32 bytes (§13).
R9. A reader whose version is below `min_reader_version` MUST return `Error{code="io.format_too_new", message=...}` naming the required application version — never crash, never a partial load (§13).
R10. Unknown chunks MUST be skipped by declared length and MUST NOT be fatal; adding an optional chunk MUST NOT raise `min_reader_version` (forward compatibility, §13).
R11. PlanGML export MUST be validated in-process against its XSD with libxml2 before any byte is written to the target path; on failure return `Result` errors with line/column. Rejection MUST NOT first be discovered on e-Plan upload (§9.9).
R12. XSD schemas and code lists MUST be loaded from `/data/catalogs` as data (canon), never compiled in, never fetched over the network at validation time.
R13. DXF MUST be first-class: full read and write, round-trip tested. DWG MUST stay read-only through LibreDWG until the R14 coverage report justifies otherwise; DWG output is produced by exporting DXF. Bidirectional DXF/DWG is a Faz 2 decision owned by that report, not by this rule (§9.8, §11 Faz 2, §15).
R14. Phase 0 MUST measure DWG coverage against a licence-cleared corpus of 50+ real files held outside the repository and referenced by URL + SHA-256 in `/data/MANIFEST.json` (`.claude/data.md` R15, R16). Only the coverage report (open success rate, per-entity-type coverage) is checked in under `/tests/golden`; a drop against the checked-in report breaks the build (§11 Faz 0, §15).
R15. Every reader entry point MUST be `Task<Result<...>>`, MUST stream (bounded working set), and MUST take a `std::stop_token` checked at least every 4 MB or 64 K entities; cancellation MUST return within 100 ms.
R16. Open 200 MB DWG MUST be ≤ 3 s and first paint of 50M-point LAZ MUST be ≤ 5 s, benchmarked in `/tests/bench`; >10% regression breaks the build (§10.1).
R17. Imports MUST run inside one `Transaction` and produce exactly one undo entry; any error MUST roll back so the `Document` is identical to its pre-import state (see `.claude/command.md`).
R18. Header-declared extents, counts, offsets and lengths MUST be treated as untrusted hints and bounds-checked against the real file size before any allocation or seek.
R19. Every parser (dxf, dwg, gml/xml, las/laz, native) MUST have a libFuzzer harness plus seed corpus in `/tests/fuzz`; a new format lands in the same PR as its harness (§9.11, §13).
R20. Every dataset MUST carry an explicit `Crs`; a missing or unrecognised CRS MUST be an error, never a silent assumption of TUREF/TM30.
R21. Every `/src/io` dependency MUST be pinned to an exact version in `vcpkg.json` with its LICENSE recorded and MUST appear in the CycloneDX SBOM produced for each release (§9.11).
R22. All optional format backends MUST sit behind `PIRICAD_WITH_*` CMake options defaulting to OFF that hard-fail with an actionable message when ON but missing (canon Phase-0 deviation 2).
R23. OGC service clients — WMS, WMTS, WFS-T, WCS, CSW and OGC API Features — MUST be implemented in `/src/io` behind `PIRICAD_WITH_OGC`, MUST expose each service as a registered command (R4), and MUST each carry a conformance-class case in `/tests/golden`; an unconformant response MUST return `Error`, never a partial layer (§12 Veri ve Kurumsal). Export-side theme/metadata obligations stay in `.claude/domain.md` R22.

## Absolute Prohibitions

P1. NEVER link, vendor, or optionally depend on **ODA Drawings SDK** — closed source, GPL-incompatible (§9.8, canon ban list).
P2. NEVER let a GDAL/OGR symbol or header (`gdal*.h`, `ogr*.h`, `cpl_*.h`) appear outside `/src/io` `.cpp` files, and NEVER in any installed header (§9.2).
P3. NEVER parse, decode, or read a file on the UI thread (§10.3).
P4. NEVER feed unvalidated external XML to the domain layer, and NEVER enable DTD loading, external entities, or XInclude in libxml2 (XXE) (§9.9, §13).
P5. NEVER write a format without a version field (§13).
P6. NEVER trust extents, feature counts, or block sizes declared in a file header.
P7. NEVER ship the full GDAL driver set — drivers come from an explicit allow-list in `/cmake`, and adding one requires an entry there in the same PR (§9.2).
P8. NEVER ship a native DWG writer while R14's coverage report stands unrevised — LibreDWG write support is not adequate today (§9.8, §15).
P9. NEVER store or round-trip geometry through `float`/`double` in the native format or in any internal transfer struct.
P10. NEVER mutate `Document` from a reader/writer outside a command transaction (see `.claude/command.md`).
P11. NEVER report success on a partially imported document, and NEVER leave orphan layers or entities after a failed import.
P12. NEVER link LASzip (use laz-perf, §9.10) or Triangle (canon ban list); NEVER link a GPLv2-only library.
P13. NEVER silently repair a malformed file — report the defect through `Result`; repair is a separate, explicit, user-invoked command.
P14. NEVER execute, eval, or resolve anything embedded in an input file (DXF/LISP hooks, XSLT, GML external references, GDAL VSI network paths).
P15. NEVER add an `/src/io` dependency without an SBOM entry and a GPLv3 compatibility check in the same PR (§9.11).
P16. NEVER use `std::toupper`/`std::tolower` on Turkish text in layer, pafta, or attribute names — use the shared Turkish folding table exposed by `piricad_command` (`.claude/command.md` R7, CLAUDE.md 5.6).

## Definitions of Done

- [ ] Wrapper header contains no third-party type; module dependency gate passes.
- [ ] Import/export exposed as a registered command with Turkish + English names.
- [ ] Written file has magic + `format_version` + `min_reader_version`; too-new file yields the explanatory error, proven by a golden test.
- [ ] Round-trip golden test added: coordinates compare bit-identical as `Mm`.
- [ ] Fuzz harness + seed corpus added or extended; ASan/UBSan fuzz job clean.
- [ ] Streaming path honours `std::stop_token`; cancel test present.
- [ ] Benchmark added/updated and within budget (§10.1).
- [ ] Failed-import rollback test present; `Document` restored exactly.
- [ ] New dependency pinned, licensed, and in the SBOM.

## Enforcement

- `scripts/ci-gate-layering.sh` — fails on `#include <Q...>` or GDAL/OGR headers outside allowed `/src/io` `.cpp` files (R3, P2).
- `/tests/fuzz`: continuous libFuzzer/AFL++ jobs for DXF, DWG, GML, LAS, native, under ASan+UBSan; any crash is a P0 and the input becomes a regression seed.
- `/tests/golden`: round-trip and version-compatibility cases, incl. the synthetic future-version file and the failed-import rollback case.
- `/tests/bench` + benchmark CI gate: 200 MB DWG ≤ 3 s, 50M-point LAZ first paint ≤ 5 s; >10% regression breaks the build.
- CI job `sbom` — CycloneDX generation per release; an unpinned or unlicensed `/src/io` dependency fails the job (R21).
- CI job `dwg-coverage` — replays the R14 external corpus and diffs against the checked-in coverage report; any regression fails the build.
- `/tests/golden` OGC conformance-class cases per service (R23).
