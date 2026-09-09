// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the design tokens, and the one place they are written down.
//
// `Screenshots/design.md` §2 is the specification and this is its transcription.
// Every colour the shell paints comes from here: the palette, the stylesheet and
// the canvas all read these names, so a token changed here changes the whole
// application and nothing needs a second edit.
//
// WHY A TOKEN AND NOT A COLOUR IN PLACE. A colour written where it is used is a
// colour nobody can find. The dialogs each carried their own `setStyleSheet` with
// their own greys, which is how the settings window and the style designer ended
// up not matching each other or the shell — the same defect a second command list
// would be (CLAUDE.md 5.10), in a different material.
//
// THE SAME ON EVERY PLATFORM. design.md §12 is explicit: `Fusion` plus one full
// stylesheet, no native theme inherited anywhere. A token set that leaves a widget
// unstyled is a widget that looks like Windows on Windows and like GNOME on
// GNOME, which is exactly what the specification forbids.
//
// LIGHT IS NOT AN AFTERTHOUGHT. design.md §12 says the light theme is produced
// from the same structure by a second mapping, so both are declared here side by
// side and neither can gain a token the other lacks — the struct sees to that.
#pragma once

#include <QColor>

namespace kentos::app {

/// The design tokens of one theme. Named after `design.md` §2, in its order.
///
/// A struct rather than a map: a missing key would be a runtime surprise and a
/// silent black, and there is no reading of this program in which a token is
/// optional.
struct Tokens
{
    // ---- surfaces, back to front ----
    QColor bgApp;         ///< outside the window, behind everything
    QColor bgWindow;      ///< window body, odd table row
    QColor bgPanel;       ///< dock panels, tool box
    QColor bgRaised;      ///< toolbars, footer bars, dialog footer
    QColor bgHeader;      ///< table header, group header
    QColor bgTitlebar;    ///< DIALOG title bar (the top stop of its gradient)
    QColor bgTitleBottom; ///< the bottom stop of that gradient
    QColor bgShellBar;    ///< the MAIN WINDOW title bar, a step darker than a dialog's
    QColor bgShellBottom; ///< the bottom stop of that gradient
    QColor bgInput;       ///< text field, combo, search
    QColor bgCanvas;      ///< the drawing itself

    // ---- lines ----
    QColor gridMinor; ///< the fine lattice
    QColor gridMajor; ///< every Nth line
    QColor lineHard;  ///< a hard 1 px divide between regions
    QColor lineSoft;  ///< a row separator
    QColor border;    ///< input and button outline

    // ---- text ----
    QColor text;      ///< primary
    QColor textDim;   ///< labels, secondary
    QColor textFaint; ///< section headings, hints, units

    // ---- meaning ----
    //
    // design.md §1.2: the accent means exactly three things — selection, active
    // tool, primary action — and warn means exactly one, the snap and edited
    // data. No other saturated colour appears in the chrome at all.
    QColor accent;
    QColor accentHi;   ///< text and icons ON the accent
    QColor accentWash; ///< the background of a selected row
    QColor warn;       ///< snap marker, edited cell, lock
    QColor warnWash;   ///< the ground of a field changed but not yet saved
    QColor ok;         ///< connection state

    // A fourth meaning, and the only one that is not in §2's original four:
    // DANGER. `bileşen_standardı.png` gives destructive actions and invalid
    // fields their own hue, because "this cannot be undone" and "this is wrong"
    // are not the same statement as "look here" (accent) or "be careful" (warn).
    QColor danger;     ///< the ink of a destructive action and an invalid value
    QColor dangerEdge; ///< its outline, a step back from the ink
    QColor dangerWash; ///< the ground of an invalid field

    // The two accent edges the component sheet distinguishes: a PRIMARY button
    // carries a lighter rim than its fill, and an ACTIVE control carries a
    // darker one. One token for both made the mode toggle look pressed and the
    // primary button look flat.
    QColor accentLift; ///< the rim of a filled accent surface
    QColor accentEdge; ///< the outline of an active or checked control

    // ---- states, §11 ----
    QColor onAccent;  ///< text and icons ON a filled accent surface, §11
    QColor onHover;   ///< icon colour under the pointer, §5
    QColor hoverIcon; ///< background under a hovered icon button
    QColor hoverRow;  ///< background under a hovered list row
    QColor rowOdd;    ///< zebra, odd
    QColor rowEven;   ///< zebra, even

    // ---- shell chrome, §7 ----
    //
    // The title bar reads at four distinct weights and the specification gives
    // each its own grey: the document name is quieter than body text, the search
    // placeholder quieter still, and the shortcut hint quietest of all. Folding
    // them into `textDim` would flatten a hierarchy the screenshot has.
    //
    // THE ORDER OF THESE FIELDS IS LOAD-BEARING. `tokens.cpp` initialises the two
    // themes positionally, so a field inserted here without the matching value
    // there shifts every colour after it into the wrong name — silently, because
    // every value is still a valid colour. `ci-gate-tokens.sh` compares the two
    // sequences name by name for exactly that reason.
    QColor bgStrip;      ///< tab strip, command line, status bar — the darkest chrome
    QColor bgTabActive;  ///< the selected document tab, a step above the strip
    QColor hoverChip;    ///< a menu title or chip under the pointer
    QColor menuText;     ///< the ten menu titles, a step quieter than body text
    QColor titleText;    ///< the document name in the middle of the title bar
    QColor hint;         ///< placeholder text inside a search field
    QColor hintFaint;    ///< the keyboard shortcut printed beside it
    QColor onAccentDark; ///< text on an accent chip that wants a DARK glyph
    QColor separator;    ///< the 1x22 rule between tool bar groups
    QColor readout;      ///< a mono value the user reads but cannot edit
    QColor readoutDim;   ///< the same, one step back: status coordinates, scale bar
    QColor bgSunken;     ///< command line, canvas rulers, the zoom stack
    QColor rulerTick;    ///< the division mark on a ruler, and its hairline

    // ---- what the canvas draws that the chrome does not ----
    QColor crosshair;
    QColor rubberBand;
    QColor hud;
    QColor selectWindow; ///< PENCERE box: wholly inside
    QColor selectCross;  ///< KESEN box: whatever it touches

    // ---- the expression bar, §9 ----
    //
    // A filter is read as much as it is typed, and §9 gives its four parts of
    // speech four inks so `"alan_m2" > 2000 AND "plan_fonksiyon" = 'Konut'` can
    // be parsed by eye: field, operator, text constant, logical word.
    QColor syntaxField;    ///< a quoted column name
    QColor syntaxOperator; ///< `=`, `>`, `<>` and the arithmetic
    QColor syntaxString;   ///< a single-quoted constant
    QColor syntaxLogical;  ///< AND, OR, NOT and their Turkish names
};

/// The dark tokens — `design.md` §2, verbatim.
const Tokens& darkTokens();

/// The light tokens: the same structure, mapped for a light ground.
const Tokens& lightTokens();

} // namespace kentos::app
