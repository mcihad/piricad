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

class QLineEdit;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// One value cell can carry a small badge — `HESAP` for a derived number,
/// `BOŞ` for a cell nobody has filled. §7 names both.
/// What kind of editor a cell offers when it is opened.
enum class EditKind : std::uint8_t {
    None,   ///< read-only: a derived number, or a fact with no command behind it
    Text,   ///< a line edit — an attribute value, a group path, a width
    Bool,   ///< no editor at all: activating the row flips it
    Colour, ///< the platform colour picker
    Choice, ///< one of `AttributeRow::choices`
};

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

    EditKind edit = EditKind::None;

    /// The values a `Choice` row may take, in the order they are offered.
    QStringList choices;
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

protected:
    /// Painted rather than laid out, for the reason the file header gives: the
    /// `112px | 1fr` grid and the 26 px row are exact numbers, and a layout of
    /// labels reaches them only by accident.
    void paintEvent(QPaintEvent* event) override;

    /// A click on a group bar collapses or opens it; a click on an editable row
    /// selects it, and a double click opens its editor.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

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
    QLineEdit* editor_ = nullptr;
    int editingGroup_  = -1;
    int editingRow_    = -1;

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
