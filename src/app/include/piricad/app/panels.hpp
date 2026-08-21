// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the right-hand panel stack.
//
// Katmanlar and Öznitelikler share a tab group on the right and can be dragged to
// the left edge, floated, or split apart. Both are read-only views onto the
// document; anything they change goes out as a command (CLAUDE.md Article 1).
#pragma once

#include "piricad/core/document.hpp"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace piricad::app {

class Controller;

/// Layer list: name, visibility, lock, colour swatch, entity count.
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(Controller& controller, QWidget* parent = nullptr);

    void refresh();
    core::LayerId selectedLayer() const;

signals:
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
    explicit PropertyPanel(Controller& controller, QWidget* parent = nullptr);

    void setLayer(core::LayerId layer);
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
