// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/toolbox.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <algorithm>
#include <functional>
#include <utility>

#include <QAccessible>
#include <QAccessibleWidget>
#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

namespace kentos::app {
namespace {

// Measured off the reference, not guessed: the active tool renders as a 32×32
// accent chip at x 7..38 inside a 45 px column, on a 34 px pitch — so a 2 px gap
// — and a group rule is a 24×1 line with 7 px of air either side.
constexpr int kColumn    = 46; ///< 45 px of content plus the 1 px right rule
constexpr int kButton    = 32;
constexpr int kIcon      = 20;
constexpr int kGap       = 2;
constexpr int kTopPad    = 6;
constexpr int kRuleWidth = 24;
constexpr int kRuleAir   = 7;

constexpr int kChip     = 22;
constexpr int kChipGap  = 3;
constexpr int kChipsPad = 10;

// ---- the family flyout, `design.md` §5 metrics at card scale ----
constexpr int kFlyRow    = 32; ///< one member, the row pitch of a panel list
constexpr int kFlyPadV   = 6;  ///< air above the first row and below the last
constexpr int kFlyIcon   = 20;
constexpr int kFlyIconX  = 12; ///< the icon's left edge inside the card
constexpr int kFlyTextX  = 42; ///< where the name starts
constexpr int kFlyGap    = 28; ///< between the longest name and the command word
constexpr int kFlyRight  = 14; ///< air after the command word
constexpr int kFlyRadius = 8;
constexpr int kFlyShadow = 12; ///< the translucent margin the shadow is drawn in
constexpr int kFlyNub    = 6;  ///< the little wedge that points back at the button
constexpr int kFlyBar    = 3;  ///< the accent bar marking the member in force
constexpr int kFlyMinW   = 190;

/// Press and hold this long and the family card opens.
///
/// IT WAS 280 ms AND THAT BROKE THE TOOL. A click is a press and a release, and
/// a hand's is not instant: a deliberate stab at a tool button routinely takes
/// longer than 280 ms. Past it the card opened instead of the tool running, and
/// because an open Qt popup grabs the mouse, the next press on the same button
/// only dismissed the card and was swallowed. Slow click, card. Click again,
/// card closes, nothing ran. Click again, card. A user reported exactly that —
/// drawing never works, all three of them — and all three were the members of
/// ONE family button.
///
/// 500 ms is the long-press every platform agrees on (macOS, Android, and every
/// tool palette that has one). The release rule below is the other half of the
/// fix and matters more: a slow click still runs the tool.
constexpr int kHoldMs = 500;

/// The corner mark: a wedge in the bottom-right of a button that holds a family,
/// and a TARGET as well as a sign. Every tool palette since the first one has
/// drawn one.
constexpr int kMark = 5;

/// How far into the button the corner mark's target reaches. The wedge is drawn
/// 5 px across; a 5 px target in a 46 px button is a target nobody hits, so the
/// hit area is the corner quarter-ish while the drawing stays small.
constexpr int kMarkTarget = 14;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// The command word an action dispatches, for the right-hand column of the card.
///
/// It is read from the action's own property rather than kept in a second list
/// here — the button and the flyout must never be able to disagree about which
/// command a tool is (CLAUDE.md 5.10).
QString commandWordOf(const QAction* action)
{
    return action->property(kToolCommandProperty).toString();
}

/// The mono face the command words are set in, matching every other readout.
QFont readoutFont()
{
    QFont face(QStringLiteral("IBM Plex Mono"));
    face.setStyleHint(QFont::Monospace);
    face.setPixelSize(11);
    return face;
}

/// THE COLUMN'S BUTTON, and it exists for one reason: a screen reader has to be
/// able to run a tool.
///
/// A tool button is CHECKABLE, because the column's job is to say which command
/// is running (`syncToolSelection`). Qt answers a checkable button's
/// accessibility with `QAccessible::CheckBox`, and the first action it offers is
/// `Toggle` — which is implemented as `QAbstractButton::toggle()`, and that sets
/// the checked flag WITHOUT emitting `clicked`. A `QAction` fires `triggered`
/// from `clicked` and from nothing else, so the accessible press lit the button
/// and the command never ran. It was measured rather than argued: with real
/// CGEvent clicks the log reads `olay bas QToolButton/ÇİZGİ` then
/// `tetiklendi ÇİZGİ`; with an accessibility press it reads `isaretlendi METİN`
/// and nothing else — a lit button that draws nothing, on every draw tool, for
/// every user of VoiceOver, NVDA or Orca.
///
/// So the column owns its button type, and `ToolButtonAccessible` below answers
/// EVERY accessible action with `click()` — the same road a mouse takes, which
/// both runs the command and moves the light. Nothing about the lit state
/// changes: the action stays checkable, stays in `drawingTools_`, and
/// `syncToolSelection` still sets it.
class ToolButton : public QToolButton
{
public:
    explicit ToolButton(QWidget* parent) : QToolButton(parent) {}

    /// Puts `action` on the face and says so in the accessibility tree.
    ///
    /// Used instead of `setDefaultAction` everywhere the face is set, because
    /// Qt copies the action's text, icon and tool tip onto the button and does
    /// NOT copy its accessible name — so a family button whose face had changed
    /// would have announced the tool it no longer was.
    void setFace(QAction* action)
    {
        setDefaultAction(action);
        announce();
    }

