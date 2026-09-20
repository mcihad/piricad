// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/app/datagrid.hpp"
#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/expression_edit.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <algorithm>

#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

// `bileşen_standardı.png`, measured: "yarıçap 4 px · kenar #363D43", a 14 px
// indicator, a 22 px chip, a 14 px badge. Every number below is the sheet's.
constexpr int kRadius      = 4;
constexpr int kBoxSize     = 14; ///< check and radio indicator
constexpr int kBoxGap      = 8;  ///< indicator to its label
constexpr int kInset       = 2;  ///< room for the focus ring around a painted control
constexpr int kChipHeight  = 22;
constexpr int kChipPadX    = 10;
constexpr int kBadgeHeight = 14;
constexpr int kBadgePadX   = 4;
constexpr int kBadgePx     = 9;
constexpr int kSectionH    = 22;
constexpr int kSectionPx   = 10;
constexpr int kStripH      = 2;
constexpr int kIconPx      = 16;
constexpr int kIconButton  = 32;
constexpr int kReadoutW    = 44;
constexpr int kBannerPad   = 12;

// ---- the conversation, measured against the same standard ------------------
constexpr int kBubblePad    = 12; ///< inside a bubble, all four sides
constexpr int kBubbleRadius = 6;  ///< one step above the 4 px of a control
constexpr int kBubbleGap    = 10; ///< between two bubbles in the transcript
constexpr int kFollowSlack  = 24; ///< how far from the end still counts as "at the end"
constexpr int kDotSize      = 4;  ///< one thinking dot
constexpr int kDotGap       = 5;
constexpr int kDotRise      = 3; ///< how far a dot lifts at the top of its arc
constexpr int kDotTickMs    = 120;
constexpr int kDotPhases    = 4;  ///< three dots and one rest
constexpr int kAttachH      = 26; ///< an attachment chip, a step above the 22 px `Chip`
constexpr int kAttachPadX   = 8;
constexpr int kMeterH       = 18;
constexpr int kMeterBarH    = 3;
constexpr int kMeterPx      = 10;

/// §11: the indeterminate strip moves; nothing else in the shell animates.
constexpr int kStripTickMs   = 16;
constexpr int kStripStep     = 6;
constexpr int kStripSegmentP = 30; ///< the moving segment, in per cent of the width

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QFont sans(int px, QFont::Weight weight = QFont::Normal, qreal tracking = 0.0)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(px);
    f.setWeight(weight);
    if (tracking != 0.0) f.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    return f;
}

/// Whether a focus arrived from the keyboard. §11 shows the ring for Tab and
/// for a shortcut and never for a click: a ring under the pointer says nothing
/// the pointer did not already say.
bool keyboardReason(const QFocusEvent* event)
{
    switch (event->reason()) {
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
    case Qt::ShortcutFocusReason: return true;
    default: return false;
    }
}

/// The ring itself: 1 px accent, `kInset` outside the control's own box.
void paintFocusRing(QPainter& p, const QRectF& around, qreal radius, const Tokens& t)
{
    p.setPen(QPen(t.accent, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(around.adjusted(-kInset + 0.5, -kInset + 0.5, kInset - 0.5, kInset - 0.5),
                      radius + kInset, radius + kInset);
}

/// Sets a stylesheet-read property and makes the style notice. Qt evaluates a
/// `[prop="x"]` selector when the widget is polished, not when the property
/// changes, so a change without a repolish is a change that shows on the next
/// resize, if ever.
void restyle(QWidget* w, const char* name, const QVariant& value)
{
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// The `tone` property value the stylesheet's `QLabel#formHelp[tone="…"]` rules
/// read, or an invalid variant for the default faint note.
QVariant toneProperty(Tone tone)
{
    switch (tone) {
    case Tone::Danger: return QStringLiteral("danger");
    case Tone::Warn: return QStringLiteral("warn");
    case Tone::Accent: return QStringLiteral("accent");
    case Tone::Ok: return QStringLiteral("ok");
    case Tone::Neutral: break;
    }
    return {};
}

const char* roleName(ButtonRole role)
{
    switch (role) {
    case ButtonRole::Primary: return "primary";
    case ButtonRole::Secondary: return "secondary";
    case ButtonRole::Ghost: return "ghost";
    case ButtonRole::Danger: return "danger";
    case ButtonRole::Mode: return "mode";
    case ButtonRole::Icon: return "iconButton";
    }
    return "secondary";
}

const char* sizeName(ControlSize size)
{
    switch (size) {
    case ControlSize::Compact: return "compact";
    case ControlSize::Regular: return "regular";
    case ControlSize::Large: return "large";
    }
    return "regular";
}

} // namespace

// =============================================================================
// tones
// =============================================================================

ToneColours toneColours(Tone tone, ThemeMode mode)
{
    const Tokens& t = tokensOf(mode);
    switch (tone) {
    case Tone::Accent: return {t.accentHi, t.accentWash};
    case Tone::Warn: return {t.warn, t.warnWash};
    case Tone::Danger: return {t.danger, t.dangerWash};
    case Tone::Ok: {
        QColor wash = t.ok;
        wash.setAlpha(26);
        return {t.ok, wash};
    }
    case Tone::Neutral: break;
    }
    return {t.textFaint, t.hoverIcon};
}

// =============================================================================
// Button
// =============================================================================

Button::Button(ButtonRole role, const QString& text, std::optional<Glyph> glyph, QWidget* parent)
    : QPushButton(text, parent), role_(role), glyph_(glyph)
{
    // THE ROLE IS THE OBJECT NAME, which is what the one stylesheet styles by.
    // It is also a property, so a probe can read it back without knowing which
    // roles have a rule of their own.
    setObjectName(QLatin1String(roleName(role_)));
    setProperty("role", QLatin1String(roleName(role_)));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(text);

    if (role_ == ButtonRole::Mode) setCheckable(true);
    if (role_ == ButtonRole::Icon) {
        setFixedSize(kIconButton, kIconButton);
    } else {
        setControlSize(ControlSize::Regular);
    }
    if (glyph_) setIconSize(QSize(kIconPx, kIconPx));
    refreshIcon();
}

Button::Button(Glyph glyph, const QString& tooltip, QWidget* parent)
    : Button(ButtonRole::Icon, QString(), glyph, parent)
{
    // THE TOOLTIP IS THE LABEL, and a screen reader has to be told so: an icon
    // button with no accessible name is announced as "button" (ui.md R22).
    setToolTip(tooltip);
    setAccessibleName(tooltip);
}

void Button::setControlSize(ControlSize size)
{
    size_ = size;
    if (role_ == ButtonRole::Icon) return; // 32×32 by the standard, whatever is asked
    setFixedHeight(static_cast<int>(size_));
    restyle(this, "size", QLatin1String(sizeName(size_)));
}

void Button::setMenuArrow(QMenu* menu)
{
    // Fusion's own indicator, for the reason the combo box keeps its own arrow:
    // styling `::menu-indicator` makes Qt stop drawing it and nothing here can
    // draw a triangle without an image asset. See `theme.cpp`.
    setMenu(menu);
    restyle(this, "menu", true);
}

void Button::refreshIcon()
{
    if (!glyph_) return;
    const Tokens& t = tokensOf(theme_);

    // The glyph wears the button's ink: white on a filled primary, danger on a
    // destructive outline, dim on everything else — and accent when a mode
    // button is on, which `icon()`'s checked variant gives for free.
    QColor ink = t.textDim;
    if (role_ == ButtonRole::Primary) ink = t.onAccent;
    if (role_ == ButtonRole::Danger) ink = t.danger;
    setIcon(kentos::app::icon(*glyph_, ink, t.accentHi, kIconPx));
}

void Button::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    refreshIcon();
    update();
}

void Button::focusInEvent(QFocusEvent* event)
{
    keyboardFocus_ = keyboardReason(event);
    QPushButton::focusInEvent(event);
}

void Button::focusOutEvent(QFocusEvent* event)
{
    keyboardFocus_ = false;
    QPushButton::focusOutEvent(event);
}

void Button::paintEvent(QPaintEvent* event)
{
    QPushButton::paintEvent(event);
    if (!keyboardFocus_) return;

    // INSIDE the border rather than 2 px outside it, because a widget cannot
    // paint past its own rectangle and a button has no margin to spare. The
    // ring sits one pixel in from the edge, in the accent, which is the shape
    // §11 asks for at the distance the geometry allows.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(tokensOf(theme_).accent, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), kRadius - 1, kRadius - 1);
}

// =============================================================================
// CheckBox
// =============================================================================

CheckBox::CheckBox(const QString& text, QWidget* parent) : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(text);
}

void CheckBox::setCheckState(Qt::CheckState state)
{
    partial_ = state == Qt::PartiallyChecked;
    setChecked(state == Qt::Checked);
    update();
}

Qt::CheckState CheckBox::checkState() const noexcept
{
    if (partial_) return Qt::PartiallyChecked;
    return isChecked() ? Qt::Checked : Qt::Unchecked;
}

void CheckBox::nextCheckState()
{
    // Off → some → on → off, when three states are allowed; the base class only
    // knows two and would skip the middle one.
    if (!tristate_) {
        partial_ = false;
        QAbstractButton::nextCheckState();
        return;
    }
    if (!isChecked() && !partial_) {
        partial_ = true;
    } else if (partial_) {
        partial_ = false;
        setChecked(true);
    } else {
        setChecked(false);
    }
    update();
}

QSize CheckBox::sizeHint() const
{
    const QFontMetrics fm(sans(12));
    return {kInset * 2 + kBoxSize + kBoxGap + fm.horizontalAdvance(text()),
            std::max(kBoxSize + kInset * 2, fm.height()) + 2};
}

