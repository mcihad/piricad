// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the style designer.
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
// a KentOSCad raster fill carries the bytes and their provenance inside the
// document. Licence is not the objection: QGIS is GPL-2.0-or-later.
//
// EVERY EDIT LEAVES AS A COMMAND. The dialog builds a `core::Symbol` and, on
// apply, emits the `STİL` invocations that produce it. There is no path from this
// window to the document that a script cannot take (Article 1.1, 1.2) — which is
// what makes the designer teachable to the AI.
#pragma once

#include "kentos_cad/app/theme.hpp"

#include "kentos_cad/app/symbol_preview.hpp"
#include "kentos_cad/core/style.hpp"
#include "kentos_cad/core/style_library.hpp"

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QDialog>
#include <QString>
#include <QVector>

#include <vector>

/// The Qt widgets this dialog holds, declared rather than included: a header that
/// pulls in the widget classes it stores pointers to makes every translation unit
/// including it wait for them.
class QCheckBox;
class QButtonGroup;
class QComboBox;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QSpinBox;
class QTabBar;
class QToolButton;
class QPushButton;
class QTreeWidget;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// Designs one symbol and applies it to a layer through the command bus.
class StyleDesigner : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens the designer on `layerName`, starting from what that layer draws.
    StyleDesigner(Controller& controller, QString layerName, QWidget* parent = nullptr);

    void applyTheme(ThemeMode mode) override;

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

        /// The heading this row sits under (design.md §8: DOLGU, KENAR, GEOMETRİ,
        /// GÖRÜNÜRLÜK), or null for a row that is always shown. A heading is
        /// visible exactly when one of its rows is; an empty heading is a
        /// promise of rows that are not there.
        QLabel* group{nullptr};
        std::vector<core::SymbolLayerType> types;
    };

    // ---- building ----
    QWidget* buildGallery();

    /// design.md §8's renderer row: kind, driving value, and size unit.
    QWidget* buildRendererRow();

    /// The `Bilgi` page: what this layer is, read from the document.
    QWidget* buildInfoPage();

    /// A page with nothing behind it yet: the phase it arrives in and one
    /// line saying what will be on it (§11.8 forbids the present tense).
    QWidget* buildPendingPage(const QString& phase, const QString& note);
    QWidget* buildTree();
    QWidget* buildGlobal();
    QWidget* buildProperties();

    /// Opens one of §8's property groups: a small-caps heading the rows below
    /// belong to. Returns the heading so the rows can name it.
    QLabel* addGroup(QVBoxLayout* form, const QString& title);

    /// Declares one property row, under `group`, and records which layer types
    /// show it.
    void addProperty(QVBoxLayout* form, QLabel* group, const QString& label, QWidget* editor,
                     QWidget* unit, std::vector<core::SymbolLayerType> types);

    // ---- the shelf on the left ----
    void refreshGalleryTree();
    void refreshGalleryItems();

    /// Fills the layer-type box with the types that mean something on the current
    /// geometry, plus `current` so an existing layer can always be read.
    void fillTypeChoices(core::SymbolLayerType current);
    void applyGalleryPick();

    // ---- the symbol being edited ----
    void refresh();
    void loadSelected();
    void applyToSelected();

    /// The stack index the tree has selected, or -1 when the ROOT is selected —
    /// which is a selection, not an absence: the root is where the properties of
    /// the whole symbol live, exactly as it is in QGIS.
    int currentLayer() const;

    /// True when the tree's selection is the symbol itself rather than a layer.
    bool rootSelected() const;

    /// Selects the layer drawn last, which is the tree's first child.
    void selectTopLayer();

    /// Fills the whole-symbol editors from `symbol_`.
    void loadGlobal();

    /// Writes one whole-symbol property across every layer that accepts it.
    void applyGlobal();

    /// Re-reads the tree rows' check boxes and lock buttons into the symbol.
    void syncTreeState();

    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayer(int delta);

    bool applyToDocument();
    void saveToLibrary();

    /// Puts the symbol back to what the layer draws right now.
    void resetToLayer();

    /// Shows where the highlighted gallery entry was published.
    void showProvenance();

    /// The geometry the preview and the gallery are showing.
    PreviewShape shape() const;

    /// Says, beside the preview, what the chosen geometry tab is drawing on.
    void updateHeaderNote();

    /// Redraws the big preview at the width the label currently has.
    /// Sizes the symbol layer stack to the rows it actually holds.
    ///
    /// Bounded on purpose: unbounded, the stack takes the column's height and the
    /// property form under it takes none — which is what the fixed height it
    /// replaces was guarding against, at the cost of an empty box under every
    /// two-layer symbol.
    void fitStackHeight();

    void updatePreview();

    /// Re-renders the preview when its label is resized; see `updatePreview()`.
    bool eventFilter(QObject* watched, QEvent* event) override;

    Controller& controller_;
    QString layerName_;
    core::Symbol symbol_{};

    /// What the layer drew when the dialog opened, for `Sıfırla`.
    core::Symbol original_{};

    /// The catalogue row currently represented by `symbol_`. Keeping this until
    /// the user edits a property lets Apply import the row's embedded raster
    /// assets into the document instead of reducing a QGIS-style symbol to the
    /// handful of scalar command parameters.
    QString galleryCode_;
    QString galleryPackage_;

    /// Guards the property widgets while they are being filled from the model, so
    /// a programmatic `setValue` does not read straight back as a user edit.
    ///
    /// Set through a scope guard that restores the PREVIOUS value rather than
    /// `false`: `refresh()` calls `loadSelected()`, and a nested clear that ended
    /// with a bare `loading_ = false` used to unguard the caller mid-rebuild.
    bool loading_{false};

    /// Guards `refresh()` against being entered from a widget it is rebuilding.
    bool refreshing_{false};

    /// The width the preview was last rendered at, so a resize that does not
    /// change it does not redraw.
    int previewWidth_{0};

    QTabBar* geometry_{nullptr};
    QLabel* preview_{nullptr};

    /// What the header says about the geometry the tabs have chosen.
    QLabel* headerNote_{nullptr};

    QTreeWidget* groups_{nullptr};
    QLineEdit* search_{nullptr};
    QListWidget* gallery_{nullptr};
    QLabel* galleryNote_{nullptr};

    /// Takes the highlighted gösterim into the stack. Disabled while the shelf
    /// has nothing on it to take.
    QPushButton* use_{nullptr};

    /// Where the highlighted gösterim was published. A plan sheet is a legal
    /// document and its symbology has a citation (CLAUDE.md 11.7); showing it
    /// while the user is choosing is cheaper than making them look it up after.
    QLabel* provenance_{nullptr};

    /// The symbol as a TREE: the symbol itself at the root, its layers under it.
    ///
    /// A flat list cannot say what the root says. Selecting the symbol is how a
    /// user reaches the properties that belong to ALL of it — the unit every
    /// measure is read in, the colour every unlocked layer takes, the opacity of
    /// the whole thing — and QGIS puts them there for the same reason.
    QTreeWidget* tree_{nullptr};

    /// The whole-symbol editors, shown when the root is selected.
    QStackedWidget* pages_{nullptr};

    /// design.md 8's left column and the pages it switches. The renderer page
    /// holds everything this window used to be; the rest name their phase.
    SectionList* sections_{nullptr};
    QComboBox* renderKind_{nullptr};
    QComboBox* renderValue_{nullptr};

    /// Keeps the three unit buttons to ONE answer.
    ///
    /// Without it Qt toggles each on its own: clicking the lit button turns it off
    /// and the control shows no unit at all, which reads as a broken segment
    /// rather than as a choice.
    QButtonGroup* unitGroup_{nullptr};

    /// The cell that holds `renderValue_`, so it can be hidden whole.
    ///
    /// Hidden while the renderer is `Tek Sembol`, which is every renderer this
    /// phase ships. A disabled combo with an em dash in it is a control the reader
    /// has to work out, and the answer is already written beside it.
    QWidget* valueCell_{nullptr};
    QVector<QPushButton*> unitButtons_;
    QStackedWidget* pageStack_{nullptr};
    QComboBox* globalUnit_{nullptr};
    QToolButton* globalColour_{nullptr};
    QSpinBox* globalWidth_{nullptr};
    QSpinBox* globalOpacity_{nullptr};
    QLabel* globalUnitNote_{nullptr};

    QComboBox* type_{nullptr};
    QComboBox* shape_{nullptr};
    QComboBox* placement_{nullptr};
    QComboBox* cap_{nullptr};
    QComboBox* join_{nullptr};
    QToolButton* stroke_{nullptr};

    /// The stroke row's label. One button, one row: a `yazi-isaretci` layer's
    /// stroke IS its text colour, so the row is renamed rather than duplicated —
    /// the same widget in two form rows is undefined in Qt and left the two rows
    /// contradicting each other about whether it was visible.
    QLabel* strokeLabel_{nullptr};
    QToolButton* fill_{nullptr};

    /// Keeps this layer's colour when the whole symbol's colour is set.
    QCheckBox* lock_{nullptr};
    QSpinBox* width_{nullptr};
    QSpinBox* size_{nullptr};
    QSpinBox* interval_{nullptr};
    QSpinBox* spacingY_{nullptr};
    QSpinBox* offset_{nullptr};

    /// How far along the line the first marker sits; see `core::SymbolLayer::phase`.
    QSpinBox* phase_{nullptr};
    QSpinBox* angle_{nullptr};
    QSpinBox* opacity_{nullptr};
    QLineEdit* text_{nullptr};
    QComboBox* sizeUnit_{nullptr};
    QComboBox* intervalUnit_{nullptr};
    QComboBox* spacingYUnit_{nullptr};
    QComboBox* offsetUnit_{nullptr};
    QComboBox* phaseUnit_{nullptr};

    std::vector<Property> properties_;
};

} // namespace kentos::app
