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

The `.txt` scenarios exist because only the command line exercises the parser's
metre-to-millimetre conversion and the polar form, which is the one place
`std::cos`/`std::sin` enters the pipeline. Fixed-point millimetre quantisation
absorbs the sub-ulp differences libm has between platforms, and that absorption
is exactly what these fixtures verify.

## Running

`piricad_tests` runs every scenario and diffs against `beklenen/`. To regenerate
after a reviewed, explained change:

```bash
PIRICAD_GOLDEN_UPDATE=1 ./build/dev/bin/piricad_tests
```

A golden file is never deleted to make a test pass, and never regenerated without
reading the diff (`.claude/test.md`).