    /// Opens the family this button holds. Does nothing on a plain tool.
    virtual void openFamily() {}

    /// Whether this button holds a family, for the column's keyboard road.
    virtual bool holdsFamily() const { return false; }

protected:
    /// The announcement follows whatever action is on the face, including the
    /// changes Qt delivers as events (an enable, a retranslate).
    void actionEvent(QActionEvent* event) override
    {
        QToolButton::actionEvent(event);
        announce();
    }

    /// Names this button for a screen reader (`ui.md` R22).
    ///
    /// The name is the tool's name and the description is its tool tip, both
    /// already translated on the action — so there is no second copy of either
    /// to drift from the flyout and the menu (CLAUDE.md 5.10).
    virtual void announce()
    {
        const QAction* face = defaultAction();
        if (face == nullptr) return;
        setAccessibleName(face->text());
        setAccessibleDescription(face->toolTip());
    }
};

/// What the accessibility layer is handed for a tool button.
///
/// It is deliberately NOT a subclass of Qt's `QAccessibleToolButton`: that class
/// lives in a private header, and the one thing that has to change is the one
/// thing it decides — `doAction`. `QAccessibleWidget` is public API, already
/// implements `QAccessibleActionInterface`, and already answers name,
/// description, rect and parent from the widget.
///
/// The CHECKED STATE IS STILL REPORTED. A screen reader announcing "Çizgi,
/// işaretli" is the spoken form of the lit button, and the column's whole job is
/// to say which command is running — so the role stays `CheckBox` and `state()`
/// carries `checked`. What changes is only what a PRESS does.
class ToolButtonAccessible : public QAccessibleWidget
{
public:
    explicit ToolButtonAccessible(ToolButton* button)
        : QAccessibleWidget(button, QAccessible::CheckBox)
    {}

    QAccessible::Role role() const override
    {
        return button()->isCheckable() ? QAccessible::CheckBox : QAccessible::PushButton;
    }

    QAccessible::State state() const override
    {
        QAccessible::State reported = QAccessibleWidget::state();
        reported.checkable          = button()->isCheckable();
        reported.checked            = button()->isChecked();
        return reported;
    }

    /// PRESS FIRST, because a platform that offers only one action picks the
    /// first it recognises, and `Press` is the one that means "do the thing".
    /// `Toggle` is still offered for a checkable tool, because a client that
    /// looks for a check box's action by name has to find it.
    QStringList actionNames() const override
    {
        if (!button()->isEnabled()) return {};
        QStringList names{pressAction()};
        if (button()->isCheckable()) names << toggleAction();
        return names;
    }

    /// EVERY ACTION IS A CLICK. This is the whole fix: `click()` is the road the
    /// mouse takes, so it runs the command AND leaves the light where a mouse
    /// would have left it. Qt's own answer for `Toggle` is `toggle()`, which
    /// moves the light and runs nothing.
    void doAction(const QString& name) override
    {
        if (!button()->isEnabled() || !actionNames().contains(name)) return;
        button()->click();
    }

    QStringList keyBindingsForAction(const QString&) const override { return {}; }

private:
    ToolButton* button() const { return static_cast<ToolButton*>(object()); }
};

/// Hands `ToolButtonAccessible` to Qt for the column's buttons and NOTHING else.
///
/// `key` is the class name Qt walked up to; our buttons declare no `Q_OBJECT`, so
/// they arrive as `QToolButton` like every other one in the program — the cast is
/// what tells them apart. Returning null lets Qt carry on to its own factory, so
/// every other tool button in the shell keeps the stock interface.
QAccessibleInterface* toolButtonInterface(const QString& key, QObject* object)
{
    if (key != QLatin1String("QToolButton")) return nullptr;
    auto* button = dynamic_cast<ToolButton*>(object);
    return button == nullptr ? nullptr : new ToolButtonAccessible(button);
}

/// A tool button that holds a family: it paints the corner mark and turns a held
/// press into a flyout instead of a command.
///
/// No `Q_OBJECT` on purpose — it declares no signal of its own and talks to the
/// column through a callback, which keeps it out of `moc`'s way in a `.cpp`.
class FamilyButton : public ToolButton
{
public:
    FamilyButton(QWidget* parent, std::function<void(FamilyButton*)> open)
        : ToolButton(parent), open_(std::move(open))
    {
        hold_.setSingleShot(true);
        hold_.setInterval(kHoldMs);
        QObject::connect(&hold_, &QTimer::timeout, this, [this] {
            held_ = true;
            setDown(false);
            open_(this);
        });
    }

    void setTheme(ThemeMode mode)
    {
        theme_ = mode;
        update();
    }

    /// The card, opened without a mouse. The column's Right arrow lands here.
    void openFamily() override { open_(this); }

