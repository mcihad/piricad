# Packaging

CPack targets, one per platform (kentoscad.md §8, §14). `make package` builds the
release preset and runs CPack.

| Platform | Format | Tooling |
|---|---|---|
| Windows | MSI | WiX, signed with a Windows EV certificate |
| macOS | DMG | Apple Developer ID + notarization |
| Linux | AppImage, `.deb`, `.rpm`, Flatpak | CPack + flatpak-builder |

Two release requirements are non-negotiable (kentoscad.md §13):

- **Reproducible builds.** The same source and toolchain produce the same bytes.
- **A CycloneDX SBOM on every release.** GPL compliance requires knowing exactly
  what shipped; `NOTICE` and the SBOM are regenerated together with any
  dependency change (CLAUDE.md 6.10).

Empty in Phase 0. The packaging job lands in Phase 1 with the CI matrix.
