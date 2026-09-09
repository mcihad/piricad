// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/fields.hpp"

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
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

namespace kentos::app {
namespace {

/// The picker on the right of a date, a colour or a multi-select: square, the
/// height of the box, and no wider than it is tall.
constexpr int kPickerWidth = 22;

/// How much of a `Range` row the readout takes, leaving the rest to the slider.
constexpr int kReadoutWidth = 54;

/// The leading mark: a 14 px glyph in a 24 px cell, the standard's own metric.
constexpr int kLeadWidth = 24;
constexpr int kLeadGlyph = 14;

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

/// The calendar card, `design.md`'s own geometry at day scale.
constexpr int kDayW      = 32;
constexpr int kDayH      = 26;
constexpr int kCalPad    = 10; ///< inside the card, around the grid
constexpr int kCalHead   = 34; ///< the month row
constexpr int kCalWeek   = 22; ///< the weekday row
constexpr int kCalFoot   = 32; ///< `Bugün` / `Temizle`
constexpr int kCalRows   = 6;  ///< always six, so the card never changes height
constexpr int kCalCols   = 7;
constexpr int kCalShadow = 12;
constexpr int kCalRadius = 8;
constexpr int kArrowW    = 26;

/// The one locale every piece of Turkish text in this program is formed with.
/// Month and day names are NOT a table here, and never `<cctype>` (CLAUDE.md 5.6).
const QLocale& turkish()
{
    static const QLocale one(QLocale::Turkish, QLocale::Turkey);
    return one;
}

} // namespace

// =============================================================================
// DatePopup
// =============================================================================

DatePopup::DatePopup(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("datePopup"));
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    const int card = kCalPad * 2 + kDayW * kCalCols;
    const int tall = kCalHead + kCalWeek + kDayH * kCalRows + kCalFoot + kCalPad;
    resize(card + kCalShadow * 2, tall + kCalShadow * 2);
}

void DatePopup::reveal(const QDate& on, const QPoint& anchor)
{
    chosen_  = on;
    cursor_  = on.isValid() ? on : QDate::currentDate();
    shown_   = cursor_;
    hover_   = -1;
    pressed_ = -1;

    QPoint at = anchor;
    if (const QScreen* screen = QApplication::screenAt(anchor); screen != nullptr) {
        const QRect room = screen->availableGeometry();
        at.setX(std::clamp(at.x(), room.left(), room.right() - width()));
        // ABOVE the anchor when there is no room below, which on a cell near the
        // bottom of a table is most of the time.
        if (at.y() + height() > room.bottom()) at.setY(anchor.y() - height() + kCalShadow * 2);
    }
    move(at);
    show();
    setFocus(Qt::PopupFocusReason);
}

QDate DatePopup::gridStart() const
{
    // MONDAY FIRST, because that is the week a Turkish calendar prints and the
    // one `QLocale(Turkish)` reports.
    const QDate first(shown_.year(), shown_.month(), 1);
    return first.addDays(-(first.dayOfWeek() - 1));
}

QRect DatePopup::arrowRect(int which) const
{
    const int top  = kCalShadow;
    const int left = kCalShadow;
    const int card = width() - kCalShadow * 2;
    switch (which) {
    case 0: return {left + kCalPad, top + 4, kArrowW, kCalHead - 8};                      // ‹ month
    case 1: return {left + kCalPad + kArrowW, top + 4, kArrowW, kCalHead - 8};            // › month
    case 2: return {left + card - kCalPad - kArrowW * 2, top + 4, kArrowW, kCalHead - 8}; // ‹ year
    case 3: return {left + card - kCalPad - kArrowW, top + 4, kArrowW, kCalHead - 8};     // › year
    default: return {};
    }
}

QRect DatePopup::footRect(int which) const
{
    const int card = width() - kCalShadow * 2;
    const int top  = height() - kCalShadow - kCalFoot;
    const int half = (card - kCalPad * 2) / 2;
    return {kCalShadow + kCalPad + which * half, top, half, kCalFoot - 6};
}

