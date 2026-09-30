# Data & Catalogues — Rules

> Scope: `/data/catalogs`, `/data/crs`, `/data/corpus` | Depends on: nothing (pure data; consumed by `/src/domain`, `/src/ai`, `/src/io`) | Source: piricad.md §5.5, §12, §15, §16

## Hard Rules

R1. Every regulatory catalogue SHALL live under `/data/catalogs` as a versioned, runtime-loadable data package: BÖHHBÜY detay kodları + nesne kataloğu, MPYY EK-1a/1b/1c/1ç/1d gösterimleri, MPYY EK-1e Detay Kataloğu. A legislation change MUST be a data release, never a rebuild (§15 "veri odaklı katalog mimarisi", §12).
R2. Every catalogue file MUST carry a header block with `schema_version`, `package_version` (semver), `source` (regulation name + annex + madde), `published` (ISO-8601 date) and `licence`. A missing field is a CI failure.
R3. Every catalogue MUST have a JSON Schema under `/data/catalogs/schema/` and MUST validate against it in CI.
R4. Any content change to a catalogue MUST bump `package_version` and add a `CHANGELOG.md` line naming the regulation/genelge that caused it, in the same commit.
R5. Catalogue entry ids (detay kodu, gösterim kodu) MUST be stable forever; a retired entry MUST be marked `deprecated: true` with `valid_until`, never removed and never reused.
R6. Catalogue lookups MUST fail loudly: an unknown code returns `Error` via `Result<T>` (`piricad/core/result.hpp`); no compiled-in fallback value. See `.claude/domain.md` for consumption rules.
R7. `/data/crs` MUST hold geoid grids and transformation parameters as files, each with a sibling `provenance.json` recording origin institution, acquisition date, licence, and whether redistribution is permitted (§12).
R8. `/data/crs` MUST cover TUREF/ITRF96 with epoch, TM 3° zones 27/30/33/36/39/42/45 at scale 1.0, ED50/UTM 6°, ITRF↔ED50 regional transformation, and the current Türkiye Jeoit Modeli (§12 Jeodezik).
R9. Each CRS definition and grid MUST be covered by a reference test that transforms TKGM-supplied reference coordinates through PROJ and asserts agreement within a tolerance stated in the test file (§16.7).
R10. `/data/corpus` MUST contain BÖHHBÜY, MPYY with its annexes, Planlı Alanlar İmar Yönetmeliği, 3194 sayılı Kanun, TUCBS tanımlama dokümanları, TKGM genelgeleri, plus the embedding index (§5.5).
R11. Every corpus chunk MUST carry `document_id`, `madde`, `published` (ISO-8601), `chunk_id` and `source_ref`, so every RAG answer can cite madde number and publication date (§5.5, §12 "AI'ya Özgü"). Citation rendering is `.claude/ai.md`.
R12. Corpus text MUST be verbatim official text; edits are limited to whitespace and pagination artefacts, recorded in the document manifest.
R13. Superseded text MUST be retained with `valid_from` / `valid_until` (answers about an older plan need the version then in force).
R14. The embedding index MUST record `embedding_model`, `dimension`, `built_at` and the corpus commit hash it was built from; changing any of the first two MUST trigger a full rebuild.
R15. Any file over 10 MB MUST be tracked with Git LFS or held in an external artefact store referenced by URL + SHA-256 in `/data/MANIFEST.json`; the working-tree size cap MUST be declared as one named value in `/data/MANIFEST.json` and enforced by `scripts/ci-gate-repo-size.sh`.
R16. Any dataset that will be shipped or redistributed MUST have a Coğrafi Veri İzin Belgesi record (permit id, covered data, scope of redistribution, expiry) in `/data/LICENCES.md`, with legal sign-off dated before the release commit (§12 "Veri ve Kurumsal").
R17. All text data MUST be UTF-8 without BOM, LF line endings; Turkish case folding is the consumer's job — the shared folding table in `/src/command` (`.claude/command.md` R7) or `QLocale` at the Qt layers (`.claude/ui.md` R25), never `std::toupper` (CLAUDE.md 5.6).

## Absolute Prohibitions

P1. NEVER hard-code a regulatory value in C++ (CLAUDE.md 5.13) — for `/data`, this means no catalogue row may be mirrored as a literal or table anywhere under `/src/**`; enforced by the grep gate below.
P2. NEVER ship a symbol, code list or catalogue row without `source` and `published`.
P3. NEVER ship a corpus chunk without `madde` and `published` metadata — an uncitable answer is worthless and dangerous here (§5.5).
P4. NEVER generate, paraphrase, summarise or translate corpus text with a model and store it as corpus. BANNED.
P5. NEVER redistribute, bundle or publish geographic data without a matching İzin Belgesi entry in `/data/LICENCES.md`.
P6. NEVER update a catalogue silently: content change without a `package_version` bump is BANNED.
P7. NEVER delete or overwrite superseded regulation text, and NEVER delete a retired catalogue id.
P8. NEVER commit a binary over 10 MB directly to git (no LFS pointer, no manifest entry). BANNED.
P9. NEVER place executable code — C++, shell, Python, Lua — under `/data`. These directories hold data, schemas and manifests only; loaders live in `/src`.
P10. NEVER let a build target embed a catalogue or grid file into the binary at compile time (no `qrc`, no generated header); they are loaded from disk at runtime.

## Definitions of Done

- [ ] New/changed catalogue validates against its schema; `schema_version`, `package_version`, `source`, `published`, `licence` present.
- [ ] `CHANGELOG.md` entry names the regulation, annex/madde and date behind the change.
- [ ] New corpus documents chunked with `madde` + `published` on every chunk; embedding index rebuilt and its manifest updated.
- [ ] New CRS grid or definition has `provenance.json` and a passing TKGM reference-coordinate test.
- [ ] Redistribution-bound data has an `/data/LICENCES.md` permit row with legal sign-off date.
- [ ] Files > 10 MB are in LFS or the external store with a SHA-256 manifest entry; repo size gate passes.

## Enforcement

- `scripts/ci-gate-catalogs.sh` — CI job `data-catalogs`: JSON Schema validation, R2 header completeness, rejection of a missing `published` or a past `valid_until`, id stability vs. previous tag, version-bump-on-change. This is the only catalogue validator; `.claude/domain.md` cites the same name.
- `scripts/ci-gate-corpus.sh` — CI job `data-corpus`: per-chunk `madde`/`published`/`source_ref` completeness, embedding-index manifest freshness against the corpus commit hash.
- `/tests/unit/crs_reference_test.cpp` in `piricad_tests` — PROJ-vs-TKGM reference coordinates. Gated behind `PIRICAD_WITH_PROJ`; while PROJ is absent (Phase-0 deviation) it MUST report as pending, never as passing.
- `scripts/ci-gate-repo-size.sh` — CI job `repo-size`: fails on any non-LFS blob over 10 MB or on tree size past the cap declared in `/data/MANIFEST.json` (R15).
- `scripts/ci-gate-data-permits.sh` — CI job `data-permits`: every directory under `/data` reachable by packaging maps to an `/data/LICENCES.md` row with an unexpired permit and a sign-off date.
- `scripts/ci-gate-layering.sh` — catalogue literals and `#include` of any `/data` path inside `/src/**` break the build (P1, P10).
