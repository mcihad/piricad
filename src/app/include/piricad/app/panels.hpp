// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the right-hand panel stack.
//
// Katmanlar and Öznitelikler share a tab group on the right and can be dragged to
// the left edge, floated, or split apart. Both are read-only views onto the
// document; anything they change goes out as a command (CLAUDE.md Article 1).
#pragma once

#include "piricad/core/document.hpp"

#include "piricad/app/theme.hpp"

#include <QIcon>
#include <QPoint>
#include <QStyledItemDelegate>
#include <QWidget>

/// Qt widgets this header only holds pointers to.
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Paints one layer row exactly as `design.md` §7 draws it: eye, colour chip,
/// name, entity count, lock — one row, no columns, no header.
///
/// A DELEGATE RATHER THAN FOUR COLUMNS. Four columns put a header on the panel
/// and let the user drag the boundaries, and the reference has neither: the row
/// is a fixed composition and the eye and the lock are at fixed offsets from the
/// two edges. Painting it is how those offsets become the numbers in the file.
class LayerRowDelegate : public QStyledItemDelegate, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Which mark the pointer is over, so the row can answer a click.
    enum class Hit { None, Eye, Lock, Row };

    explicit LayerRowDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Where `at` falls inside a row of `width`.
    static Hit hitTest(int x, int width);

    void applyTheme(ThemeMode mode) override { theme_ = mode; }

signals:
    /// The eye was clicked on `layer`; the panel turns it into a `KATMAN` call.
    void toggleVisible(int layer);

    /// The lock was clicked on `layer`.
    void toggleLocked(int layer);

private:
    ThemeMode theme_ = ThemeMode::Dark;
};

/// Layer list: name, visibility, lock, colour swatch, entity count.
class LayerPanel : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds the panel over a controller, which outlives it.
    explicit LayerPanel(Controller& controller, QWidget* parent = nullptr);

    /// Rebuilds the list from the document. Called on every document change
    /// rather than patched incrementally: a layer list is tens of rows, and a
    /// panel that maintained its own copy could disagree with the document — which
    /// is the class of bug a single source of truth exists to prevent.
    void refresh();

    void applyTheme(ThemeMode mode) override;

    /// The layer the user has picked, or `kNoLayer`. This is SELECTION state and
    /// therefore not document state (model.md R43).
    core::LayerId selectedLayer() const;

signals:
    /// Emitted when the user picks a row, so the property panel can follow.
    void layerSelected(core::LayerId layer);

    /// Emitted when the user asks to edit a layer's style, so the shell can open
    /// the designer. The panel does not own the dialog: a panel that opened a
    /// window would be a panel that has to know what is in it.
    void styleRequested(const QString& layerName);

private:
    void onItemActivated(QTreeWidgetItem* item, int column);

    /// The right-click menu. Every entry leaves through the command bus, so a
    /// script can do the same things (Article 1.2).
    void showContextMenu(const QPoint& where);

    /// A small preview of what this layer draws, rendered by the CANVAS backend
    /// so the swatch and the map cannot disagree.
    ///
    /// `used` is the style its entities carry, or `kByLayerStyle` when they carry
    /// none — in which case the layer's own default is what it draws.
    QIcon layerIcon(const core::Layer& layer, core::StyleId used) const;

    Controller& controller_;
    QTreeWidget* tree_{nullptr};
    LayerRowDelegate* rows_{nullptr};
    QLabel* footer_{nullptr};
};

} // namespace piricad::app