void CheckBox::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void CheckBox::focusInEvent(QFocusEvent* event)
{
    keyboardFocus_ = keyboardReason(event);
    QAbstractButton::focusInEvent(event);
}

void CheckBox::focusOutEvent(QFocusEvent* event)
{
    keyboardFocus_ = false;
    QAbstractButton::focusOutEvent(event);
}

void CheckBox::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled()) p.setOpacity(0.38); // §11: disabled is opacity, not a colour

    const QRectF box(kInset, (height() - kBoxSize) / 2.0, kBoxSize, kBoxSize);
    const bool on = isChecked() || partial_;

    p.setPen(QPen(on ? t.accentLift : (underMouse() ? t.separator : t.border), 1.0));
    p.setBrush(on ? t.accent : t.bgInput);
    p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);

    // THE SHAPE, not only the colour (§13): a tick when on, a dash when partly.
    if (partial_) {
        p.setPen(QPen(t.onAccent, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(box.left() + 3.5, box.center().y()),
                   QPointF(box.right() - 3.5, box.center().y()));
    } else if (isChecked()) {
        p.drawPixmap(box.adjusted(1, 1, -1, -1).toRect(),
                     glyph_pixmap(Glyph::Check, t.onAccent, kBoxSize - 2, devicePixelRatioF()));
    }

    p.setFont(sans(12));
    p.setPen(t.text);
    p.drawText(QRectF(box.right() + kBoxGap, 0, width() - box.right() - kBoxGap, height()),
               Qt::AlignVCenter | Qt::AlignLeft, text());

    if (keyboardFocus_) paintFocusRing(p, box, 3.0, t);
}

// =============================================================================
// RadioButton
// =============================================================================

RadioButton::RadioButton(const QString& text, QWidget* parent) : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setAutoExclusive(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(text);
}

QSize RadioButton::sizeHint() const
{
    const QFontMetrics fm(sans(12));
    return {kInset * 2 + kBoxSize + kBoxGap + fm.horizontalAdvance(text()),
            std::max(kBoxSize + kInset * 2, fm.height()) + 2};
}

void RadioButton::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void RadioButton::focusInEvent(QFocusEvent* event)
{
    keyboardFocus_ = keyboardReason(event);
    QAbstractButton::focusInEvent(event);
}

void RadioButton::focusOutEvent(QFocusEvent* event)
{
    keyboardFocus_ = false;
    QAbstractButton::focusOutEvent(event);
}

void RadioButton::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled()) p.setOpacity(0.38);

    const QRectF ring(kInset, (height() - kBoxSize) / 2.0, kBoxSize, kBoxSize);

    p.setPen(QPen(isChecked() ? t.accent : (underMouse() ? t.separator : t.border), 1.0));
    p.setBrush(t.bgInput);
    p.drawEllipse(ring.adjusted(0.5, 0.5, -0.5, -0.5));

    // The dot is the shape; the ring's colour only repeats it.
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        p.drawEllipse(ring.center(), 3.5, 3.5);
    }

    p.setFont(sans(12));
    p.setPen(t.text);
    p.drawText(QRectF(ring.right() + kBoxGap, 0, width() - ring.right() - kBoxGap, height()),
               Qt::AlignVCenter | Qt::AlignLeft, text());

    if (keyboardFocus_) paintFocusRing(p, ring, kBoxSize / 2.0, t);
}

// =============================================================================
// Segment
// =============================================================================

Segment::Segment(QWidget* parent) : QWidget(parent)
{
    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(0, 0, 0, 0);
    row_->setSpacing(0);

    // THE CONTROL IS ITS BUTTONS' HEIGHT. Left with the default vertical policy,
    // a layout with room to spare stretched the container while the buttons
    // inside kept their 24 px — and the inventory then measured a segment eight
    // pixels taller than anything a person could see.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(static_cast<int>(size_));

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    connect(group_, &QButtonGroup::idClicked, this, &Segment::setCurrent);
}

int Segment::addOption(const QString& text, const QString& tooltip)
{
    // A push button INSIDE the component: the one place `new QPushButton` is
    // written for a segment, so the corner radii and the shared edges are
    // decided once. Every earlier segment in the shell rounded a different
    // corner.
    auto* button = new QPushButton(text, this);
    button->setObjectName(QStringLiteral("segment"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setAccessibleName(text);
    if (!tooltip.isEmpty()) button->setToolTip(tooltip);
    button->setFixedHeight(static_cast<int>(size_));

    const int index = static_cast<int>(buttons_.size());
    buttons_.push_back(button);
    group_->addButton(button, index);
    row_->addWidget(button);

    // First, middle, last: the outer two carry the outer radii and every one
    // after the first drops its left border, so two neighbours share one line.
    for (int i = 0; i < buttons_.size(); ++i) {
        const char* edge = buttons_.size() == 1       ? "only"
                           : i == 0                   ? "first"
                           : i == buttons_.size() - 1 ? "last"
                                                      : "mid";
        restyle(buttons_[i], "edge", QLatin1String(edge));
    }

    if (current_ < 0) setCurrent(0);
    return index;
}

void Segment::setCurrent(int index)
{
    if (index >= buttons_.size()) return;
    const bool changed = index != current_;
    current_           = index;

    if (index < 0) {
        // NO ANSWER, ON PURPOSE. The style designer asks for this when a
        // symbol's layers disagree about their unit: no one option is the truth,
        // and lighting the first would be a lie. An exclusive group refuses to
        // have nothing checked, so exclusivity is relaxed for the write and
        // restored after it — Qt's own way of clearing a segmented control.
        group_->setExclusive(false);
        for (QPushButton* button : buttons_)
            button->setChecked(false);
        group_->setExclusive(true);
    } else {
        buttons_[index]->setChecked(true);
    }
    if (changed) emit currentChanged(index);
}

void Segment::setControlSize(ControlSize size)
{
    setFixedHeight(static_cast<int>(size));
    size_ = size;
    for (QPushButton* button : buttons_) {
        button->setFixedHeight(static_cast<int>(size_));
        restyle(button, "size", QLatin1String(sizeName(size_)));
    }
}

void Segment::applyTheme(ThemeMode mode)
{
    theme_ = mode; // the buttons are the sheet's; nothing here paints
}

// =============================================================================
// ComboBox
// =============================================================================

namespace {
constexpr int kComboPadX   = 10; ///< text inset
constexpr int kComboArrow  = 14; ///< the chevron
constexpr int kComboArrowW = 26; ///< the room the chevron takes at the right
} // namespace

ComboBox::ComboBox(QWidget* parent) : QComboBox(parent)
{
    // The sheet is told to draw NOTHING for this box — see `theme.cpp` — so the
    // ground, the border and the chevron below are the only ones there are.
    setObjectName(QStringLiteral("comboBox"));
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setControlSize(ControlSize::Regular);

    // THE POPUP IS THE SHELL'S LIST. Qt's default view is a `QListView` drawn by
    // the platform style; naming it lets the one sheet give it the panel ground,
    // the 26 px rows and the hover wash every other list in the program has.
    auto* list = new QListView(this);
    list->setObjectName(QStringLiteral("comboPopup"));
    list->setUniformItemSizes(true);
    setView(list);
}

void ComboBox::setControlSize(ControlSize size)
{
    size_ = size;
    setFixedHeight(static_cast<int>(size_));
    restyle(this, "size", QLatin1String(sizeName(size_)));
}

void ComboBox::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QSize ComboBox::sizeHint() const
{
    const QFontMetrics fm(font());
    int widest = 0;
    for (int i = 0; i < count(); ++i)
        widest = std::max(widest, fm.horizontalAdvance(itemText(i)));
    return {widest + 2 * kComboPadX + kComboArrowW + (iconSize().width() > 0 ? 22 : 0),
            static_cast<int>(size_)};
}

void ComboBox::focusInEvent(QFocusEvent* event)
{
    QComboBox::focusInEvent(event);
    update();
}

void ComboBox::focusOutEvent(QFocusEvent* event)
{
    QComboBox::focusOutEvent(event);
    update();
}

void ComboBox::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // A combo INSIDE A FIELD wears no box: the field paints the frame, or the
    // table row is the frame (fields.hpp). Everywhere else it is the standard's
    // input, box and all.
    const bool bare     = bare_;
    const QString state = property("state").toString();
    const bool on       = isEnabled();

    QColor ground = t.bgInput;
    QColor edge   = underMouse() && on ? t.separator : t.border;
    if (state == QLatin1String("changed")) {
        edge   = t.warn;
        ground = t.warnWash;
    } else if (state == QLatin1String("invalid")) {
        edge   = t.dangerEdge;
        ground = t.dangerWash;
    } else if (state == QLatin1String("derived")) {
        edge = t.accentEdge;
    } else if (state == QLatin1String("readonly")) {
        edge   = t.lineSoft;
        ground = t.bgSunken;
    }
    if (!on) {
        edge   = t.lineSoft;
        ground = Qt::transparent;
    }
    if (hasFocus() && on) edge = t.accent;

    if (!bare) {
        p.setPen(QPen(edge, 1.0));
        p.setBrush(ground);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    }

    // AN EDITABLE COMBO ALREADY HAS SOMETHING DRAWING ITS TEXT: its own
    // `QLineEdit`. Painting the current text here too put two strings in the
    // same pixels, which on the model chooser read as `llama3.1` over
    // `llama3.1 · Llama 3.1 8B · 131 b`. The chrome is still this widget's; the
    // words belong to the editor, and only the arrow is added after it.
    if (isEditable()) {
        const QRectF arrow(width() - kComboPadX - kComboArrow, (height() - kComboArrow) / 2.0,
                           kComboArrow, kComboArrow);
        p.drawPixmap(arrow.toRect(), glyph_pixmap(Glyph::ChevronDown, on ? t.textDim : t.textFaint,
                                                  kComboArrow, devicePixelRatioF()));
        return;
    }

    // The value, with the item's icon before it when it has one.
    int x            = kComboPadX;
    const QIcon mark = currentIndex() >= 0 ? itemIcon(currentIndex()) : QIcon();
    if (!mark.isNull()) {
        const QSize sz =
            iconSize().isValid() && iconSize().width() > 0 ? iconSize() : QSize(16, 16);
        mark.paint(&p, QRect(x, (height() - sz.height()) / 2, sz.width(), sz.height()));
        x += sz.width() + 6;
    }
    QFont face(QStringLiteral("IBM Plex Sans"));
    face.setPixelSize(size_ == ControlSize::Compact ? 11 : 12);
    p.setFont(face);
    QColor ink = on ? t.text : t.textFaint;
    if (state == QLatin1String("changed")) ink = t.warn;
    if (state == QLatin1String("invalid")) ink = t.danger;
    if (state == QLatin1String("derived")) ink = t.accentHi;
    if (state == QLatin1String("readonly")) ink = t.textDim;
    p.setPen(ink);
    const QRect box(x, 0, width() - x - kComboArrowW, height());
    const QString shown = currentIndex() >= 0 ? currentText() : placeholderText();
    if (currentIndex() < 0) p.setPen(t.hint);
    p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(shown, Qt::ElideRight, box.width()));

    // The chevron, drawn: the mark that says "this opens", re-tinted with the
    // theme, the same 14 px on every list in the program.
    p.drawPixmap(QRect(width() - kComboArrowW + (kComboArrowW - kComboArrow) / 2,
                       (height() - kComboArrow) / 2, kComboArrow, kComboArrow),
                 glyph_pixmap(Glyph::ChevronDown, on ? t.textDim : t.textFaint, kComboArrow,
                              devicePixelRatioF()));

    if (hasFocus() && on && !bare) paintFocusRing(p, QRectF(rect()), 4.0, t);
}

