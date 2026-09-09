// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the inline editors, and the only ones.
//
// WHY THIS FILE EXISTS. The object inspector opened a bare `QLineEdit` over the
// cell being edited. Two things were wrong with that and both were visible:
// the box wore Qt's own frame and padding instead of the shell's, so it read as
// a widget bolted onto the panel rather than the cell turning editable; and it
// was a line edit whatever the column held, so a date, a yes/no and a code from
// a catalogue were all typed as free text and checked, if at all, by the command
// that received them.
//
// So an editor here is TWO promises. It fills the rectangle it is given exactly
// — no frame of its own, no margin, the same type and the same alignment the
// cell was painted with, so the only change on screen is that the value is now
// selectable. And it knows what it is editing: a `Number` refuses letters, a
// `Date` offers a calendar, a `Combo` offers the values the schema allows.
//
// THE KEYS ARE THE SAME EVERYWHERE, which is the other half of why there is one
// file rather than an editor per panel:
//
//   Enter      — take the value and leave editing
//   Esc        — leave editing and change nothing
//   focus out  — take the value, exactly as Enter does
//
// The last one is the one people notice. A user who types a number and then
// clicks the next row has finished editing; a program that discards the typing
// because they did not press Enter has thrown away work they can see they did.
//
// NOTHING HERE TOUCHES THE DOCUMENT. A field emits the text it ended up with;
// the panel that owns it turns that into a command (CLAUDE.md 1.1, 5.9).
#pragma once

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/core/attribute.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include <QDate>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QWidget>

class QAbstractButton;
class QLabel;
class QLineEdit;
class QSlider;
class QToolButton;

namespace kentos::app {

/// The component set; see widgets.hpp.
class ComboBox;

/// What a cell offers when it is opened.
///
/// One per shape of value, not one per attribute type: `Number` serves an ada
/// number and a floor count, `Decimal` serves a TAKS and a slope. The mapping
/// from `core::AttrType` to one of these lives where the schema is read.
enum class FieldKind : std::uint8_t {
    Text,        ///< free text
    Number,      ///< a whole number, optionally bounded
    Decimal,     ///< a fixed-point number with a declared number of digits
    Bool,        ///< evet / hayır, as a two-state segment rather than a tick
    Combo,       ///< exactly one of a declared list
    MultiSelect, ///< any number of a declared list, joined by `, `
    Date,        ///< a calendar day, `YYYY-AA-GG`, with a picker
    Range,       ///< a bounded number, dragged rather than typed
    Colour,      ///< `0xAARRGGBB`, with the platform picker
};

/// What a value IS RIGHT NOW, from `bileşen_standardı.png`'s seven states —
/// minus focus and disabled, which the widget system already carries.
///
/// Each says a DIFFERENT thing, and the two easiest to confuse are the ones
/// worth stating: `Changed` (warn) means "you edited this and it is not saved",
/// `Invalid` (danger) means "this value is not acceptable". A field that used one
/// colour for both would tell a user their own edit was wrong.
enum class FieldState : std::uint8_t {
    Normal,   ///< the value is what it was
    Changed,  ///< edited and not yet written — warn
    Invalid,  ///< not acceptable — danger
    Derived,  ///< computed by the program; editing it is meaningless — accent, `fx`
    ReadOnly, ///< can be read and not written — dim, no ground
};

/// How an editor is framed, which is a question about WHERE it is, not what it
/// edits.
///
/// The same `Field` serves two places and they want opposite things. In a FORM —
/// the column dialog, a settings row — it is one control among several and has
/// to look like the shell's other inputs: a ground, a border, a radius, the
/// accent on focus. In a CELL it is replacing a value that was already painted
/// there, and a box drawn around it would be a widget appearing on top of the
/// panel rather than the row becoming editable.
///
/// Getting this wrong is visible either way round: a form of borderless fields
/// reads as text floating on a dialog, and a bordered box in a table row reads
/// as something bolted on.
enum class FieldFrame : std::uint8_t {
    Box,  ///< a form control: ground, border, radius, accent on focus
    Cell, ///< fills the cell it replaces; an accent ring says it is open
};

/// How one editor is configured. Everything is optional and the defaults are the
/// unconstrained case, so a plain text cell needs `{}`.
struct FieldSpec
{
    /// Which editor opens. The rest of this struct configures that one.
    FieldKind kind{FieldKind::Text};

    /// Shown in an empty box. Says what the cell WANTS, never what it holds.
    QString placeholder;

    /// The values a `Combo` or a `MultiSelect` offers, in the order declared.
    QStringList choices;

    /// Bounds for `Number`, `Decimal` and `Range`. `Range` needs both; the other
    /// two treat an empty span as unbounded.
    long long minimum{0};
    long long maximum{0};
    bool bounded{false};

    /// Digits after the point, for `Decimal`. Zero makes it a whole number in a
    /// decimal's clothing, which is why `SÜTUN` defaults it to two.
    int decimals{0};

    /// A unit printed inside the box, after the value, dim and in mono: `m`,
    /// `m²`, `°`. The standard draws it at the right edge, and a length with
    /// its unit beside it is a length nobody has to guess the unit of.
    QString suffix;

