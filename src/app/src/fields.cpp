// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/fields.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <QApplication>
#include <QCalendarWidget>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>

namespace kentos::app {
namespace {

/// The picker on the right of a date, a colour or a multi-select: square, the
/// height of the box, and no wider than it is tall.
constexpr int kPickerWidth = 22;

/// How much of a `Range` row the readout takes, leaving the rest to the slider.
constexpr int kReadoutWidth = 54;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// A regular expression that accepts a fixed-point number with at most
/// `decimals` digits after either separator, and a leading sign.
///
/// BOTH SEPARATORS, because a user typing `0,40` into a Turkish panel and a
/// script writing `0.40` mean the same number — the same bargain
/// `core::decimal_from_text` makes on the other side of the command.
QString decimalPattern(int decimals)
{
    if (decimals <= 0) return QStringLiteral("^[+-]?\\d*$");
    return QStringLiteral("^[+-]?\\d*(?:[.,]\\d{0,%1})?$").arg(decimals);
}

} // namespace

Field::Field(const FieldSpec& spec, QWidget* parent) : QWidget(parent), spec_(spec)
{
    setObjectName(QStringLiteral("field"));

    // WITHOUT THIS THE STYLESHEET PAINTS NOTHING. A plain `QWidget` subclass
    // ignores `background` and `border` from a sheet unless it is told to draw
    // its own styled background — which is why the first cut of this file looked
    // like it had no rule at all rather than like it had the wrong one.
    setAttribute(Qt::WA_StyledBackground, true);

    // THE FRAME RIDES AS A PROPERTY, so one rule set in `theme.cpp` can answer
    // both shapes with an attribute selector instead of two object names that
    // would have to be kept in step. Set before anything is built, because a
    // property read by the stylesheet has to be there when the child is polished.
    setProperty("frame",
                spec_.frame == FieldFrame::Cell ? QStringLiteral("cell") : QStringLiteral("box"));

    // NO MARGIN AND NO SPACING. The field IS the cell: its owner hands it the
    // rectangle the value was painted in, and anything this layout added would
    // show up as the box being a size the row is not.
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    const auto frameOf = [this](QWidget* child) {
        child->setProperty("frame", property("frame"));
        return child;
    };

    const auto plainLine = [this, row, frameOf] {
        line_ = new QLineEdit(this);
        line_->setObjectName(QStringLiteral("fieldLine"));
        frameOf(line_);
        line_->setFrame(false);
        line_->setPlaceholderText(spec_.placeholder);
        line_->installEventFilter(this);
        row->addWidget(line_, 1);
        return line_;
    };

    const auto squareButton = [this, row, frameOf](const QString& glyph) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("fieldPicker"));
        frameOf(button);
        button->setText(glyph);
        button->setFixedWidth(kPickerWidth);
        button->setFocusPolicy(Qt::NoFocus);
        button->installEventFilter(this);
        row->addWidget(button, 0);
        return button;
    };

    switch (spec_.kind) {
    case FieldKind::Text:
    case FieldKind::Number:
    case FieldKind::Decimal:
        plainLine();
        applyValidator();
        break;

    case FieldKind::Date:
        plainLine();
        line_->setPlaceholderText(spec_.placeholder.isEmpty() ? tr("YYYY-AA-GG")
                                                              : spec_.placeholder);
        // A CALENDAR, NOT A SPIN BOX. `QDateEdit` splits the day into three
        // little fields with their own arrows, which at row height is three
        // targets nobody can hit; a typed ISO date plus a calendar for the
        // people who would rather look at a month is the pair that works.
        picker_ = squareButton(QStringLiteral("▾"));
        connect(picker_, &QToolButton::clicked, this, [this] {
            auto* menu     = new QMenu(this);
            auto* calendar = new QCalendarWidget(menu);
            calendar->setGridVisible(false);
            calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

            const QDate current = QDate::fromString(line_->text(), Qt::ISODate);
            calendar->setSelectedDate(current.isValid() ? current : QDate::currentDate());

            auto* holder = new QWidgetAction(menu);
            holder->setDefaultWidget(calendar);
            menu->addAction(holder);

            connect(calendar, &QCalendarWidget::clicked, this, [this, menu](const QDate& picked) {
                line_->setText(picked.toString(Qt::ISODate));
                menu->close();
                commit();
            });
            menu->exec(mapToGlobal(QPoint(0, height())));
            menu->deleteLater();
        });
        break;

    case FieldKind::Bool: {
        // A SEGMENT, NOT A TICK. A check box in a cell is a 13 px target with a
        // label somewhere else; two words that light up say what they mean at
        // row height and read the same as the value the cell was painting.
        const auto segment = [this, row, frameOf](const QString& text) {
            auto* button = new QPushButton(text, this);
            button->setObjectName(QStringLiteral("fieldSegment"));
            frameOf(button);
            button->setCheckable(true);
            button->setFocusPolicy(Qt::StrongFocus);
            button->installEventFilter(this);
            row->addWidget(button, 1);
            return button;
        };
        yes_ = segment(tr("evet"));
        no_  = segment(tr("hayır"));
        connect(yes_, &QAbstractButton::clicked, this, [this] {
            yes_->setChecked(true);
            no_->setChecked(false);
            commit();
        });
        connect(no_, &QAbstractButton::clicked, this, [this] {
            no_->setChecked(true);
            yes_->setChecked(false);
            commit();
        });
        break;
    }

    case FieldKind::Combo:
        combo_ = new QComboBox(this);
        combo_->setObjectName(QStringLiteral("fieldCombo"));
        frameOf(combo_);
        combo_->addItems(spec_.choices);
        combo_->installEventFilter(this);
        row->addWidget(combo_, 1);

        // COMMITTED ON ACTIVATION, not on every index change. `activated` fires
        // when a PERSON picks; `currentIndexChanged` also fires when the box is
        // filled, which would commit the first entry the moment the editor opened.
        connect(combo_, &QComboBox::activated, this, [this](int) { commit(); });
        break;

    case FieldKind::MultiSelect:
        picker_ = new QToolButton(this);
        picker_->setObjectName(QStringLiteral("fieldMulti"));
        frameOf(picker_);
        picker_->setToolButtonStyle(Qt::ToolButtonTextOnly);
        picker_->setPopupMode(QToolButton::InstantPopup);
        picker_->installEventFilter(this);
        row->addWidget(picker_, 1);
        {
            auto* menu = new QMenu(picker_);
            for (const QString& choice : spec_.choices) {
                QAction* entry = menu->addAction(choice);
                entry->setCheckable(true);
                connect(entry, &QAction::toggled, this, [this, choice](bool on) {
                    if (on && !ticked_.contains(choice))
                        ticked_ << choice;
                    else if (!on)
                        ticked_.removeAll(choice);
                    refreshMultiFace();
                });
            }
            picker_->setMenu(menu);
        }
        refreshMultiFace();
        break;

    case FieldKind::Range: {
        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setObjectName(QStringLiteral("fieldSlider"));
        slider_->setRange(static_cast<int>(spec_.minimum), static_cast<int>(spec_.maximum));
        slider_->installEventFilter(this);
        row->addWidget(slider_, 1);

        // THE NUMBER, BESIDE THE SLIDER AND EDITABLE. A slider alone cannot be
        // given an exact value, and an exact value is what a regulation carries;
        // a slider alone is a control for a preference, not for a figure.
        line_ = new QLineEdit(this);
        line_->setObjectName(QStringLiteral("fieldLine"));
        frameOf(line_);
        line_->setFrame(false);
        line_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        line_->setFixedWidth(kReadoutWidth);
        line_->installEventFilter(this);
        row->addWidget(line_, 0);
        applyValidator();

        connect(slider_, &QSlider::valueChanged, this,
                [this](int now) { line_->setText(QString::number(now)); });
        connect(line_, &QLineEdit::textEdited, this, [this](const QString& typed) {
            QSignalBlocker block(slider_);
            slider_->setValue(typed.toInt());
        });
        break;
    }

    case FieldKind::Colour:
        plainLine();
        line_->setReadOnly(true);
        picker_ = squareButton(QString());
        connect(picker_, &QToolButton::clicked, this, [this] {
            const QColor before = QColor::fromRgba(colour_.mid(2).toUInt(nullptr, 16));
            const QColor picked =
                QColorDialog::getColor(before, this, tr("Renk"), QColorDialog::ShowAlphaChannel);
            if (!picked.isValid()) {
                cancel();
                return;
            }
            colour_ = QStringLiteral("0x%1").arg(picked.rgba(), 8, 16, QLatin1Char('0')).toUpper();
            line_->setText(colour_);
            commit();
        });
        break;
    }

    installEventFilter(this);
}

