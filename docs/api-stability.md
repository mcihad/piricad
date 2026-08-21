# API Stability Policy

PiriCAD follows [Semantic Versioning](https://semver.org). This document states
what each surface guarantees, which is a world-standard checklist item
(piricad.md §13) and a merge requirement for `/src/plugin-api` changes
(CLAUDE.md 6.10).

## Surfaces and their guarantees

| Surface | Guarantee | Breaks on |
|---|---|---|
| Plugin C ABI (`/src/plugin-api`) | Additive-only within a major version; version handshake on load | major only |
| Command ids (`core.line`, …) | Stable forever once released; renamed ids keep an alias | never |
| Command names (`ÇİZGİ`, `LINE`, `Ç`) | Additive; a released name is never repurposed | never |
| Command parameters | Additive; a released parameter keeps its name, type and meaning | major |
| Journal JSONL schema | Forward-compatible; unknown fields are ignored on replay | major |
| Project file format | Versioned; an older build opening a newer file gives an explanatory message, never a crash | major |
| C++ headers under `piricad/` | Internal. No guarantee between minor versions | any |
| Script API (`h.komut`, read APIs) | Additive within a major version | major |

## Deprecation

1. Mark the surface deprecated in the same release that ships its replacement.
2. Keep it working for **two minor versions** minimum, with a runtime warning.
3. Record the removal in `CHANGELOG.md` under a `### Kaldırıldı` heading.
4. A command id is never removed — it becomes an alias for its successor.

## Why command ids are permanent

The journal is the project's undo, macro, regression, crash-recovery and remote
API mechanism at once (piricad.md §2.2). A recorded session from three years ago
must still replay. That makes a command id a data format, not an implementation
detail.
