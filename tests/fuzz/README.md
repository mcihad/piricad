# Fuzzing

File reading is the largest attack surface in this product (kentoscad.md §13), so
every parser is fuzzed continuously: DXF, DWG, GML/PlanGML, LAS/LAZ, GeoJSON, the
native project format, the command-line grammar and the journal reader.

A parser or format change ships its libFuzzer harness and seed corpus in the same
PR (CLAUDE.md 6.7).

## What exists today

| Hedef | Ayrıştırıcı | Tohum korpusu |
|---|---|---|
| `kentos_fuzz_proje` | native project format (`.pcad`) | `tohum/proje/` |
| `kentos_fuzz_dxf` | DXF import seam (GDAL/OGR + the KentOSCad conversion) | `tohum/dxf/` |

Still to land with their formats: DWG, GML/PlanGML, LAS/LAZ, GeoJSON, the command
line grammar and the journal reader.

## Running them

The targets are Clang-only (`-fsanitize=fuzzer`) and off by default:

```bash
cmake -S . -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ -DKENTOS_BUILD_FUZZ=ON -DKENTOS_BUILD_APP=OFF
cmake --build build/fuzz --target kentos_fuzz_proje kentos_fuzz_dxf

mkdir -p build/fuzz/fuzz-corpus/proje build/fuzz/fuzz-corpus/dxf
./build/fuzz/bin/kentos_fuzz_proje build/fuzz/fuzz-corpus/proje tests/fuzz/tohum/proje \
    -max_total_time=300
./build/fuzz/bin/kentos_fuzz_dxf   build/fuzz/fuzz-corpus/dxf   tests/fuzz/tohum/dxf \
    -max_total_time=300
```

**Give the scratch directory first.** libFuzzer saves every new unit it finds into
the FIRST directory on the command line. Passing `tohum/` first sprays a few
hundred unnamed binaries into the source tree; passing a build-tree directory
first keeps the checked-in corpus exactly the named files below.

Both are built with ASan and UBSan, because fuzzing without them finds only the
crashes that happen to be fatal. `ctest` runs a short deterministic smoke run of
each so a broken harness fails the build rather than the nightly job.

**The corpora are exercised even without Clang.** `tests/unit/test_io.cpp` replays
every seed in `tohum/proje/` through the same reader on every ordinary build, so
the seeds never become dead weight.

## What a seed is for

Each seed is a shape the reader has to survive, not a file that has to load:

| Tohum | Ne sınar |
|---|---|
| `proje/01-bos.pcad` | the smallest valid file: header, document record, directory |
| `proje/02-parsel.pcad` | layers, styles, rings, an erased entity, project settings |
| `proje/03-kesik.pcad` | truncation mid-payload |
| `proje/04-gelecek-surum.pcad` | `min_reader_version` beyond this build (io.md R9) |
| `proje/05-dizin-tasmasi.pcad` | a directory offset that overflows the file |
| `proje/06-sahte-sihirli-sayi.pcad` | correct magic, nothing behind it |
| `proje/07-yuva-disinda.pcad` | entities pointing at geometry slots that do not exist |
| `proje/08-cakisan-halkalar.pcad` | overlapping ring ranges whose per-entity sum overflows `size_t` |
| `dxf/01-cizgi-ve-parsel.dxf` | a line and a parcel with a hole, with its `.prj` companion |
| `dxf/02-koordinat-sistemsiz.dxf` | no CRS anywhere — the io.md R20 rejection path |
| `dxf/03-kesik.dxf` | truncation mid-section |
| `dxf/04-cop.dxf` | not a DXF at all |

## When a crash is found

test.md R10: minimise it, commit it as a regression case under `/tests/unit` or
`/tests/golden` **before** the fix merges, and add the input to the seed corpus.
A crash in a parser is a P0 (io.md Enforcement).
