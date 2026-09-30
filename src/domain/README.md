# /src/domain — the Turkish regulatory core

Empty in Phase 0. Rules already binding: `.claude/domain.md`.

| Subdirectory | Scope |
|---|---|
| `geodesy/` | TUREF/ITRF96, TM 3° zones, ED50, geoid, TUSAGA-Aktif, network adjustment |
| `cadastre/` | ifraz, tevhit, yola terk, ihdas, irtifak, cins değişikliği, MEGSİS/TAKBİS |
| `planning/` | MPYY symbology, PlanGML, e-Plan, 3194/18. madde parselasyon, DOP, TAKS/KAKS |
| `surface/` | TIN/DEM, contours, volumes, profiles, alignments |

Two rules dominate here. Every regulatory constant lives in `/data/catalogs` as
data, never as code (CLAUDE.md 5.13). Every mutation goes through the command bus
— direct geometry mutation in this directory breaks the build
(`scripts/ci-gate-command-mutation.sh`, piricad.md §8).
