#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Writes the DWG seed corpus under tests/fuzz/tohum/dwg/ — the drawings the DWG
# reader (src/io/src/dwg.cpp over LibreDWG) is fuzzed from and the unit tests
# replay (.claude/io.md R19, test.md R9).
#
# WHERE THE BYTES COME FROM. Not from anybody's project: a user's DWG is their
# office's data and does not belong in a public repository (the NCZ corpus says the
# same, scripts/ncz-tohum.py). The valid seeds are copied from LibreDWG's own
# test drawings — `test/test-data` in the repository the build already pins
# (cmake/PiriCADDependencies.cmake), GPL-3.0-or-later like the library — which
# tests/unit/test_io.cpp already reads through `PIRICAD_DWG_SAMPLES`. The small ones
# were picked, one or two per format generation from release 1.4 to 2018, because a
# fuzzer works faster on a 5 KB file than on a 500 KB one and every generation is a
# different decoder: R2004 compresses its pages, R2007 re-encodes its strings, and
# the old releases have no section table at all.
#
# The DAMAGED seeds are derived from those, by rules written out below, so a reviewer
# reads WHAT each one breaks rather than diffing bytes. Running this again writes the
# same bytes: nothing here is random.
#
#   python3 scripts/dwg-tohum.py                      # test data from build/dev, write the corpus
#   python3 scripts/dwg-tohum.py TEST_DATA [OUT_DIR]  # a LibreDWG checkout's test/test-data
import hashlib
import pathlib
import random
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = ROOT / "build" / "dev" / "_deps" / "libredwg-src" / "test" / "test-data"
DEFAULT_OUT = ROOT / "tests" / "fuzz" / "tohum" / "dwg"

# (seed name, path inside LibreDWG's test/test-data)
VALID = [
    ("01-r1-4-nesneler.dwg", "r1.4/entities.dwg"),
    ("02-r2-10-blok.dwg", "r2.10/block.dwg"),
    ("03-r2-6-olcu.dwg", "r2.6/dim.dwg"),
    ("04-r9-nesneler.dwg", "r9/entities.dwg"),
    ("05-r10-nesneler.dwg", "r10/entities.dwg"),
    ("06-r11-nesneler-2d.dwg", "r11/entities-2d.dwg"),
    ("07-r14.dwg", "r14/v.dwg"),
    ("08-2000-sade.dwg", "sample_2000.dwg"),
    ("09-2000-nesneler.dwg", "2000/entities-2d.dwg"),
    ("10-2004-tarama.dwg", "2004/HatchG.dwg"),
    ("11-2007-nokta.dwg", "2007/Point.dwg"),
    ("12-2010-nokta.dwg", "2010/Point.dwg"),
    ("13-2013-coklu-cizgi.dwg", "2013/Polyline.dwg"),
    ("14-2018-sade.dwg", "sample_2018.dwg"),
    ("15-2018-yay.dwg", "2018/Arc.dwg"),
    # A real, undamaged 2018 drawing whose only entity is one this reader does not
    # translate: it is REFUSED ("no readable geometry"), and the refusal must leave
    # the document untouched. The seam's other exit, next to the damaged files.
    ("16-2018-okunmayan.dwg", "2018/Text.dwg"),
]

# Release 2000 ("AC1015"): the file header holds a table of section locators —
# a record count (RL) at 0x15, then 9-byte records of (number RC, seeker RL, size RL)
# from 0x19. Every seed below that overwrites it is the same attack on the same
# field: a number the reader will believe about its own file.
R2000_COUNT_AT = 0x15
R2000_FIRST_RECORD = 0x19
R2000_RECORD = 9


def damaged(sources):
    """name -> bytes, each derived from a valid seed by one stated rule."""
    r2000 = sources["08-2000-sade.dwg"]
    r2013 = sources["13-2013-coklu-cizgi.dwg"]
    out = {}

    # Truncation in the middle of the data, in two generations: the compressed one
    # (2013) and the uncompressed one with a locator table (2000).
    out["17-kesik-2013.dwg"] = r2013[: len(r2013) * 55 // 100]
    out["18-kesik-2000.dwg"] = r2000[: len(r2000) // 3]

    # Not a drawing at all: 4 KB from a generator with a fixed seed.
    out["19-cop.dwg"] = random.Random(1).randbytes(4096)

    # A real version string and nothing behind it.
    out["20-sahte-baslik.dwg"] = b"AC1032" + b"\x00" * 0x80 + b"\xff" * 0x80

    # The locator table's COUNT claims four billion records.
    big = bytearray(r2000)
    struct.pack_into("<I", big, R2000_COUNT_AT, 0xFFFFFFFF)
    out["21-dizin-tasmasi-2000.dwg"] = bytes(big)

    # One record whose seeker plus size overflows 32 bits, and another that claims
    # two gigabytes at a seeker inside the file.
    big = bytearray(r2000)
    struct.pack_into("<II", big, R2000_FIRST_RECORD + 1, 0xFFFFFFF0, 0xFFFFFFFF)
    struct.pack_into("<II", big, R2000_FIRST_RECORD + 5 * R2000_RECORD + 1, 0x61, 0x7FFFFFFF)
    out["22-bolum-tasmasi-2000.dwg"] = bytes(big)

    return out


def main():
    source = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SOURCE
    out = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT

    if not source.is_dir():
        sys.exit(f"{source}: no LibreDWG test data here. Configure once (cmake --preset dev "
                 "fetches it) or pass the directory of a LibreDWG checkout's test/test-data.")

    sources = {}
    for name, rel in VALID:
        path = source / rel
        if not path.is_file():
            sys.exit(f"{path}: missing — is this the LibreDWG commit the build pins?")
        sources[name] = path.read_bytes()

    seeds = dict(sources)
    seeds.update(damaged(sources))

    out.mkdir(parents=True, exist_ok=True)
    # Numbers from 90 up are regression inputs the fuzzer found (test.md R10): kept by
    # hand, never derived, so never written here and never deleted here.
    for stale in out.glob("*.dwg"):
        if stale.name in seeds:
            continue
        number = stale.name[:2]
        if number.isdigit() and int(number) >= 90:
            continue
        stale.unlink()

    origin = dict(VALID)
    for name in sorted(seeds):
        (out / name).write_bytes(seeds[name])
        digest = hashlib.sha256(seeds[name]).hexdigest()[:12]
        where = f"test-data/{origin[name]}" if name in origin else "derived"
        print(f"{name:32s} {len(seeds[name]):7d} B  {digest}  {where}")
    print(f"{len(seeds)} seeds, {sum(len(b) for b in seeds.values()) // 1024} KiB, in {out}")


if __name__ == "__main__":
    main()