void Field::applyValidator()
{
    if (line_ == nullptr) return;

    if (spec_.kind == FieldKind::Number || spec_.kind == FieldKind::Range) {
        auto* whole = spec_.bounded ? new QIntValidator(static_cast<int>(spec_.minimum),
                                                        static_cast<int>(spec_.maximum), line_)
                                    : new QIntValidator(line_);
        line_->setValidator(whole);
        return;
    }
    if (spec_.kind == FieldKind::Decimal) {
        // A REGULAR EXPRESSION, not `QDoubleValidator`. The double validator
        // works in binary floating point and follows the LOCALE for its
        // separator, and this program stores fixed point and accepts both
        // separators on purpose — see `core::decimal_from_text`.
        line_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(decimalPattern(spec_.decimals)), line_));
    }
}

void Field::refreshMultiFace()
{
    if (picker_ == nullptr) return;

    // WHAT IS TICKED, THEN HOW MANY MORE. A button that listed every choice would
    // be a button wider than the panel; one that only said "3 seçili" would make
    // the user open it to find out which three.
    if (ticked_.isEmpty()) {
        picker_->setText(spec_.placeholder.isEmpty() ? tr("seçin…") : spec_.placeholder);
        return;
    }
    const QString head = ticked_.front();
    picker_->setText(ticked_.size() == 1 ? head : tr("%1  +%2").arg(head).arg(ticked_.size() - 1));
}

