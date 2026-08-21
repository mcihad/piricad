# Catalogue schemas

One JSON Schema per catalogue kind. Every catalogue under `/data/catalogs` is
validated against its schema in CI (`scripts/ci-gate-catalogs.sh`).

Required keys on every catalogue, checked by the gate:

- `schema_version`  — the schema this file conforms to
- `package_version` — bumped on every catalogue change
- `source`          — regulation name, annex, madde
- `published`       — publication date of that regulation text (ISO 8601)
- `licence`         — redistribution terms, cross-referenced in `data/LICENCES.md`

Schemas land with their catalogues in Phase 2 (BÖHHBÜY) and Phase 3 (MPYY).
