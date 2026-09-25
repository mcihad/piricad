# I/O Engine — Rules

> Scope: `/src/io` (GDAL/OGR wrapper, native mmap-able format, DXF/DWG, GML/XML, LAS/LAZ, OGC services), `/tests/fuzz`, `/tests/golden` format cases  |  Depends on: `kentos_core`, `kentos_command` only (canon dependency table)  |  Source: kentoscad.md §9.2, §9.3, §9.8, §9.9, §9.10, §9.11, §10.1–§10.3, §10.5, §13, §15

## Hard Rules

R1. Every format access MUST go through a thin wrapper in `/src/io`. Public headers under `kentos_cad/io/` MUST expose only core/command types (`Document`, `Layer`, `EntityId`, `Mm`, `Point2`, `Box2`, `Crs`, `Result<T>`, `Task<T>`) and MUST return `Result<T>` on failure.
R2. GDAL/OGR/LibreDWG/libdxfrw/libxml2/PDAL headers MUST appear only in `.cpp` files under `/src/io`. Wrapper classes MUST hide them behind pimpl or forward declarations (§9.2).
R3. `/src/io` MUST link `kentos_core` + `kentos_command` + format libraries only. No `#include <Q...>` anywhere in `/src/io`.
R4. Every import and export MUST be a command registered in `Registry` via `KENTOS_COMMAND`/`CommandSpec`, with Turkish primary + English names; readers MUST NOT mutate `Document` directly (see `.claude/command.md`).
R5. The native project format MUST be a single file that is columnar (SoA blocks mirroring the `Document` polyline store), 8-byte aligned, offset-addressed with `u64`, and usable by `mmap` with zero parsing of geometry blocks (§9.3, §10.2).
R6. The native format MUST embed 4–5 precomputed Douglas–Peucker LOD levels written into quadtree tiles (§10.3) and a bulk-loaded STR R-tree (§10.5). Rebuilding either on open is a bug.
R7. All coordinates in the native format MUST be stored as `Mm` (int64 fixed-point millimetres); the file MUST NOT contain floating-point geometry (canon).
R8. Every format KentOSCad writes MUST begin with a magic string, a `u32 format_version`, and a `u32 min_reader_version` inside the first 32 bytes (§13).
R9. A reader whose version is below `min_reader_version` MUST return `Error{code="io.format_too_new", message=...}` naming the required application version — never crash, never a partial load (§13).
R10. Unknown chunks MUST be skipped by declared length and MUST NOT be fatal; adding an optional chunk MUST NOT raise `min_reader_version` (forward compatibility, §13).
R10a. In the native format the slot-indexed blocks — geometry, captions, attribute cells, foreign data, kind payload — MUST be laid out ONE SLOT PER WRITTEN ROW: file slot r is row r as it stands. The document keeps a slot per geometry version (an edit appends one and leaves the old for undo), and that history MUST NOT reach the file: a reader takes slot r for row r, and a writer that dumped history gave a moved parcel's cells to the next object drawn (format 3, `io::kFormatVersionRowSlots`). A document with one slot per row in row order is streamed straight from its columns, so its bytes do not change. A reader MUST still map a slot to the row that holds it rather than assume the numbers agree, and MUST pass over the leftover slots of a file older than format 3 as history — never as a fault, never onto another object. `Document::content_hash` folds the slot tables over the same rows (`Document::row_slots`), so a fingerprint does not depend on history either. The list of blocks a reader KNOWS is every block it reads; a block read in full and reported as "not preserved" is a false alarm about the user's data.
R10b. The native writer MUST NOT write the members of an external reference or of its dependents (model.md R45a): their source is the reference's own file, read again on every open. The block record keeps the name, the flags and the path — relative to the directory the project file is written into when both share a root, with forward slashes — and a file that holds one writes `min_reader_version` = `kMinReaderVersionExternal`. The reader steps over the entity keys the file left out ONLY in a file that declares such a block, and stops the key counter where the file's document record says, so no key a member once had is handed to another object. Loading the references (`load_externals`) happens inside the read transaction, before the document is shown; one whose file cannot be read is a warning, and a file found beside the project rather than where recorded is loaded, said, and its new place written on the next save.
R10c. A file holding a CLIPPED block reference (model.md R45b) — a layout-2 payload in any row the file writes, live or not — MUST write `min_reader_version` = `kMinReaderVersionClip` (format 5); the requirement is per drawing and the highest of what the drawing holds wins, so taking every clip off and saving gives the file back to older readers. The DXF writer has no way to carry a clip through libdxfrw (AutoCAD keeps it in a `SPATIAL_FILTER` object under the INSERT's extension dictionary) and MUST say how many references went out unclipped (`Severity::Degraded`); writing a filter object the program cannot verify against AutoCAD is not the answer, because a malformed OBJECTS section can make AutoCAD refuse the whole file.
R11. PlanGML export MUST be validated in-process against its XSD with libxml2 before any byte is written to the target path; on failure return `Result` errors with line/column. Rejection MUST NOT first be discovered on e-Plan upload (§9.9).
R12. XSD schemas and code lists MUST be loaded from `/data/catalogs` as data (canon), never compiled in, never fetched over the network at validation time.
R13. DXF MUST be first-class: full read and write, round-trip tested. The DXF road is **libdxfrw** (`io/dxf.hpp`, `KENTOS_WITH_DXFRW`): the file is read at group-code level, every entity as what it is; GDAL's DXF driver is the fallback of a build without it and reads less, saying so. DWG MUST stay read-only through LibreDWG until the R14 coverage report justifies otherwise; DWG output is produced by exporting DXF. Bidirectional DXF/DWG is a Faz 2 decision owned by that report, not by this rule (§9.8, §11 Faz 2, §15).
R14. Phase 0 MUST measure DWG coverage against a licence-cleared corpus of 50+ real files held outside the repository and referenced by URL + SHA-256 in `/data/MANIFEST.json` (`.claude/data.md` R15, R16). Only the coverage report (open success rate, per-entity-type coverage) is checked in under `/tests/golden`; a drop against the checked-in report breaks the build (§11 Faz 0, §15).
R15. Every reader entry point MUST be `Task<Result<...>>`, MUST stream (bounded working set), and MUST take a `std::stop_token` checked at least every 4 MB or 64 K entities; cancellation MUST return within 100 ms. An import is TWO PHASES (P3): `read_into_scratch` fills a scratch `Document` as a `Job` (command.md R8a) on whatever thread hosts it, and `Transaction::adopt_from` copies the scratch into the real document on the bus thread inside the command's one transaction (R17). A callback-driven library (libdxfrw, later) honours the stop by refusing to write past it; the rest of its parse is discarded with the scratch.
R16. Open 200 MB DWG MUST be ≤ 3 s and first paint of 50M-point LAZ MUST be ≤ 5 s, benchmarked in `/tests/bench`; >10% regression breaks the build (§10.1).
R17. Imports MUST run inside one `Transaction` and produce exactly one undo entry; any error MUST roll back so the `Document` is identical to its pre-import state (see `.claude/command.md`).
R18. Header-declared extents, counts, offsets and lengths MUST be treated as untrusted hints and bounds-checked against the real file size before any allocation or seek.
R19. Every parser (dxf, dwg, gml/xml, las/laz, native) MUST have a libFuzzer harness plus seed corpus in `/tests/fuzz`; a new format lands in the same PR as its harness (§9.11, §13).
R20. Every dataset MUST carry an explicit `Crs`; a missing or unrecognised CRS MUST be an error, never a silent assumption of TUREF/TM30.
R20a. **A dataset's system MUST count metres.** The store holds millimetres (model.md R36a), so a layer whose system counts degrees (geographic), another linear unit (feet) or a geocentric X/Y/Z MUST be refused on import — asked of GDAL (`crs_with_unit`, `OGRSpatialReference::IsGeographic`/`GetLinearUnits`), never inferred from the digits — with the way to convert it (`file_crs_holds_metres`). A `.prj` beside a DXF is held to the same rule. A layer with NO system is read in the drawing's with a warning (the sanctioned DXF case of R20); when every coordinate of such a GIS layer lies inside ±180 × ±90 the warning says they are probably degrees, and a read that kept nothing says so in its refusal. On export, a `.prj` is written beside a DXF ONLY when the DXF is written in metres: a sidecar names a metre-counting system and a GIS program trusting it would read millimetre numbers a thousand times too far out (`dxf_prj_withheld`). A world file is written in the system's metres, never in the store's millimetres (TODOS F-03).
R20b. **A curve goes to a file that cannot hold one as its SHAPE, within a stated error.** GeoPackage (and the GDAL DXF fallback) and PostGIS write every circle, arc, ellipse, arc polyline and spline through `core::stroke_curve` at the project's `core.aktarim.egri_sapmasi` — never the definition points a kind stores (a circle's centre and radius handle), never the picture's fixed density, never nothing (a spline with fit points) — and the export report says how many curves were stroked and the worst chord deviation left (TODOS F-03). A format that holds curves (libdxfrw DXF) writes them as curves.
R21. Every `/src/io` dependency MUST be pinned to an exact version in `vcpkg.json` with its LICENSE recorded and MUST appear in the CycloneDX SBOM produced for each release (§9.11).
R22. All optional format backends MUST sit behind `KENTOS_WITH_*` CMake options defaulting to OFF that hard-fail with an actionable message when ON but missing (canon Phase-0 deviation 2).
R23. OGC service clients — WMS, WMTS, WFS-T, WCS, CSW and OGC API Features — MUST be implemented in `/src/io` behind `KENTOS_WITH_OGC`, MUST expose each service as a registered command (R4), and MUST each carry a conformance-class case in `/tests/golden`; an unconformant response MUST return `Error`, never a partial layer (§12 Veri ve Kurumsal). Export-side theme/metadata obligations stay in `.claude/domain.md` R22.
R24. Print profiles — a named sheet: paper, orientation, resolution, margin — live in `io::PrintProfiles` (`io/print_profiles.hpp`) as JSON in the user's configuration directory, NEVER in the document (model.md R39: they are application state, like a printer preference). The store owns the ISO 216 paper table, the floor every profile has to clear (a known paper or an explicit custom size, 72–4800 dpi, a margin that leaves printable area) and the `resolve` that lays a request's overrides on a profile. Exactly one profile is the default. A missing file is the built-in set; a file that does not parse is REPORTED and the user's file is left alone (R42's spirit: never silently start over).
R25. A PDF is written by the application (Qt has the page geometry and the painter) and encrypted by `io::pdf_encrypt` (qpdf, `KENTOS_WITH_QPDF`): AES-256 (R6 revision), the two passwords and the three permission flags, plus the `/Author` field in the same pass. Encryption is never hand-rolled (CLAUDE.md 5.16) and the qpdf headers stay inside `src/pdf_encrypt.cpp` (R2/P2). A build without it says so and writes an unencrypted file only when no password was asked for.

