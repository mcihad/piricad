// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the right-hand panel stack.
//
// Katmanlar and Öznitelikler share a tab group on the right and can be dragged to
// the left edge, floated, or split apart. Both are read-only views onto the
// document; anything they change goes out as a command (CLAUDE.md Article 1).
#pragma once

#include "piricad/core/document.hpp"

#include <QIcon>
#include <QPoint>
#include <QWidget>

/// Qt widgets this header only holds pointers to.
class QTreeWidget;
class QTreeWidgetItem;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Layer list: name, visibility, lock, colour swatch, entity count.
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    /// Builds the panel over a controller, which outlives it.
    explicit LayerPanel(Controller& controller, QWidget* parent = nullptr);

    /// Rebuilds the list from the document. Called on every document change
    /// rather than patched incrementally: a layer list is tens of rows, and a
    /// panel that maintained its own copy could disagree with the document — which
    /// is the class of bug a single source of truth exists to prevent.
    void refresh();

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
};

/// Property sheet for the current selection: the document itself, or the layer
/// picked in the layer panel.
class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    /// Builds the panel over a controller, which outlives it.
    explicit PropertyPanel(Controller& controller, QWidget* parent = nullptr);

    /// Shows the properties of one layer. `kNoLayer` shows the document's own.
    void setLayer(core::LayerId layer);

    /// Rebuilds the sheet from whatever it is currently showing.
    void refresh();

private:
    void addGroup(const QString& title);
    void addRow(const QString& key, const QString& value);

    Controller& controller_;
    QTreeWidget* tree_{nullptr};
    QTreeWidgetItem* group_{nullptr};
    core::LayerId layer_{core::kNoLayer};
};

} // namespace piricad::app
