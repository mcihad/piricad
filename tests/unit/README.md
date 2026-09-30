# Unit tests

`test_core.cpp`    — fixed-point units, Turkish casing, document ops, JSON.
`test_command.cpp` — registry, parser, validation, transactions, undo, batching.
`test_proof.cpp`   — **the Phase-0 keystone proof** (piricad.md §16.5): the same
                     `ÇİZGİ` command from a GUI button, from the command line and
                     from a JSON script produces an identical document and an
                     identical journal. If this stops holding, the architecture is
                     broken and the build must fail.

The harness is doctest, pinned in `cmake/PiriCADDependencies.cmake`.
`tests/support/piricad_test.hpp` adds one macro doctest has no concept of —
`PENDING(reason)`, for a case an optional dependency makes unrunnable, which
`.claude/data.md` requires to report as pending and never as passing.
