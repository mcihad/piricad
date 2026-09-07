// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/toolbox.hpp"

#include "kentos_cad/app/tokens.hpp"

#include <algorithm>
#include <functional>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

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

/// Press and hold this long and the family opens instead of the tool running.
/// Below ~200 ms a deliberate click starts opening it; above ~400 ms the hold
/// feels broken. Windows uses 400, macOS 250; this sits where a CAD user's
/// stab at a tool button lands.
constexpr int kHoldMs = 280;

/// The corner mark: a small wedge in the bottom-right of a button that holds a
/// family. Every tool palette since the first one has drawn one.
constexpr int kMark = 5;

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

/// A tool button that holds a family: it paints the corner mark and turns a held
/// press into a flyout instead of a command.
///
/// No `Q_OBJECT` on purpose — it declares no signal of its own and talks to the
/// column through a callback, which keeps it out of `moc`'s way in a `.cpp`.
class FamilyButton : public QToolButton
{
public:
    FamilyButton(QWidget* parent, std::function<void(FamilyButton*)> open)
        : QToolButton(parent), open_(std::move(open))
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

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        held_ = false;
        // The corner mark is a target of its own: a user who can see the wedge
        // should not have to discover that holding does the same thing.
        const bool onMark = event->position().x() > width() - (kMark * 2) &&
                            event->position().y() > height() - (kMark * 2);
        if (event->button() == Qt::RightButton || onMark) {
            held_ = true;
            open_(this);
            return;
        }
        hold_.start();
        QToolButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        hold_.stop();
        // A release after the card opened must not ALSO run the tool: the user
        // was reaching for the family, not for the face.
        if (held_) {
            setDown(false);
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
    setToolTip(tr("Çizim ve dolgu rengi"));
}

void ColourChips::setColours(const QColor& stroke, const QColor& fill)
{
    stroke_ = stroke;
    fill_   = fill;
    update();
}

void ColourChips::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    if (!stroke_.isValid()) stroke_ = tokensOf(mode).accent;
    update();
}

void ColourChips::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto chip = [&](int top, const QColor& fill, const QColor& edge) {
        const QRectF box(0.5, top + 0.5, kChip - 1, kChip - 1);
        p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
        p.setPen(QPen(edge, 1.0));
        p.drawRoundedRect(box, 3.0, 3.0);
    };

    chip(0, stroke_.isValid() ? stroke_ : t.accent, t.accentHi);
    chip(kChip + kChipGap, fill_, t.border);
}

void ColourChips::mousePressEvent(QMouseEvent* event)
{
    emit chipActivated(event->position().y() < kChip ? 0 : 1);
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

    show();
    setFocus(Qt::PopupFocusReason);
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

void ToolFlyout::mouseReleaseEvent(QMouseEvent* event)
{
    const int row = rowAt(event->position().toPoint());
    if (row < 0) {
        close();
        return;
    }
    settle(row);
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

    const QRectF card(kFlyShadow, kFlyShadow, width() - (kFlyShadow * 2.0),
                      height() - (kFlyShadow * 2.0));

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

    column_ = new QVBoxLayout(this);
    column_->setContentsMargins(0, kTopPad, 1, kChipsPad);
    column_->setSpacing(kGap);
    column_->setAlignment(Qt::AlignHCenter);

    // BUILT HERE, not on the first theme pass. They used to be created lazily in
    // `applyTheme`, which runs after the shell has already asked for `chips()` to
    // connect to it — so the connect got a null and the program died on the next
    // line. A widget the caller can ask for must exist as soon as the object does.
    //
    // The stretch goes in first so the chips stay pinned to the foot of the
    // column whatever tools are added above them.
    chipsSpacer_ = column_->count();
    column_->addStretch(1);

    chips_ = new ColourChips(this);
    column_->addWidget(chips_, 0, Qt::AlignHCenter);
}

void ToolBox::addTool(QAction* action)
{
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("toolBoxButton"));
    button->setDefaultAction(action);
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);

    buttons_.push_back(button);
    column_->insertWidget(chipsSpacer_++, button, 0, Qt::AlignHCenter);
}

void ToolBox::addFamily(const QVector<QAction*>& family)
{
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
        button->setDefaultAction(member);
        member->trigger();
    });

    // THE FACE FOLLOWS THE TOOL, wherever it was armed from. `ÇOKLUÇİZGİ` chosen
    // from the Çiz menu, typed at the prompt or sent by a script must light this
    // button too — otherwise the column would say no tool is running while one
    // plainly is, and the family would look like a mouse-only grouping.
    for (QAction* member : family)
        QObject::connect(member, &QAction::toggled, button, [button, member](bool on) {
            if (on && button->defaultAction() != member) button->setDefaultAction(member);
        });

    button->setObjectName(QStringLiteral("toolBoxButton"));
    button->setDefaultAction(family.front());
    button->setIconSize(QSize(kIcon, kIcon));
    button->setFixedSize(kButton, kButton);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setTheme(theme_);

    buttons_.push_back(button);
    column_->insertWidget(chipsSpacer_++, button, 0, Qt::AlignHCenter);
}

void ToolBox::addSeparator()
{
    auto* rule = new QWidget(this);
    rule->setObjectName(QStringLiteral("toolBoxRule"));
    rule->setFixedSize(kRuleWidth, 1);

    separators_.push_back(rule);

    // The rule carries its own air rather than relying on the layout's spacing,
    // so the 51 px pitch across a group boundary is exactly the reference's.
    column_->insertSpacing(chipsSpacer_++, kRuleAir - kGap);
    column_->insertWidget(chipsSpacer_++, rule, 0, Qt::AlignHCenter);
    column_->insertSpacing(chipsSpacer_++, kRuleAir - kGap);
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
}

} // namespace kentos::app
