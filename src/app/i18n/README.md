# Translations

Source language is **Turkish**. `kentos_tr.ts` is the identity catalogue that
keeps every user-visible string discoverable; `kentos_en.ts` carries the English
translation. Both are checked for completeness in CI (CLAUDE.md 6.9).

Regenerate from the sources:

    lupdate ../src ../include -ts kentos_tr.ts kentos_en.ts
    lrelease kentos_tr.ts kentos_en.ts

Turkish case conversion goes through `QLocale(QLocale::Turkish)` at the Qt layers
and through `kentos::core::turkish_upper` in the Qt-free layers. `std::toupper`
is banned project-wide (CLAUDE.md 5.6) and checked by `scripts/ci-gate-i18n.sh`.