void ComboBox::setBare(bool on)
{
    bare_ = on;
    // The owner's height is the height: a field is 30 px, a table cell is
    // whatever the row is, and a fixed 30 inside a 26 px cell overflowed it.
    if (on) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
        setControlSize(size_);
    }
    update();
}

// =============================================================================
// Slider
// =============================================================================

Slider::Slider(QWidget* parent) : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setObjectName(QStringLiteral("fieldSlider"));
    slider_->setFocusPolicy(Qt::StrongFocus);
    row->addWidget(slider_, 1);

    // MONO AND RIGHT-ALIGNED, because it is a figure (design.md §1.3): a reader
    // compares it with the one above and below, and that needs the digits to
    // line up.
    readout_ = new QLabel(this);
    readout_->setObjectName(QStringLiteral("sliderReadout"));
    readout_->setFixedWidth(kReadoutW);
    readout_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(readout_, 0);

    connect(slider_, &QSlider::valueChanged, this, [this](int value) {
        refreshReadout();
        emit valueChanged(value);
    });
    refreshReadout();
}

void Slider::setRange(int low, int high)
{
    slider_->setRange(low, high);
    refreshReadout();
}

void Slider::setValue(int value)
{
    slider_->setValue(value);
}

int Slider::value() const
{
    return slider_->value();
}

void Slider::setAffixes(const QString& prefix, const QString& suffix)
{
    prefix_ = prefix;
    suffix_ = suffix;
    refreshReadout();
}

void Slider::refreshReadout()
{
    readout_->setText(prefix_ + QString::number(slider_->value()) + suffix_);
    readout_->setAccessibleName(readout_->text());
}

void Slider::applyTheme(ThemeMode mode)
{
    theme_ = mode;
}

// =============================================================================
// Chip
// =============================================================================

Chip::Chip(const QString& text, const QColor& colour, QWidget* parent)
    : QAbstractButton(parent), colour_(colour)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(text);
}

Chip* Chip::overflow(int hidden, QWidget* parent)
{
    auto* chip     = new Chip(QStringLiteral("+%1").arg(hidden), QColor(), parent);
    chip->neutral_ = true;
    chip->setCheckable(false);
    chip->setFocusPolicy(Qt::NoFocus);
    chip->setCursor(Qt::ArrowCursor);
    return chip;
}

void Chip::setColour(const QColor& colour)
{
    colour_ = colour;
    update();
}

QSize Chip::sizeHint() const
{
    const QFontMetrics fm(sans(11, QFont::Medium));
    return {kInset * 2 + kChipPadX * 2 + fm.horizontalAdvance(text()), kChipHeight + kInset * 2};
}

void Chip::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void Chip::focusInEvent(QFocusEvent* event)
{
    keyboardFocus_ = keyboardReason(event);
    QAbstractButton::focusInEvent(event);
}

void Chip::focusOutEvent(QFocusEvent* event)
{
    keyboardFocus_ = false;
    QAbstractButton::focusOutEvent(event);
}

void Chip::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled()) p.setOpacity(0.38);

    const QColor ink = neutral_ ? t.textDim : colour_;
    const QRectF pill(kInset, kInset, width() - kInset * 2.0, kChipHeight);

    // The fill says "chosen", the outline says "which": a chosen chip is its own
    // colour at a fifth, a hovered one at a twelfth, a resting one hollow.
    QColor fill = ink;
    fill.setAlpha(isChecked() ? 52 : (underMouse() && isCheckable() ? 31 : 0));

    p.setPen(QPen(neutral_ ? t.border : ink, 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), kChipHeight / 2.0, kChipHeight / 2.0);

    p.setFont(sans(11, QFont::Medium));
    p.setPen(ink);
    p.drawText(pill, Qt::AlignCenter, text());

    if (keyboardFocus_) paintFocusRing(p, pill, kChipHeight / 2.0, t);
}

// =============================================================================
// Badge
// =============================================================================

Badge::Badge(const QString& text, Tone tone, QWidget* parent)
    : QWidget(parent), text_(text), tone_(tone)
{
    setAccessibleName(text);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Badge::setText(const QString& text)
{
    text_ = text;
    setAccessibleName(text);
    updateGeometry();
    update();
}

void Badge::setTone(Tone tone)
{
    tone_ = tone;
    update();
}

int Badge::widthFor(const QString& text)
{
    const QFontMetrics fm(sans(kBadgePx, QFont::DemiBold, 0.5));
    return fm.horizontalAdvance(text) + kBadgePadX * 2;
}

QSize Badge::sizeHint() const
{
    return {widthFor(text_), kBadgeHeight};
}

void Badge::paint(QPainter& painter, const QRect& box, const QString& text, Tone tone,
                  ThemeMode mode)
{
    const ToneColours c = toneColours(tone, mode);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Wash inside, ink around, ink on top. The outline is what keeps the badge
    // legible on a row that is itself selected in the same wash.
    painter.setPen(QPen(c.ink, 1.0));
    painter.setBrush(c.wash);
    painter.drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 2.5, 2.5);

    painter.setFont(sans(kBadgePx, QFont::DemiBold, 0.5));
    painter.setPen(c.ink);
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
}

void Badge::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void Badge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    paint(p, QRect(0, (height() - kBadgeHeight) / 2, widthFor(text_), kBadgeHeight), text_, tone_,
          theme_);
}

// =============================================================================
// Banner
// =============================================================================

Banner::Banner(Tone tone, const QString& title, const QString& text, QWidget* parent)
    : QWidget(parent), tone_(tone)
{
    setObjectName(QStringLiteral("banner"));

    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(kBannerPad, kBannerPad - 2, kBannerPad, kBannerPad - 2);
    row_->setSpacing(10);

    glyph_ = new QLabel(this);
    glyph_->setFixedSize(kIconPx, kIconPx);
    row_->addWidget(glyph_, 0, Qt::AlignTop);

    auto* words = new QVBoxLayout;
    words->setContentsMargins(0, 0, 0, 0);
    words->setSpacing(3);

    title_ = new QLabel(title, this);
    title_->setObjectName(QStringLiteral("bannerTitle"));
    words->addWidget(title_);

    text_ = new QLabel(text, this);
    text_->setObjectName(QStringLiteral("bannerText"));
    text_->setWordWrap(true);
    words->addWidget(text_);

    row_->addLayout(words, 1);

    setAccessibleName(title);
    setAccessibleDescription(text);
}

Button* Banner::addButton(const QString& text)
{
    auto* action = new Button(ButtonRole::Secondary, text, std::nullopt, this);
    action->setControlSize(ControlSize::Compact);
    row_->addWidget(action, 0, Qt::AlignVCenter);
    return action;
}

void Banner::setTitle(const QString& title)
{
    title_->setText(title);
    setAccessibleName(title);
}

void Banner::setText(const QString& text)
{
    text_->setText(text);
    setAccessibleDescription(text);
}

void Banner::applyTheme(ThemeMode mode)
{
    theme_              = mode;
    const ToneColours c = toneColours(tone_, mode);

    // The glyph is the tone's second statement (§13): a triangle asks for care,
    // a ring only informs, a tick says the check passed.
    Glyph mark = Glyph::Info;
    if (tone_ == Tone::Warn || tone_ == Tone::Danger) mark = Glyph::Warning;
    if (tone_ == Tone::Ok) mark = Glyph::Check;
    glyph_->setPixmap(glyph_pixmap(mark, c.ink, kIconPx, devicePixelRatioF()));
    update();
}

void Banner::paintEvent(QPaintEvent*)
{
    const ToneColours c = toneColours(tone_, theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c.ink, 1.0));
    p.setBrush(c.wash);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
}

// =============================================================================
// ProgressStrip
// =============================================================================

