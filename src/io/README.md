# /src/io — format I/O

Empty in Phase 0. Rules already binding: `.claude/io.md`.

Owns the GDAL/OGR wrapper, the mmap-able native project format, DWG/DXF and
GML/XML. GDAL types never leak above this module. Parsers are the largest attack
surface in the product and are fuzzed continuously (piricad.md §9.11, §13).

Phase 0 gate on this directory: `scripts/ci-gate-layering.sh` engages the moment a
file lands here.