QDate DatePopup::dayAt(const QPoint& where) const
{
    const int left = kCalShadow + kCalPad;
    const int top  = kCalShadow + kCalHead + kCalWeek;

    const int column = (where.x() - left) / kDayW;
    const int row    = (where.y() - top) / kDayH;
    if (where.x() < left || where.y() < top) return {};
    if (column < 0 || column >= kCalCols || row < 0 || row >= kCalRows) return {};
    return gridStart().addDays(row * kCalCols + column);
}

void DatePopup::mouseMoveEvent(QMouseEvent* event)
{
    const QDate under = dayAt(event->position().toPoint());
    const int cell    = under.isValid() ? static_cast<int>(gridStart().daysTo(under)) : -1;
    if (cell == hover_) return;
    hover_ = cell;
    update();
}

void DatePopup::mousePressEvent(QMouseEvent* event)
{
    const QPoint at = event->position().toPoint();

    for (int which = 0; which < 4; ++which)
        if (arrowRect(which).contains(at)) {
            shown_ = shown_.addMonths(which == 0   ? -1
                                      : which == 1 ? 1
                                                   : 0)
                         .addYears(which == 2   ? -1
                                   : which == 3 ? 1
                                                : 0);
            update();
            return;
        }

    if (footRect(0).contains(at)) {
        const QDate today = QDate::currentDate();
        close();
        emit picked(today);
        return;
    }
    if (footRect(1).contains(at)) {
        close();
        emit cleared();
        return;
    }

    if (const QDate day = dayAt(at); day.isValid()) {
        close();
        emit picked(day);
        return;
    }

    // Outside the card dismisses it; a `Qt::Popup` grabs the pointer but does
    // not close itself.
    if (!QRect(kCalShadow, kCalShadow, width() - kCalShadow * 2, height() - kCalShadow * 2)
             .contains(at))
        close();
}

void DatePopup::keyPressEvent(QKeyEvent* event)
{
    // A CALENDAR IS A GRID AND ARROWS WALK IT. Somebody entering a hundred
    // approval dates should never have to reach for the mouse to move one day.
    switch (event->key()) {
    case Qt::Key_Left: cursor_ = cursor_.addDays(-1); break;
    case Qt::Key_Right: cursor_ = cursor_.addDays(1); break;
    case Qt::Key_Up: cursor_ = cursor_.addDays(-7); break;
    case Qt::Key_Down: cursor_ = cursor_.addDays(7); break;
    case Qt::Key_PageUp: cursor_ = cursor_.addMonths(-1); break;
    case Qt::Key_PageDown: cursor_ = cursor_.addMonths(1); break;
    case Qt::Key_Home: cursor_ = QDate::currentDate(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        const QDate day = cursor_;
        close();
        emit picked(day);
        return;
    }
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        close();
        emit cleared();
        return;
    case Qt::Key_Escape: close(); return;
    default: QWidget::keyPressEvent(event); return;
    }

    shown_ = cursor_;
    update();
}

