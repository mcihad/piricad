// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/ribbon.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/drawing_catalogs.hpp"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QAction>
#include <QActionEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace kentos::app {
namespace {

// The chips keep the numbers the title bar gave them (`design.md` §7): the
// search chip is 190 × 24, the initials chip a 22 px circle, 8 px apart.
constexpr int kSearchWidth = 190;
constexpr int kChipHeight  = 22;
constexpr int kChipGap     = 8;
constexpr int kCornerPad   = 10;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

} // namespace

/// The command search: an input-looking chip that opens the palette. It is not a
/// `QLineEdit` because it never accepts typing in place — clicking it raises the
/// palette, which is where the typing happens.
class SearchChip : public QWidget
{
public:
    SearchChip(QWidget* parent, std::function<void()> pressed)
        : QWidget(parent), pressed_(std::move(pressed))
    {
        setObjectName(QStringLiteral("titleSearch"));
        setFixedSize(kSearchWidth, kChipHeight + 2);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(tr("Komut ara"));
        setAccessibleDescription(tr("Komut listesini açar; kısayolu Ctrl+K"));
    }

    void setTokens(const Tokens& t)
    {
        tokens_ = t;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(tokens_.bgInput);
        p.setPen(QPen(hover_ ? tokens_.accent : tokens_.separator, 1.0));
        p.drawRoundedRect(box, 4.0, 4.0);

        p.drawPixmap(QRect(8, (height() - 15) / 2, 15, 15),
                     glyph_pixmap(Glyph::Search, tokens_.textFaint, 15, devicePixelRatioF()));

        QFont label = font();
        label.setPointSizeF(8.6);
        p.setFont(label);
        p.setPen(tokens_.hint);
        p.drawText(QRect(29, 0, width() - 62, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   tr("Komut ara…"));

        QFont mono(QStringLiteral("IBM Plex Mono"));
        mono.setPointSizeF(7.5);
        mono.setStyleHint(QFont::Monospace);
        p.setFont(mono);
        p.setPen(tokens_.hintFaint);
        p.drawText(QRect(0, 0, width() - 8, height()), Qt::AlignVCenter | Qt::AlignRight,
                   QStringLiteral("Ctrl+K"));
    }

    void enterEvent(QEnterEvent*) override
    {
        hover_ = true;
        update();
    }

    void leaveEvent(QEvent*) override
    {
        hover_ = false;
        update();
    }

    void mousePressEvent(QMouseEvent*) override
    {
        if (pressed_) pressed_();
    }

private:
    std::function<void()> pressed_;
    Tokens tokens_{};
    bool hover_ = false;
};

/// The round initials chip at the right end.
class UserChip : public QWidget
{
public:
    explicit UserChip(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("titleUser"));
        setFixedSize(kChipHeight, kChipHeight);
    }

    void setTokens(const Tokens& t)
    {
        tokens_ = t;
        update();
    }

    void setInitials(const QString& text)
    {
        initials_ = text;
        setAccessibleName(tr("Kullanıcı %1").arg(text));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(tokens_.accent);
        p.drawEllipse(rect());

        QFont f = font();
        f.setPointSizeF(7.9);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(tokens_.onAccentDark);
        p.drawText(rect(), Qt::AlignCenter, initials_);
    }

private:
    Tokens tokens_{};
    QString initials_ = QStringLiteral("PC");
};

// =============================================================================
// RibbonElementFactory
// =============================================================================

namespace {

/// Where every ribbon button's rich tip comes from (`setTipSource`).
std::function<QString(const QAction*)>& tipSource()
{
    static std::function<QString(const QAction*)> source;
    return source;
}

/// A ribbon button that opens its list from the keyboard as well as from the
/// arrow — Down or F4, what a menu button does in every desktop toolkit — and
/// shows the ribbon's rich tip instead of the action's one-line tooltip.
class KeyedRibbonButton : public SARibbonToolButton
{
public:
    using SARibbonToolButton::SARibbonToolButton;

