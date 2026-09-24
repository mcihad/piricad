// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/app_menu.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace kentos::app {
namespace {

// The measures, on the design's 4 px grid.
constexpr int kMenuWidth   = 660;
constexpr int kColumnWidth = 268;
constexpr int kRowHeight   = 42;
constexpr int kPaneRow     = 40;
constexpr int kIconSize    = 24;
constexpr int kRadius      = 8;
constexpr int kPad         = 8;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QColor mixed(const QColor& a, const QColor& b, double k)
{
    const auto blend = [k](float x, float y) {
        return static_cast<float>(static_cast<double>(x) * (1.0 - k) + static_cast<double>(y) * k);
    };
    return QColor::fromRgbF(blend(a.redF(), b.redF()), blend(a.greenF(), b.greenF()),
                            blend(a.blueF(), b.blueF()));
}

QFont face(int pixels, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(pixels);
    f.setWeight(weight);
    return f;
}

} // namespace

/// ONE ROW, left or right: a picture, a name, a line under it and, when it
/// leads somewhere, the chevron. A real button, so the keyboard reaches it,
/// Space and Enter press it, and a screen reader reads its name and its line.
class AppMenuRow : public QAbstractButton
{
public:
    enum class Kind : std::uint8_t { Verb, Item, Heading, Rule };

