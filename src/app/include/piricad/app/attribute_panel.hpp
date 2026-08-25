// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the attributes panel, `design.md` §7.
//
// A selected-object card over collapsible groups of key/value rows. The grid is
// `112px | 1fr`: the field name on the left in the interface face, the value on
// the right in mono, because a value is data and data is read in mono.
//
// IT READS THE DOCUMENT AND NEVER WRITES IT. Editing a cell dispatches
// `ÖZNİTELİK` through the bus like any other client (CLAUDE.md 5.9); the panel
// owns no path of its own into the entity store.
#pragma once

#include "piricad/app/theme.hpp"
#include "piricad/core/layer.hpp"

#include <QString>
#include <QVector>
#include <QWidget>

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// One value cell can carry a small badge — `HESAP` for a derived number,
/// `BOŞ` for a cell nobody has filled. §7 names both.
struct AttributeRow
{
    QString key;          ///< the column's Turkish name, or its id when it has none
    QString value;        ///< what the cell holds, already formatted for paper
    QString badge;        ///< `HESAP` or `BOŞ`, or empty for a plain stored value
    bool derived = false; ///< computed rather than stored: shown, never edited
};

struct AttributeGroup
{
    QString title;              ///< the uppercase heading on the group bar
    QVector<AttributeRow> rows; ///< in declaration order, never sorted
    bool open = true;           ///< collapsed groups keep their rows, just unpainted
};

class AttributePanel : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds the panel over a controller, which outlives it.
    explicit AttributePanel(Controller& controller, QWidget* parent = nullptr);

    /// Re-reads the selection and rebuilds the card and the groups.
    void refresh();

    /// Shows one LAYER's properties instead of the selection's. `kNoLayer`
    /// returns the panel to whatever is selected.
    ///
    /// design.md 7 gives the shell ONE property surface, so a layer picked in the
    /// Katmanlar panel and an entity picked on the canvas both land here rather
    /// than in two panels that can disagree about what is being looked at.
    void setLayer(core::LayerId layer);

    void applyTheme(ThemeMode mode) override;

protected:
    /// Painted rather than laid out, for the reason the file header gives: the
    /// `112px | 1fr` grid and the 26 px row are exact numbers, and a layout of
    /// labels reaches them only by accident.
    void paintEvent(QPaintEvent* event) override;

    /// A click on a group bar collapses or opens it. Nothing else is clickable
    /// yet; editing a cell will dispatch `ÖZNİTELİK` when it lands.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// Lays the card and the groups out and returns the total height, so the
    /// same walk serves painting, hit testing and the scroll range.
    int layout(QVector<QPair<int, int>>* headerBands) const;

    Controller& controller_;
    QVector<AttributeGroup> groups_;
    QString title_;    ///< `Parsel 1284 / 21`
    QString subtitle_; ///< `POLYGON · fid 4128 · 4 köşe`
    int glyph_           = 0;
    core::LayerId layer_ = core::kNoLayer;
    int scroll_          = 0;
    int hotGroup_        = -1;
    ThemeMode theme_     = ThemeMode::Dark;
};

} // namespace piricad::app