    bool holdsFamily() const override { return true; }

protected:
    /// SAYS IT HOLDS A FAMILY, because the corner mark is a picture and a screen
    /// reader cannot see it. Without this, ten of the eleven tools behind these
    /// buttons were invisible to a user who never sees the wedge.
    void announce() override
    {
        ToolButton::announce();
        const QAction* face = defaultAction();
        if (face == nullptr) return;
        setAccessibleDescription(ToolBox::tr("%1  ·  araç ailesi: sağ ok tuşu ailenin kartını açar")
                                     .arg(face->toolTip()));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        held_  = false;
        moved_ = false;
        // The corner mark is a target of its own: a user who can see the wedge
        // should not have to discover that holding does the same thing.
        const bool onMark = event->position().x() > width() - kMarkTarget &&
                            event->position().y() > height() - kMarkTarget;
        if (event->button() == Qt::RightButton || onMark) {
            held_  = true;
            moved_ = true; ///< asked for the card on purpose; the release keeps it
            open_(this);
            return;
        }
        hold_.start();
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        // LEAVING THE BUTTON IS WHAT MAKES IT A HOLD. A hand on its way to the
        // card moves off the button; a hand making a slow click does not.
        if (!rect().contains(event->position().toPoint())) moved_ = true;
        QToolButton::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        hold_.stop();
        if (held_) {
            setDown(false);
            // A SLOW CLICK IS STILL A CLICK. Released on the button without ever
            // leaving it, the user reached for the TOOL and their hand was merely
            // slower than a timer: the card goes away and the face runs. Only a
            // hand that moved off the button — or asked for the card by its mark
            // or the right button — is reaching for the family.
            if (!moved_ && rect().contains(event->position().toPoint())) {
                if (QWidget* card = QApplication::activePopupWidget();
                    card != nullptr && card->property("kentos.rows").isValid())
                    card->close();
                if (QAction* face = defaultAction(); face != nullptr) face->trigger();
            }
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QToolButton::paintEvent(event);

        const Tokens& t = tokensOf(theme_);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath wedge;
        const qreal x = width() - 4.0;
        const qreal y = height() - 4.0;
        wedge.moveTo(x, y);
        wedge.lineTo(x - kMark, y);
        wedge.lineTo(x, y - kMark);
        wedge.closeSubpath();
        p.fillPath(wedge, isChecked() ? t.accentHi : t.textFaint);
    }

private:
    std::function<void(FamilyButton*)> open_;
    QTimer hold_;
    bool held_{false};

    /// Whether the pointer left the button after the press: a hand on its way to
    /// the card, rather than one making a slow click.
    bool moved_{false};
    ThemeMode theme_ = ThemeMode::Dark;
};

} // namespace

// =============================================================================
// ColourChips
// =============================================================================

ColourChips::ColourChips(QWidget* parent) : QWidget(parent)
{
    setFixedSize(kChip, kChip * 2 + kChipGap);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(tr("Çizgi ve dolgu rengi"));
    setAccessibleDescription(tr("Yukarı/aşağı ok iki kutu arasında gezer; Enter ya da Boşluk "
                                "odaktaki kutunun renk menüsünü açar"));
    setToolTip(tr("Çizim ve dolgu rengi"));
}

void ColourChips::setColours(const QColor& stroke, const QColor& fill)
{
    stroke_ = stroke;
    fill_   = fill;
    update();
}

void ColourChips::setDescriptions(const QString& stroke, const QString& fill)
{
    strokeTip_ = stroke;
    fillTip_   = fill;
}

QRect ColourChips::chipRect(int which)
{
    return {0, which == 0 ? 0 : kChip + kChipGap, kChip, kChip};
}

void ColourChips::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    if (!stroke_.isValid()) stroke_ = tokensOf(mode).accent;
    update();
}

void ColourChips::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto chip = [&](int which, const QColor& fill, const QColor& edge) {
        const QRectF box = QRectF(chipRect(which)).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
        p.setPen(QPen(edge, 1.0));
        p.drawRoundedRect(box, 3.0, 3.0);

        // NO FILL is a hollow chip crossed by one diagonal, the mark a CAD
        // palette has always used; hollow alone reads as "not loaded yet".
        if (!fill.isValid()) p.drawLine(box.bottomLeft(), box.topRight());
    };

    chip(0, stroke_.isValid() ? stroke_ : t.accent, t.accentHi);
    chip(1, fill_, t.border);

    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(t.accentEdge, 1.5));
        p.drawRoundedRect(QRectF(chipRect(focused_)).adjusted(1.5, 1.5, -1.5, -1.5), 2.0, 2.0);
    }
}

void ColourChips::mousePressEvent(QMouseEvent* event)
{
    focused_ = event->position().y() < kChip ? 0 : 1;
    emit chipActivated(focused_);
}

void ColourChips::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
        focused_ = event->key() == Qt::Key_Up ? 0 : 1;
        update();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: emit chipActivated(focused_); return;
    default: QWidget::keyPressEvent(event);
    }
}

bool ColourChips::event(QEvent* event)
{
    // EACH CHIP SAYS ITS OWN THING: the colour it shows, whose it is and what a
    // press does. One tooltip for the pair could only say that they are colours.
    if (event->type() == QEvent::ToolTip) {
        const auto* help   = static_cast<QHelpEvent*>(event);
        const bool upper   = help->pos().y() < kChip;
        const QString said = upper ? strokeTip_ : fillTip_;
        if (!said.isEmpty()) {
            QToolTip::showText(help->globalPos(), said, this, chipRect(upper ? 0 : 1));
            return true;
        }
    }
    return QWidget::event(event);
}