ProgressStrip::ProgressStrip(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kStripH);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("Yükleniyor"));

    clock_ = new QTimer(this);
    clock_->setInterval(kStripTickMs);
    connect(clock_, &QTimer::timeout, this, [this] {
        phase_ = (phase_ + kStripStep) % std::max(1, width() + width() * kStripSegmentP / 100);
        update();
    });
}

void ProgressStrip::setActive(bool on)
{
    if (active_ == on) return;
    active_ = on;
    if (on) {
        // The first frame already shows a segment, a third of the way along,
        // rather than the sliver of one entering from the left: a strip that is
        // photographed the instant it starts — the living standard does exactly
        // that — has to look like what it is.
        phase_ = width() * (kStripSegmentP + 30) / 100;
        clock_->start();
    } else {
        clock_->stop();
    }
    update();
}

QSize ProgressStrip::sizeHint() const
{
    return {100, kStripH};
}

void ProgressStrip::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void ProgressStrip::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setActive(true);
}

void ProgressStrip::hideEvent(QHideEvent* event)
{
    setActive(false);
    QWidget::hideEvent(event);
}

void ProgressStrip::paintEvent(QPaintEvent*)
{
    if (!active_) return; // stopped, the strip is nothing; the content is what shows

    const Tokens& t   = tokensOf(theme_);
    const int segment = width() * kStripSegmentP / 100;
    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setBrush(t.accent);
    p.drawRoundedRect(QRectF(phase_ - segment, 0, segment, kStripH), 1.0, 1.0);
}

// =============================================================================
// Conversation — ThinkingDot, MessageBubble, Transcript, AttachmentChip,
//                ContextMeter
// =============================================================================

QString formatTokenCount(qint64 tokens)
{
    if (tokens < 1000) return QString::number(tokens);
    // One decimal up to a hundred thousand, none above it: `12.4 b` is a number
    // a reader uses, `128.0 b` is noise around a number they already know.
    if (tokens < 100000)
        return QStringLiteral("%1 b").arg(
            QString::number(static_cast<double>(tokens) / 1000.0, 'f', 1));
    return QStringLiteral("%1 b").arg(tokens / 1000);
}

QString formatByteCount(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024LL * 1024)
        return QStringLiteral("%1 KB").arg(
            QString::number(static_cast<double>(bytes) / 1024.0, 'f', 0));
    return QStringLiteral("%1 MB").arg(
        QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1));
}

// ---- ThinkingDot ------------------------------------------------------------

namespace {

/// `Düşünüyor · 4 s`, or the label alone before a turn is worth timing.
QString dotCaption(const QString& label, int elapsed)
{
    if (elapsed < 0) return label;
    return QStringLiteral("%1 · %2 s").arg(label).arg(elapsed);
}

} // namespace

ThinkingDot::ThinkingDot(QWidget* parent) : QWidget(parent), label_(tr("Düşünüyor"))
{
    setFixedHeight(kBoxSize);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(label_);

    clock_ = new QTimer(this);
    clock_->setInterval(kDotTickMs);
    connect(clock_, &QTimer::timeout, this, [this] {
        phase_ = (phase_ + 1) % kDotPhases;
        update();
    });
}

void ThinkingDot::setActive(bool on)
{
    if (active_ == on) return;
    active_ = on;
    if (on) {
        phase_ = 0;
        clock_->start();
    } else {
        clock_->stop();
        elapsed_ = -1;
    }
    update();
}

void ThinkingDot::setLabel(const QString& text)
{
    label_ = text;
    setAccessibleName(text);
    updateGeometry();
    update();
}

void ThinkingDot::setElapsedSeconds(int seconds)
{
    if (elapsed_ == seconds) return;
    elapsed_ = seconds;
    updateGeometry();
    update();
}

void ThinkingDot::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QSize ThinkingDot::sizeHint() const
{
    const QString words = dotCaption(label_, elapsed_);
    const int dots      = 3 * kDotSize + 2 * kDotGap;
    return {dots + kBoxGap + QFontMetrics(sans(kBadgePx + 1)).horizontalAdvance(words), kBoxSize};
}

void ThinkingDot::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setActive(true);
}

void ThinkingDot::hideEvent(QHideEvent* event)
{
    setActive(false);
    QWidget::hideEvent(event);
}

void ThinkingDot::paintEvent(QPaintEvent*)
{
    if (!active_) return;

    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    const int mid = height() / 2;
    for (int i = 0; i < 3; ++i) {
        // The lifted dot is the accent at full strength and the others are the
        // same colour held back, so the motion reads as one dot travelling
        // rather than three lights blinking.
        const bool up = i == phase_;
        QColor ink    = t.accent;
        if (!up) ink.setAlphaF(0.45F);
        p.setBrush(ink);
        const int x = i * (kDotSize + kDotGap);
        const int y = mid - kDotSize / 2 - (up ? kDotRise : 0);
        p.drawEllipse(QRect(x, y, kDotSize, kDotSize));
    }

    p.setFont(sans(kBadgePx + 1));
    p.setPen(t.textDim);
    const int words = 3 * kDotSize + 2 * kDotGap + kBoxGap;
    p.drawText(QRect(words, 0, width() - words, height()), Qt::AlignVCenter | Qt::AlignLeft,
               dotCaption(label_, elapsed_));
}

// ---- MessageBubble ----------------------------------------------------------

namespace {

/// What a speaker is CALLED and what tone it carries. One table, so the head, the
/// wash and the badge cannot disagree about who is talking.
///
/// THE WORDS ARE BUILT WITH `QCoreApplication::translate`, not handed out as
/// `const char*` for a caller to `tr()`. `tr(variable)` is invisible to
/// `lupdate`, so a table of raw pointers would have quietly kept four visible
/// strings out of `kentos_tr.ts` and out of Article 6.9's reach.
struct SpeakerFace
{
    QString who;
    QString mark; ///< the badge's text, empty for none
    Tone tone;
};

SpeakerFace faceOf(Speaker speaker)
{
    const auto say = [](const char* text) {
        return QCoreApplication::translate("kentos::app::MessageBubble", text);
    };
    switch (speaker) {
    case Speaker::Person: return {say("Siz"), QString(), Tone::Neutral};
    // `ÖNERİ` IS NOT OPTIONAL AND NOT CONDITIONAL. ai.md R17 requires every piece
    // of model output to be labelled a suggestion, and P4 forbids the words that
    // would make it sound like anything else. The badge is built with the bubble,
    // before any text has arrived, so there is no state in which a model turn is
    // on screen without it.
    case Speaker::Model: return {say("Model"), say("ÖNERİ"), Tone::Accent};
    case Speaker::ToolResult: return {say("Araç"), say("SONUÇ"), Tone::Neutral};
    case Speaker::Notice: return {say("KentOSCad"), QString(), Tone::Warn};
    }
    return {QString(), QString(), Tone::Neutral};
}

} // namespace

