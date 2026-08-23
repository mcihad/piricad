// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the style designer.
//
// The layout is QGIS's symbol selector, deliberately and openly: the whole symbol
// previewed at the top, its layer STACK on the left with add/remove/reorder, the
// selected layer's properties on the right. That arrangement is the right one and
// every planner who has used QGIS already knows it.
//
// What is NOT QGIS is the machinery behind it, and the reason is measurable
// rather than a preference. Linking `libqgis_gui` to reuse the dialog itself
// pulls 254 shared objects and 75 MB, and `QgsApplication::initQgis()` loads a
// provider registry and an SRS database into a program whose cold-start budget is
// two seconds (CLAUDE.md Article 7). It is also lossy exactly where this project
// differs on purpose: `QgsRasterFillSymbolLayer` names a FILE, while a PiriCAD
// raster fill carries the bytes and their provenance inside the document, which
// is what lets a drawing survive being emailed to the belediye that checks it.
// Licence is not the objection — QGIS is GPL-2.0-or-later and compatible.
//
// EVERY EDIT LEAVES AS A COMMAND. The dialog builds a `core::Symbol` while the
// user works and, on accept, emits the `STİL` invocations that produce it. There
// is no path from this window to the document that a script cannot take (Article
// 1.1, 1.2) — which is also what makes the designer teachable to the AI.
#pragma once

#include "piricad/core/image_store.hpp"
#include "piricad/core/style.hpp"

#include <QDialog>
#include <QString>

/// The Qt widgets this dialog holds, declared rather than included: a header that
/// pulls in the widget classes it stores pointers to makes every translation unit
/// including it wait for them.
class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;
class QToolButton;

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Designs one symbol and applies it to a layer through the command bus.
class StyleDesigner : public QDialog
{
    Q_OBJECT

public:
    /// Opens the designer on `layerName`, starting from what that layer draws.
    StyleDesigner(Controller& controller, QString layerName, QWidget* parent = nullptr);

    /// The symbol as the user left it.
    const core::Symbol& symbol() const noexcept { return symbol_; }

private:
    /// Rebuilds the stack list and the big preview from `symbol_`.
    void refresh();

    /// Loads the selected layer's values into the property side.
    void loadSelected();

    /// Writes the property side back into the selected symbol layer.
    void applyToSelected();

    /// The index the stack list has selected, or -1.
    int currentLayer() const;

    void addLayer();
    void removeLayer();
    void moveLayer(int delta);

    /// Emits the `STİL` invocations that reproduce `symbol_` on the layer.
    void applyToDocument();

    /// Writes the symbol to the application's own settings directory as a
    /// loadable gösterim package.
    void saveToLibrary();

    Controller& controller_;
    QString layerName_;
    core::Symbol symbol_{};

    /// Guards the property widgets while they are being filled from the model, so
    /// a programmatic `setValue` does not read straight back as a user edit.
    bool loading_{false};

    QLabel* preview_{nullptr};
    QListWidget* stack_{nullptr};

    QComboBox* type_{nullptr};
    QComboBox* shape_{nullptr};
    QComboBox* placement_{nullptr};
    QComboBox* unit_{nullptr};
    QToolButton* stroke_{nullptr};
    QToolButton* fill_{nullptr};
    QSpinBox* width_{nullptr};
    QSpinBox* size_{nullptr};
    QSpinBox* interval_{nullptr};
    QSpinBox* spacingY_{nullptr};
    QSpinBox* angle_{nullptr};
    QSpinBox* opacity_{nullptr};
};

} // namespace piricad::app
