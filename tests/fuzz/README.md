# Fuzzing

File reading is the largest attack surface in this product (piricad.md §13), so
every parser is fuzzed continuously: DXF, DWG, GML/PlanGML, LAS/LAZ, GeoJSON, the
native project format, the command-line grammar and the journal reader.

A parser or format change ships its libFuzzer harness and seed corpus in the same
PR (CLAUDE.md 6.7).

## What exists today

| Hedef | Ayrıştırıcı | Tohum korpusu |
|---|---|---|
| `piricad_fuzz_proje` | native project format (`.pcad`) | `tohum/proje/` |
| `piricad_fuzz_dxf` | DXF import seam (GDAL/OGR + the PiriCAD conversion) | `tohum/dxf/` |
| `piricad_fuzz_shp` | Shapefile import seam | `tohum/shp/` |
| `piricad_fuzz_dwg` | the DWG reader (`io/src/dwg.cpp` over LibreDWG): the file header and section table of every format generation from release 1.4 to 2018, the object walk, the conversion to `Mm`, the layer census and the rollback. A DWG is the file an engineer receives from somebody else and its decoder is a C library for a closed format, so this is the reader a crash would hurt most | `tohum/dwg/` |
| `piricad_fuzz_ncz` | the Netcad NCZ reader (`io/ncz.hpp`): the block walk, every geometry record, smart objects, attribute tables and the mapping into the document — hand-written, needs no library | `tohum/ncz/` |
| `piricad_fuzz_komut` | the command-line grammar (`command/parser.hpp`): line, expression, predicate and single coordinate, every coordinate resolved under all six angle conventions, with and without a numbered-point lookup behind `n()` | `tohum/komut/` |

Still to land with their formats: GML/PlanGML, LAS/LAZ, GeoJSON and the journal
reader.

## Running them

The targets are Clang-only (`-fsanitize=fuzzer`) and off by default:

```bash
cmake -S . -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ -DPIRICAD_BUILD_FUZZ=ON -DPIRICAD_BUILD_APP=OFF
cmake --build build/fuzz --target piricad_fuzz_proje piricad_fuzz_dxf piricad_fuzz_komut piricad_fuzz_ncz

mkdir -p build/fuzz/fuzz-corpus/proje build/fuzz/fuzz-corpus/dxf build/fuzz/fuzz-corpus/komut
./build/fuzz/bin/piricad_fuzz_proje build/fuzz/fuzz-corpus/proje tests/fuzz/tohum/proje \
    -max_total_time=300
./build/fuzz/bin/piricad_fuzz_dxf   build/fuzz/fuzz-corpus/dxf   tests/fuzz/tohum/dxf \
    -max_total_time=300
./build/fuzz/bin/piricad_fuzz_komut build/fuzz/fuzz-corpus/komut tests/fuzz/tohum/komut \
    -max_total_time=300
```

### The DWG target needs more than the others

Three things that are not true of the rest, each learned the first time it was run:

1. **LibreDWG is C, built from source in the same tree, and it is the decoder.** The
   fuzzer only sees coverage in code compiled with `-fsanitize=fuzzer-no-link`, and
   ASan only sees a bad read in code it instrumented, so the C flags go on too — an
   `asan` or `dev` tree with `CMAKE_CXX_FLAGS` alone would fuzz the thin seam and be
   blind inside the decoder:

   ```bash
   cmake -S . -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
         -DCMAKE_C_FLAGS="-fsanitize=address,fuzzer-no-link -fno-omit-frame-pointer" \
         -DCMAKE_CXX_FLAGS="-fsanitize=address,fuzzer-no-link -fno-omit-frame-pointer" \
         -DPIRICAD_WITH_OCCT=ON -DPIRICAD_WITH_DWG=ON -DPIRICAD_BUILD_FUZZ=ON \
         -DPIRICAD_BUILD_APP=OFF
   cmake --build build/fuzz --target piricad_fuzz_dwg
   ```

2. **The pinned `fmt` 11.0.2 does not compile with Clang 20 and newer** (Ubuntu 26.04
   ships 21): its own `FMT_STRING` calls fail as "not a constant expression", and the
   macro that would switch it off is redefined unconditionally. Until the pin moves,
   point the tree at a newer checkout of the same library, for this build only:
   `-DFETCHCONTENT_SOURCE_DIR_FMT=/path/to/fmt-11.2.0` (fmt 11.2.0 and spdlog 1.15.3
   build clean with Clang 21).

3. **Leak detection off, and `-max_len` up.** LibreDWG leaves memory behind on the error
   paths of a damaged file — every leak in the first run was inside the library — and
   LeakSanitizer reports each one as a crash, which stops the run before it reaches
   the memory-safety bugs it is here for. libFuzzer's own `-detect_leaks=0` is not
   enough in `-fork` mode; the environment variable is. And libFuzzer cuts every input
   to 4096 bytes unless told otherwise, shorter than most seeds:

   ```bash
   mkdir -p build/fuzz/run && cd build/fuzz/run
   ASAN_OPTIONS=detect_leaks=0 ../bin/piricad_fuzz_dwg ../fuzz-corpus/dwg \
       ../../../tests/fuzz/tohum/dwg -fork=8 -ignore_crashes=1 -max_total_time=900 \
       -max_len=131072 -timeout=10 -artifact_prefix=./
   ```

   `-timeout=10`: the default is twenty minutes per input, which turns a decoder that
   loops on a lying section count into a silent stall.