    AppMenuRow(Kind kind, QWidget* parent) : QAbstractButton(parent), kind_(kind)
    {
        setObjectName(QStringLiteral("applicationMenuRow"));
        setFocusPolicy(kind == Kind::Heading || kind == Kind::Rule ? Qt::NoFocus : Qt::StrongFocus);
        setCursor(kind == Kind::Heading || kind == Kind::Rule ? Qt::ArrowCursor
                                                              : Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void setContent(const QIcon& picture, const QString& name, const QString& line, bool more,
                    const QString& trailing = QString())
    {
        picture_  = picture;
        line_     = line;
        more_     = more;
        trailing_ = trailing;
        setText(name);
        setAccessibleName(name);
        setAccessibleDescription(line);
        update();
    }

    void setTokens(const Tokens& t)
    {
        tokens_ = t;
        update();
    }

    /// Lit as the current verb: its choices are what the pane shows.
    void setCurrent(bool on)
    {
        current_ = on;
        update();
    }

    /// Called when the pointer arrives or the keyboard lands.
    std::function<void()> onEnter;

    QSize sizeHint() const override
    {
        switch (kind_) {
        case Kind::Verb: return {kColumnWidth, kRowHeight};
        case Kind::Item: return {kMenuWidth - kColumnWidth, kPaneRow};
        case Kind::Heading: return {kMenuWidth - kColumnWidth, 28};
        case Kind::Rule: return {kColumnWidth, 9};
        }
        return {kColumnWidth, kRowHeight};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = QRectF(rect()).adjusted(4.0, 1.0, -4.0, -1.0);

        if (kind_ == Kind::Rule) {
            p.setPen(QPen(tokens_.lineSoft, 1.0));
            p.drawLine(QPointF(14.0, height() / 2.0), QPointF(width() - 14.0, height() / 2.0));
            return;
        }
        if (kind_ == Kind::Heading) {
            p.setFont(face(11, QFont::DemiBold));
            p.setPen(tokens_.textFaint);
            p.drawText(QRectF(14.0, 0.0, width() - 28.0, height()), Qt::AlignLeft | Qt::AlignBottom,
                       text());
            return;
        }

        const bool lit = isEnabled() && (current_ || underMouse() || hasFocus() || isDown());
        if (lit) {
            p.setPen(Qt::NoPen);
            p.setBrush(mixed(tokens_.ribbonBody, tokens_.accent, isDown() ? 0.22 : 0.12));
            p.drawRoundedRect(box, 5.0, 5.0);
            if (current_ && kind_ == Kind::Verb) {
                p.setBrush(tokens_.accent);
                p.drawRoundedRect(QRectF(box.left(), box.top() + 8.0, 3.0, box.height() - 16.0),
                                  1.5, 1.5);
            }
        }
        if (hasFocus()) {
            p.setPen(QPen(tokens_.accent, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 5.0, 5.0);
        }

        const int icon = kind_ == Kind::Verb ? kIconSize : 18;
        qreal x        = 16.0;
        if (!picture_.isNull()) {
            picture_.paint(&p, QRect(static_cast<int>(x), (height() - icon) / 2, icon, icon),
                           Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
            x += icon + 12.0;
        }
        const qreal right = width() - (more_ ? 30.0 : 16.0);
        const QColor ink  = isEnabled() ? tokens_.text : tokens_.textFaint;
        if (line_.isEmpty()) {
            p.setFont(face(kind_ == Kind::Verb ? 13 : 12, QFont::Medium));
            p.setPen(ink);
            p.drawText(
                QRectF(x, 0.0, right - x, height()), Qt::AlignLeft | Qt::AlignVCenter,
                p.fontMetrics().elidedText(text(), Qt::ElideRight, static_cast<int>(right - x)));
        } else {
            p.setFont(face(kind_ == Kind::Verb ? 13 : 12, QFont::Medium));
            p.setPen(ink);
            const qreal mid = height() / 2.0;
            p.drawText(
                QRectF(x, mid - 17.0, right - x, 17.0), Qt::AlignLeft | Qt::AlignBottom,
                p.fontMetrics().elidedText(text(), Qt::ElideRight, static_cast<int>(right - x)));
            p.setFont(face(11));
            p.setPen(tokens_.textFaint);
            p.drawText(
                QRectF(x, mid + 1.0, right - x, 15.0), Qt::AlignLeft | Qt::AlignTop,
                p.fontMetrics().elidedText(line_, Qt::ElideMiddle, static_cast<int>(right - x)));
        }
        if (!trailing_.isEmpty()) {
            p.setFont(face(11));
            p.setPen(tokens_.textFaint);
            p.drawText(QRectF(x, 0.0, width() - x - 16.0, height()),
                       Qt::AlignRight | Qt::AlignVCenter, trailing_);
        }
        if (more_)
            p.drawPixmap(
                QRect(width() - 26, (height() - 14) / 2, 14, 14),
                glyph_pixmap(Glyph::ChevronRight, tokens_.textDim, 14, devicePixelRatioF()));
    }

    void enterEvent(QEnterEvent* event) override
    {
        QAbstractButton::enterEvent(event);
        if (onEnter) onEnter();
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        QAbstractButton::leaveEvent(event);
        update();
    }

    void focusInEvent(QFocusEvent* event) override
    {
        QAbstractButton::focusInEvent(event);
        if (onEnter) onEnter();
        update();
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        QAbstractButton::focusOutEvent(event);
        update();
    }

private:
    Kind kind_;
    QIcon picture_;
    QString line_;
    QString trailing_;
    bool more_{false};
    bool current_{false};
    Tokens tokens_{};
};

namespace {

/// The command search at the head of the menu: the corner chip's twin, which
/// hands over to the palette.
class MenuSearch : public QAbstractButton
{
public:
    explicit MenuSearch(QWidget* parent) : QAbstractButton(parent)
    {
        setText(QObject::tr("Komut ara…"));
        setAccessibleName(QObject::tr("Komut ara"));
        setAccessibleDescription(QObject::tr("Komut listesini açar; kısayolu Ctrl+K"));
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setFixedHeight(30);
        setAttribute(Qt::WA_Hover, true);
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
        p.setPen(QPen(underMouse() || hasFocus() ? tokens_.accent : tokens_.border, 1.0));
        p.drawRoundedRect(box, 5.0, 5.0);
        p.drawPixmap(QRect(10, (height() - 16) / 2, 16, 16),
                     glyph_pixmap(Glyph::Search, tokens_.textFaint, 16, devicePixelRatioF()));
        p.setFont(face(12));
        p.setPen(tokens_.hint);
        p.drawText(QRect(34, 0, width() - 90, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        QFont mono(QStringLiteral("IBM Plex Mono"));
        mono.setPixelSize(11);
        p.setFont(mono);
        p.setPen(tokens_.hintFaint);
        p.drawText(QRect(0, 0, width() - 10, height()), Qt::AlignVCenter | Qt::AlignRight,
                   QStringLiteral("Ctrl+K"));
    }

private:
    Tokens tokens_{};
};

} // namespace

// =============================================================================
// ApplicationMenu
// =============================================================================

ApplicationMenu::ApplicationMenu(QWidget* parent) : QWidget(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("applicationMenu"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAccessibleName(tr("KentOS CAD ana menüsü"));
    setFixedWidth(kMenuWidth);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    // ---- the head: the search chip ----------------------------------------
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("applicationMenuHead"));
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(12, 12, 12, 10);
    auto* search = new MenuSearch(head);
    search_      = search;
    connect(search, &QAbstractButton::clicked, this, [this] {
        hide();
        emit searchRequested();
    });
    headRow->addWidget(search);
    outer->addWidget(head);

    // ---- the body: the verbs and the pane ---------------------------------
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("applicationMenuBody"));
    auto* bodyRow = new QHBoxLayout(body);
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(0);

    auto* left = new QWidget(body);
    left->setObjectName(QStringLiteral("applicationMenuColumn"));
    left->setFixedWidth(kColumnWidth);
    column_ = new QVBoxLayout(left);
    column_->setContentsMargins(kPad / 2, 4, kPad / 2, kPad);
    column_->setSpacing(0);
    bodyRow->addWidget(left);

    auto* right = new QWidget(body);
    right->setObjectName(QStringLiteral("applicationMenuPane"));
    pane_ = new QVBoxLayout(right);
    pane_->setContentsMargins(kPad / 2, 4, kPad, kPad);
    pane_->setSpacing(0);
    bodyRow->addWidget(right, 1);
    outer->addWidget(body, 1);

    // ---- the foot ------------------------------------------------------------
    auto* foot = new QWidget(this);
    foot->setObjectName(QStringLiteral("applicationMenuFoot"));
    auto* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(12, 9, 12, 10);
    footRow->setSpacing(8);
    outer->addWidget(foot);
}

void ApplicationMenu::setEntries(const QVector<Entry>& entries)
{
    entries_ = entries;
    rows_.clear();
    while (QLayoutItem* item = column_->takeAt(0)) {
        if (QWidget* w = item->widget(); w != nullptr) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    const Tokens& t = tokensOf(theme_);
    for (int i = 0; i < entries_.size(); ++i) {
        const Entry& e = entries_[i];
        if (e.groupStart && i > 0) {
            auto* rule = new AppMenuRow(AppMenuRow::Kind::Rule, this);
            rule->setTokens(t);
            column_->addWidget(rule);
        }
        auto* row = new AppMenuRow(AppMenuRow::Kind::Verb, this);
        row->setObjectName(QStringLiteral("applicationMenuVerb"));
        row->setTokens(t);
        row->setContent(e.action->icon(), QString(e.action->text()).remove(QLatin1Char('&')),
                        e.detail, static_cast<bool>(e.choices));
        row->setEnabled(e.action->isEnabled());
        QAction* action = e.action;
        row->onEnter    = [this, i] { setCurrent(i); };
        connect(row, &QAbstractButton::clicked, this, [this, action, i] {
            // A LIST IS OPENED, A VERB IS DONE. `Çıktı Yerleşimleri` is a list
            // and pressing it puts the keyboard on its choices; `Yazdır` is a
            // verb whose choices are its profiles, and pressing it prints.
            if (action->menu() != nullptr) {
                setCurrent(i);
                for (AppMenuRow* r : std::as_const(paneRows_))
                    if (r->isEnabled() && r->focusPolicy() != Qt::NoFocus) {
                        r->setFocus(Qt::TabFocusReason);
                        break;
                    }
                return;
            }
            choose(action);
        });
        rows_ << row;
        column_->addWidget(row);
    }
    column_->addStretch(1);
    setCurrent(-1);
}

void ApplicationMenu::setRecent(const QVector<Recent>& recent)
{
    recent_ = recent;
    if (current_ < 0 || !entries_.value(current_).choices) showRecent();
}

void ApplicationMenu::setFooter(QAction* help, QAction* about, QAction* settings, QAction* quit)
{
    auto* foot = findChild<QWidget*>(QStringLiteral("applicationMenuFoot"));
    auto* row  = qobject_cast<QHBoxLayout*>(foot->layout());
    for (Button* b : std::as_const(footer_))
        b->deleteLater();
    footer_.clear();
    const auto add = [&](QAction* action, ButtonRole role, std::optional<Glyph> glyph) {
        auto* b = new Button(role, QString(action->text()).remove(QLatin1Char('&')), glyph, foot);
        b->setControlSize(ControlSize::Compact);
        const QString keys = action->shortcut().toString(QKeySequence::NativeText);
        b->setToolTip(keys.isEmpty() ? action->toolTip()
                                     : tr("%1 (%2)").arg(action->toolTip(), keys));
        connect(b, &QAbstractButton::clicked, this, [this, action] { choose(action); });
        footer_ << b;
        return b;
    };
    row->addWidget(add(help, ButtonRole::Ghost, Glyph::Help));
    row->addWidget(add(about, ButtonRole::Ghost, Glyph::Info));
    row->addStretch(1);
    row->addWidget(add(settings, ButtonRole::Secondary, Glyph::Settings));
    row->addWidget(add(quit, ButtonRole::Secondary, std::nullopt));
    applyThemeToChildren(foot, theme_);
}

void ApplicationMenu::popupUnder(QWidget* anchor)
{
    // EACH TIME IT OPENS THE VERBS ARE READ AGAIN: the picture the theme drew
    // since, a name that changed, an action disabled since the last time.
    for (int i = 0; i < rows_.size(); ++i) {
        const Entry& e = entries_[i];
        rows_[i]->setContent(e.action->icon(), QString(e.action->text()).remove(QLatin1Char('&')),
                             e.detail, static_cast<bool>(e.choices));
        rows_[i]->setEnabled(e.action->isEnabled());
    }
    setCurrent(-1);
    adjustSize();
    QPoint at = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    if (const QScreen* screen = anchor->screen(); screen != nullptr) {
        const QRect room = screen->availableGeometry();
        at.setX(std::clamp(at.x(), room.left(), std::max(room.left(), room.right() - width())));
        at.setY(std::min(at.y(), std::max(room.top(), room.bottom() - height())));
    }
    move(at);
    show();
    raise();
    activateWindow();
    if (!rows_.isEmpty()) rows_.front()->setFocus(Qt::PopupFocusReason);
}

void ApplicationMenu::applyTheme(ThemeMode mode)
{
    theme_          = mode;
    const Tokens& t = tokensOf(mode);
    for (QAbstractButton* button : findChildren<QAbstractButton*>())
        if (auto* row = dynamic_cast<AppMenuRow*>(button); row != nullptr) row->setTokens(t);
    if (auto* chip = dynamic_cast<MenuSearch*>(search_); chip != nullptr) chip->setTokens(t);
    for (Button* b : std::as_const(footer_))
        b->applyTheme(mode);
    update();
}

void ApplicationMenu::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath shape;
    shape.addRoundedRect(box, kRadius, kRadius);
    p.setClipPath(shape);
    // The column on the body's own ground, the pane a step back, the head and
    // the foot on the tab row's tone — the three regions a glance can tell apart.
    p.fillRect(rect(), t.ribbonBody);
    const QWidget* pane = findChild<QWidget*>(QStringLiteral("applicationMenuPane"));
    const QWidget* head = findChild<QWidget*>(QStringLiteral("applicationMenuHead"));
    const QWidget* foot = findChild<QWidget*>(QStringLiteral("applicationMenuFoot"));
    const QColor back   = mixed(t.ribbonBody, t.ribbonTabs, 0.45);
    if (pane != nullptr) p.fillRect(QRect(pane->mapTo(this, QPoint(0, 0)), pane->size()), back);
    if (head != nullptr) p.fillRect(QRect(QPoint(0, 0), QSize(width(), head->height())), back);
    if (foot != nullptr)
        p.fillRect(QRect(foot->mapTo(this, QPoint(0, 0)), QSize(width(), foot->height())), back);
    p.setClipping(false);

    p.setPen(QPen(t.lineSoft, 1.0));
    if (head != nullptr)
        p.drawLine(QPointF(0.0, head->height() - 0.5), QPointF(width(), head->height() - 0.5));
    if (foot != nullptr) {
        const qreal y = foot->mapTo(this, QPoint(0, 0)).y() + 0.5;
        p.drawLine(QPointF(0.0, y), QPointF(width(), y));
    }
    if (pane != nullptr) {
        const qreal x      = pane->mapTo(this, QPoint(0, 0)).x() + 0.5;
        const qreal top    = head != nullptr ? head->height() : 0.0;
        const qreal bottom = foot != nullptr ? foot->mapTo(this, QPoint(0, 0)).y() : height();
        p.drawLine(QPointF(x, top), QPointF(x, bottom));
    }
    p.setPen(QPen(t.border, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(shape);
}

void ApplicationMenu::keyPressEvent(QKeyEvent* event)
{
    // THE KEYBOARD WALKS IT LIKE A MENU: up and down along a column, right into
    // the pane, left back to the verbs, Esc out.
    QWidget* at                    = focusWidget();
    auto* row                      = dynamic_cast<AppMenuRow*>(at);
    const bool inPane              = row != nullptr && paneRows_.contains(row);
    const QList<AppMenuRow*>& list = inPane ? paneRows_ : rows_;
    const auto step                = [&](int by) {
        if (list.isEmpty()) return;
        int i = row != nullptr ? static_cast<int>(list.indexOf(row)) : -1;
        for (int n = 0; n < list.size(); ++n) {
            i = (i + by + static_cast<int>(list.size())) % static_cast<int>(list.size());
            if (list[i]->isEnabled() && list[i]->focusPolicy() != Qt::NoFocus) {
                list[i]->setFocus(Qt::TabFocusReason);
                return;
            }
        }
    };
    switch (event->key()) {
    case Qt::Key_Escape: hide(); return;
    case Qt::Key_Down: step(+1); return;
    case Qt::Key_Up: step(-1); return;
    case Qt::Key_Right:
        if (!inPane)
            for (AppMenuRow* r : std::as_const(paneRows_))
                if (r->isEnabled() && r->focusPolicy() != Qt::NoFocus) {
                    r->setFocus(Qt::TabFocusReason);
                    return;
                }
        return;
    case Qt::Key_Left:
        if (inPane && current_ >= 0 && current_ < rows_.size())
            rows_[current_]->setFocus(Qt::TabFocusReason);
        return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

void ApplicationMenu::setCurrent(int index)
{
    if (index == current_ && index >= 0) return;
    current_ = index;
    for (int i = 0; i < rows_.size(); ++i)
        rows_[i]->setCurrent(i == index);
    if (index >= 0 && index < entries_.size() && entries_[index].choices)
        showChoices(QString(entries_[index].action->text()).remove(QLatin1Char('&')),
                    entries_[index].choices());
    else
        showRecent();
}

void ApplicationMenu::clearPane()
{
    paneRows_.clear();
    // HIDDEN AT ONCE, deleted later: a row the pointer has just left may still
    // be in the middle of its own event, and a deleted-later one that stayed
    // visible was painted under its replacement.
    while (QLayoutItem* item = pane_->takeAt(0)) {
        if (QWidget* w = item->widget(); w != nullptr) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
}

void ApplicationMenu::showRecent()
{
    clearPane();
    const Tokens& t = tokensOf(theme_);
    auto* heading   = new AppMenuRow(AppMenuRow::Kind::Heading, this);
    heading->setTokens(t);
    heading->setContent(QIcon(), tr("Son kullanılan belgeler"), QString(), false);
    pane_->addWidget(heading);
    if (recent_.isEmpty()) {
        auto* none = new AppMenuRow(AppMenuRow::Kind::Item, this);
        none->setTokens(t);
        none->setContent(QIcon(), tr("Henüz bir belge yok"),
                         tr("Açtığınız ve kaydettiğiniz çizimler burada listelenir"), false);
        none->setEnabled(false);
        pane_->addWidget(none);
    }
    const QIcon document = colour_icon(Glyph::Document,
                                       GlyphInks{t.iconInk, t.iconShape, t.iconFill, t.iconCut,
                                                 t.iconNote, t.iconData, t.iconAdd, t.iconPaper},
                                       18);
    for (const Recent& r : std::as_const(recent_)) {
        auto* row = new AppMenuRow(AppMenuRow::Kind::Item, this);
        row->setObjectName(QStringLiteral("applicationMenuRecent"));
        row->setTokens(t);
        row->setContent(document, r.name, r.folder, false, r.when);
        row->setToolTip(r.path);
        const QString path = r.path;
        connect(row, &QAbstractButton::clicked, this, [this, path] {
            hide();
            emit recentChosen(path);
        });
        paneRows_ << row;
        pane_->addWidget(row);
    }
    pane_->addStretch(1);
}

void ApplicationMenu::showChoices(const QString& title, const QList<QAction*>& choices)
{
    clearPane();
    const Tokens& t = tokensOf(theme_);
    auto* heading   = new AppMenuRow(AppMenuRow::Kind::Heading, this);
    heading->setTokens(t);
    heading->setContent(QIcon(), title, QString(), false);
    pane_->addWidget(heading);
    for (QAction* action : choices) {
        if (action->isSeparator()) {
            auto* rule = new AppMenuRow(AppMenuRow::Kind::Rule, this);
            rule->setTokens(t);
            pane_->addWidget(rule);
            continue;
        }
        // A SUBMENU IS SHOWN IN PLACE, one level deep, under its own name: the
        // pane is the menu's second column and has no third.
        const QString name = QString(action->text()).remove(QLatin1Char('&')).trimmed();
        if (QMenu* sub = action->menu(); sub != nullptr) {
            auto* group = new AppMenuRow(AppMenuRow::Kind::Heading, this);
            group->setTokens(t);
            group->setContent(QIcon(), name, QString(), false);
            pane_->addWidget(group);
            for (QAction* inner : sub->actions()) {
                if (inner->isSeparator() || inner->menu() != nullptr) continue;
                auto* row = new AppMenuRow(AppMenuRow::Kind::Item, this);
                row->setObjectName(QStringLiteral("applicationMenuChoice"));
                row->setTokens(t);
                row->setContent(inner->icon(), QString(inner->text()).remove(QLatin1Char('&')),
                                QString(), false);
                row->setEnabled(inner->isEnabled());
                // A GUARDED POINTER: the list is rebuilt from the document, and a
                // row must not press an action a rebuild has since thrown away.
                connect(row, &QAbstractButton::clicked, this,
                        [this, guard = QPointer<QAction>(inner)] { choose(guard); });
                paneRows_ << row;
                pane_->addWidget(row);
            }
            continue;
        }
        // A disabled row without a verb of its own is a list's heading.
        if (!action->isEnabled() && action->menu() == nullptr) {
            auto* sub = new AppMenuRow(AppMenuRow::Kind::Heading, this);
            sub->setTokens(t);
            sub->setContent(QIcon(), name, QString(), false);
            pane_->addWidget(sub);
            continue;
        }
        // WHAT IT IS, without saying its name twice: a tip that opens with the
        // name or the command word (`A4 Dikey — …`, `ÇIKTIYERLEŞİMİ … — …`)
        // keeps only what follows.
        QString line = action->toolTip();
        if (line == action->text() || line == name) line.clear();
        if (const qsizetype dash = line.indexOf(QStringLiteral(" — ")); dash > 0) {
            const QString lead = line.left(dash);
            if (lead == name || lead.section(QLatin1Char(' '), 0, 0).toUpper() ==
                                    lead.section(QLatin1Char(' '), 0, 0))
                line = line.mid(dash + 3);
        }
        if (!line.isEmpty()) line[0] = line[0].toUpper();
        auto* row = new AppMenuRow(AppMenuRow::Kind::Item, this);
        row->setObjectName(QStringLiteral("applicationMenuChoice"));
        row->setTokens(t);
        row->setContent(action->icon(), name, line, false);
        connect(row, &QAbstractButton::clicked, this,
                [this, guard = QPointer<QAction>(action)] { choose(guard); });
        paneRows_ << row;
        pane_->addWidget(row);
    }
    pane_->addStretch(1);
}

void ApplicationMenu::choose(QAction* action)
{
    hide();
    if (action != nullptr && action->isEnabled()) action->trigger();
}

} // namespace kentos::app