QString Field::value() const
{
    switch (spec_.kind) {
    case FieldKind::Bool: return (yes_ != nullptr && yes_->isChecked()) ? tr("evet") : tr("hayır");
    case FieldKind::Combo: return combo_ != nullptr ? combo_->currentText() : QString();
    case FieldKind::MultiSelect: return ticked_.join(QStringLiteral(", "));
    case FieldKind::Colour: return colour_;
    default: break;
    }
    return line_ != nullptr ? line_->text() : QString();
}

void Field::setValue(const QString& text)
{
    switch (spec_.kind) {
    case FieldKind::Bool:
        if (yes_ != nullptr && no_ != nullptr) {
            const bool on =
                text.compare(tr("evet"), Qt::CaseInsensitive) == 0 || text == QLatin1String("1");
            yes_->setChecked(on);
            no_->setChecked(!on);
        }
        return;

    case FieldKind::Combo:
        if (combo_ != nullptr) {
            const int at = combo_->findText(text);
            combo_->setCurrentIndex(at >= 0 ? at : 0);
        }
        return;

    case FieldKind::MultiSelect: {
        ticked_.clear();
        for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts))
            ticked_ << part.trimmed();
        if (picker_ != nullptr && picker_->menu() != nullptr)
            for (QAction* entry : picker_->menu()->actions()) {
                QSignalBlocker block(entry);
                entry->setChecked(ticked_.contains(entry->text()));
            }
        refreshMultiFace();
        return;
    }

    case FieldKind::Colour:
        colour_ = text;
        if (line_ != nullptr) line_->setText(text);
        update();
        return;

    case FieldKind::Range:
        if (slider_ != nullptr) {
            QSignalBlocker block(slider_);
            slider_->setValue(text.toInt());
        }
        if (line_ != nullptr) line_->setText(text);
        return;

    default: break;
    }
    if (line_ != nullptr) line_->setText(text);
}

void Field::beginEditing()
{
    done_ = false;

    if (line_ != nullptr && !line_->isReadOnly()) {
        line_->setFocus(Qt::OtherFocusReason);
        line_->selectAll();
        return;
    }
    if (combo_ != nullptr) {
        combo_->setFocus(Qt::OtherFocusReason);
        combo_->showPopup();
        return;
    }
    if (yes_ != nullptr) {
        (yes_->isChecked() ? yes_ : no_)->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (picker_ != nullptr) picker_->setFocus(Qt::OtherFocusReason);
}

void Field::commit()
{
    if (done_) return;
    done_ = true;
    emit committed(value());
}

void Field::cancel()
{
    if (done_) return;
    done_ = true;
    emit cancelled();
}

bool Field::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            commit();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            cancel();
            return true;
        }
    }

    // FOCUS LEAVING IS ENTER, and this is the rule people notice. Someone who
    // types a value and then clicks the next row has finished; discarding it
    // because they did not press Enter throws away work they can see they did.
    //
    // EXCEPT WHEN A POPUP TOOK IT. A combo's list, a calendar and a colour
    // dialog all take the focus off the editor while the edit is still going on,
    // and committing there would close the editor under the very list the user
    // just opened.
    // The focus ring on a FORM field lives on the container, because the border
    // does: Qt's stylesheets have no `:focus-within`, so the state is carried as
    // a property and repolished by hand.
    if (event->type() == QEvent::FocusIn && spec_.frame == FieldFrame::Box) {
        setProperty("state", QStringLiteral("focus"));
        style()->unpolish(this);
        style()->polish(this);
    }

    if (event->type() == QEvent::FocusOut) {
        auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() == Qt::PopupFocusReason ||
            focus->reason() == Qt::ActiveWindowFocusReason)
            return QWidget::eventFilter(watched, event);

        // Focus moving BETWEEN this field's own widgets — the two halves of a
        // bool, the slider and its readout — is not leaving the field.
        QWidget* next = QApplication::focusWidget();
        if (next != nullptr && (next == this || isAncestorOf(next)))
            return QWidget::eventFilter(watched, event);

        if (spec_.frame == FieldFrame::Box) {
            setProperty("state", QVariant());
            style()->unpolish(this);
            style()->polish(this);
        }
        commit();
    }
    return QWidget::eventFilter(watched, event);
}