    /// Whether the button opens a list: its own menu or its action's.
    bool listed() const
    {
        return menu() != nullptr ||
               (defaultAction() != nullptr && defaultAction()->menu() != nullptr);
    }

protected:
    /// THE NAME FOLLOWS THE FACE, for a screen reader (`ui.md` R22): a family's
    /// button is renamed whenever another member takes the face, and Qt tells
    /// the button through this event.
    void actionEvent(QActionEvent* event) override
    {
        SARibbonToolButton::actionEvent(event);
        // IN THE TAB CHAIN, so every button the ribbon shows is reachable
        // without the mouse (`.claude/ui.md` R21). Set here because SARibbon
        // takes the focus policy away right after the factory makes the button
        // and before it hands the button its action; a click still does not
        // take the focus from the canvas, which is where the drawing's keys go.
        setFocusPolicy(Qt::TabFocus);
        if (const QAction* face = defaultAction(); face != nullptr) {
            setAccessibleName(QString(face->text()).remove(QLatin1Char('&')));
            setAccessibleDescription(face->toolTip());
        }
    }

    bool event(QEvent* event) override
    {
        // THE TIP IS COMPOSED, NOT STORED: the title, what the command does in
        // the registry's words, how to type it, its shortcut. The action keeps
        // its plain tooltip, which is what a screen reader reads.
        if (event->type() == QEvent::ToolTip && defaultAction() != nullptr) {
            const QString tip = RibbonElementFactory::tipFor(defaultAction());
            if (!tip.isEmpty()) {
                QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), tip, this, rect());
                return true;
            }
        }
        return SARibbonToolButton::event(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const bool opens = event->key() == Qt::Key_Down || event->key() == Qt::Key_F4;
        if (opens && listed()) {
            showMenu();
            return;
        }
        SARibbonToolButton::keyPressEvent(event);
    }
};

/// WHAT THE ACCESSIBILITY LAYER IS HANDED FOR A RIBBON BUTTON.
///
/// Qt's own interface gives a split button — a face that runs, an arrow that
/// lists — only `ShowMenu`, so a screen reader or a switch user could open the
/// list and never press the face. This one offers `Press` first (a platform
/// that takes one action takes the first it knows), `Toggle` for a tool that
/// lights, and `ShowMenu` for one with a list. Every press is a `click()`, the
/// mouse's own road: it runs the command AND leaves the light where a hand
/// would have. `QAccessibleWidget` is public API and answers name, description,
/// rect and parent from the widget; Qt's `QAccessibleToolButton` is private.
class RibbonButtonAccessible : public QAccessibleWidget
{
public:
    explicit RibbonButtonAccessible(KeyedRibbonButton* button)
        : QAccessibleWidget(button, QAccessible::PushButton)
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
        reported.hasPopup           = button()->listed();
        return reported;
    }

    QStringList actionNames() const override
    {
        if (!button()->isEnabled()) return {};
        QStringList names{pressAction()};
        if (button()->isCheckable()) names << toggleAction();
        if (button()->listed()) names << showMenuAction();
        return names;
    }

    void doAction(const QString& name) override
    {
        if (!button()->isEnabled() || !actionNames().contains(name)) return;
        if (name == showMenuAction())
            button()->showMenu();
        else
            button()->click();
    }

    QStringList keyBindingsForAction(const QString& /*name*/) const override { return {}; }

private:
    KeyedRibbonButton* button() const { return static_cast<KeyedRibbonButton*>(object()); }
};

/// Hands `RibbonButtonAccessible` to Qt for the ribbon's buttons and nothing
/// else: they have no `Q_OBJECT` of their own and arrive under SARibbon's class
/// name, so the cast is what tells them apart; null lets Qt carry on.
QAccessibleInterface* ribbonButtonInterface(const QString& key, QObject* object)
{
    if (key != QLatin1String("SARibbonToolButton")) return nullptr;
    auto* button = dynamic_cast<KeyedRibbonButton*>(object);
    return button == nullptr ? nullptr : new RibbonButtonAccessible(button);
}

} // namespace

SARibbonToolButton* RibbonElementFactory::createRibbonToolButton(QWidget* parent)
{
    // Installed once, the first time a button is made; Qt keeps the list.
    static const bool installed = [] {
        QAccessible::installFactory(&ribbonButtonInterface);
        return true;
    }();
    (void)installed;
    return new KeyedRibbonButton(parent);
}

void RibbonElementFactory::setTipSource(std::function<QString(const QAction*)> source)
{
    tipSource() = std::move(source);
}

QString RibbonElementFactory::tipFor(const QAction* action)
{
    return tipSource() ? tipSource()(action) : QString();
}

// =============================================================================
// RibbonBar
// =============================================================================

RibbonBar::RibbonBar(QWidget* parent) : SARibbonBar(parent) {}

