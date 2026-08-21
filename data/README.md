# /data — regulation as data, never as code

CLAUDE.md 5.13 and `.claude/data.md`: a regulatory value is never hard-coded in
C++. Turkish surveying and planning legislation changes often; a change here must
be a **data release**, not a rebuild (piricad.md §15).

| Directory | Contents |
|---|---|
| `catalogs/` | BÖHHBÜY detail codes and object catalogue; MPYY EK-1a…1ç/1d symbology and the EK-1e detail catalogue |
| `crs/` | Geoid grids and transformation parameters, with recorded provenance and licence |
| `corpus/` | Legislation texts and the embedding index for the mevzuat RAG |

Every file carries `schema_version`, `package_version`, `source` (regulation +
annex + madde), `published` and `licence`. Enforced by
`scripts/ci-gate-catalogs.sh` and `scripts/ci-gate-corpus.sh`.

Redistribution of geographic data is governed by the Coğrafi Veri İzin Belgesi
process (piricad.md §12). Every dataset's permit status is recorded in
`LICENCES.md` and checked by `scripts/ci-gate-data-permits.sh`.
