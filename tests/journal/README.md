# Journal regression pack

Recorded real sessions, replayed nightly and compared against golden output.
kentoscad.md §14 calls this the most valuable test kind for CAD software.

Each file is a command array that the bus replays exactly as it was recorded.
`tests/unit/test_proof.cpp` proves the replay is faithful; the nightly job that
diffs the resulting documents is a Phase-1 deliverable (.claude/test.md).

Run one by hand:

    make run-script SCRIPT=tests/journal/ornek-parsel.json