void RibbonBar::paintContextCategoryTab(QPainter& painter, const QString& /*title*/,
                                        const QRect& contextRect, const QColor& color)
{
    // THE CAP sits in the air above the folder tab (the tab stands 6 px off the
    // top of its row, `ribbonStyleSheet`), 3 px tall, as wide as the tab's own
    // edge — visible whether the tab is chosen or not.
    const SARibbonTabBar* tabs = const_cast<RibbonBar*>(this)->ribbonTabBar();
    const int top              = tabs != nullptr ? tabs->y() + 2 : contextRect.top();
    const QRectF cap(contextRect.left() + 2.0, top, contextRect.width() - 4.0, 3.0);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(cap, 1.5, 1.5);
    painter.restore();
}

// =============================================================================
// RibbonAppButton
// =============================================================================

RibbonAppButton::RibbonAppButton(const QString& text, QWidget* parent)
    : SARibbonApplicationButton(text, parent)
{}

QSize RibbonAppButton::sizeHint() const
{
    const QSize natural = SARibbonApplicationButton::sizeHint();
    const auto* bar     = qobject_cast<const SARibbonBar*>(parentWidget());
    const int row       = bar != nullptr ? bar->titleBarHeight() - 2 : natural.height();
    // The layout's aspect factor, undone: height × (w / h) / 1,5 = w.
    constexpr qreal kLayoutAspect = 1.5;
    return {std::max(natural.width(), minimumWidth()),
            std::max(1, static_cast<int>(std::lround(row / kLayoutAspect)))};
}

// =============================================================================
// RibbonFamily
// =============================================================================

RibbonFamily::RibbonFamily(const QList<QAction*>& members, QObject* parent)
    : QObject(parent), members_(members)
{
    head_ = new QAction(this);
    // A FAMILY OF TOOLS LIGHTS, a family of one-shot commands does not: the
    // guides are placed and done, and a button that looked pressed would say a
    // tool was waiting that is not.
    head_->setCheckable(std::any_of(members_.begin(), members_.end(),
                                    [](const QAction* m) { return m->isCheckable(); }));
    head_->setObjectName(QStringLiteral("ribbonFamily"));

    // THE MENU BELONGS TO THE WINDOW, not to the button that shows it: a panel
    // that folds when the window narrows rebuilds its buttons, and a menu owned
    // by the old button would go with it.
    menu_ = new QMenu(qobject_cast<QWidget*>(parent));
    for (QAction* member : members_) {
        menu_->addAction(member);
        // THE FACE IS THE MEMBER LAST USED — from the menu, from the canvas's
        // re-arm, from anywhere — so the button offers what the hand did last.
        connect(member, &QAction::triggered, this, [this, member] { adopt(member); });
        connect(member, &QAction::toggled, this, [this] { syncChecked(); });
        connect(member, &QAction::changed, this, [this, member] {
            if (member == face_) adopt(member);
        });
    }
    head_->setMenu(menu_);
    connect(head_, &QAction::triggered, this, [this] {
        if (face_ != nullptr) face_->trigger();
        syncChecked();
    });
    adopt(members_.value(0));
}

void RibbonFamily::setFixedLabel(const QString& label)
{
    fixedLabel_ = label;
    if (face_ != nullptr) adopt(face_);
}

void RibbonFamily::adopt(QAction* member)
{
    if (member == nullptr) return;
    face_ = member;
    // The word on the button: the member's own short word, else the family's,
    // else the member's full name.
    const QString shortLabel = member->property(kRibbonShortLabel).toString();
    head_->setText(!shortLabel.isEmpty()    ? shortLabel
                   : !fixedLabel_.isEmpty() ? fixedLabel_
                                            : member->text());
    head_->setIcon(member->icon());
    // The glyph rides along in data(), so the theme walk re-tints the head the
    // way it re-tints every other action (`MainWindow::applyTheme`).
    head_->setData(member->data());

    QStringList others;
    for (const QAction* m : std::as_const(members_))
        if (m != member) others << QString(m->text()).remove(QLatin1Char('&'));
    QString tip = member->toolTip();
    if (!others.isEmpty())
        tip += QStringLiteral("\n") + tr("Oka basın: %1").arg(others.join(QStringLiteral(", ")));
    head_->setToolTip(tip);
    head_->setStatusTip(member->statusTip());
    syncChecked();
}

void RibbonFamily::syncChecked()
{
    bool running = false;
    bool enabled = false;
    for (const QAction* m : std::as_const(members_)) {
        running = running || m->isChecked();
        enabled = enabled || m->isEnabled();
    }
    head_->setChecked(running);
    head_->setEnabled(enabled);
}