// =============================================================================
// SwatchRow
// =============================================================================

namespace {
constexpr int kSwatch    = 20; ///< one swatch, square
constexpr int kSwatchGap = 5;
constexpr int kSwatchPad = 10; ///< air round the row, matching a menu row's inset
} // namespace

SwatchRow::SwatchRow(QVector<Swatch> swatches, QWidget* parent)
    : QWidget(parent), swatches_(std::move(swatches))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(tr("Renkler"));
    setAccessibleDescription(tr("Sol/sağ ok renkler arasında gezer; Enter ya da Boşluk seçer"));
}

QSize SwatchRow::sizeHint() const
{
    const int n = static_cast<int>(swatches_.size());
    return {kSwatchPad * 2 + n * kSwatch + std::max(0, n - 1) * kSwatchGap,
            kSwatchPad + kSwatch + kSwatchPad / 2};
}

void SwatchRow::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QRectF SwatchRow::swatchRect(int index)
{
    return {static_cast<qreal>(kSwatchPad + index * (kSwatch + kSwatchGap)),
            static_cast<qreal>(kSwatchPad) / 2.0, static_cast<qreal>(kSwatch),
            static_cast<qreal>(kSwatch)};
}

int SwatchRow::swatchAt(QPointF at) const
{
    for (int i = 0; i < swatches_.size(); ++i)
        if (swatchRect(i).contains(at)) return i;
    return -1;
}

void SwatchRow::moveTo(int index)
{
    if (swatches_.isEmpty()) return;
    focus_ = std::clamp(index, 0, static_cast<int>(swatches_.size()) - 1);
    update();

    // A SCREEN READER HEARS THE COLOUR, not "Renkler" nine times over.
    setAccessibleName(swatches_[focus_].name);
    QAccessibleEvent moved(this, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&moved);
}

void SwatchRow::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < swatches_.size(); ++i) {
        const QRectF box = swatchRect(i).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(swatches_[i].colour);
        p.setPen(QPen(t.border, 1.0));
        p.drawRoundedRect(box, 3.0, 3.0);

        // Under the pointer, and where the keyboard is: a ring OUTSIDE the
        // swatch, so the colour itself is never covered by the mark.
        const bool keyed = hasFocus() && i == focus_;
        if (i == hover_ || keyed) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(keyed ? t.accentEdge : t.text, 1.5));
            p.drawRoundedRect(box.adjusted(-2.5, -2.5, 2.5, 2.5), 4.5, 4.5);
        }
    }
}

void SwatchRow::mouseMoveEvent(QMouseEvent* event)
{
    const int at = swatchAt(event->position());
    if (at != hover_) {
        hover_ = at;
        update();
    }
}

void SwatchRow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (const int at = swatchAt(event->position()); at >= 0) emit picked(at);
}

void SwatchRow::leaveEvent(QEvent* /*event*/)
{
    hover_ = -1;
    update();
}

void SwatchRow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left: moveTo(focus_ - 1); return;
    case Qt::Key_Right: moveTo(focus_ + 1); return;
    case Qt::Key_Home: moveTo(0); return;
    case Qt::Key_End: moveTo(static_cast<int>(swatches_.size()) - 1); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (!swatches_.isEmpty()) emit picked(focus_);
        return;
    default:
        // Up and Down belong to the menu the row sits in.
        event->ignore();
    }
}

