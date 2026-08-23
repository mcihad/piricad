// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the style designer.
//
// WHAT WAS LOOKED AT. QGIS 4.2's `QgsSymbolSelectorDialog` and
// `QgsStyleManagerDialog` were opened and read, not remembered. What they get
// right and what an earlier version of this file missed:
//
//   * a GALLERY of ready-made symbols, searchable, inside the editing dialog —
//     not a separate window you have to know exists;
//   * classification BY GEOMETRY first (Marker / Line / Fill tabs), because
//     somebody looking for a boundary should not scroll past four hundred areas;
//   * a preview drawn on the geometry the symbol is for;
//   * a UNIT beside each size, not one unit for the whole layer;
//   * the symbol layers as a list with an ON/OFF box per layer.
//
// WHAT IS DELIBERATELY NOT COPIED. QGIS splits this across two dialogs and shows
// every property of every layer type at once, greyed. Here it is one window, and
// the property side shows only what the selected layer type actually reads —
// a `dolgu` layer has no marker placement and should not display one.
//
// The machinery is ours for reasons measured rather than preferred: linking
// `libqgis_gui` pulls 254 shared objects and 75 MB, `QgsApplication::initQgis()`
// loads a provider registry and an SRS database into a program budgeted at a
// two-second cold start (Article 7), and the round trip is lossy exactly where
// this project differs on purpose — `QgsRasterFillSymbolLayer` names a FILE while
// a PiriCAD raster fill carries the bytes and their provenance inside the
// document. Licence is not the objection: QGIS is GPL-2.0-or-later.
//
// EVERY EDIT LEAVES AS A COMMAND. The dialog builds a `core::Symbol` and, on
// apply, emits the `STİL` invocations that produce it. There is no path from this
// window to the document that a script cannot take (Article 1.1, 1.2) — which is
// what makes the designer teachable to the AI.
#pragma once

#include "piricad/app/symbol_preview.hpp"
#include "piricad/core/style.hpp"
#include "piricad/core/style_library.hpp"

#include <QDialog>
#include <QString>

#include <vector>

/// The Qt widgets this dialog holds, declared rather than included: a header that
/// pulls in the widget classes it stores pointers to makes every translation unit
/// including it wait for them.
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTabBar;
class QToolButton;
class QTreeWidget;

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
    /// One editable property: its widgets and the layer types that read it.
    ///
    /// The visibility table IS this list. A property that no type declares is a
    /// property nobody can reach, and a type that forgets to declare one loses a
    /// row rather than silently editing something invisible.
    struct Property
    {
        QLabel* label{nullptr};
        QWidget* editor{nullptr};
        QWidget* unit{nullptr}; ///< the unit combo beside it, or null
        std::vector<core::SymbolLayerType> types;
    };

    // ---- building ----
    QWidget* buildGallery();
    QWidget* buildStack();
    QWidget* buildProperties();

    /// Declares one property row and records which layer types show it.
    void addProperty(QFormLayout* form, const QString& label, QWidget* editor, QWidget* unit,
                     std::vector<core::SymbolLayerType> types);

    // ---- the shelf on the left ----
    void refreshGalleryTree();
    void refreshGalleryItems();
    void applyGalleryPick();

    // ---- the symbol being edited ----
    void refresh();
    void loadSelected();
    void applyToSelected();
    int currentLayer() const;

    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayer(int delta);

    void applyToDocument();
    void saveToLibrary();

    /// The geometry the preview and the gallery are showing.
    PreviewShape shape() const;

    Controller& controller_;
    QString layerName_;
    core::Symbol symbol_{};

    /// Guards the property widgets while they are being filled from the model, so
    /// a programmatic `setValue` does not read straight back as a user edit.
    bool loading_{false};

    QTabBar* geometry_{nullptr};
    QLabel* preview_{nullptr};

    QTreeWidget* groups_{nullptr};
    QLineEdit* search_{nullptr};
    QListWidget* gallery_{nullptr};
    QLabel* galleryNote_{nullptr};

    QListWidget* stack_{nullptr};

    QComboBox* type_{nullptr};
    QComboBox* shape_{nullptr};
    QComboBox* placement_{nullptr};
    QComboBox* cap_{nullptr};
    QComboBox* join_{nullptr};
    QToolButton* stroke_{nullptr};
    QToolButton* fill_{nullptr};
    QSpinBox* width_{nullptr};
    QSpinBox* size_{nullptr};
    QSpinBox* interval_{nullptr};
    QSpinBox* spacingY_{nullptr};
    QSpinBox* offset_{nullptr};
    QSpinBox* angle_{nullptr};
    QSpinBox* opacity_{nullptr};
    QComboBox* sizeUnit_{nullptr};
    QComboBox* intervalUnit_{nullptr};
    QComboBox* spacingYUnit_{nullptr};
    QComboBox* offsetUnit_{nullptr};

    std::vector<Property> properties_;
};

} // namespace piricad::app
