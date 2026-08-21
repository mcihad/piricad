# Unit tests

`test_core.cpp`    — fixed-point units, Turkish casing, document ops, JSON.
`test_command.cpp` — registry, parser, validation, transactions, undo, batching.
`test_proof.cpp`   — **the Phase-0 keystone proof** (piricad.md §16.5): the same
                     `ÇİZGİ` command from a GUI button, from the command line and
                     from a JSON script produces an identical document and an
                     identical journal. If this stops holding, the architecture is
                     broken and the build must fail.

The harness is `tests/support/microtest.hpp`, 60 lines standing in for doctest
until the dependency set lands (CLAUDE.md Article 8.2).