**First run (2 October 2026):** one real bug in the first minute — a heap-use-after-free
in LibreDWG's `dwg_ent_get_layer_name`, reached from `layer_of` on a release 10 file
whose tables made the library reallocate its object array and leave every layer
reference pointing into freed memory (`tohum/dwg/90-bayat-basvuru-r10.dwg`, 32 KB
minimised to 496 bytes). Fixed in `layer_of`, which now resolves the layer through
`dwg_ref_object_silent`, the call the library built for exactly this. After it, 15
minutes on eight workers — 745 000 executions, 28 700 coverage points — found nothing
more. The bug is upstream's, and the report to send is the 496-byte file with the
ASan trace.

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
point of the grammar, `tests/unit/test_ncz.cpp` for `tohum/ncz/` through
İÇEAKTAR, and `tests/unit/test_io.cpp` for `tohum/dwg/` through İÇEAKTAR as well,
so the seeds never become dead weight. The NCZ seeds are written by
`scripts/ncz-tohum.py`; run it again after changing it and read the diff.

## What a seed is for

Each seed is a shape the reader has to survive, not a file that has to load:

The DWG seeds are LibreDWG's own test drawings (`test/test-data` in the repository the
build pins; GPL-3.0-or-later, like the library and like this repository) and damaged
copies of them. **None of them is anyone's project**: a user's DWG is their office's
data and does not belong in a public repository. `scripts/dwg-tohum.py` copies and
derives them deterministically and prints each seed's size, hash and origin; run it
again after changing it and read the diff.

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
| `proje/16-sonuc-kokenleri.pcad` | the three result blocks (TODOS F-04): two wells, a buffer round each, one well moved after — one buffer out of date, the other current |
| `proje/17-sonuc-satiri-bozuk.pcad` | the same with two faulty result rows: an object the file does not hold, an origin past the origins block — each passed over and said |
| `proje/18-sonuc-kaynagi-tasan.pcad` | the same with an origin whose sources run past the sources block — refused by name |
| `proje/19-sonuc-argumanli.pcad` | a result's origin with the arguments it was run with (TODOS F-04, stage 3): a buffer whose well moved, which can be computed again after a reopen |
| `proje/20-sonuc-argumani-tasan.pcad` | the same with the arguments' string index past the pool — refused by name |
| `proje/21-yay-bagi.pcad` | format 6: a caption tied to an arc of a round zone, its offset measured round the arc (the attachment record's flag byte, model.md R46g) — `min_reader_version` 6 |
| `proje/24-sembol-faz-birimi.pcad` | marker interval in pixels and phase in ground millimetres: the optional phase and phase-unit columns must survive reopening |
| `proje/25-sembol-faz-birimi-bozuk.pcad` | the same drawing with an invalid phase unit; the reader refuses it without changing the open document |
| `proje/13-tarih-yuvalari.pcad` | written by the build before format 3: geometry slots no row holds (a moved point's ada number, a moved-then-corrected caption, a moved line's XDATA) — the history the reader must pass over, and the file it used to refuse (io.md R10a) |
| `dxf/01-cizgi-ve-parsel.dxf` | a line and a parcel with a hole, with its `.prj` companion |
| `dxf/02-koordinat-sistemsiz.dxf` | no CRS anywhere — the io.md R20 rejection path |
| `dxf/03-kesik.dxf` | truncation mid-section |
| `dxf/04-cop.dxf` | not a DXF at all |
| `dxf/23-tarama-desen-satirlari.dxf` | three hatches with definition lines after group 78: a catalogue pattern set on its own origin, a pattern no catalogue has with a dashed line, and a record announcing more lines than it carries |
| `dxf/24-tarama-desen-bozuk.dxf` | five hatches whose definition lines lie: a count past any pattern, a base point before its line, a dash count past the bound, an angle that is not a number, a dash past its count |
| `dxf/25-cok-satirli-yazi.dxf` | a bottom-right MTEXT of three paragraphs with an underline switched on and off, spaced twice; an ALIGNED TEXT whose two points are both its ends |
| `dxf/29-milimetre-alti.dxf` | a detail drawn in millimetres (`$INSUNITS 4`) finer than the store: a 12,345 mm line, a 0,3 mm gap, a circle of 0,4 mm radius, a 2,5 mm text — what the millimetre rounds away, counted and said (TODOS F-03) |
| `dxf/30-kapali-iki-koseli-cokgen.dxf` | seven closed LWPOLYLINEs of two vertices: two half circles (a CIRCLE, either direction), the same with a constant width (an arc polyline that keeps it), one and both edges bent (a circular segment, a lens), and one with no bulge (a line). A real 1.2 MB utility drawing was refused whole for the first of them ("Yay kenar 1 yok") |
| `dxf/28-dis-referans-blogu.dxf` | a BLOCK flagged as an external reference (group 70 bit 4, path in group 1) with an INSERT of it: the reader names it and brings it as an empty block (TODOS C-14) |
| `dwg/01-r1-4-nesneler.dwg` … `06-r11-nesneler-2d.dwg` | the releases before R13 (1.4, 2.10, 2.6, 9, 10, 11), 1–5 KB each: an older layout than everything after it, decoded by its own LibreDWG code path (`decode_r11.c`). Read as far as LibreDWG reads them; a refusal must leave the document untouched |
| `dwg/07-r14.dwg` | release 14 |
| `dwg/08-2000-sade.dwg`, `09-2000-nesneler.dwg` | release 2000 (`AC1015`): the first generation with the section-locator table in the file header, which seeds 21 and 22 then damage |
| `dwg/10-2004-tarama.dwg` | release 2004: compressed pages; a hatch drawing, so the census of types this reader does not translate has something to count |
| `dwg/11-2007-nokta.dwg` | release 2007: re-encoded (Reed-Solomon) pages and UTF-16 strings, the most intricate decoder in LibreDWG (`decode_r2007.c`) |
| `dwg/12-2010-nokta.dwg`, `13-2013-coklu-cizgi.dwg`, `14-2018-sade.dwg`, `15-2018-yay.dwg` | releases 2010, 2013 and 2018: points, a polyline, an arc. 08–15 are real drawings that hold geometry this reader translates, so `test_io.cpp` requires every one of them to open |
| `dwg/16-2018-okunmayan.dwg` | a real, undamaged 2018 drawing that holds no entity this reader translates: refused ("no readable geometry"), the document untouched — the seam's other exit, next to the damaged files |
| `dwg/17-kesik-2013.dwg`, `18-kesik-2000.dwg` | truncation mid-data, in a compressed generation and an uncompressed one |
| `dwg/19-cop.dwg` | 4 KB of fixed-seed noise: not a DWG at all |
| `dwg/20-sahte-baslik.dwg` | a real version string (`AC1032`) and nothing behind it |
| `dwg/21-dizin-tasmasi-2000.dwg` | release 2000 with the section-locator COUNT overwritten by `0xFFFFFFFF` |
| `dwg/22-bolum-tasmasi-2000.dwg` | release 2000 with one locator whose seeker plus size overflows 32 bits and another that claims two gigabytes at a seeker inside the file |
| `dwg/90-bayat-basvuru-r10.dwg` | **found by `piricad_fuzz_dwg`, 2 October 2026** (test.md R10): 496 bytes, minimised from 32 KB. A release 10 file (`AC1006`) whose tables make LibreDWG reallocate its object array; every layer reference was left pointing into freed memory and `dwg_ent_get_layer_name` read it (heap-use-after-free). Numbers from 90 up are kept by hand, never derived by `scripts/dwg-tohum.py`. Only a sanitizer tells the fixed reader from the broken one — the document is refused either way — so `fuzz_dwg_smoke` is its test |
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
| `ncz/01-her-tur.ncz` | every block type the reader knows, once: version, MPROJ, TILED_XML, layer table with a blank name, LEX.ST2 colours, point, line, circle, arc, text, symbol, closed and open multiline, compressed curve, box, map sheet, triangle, block reference, two unknown types, a container |
| `ncz/02-akilli-nesne.ncz` | a SmartObject and the `S0` grid marks the reader then leaves out |
| `ncz/03-gec-tablolar.ncz` | geometry before the tables that name its layers |
| `ncz/04-bozuk-kayitlar.ncz` | records the reference drops in silence — too short, outside the world, no height, no area, one point, a compressed curve that goes bad — and bytes past the end: nothing readable, refused |
| `ncz/05-oznitelik-tablolari.ncz` | two `@TAB` attribute tables after the block chain |
| `ncz/06-kesik.ncz` | the first seed cut in the middle of a block: read as far as it goes, the cut said |
| `ncz/07-akilli-nesneler.ncz` | Netcad 8 smart objects of every class, with their property lists and a plan note's RTF |
| `ncz/08-paftalar.ncz` | a 1:1000 sheet index in TM39: four sheets stored as their bounding boxes, drawn as the turned quadrilaterals they are; one local sheet that keeps its box |
| `ncz/09-paftalar-sistemsiz.ncz` | the same sheets with no declared system: boxes, said |
| `ncz/10-cografi.ncz` | a geographic declaration over degrees: refused (io.md R20a) |
| `ncz/11-cografi-bildirim-metre.ncz` | a geographic declaration over metres: the declaration is what is wrong, the numbers are read |

## When a crash is found

test.md R10: minimise it, commit it as a regression case under `/tests/unit` or
`/tests/golden` **before** the fix merges, and add the input to the seed corpus.
A crash in a parser is a P0 (io.md Enforcement).