void DatePopup::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void DatePopup::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF card(kCalShadow, kCalShadow, width() - kCalShadow * 2.0,
                      height() - kCalShadow * 2.0);

    // The same shadow the tool flyout wears, for the same reason: a popup that
    // sits flat on the window behind it reads as part of that window.
    p.setPen(Qt::NoPen);
    for (int ring = kCalShadow; ring > 0; ring -= 3) {
        QColor ink(0, 0, 0);
        ink.setAlphaF(0.030f * static_cast<float>(kCalShadow - ring + 3) / 3.0f);
        p.setBrush(ink);
        p.drawRoundedRect(card.adjusted(-ring, -ring + 2, ring, ring + 2), kCalRadius + ring,
                          kCalRadius + ring);
    }

    p.setBrush(t.bgRaised);
    p.setPen(QPen(t.border, 1.0));
    p.drawRoundedRect(card, kCalRadius, kCalRadius);

    QFont face = font();

    // ---- the month row ----
    face.setPixelSize(12);
    face.setWeight(QFont::DemiBold);
    p.setFont(face);
    p.setPen(t.text);
    p.drawText(QRectF(card.left(), card.top(), card.width(), kCalHead), Qt::AlignCenter,
               turkish().toString(shown_, QStringLiteral("MMMM yyyy")));

    face.setWeight(QFont::Normal);
    face.setPixelSize(13);
    p.setFont(face);
    static const char* kArrows[] = {"‹", "›", "«", "»"};
    for (int which = 0; which < 4; ++which) {
        const QRect box = arrowRect(which);
        if (box.contains(mapFromGlobal(QCursor::pos()))) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverIcon);
            p.drawRoundedRect(box, 4, 4);
        }
        p.setPen(t.textDim);
        p.drawText(box, Qt::AlignCenter, QString::fromUtf8(kArrows[which]));
    }

    // ---- the weekday row ----
    face.setPixelSize(10);
    p.setFont(face);
    p.setPen(t.textFaint);
    for (int column = 0; column < kCalCols; ++column) {
        const QRect box(kCalShadow + kCalPad + column * kDayW, kCalShadow + kCalHead, kDayW,
                        kCalWeek);
        p.drawText(box, Qt::AlignCenter, turkish().dayName(column + 1, QLocale::ShortFormat));
    }

    // ---- the days ----
    face.setPixelSize(12);
    p.setFont(face);

    const QDate start = gridStart();
    const QDate today = QDate::currentDate();
    for (int cell = 0; cell < kCalRows * kCalCols; ++cell) {
        const QDate day = start.addDays(cell);
        const QRect box(kCalShadow + kCalPad + (cell % kCalCols) * kDayW,
                        kCalShadow + kCalHead + kCalWeek + (cell / kCalCols) * kDayH, kDayW, kDayH);

        const bool inMonth  = day.month() == shown_.month();
        const bool isChosen = day == chosen_;
        const bool isCursor = day == cursor_;

        if (isChosen) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.accent);
            p.drawRoundedRect(box.adjusted(2, 1, -2, -1), 4, 4);
        } else if (cell == hover_) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverRow);
            p.drawRoundedRect(box.adjusted(2, 1, -2, -1), 4, 4);
        }
        if (isCursor && !isChosen) {
            p.setPen(QPen(t.accentEdge, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(2, 1, -2, -1), 4, 4);
        }

        // TODAY IS A MARK, not a fill: a fill would compete with the selection,
        // and on the day somebody happens to be entering they would be the same
        // colour and mean two different things.
        if (day == today && !isChosen) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.warn);
            p.drawEllipse(QPointF(box.center().x() + 0.5, box.bottom() - 3.0), 1.6, 1.6);
        }

        p.setPen(isChosen ? t.onAccent : (inMonth ? t.text : t.textFaint));
        p.drawText(box, Qt::AlignCenter, QString::number(day.day()));
    }

    // ---- the foot ----
    face.setPixelSize(11);
    p.setFont(face);
    static const char* kFeet[] = {"Bugün", "Temizle"};
    for (int which = 0; which < 2; ++which) {
        const QRect box = footRect(which);
        if (box.contains(mapFromGlobal(QCursor::pos()))) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverRow);
            p.drawRoundedRect(box, 4, 4);
        }
        p.setPen(which == 0 ? t.accent : t.textDim);
        p.drawText(box, Qt::AlignCenter, tr(kFeet[which]));
    }
}

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

    // THE MARK AT THE LEFT EDGE, before whatever the kind puts in the row: a
    // ruler before a length, a lock before a read-only code. Built for every
    // kind that has a line, hidden until a glyph or a derived state asks for it.
    lead_ = new QLabel(this);
    lead_->setObjectName(QStringLiteral("fieldLead"));
    lead_->setFixedWidth(kLeadWidth);
    lead_->setAlignment(Qt::AlignCenter);
    lead_->setVisible(spec_.glyph.has_value());
    row->addWidget(lead_, 0);

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
            if (calendar_ == nullptr) {
                calendar_ = new DatePopup(this);
                connect(calendar_, &DatePopup::picked, this, [this](const QDate& day) {
                    line_->setText(day.toString(Qt::ISODate));
                    commit();
                });
                connect(calendar_, &DatePopup::cleared, this, [this] {
                    line_->clear();
                    commit();
                });
            }
            calendar_->applyTheme(theme_);
            calendar_->reveal(QDate::fromString(line_->text(), Qt::ISODate),
                              mapToGlobal(QPoint(0, height())));
        });
        break;

    case FieldKind::Bool: {
        // IN A FORM, A SWITCH: `bileşen_standardı.png` gives an on/off setting the
        // pill, and a `Zorunlu` row that stretched two words across the whole
        // column read as a segmented control about something else. The word
        // beside the pill says the state in text as well (design.md §13).
        if (spec_.frame == FieldFrame::Box) {
            // No box around a pill: the switch is its own shape, and a bordered
            // input ground behind it read as a text field holding a toggle.
            setProperty("frame", QStringLiteral("bare"));
            auto* pill = new ToggleSwitch(this);
            pill->installEventFilter(this);
            toggle_     = pill;
            toggleWord_ = new QLabel(tr("hayır"), this);
            toggleWord_->setObjectName(QStringLiteral("fieldUnit"));
            connect(pill, &QAbstractButton::toggled, this, [this](bool on) {
                toggleWord_->setText(on ? tr("evet") : tr("hayır"));
                commit();
            });
            row->addSpacing(6);
            row->addWidget(pill);
            row->addWidget(toggleWord_);
            row->addStretch(1);
            break;
        }

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
        combo_ = new ComboBox(this);
        combo_->setObjectName(QStringLiteral("fieldCombo"));
        combo_->setBare(true); // the field is the box
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

    // THE UNIT AT THE RIGHT EDGE — `m`, `m²`, `°` — dim and in mono, because it
    // is read with the figure and not instead of it. `spec.suffix` was carried
    // by every length field and drawn by none until this label existed.
    unit_ = new QLabel(spec_.suffix, this);
    unit_->setObjectName(QStringLiteral("fieldUnit"));
    unit_->setVisible(!spec_.suffix.isEmpty());
    if (picker_ != nullptr)
        row->insertWidget(row->indexOf(picker_), unit_, 0);
    else
        row->addWidget(unit_, 0);

    refreshLead();
    installEventFilter(this);
}

