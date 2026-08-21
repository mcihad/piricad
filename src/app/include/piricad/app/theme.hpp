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

enum class ThemeMode { Light, Dark };

struct Palette
{
    QColor window; ///< chrome background
    QColor panel;  ///< dock and panel background
    QColor field;  ///< editable surfaces
    QColor border;
    QColor text;
    QColor textMuted;
    QColor accent;
    QColor accentSoft;
    QColor hover;
    QColor alternate; ///< alternating row background; unset it and dark rows wash out

    QColor canvas; ///< drawing background
    QColor grid;
    QColor gridMajor;
    QColor crosshair;
    QColor rubberBand;
    QColor hud;
};

const Palette& themePalette(ThemeMode mode);

/// The whole application stylesheet for a mode.
QString themeStyleSheet(ThemeMode mode);

} // namespace piricad::app