void Field::applyTheme(ThemeMode mode)
{
    theme_ = mode;

    // The colour picker paints the value it holds, so it has to be repainted
    // when the tokens change — its border comes from them.
    if (spec_.kind == FieldKind::Colour && picker_ != nullptr) {
        const QColor shown = QColor::fromRgba(colour_.mid(2).toUInt(nullptr, 16));
        QPixmap chip(kPickerWidth - 8, kPickerWidth - 8);
        chip.fill(shown.isValid() ? shown : tokensOf(mode).bgInput);
        picker_->setIcon(QIcon(chip));
    }
    update();
}

// =============================================================================
// field_for
// =============================================================================

FieldSpec field_for(const core::AttrSpec& column)
{
    switch (column.type) {
    case core::AttrType::Bool: return field_of(FieldKind::Bool);

    case core::AttrType::Date: {
        FieldSpec spec   = field_of(FieldKind::Date);
        spec.placeholder = QStringLiteral("YYYY-AA-GG");
        return spec;
    }

    case core::AttrType::Decimal: return decimal_of(column.scale);

    case core::AttrType::Int64: return field_of(FieldKind::Number);

    case core::AttrType::Length: {
        FieldSpec spec = field_of(FieldKind::Number);
        spec.suffix    = QStringLiteral("mm");
        return spec;
    }

    case core::AttrType::CodeRef:
    case core::AttrType::Text: break;
    }
    return field_of(FieldKind::Text);
}

// =============================================================================
// FieldDelegate
// =============================================================================

FieldDelegate::FieldDelegate(SpecFor specs, QObject* parent)
    : QStyledItemDelegate(parent), specs_(std::move(specs))
{}

QWidget* FieldDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                     const QModelIndex& index) const
{
    FieldSpec spec = specs_ ? specs_(index.column()) : FieldSpec{};
    spec.frame     = FieldFrame::Cell;

    auto* editor = new Field(spec, parent);
    editor->applyTheme(theme_);

    // COMMIT, CLOSE, THEN MOVE, in that order and through Qt's own signals. A
    // delegate that wrote the model itself would bypass `setModelData`, which is
    // the one place the value becomes a command.
    connect(editor, &Field::committed, this, [this, editor, index](const QString&) {
        auto* self = const_cast<FieldDelegate*>(this);
        emit self->commitData(editor);
        emit self->closeEditor(editor, QAbstractItemDelegate::NoHint);
        emit self->advanced(index);
    });
    connect(editor, &Field::cancelled, this, [this, editor] {
        auto* self = const_cast<FieldDelegate*>(this);
        emit self->closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
    });
    return editor;
}

void FieldDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* field = qobject_cast<Field*>(editor);
    if (field == nullptr) return;

    // THE EDIT ROLE, not the display one. A table prints `2 940.12` with a
    // grouping space and a `—` for an empty cell; neither is a value a command
    // would take, and putting one in the box makes the user delete characters
    // that were never there.
    const QString stored = index.data(Qt::EditRole).toString();
    field->setValue(stored == QStringLiteral("—") ? QString() : stored);
    field->beginEditing();
}

void FieldDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                 const QModelIndex& index) const
{
    auto* field = qobject_cast<Field*>(editor);
    if (field == nullptr) return;
    model->setData(index, field->value(), Qt::EditRole);
}

void FieldDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                         const QModelIndex&) const
{
    // EXACTLY THE CELL, for the reason the inspector's editor is: the only thing
    // that should change on screen is that the value became selectable.
    editor->setGeometry(option.rect);
}

} // namespace kentos::app