MessageBubble::MessageBubble(Speaker speaker, QWidget* parent) : QWidget(parent), speaker_(speaker)
{
    setObjectName(QStringLiteral("messageBubble"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    column_ = new QVBoxLayout(this);
    column_->setContentsMargins(kBubblePad, kBubblePad - 2, kBubblePad, kBubblePad - 2);
    column_->setSpacing(6);

    const SpeakerFace face = faceOf(speaker_);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(6);
    who_ = new QLabel(face.who, this);
    who_->setObjectName(QStringLiteral("bubbleWho"));
    head->addWidget(who_);
    if (!face.mark.isEmpty()) {
        mark_ = new Badge(face.mark, face.tone, this);
        head->addWidget(mark_);
    }
    head->addStretch(1);
    column_->addLayout(head);

    body_ = new QLabel(this);
    body_->setObjectName(QStringLiteral("bubbleBody"));
    body_->setWordWrap(true);
    body_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    // AN EMPTY BODY TAKES NO ROOM. A bubble that is only waiting, or only a
    // refusal, would otherwise carry a blank line where its words will go, and
    // the head would float away from what it introduces.
    body_->setVisible(false);
    // MONO FOR A TOOL'S OUTPUT, because it is a command's echo: aligned columns,
    // coordinates and object counts, and prose type would break every one of them.
    if (speaker_ == Speaker::ToolResult) body_->setObjectName(QStringLiteral("bubbleBodyMono"));
    column_->addWidget(body_);

    setAccessibleName(tr("%1 iletisi").arg(face.who));
}

void MessageBubble::setText(const QString& text)
{
    body_->setText(text);
    body_->setVisible(!text.isEmpty());
}

void MessageBubble::appendText(const QString& text)
{
    if (text.isEmpty()) return;
    body_->setText(body_->text() + text);
    body_->setVisible(true);
}

QString MessageBubble::text() const
{
    return body_->text();
}

void MessageBubble::appendReasoning(const QString& text)
{
    if (text.isEmpty()) return;
    reasoning_ += text;

    if (fold_ == nullptr) {
        // THE BUTTON GOES IN THE COLUMN, not in a row widget of its own. A
        // wrapper `QWidget` here painted a grey band across the whole bubble —
        // its own ground, inherited from the panel's rule — behind a button that
        // is 140 px wide. One widget, aligned left, and the bubble's wash is the
        // only ground in the bubble.
        foldButton_ =
            new Button(ButtonRole::Ghost, tr("Düşünme süreci"), Glyph::ChevronRight, this);
        foldButton_->setControlSize(ControlSize::Compact);

        fold_ = new QLabel(this);
        fold_->setObjectName(QStringLiteral("bubbleReasoning"));
        fold_->setWordWrap(true);
        fold_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fold_->setVisible(false);

        // THE FOLD GOES ABOVE THE ANSWER, where the thinking happened. A reader
        // scrolling a finished turn wants the answer at the bottom, next to what
        // they type; a reader watching one arrive wants the thinking where it is
        // being written. Both want it in the order it occurred.
        const int at = column_->indexOf(body_);
        column_->insertWidget(at, foldButton_, 0, Qt::AlignLeft);
        column_->insertWidget(at + 1, fold_);

        connect(foldButton_, &QPushButton::clicked, this,
                [this] { setReasoningOpen(!fold_->isVisible()); });
        applyTheme(theme_);
    }
    fold_->setText(reasoning_);
}

void MessageBubble::setReasoningOpen(bool open)
{
    if (fold_ == nullptr) return;
    fold_->setVisible(open);
    foldButton_->setIcon(icon(open ? Glyph::ChevronDown : Glyph::ChevronRight,
                              tokensOf(theme_).textDim, tokensOf(theme_).accent, kIconPx));
}

bool MessageBubble::hasReasoning() const noexcept
{
    return fold_ != nullptr;
}

void MessageBubble::setWaiting(bool waiting)
{
    if (!waiting) {
        if (dots_ != nullptr) dots_->setVisible(false);
        return;
    }
    if (dots_ == nullptr) {
        dots_ = new ThinkingDot(this);
        column_->addWidget(dots_);
        applyTheme(theme_);
    }
    dots_->setVisible(true);
}

void MessageBubble::setNote(const QString& text, Tone tone)
{
    if (note_ == nullptr) {
        note_ = new QLabel(this);
        note_->setObjectName(QStringLiteral("formHelp"));
        note_->setWordWrap(true);
        column_->addWidget(note_);
    }
    note_->setText(text);
    note_->setVisible(!text.isEmpty());
    // The SAME name and the SAME property `FormRow` uses, so one stylesheet rule
    // colours the note under a field and the note under a bubble.
    restyle(note_, "tone", toneProperty(tone));
}

void MessageBubble::setFooter(QWidget* footer)
{
    if (footer_ != nullptr) {
        column_->removeWidget(footer_);
        footer_->deleteLater();
    }
    footer_ = footer;
    if (footer_ == nullptr) return;
    footer_->setParent(this);
    column_->addWidget(footer_);
    applyTheme(theme_);
}

void MessageBubble::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    if (fold_ != nullptr) setReasoningOpen(fold_->isVisible());
    applyThemeToChildren(this, mode);
    update();
}

void MessageBubble::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // THE WASH SAYS WHO IS TALKING and the outline keeps the bubble legible on a
    // panel that is itself the same family of greys. A person's turn is the
    // accent wash — it is the one thing on the transcript they wrote themselves;
    // the model's is the panel's own raised ground, because an answer labelled
    // ÖNERİ must not also be dressed as the program's own voice.
    QColor wash = t.bgRaised;
    QColor edge = t.border;
    switch (speaker_) {
    case Speaker::Person:
        wash = t.accentWash;
        edge = t.accentEdge;
        break;
    case Speaker::Model: break;
    case Speaker::ToolResult: wash = t.bgSunken; break;
    case Speaker::Notice:
        wash = t.warnWash;
        edge = t.warn;
        break;
    }

    p.setPen(QPen(edge, 1.0));
    p.setBrush(wash);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kBubbleRadius, kBubbleRadius);
}

// ---- Transcript -------------------------------------------------------------

Transcript::Transcript(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("transcript"));

    auto* frame = new QVBoxLayout(this);
    frame->setContentsMargins(0, 0, 0, 0);

    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("transcriptScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frame->addWidget(scroll_);

    column_ = new QWidget(scroll_);
    column_->setObjectName(QStringLiteral("transcriptColumn"));
    stack_ = new QVBoxLayout(column_);
    stack_->setContentsMargins(12, 12, 12, 12);
    stack_->setSpacing(kBubbleGap);

    placeholder_ = new QLabel(column_);
    placeholder_->setObjectName(QStringLiteral("formHelp"));
    placeholder_->setWordWrap(true);
    placeholder_->setAlignment(Qt::AlignCenter);
    stack_->addWidget(placeholder_);
    stack_->addStretch(1);

    scroll_->setWidget(column_);
}

void Transcript::append(QWidget* bubble)
{
    const bool follow = atEnd();
    placeholder_->setVisible(false);
    bubble->setParent(column_);
    // Before the trailing stretch, which is what keeps a short transcript at the
    // top of the panel rather than centred in it.
    stack_->insertWidget(stack_->count() - 1, bubble);
    bubble->show();
    if (follow) toEnd();
}

QWidget* Transcript::last() const noexcept
{
    for (int i = stack_->count() - 2; i >= 0; --i) {
        QWidget* w = stack_->itemAt(i)->widget();
        if (w != nullptr && w != placeholder_) return w;
    }
    return nullptr;
}

int Transcript::count() const
{
    int n = 0;
    for (int i = 0; i < stack_->count(); ++i) {
        QWidget* w = stack_->itemAt(i)->widget();
        if (w != nullptr && w != placeholder_) ++n;
    }
    return n;
}

void Transcript::clear()
{
    for (int i = stack_->count() - 2; i >= 0; --i) {
        QWidget* w = stack_->itemAt(i)->widget();
        if (w == nullptr || w == placeholder_) continue;
        stack_->removeWidget(w);
        w->deleteLater();
    }
    placeholder_->setVisible(true);
}

void Transcript::bumped()
{
    if (atEnd()) toEnd();
}

bool Transcript::atEnd() const
{
    const QScrollBar* bar = scroll_->verticalScrollBar();
    return bar->value() >= bar->maximum() - kFollowSlack;
}

void Transcript::toEnd()
{
    // AFTER THE LAYOUT, NOT BEFORE IT. The bubble that was just added has no
    // height until the layout has run, so scrolling now would scroll to the old
    // maximum and stop one bubble short — which looked exactly like a transcript
    // that does not follow.
    QTimer::singleShot(0, this, [this] {
        QScrollBar* bar = scroll_->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void Transcript::toStart()
{
    QTimer::singleShot(0, this, [this] { scroll_->verticalScrollBar()->setValue(0); });
}

void Transcript::setPlaceholder(const QString& text)
{
    placeholder_->setText(text);
}

void Transcript::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    applyThemeToChildren(this, mode);
    update();
}

// ---- AttachmentChip ---------------------------------------------------------

AttachmentChip::AttachmentChip(const QString& name, qint64 bytes, const QString& media,
                               QWidget* parent)
    : QWidget(parent), name_(name), bytes_(bytes)
{
    setObjectName(QStringLiteral("attachmentChip"));
    setFixedHeight(kAttachH);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // The glyph is the media type's, from the type alone: a chip must not have to
    // guess from an extension, and a file the user renamed is still a PNG.
    glyph_ = Glyph::Document;
    if (media.startsWith(QStringLiteral("image/")))
        glyph_ = Glyph::Palette;
    else if (media.startsWith(QStringLiteral("text/")))
        glyph_ = Glyph::Text;

    // The name is elided in the middle: the beginning says what it is and the
    // extension says what kind, and a tail-elided name loses the second.
    shown_ = QFontMetrics(sans(kBadgePx + 2)).elidedText(name_, Qt::ElideMiddle, 160);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kAttachPadX, 0, 4, 0);
    row->setSpacing(4);
    row->addStretch(1);
    drop_ = new Button(Glyph::Close, tr("Eki kaldır"), this);
    drop_->setFixedSize(kBoxSize + 4, kBoxSize + 4);
    row->addWidget(drop_);
    connect(drop_, &QPushButton::clicked, this, &AttachmentChip::removeRequested);

    setAccessibleName(tr("Ek: %1, %2").arg(name_, formatByteCount(bytes_)));
    setToolTip(accessibleName());
}

void AttachmentChip::setRemovable(bool on)
{
    drop_->setVisible(on);
    updateGeometry();
}

void AttachmentChip::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    applyThemeToChildren(this, mode);
    update();
}

QSize AttachmentChip::sizeHint() const
{
    const QFontMetrics words(sans(kBadgePx + 2));
    const int text =
        words.horizontalAdvance(shown_) + 6 + words.horizontalAdvance(formatByteCount(bytes_));
    const int drop = drop_->isVisibleTo(this) ? kBoxSize + 8 : 0;
    return {kAttachPadX + kIconPx + 6 + text + drop + 4, kAttachH};
}

void AttachmentChip::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(t.border, 1.0));
    p.setBrush(t.bgInput);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);

    const int mid = height() / 2;
    p.drawPixmap(QRect(kAttachPadX, mid - kIconPx / 2, kIconPx, kIconPx),
                 glyph_pixmap(glyph_, t.textDim, kIconPx, devicePixelRatioF()));

    const QFontMetrics words(sans(kBadgePx + 2));
    int x = kAttachPadX + kIconPx + 6;
    p.setFont(sans(kBadgePx + 2));
    p.setPen(t.text);
    p.drawText(QRect(x, 0, words.horizontalAdvance(shown_), height()),
               Qt::AlignVCenter | Qt::AlignLeft, shown_);
    x += words.horizontalAdvance(shown_) + 6;
    p.setPen(t.textFaint);
    p.drawText(QRect(x, 0, words.horizontalAdvance(formatByteCount(bytes_)), height()),
               Qt::AlignVCenter | Qt::AlignLeft, formatByteCount(bytes_));
}

// ---- ContextMeter -----------------------------------------------------------

ContextMeter::ContextMeter(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("contextMeter"));
    setFixedHeight(kMeterH);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(tr("Bağlam kullanımı"));
}

void ContextMeter::setUsage(qint64 used, qint64 window, bool measured)
{
    used_     = used;
    window_   = window;
    measured_ = measured;
    setToolTip(caption());
    update();
}