    /// A mark at the left edge, before the value: a ruler on a length, a lock
    /// on a read-only code. `Derived` supplies its own `fx` when this is empty.
    std::optional<Glyph> glyph;

    /// Whether this editor is a form control or a cell. Defaults to the form,
    /// because that is the one that looks wrong when it is silently omitted.
    FieldFrame frame{FieldFrame::Box};
};

/// The same spec, framed as a table cell rather than as a form control.
inline FieldSpec as_cell(FieldSpec spec)
{
    spec.frame = FieldFrame::Cell;
    return spec;
}

/// A spec that only names its kind — the common case, and the one the compiler
/// otherwise makes noisy: an aggregate written `{FieldKind::Bool}` leaves four
/// members unnamed and `-Wmissing-field-initializers` says so at every call.
inline FieldSpec field_of(FieldKind kind)
{
    FieldSpec spec;
    spec.kind = kind;
    return spec;
}

/// A one-of-these picker over `choices`.
inline FieldSpec combo_of(QStringList choices)
{
    FieldSpec spec;
    spec.kind    = FieldKind::Combo;
    spec.choices = std::move(choices);
    return spec;
}

/// A bounded whole number, with an optional unit printed after it.
inline FieldSpec number_of(long long low, long long high, QString suffix = {})
{
    FieldSpec spec;
    spec.kind    = FieldKind::Number;
    spec.minimum = low;
    spec.maximum = high;
    spec.bounded = true;
    spec.suffix  = std::move(suffix);
    return spec;
}

/// A fixed-point number carrying `decimals` digits after the point.
inline FieldSpec decimal_of(int decimals)
{
    FieldSpec spec;
    spec.kind     = FieldKind::Decimal;
    spec.decimals = decimals;
    return spec;
}

/// The calendar a `Date` field opens.
///
/// DRAWN, NOT ASSEMBLED. `QCalendarWidget` is a `QTableView` with a navigation
/// bar of tool buttons and spin boxes bolted to the top of it, and it arrives
/// wearing whatever the platform style gives it: month and year in two little
/// combos, arrows sized for a desktop from 2005, a grid with its own header and
/// its own selection colours. Styling it means reaching into four private
/// subwidgets and losing to at least one of them; the result is the "extra
/// component dropped on the window" look this program spent a whole component
/// set getting rid of.
///
/// It is thirty lines of painting to do it properly, and then the card matches
/// the tool flyout because it is the same card: rounded, shadowed, the shell's
/// own tokens, its own type.
///
/// TURKISH FROM `QLocale`, never from a table here and never from `<cctype>`:
/// month and day names come from `QLocale(QLocale::Turkish)`, which is the rule
/// for every piece of Turkish text this program shows (CLAUDE.md 5.6).
class DatePopup : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the card. Sized once, from the metrics in `fields.cpp`: six week
    /// rows always, so the card never changes height as the months go by.
    explicit DatePopup(QWidget* parent = nullptr);

    /// Opens the card under `anchor`, showing the month `on` falls in.
    void reveal(const QDate& on, const QPoint& anchor);

    void applyTheme(ThemeMode mode) override;

signals:
    /// A day was chosen. The card has already closed.
    void picked(const QDate& day);

    /// The user asked for the cell to be emptied.
    void cleared();

protected:
    /// Draws the shadow, the card, the month row, the weekday row, the grid and
    /// the two footer buttons.
    void paintEvent(QPaintEvent* event) override;

    /// Tracks the day under the pointer.
    void mouseMoveEvent(QMouseEvent* event) override;

    /// Steps a month or a year, picks a day, or dismisses the card.
    void mousePressEvent(QMouseEvent* event) override;

    /// Arrows walk by day, PgUp/PgDn by month, Home is today, Enter picks,
    /// Delete empties the cell, Esc closes. A calendar is a grid and somebody
    /// entering a hundred approval dates should never reach for the mouse to
    /// move one day.
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// The day under `where`, or an invalid date when the point is off the grid.
    QDate dayAt(const QPoint& where) const;

    /// The rectangle of the header button `which`: 0 and 1 step the month back
    /// and on, 2 and 3 the year.
    QRect arrowRect(int which) const;

    /// The two footer buttons: 0 is `Bugün`, 1 is `Temizle`.
    QRect footRect(int which) const;

    /// The first cell of the grid: the Monday on or before the first of `shown_`.
    QDate gridStart() const;

    QDate shown_;     ///< any day in the month being displayed
    QDate chosen_;    ///< what the field held when the card opened
    QDate cursor_;    ///< where the keyboard is
    int hover_{-1};   ///< the cell under the pointer, or -1
    int pressed_{-1}; ///< the header or footer button under the pointer, or -1
    ThemeMode theme_{ThemeMode::Dark};
};

/// The editor a declared attribute column asks for.
///
/// ONE MAPPING, not one per panel. The object inspector and the attribute table
/// both open editors over the same columns, and two copies of "a `tarih` gets a
/// calendar" is how one of them ends up offering a line edit for a date long
/// after the other stopped.
FieldSpec field_for(const core::AttrSpec& column);