bool SwatchRow::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        if (const int at = swatchAt(help->pos()); at >= 0) {
            QToolTip::showText(help->globalPos(), swatches_[at].name, this,
                               swatchRect(at).toAlignedRect());
            return true;
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

// =============================================================================
// ToolFlyout
// =============================================================================

ToolFlyout::ToolFlyout(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("toolFlyout"));

    // A POPUP, which is what makes it close when the user looks elsewhere: Qt
    // grabs the mouse for a `Qt::Popup` and gives it back the moment a click
    // lands outside. `NoDropShadowWindowHint` because the shadow is painted here,
    // in the theme's own colours, rather than borrowed from the window manager.
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void ToolFlyout::reveal(const QVector<QAction*>& family, QAction* current, const QPoint& anchor)
{
    family_  = family;
    hover_   = -1;
    current_ = 0;
    for (int i = 0; i < static_cast<int>(family_.size()); ++i)
        if (family_[i] == current) current_ = i;

    // MEASURED, not fixed. A card wide enough for `DİKDÖRTGEN` and one wide
    // enough for `ÇOKLUÇİZGİ` are different cards, and a name clipped to fit
    // would defeat the only reason the card carries names at all.
    const QFontMetrics name(font());
    const QFontMetrics word(readoutFont());
    int widest = 0;
    for (const QAction* member : family_)
        widest = std::max(widest, kFlyTextX + name.horizontalAdvance(member->text()) + kFlyGap +
                                      word.horizontalAdvance(commandWordOf(member)) + kFlyRight);

    const int cardW = std::max(widest, kFlyMinW);
    const int cardH = (static_cast<int>(family_.size()) * kFlyRow) + (kFlyPadV * 2);
    resize(cardW + (kFlyShadow * 2), cardH + (kFlyShadow * 2));

    // The nub points back at the button, and the card is placed so that it does
    // even when the button is at the very top or the very bottom of the column.
    int top = anchor.y() - kFlyShadow - kFlyPadV - (kFlyRow / 2) - (current_ * kFlyRow);
    if (const QScreen* screen = QApplication::screenAt(anchor); screen != nullptr) {
        const QRect room = screen->availableGeometry();
        top              = std::clamp(top, room.top(), room.bottom() - height());
    }
    move(anchor.x(), top);
    nub_ = anchor.y() - top;

    // WHERE ITS ROWS ARE, for `KENTOS_FLYOUT_PROBE`. Eleven tools live only
    // behind these cards and no test had ever pressed one, because a test cannot
    // click a row whose position it has no way to learn. Dynamic properties
    // rather than an accessor: the class is private to this file and the probe
    // is the only reader.
    setProperty("kentos.rows", static_cast<int>(family_.size()));
    setProperty("kentos.rowPitch", kFlyRow);
    setProperty("kentos.rowCentre",
                QPoint(kFlyShadow + (kFlyMinW / 2), kFlyShadow + kFlyPadV + (kFlyRow / 2)));

    show();
    setFocus(Qt::PopupFocusReason);
}

QRectF ToolFlyout::cardRect() const
{
    return {kFlyShadow, kFlyShadow, width() - (kFlyShadow * 2.0), height() - (kFlyShadow * 2.0)};
}

int ToolFlyout::rowAt(const QPoint& where) const
{
    const int row = (where.y() - kFlyShadow - kFlyPadV) / kFlyRow;
    if (where.x() < kFlyShadow || where.x() > width() - kFlyShadow) return -1;
    if (row < 0 || row >= static_cast<int>(family_.size())) return -1;
    return row;
}

void ToolFlyout::settle(int row)
{
    if (row < 0 || row >= static_cast<int>(family_.size())) return;
    QAction* member = family_[row];
    close();
    emit chosen(member);
}

void ToolFlyout::mouseMoveEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint());
    if (row == hover_) return;
    hover_ = row;
    update();
}

void ToolFlyout::mousePressEvent(QMouseEvent* event)
{
    // Outside the card means "not this after all". Inside it, the press does
    // nothing: the choice is made on the release, over the row it lands on.
    if (rowAt(event->position().toPoint()) < 0 && !cardRect().contains(event->position())) close();
}

void ToolFlyout::mouseReleaseEvent(QMouseEvent* event)
{
    // TWO GESTURES, ONE CARD, and the second one is why a release off the rows
    // may not close anything.
    //
    //   HOLD AND DRAG — press the button, hold, slide onto a row, let go there.
    //   HOLD AND LOOK — press the button, hold, let go, read the card, click.
    //
    // The release that ends the hold arrives HERE, through the popup grab, with
    // the pointer still sitting on the button and therefore on no row at all.
    // Closing on it made the second gesture impossible: the card opened and
    // vanished in the same motion.
    const int row = rowAt(event->position().toPoint());
    if (row >= 0) settle(row);
}

