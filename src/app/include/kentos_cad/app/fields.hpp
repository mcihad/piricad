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

#include "kentos_cad/app/theme.hpp"

#include <cstdint>
#include <utility>

#include <QString>
#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QSlider;
class QToolButton;

namespace kentos::app {

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

    /// A unit printed inside the box, after the value: `m`, `m²`, `°`.
    QString suffix;
};

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

    FieldSpec spec_;
    bool done_{false}; ///< one edit ends once, whichever way it ends

    QLineEdit* line_{nullptr};      ///< Text, Number, Decimal, Date
    QComboBox* combo_{nullptr};     ///< Combo
    QAbstractButton* yes_{nullptr}; ///< Bool
    QAbstractButton* no_{nullptr};  ///< Bool
    QToolButton* picker_{nullptr};  ///< Date, Colour, MultiSelect
    QSlider* slider_{nullptr};      ///< Range
    QStringList ticked_;            ///< MultiSelect
    QString colour_;                ///< Colour, as `0xAARRGGBB`
    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
