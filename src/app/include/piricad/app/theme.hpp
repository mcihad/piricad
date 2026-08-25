// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the light/dark palette.
//
// Day mode is the default. A dark theme is a world-standard checklist item
// (piricad.md §13), so both are first class and every colour used by the shell
// and the canvas comes from here — no widget picks its own.
#pragma once

#include <QColor>
#include <QWidget>
#include <QString>

namespace piricad::app {

/// Which of the two palettes is in force.
///
/// The setting `core.arayuz.tema` also offers "sistem", which resolves to one of
/// these before it reaches this type: the renderer needs a colour, not a policy.
enum class ThemeMode { Light, Dark };

/// Every colour the shell and the canvas draw with, for one theme.
///
/// A struct of named colours rather than a lookup by string: a missing key would
/// be a black widget discovered by a user, while a missing member is a compile
/// error discovered by whoever added the theme.
struct Palette
{
    QColor window;     ///< chrome background
    QColor panel;      ///< dock and panel background
    QColor field;      ///< editable surfaces
    QColor border;     ///< separators and widget outlines
    QColor text;       ///< primary label and content text
    QColor textMuted;  ///< secondary text: units, hints, disabled items
    QColor accent;     ///< the active-selection colour of the shell
    QColor accentSoft; ///< the same accent at low weight, for backgrounds
    QColor hover;      ///< pointer-over feedback
    QColor alternate;  ///< alternating row background; unset it and dark rows wash out

    QColor canvas;     ///< drawing background
    QColor grid;       ///< minor grid line
    QColor gridMajor;  ///< every Nth line, so distance is readable without labels
    QColor crosshair;  ///< the cursor's full-height and full-width guides
    QColor rubberBand; ///< the dashed preview from the last point to the cursor
    QColor hud;        ///< F12 developer overlay

    QColor selection;    ///< highlight on a selected entity
    QColor selectWindow; ///< PENCERE box: what is wholly inside
    QColor selectCross;  ///< KESEN box: whatever the box touches
    QColor snapMarker;   ///< object-snap glyph and its label
};

/// The palette for a mode. Returned by reference to a constant: a palette is a
/// description of the product, not state, so there is exactly one of each.
const Palette& themePalette(ThemeMode mode);

/// The whole application stylesheet for a mode.
QString themeStyleSheet(ThemeMode mode);

/// Anything that paints from `tokens.hpp` and must be told when the theme moves.
///
/// WHY AN INTERFACE AND NOT A CONVENTION. Every painted widget needs the same
/// call, and the shell got it wrong exactly the way conventions get gotten wrong:
/// `SettingsDialog` never told its `SectionList`, so the settings window kept a
/// black sidebar in the light theme — the widget was still painting `darkTokens()`
/// because nobody had said otherwise. A window can now hand the theme to every
/// painted child it contains without knowing what any of them are, and a class
/// that forgets to declare this is a class the compiler cannot help with but the
/// eye can: it simply does not change colour.
class Themed
{
public:
    /// An interface, so it owns nothing and copies nowhere: the widget that
    /// implements it is a `QObject` and Qt already forbids copying one.
    Themed()          = default;
    virtual ~Themed() = default;

    Themed(const Themed&)            = delete;
    Themed& operator=(const Themed&) = delete;

    /// Re-reads the tokens and repaints.
    virtual void applyTheme(ThemeMode mode) = 0;
};

/// Hands `mode` to every painted descendant of `root`, and to `root` itself.
///
/// One walk, so a window cannot theme half of itself. Widgets that do not
/// implement `Themed` are skipped — they are styled by the sheet instead.
void applyThemeToChildren(QWidget* root, ThemeMode mode);

/// Installs `Fusion` under the one behaviour change the specification needs.
///
/// design.md 7 shows ten menu titles with NO mnemonic underline, and every Qt
/// style on Linux draws them permanently. Windows shows them only while Alt is
/// held, which is both what the reference looks like at rest and what keeps the
/// keyboard path discoverable the moment a user reaches for it (design.md 13) —
/// so that is the behaviour, on all three platforms.
void installShellStyle();

/// Loads the bundled IBM Plex faces from `data/fonts` and makes Sans the
/// application font (`design.md` §3).
///
/// Returns false when the files are not where `data_root()` says they are, which
/// the caller reports rather than swallows: falling back to the platform's own
/// sans is exactly the "three different applications" §12 forbids, so the user is
/// told which directory was searched instead of quietly getting the wrong face.
bool loadShellFonts(QString* whereLooked = nullptr);

} // namespace piricad::app

Q_DECLARE_INTERFACE(piricad::app::Themed, "org.piricad.app.Themed")