void ToolFlyout::keyPressEvent(QKeyEvent* event)
{
    // KEYBOARD FIRST, even though this opened under a finger. A card that could
    // only be answered with the mouse would make the family it holds a
    // mouse-only capability, which is the one thing the tool palette may not do
    // (CLAUDE.md 5.15).
    switch (event->key()) {
    case Qt::Key_Down:
        hover_ = hover_ < 0 ? current_ : (hover_ + 1) % static_cast<int>(family_.size());
        update();
        return;
    case Qt::Key_Up:
        hover_ = hover_ <= 0 ? static_cast<int>(family_.size()) - 1 : hover_ - 1;
        update();
        return;
    case Qt::Key_Home:
        hover_ = 0;
        update();
        return;
    case Qt::Key_End:
        hover_ = static_cast<int>(family_.size()) - 1;
        update();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: settle(hover_ < 0 ? current_ : hover_); return;
    case Qt::Key_Escape: close(); return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

void ToolFlyout::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void ToolFlyout::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF card = cardRect();

    // The shadow is drawn as a short stack of rounded rects fading outwards. A
    // blur would cost a full-card raster on every open; this reads the same at
    // this size and costs four fills.
    p.setPen(Qt::NoPen);
    for (int ring = kFlyShadow; ring > 0; ring -= 3) {
        QColor ink(0, 0, 0);
        ink.setAlphaF(0.030f * static_cast<float>(kFlyShadow - ring + 3) / 3.0f);
        p.setBrush(ink);
        p.drawRoundedRect(card.adjusted(-ring, -ring + 2, ring, ring + 2), kFlyRadius + ring,
                          kFlyRadius + ring);
    }

    // The card, and the wedge that points back at the button it came from, drawn
    // as ONE path so the border runs around both and no seam shows.
    QPainterPath shape;
    shape.addRoundedRect(card, kFlyRadius, kFlyRadius);
    QPainterPath nub;
    nub.moveTo(card.left(), nub_ - kFlyNub);
    nub.lineTo(card.left() - kFlyNub, nub_);
    nub.lineTo(card.left(), nub_ + kFlyNub);
    nub.closeSubpath();
    shape = shape.united(nub);

    p.setBrush(t.bgRaised);
    p.setPen(QPen(t.border, 1.0));
    p.drawPath(shape);

    const QFont word = readoutFont();
    for (int i = 0; i < static_cast<int>(family_.size()); ++i) {
        const QAction* member = family_[i];
        const QRectF row(card.left() + 1, card.top() + kFlyPadV + (i * kFlyRow), card.width() - 2,
                         kFlyRow);

        if (i == hover_) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverRow);
            p.drawRoundedRect(row.adjusted(4, 1, -4, -1), 5.0, 5.0);
        }
        if (i == current_) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.accent);
            p.drawRoundedRect(QRectF(row.left() + 4, row.center().y() - 8, kFlyBar, 16), 1.5, 1.5);
        }

        const QIcon::Mode mode = i == hover_ ? QIcon::Active : QIcon::Normal;
        member->icon().paint(&p,
                             QRect(static_cast<int>(row.left()) + kFlyIconX,
                                   static_cast<int>(row.center().y()) - (kFlyIcon / 2), kFlyIcon,
                                   kFlyIcon),
                             Qt::AlignCenter, mode);

        p.setFont(font());
        p.setPen(i == current_ || i == hover_ ? t.text : t.textDim);
        p.drawText(row.adjusted(kFlyTextX, 0, -kFlyRight, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   member->text());

        // The command word, quiet and on the right: it is what the user will type
        // once they no longer need the button.
        p.setFont(word);
        p.setPen(t.readoutDim);
        p.drawText(row.adjusted(0, 0, -kFlyRight, 0), Qt::AlignVCenter | Qt::AlignRight,
                   commandWordOf(member));
    }
}

// =============================================================================
// ToolBox
// =============================================================================

ToolBox::ToolBox(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("toolBox"));
    setFixedWidth(kColumn);

    // ONCE PER PROCESS, and here rather than in `main` because the column is what
    // knows the class exists. `QAccessible` keeps the factories in a list and
    // asks the LAST one first, so this is consulted before Qt's own.
    static const bool registered = [] {
        QAccessible::installFactory(&toolButtonInterface);
        return true;
    }();
    (void)registered;

    // ONE TAB STOP FOR THE WHOLE COLUMN, and arrow keys inside it — the pattern
    // every tool palette and every toolbar uses. The alternative, a focus policy
    // on each button, would have put thirty stops between the menu bar and the
    // canvas for every Tab press the program will ever see.
    //
    // The buttons stay `Qt::NoFocus` (`addTool`): clicking a tool must not take
    // the keyboard off the canvas, which is where Esc and the arrow keys belong
    // while a command is running.
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(tr("Araç kutusu"));
    setAccessibleDescription(tr("Çizim ve düzenleme araçları — yukarı/aşağı ok tuşları gezer, "
                                "Boşluk ya da Enter aracı çalıştırır, sağ ok aile kartını açar"));

    // BUILT HERE, not on the first theme pass. They used to be created lazily in
    // `applyTheme`, which runs after the shell has already asked for `chips()` to
    // connect to it — so the connect got a null and the program died on the next
    // line. A widget the caller can ask for must exist as soon as the object does.
    // `place` pins them to the foot of the first column.
    chips_ = new ColourChips(this);
}

QVector<QRect> ToolBox::place(int height, int& columns) const
{
    // WHOLE GROUPS UNLESS THAT COSTS A COLUMN. Kept whole, the select group can
    // be left alone in the first column with two thirds of it empty while the
    // tools run on to a fourth; a column is 45 px the canvas does not get.
    int whole_columns      = 1;
    QVector<QRect> whole   = flow(height, true, whole_columns);
    int flowing_columns    = 1;
    QVector<QRect> flowing = flow(height, false, flowing_columns);
    if (flowing_columns < whole_columns) {
        columns = flowing_columns;
        return flowing;
    }
    columns = whole_columns;
    return whole;
}

