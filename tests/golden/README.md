# Golden data

Reference outputs that must match **bit for bit** on Linux, Windows and macOS in
the same CI run (piricad.md §7.3, §10.5). This is why `-ffp-contract=off` is
mandatory and `-ffast-math` is banned: a survey document's area, intersection and
adjustment results are legal figures and may not depend on the machine that
produced them.

## Layout

| Path | Contents |
|---|---|
| `senaryolar/*.json` | A command script, replayed through `script::JsonRunner` |
| `senaryolar/*.txt` | One command line per run, replayed through `Bus::execute_line` |
| `beklenen/<ad>.txt` | The expected deterministic dump of the resulting document |
| `mpyy/beklenen.txt` | The expected summary of the shipped MPYY catalogues — row counts per annex, reference colours, the `belirsiz` roster and the SHA-256 of each generated file |

`mpyy/beklenen.txt` is checked by `scripts/ci-gate-mpyy.sh`, not by
`piricad_tests`: a regulation catalogue is data, and it is diffed as data. The gate
also re-runs `scripts/mpyy-cikar.py` and compares byte for byte when the source
annexes are present (`PIRICAD_MPYY_KAYNAK`), which is the determinism proof for the
extraction itself. Regenerate it, after reading the diff, with
`PIRICAD_GOLDEN_UPDATE=1 bash scripts/ci-gate-mpyy.sh`.

The `.txt` scenarios exist because only the command line exercises the parser's
metre-to-millimetre conversion and the polar form. The polar form used to be the
one place `std::cos`/`std::sin` entered the pipeline; it now resolves through
`core::polar_offset`, whose trigonometry is the deterministic `sin_cos_udeg` of
`core/trig.hpp`, under the session's angle convention (`core.aci.kural`,
`core.aci.birim` — semt + grad by default). `koordinat-bicimleri.txt` locks the
four axes exact in both rules and all three units, with and without the `g/d/r`
suffix, so a platform that disagreed in the last bit would show up here as a
moved vertex rather than as a surveyor's complaint.

`alan-boolean.txt` fixes union (175 m²), intersection (25 m²), difference
with a true hole (64 m²), symmetric difference (two 75 m² pieces) and a
circle's half-disc intersection (39,269,908 mm²). The whole-millimetre paths,
arc payloads, provenance and replayed journal are checked on every platform.
The multipart unit fixture is imported through the public command from
`cizim/alan-boolean-multipart.gpkg`; its two exteriors and first part's hole
come from the adjacent GeoJSON source. Recreate the fixture with:

```bash
ogr2ogr -f GPKG /tmp/alan-boolean-multipart.gpkg tests/golden/cizim/alan-boolean-multipart.geojson
```

`occt-kesisim.txt` fixes the O-5 OpenCASCADE results at large TUREF coordinates:
an ellipse meets a line at the hand-computed ±8 m, and two opposite quadratic
splines meet at 1.464 m and 8.536 m after rounding once to millimetres. BUDA keeps
the exact ellipse and spline definitions; their payloads, vertices and journal
are compared byte for byte by the same harness on each platform.

`occt-yuvarlama.txt` fixes YUVARLA between two objects when one is an ellipse
or a spline, through OCCT's 2D fillet (`ChFi2d`): a half ellipse and the line
y = 3 m that crosses it at the hand-computed (8, 3) m, rounded at 1 m — the
centre 1 m under the line, the ellipse cut at its touch and still an ellipse;
a parabola and a line that stops short, rounded at 0.5 m with the line carried
on to its touch; and the same ellipse and line met at radius zero exactly at
(8, 3) m. The tangent points were checked by hand against the curves' equations.

`nokta-fonksiyonlari.txt` does the same for the point functions (TODOS-CAD
P1a), and it is the harder case: `dik`, `kes`, `ara` and `uzanti` divide by a
square root, so every vertex in it is an answer that a differently-rounded
`sqrt` or a fused multiply-add would move. The triangles are chosen so the
answer is a whole millimetre — a 3-4-5 with legs of 30 and 40 metres, and the
axes — which is what makes a one-millimetre drift visible as a wrong integer
rather than as a last-bit difference nobody reads.

## Running

`piricad_tests` runs every scenario and diffs against `beklenen/`. To regenerate
after a reviewed, explained change:

```bash
PIRICAD_GOLDEN_UPDATE=1 ./build/dev/bin/piricad_tests
```

A golden file is never deleted to make a test pass, and never regenerated without
reading the diff (`.claude/test.md`).
