# Golden data

Reference outputs that must match **bit for bit** on Linux, Windows and macOS in
the same CI run (piricad.md §7.3, §10.5). This is why `-ffp-contract=off` is
mandatory and `-ffast-math` is banned: a survey document's area, intersection and
adjustment results are legal figures and may not depend on the machine that
produced them.

A golden file is never deleted to make a test pass. It is regenerated only with a
reviewed, explained diff (`.claude/test.md`).

Empty in Phase 0; seeded in Phase 1 alongside the PROJ/TKGM reference comparison.
