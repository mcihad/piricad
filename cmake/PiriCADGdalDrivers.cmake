# SPDX-License-Identifier: GPL-3.0-or-later
#
# THE GDAL DRIVER ALLOW-LIST.
#
# .claude/io.md P7: "NEVER ship the full GDAL driver set — drivers come from an
# explicit allow-list in /cmake, and adding one requires an entry there in the
# same PR."
#
# GDAL ships well over a hundred vector drivers. Most of them are formats this
# product has no business opening, several reach the network, and every one of
# them is parser attack surface that /tests/fuzz does not cover. So KentOSCad never
# asks GDAL to guess: this list is handed to `GDALOpenEx` as `papszAllowedDrivers`
# and to the file dialogs as the filter, from the same string, so the two can
# never disagree.
#
# FORMAT, one entry per line, vertical bars between entries (a semicolon would be
# a CMake list separator and would not survive the compile definition):
#
#     DRIVER:.uzanti:modlar:Türkçe etiket
#
#   DRIVER   the exact OGR driver short name (case-sensitive, as GDAL spells it)
#   .uzanti  the extension the file dialog filters on, lower case, with the dot
#   modlar   r, w, or rw
#   etiket   the Turkish label a user reads in the dialog
#
# EDITING THIS LIST DOES NOT REACH AN EXISTING BUILD TREE. It is a CMake CACHE
# variable, so `set(... CACHE ...)` leaves an already-configured tree on its old
# value and the new driver silently does not appear — the import then fails with
# `io.no_driver` and nothing points at the cache. After editing, run:
#
#     cmake -U KENTOS_GDAL_DRIVER_ALLOWLIST -S . -B build/dev
#
# ADDING A DRIVER is a reviewed change. Before adding one:
#   1. it must have a libFuzzer harness and a seed corpus in /tests/fuzz;
#   2. it must have a round-trip case in /tests/golden;
#   3. its page under /docs/veri must say what does and does not survive.
#
# WHY SHAPEFILE IS HERE. It is what Turkish institutions actually send: TKGM,
# the belediye and the il müdürlüğü hand over `.shp`, and a program that cannot
# open one is outside the workflow whatever else it can do. It is also the format
# with the fewest surprises — a documented, frozen spec with one geometry type
# per file — which is why it earns its place ahead of anything richer.
#
# It is a MULTI-FILE format: `.shp` carries the geometry, `.shx` the index, `.dbf`
# the attributes and `.prj` the coordinate system. All four travel together, and
# `/docs/veri/dis-formatlar.md` says which of them KentOSCad requires.
#
# READ ONLY, and that is the FORMAT's limit rather than ours. A shapefile holds
# exactly ONE geometry type: a drawing with parcels, boundaries, monuments and
# parcel numbers in it cannot be written to one file at all, and GDAL says so by
# refusing every feature after the first — which is a worse answer than not
# offering the button. Writing needs a decision about splitting one drawing into
# several files and naming them, and that decision belongs in a reviewed change
# of its own. DXF and GPKG both write, and both hold a whole drawing.
#
# DELIBERATELY ABSENT, with the reason:
#
#   DWG   io.md R13 keeps DWG read-only and R14 makes Phase 0 measure coverage
#         against a 50-file licence-cleared corpus first. GDAL's CAD driver is
#         libopencad, not the LibreDWG path R13 names, so enabling it here would
#         quietly pick a different implementation than the rulebook chose.
#         P1/P8: the ODA Drawings SDK is banned outright and no native DWG writer
#         ships while that coverage report stands.
#   GML   io.md R11 requires PlanGML export to be validated in-process against its
#         XSD with libxml2 BEFORE a byte reaches the target path, and R12 requires
#         the schemas to be loaded from /data/catalogs. Neither exists yet, and an
#         unvalidated PlanGML export is a file that fails on e-Plan upload.
#   Anything with a network path  io.md P14. /vsicurl, /vsis3 and friends are
#         refused in src/io/src/vector.cpp before GDAL sees the path.
set(KENTOS_GDAL_DRIVER_ALLOWLIST
    "DXF:.dxf:rw:AutoCAD DXF çizim dosyası"
    "ESRI Shapefile:.shp:r:ESRI Shapefile"
    "GPKG:.gpkg:rw:OGC GeoPackage veri tabanı"
    CACHE STRING "Allow-listed OGR drivers (io.md P7). Editing this is a reviewed change.")

# One string for the compile definition, because a C++ literal cannot be a list.
string(JOIN "|" KENTOS_GDAL_DRIVERS_STRING ${KENTOS_GDAL_DRIVER_ALLOWLIST})
