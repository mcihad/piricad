# Fuzzing

File reading is the largest attack surface in this product (piricad.md §13), so
every parser is fuzzed continuously: DXF, DWG, GML/PlanGML, LAS/LAZ, GeoJSON, the
native project format, the command-line grammar and the journal reader.

A parser or format change ships its libFuzzer harness and seed corpus in the same
PR (CLAUDE.md 6.7).

Empty in Phase 0.
