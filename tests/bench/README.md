# Benchmarks

The piricad.md §10.1 budgets, enforced as gates. A benchmark more than **10%
worse than the stored baseline breaks the build**, and a budget is never relaxed
to make a benchmark pass (CLAUDE.md Article 7).

| Scenario | Gate |
|---|---|
| Pan/zoom, 5M-polygon cadastral layer | ≤ 16 ms/frame |
| Open 200 MB DWG | ≤ 3 s |
| First paint, 50M-point LAZ | ≤ 5 s |
| Topological validation, 100k parcels | ≤ 2 s |
| Cold start | ≤ 2 s |
| Command-line keystroke → screen | ≤ 30 ms |
| Command dispatch from a script | ≤ 10 µs |
| RAM, empty project | ≤ 300 MB |

Empty in Phase 0. `make bench` reports this.