// =============================================================================
// RibbonLayerBox
// =============================================================================

namespace {

// The marks a layer row wears, left to right: its eye, its padlock (only when
// locked; the place is kept so the swatches line up) and its colour.
constexpr int kMarkEye     = 14;
constexpr int kMarkLock    = 12;
constexpr int kMarkSwatch  = 12;
constexpr int kMarkGap     = 3;
constexpr int kMarksWidth  = kMarkEye + kMarkGap + kMarkLock + kMarkGap + kMarkSwatch;
constexpr int kMarksHeight = 16;
constexpr qreal kMarkDpr   = 2.0;

/// A colour square with a hairline edge, so white reads on white.
void paintSwatch(QPainter& p, const QRectF& box, const QColor& colour, const Tokens& t)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(t.separator, 1.0));
    if (colour.isValid()) {
        p.setBrush(colour);
        p.drawRoundedRect(box, 2.0, 2.0);
    } else {
        // NONE, the way every paint program says it: an empty square struck out.
        p.setBrush(t.bgInput);
        p.drawRoundedRect(box, 2.0, 2.0);
        p.setPen(QPen(t.danger, 1.2));
        p.drawLine(box.bottomLeft() + QPointF(1.5, -1.5), box.topRight() + QPointF(-1.5, 1.5));
    }
    p.restore();
}

QIcon layerMarks(const RibbonLayerRow& row, const Tokens& t)
{
    QPixmap pm(QSize(kMarksWidth, kMarksHeight) * kMarkDpr);
    pm.setDevicePixelRatio(kMarkDpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.drawPixmap(QRect(0, (kMarksHeight - kMarkEye) / 2, kMarkEye, kMarkEye),
                 glyph_pixmap(row.visible ? Glyph::Eye : Glyph::EyeOff,
                              row.visible ? t.textDim : t.textFaint, kMarkEye, kMarkDpr));
    const int lockX = kMarkEye + kMarkGap;
    if (row.locked)
        p.drawPixmap(QRect(lockX, (kMarksHeight - kMarkLock) / 2, kMarkLock, kMarkLock),
                     glyph_pixmap(Glyph::Lock, t.warn, kMarkLock, kMarkDpr));
    const int swatchX = lockX + kMarkLock + kMarkGap;
    paintSwatch(p,
                QRectF(swatchX + 0.5, (kMarksHeight - kMarkSwatch) / 2.0 + 0.5, kMarkSwatch - 1.0,
                       kMarkSwatch - 1.0),
                row.colour, t);
    return QIcon(pm);
}

} // namespace

RibbonLayerBox::RibbonLayerBox(QWidget* parent) : ComboBox(parent)
{
    setObjectName(QStringLiteral("ribbonLayerBox"));
    setControlSize(ControlSize::Compact);
    setIconSize(QSize(kMarksWidth, kMarksHeight));
    setAccessibleName(tr("Katman"));
    connect(this, &QComboBox::activated, this, [this](int index) {
        if (index >= 0 && index < rows_.size()) emit layerPicked(rows_[index].name);
    });
}

void RibbonLayerBox::setRows(const QVector<RibbonLayerRow>& rows, int shown, const QString& mixed)
{
    const QSignalBlocker quiet(this);
    rows_ = rows;
    clear();
    for (const RibbonLayerRow& row : std::as_const(rows_))
        addItem(row.name);
    repaintMarks();
    setPlaceholderText(mixed);
    setCurrentIndex(shown >= 0 && shown < rows_.size() ? shown : -1);
}

void RibbonLayerBox::applyTheme(ThemeMode mode)
{
    mode_ = mode;
    ComboBox::applyTheme(mode);
    repaintMarks();
}

void RibbonLayerBox::repaintMarks()
{
    const Tokens& t = tokensOf(mode_);
    for (int i = 0; i < rows_.size(); ++i) {
        setItemIcon(i, layerMarks(rows_[i], t));
        QString state;
        if (!rows_[i].visible) state += tr("gizli");
        if (rows_[i].locked) state += (state.isEmpty() ? QString() : tr(", ")) + tr("kilitli");
        setItemData(i, state.isEmpty() ? rows_[i].name : tr("%1 (%2)").arg(rows_[i].name, state),
                    Qt::ToolTipRole);
    }
    update();
}

// =============================================================================
// RibbonColourBox
// =============================================================================

