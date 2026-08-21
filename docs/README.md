# Documentation

| File | Contents |
|---|---|
| `api-stability.md` | What each public surface guarantees, and the deprecation policy |
| `COMMANDS.md` | Generated command reference — never edit by hand |

`COMMANDS.md` is produced from the command registry
(`Registry::markdown_reference()`), which is the single source of truth for every
command in the project (piricad.md §2.3). Regenerate it with `make reference`.

User documentation (MkDocs) and API documentation (Doxygen) are Phase-1
deliverables, both published from CI (piricad.md §13).