R26. A record store the program writes outside `/src/io` still obeys R24's shape and P5's version field: `ai-modelleri.json` and the audit log each carry `sürüm`, live in the user's configuration directory, are never part of the document, and are never journalled. A configuration file with no version is a file a later build cannot read safely.

## Absolute Prohibitions

P1. NEVER link, vendor, or optionally depend on **ODA Drawings SDK** — closed source, GPL-incompatible (§9.8, canon ban list).
P2. NEVER let a GDAL/OGR symbol or header (`gdal*.h`, `ogr*.h`, `cpl_*.h`) appear outside `/src/io` `.cpp` files, and NEVER in any installed header (§9.2).
P3. NEVER parse, decode, or read a file on the UI thread (§10.3).
P4. NEVER feed unvalidated external XML to the domain layer, and NEVER enable DTD loading, external entities, or XInclude in libxml2 (XXE) (§9.9, §13).
P5. NEVER write a format without a version field (§13).
P5a. NEVER put a PDF password, a user password or an owner password into a journal line, a log line, a transcript line or an error message. `core.print` is read-only and therefore never journalled; that is the mechanism, and it must stay the mechanism.
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
P16. NEVER use `std::toupper`/`std::tolower` on Turkish text in layer, pafta, or attribute names — use the shared Turkish folding table exposed by `kentos_command` (`.claude/command.md` R7, CLAUDE.md 5.6).

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