QString ContextMeter::caption() const
{
    // THE SENTENCE NAMES WHICH NUMBER IT IS, always. `tahmin` while the program
    // is counting for itself, `ölçüldü` once the provider has reported — the one
    // distinction a context readout must never blur (`ai/chat.hpp`,
    // `Conversation::reconcile`).
    const QString kind = measured_ ? tr("ölçüldü") : tr("tahmin");
    // THE WINDOW FACT FIRST, then whose number it is: a reader scanning the line
    // needs to know there is no denominator before they weigh the numerator.
    if (window_ <= 0)
        return tr("%1 belirteç · pencere bilinmiyor · %2").arg(formatTokenCount(used_), kind);
    return tr("%1 / %2 belirteç · %3")
        .arg(formatTokenCount(used_), formatTokenCount(window_), kind);
}

void ContextMeter::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QSize ContextMeter::sizeHint() const
{
    return {180, kMeterH};
}

void ContextMeter::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setFont(sans(kMeterPx));
    p.setPen(measured_ ? t.textDim : t.textFaint);
    p.drawText(QRect(0, 0, width(), height() - kMeterBarH - 2), Qt::AlignVCenter | Qt::AlignLeft,
               caption());

    // NO BAR WITHOUT A DENOMINATOR. An unknown window is the ordinary case for a
    // cloud endpoint and a bar drawn anyway would have to invent the one number
    // the user is deciding on.
    if (window_ <= 0) return;

    const QRectF track(0, height() - kMeterBarH, width(), kMeterBarH);
    p.setPen(Qt::NoPen);
    p.setBrush(t.bgSunken);
    p.drawRoundedRect(track, kMeterBarH / 2.0, kMeterBarH / 2.0);

    const double fill =
        std::clamp(static_cast<double>(used_) / static_cast<double>(window_), 0.0, 1.0);
    // The colour is the statement: accent while there is room, warn past four
    // fifths, danger when the next turn will not fit. A number a user reads to
    // decide whether to start a new chat should not need arithmetic.
    QColor ink = t.accent;
    if (fill >= 0.95)
        ink = t.danger;
    else if (fill >= 0.8)
        ink = t.warn;
    p.setBrush(ink);
    p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * fill, track.height()),
                      kMeterBarH / 2.0, kMeterBarH / 2.0);
}

// =============================================================================
// FormRow
// =============================================================================

FormRow::FormRow(const QString& label, QWidget* editor, QWidget* parent)
    : QWidget(parent), label_(label), editor_(editor)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);

    head_ = new QHBoxLayout;
    head_->setContentsMargins(0, 0, 0, 0);
    head_->setSpacing(6);

    name_ = new QLabel(this);
    name_->setObjectName(QStringLiteral("formRowLabel"));
    name_->setTextFormat(Qt::RichText);
    head_->addWidget(name_);
    head_->addStretch(1);

    caption_ = new Badge(QString(), Tone::Neutral, this);
    caption_->setVisible(false);
    head_->addWidget(caption_);
    column->addLayout(head_);

    editor_->setParent(this);
    editor_->setAccessibleName(label);
    column->addWidget(editor_);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("formHelp"));
    note_->setWordWrap(true);
    note_->setVisible(false);
    column->addWidget(note_);

    refreshLabel();
}

void FormRow::setRequired(bool on)
{
    required_ = on;
    refreshLabel();
}

void FormRow::setCaption(const QString& text, Tone tone)
{
    caption_->setText(text);
    caption_->setTone(tone);
    caption_->setVisible(!text.isEmpty());
}

void FormRow::setHelp(const QString& text)
{
    note_->setText(text);
    note_->setVisible(!text.isEmpty());
    restyle(note_, "tone", QVariant());
    restyle(editor_, "state", QVariant());
    for (QWidget* inner : editor_->findChildren<QWidget*>())
        if (inner->property("state").toString() == QStringLiteral("invalid"))
            restyle(inner, "state", QVariant());
}

void FormRow::setError(const QString& text)
{
    note_->setText(text);
    note_->setVisible(!text.isEmpty());
    restyle(note_, "tone", text.isEmpty() ? QVariant() : QStringLiteral("danger"));

    // The editor says it too, through the `state` the stylesheet reads — on the
    // container and on the line inside it, because the border is the one's and
    // the ink is the other's.
    const QVariant state = text.isEmpty() ? QVariant() : QStringLiteral("invalid");
    restyle(editor_, "state", state);
    for (QWidget* inner : editor_->findChildren<QWidget*>())
        if (inner->objectName() == QStringLiteral("fieldLine")) restyle(inner, "state", state);
}

void FormRow::refreshLabel()
{
    const Tokens& t = tokensOf(theme_);
    // The star is the accent, and it is a mark rather than a colour on the
    // label: `Ada No *` reads as required with the star alone (§13).
    name_->setText(required_ ? QStringLiteral("%1 <span style=\"color:%2\">*</span>")
                                   .arg(label_.toHtmlEscaped(), t.accent.name())
                             : label_.toHtmlEscaped());
}

void FormRow::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    refreshLabel();
    applyThemeToChildren(this, mode);
}

// =============================================================================
// FormSection
// =============================================================================

FormSection::FormSection(const QString& title, const QString& note, QWidget* parent)
    : QWidget(parent), title_(title), note_(note)
{
    setFixedHeight(kSectionH);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(title);
}

void FormSection::setNote(const QString& note)
{
    note_ = note;
    update();
}

void FormSection::setTitle(const QString& title)
{
    title_ = title;
    update();
}

QSize FormSection::sizeHint() const
{
    return {200, kSectionH};
}

void FormSection::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void FormSection::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);

    // design.md §3: the group label is 10–10.5 px, 600, tracked, UPPERCASE —
    // and the uppercase is the caller's, never `toUpper()` (P4).
    p.setFont(sans(kSectionPx, QFont::DemiBold, 1.0));
    p.setPen(t.textFaint);
    const QFontMetrics fm(p.font());
    const int titleW = fm.horizontalAdvance(title_);
    p.drawText(QRect(0, 0, titleW, height()), Qt::AlignVCenter | Qt::AlignLeft, title_);

    int noteW = 0;
    if (!note_.isEmpty()) {
        p.setFont(sans(kSectionPx));
        noteW = QFontMetrics(p.font()).horizontalAdvance(note_);
        p.drawText(QRect(width() - noteW, 0, noteW, height()), Qt::AlignVCenter | Qt::AlignRight,
                   note_);
    }

    // The rule runs from the title to the note, and stops short of both.
    const int from = titleW + 10;
    const int to   = width() - (noteW > 0 ? noteW + 10 : 0);
    if (to > from) p.fillRect(QRect(from, height() / 2, to - from, 1), t.lineSoft);
}

// =============================================================================
// The living standard
// =============================================================================

namespace {

/// Three parcels for the sheet's table: what §9's grid looks like with a figure
/// column, a word column, a NULL and an edited cell — no document behind it.
class SheetRows : public QAbstractTableModel
{
public:
    explicit SheetRows(QObject* parent) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 3;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 4;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal) return {};
        static const char* const kNames[] = {"fid", "ada_no", "alan_m2", "nitelik"};
        if (role == Qt::DisplayRole) return QString::fromUtf8(kNames[section]);
        if (role == Qt::TextAlignmentRole)
            return QVariant(static_cast<int>((section < 3 ? Qt::AlignRight : Qt::AlignLeft) |
                                             Qt::AlignVCenter));
        return {};
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        static const char* const kCells[3][4] = {{"4126", "1284", "2 940.12", "Arsa"},
                                                 {"4127", "1284", "3 105.80", "Arsa"},
                                                 {"4128", "1285", "3 482.64", ""}};
        if (role == Qt::DisplayRole) return QString::fromUtf8(kCells[index.row()][index.column()]);
        if (role == Qt::TextAlignmentRole)
            return QVariant(static_cast<int>((index.column() < 3 ? Qt::AlignRight : Qt::AlignLeft) |
                                             Qt::AlignVCenter));
        if (role == GridRole::Null) return index.row() == 2 && index.column() == 3;
        if (role == GridRole::Edited) return index.row() == 1 && index.column() == 2;
        return {};
    }
};

/// Tags one sheet item so `componentSheetInventory` can read it back.
template<class W> W* shown(W* w, const char* kind, const char* state)
{
    w->setProperty(
        "sheet", QStringLiteral("%1 · %2").arg(QString::fromUtf8(kind), QString::fromUtf8(state)));
    return w;
}

} // namespace