QVector<QRect> ToolBox::flow(int height, bool whole, int& columns) const
{
    // THE REFERENCE'S PITCH, kept exactly: 34 px from button to button inside a
    // group, 51 px across a group boundary — the 24 px rule with 9 px of air
    // above and below it (7 of its own, 2 of the gap). What is new is only where
    // a column ENDS: at the chips, not at the window's foot.
    constexpr int kPitch     = kColumn - 1; ///< one column's content width
    constexpr int kRuleBlock = 2 * (kRuleAir + kGap) + 1;
    const int foot           = kChipsPad + chips_->height() + kRuleAir + kGap;
    const int limit          = std::max(kTopPad + kButton, height - foot);

    QVector<QRect> at(entries_.size());
    int column = 0;
    int y      = kTopPad;
    bool empty = true; ///< nothing placed in this column yet

    const auto button_at = [&](int i) {
        at[i] = QRect(column * kPitch + (kPitch - kButton) / 2, y, kButton, kButton);
        y += kButton;
        empty = false;
    };
    const auto next_column = [&] {
        ++column;
        y     = kTopPad;
        empty = true;
    };

    int i = 0;
    while (i < entries_.size()) {
        if (entries_[i].rule) {
            // A RULE HEADS A GROUP, and it is only drawn between two groups in
            // one column. The group after it moves on whole when it would not
            // fit below it but does fit in a column of its own.
            int n = 0;
            while (i + 1 + n < entries_.size() && !entries_[i + 1 + n].rule)
                ++n;
            const int group = n * kButton + std::max(0, n - 1) * kGap;
            if (whole && !empty && y + kRuleBlock + group > limit && kTopPad + group <= limit)
                next_column();
            if (!empty && y + kRuleBlock + kButton <= limit) {
                at[i] = QRect(column * kPitch + (kPitch - kRuleWidth) / 2, y + kRuleAir + kGap,
                              kRuleWidth, 1);
                y += kRuleBlock;
            } else if (!empty) {
                next_column();
            }
            ++i;
            continue;
        }
        if (!empty) {
            if (y + kGap + kButton > limit)
                next_column();
            else
                y += kGap;
        }
        button_at(i);
        ++i;
    }
    columns = column + 1;
    return at;
}

int ToolBox::columnsFor(int height) const
{
    int columns = 1;
    (void)place(height, columns);
    return columns;
}

QSize ToolBox::sizeHint() const
{
    // One column's worth of height: what the column would take if the body
    // gave it everything it asks for.
    int total  = kTopPad;
    bool first = true;
    for (const Entry& e : entries_) {
        if (e.rule) {
            total += 2 * (kRuleAir + kGap) + 1;
            first = true;
            continue;
        }
        total += (first ? 0 : kGap) + kButton;
        first = false;
    }
    total += kRuleAir + kGap + chips_->height() + kChipsPad;
    return {columns_ * (kColumn - 1) + 1, total};
}

QSize ToolBox::minimumSizeHint() const
{
    return {kColumn, kTopPad + kButton + kRuleAir + kGap + chips_->height() + kChipsPad};
}

void ToolBox::resizeEvent(QResizeEvent* event)
{
    int columns             = 1;
    const QVector<QRect> at = place(height(), columns);
    for (int i = 0; i < entries_.size(); ++i) {
        QWidget* w = entries_[i].widget;
        if (at[i].isEmpty()) {
            w->hide();
            continue;
        }
        w->setGeometry(at[i]);
        w->show();
    }
    constexpr int kPitch = kColumn - 1;
    chips_->move((kPitch - chips_->width()) / 2, height() - kChipsPad - chips_->height());

    // WIDER ONLY WHEN IT MUST, and the body lays itself out again around the
    // new width; the height, and so the count, does not change on that pass.
    if (columns != columns_) {
        columns_ = columns;
        setFixedWidth(columns * kPitch + 1);
        updateGeometry();
    }
    QWidget::resizeEvent(event);
}

void ToolBox::addTool(QAction* action)
{
    // A TOOL THAT DOES NOT EXIST YET is kept out of the column and kept IN the
    // list, so `KENTOS_TOOL_PROBE` can name it: four tools were once added
    // before the code that makes them had run, and the column simply went
    // without them — nothing crashed and nothing said so.
    if (action == nullptr) {
        tools_.push_back(nullptr);
        return;
    }
    auto* button = new ToolButton(this);
    button->setObjectName(QStringLiteral("toolBoxButton"));
    button->setFace(action);
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);

    buttons_.push_back(button);
    tools_.push_back(action);
    entries_.push_back(Entry{.widget = button});
}

void ToolBox::addFamily(const QVector<QAction*>& given)
{
    tools_ += given; ///< nulls included, for the probe — see `addTool`
    QVector<QAction*> family;
    for (QAction* member : given)
        if (member != nullptr) family.push_back(member);
    if (family.isEmpty()) return;
    if (flyout_ == nullptr) {
        flyout_ = new ToolFlyout(this);
        flyout_->applyTheme(theme_);
    }

    auto* button = new FamilyButton(this, [this, family](FamilyButton* from) {
        flyout_->reveal(family, from->defaultAction(),
                        from->mapToGlobal(QPoint(from->width() + kFlyNub, from->height() / 2)));
    });

    // CONNECTED ONCE, and it filters by membership. The one card serves every
    // family, so every button hears every choice; a button that answered one
    // meant for another family would arm the wrong tool. Membership is the exact
    // test because an action belongs to one family and no other.
    //
    // The earlier shape — a connection made fresh on each open and deleted when
    // it fired — died in the destructor: the second open left the first open's
    // heap `Connection` in a `destroyed` handler that had already freed it.
    QObject::connect(flyout_, &ToolFlyout::chosen, button, [button, family](QAction* member) {
        if (!family.contains(member)) return;
        button->setFace(member);
        member->trigger();
    });

    // THE FACE FOLLOWS THE TOOL, wherever it was armed from. `ÇOKLUÇİZGİ` chosen
    // from the Çiz menu, typed at the prompt or sent by a script must light this
    // button too — otherwise the column would say no tool is running while one
    // plainly is, and the family would look like a mouse-only grouping.
    for (QAction* member : family)
        QObject::connect(member, &QAction::toggled, button, [button, member](bool on) {
            if (on && button->defaultAction() != member) button->setFace(member);
        });

    button->setObjectName(QStringLiteral("toolBoxButton"));
    // IT IS A FAMILY, and a probe has to be able to tell. Pressing an ordinary
    // tool button runs its command; pressing a family button and holding opens a
    // card. A probe that could not tell them apart had to press everything to
    // find out, which starts nineteen commands to open four cards.
    button->setProperty("kentos.family", true);
    button->setFace(family.front());
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setTheme(theme_);

    buttons_.push_back(button);
    entries_.push_back(Entry{.widget = button});
}

