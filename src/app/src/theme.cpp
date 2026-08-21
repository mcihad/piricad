// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/theme.hpp"

namespace piricad::app {
namespace {

const Palette kLight{
    /* window     */ QColor(0xF2, 0xF3, 0xF5),
    /* panel      */ QColor(0xFA, 0xFB, 0xFC),
    /* field      */ QColor(0xFF, 0xFF, 0xFF),
    /* border     */ QColor(0xD6, 0xDA, 0xE0),
    /* text       */ QColor(0x22, 0x27, 0x2E),
    /* textMuted  */ QColor(0x6B, 0x74, 0x82),
    /* accent     */ QColor(0x1F, 0x6F, 0xEB),
    /* accentSoft */ QColor(0xDC, 0xE9, 0xFD),
    /* hover      */ QColor(0xE6, 0xEA, 0xF0),
    /* alternate  */ QColor(0xF4, 0xF6, 0xF8),

    /* canvas     */ QColor(0xFC, 0xFC, 0xFB),
    /* grid       */ QColor(0x00, 0x00, 0x00, 18),
    /* gridMajor  */ QColor(0x00, 0x00, 0x00, 38),
    /* crosshair  */ QColor(0x1F, 0x6F, 0xEB, 120),
    /* rubberBand */ QColor(0xD9, 0x73, 0x06),
    /* hud        */ QColor(0x8A, 0x93, 0xA1),
};

const Palette kDark{
    /* window     */ QColor(0x1B, 0x1E, 0x24),
    /* panel      */ QColor(0x21, 0x25, 0x2C),
    /* field      */ QColor(0x14, 0x17, 0x1C),
    /* border     */ QColor(0x2F, 0x36, 0x41),
    /* text       */ QColor(0xD8, 0xDE, 0xE9),
    /* textMuted  */ QColor(0x9A, 0xA5, 0xB4),
    /* accent     */ QColor(0x5A, 0x9C, 0xF8),
    /* accentSoft */ QColor(0x25, 0x3A, 0x57),
    /* hover      */ QColor(0x2F, 0x36, 0x41),
    /* alternate  */ QColor(0x1A, 0x1E, 0x24),

    /* canvas     */ QColor(0x18, 0x1B, 0x20),
    /* grid       */ QColor(0xFF, 0xFF, 0xFF, 16),
    /* gridMajor  */ QColor(0xFF, 0xFF, 0xFF, 34),
    /* crosshair  */ QColor(0x78, 0xC8, 0xFF, 110),
    /* rubberBand */ QColor(0xFF, 0xBE, 0x50),
    /* hud        */ QColor(0x96, 0xA0, 0xAF),
};

QString hex(const QColor& c)
{
    return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}

} // namespace

const Palette& themePalette(ThemeMode mode)
{
    return mode == ThemeMode::Light ? kLight : kDark;
}

QString themeStyleSheet(ThemeMode mode)
{
    const Palette& p = themePalette(mode);

    return QStringLiteral(R"(
        QMainWindow, QWidget            { background: %1; color: %5; }
        QMenuBar                        { background: %1; border-bottom: 1px solid %4; }
        QMenuBar::item                  { padding: 6px 10px; background: transparent; }
        QMenuBar::item:selected         { background: %9; border-radius: 4px; }
        QMenu                           { background: %2; border: 1px solid %4; padding: 4px; }
        QMenu::item                     { padding: 6px 24px 6px 12px; border-radius: 4px; }
        QMenu::item:selected            { background: %8; color: %5; }
        QMenu::separator                { height: 1px; background: %4; margin: 4px 8px; }

        QDockWidget                     { color: %6; font-size: 11px; }
        QDockWidget::title              { background: %2; border: 1px solid %4;
                                          border-bottom: none; padding: 7px 10px;
                                          text-transform: uppercase; letter-spacing: 0.6px; }
        QDockWidget > QWidget           { background: %2; border: 1px solid %4; }

        QTabBar::tab                    { background: %1; color: %6; border: 1px solid %4;
                                          border-bottom: none; padding: 6px 14px;
                                          margin-right: 2px;
                                          border-top-left-radius: 4px;
                                          border-top-right-radius: 4px; }
        QTabBar::tab:selected           { background: %2; color: %5; }
        QTabBar::tab:hover:!selected    { background: %9; }

        QTreeWidget, QTableWidget, QPlainTextEdit, QLineEdit, QListWidget {
            background: %3; color: %5; border: 1px solid %4;
            selection-background-color: %7; selection-color: #ffffff;
        }
        QTreeWidget::item, QListWidget::item { padding: 3px 2px; }
        QTreeWidget::item:selected, QListWidget::item:selected { background: %7; }
        QHeaderView::section            { background: %2; color: %6; border: none;
                                          border-bottom: 1px solid %4; padding: 5px 6px;
                                          font-weight: 600; }

        QLineEdit                       { padding: 7px 10px; border-radius: 4px;
                                          font-family: monospace; font-size: 13px; }
        QLineEdit:focus                 { border: 1px solid %7; }

        QStatusBar                      { background: %2; border-top: 1px solid %4; }
        QStatusBar QLabel               { color: %6; padding: 0 6px; }
        QStatusBar::item                { border: none; }

        QToolButton                     { background: transparent; border: 1px solid transparent;
                                          border-radius: 5px; padding: 5px; }
        QToolButton:hover               { background: %9; border-color: %4; }
        QToolButton:checked,
        QToolButton:pressed             { background: %8; border-color: %7; }

        QScrollBar:vertical             { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical     { background: %4; border-radius: 5px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: %6; }
        QScrollBar:horizontal           { background: transparent; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal   { background: %4; border-radius: 5px; min-width: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        QSplitter::handle               { background: %4; }
        QFrame[frameShape="4"]          { color: %4; }
    )")
        .arg(hex(p.window), hex(p.panel), hex(p.field), hex(p.border), hex(p.text),
             hex(p.textMuted), hex(p.accent), hex(p.accentSoft), hex(p.hover));
}

} // namespace piricad::app