void Field::setState(FieldState state)
{
    state_ = state;

    // One word the stylesheet reads, on the frame AND on the line: the frame
    // owns the border, the line owns the ink, and the two rules live under the
    // same name so they cannot say different things.
    const char* word = nullptr;
    switch (state_) {
    case FieldState::Changed: word = "changed"; break;
    case FieldState::Invalid: word = "invalid"; break;
    case FieldState::Derived: word = "derived"; break;
    case FieldState::ReadOnly: word = "readonly"; break;
    case FieldState::Normal: break;
    }
    const QVariant value = word != nullptr ? QVariant(QLatin1String(word)) : QVariant();

    const auto restyle = [](QWidget* w, const QVariant& v) {
        w->setProperty("state", v);
        w->style()->unpolish(w);
        w->style()->polish(w);
    };
    restyle(this, value);
    if (line_ != nullptr) {
        restyle(line_, value);
        line_->setReadOnly(state_ == FieldState::ReadOnly || spec_.kind == FieldKind::Colour);
    }
    if (combo_ != nullptr) restyle(combo_, value);
    refreshLead();
}

void Field::setUnit(const QString& unit)
{
    spec_.suffix = unit;
    if (unit_ == nullptr) return;
    unit_->setText(unit);
    unit_->setVisible(!unit.isEmpty());
}

