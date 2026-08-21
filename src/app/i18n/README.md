# Translations

Source language is **Turkish**. `piricad_tr.ts` is the identity catalogue that
keeps every user-visible string discoverable; `piricad_en.ts` carries the English
translation. Both are checked for completeness in CI (CLAUDE.md 6.9).

Regenerate from the sources:

    lupdate ../src ../include -ts piricad_tr.ts piricad_en.ts
    lrelease piricad_tr.ts piricad_en.ts

Turkish case conversion goes through `QLocale(QLocale::Turkish)` at the Qt layers
and through `piricad::core::turkish_upper` in the Qt-free layers. `std::toupper`
is banned project-wide (CLAUDE.md 5.6) and checked by `scripts/ci-gate-i18n.sh`.