/// The Qt item delegate that puts a `Field` in a table cell.
///
/// WHY THE TABLE NEEDS ONE. `QStyledItemDelegate` opens Qt's own editors — a
/// `QLineEdit` for everything, a `QSpinBox` with arrows nobody can hit at row
/// height — which is the same "bolted-on widget" the inspector had before
/// `Field` existed, and the same wrong editor for a date or a yes/no.
///
/// ENTER MOVES ON, which is the other half. A table of attributes is filled in
/// the way a ledger is filled in: type, Enter, type, Enter. The delegate reports
/// each finished cell through `advanced` and the window decides where the next
/// one is — the delegate knows about editors, not about the shape of the grid.
class FieldDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// `specs` answers "what does column N hold"; the delegate calls it per cell
    /// rather than caching, because a column can be declared while the table is
    /// open.
    using SpecFor = std::function<FieldSpec(int column)>;

    FieldDelegate(SpecFor specs, QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    /// Does nothing, ON PURPOSE, and this is load-bearing.
    ///
    /// Qt installs the delegate as an event filter on every editor it creates,
    /// and `QAbstractItemDelegate::eventFilter` then handles Enter, Esc and focus
    /// leaving itself — commit, close, move. A `Field` handles all three too, and
    /// it must: it is a COMPOSITE, so the key press lands on its inner line edit
    /// where Qt's filter, installed on the outer widget, never sees it.
    ///
    /// Two committers is not twice as good. Qt closed the editor on focus-out
    /// while the field was still emitting for the same edit, so `commitData` and
    /// `closeEditor` arrived for an editor the view had already released —
    /// "editor that does not belong to this view", and then a crash, because the
    /// object was destroyed with its own event handler still on the stack.
    ///
    /// So the default handling is switched off and the field's is the only one.
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// The palette the editors are themed with. Set by the window that owns the
    /// table, and remembered so editors built later match.
    void setTheme(ThemeMode mode) { theme_ = mode; }

signals:
    /// One cell was finished with Enter. The window moves to the next one.
    void advanced(const QModelIndex& from);

private:
    SpecFor specs_;
    ThemeMode theme_{ThemeMode::Dark};
};

/// One inline editor. Built for a cell, sized by the cell, gone when it commits.
class Field : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the editor `spec` describes. The widget is frameless and expects to
    /// be given a geometry by its owner rather than laid out.
    explicit Field(const FieldSpec& spec, QWidget* parent = nullptr);

    /// What the editor currently holds, as the text a command takes.
    QString value() const;

    /// Puts `text` in without emitting anything. An empty string means "no value
    /// yet", which is not the same as the string `—` the cell paints for one.
    void setValue(const QString& text);

    /// Focuses the editor and selects what is in it, so typing replaces.
    void beginEditing();

    /// Puts the field in one of the standard's states; see `FieldState`. The
    /// stylesheet reads it as the `state` property on the frame and the line.
    void setState(FieldState state);

    FieldState state() const noexcept { return state_; }

    /// The unit at the right edge, or empty for none.
    void setUnit(const QString& unit);

    /// The kind this field was built for, so an owner can decide whether the
    /// editor it has is the editor it now needs.
    FieldKind kind() const noexcept { return spec_.kind; }

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user finished: Enter, a choice from a list, or focus leaving.
    void committed(const QString& value);

    /// The user backed out with Esc. The owner puts nothing anywhere.
    void cancelled();

protected:
    /// Watches the inner widget for Enter, Esc and focus leaving, so every kind
    /// obeys the same three keys without each one re-implementing them.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Emits `committed` once, and never again for the same edit.
    void commit();

    /// Emits `cancelled` once.
    void cancel();

    /// Fills `line_` with the validator the kind and the bounds imply.
    void applyValidator();

    /// Rewrites the button face of a `MultiSelect` from the ticked entries.
    void refreshMultiFace();

    /// Repaints the leading mark in the ink the state calls for.
    void refreshLead();

    FieldSpec spec_;
    bool done_{false}; ///< one edit ends once, whichever way it ends

    QLineEdit* line_{nullptr};         ///< Text, Number, Decimal, Date
    ComboBox* combo_{nullptr};         ///< Combo
    QAbstractButton* yes_{nullptr};    ///< Bool, in a cell: the `evet` half
    QAbstractButton* toggle_{nullptr}; ///< Bool, in a form: the switch
    QLabel* toggleWord_{nullptr};      ///< the word beside the switch
    QAbstractButton* no_{nullptr};     ///< Bool
    QToolButton* picker_{nullptr};     ///< Date, Colour, MultiSelect
    DatePopup* calendar_{nullptr};     ///< Date, built on the first press
    QLabel* lead_{nullptr};            ///< the mark at the left edge, when there is one
    QLabel* unit_{nullptr};            ///< the unit at the right edge, when there is one
    FieldState state_{FieldState::Normal};
    QSlider* slider_{nullptr}; ///< Range
    QStringList ticked_;       ///< MultiSelect
    QString colour_;           ///< Colour, as `0xAARRGGBB`
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
