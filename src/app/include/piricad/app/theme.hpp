// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the light/dark palette.
//
// Day mode is the default. A dark theme is a world-standard checklist item
// (piricad.md §13), so both are first class and every colour used by the shell
// and the canvas comes from here — no widget picks its own.
#pragma once

#include <QColor>
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

} // namespace piricad::app
