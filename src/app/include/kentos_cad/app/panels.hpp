// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the right-hand panel stack.
//
// Katmanlar and Öznitelikler share a tab group on the right and can be dragged to
// the left edge, floated, or split apart. Both are read-only views onto the
// document; anything they change goes out as a command (CLAUDE.md Article 1).
#pragma once

#include "kentos_cad/core/document.hpp"

#include "kentos_cad/app/theme.hpp"

#include <QEvent>
#include <QIcon>
#include <QPoint>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QWidget>

/// Qt widgets this header only holds pointers to.
class QLabel;
class QLineEdit;
class QMenu;
class QTreeWidget;
class QTreeWidgetItem;

namespace kentos::app {

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
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Which mark the pointer is over, so the row can answer a click.
    enum class Hit { None, Eye, Lock, Row };

    explicit LayerRowDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Where `at` falls inside a row of `width`.
    static Hit hitTest(int x, int width, int depth = 0);

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
    Q_INTERFACES(kentos::app::Themed)

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

    /// Clicks the eye and the lock of the first rows with a REAL mouse event and
    /// prints what the document did, one line each.
    ///
    /// It exists because "katman gizleme gösterme çalışmıyor" was true while every
    /// unit test passed: the handler read columns the refresh never wrote, and the
    /// eye was a picture with nothing behind it. A probe that calls `toggleRow`
    /// directly would have passed too — the defect was in the ROUTE from the
    /// click to the handler, so the probe has to start at the click.
    void probeByHand();

    /// Opens the REAL context menu of the row named `layerName` and fires the
    /// entry whose text is `entry`. Returns false when either is not there.
    ///
    /// It exists for `KENTOS_LAYER_PROBE`, and for the reason `probeByHand`
    /// gives: calling a handler proves the handler works and says nothing about
    /// whether the menu a user opens can reach it. The menu it opens is built by
    /// the same function the right-click builds it with — there is no second
    /// menu for the probe to be right about.
    bool triggerContextEntry(const QString& layerName, const QString& entry);

    /// The texts of the row's context menu, in order, separators as `—`.
    ///
    /// The SHAPE of the menu, not just whether one entry works. It is here
    /// because two entries were taken out of it — `Stili düzenle…` and `Stili
    /// temizle`, both pieces of the Katman Özellikleri window shown as menu items
    /// — and a removal nothing checks is a removal that comes back.
    QStringList contextEntries(const QString& layerName);

    /// Highlights `layer` in the list without sending anything to the bus.
    ///
    /// Called when the CANVAS selection changes: picking a parcel on the map and
    /// then hunting for its layer in a list of forty is work the program can do.
    /// Selection is not document state (model.md R43), so this moves a highlight
    /// and nothing else — in particular it does NOT make the layer active, which
    /// would be an edit nobody asked for.
    void selectLayer(core::LayerId layer);

    /// Asks for a name and runs `KATMAN ad=…` — the header's `+` and the context
    /// menu's `Yeni katman…` are the same road.
    void addLayerInteractively();

    /// Shows the filter box above the list and puts the cursor in it, or hides
    /// it and shows every row again. The header's filter mark.
    void toggleFilter();

signals:
    /// Emitted when the user picks a row, so the property panel can follow.
    void layerSelected(core::LayerId layer);

    /// Emitted when the user asks for a layer's PROPERTIES, so the shell can open
    /// that window. The panel does not own the dialog: a panel that opened a
    /// window would be a panel that has to know what is in it.
    ///
    /// It used to be `styleRequested`, from a `Stili düzenle…` entry, and the
    /// name was the smaller half of the truth: the window it opens has always
    /// been titled `Katman Özellikleri` and has a dozen pages of which the
    /// symbology is one. A context menu that offered the style — and, beside it,
    /// a second entry that cleared the style — was scattering one window's
    /// contents across a menu.
    void propertiesRequested(const QString& layerName);

    /// Emitted when the user asks for a layer's attribute table, for the same
    /// reason and by the same route as `propertiesRequested`.
    ///
    /// ON THE LAYER THE MENU WAS OPENED ON, not on the active one. The table
    /// already knew how to open on a named layer; the only way to reach it was
    /// the Katman menu, which passes whichever layer happens to be ACTIVE — so
    /// looking at the attributes of a layer meant making it active first, which
    /// is an edit nobody asked for (`selectLayer` refuses to do it for the same
    /// reason).
    void attributeTableRequested(const QString& layerName);

protected:
    /// Watches the tree's viewport for a click on the eye or the lock.
    ///
    /// A DELEGATE CANNOT ANSWER A CLICK. It paints, and `LayerRowDelegate` paints
    /// the whole row — eye, chip, name, count, lock — into column 0, which is
    /// exactly why the row has no columns to click. `hitTest` was written for
    /// this and nothing called it: the eye looked like a control and was a
    /// picture, and the only handler read column data the refresh never wrote.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onItemActivated(QTreeWidgetItem* item, int column);

    /// Hides every row whose name does not contain the filter box's text,
    /// Turkish-folded; an empty box shows them all.
    void applyFilter();

    /// Sends the KATMAN call that flips one row's eye or lock.
    void toggleRow(QTreeWidgetItem* item, bool visibility);

    /// Selects every object on `name`, through the bus like everything else.
    void selectAllOn(const QString& name);

    /// The right-click menu. Every entry leaves through the command bus, so a
    /// script can do the same things (Article 1.2).
    void showContextMenu(const QPoint& where);

    /// Builds that menu for one row, owned by the caller. Separate from
    /// `showContextMenu` only so `triggerContextEntry` can open the same one.
    QMenu* buildContextMenu(QTreeWidgetItem* item);

    /// A small preview of what this layer draws, rendered by the CANVAS backend
    /// so the swatch and the map cannot disagree.
    ///
    /// `used` is the style its entities carry, or `kByLayerStyle` when they carry
    /// none — in which case the layer's own default is what it draws.
    QIcon layerIcon(const core::Layer& layer, core::StyleId used) const;

    Controller& controller_;
    QTreeWidget* tree_{nullptr};
    QLineEdit* filter_{nullptr};
    LayerRowDelegate* rows_{nullptr};
    QLabel* footer_{nullptr};
};

} // namespace kentos::app
