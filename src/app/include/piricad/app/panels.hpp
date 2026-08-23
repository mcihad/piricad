// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the right-hand panel stack.
//
// Katmanlar and Öznitelikler share a tab group on the right and can be dragged to
// the left edge, floated, or split apart. Both are read-only views onto the
// document; anything they change goes out as a command (CLAUDE.md Article 1).
#pragma once

#include "piricad/core/document.hpp"

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

private:
    void onItemActivated(QTreeWidgetItem* item, int column);

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