namespace {
constexpr int kColourSwatch = 14;
} // namespace

RibbonColourBox::RibbonColourBox(QWidget* parent) : ComboBox(parent)
{
    setObjectName(QStringLiteral("ribbonColourBox"));
    setControlSize(ControlSize::Compact);
    setIconSize(QSize(kColourSwatch, kColourSwatch));
    addItem(QString());
}

void RibbonColourBox::setShown(const QColor& colour, const QString& label)
{
    colour_ = colour;
    label_  = label;
    const QSignalBlocker quiet(this);
    setItemText(0, label);
    repaintSwatch();
}

void RibbonColourBox::showPopup()
{
    emit menuRequested();
}

void RibbonColourBox::applyTheme(ThemeMode mode)
{
    mode_ = mode;
    ComboBox::applyTheme(mode);
    repaintSwatch();
}

void RibbonColourBox::repaintSwatch()
{
    QPixmap pm(QSize(kColourSwatch, kColourSwatch) * kMarkDpr);
    pm.setDevicePixelRatio(kMarkDpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paintSwatch(p, QRectF(0.5, 0.5, kColourSwatch - 1.0, kColourSwatch - 1.0), colour_,
                tokensOf(mode_));
    p.end();
    setItemIcon(0, QIcon(pm));
    update();
}

// =============================================================================
// Pictures of data: hatch patterns and text anchors
// =============================================================================

QIcon hatch_swatch(const command::HatchPattern& pattern, const QColor& ink, const QColor& ground,
                   QSize size)
{
    constexpr qreal kDpr = 2.0;
    QPixmap pm(size * kDpr);
    pm.setDevicePixelRatio(kDpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box(0.5, 0.5, size.width() - 1.0, size.height() - 1.0);
    QColor edge = ink;
    edge.setAlphaF(0.35F);
    p.setPen(QPen(edge, 1.0));
    p.setBrush(ground);
    p.drawRoundedRect(box, 3.0, 3.0);

    const QRectF inside = box.adjusted(2.0, 2.0, -2.0, -2.0);
    if (pattern.families.empty()) { // SOLID
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawRoundedRect(inside, 1.5, 1.5);
        return QIcon(pm);
    }
    p.setClipRect(inside);

    // THE SCALE THIS PATTERN IS SHOWN AT: its closest two parallel lines 4,5 px
    // apart, and its shortest dash at least 3 px long, so ANSI31 and GRASS are
    // both legible in the same box though their spacings differ tenfold.
    constexpr double kPi = 3.14159265358979323846;
    std::map<std::int64_t, std::vector<double>> across; // angle -> line positions mod period
    std::map<std::int64_t, double> period;
    double shortestDash = std::numeric_limits<double>::infinity();
    for (const core::HatchDef::Family& f : pattern.families) {
        const double a  = static_cast<double>(f.angle_udeg) * 1e-6 * kPi / 180.0;
        const double oy = std::abs(static_cast<double>(f.offset_y_um));
        if (oy <= 0.0) continue;
        const double nx = -std::sin(a);
        const double ny = std::cos(a);
        const double at =
            static_cast<double>(f.base_x_um) * nx + static_cast<double>(f.base_y_um) * ny;
        across[f.angle_udeg].push_back(std::fmod(std::fmod(at, oy) + oy, oy));
        period[f.angle_udeg] = oy;
        for (const std::int64_t d : f.dashes_um)
            if (d > 0) shortestDash = std::min(shortestDash, static_cast<double>(d));
    }
    double gap = std::numeric_limits<double>::infinity();
    for (auto& [angle, positions] : across) {
        std::sort(positions.begin(), positions.end());
        const double oy = period[angle];
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const double next =
                i + 1 < positions.size() ? positions[i + 1] : positions.front() + oy;
            if (next - positions[i] > 1e-6) gap = std::min(gap, next - positions[i]);
        }
    }
    if (!std::isfinite(gap)) return QIcon(pm);
    double scale = 4.5 / gap; // px per pattern micrometre
    if (std::isfinite(shortestDash)) scale = std::max(scale, 3.0 / shortestDash);

    const QPointF centre = inside.center();
    const double reach   = std::hypot(inside.width(), inside.height()) / 2.0 / scale; // µm
    const auto toPx      = [&](double u, double v) {
        return QPointF(centre.x() + u * scale, centre.y() - v * scale);
    };
    p.setPen(QPen(ink, 1.0, Qt::SolidLine, Qt::FlatCap));
    for (const core::HatchDef::Family& f : pattern.families) {
        const double a  = static_cast<double>(f.angle_udeg) * 1e-6 * kPi / 180.0;
        const double dx = std::cos(a);
        const double dy = std::sin(a);
        const double nx = -dy;
        const double ny = dx;
        const double ox = static_cast<double>(f.offset_x_um);
        const double oy = static_cast<double>(f.offset_y_um);
        if (std::abs(oy) <= 0.0) continue;
        const double bx  = static_cast<double>(f.base_x_um);
        const double by  = static_cast<double>(f.base_y_um);
        const double at  = bx * nx + by * ny;
        const auto first = static_cast<long>(std::floor((-reach - at) / std::abs(oy))) - 1;
        const auto last  = static_cast<long>(std::ceil((reach - at) / std::abs(oy))) + 1;
        double cycle     = 0.0;
        for (const std::int64_t d : f.dashes_um)
            cycle += std::abs(static_cast<double>(d));
        for (long k = first; k <= last; ++k) {
            const double kk = static_cast<double>(oy > 0 ? k : -k);
            const double px = bx + kk * (ox * dx + oy * nx);
            const double py = by + kk * (ox * dy + oy * ny);
            const double tc = -(px * dx + py * dy); // the swatch centre, along this line
            if (f.dashes_um.empty() || cycle <= 0.0) {
                p.drawLine(toPx(px + (tc - reach) * dx, py + (tc - reach) * dy),
                           toPx(px + (tc + reach) * dx, py + (tc + reach) * dy));
                continue;
            }
            double t = std::floor((tc - reach) / cycle) * cycle;
            while (t < tc + reach) {
                for (const std::int64_t d : f.dashes_um) {
                    const double len = std::abs(static_cast<double>(d));
                    if (d > 0)
                        p.drawLine(toPx(px + t * dx, py + t * dy),
                                   toPx(px + (t + len) * dx, py + (t + len) * dy));
                    else if (d == 0) {
                        p.save();
                        p.setPen(Qt::NoPen);
                        p.setBrush(ink);
                        p.drawEllipse(toPx(px + t * dx, py + t * dy), 0.75, 0.75);
                        p.restore();
                    }
                    t += len;
                }
            }
        }
    }
    return QIcon(pm);
}

QIcon anchor_icon(int column, int row, const QColor& ink, const QColor& mark)
{
    constexpr qreal kDpr = 2.0;
    constexpr int kSide  = 20;
    QPixmap pm(QSize(kSide, kSide) * kDpr);
    pm.setDevicePixelRatio(kDpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Three lines of text, set the way the column sets them.
    constexpr qreal kLeft  = 3.5;
    constexpr qreal kRight = 16.5;
    const qreal lengths[]  = {13.0, 9.0, 11.0};
    const qreal ys[]       = {6.0, 10.0, 14.0};
    p.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 3; ++i) {
        qreal x0 = kLeft;
        if (column == 1) x0 = (kLeft + kRight - lengths[i]) / 2.0;
        if (column == 2) x0 = kRight - lengths[i];
        p.drawLine(QPointF(x0, ys[i]), QPointF(x0 + lengths[i], ys[i]));
    }
    // And the point, where it sits on the block: top, middle or baseline.
    const qreal mx = column == 0 ? kLeft : column == 1 ? (kLeft + kRight) / 2.0 : kRight;
    const qreal my = row == 0 ? 3.2 : row == 1 ? 10.0 : 16.8;
    p.setPen(Qt::NoPen);
    p.setBrush(mark);
    p.drawEllipse(QPointF(mx, my), 2.3, 2.3);
    return QIcon(pm);
}

// =============================================================================
// ShellCorner
// =============================================================================

ShellCorner::ShellCorner(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("shellCorner"));
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, kCornerPad, 0);
    row->setSpacing(0);

    search_ = new SearchChip(this, [this] { emit searchRequested(); });
    row->addWidget(search_, 0, Qt::AlignVCenter);
    row->addSpacing(kChipGap);

    user_ = new UserChip(this);
    row->addWidget(user_, 0, Qt::AlignVCenter);
}

void ShellCorner::setUserInitials(const QString& initials)
{
    user_->setInitials(initials);
}

void ShellCorner::applyTheme(ThemeMode mode)
{
    const Tokens& t = tokensOf(mode);
    search_->setTokens(t);
    user_->setTokens(t);
}

} // namespace kentos::app