void ToolBox::addSeparator()
{
    auto* rule = new QWidget(this);
    rule->setObjectName(QStringLiteral("toolBoxRule"));
    rule->setFixedSize(kRuleWidth, 1);

    separators_.push_back(rule);
    entries_.push_back(Entry{.widget = rule, .rule = true});
}

void ToolBox::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    if (flyout_ != nullptr) flyout_->applyTheme(mode);
    for (QToolButton* button : buttons_)
        if (auto* family = dynamic_cast<FamilyButton*>(button); family != nullptr)
            family->setTheme(mode);

    const Tokens& t = tokensOf(mode);
    for (QWidget* rule : separators_) {
        QPalette palette = rule->palette();
        palette.setColor(QPalette::Window, t.separator);
        rule->setAutoFillBackground(true);
        rule->setPalette(palette);
    }
    update();
}

void ToolBox::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.fillRect(rect(), t.bgPanel);
    p.fillRect(QRect(width() - 1, 0, 1, height()), t.lineHard);

    // WHERE THE KEYBOARD IS, and only when the keyboard is here (`ui.md` R31: a
    // focus ring is for keyboard focus and nothing else). It is drawn by the
    // column rather than as a `:focus` rule on the button, because the buttons
    // stay `Qt::NoFocus` — the focus is on the column and the ring says which
    // tool Enter would run. Three pixels out, so it sits in the column's own air
    // and the button paints over none of it.
    if (hasFocus() && focused_ >= 0 && focused_ < buttons_.size()) {
        const QRectF ring = QRectF(buttons_[focused_]->geometry()).adjusted(-2.5, -2.5, 2.5, 2.5);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(t.accent, 1.0));
        p.drawRoundedRect(ring, 6.0, 6.0);
    }
}

int ToolBox::stepFrom(int start, int by) const
{
    // Skips what a hand would skip. A Faz 2 placeholder is in the column on
    // purpose — hiding it would be worse than showing it disabled (`arayuz.md`) —
    // but landing the keyboard on one would be a dead stop nobody can act on.
    const int count = static_cast<int>(buttons_.size());
    if (count == 0) return -1;
    for (int tried = 0; tried < count; ++tried) {
        start = ((start + by) % count + count) % count;
        if (buttons_[start]->isEnabled()) return start;
    }
    return -1;
}

void ToolBox::focusInEvent(QFocusEvent* event)
{
    // ARRIVES ON THE TOOL IN THE HAND. Tabbing into the column and being put on
    // whatever was highlighted last time would make the ring say one thing while
    // the lit button says another; the tool that is RUNNING is the one a user is
    // thinking about.
    if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason)
        for (int i = 0; i < buttons_.size(); ++i)
            if (buttons_[i]->isChecked()) {
                focused_ = i;
                break;
            }
    if (focused_ < 0 || focused_ >= buttons_.size() || !buttons_[focused_]->isEnabled())
        focused_ = stepFrom(-1, 1);
    update();
    QWidget::focusInEvent(event);
}

void ToolBox::focusOutEvent(QFocusEvent* event)
{
    update();
    QWidget::focusOutEvent(event);
}

void ToolBox::keyPressEvent(QKeyEvent* event)
{
    if (buttons_.isEmpty() || focused_ < 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Down:
        focused_ = stepFrom(focused_, 1);
        update();
        return;
    case Qt::Key_Up:
        focused_ = stepFrom(focused_, -1);
        update();
        return;
    case Qt::Key_Home:
        focused_ = stepFrom(-1, 1);
        update();
        return;
    case Qt::Key_End:
        // BACKWARDS FROM THE TOP, which wraps to the bottom. Stepping back from
        // the last index would land on the one before it.
        focused_ = stepFrom(0, -1);
        update();
        return;
    case Qt::Key_Right:
        // THE CARD, WITHOUT A MOUSE. Ten of the eleven family members are behind
        // it, and the card itself already answers arrow keys, Enter and Esc
        // (`ToolFlyout::keyPressEvent`) — so this is the last link in a road that
        // was otherwise complete and unreachable.
        if (auto* tool = dynamic_cast<ToolButton*>(buttons_[focused_]);
            tool != nullptr && tool->holdsFamily()) {
            tool->openFamily();
            return;
        }
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // `click()`, the same call the accessible press makes and the same one a
        // real mouse release ends in — one road into a tool, whoever asks.
        buttons_[focused_]->click();
        return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

} // namespace kentos::app