QWidget* buildComponentSheet(ThemeMode mode, QWidget* parent)
{
    const Tokens& t = tokensOf(mode);

    auto* sheet = new QWidget(parent);
    sheet->setObjectName(QStringLiteral("componentSheet"));
    auto* page = new QVBoxLayout(sheet);
    page->setContentsMargins(24, 20, 24, 20);
    page->setSpacing(14);

    const auto caption = [sheet](const QString& text) {
        auto* label = new QLabel(text, sheet);
        label->setObjectName(QStringLiteral("formRowLabel"));
        return label;
    };

    // ---- buttons ----
    page->addWidget(
        new FormSection(QStringLiteral("BUTONLAR — HİYERARŞİ"),
                        QStringLiteral("Bir ekranda yalnızca tek birincil buton bulunur"), sheet));
    {
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(8);

        // Pointers first, then the two enums: the analyser counts the padding a
        // byte-sized enum between two pointers costs, and it is right to.
        struct Row
        {
            const char* caption;
            const char* text;
            const char* kind;
            Glyph glyph;
            ButtonRole role;
        };

        const Row rows[] = {
            {"Birincil", "Kaydet", "primary", Glyph::Save, ButtonRole::Primary},
            {"İkincil", "Uygula", "secondary", Glyph::Check, ButtonRole::Secondary},
            {"Hayalet", "Sıfırla", "ghost", Glyph::Undo, ButtonRole::Ghost},
            {"Yıkıcı", "Kaydı sil", "danger", Glyph::Trash, ButtonRole::Danger},
            {"Kip anahtarı", "Düzenleme", "mode", Glyph::Pencil, ButtonRole::Mode},
        };
        int column = 0;
        for (const Row& row : rows) {
            grid->addWidget(caption(QString::fromUtf8(row.caption)), 0, column);
            auto* on = shown(new Button(row.role, QString::fromUtf8(row.text), row.glyph, sheet),
                             row.kind, "etkin");
            if (row.role == ButtonRole::Mode) on->setChecked(true);
            grid->addWidget(on, 1, column);
            auto* off = shown(new Button(row.role, QString::fromUtf8(row.text), row.glyph, sheet),
                              row.kind, "devre dışı");
            off->setEnabled(false);
            grid->addWidget(off, 2, column);
            ++column;
        }
        grid->addWidget(caption(QStringLiteral("İkon")), 0, column);
        grid->addWidget(
            shown(new Button(Glyph::Tune, QStringLiteral("Ayarlar"), sheet), "iconButton", "etkin"),
            1, column);
        auto* iconOff = shown(new Button(Glyph::Tune, QStringLiteral("Ayarlar"), sheet),
                              "iconButton", "devre dışı");
        iconOff->setEnabled(false);
        grid->addWidget(iconOff, 2, column);

        // The three heights, side by side, so a reader sees the whole scale.
        ++column;
        grid->addWidget(caption(QStringLiteral("Boy 24 / 30 / 36")), 0, column);
        auto* sizes = new QHBoxLayout;
        for (const ControlSize size :
             {ControlSize::Compact, ControlSize::Regular, ControlSize::Large}) {
            auto* b =
                shown(new Button(ButtonRole::Secondary, QStringLiteral("Boy"), std::nullopt, sheet),
                      "secondary", sizeName(size));
            b->setControlSize(size);
            sizes->addWidget(b);
        }
        grid->addLayout(sizes, 1, column, 2, 1, Qt::AlignTop);
        page->addLayout(grid);
    }

    // ---- inputs ----
    page->addWidget(
        new FormSection(QStringLiteral("METİN VE SAYI GİRDİLERİ — DURUMLAR"), QString(), sheet));
    {
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(10);

        const auto field = [sheet](FieldSpec spec, const QString& value, FieldState state,
                                   std::optional<Glyph> lead) {
            spec.frame = FieldFrame::Box;
            if (lead) spec.glyph = lead;
            auto* f = new Field(spec, sheet);
            f->setFixedHeight(static_cast<int>(ControlSize::Regular));
            f->setValue(value);
            f->setState(state);
            return f;
        };

        FieldSpec metres = decimal_of(2);
        metres.suffix    = QStringLiteral("m");
        FieldSpec area   = decimal_of(2);
        area.suffix      = QStringLiteral("m²");

        auto* plain = new FormRow(
            QStringLiteral("Varsayılan"),
            shown(field(metres, QStringLiteral("48.20"), FieldState::Normal, Glyph::Ruler), "girdi",
                  "varsayılan"),
            sheet);
        auto* changed = new FormRow(
            QStringLiteral("Değiştirilmiş"),
            shown(field(area, QStringLiteral("3480.00"), FieldState::Changed, std::nullopt),
                  "girdi", "değiştirilmiş"),
            sheet);
        changed->setCaption(QStringLiteral("KAYDEDİLMEDİ"), Tone::Warn);

        auto* invalid = new FormRow(
            QStringLiteral("Hatalı"),
            shown(field(field_of(FieldKind::Text), QString(), FieldState::Invalid, std::nullopt),
                  "girdi", "hatalı"),
            sheet);
        invalid->setRequired(true);
        invalid->setCaption(QStringLiteral("ZORUNLU"), Tone::Danger);
        invalid->setError(QStringLiteral("Değer girilmedi"));

        FieldSpec code = field_of(FieldKind::Text);
        auto* readOnly = new FormRow(
            QStringLiteral("Salt okunur"),
            shown(field(code, QStringLiteral("4128 9021"), FieldState::ReadOnly, Glyph::Lock),
                  "girdi", "salt okunur"),
            sheet);
        readOnly->setCaption(QStringLiteral("SALT OKUNUR"), Tone::Neutral);

        auto* disabledField =
            field(field_of(FieldKind::Text), QString(), FieldState::Normal, std::nullopt);
        disabledField->setEnabled(false);
        auto* disabled = new FormRow(QStringLiteral("Devre dışı"),
                                     shown(disabledField, "girdi", "devre dışı"), sheet);

        auto* derived = new FormRow(
            QStringLiteral("Türetilmiş"),
            shown(field(metres, QStringLiteral("241.08"), FieldState::Derived, std::nullopt),
                  "girdi", "türetilmiş"),
            sheet);
        derived->setCaption(QStringLiteral("HESAP"), Tone::Accent);
        derived->setHelp(QStringLiteral("Ölçü alanından 2.64 m² sapma — tolerans içinde."));

        auto* combo =
            new FormRow(QStringLiteral("Açılır liste"),
                        shown(field(combo_of({QStringLiteral("Ayrık"), QStringLiteral("Blok"),
                                              QStringLiteral("İkiz")}),
                                    QStringLiteral("Ayrık"), FieldState::Normal, std::nullopt),
                              "girdi", "açılır liste"),
                        sheet);

        FieldSpec dateSpec = field_of(FieldKind::Date);
        auto* date         = new FormRow(
            QStringLiteral("Tarih"),
            shown(field(dateSpec, QStringLiteral("2019-03-14"), FieldState::Normal, std::nullopt),
                          "girdi", "tarih"),
            sheet);

        grid->addWidget(plain, 0, 0);
        grid->addWidget(changed, 0, 1);
        grid->addWidget(invalid, 0, 2);
        grid->addWidget(readOnly, 1, 0);
        grid->addWidget(disabled, 1, 1);
        grid->addWidget(derived, 1, 2);
        // The list on its own, outside a field: the component every drop-down in
        // the program is an instance of.
        auto* list = shown(new ComboBox(sheet), "açılır liste", "3 seçenek");
        list->addItems({QStringLiteral("Ayrık nizam"), QStringLiteral("Blok nizam"),
                        QStringLiteral("İkiz nizam")});
        auto* listRow = new FormRow(QStringLiteral("Açılır liste (bileşen)"), list, sheet);

        grid->addWidget(combo, 2, 0);
        grid->addWidget(date, 2, 1);
        grid->addWidget(listRow, 2, 2);

        // FROM THE SCENE: a coordinate and an object, each typed or picked. The
        // second is shown armed, so the pressed pick button is on the sheet.
        auto* point = new FormRow(
            QStringLiteral("Nokta (sahneden)"),
            shown(field(field_of(FieldKind::Point), QStringLiteral("485320.150,4310220.000"),
                        FieldState::Normal, std::nullopt),
                  "girdi", "nokta"),
            sheet);
        point->setHelp(QStringLiteral("x,y yazın ya da nişan düğmesiyle tuvalden tıklayın."));
        auto* objectField =
            field(field_of(FieldKind::Object), QString(), FieldState::Normal, std::nullopt);
        objectField->setPicking(true);
        auto* object = new FormRow(QStringLiteral("Nesne (sahneden, seçiliyor)"),
                                   shown(objectField, "girdi", "nesne"), sheet);
        object->setHelp(QStringLiteral("Nesne kimliğini yazın ya da tuvalden bir nesne tıklayın."));
        grid->addWidget(point, 3, 0);
        grid->addWidget(object, 3, 1);
        page->addLayout(grid);
    }

    // ---- selection ----
    page->addWidget(new FormSection(QStringLiteral("SEÇİM BİLEŞENLERİ"), QString(), sheet));
    {
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(8);

        auto* boxes = new QVBoxLayout;
        boxes->addWidget(caption(QStringLiteral("Onay kutusu")));
        auto* checked =
            shown(new CheckBox(QStringLiteral("Köşe noktası"), sheet), "onay", "işaretli");
        checked->setChecked(true);
        boxes->addWidget(checked);
        auto* partial =
            shown(new CheckBox(QStringLiteral("Segment orta noktası"), sheet), "onay", "kısmi");
        partial->setTristate(true);
        partial->setCheckState(Qt::PartiallyChecked);
        boxes->addWidget(partial);
        auto* boxOff = shown(new CheckBox(QStringLiteral("Kesişim (devre dışı)"), sheet), "onay",
                             "devre dışı");
        boxOff->setEnabled(false);
        boxes->addWidget(boxOff);
        boxes->addStretch(1);
        grid->addLayout(boxes, 0, 0);

        auto* radios     = new QVBoxLayout;
        auto* radioGroup = new QWidget(sheet);
        auto* radioCol   = new QVBoxLayout(radioGroup);
        radioCol->setContentsMargins(0, 0, 0, 0);
        radios->addWidget(caption(QStringLiteral("Radyo")));
        auto* r1 =
            shown(new RadioButton(QStringLiteral("Ayrık nizam"), radioGroup), "radyo", "seçili");
        r1->setChecked(true);
        radioCol->addWidget(r1);
        radioCol->addWidget(shown(new RadioButton(QStringLiteral("Blok nizam"), radioGroup),
                                  "radyo", "seçili değil"));
        auto* r3 =
            shown(new RadioButton(QStringLiteral("İkiz nizam"), radioGroup), "radyo", "devre dışı");
        r3->setEnabled(false);
        radioCol->addWidget(r3);
        radios->addWidget(radioGroup);
        radios->addStretch(1);
        grid->addLayout(radios, 0, 1);

        auto* switches = new QVBoxLayout;
        switches->addWidget(caption(QStringLiteral("Anahtar")));
        for (const char* label : {"Topoloji denetimi", "Otomatik kaydet", "3B görünüm (kilitli)"}) {
            auto* line    = new QHBoxLayout;
            auto* sw      = new ToggleSwitch(sheet);
            const bool on = std::string_view(label) == "Topoloji denetimi";
            sw->setChecked(on);
            if (std::string_view(label).find("kilitli") != std::string_view::npos)
                sw->setEnabled(false);
            shown(sw, "anahtar", on ? "açık" : (sw->isEnabled() ? "kapalı" : "devre dışı"));
            line->addWidget(sw);
            line->addWidget(caption(QString::fromUtf8(label)));
            line->addStretch(1);
            switches->addLayout(line);
        }
        switches->addStretch(1);
        grid->addLayout(switches, 0, 2);

        auto* misc = new QVBoxLayout;
        misc->addWidget(caption(QStringLiteral("Segment · kaydırıcı · etiket")));
        auto* segment = shown(new Segment(sheet), "segment", "3 seçenek");
        segment->addOption(QStringLiteral("mm"));
        segment->addOption(QStringLiteral("harita"));
        segment->addOption(QStringLiteral("px"));
        misc->addWidget(segment, 0, Qt::AlignLeft);
        auto* slider = shown(new Slider(sheet), "kaydırıcı", "%62");
        slider->setRange(0, 100);
        slider->setAffixes(QStringLiteral("%"), QString());
        slider->setValue(62);
        misc->addWidget(slider);
        auto* chips = new QHBoxLayout;
        chips->setSpacing(6);
        auto* konut = shown(new Chip(QStringLiteral("Konut"), t.warn, sheet), "etiket", "seçili");
        konut->setChecked(true);
        chips->addWidget(konut);
        chips->addWidget(
            shown(new Chip(QStringLiteral("Ticaret"), t.accent, sheet), "etiket", "seçili değil"));
        chips->addWidget(
            shown(new Chip(QStringLiteral("Yeşil alan"), t.ok, sheet), "etiket", "seçili değil"));
        chips->addWidget(shown(Chip::overflow(2, sheet), "etiket", "taşma"));
        chips->addStretch(1);
        misc->addLayout(chips);
        misc->addStretch(1);
        grid->addLayout(misc, 0, 3);
        page->addLayout(grid);
    }

    // ---- annotation ----
    page->addWidget(
        new FormSection(QStringLiteral("ROZET · UYARI ŞERİDİ · YÜKLENİYOR"), QString(), sheet));
    {
        auto* badges = new QHBoxLayout;
        badges->setSpacing(8);
        badges->addWidget(
            shown(new Badge(QStringLiteral("HESAP"), Tone::Accent, sheet), "rozet", "accent"));
        badges->addWidget(
            shown(new Badge(QStringLiteral("BOŞ"), Tone::Warn, sheet), "rozet", "warn"));
        badges->addWidget(
            shown(new Badge(QStringLiteral("ZORUNLU"), Tone::Danger, sheet), "rozet", "danger"));
        badges->addWidget(shown(new Badge(QStringLiteral("SALT OKUNUR"), Tone::Neutral, sheet),
                                "rozet", "neutral"));
        badges->addWidget(
            shown(new Badge(QStringLiteral("GEÇERLİ"), Tone::Ok, sheet), "rozet", "ok"));
        badges->addStretch(1);
        page->addLayout(badges);

        auto* banner =
            shown(new Banner(Tone::Warn, QStringLiteral("Geçerlilik denetimi: 2 uyarı"),
                             QStringLiteral(
                                 "Beyanlar alanı boş — imar durumu belgesi üretilirken zorunludur. "
                                 "Yapı ruhsat tarihi, tapu tarihinden önce görünüyor."),
                             sheet),
                  "uyarı şeridi", "warn");
        banner->addButton(QStringLiteral("Ayrıntı"));
        page->addWidget(banner);

        auto* strip = shown(new ProgressStrip(sheet), "yükleniyor", "etkin");
        strip->setActive(true);
        page->addWidget(strip);
    }

    // ---- TABLO · İFADE ÇUBUĞU, design.md §9 ------------------------------------
    {
        page->addWidget(new FormSection(
            QStringLiteral("TABLO · İFADE ÇUBUĞU"),
            QStringLiteral("zebra, seçili satır, boş hücre, düzenlenmiş hücre"), sheet));
        auto* expression = shown(new ExpressionEdit(sheet), "ifade", "renkli");
        expression->setExpression(
            QStringLiteral("\"alan_m2\" > 2000 AND \"plan_fonksiyon\" = 'Konut'"));
        page->addWidget(expression);
        auto* grid = shown(new DataGrid(sheet), "tablo", "3 satır");
        grid->setModel(new SheetRows(grid));
        grid->setFixedHeight(30 + 3 * 26 + 2);
        grid->setColumnWidth(0, 72);
        grid->setColumnWidth(1, 100);
        grid->setColumnWidth(2, 120);
        grid->setColumnWidth(3, 160);
        grid->selectRow(1);
        page->addWidget(grid);
    }

    // ---- SOHBET, design.md §12 and CLAUDE.md's "modern chat window" ----------
    {
        page->addWidget(
            new FormSection(QStringLiteral("SOHBET — DÖKÜM · İLETİ · DÜŞÜNME · EK · BAĞLAM"),
                            QStringLiteral("model çıktısı her durumda ÖNERİ etiketli"), sheet));

        auto* transcript = shown(new Transcript(sheet), "döküm", "5 ileti");
        transcript->setPlaceholder(
            QStringLiteral("Ne yapmak istediğinizi Türkçe yazın; model komut önerir."));
        // Tall enough for every bubble below, so the living standard shows all
        // five rather than the tail of a scroll.
        transcript->setFixedHeight(460);

        auto* asked = new MessageBubble(Speaker::Person, sheet);
        asked->setText(QStringLiteral(
            "1284 ada 7 parseli iki eşit parçaya böl, bölme çizgisi kuzey cepheye dik olsun."));
        transcript->append(shown(asked, "ileti", "kişi"));

        auto* answered = new MessageBubble(Speaker::Model, sheet);
        answered->setText(QStringLiteral(
            "Parselin alanı 3 482.64 m²; iki eşit parça 1 741.32 m² olur. Kuzey cephe "
            "azimutu 118°, bölme çizgisi buna dik. Aşağıdaki iki komutu öneriyorum."));
        answered->appendReasoning(QStringLiteral(
            "Cephe kenarını kuzeyden seçtim, azimutunu hesapladım, dik doğrultuda alanı "
            "ikiye bölen konumu ikili arama ile buldum."));
        answered->setNote(QStringLiteral("2 adım öneri olarak kaydedildi (KG-4)."), Tone::Accent);
        transcript->append(shown(answered, "ileti", "model"));

        auto* reported = new MessageBubble(Speaker::ToolResult, sheet);
        reported->setText(QStringLiteral("sorgula: 1 nesne · alan 3482.64 m² · @9f3c1a82b7d04e56"));
        transcript->append(shown(reported, "ileti", "araç"));

        auto* thought = new MessageBubble(Speaker::Model, sheet);
        thought->setWaiting(true);
        transcript->append(shown(thought, "ileti", "bekliyor"));

        auto* refused = new MessageBubble(Speaker::Notice, sheet);
        refused->setNote(
            QStringLiteral("Koordinat reddedildi: bu parametre yalnız araç sonucu tutamağı "
                           "kabul eder (ai.md R10)."),
            Tone::Danger);
        transcript->append(shown(refused, "ileti", "uyarı"));
        // THE TOP, not the tail: a standard is read from the beginning, and the
        // follow-the-end rule that is right in a live panel would photograph the
        // last bubble and hide the four above it.
        transcript->toStart();
        page->addWidget(transcript);

        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* dots = shown(new ThinkingDot(sheet), "düşünme", "etkin");
        dots->setElapsedSeconds(4);
        dots->setActive(true);
        row->addWidget(dots);
        row->addWidget(shown(new AttachmentChip(QStringLiteral("olcum_listesi.csv"), 248LL * 1024,
                                                QStringLiteral("text/csv"), sheet),
                             "ek", "kaldırılabilir"));
        auto* sent = new AttachmentChip(QStringLiteral("kroki.png"), 1536LL * 1024,
                                        QStringLiteral("image/png"), sheet);
        sent->setRemovable(false);
        row->addWidget(shown(sent, "ek", "gönderilmiş"));
        row->addStretch(1);
        page->addLayout(row);

        auto* meters = new QHBoxLayout;
        meters->setSpacing(24);
        auto* measured = shown(new ContextMeter(sheet), "bağlam", "ölçüldü");
        measured->setUsage(12400, 128000, true);
        meters->addWidget(measured);
        auto* guessed = shown(new ContextMeter(sheet), "bağlam", "tahmin");
        guessed->setUsage(3200, 8192, false);
        meters->addWidget(guessed);
        auto* unknown = shown(new ContextMeter(sheet), "bağlam", "pencere bilinmiyor");
        unknown->setUsage(9100, 0, true);
        meters->addWidget(unknown);
        page->addLayout(meters);
    }

    page->addStretch(1);
    applyThemeToChildren(sheet, mode);
    return sheet;
}

QStringList componentSheetInventory(QWidget* sheet)
{
    QStringList out;
    for (QWidget* w : sheet->findChildren<QWidget*>()) {
        const QVariant tag = w->property("sheet");
        if (!tag.isValid()) continue;
        out << QStringLiteral("%1 · %2 px").arg(tag.toString()).arg(w->height());
    }
    return out;
}

} // namespace kentos::app