void Field::refreshLead()
{
    if (lead_ == nullptr) return;
    const Tokens& t = tokensOf(theme_);

    // A derived value wears the `fx` of a formula in the accent, whatever mark
    // the spec asked for: that it is COMPUTED is the more important thing to say.
    std::optional<Glyph> mark = spec_.glyph;
    QColor ink                = t.textDim;
    if (state_ == FieldState::Derived) {
        mark = Glyph::Function;
        ink  = t.accentHi;
    }
    if (state_ == FieldState::Invalid) ink = t.danger;
    if (state_ == FieldState::Changed) ink = t.warn;

    lead_->setVisible(mark.has_value());
    if (mark) lead_->setPixmap(glyph_pixmap(*mark, ink, kLeadGlyph, devicePixelRatioF()));
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
    case FieldKind::Bool:
        if (toggle_ != nullptr) return toggle_->isChecked() ? tr("evet") : tr("hayır");
        return (yes_ != nullptr && yes_->isChecked()) ? tr("evet") : tr("hayır");
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
    case FieldKind::Bool: {
        const bool on =
            text.compare(tr("evet"), Qt::CaseInsensitive) == 0 || text == QLatin1String("1");
        if (toggle_ != nullptr) {
            QSignalBlocker quiet(toggle_);
            toggle_->setChecked(on);
            if (toggleWord_ != nullptr) toggleWord_->setText(on ? tr("evet") : tr("hayır"));
        }
        if (yes_ != nullptr && no_ != nullptr) {
            yes_->setChecked(on);
            no_->setChecked(!on);
        }
        return;
    }

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

    // QUEUED, AND THIS IS A CRASH FIX rather than a nicety. Both ways an edit
    // ends — Enter and focus leaving — are handled inside this widget's own event
    // filter. Whoever hears `committed` closes the editor, and in a table view
    // that means Qt DELETES it: the object whose event handler is still on the
    // stack. Deferring the emit to the next turn of the loop lets the handler
    // return first, and the editor is destroyed with nothing of its own running.
    //
    // The context object is `this`, so an editor destroyed before the loop gets
    // there simply never emits — which is the right answer and not a leak.
    const QString settled = value();
    QMetaObject::invokeMethod(
        this, [this, settled] { emit committed(settled); }, Qt::QueuedConnection);
}

void Field::cancel()
{
    if (done_) return;
    done_ = true;
    QMetaObject::invokeMethod(this, [this] { emit cancelled(); }, Qt::QueuedConnection);
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

        // THE PICKER OPENS FROM THE KEYBOARD — Alt+Down or F4, the keys a combo
        // box answers to — because the button beside the line takes no focus
        // (it must not: Tab has to leave the field in one step) and a calendar
        // only a mouse could open would be a capability the keyboard lacks
        // (CLAUDE.md 5.15, ui.md R21).
        const bool opensPicker =
            key->key() == Qt::Key_F4 ||
            (key->key() == Qt::Key_Down && key->modifiers().testFlag(Qt::AltModifier));
        if (opensPicker && picker_ != nullptr) {
            if (picker_->menu() != nullptr)
                picker_->showMenu();
            else
                picker_->click();
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
    refreshLead();

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

bool FieldDelegate::eventFilter(QObject*, QEvent*)
{
    // Deliberately NOT `QStyledItemDelegate::eventFilter` — see the header. And
    // not `QObject::eventFilter` either: that would be the same decision spelt as
    // a call past the parent, which the analyser rightly reads as a mistake. The
    // base implementation is `return false`, so this says so itself: the field
    // handles its own keys, and this filter handles nothing.
    return false;
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
