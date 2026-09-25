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
| `kentos_fuzz_shp` | Shapefile import seam | `tohum/shp/` |
| `kentos_fuzz_komut` | the command-line grammar (`command/parser.hpp`): line, expression, predicate and single coordinate, every coordinate resolved under all six angle conventions, with and without a numbered-point lookup behind `n()` | `tohum/komut/` |

Still to land with their formats: DWG, GML/PlanGML, LAS/LAZ, GeoJSON and the
journal reader.

## Running them

The targets are Clang-only (`-fsanitize=fuzzer`) and off by default:

```bash
cmake -S . -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ -DKENTOS_BUILD_FUZZ=ON -DKENTOS_BUILD_APP=OFF
cmake --build build/fuzz --target kentos_fuzz_proje kentos_fuzz_dxf kentos_fuzz_komut

mkdir -p build/fuzz/fuzz-corpus/proje build/fuzz/fuzz-corpus/dxf build/fuzz/fuzz-corpus/komut
./build/fuzz/bin/kentos_fuzz_proje build/fuzz/fuzz-corpus/proje tests/fuzz/tohum/proje \
    -max_total_time=300
./build/fuzz/bin/kentos_fuzz_dxf   build/fuzz/fuzz-corpus/dxf   tests/fuzz/tohum/dxf \
    -max_total_time=300
./build/fuzz/bin/kentos_fuzz_komut build/fuzz/fuzz-corpus/komut tests/fuzz/tohum/komut \
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
every seed in `tohum/proje/` through the same reader on every ordinary build, and
`tests/unit/test_command.cpp` does the same for `tohum/komut/` through every entry
point of the grammar, so the seeds never become dead weight.

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
| `proje/09-olcu-baglari.pcad` | the dimension link block: one dimension tied to a line, one whose line was erased (broken links) |
| `proje/10-olcu-bagi-bozuk.pcad` | the same block with one fault per row: a dimension the file does not hold, an anchor past the last, a definition point the dimension lacks, a live link to no object |
| `proje/11-tarama-baglari.pcad` | the hatch link block: one hatch tied to a parcel, one whose parcel was erased (a broken source) |
| `proje/12-tarama-bagi-bozuk.pcad` | the same block with one fault per row: a hatch the file does not hold, a live link to no object |
| `proje/14-kokenler.pcad` | the lineage block: a trimmed line's new piece and a copied parcel whose source was erased — a dead source, which a lineage keeps |
| `proje/15-koken-bozuk.pcad` | the same block with one fault per row: an object the file does not hold, an operation string past the end of the pool |
| `proje/13-tarih-yuvalari.pcad` | written by the build before format 3: geometry slots no row holds (a moved point's ada number, a moved-then-corrected caption, a moved line's XDATA) — the history the reader must pass over, and the file it used to refuse (io.md R10a) |
| `dxf/01-cizgi-ve-parsel.dxf` | a line and a parcel with a hole, with its `.prj` companion |
| `dxf/02-koordinat-sistemsiz.dxf` | no CRS anywhere — the io.md R20 rejection path |
| `dxf/03-kesik.dxf` | truncation mid-section |
| `dxf/04-cop.dxf` | not a DXF at all |
| `dxf/23-tarama-desen-satirlari.dxf` | three hatches with definition lines after group 78: a catalogue pattern set on its own origin, a pattern no catalogue has with a dashed line, and a record announcing more lines than it carries |
| `dxf/24-tarama-desen-bozuk.dxf` | five hatches whose definition lines lie: a count past any pattern, a base point before its line, a dash count past the bound, an angle that is not a number, a dash past its count |
| `dxf/25-cok-satirli-yazi.dxf` | a bottom-right MTEXT of three paragraphs with an underline switched on and off, spaced twice; an ALIGNED TEXT whose two points are both its ends |
| `dxf/29-milimetre-alti.dxf` | a detail drawn in millimetres (`$INSUNITS 4`) finer than the store: a 12,345 mm line, a 0,3 mm gap, a circle of 0,4 mm radius, a 2,5 mm text — what the millimetre rounds away, counted and said (TODOS F-03) |
| `dxf/28-dis-referans-blogu.dxf` | a BLOCK flagged as an external reference (group 70 bit 4, path in group 1) with an INSERT of it: the reader names it and brings it as an empty block (TODOS C-14) |
| `komut/01-mutlak.txt` | two absolute metre coordinates |
| `komut/02-goreli.txt` | relative coordinates, negative and fractional |
| `komut/03-kutupsal-soneksiz.txt` | bare polar angles on all four axes, the diagonal, negative and past a full turn |
| `komut/04-kutupsal-sonekli.txt` | the `g` / `d` / `r` unit suffixes, lower and upper case |
| `komut/05-ifade.txt` | inline expressions inside every coordinate slot, a suffix after a parenthesis |
| `komut/06-anahtar-deger.txt` | `key=value` with a quoted value holding a space |
| `komut/07-tirnak-kacis.txt` | escape sequences and a quoted value holding `=` |
| `komut/08-bozuk.txt` | every malformed `@` form, a bad suffix letter, a doubled suffix, an unclosed quote |
| `komut/09-yuklem-satiri.txt` | a filter predicate carried as a `key=` argument |
| `komut/10-yuklem.txt` | a bare predicate: AND / OR / NOT / IS NULL / `<>` / arithmetic |
| `komut/11-derin-parantez.txt` | deep nesting in expressions and coordinates |
| `komut/12-unicode.txt` | Turkish letters in a command name and a quoted value |
| `komut/13-bos-ve-yalniz-ad.txt` | an empty line, a blank line, a bare command name |
| `komut/14-asiri-sayi.txt` | numbers at the edge of a double, an overflowing power |
| `komut/15-nokta-fonksiyonu.txt` | every point function and every shape of `kes`, nested calls, a suffixed angle inside a call, `son`, `yon=` |
| `komut/16-nokta-fonksiyonu-bozuk.txt` | parallel directions, circles that do not meet, a missing `n()`, an argument list that fits no shape, unbalanced brackets |
| `komut/17-nokta-derin.txt` | point functions nested past `detail::kMaxCallDepth` |

## When a crash is found

test.md R10: minimise it, commit it as a regression case under `/tests/unit` or
`/tests/golden` **before** the fix merges, and add the input to the seed corpus.
A crash in a parser is a P0 (io.md Enforcement).
