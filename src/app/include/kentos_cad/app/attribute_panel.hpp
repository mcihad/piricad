// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the attributes panel, `design.md` §7.
//
// A selected-object card over collapsible groups of key/value rows. The grid is
// `112px | 1fr`: the field name on the left in the interface face, the value on
// the right in mono, because a value is data and data is read in mono.
//
// IT READS THE DOCUMENT AND NEVER WRITES IT. Editing a cell dispatches
// `ÖZNİTELİK` through the bus like any other client (CLAUDE.md 5.9); the panel
// owns no path of its own into the entity store.
#pragma once

#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/core/layer.hpp"

#include <QHash>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <array>
#include <cstdint>

namespace kentos::app {

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

    /// The command line this row writes when it is edited, with `%1` where the
    /// new value goes.
    ///
    /// THIS IS THE WHOLE EDITING MODEL. A cell does not touch the document — it
    /// builds a command and hands it to the bus, so editing `ada_no` from this
    /// panel and typing `ÖZNİTELİK ad=ada_no nesne=1284 deger=17` are the same
    /// write, land in the same journal and undo in one step (CLAUDE.md 1.1, 1.2).
    /// Empty means the row cannot be edited.
    QString command;

    /// The editor this cell opens, from `fields.hpp`.
    ///
    /// IT REPLACED AN `EditKind` ENUM, and the enum was the smaller half of the
    /// question: it said a cell was "text" or "a choice" and had nowhere to put
    /// the bounds of a number, the digits of a decimal or the day format of a
    /// date. A column declared as `tarih` was typed as free text and found out
    /// it was wrong only when the command refused it.
    ///
    /// Read-only is still the empty `command`, not a kind: a row with nothing to
    /// send has nothing to edit, and saying so twice invites the two to disagree.
    FieldSpec field;
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
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the panel over a controller, which outlives it.
    explicit AttributePanel(Controller& controller, QWidget* parent = nullptr);

    /// Re-reads the selection and rebuilds the card and the groups.
    void refresh();

private:
    /// Commits the value and opens the NEXT editable row.
    ///
    /// WHY ENTER MOVES ON. A panel of attributes is filled the way a ledger is:
    /// type, Enter, type, Enter. Stopping after each value makes the user reach
    /// for the mouse between every two fields, which on a parcel with six
    /// attributes is five reaches nobody asked for.
    ///
    /// Separate from `commitEdit` because that one is also what a Bool toggle and
    /// the probe call, and neither of those is a person working down a list.
    void commitAndAdvance(const QString& value);

    /// The row after (`group`, `index`) that can be edited, or false when the
    /// list ends. Collapsed groups are OPENED on the way past, because a row the
    /// user cannot see is not a row they were about to fill in.
    bool nextEditable(int group, int index, int& outGroup, int& outIndex);

    /// Builds `groups_` for whatever is selected. Called only by `refresh()`,
    /// which then restores what the user had opened.
    void rebuild();

public:
    /// Shows one LAYER's properties instead of the selection's. `kNoLayer`
    /// returns the panel to whatever is selected.
    ///
    /// design.md 7 gives the shell ONE property surface, so a layer picked in the
    /// Katmanlar panel and an entity picked on the canvas both land here rather
    /// than in two panels that can disagree about what is being looked at.
    void setLayer(core::LayerId layer);

    void applyTheme(ThemeMode mode) override;

    /// Edits the row whose key is `key` to `value`, the way a user would, for
    /// `KENTOS_EDIT_PROBE`. Returns false when no such editable row is shown.
    ///
    /// Exists because the panel is painted rather than laid out, so there is no
    /// child widget a test could find and drive — and a panel nothing exercises
    /// is a panel whose editing path breaks silently.
    bool editRowForProbe(const QString& key, const QString& value);

    /// Opens the editor on the row named `key` and LEAVES it open, so a probe can
    /// photograph what a user is looking at while they type.
    ///
    /// `editRowForProbe` above commits without ever building the editor, which is
    /// the right shape for checking that the command goes out and the wrong one
    /// for checking that the cell is readable while it is open — the row's stored
    /// value used to be painted underneath the box, and no transcript could have
    /// shown that.
    bool openRowForProbe(const QString& key);

    /// Every row key the panel is currently showing, for the same probe.
    QStringList probeRowKeys() const;

signals:
    /// The user asked for the shown object's corners as a file. The shell opens
    /// the export window; the panel does not own it.
    void exportCoordinatesRequested();

    /// The user asked for the attribute table of `layer`.
    void tableRequested(const QString& layer);

    /// The user asked for the layer properties window of `layer`.
    void propertiesRequested(const QString& layer);

protected:
    /// Painted rather than laid out, for the reason the file header gives: the
    /// `112px | 1fr` grid and the 26 px row are exact numbers, and a layout of
    /// labels reaches them only by accident.
    void paintEvent(QPaintEvent* event) override;

    /// A click on a group bar collapses or opens it; a click on an editable row
    /// selects it, and a double click opens its editor.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// The object's menu: export its corners, copy them, open its layer's table
    /// or properties. Offered while ONE object is shown, wherever on the panel
    /// the click lands — and from the keyboard through the Menu key, which Qt
    /// routes here too (ui.md R21).
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// Up and down walk the rows, Enter and F2 open the editor, Space flips a
    /// boolean. ui.md R21: every operation is reachable from the keyboard.
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// Lays the card and the groups out and returns the total height, so the
    /// same walk serves painting, hit testing and the scroll range.
    ///
    /// `rowBands`, when given, collects one `(top, group, index)` triple per
    /// drawn row, so a click can find the row under it by the same walk that
    /// painted it — a second layout for hit testing is how a panel starts
    /// editing the row above the one the user clicked.
    int layout(QVector<QPair<int, int>>* headerBands,
               QVector<std::array<int, 3>>* rowBands = nullptr) const;

    /// Opens the editor for the row at `group`,`index`, or flips it when it is a
    /// boolean. Does nothing for a row with no command behind it.
    void beginEdit(int group, int index);

    /// Sends the row's command with `value` substituted, and closes the editor.
    void commitEdit(const QString& value);

    void closeEditor();

    /// Where the value half of one row sits, in widget coordinates, so the editor
    /// can be placed exactly over it.
    QRect rowRect(int group, int index) const;

    /// The row the keyboard is on, or -1. Painted with the selected-row pattern
    /// design.md §2 fixes for every list in the program.
    int hotRowGroup_ = -1;
    int hotRow_      = -1;

    /// The line edit, created on first use, parented here.
    Field* editor_    = nullptr;
    int editingGroup_ = -1;
    int editingRow_   = -1;

    Controller& controller_;
    QVector<AttributeGroup> groups_;

    /// Which groups the user has opened or closed, by heading.
    ///
    /// `refresh()` rebuilds every group from scratch on each selection change, so
    /// without this a user who opened GEOMETRİ to read an area would find it shut
    /// again the moment they picked the next parcel — which is the one moment
    /// they wanted it open. Keyed by the heading rather than by index because the
    /// GROUPS THEMSELVES CHANGE: a document, a layer, one object and many objects
    /// each build a different set.
    QHash<QString, bool> disclosed_;
    QString title_;    ///< `Parsel 1284 / 21`
    QString subtitle_; ///< `POLYGON · fid 4128 · 4 köşe`
    int glyph_           = 0;
    core::LayerId layer_ = core::kNoLayer;
    int scroll_          = 0;
    int hotGroup_        = -1;
    ThemeMode theme_     = ThemeMode::Dark;
};

} // namespace kentos::app
