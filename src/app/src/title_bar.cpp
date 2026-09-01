// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/title_bar.hpp"

#include "piricad/app/icons.hpp"
#include "piricad/app/tokens.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace piricad::app {
namespace {

// design.md §7 and the reference mockup, in the mockup's own numbers.
// 34 px INCLUDING the rule beneath it — the reference measures 33 rows of
// gradient and then one row of `lineHard`. Every band in the shell is border-box
// like this, so a bar that adds its rule on top of its declared height pushes
// everything below it down by a pixel.
constexpr int kBarHeight   = 34;
constexpr int kGradient    = 33;
constexpr int kSidePad     = 10;
constexpr int kSearchWidth = 190;
constexpr int kChipHeight  = 22;
constexpr int kRightGap    = 8;

} // namespace

/// The command search: an input-looking chip that opens the palette. It is not a
/// `QLineEdit` because it never accepts typing in place — clicking it raises the
/// palette, which is where the typing happens.
class SearchChip : public QWidget
{
public:
    explicit SearchChip(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("titleSearch"));
        setFixedSize(kSearchWidth, kChipHeight + 2);
        setCursor(Qt::PointingHandCursor);
    }

    void setTokens(const Tokens& t)
    {
        tokens_ = t;
        update();
    }

    void setShortcut(const QString& text) { shortcut_ = text; }

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
                   shortcut_);
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
        emit static_cast<TitleBar*>(parentWidget())->searchRequested();
    }

private:
    Tokens tokens_{};
    QString shortcut_ = QStringLiteral("Ctrl+K");
    bool hover_       = false;
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

TitleBar::TitleBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kBarHeight);
    setAttribute(Qt::WA_StyledBackground, false);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kSidePad, 0, kSidePad, kBarHeight - kGradient);
    row->setSpacing(0);

    menus_ = new QMenuBar(this);
    menus_->setObjectName(QStringLiteral("shellMenuBar"));
    menus_->setNativeMenuBar(false);
    menus_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    row->addWidget(menus_, 0, Qt::AlignVCenter);

    row->addStretch(1);

    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("shellDocTitle"));
    row->addWidget(title_, 0, Qt::AlignVCenter);

    row->addStretch(1);

    search_ = new SearchChip(this);
    row->addWidget(search_, 0, Qt::AlignVCenter);
    row->addSpacing(kRightGap);

    user_ = new UserChip(this);
    row->addWidget(user_, 0, Qt::AlignVCenter);
}

QSize TitleBar::sizeHint() const
{
    return QSize(QWidget::sizeHint().width(), kBarHeight);
}

void TitleBar::setDocumentName(const QString& name)
{
    title_->setText(name);
}

void TitleBar::setUserInitials(const QString& initials)
{
    user_->setInitials(initials);
}

void TitleBar::applyTheme(ThemeMode mode)
{
    theme_          = mode;
    const Tokens& t = mode == ThemeMode::Dark ? darkTokens() : lightTokens();
    search_->setTokens(t);
    user_->setTokens(t);
    update();
}

void TitleBar::paintEvent(QPaintEvent*)
{
    const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();

    QPainter p(this);
    QLinearGradient sky(0, 0, 0, kGradient);
    sky.setColorAt(0.0, t.bgShellBar);
    sky.setColorAt(1.0, t.bgShellBottom);
    p.fillRect(QRect(0, 0, width(), kGradient), sky);

    p.setPen(t.lineHard);
    p.drawLine(0, kGradient, width(), kGradient);
}

} // namespace piricad::app
