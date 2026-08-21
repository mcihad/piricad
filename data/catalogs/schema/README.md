# Catalogue schemas

One JSON Schema per catalogue kind. Every catalogue under `/data/catalogs` is
validated against its schema in CI (`scripts/ci-gate-catalogs.sh`).

Required keys on every catalogue, checked by the gate:

- `schema_version`  — the schema this file conforms to
- `package_version` — bumped on every catalogue change
- `source`          — regulation name, annex, madde
- `published`       — publication date of that regulation text (ISO 8601)
- `licence`         — redistribution terms, cross-referenced in `data/LICENCES.md`

| Schema | Catalogue |
|---|---|
| `plan-gosterim.schema.json` | `catalogs/mpyy/plan-gosterim.json` — MPYY EK-1a…EK-1d gösterimleri |
| `detay-katalogu.schema.json` | `catalogs/mpyy/detay-katalogu.json` — MPYY EK-1e detay kartları |
| `asgari-standartlar.schema.json` | `catalogs/mpyy/asgari-standartlar.json` — MPYY EK-2 asgari altyapı standartları |

`plan-gosterim.schema.json` is at `schema_version` 2. Version 2 only ADDED fields —
the source-trace block on a row (`sutunlar`, `gorsel`, `sutun_metinleri`, `bolum`,
`grup`, `renk_secenekleri`, `simge_renk`, `belirsiz`) and the package-level
`gorseller` table. No version-1 field changed meaning (CLAUDE.md 0.2a), so a
version-1 catalogue still validates and still loads.

The BÖHHBÜY schema lands with its catalogue in Phase 2.
