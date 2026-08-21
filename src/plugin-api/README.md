# /src/plugin-api — the stable C ABI

Empty in Phase 0. Rules already binding: `.claude/plugin-api.md`.

Pure C99 across the boundary: no C++ types, no STL containers, no exceptions, no
templates. Versioned and additive-only within a major version, with a handshake on
load. A plugin reaches the application only through the command bus and the read
APIs — exactly like a script, with the same güvenli/proje/tam sandbox levels.

Enforced by `scripts/ci-gate-abi.sh` and `scripts/ci-gate-visibility.sh`.
