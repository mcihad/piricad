// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/dialog_chrome.hpp"

#include "piricad/app/tokens.hpp"
#include "piricad/core/text.hpp"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace piricad::app {
namespace {

// `design.md` §8–§10, measured off `stil.png` and `seçenekler.png`.
constexpr int kTitleHeight  = 38;
constexpr int kTitlePadX    = 12;
constexpr int kTitleGap     = 9;
constexpr int kTitleIcon    = 16;
constexpr int kTitlePx      = 13;
constexpr int kTitleMark    = 24;
constexpr int kFooterHeight = 48;

constexpr int kSectionRow  = 35;
constexpr int kSectionPadX = 13;
constexpr int kSectionIcon = 16;
constexpr int kSectionGap  = 10;
constexpr int kSectionPx   = 12;
constexpr int kSectionEdge = 2;
constexpr int kSectionDot  = 5;

constexpr int kSwitchWidth  = 38;
constexpr int kSwitchHeight = 20;
constexpr int kSwitchKnob   = 7;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QFont sans(int px, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(px);
    f.setWeight(weight);
    return f;
}

} // namespace

// =============================================================================
// DialogFrame
// =============================================================================

/// The title bar of a dialog: icon, name, dim qualifier, help and close.
class DialogTitleBar : public QWidget
{
public:
    explicit DialogTitleBar(DialogFrame* owner) : QWidget(owner), owner_(owner)
    {
        setFixedHeight(kTitleHeight);
        setMouseTracking(true);
    }

    void setHeading(Glyph glyph, const QString& title, const QString& subtitle)
    {
        glyph_    = glyph;
        title_    = title;
        subtitle_ = subtitle;
        update();
    }

    void setHelpVisible(bool on)
    {
        help_ = on;
        update();
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

        QLinearGradient sky(0, 0, 0, kTitleHeight - 1);
        sky.setColorAt(0.0, tokens_.bgTitlebar);
        sky.setColorAt(1.0, tokens_.bgTitleBottom);
        p.fillRect(QRect(0, 0, width(), kTitleHeight - 1), sky);
        p.fillRect(QRect(0, kTitleHeight - 1, width(), 1), tokens_.lineHard);

        int x = kTitlePadX;
        p.drawPixmap(QRect(x, (kTitleHeight - kTitleIcon) / 2, kTitleIcon, kTitleIcon),
                     glyph_pixmap(glyph_, tokens_.accent, kTitleIcon, devicePixelRatioF()));
        x += kTitleIcon + kTitleGap;

        p.setFont(sans(kTitlePx, QFont::DemiBold));
        p.setPen(tokens_.text);
        const QFontMetrics name(p.font());
        p.drawText(QRect(x, 0, width(), kTitleHeight - 1), Qt::AlignVCenter | Qt::AlignLeft,
                   title_);
        x += static_cast<int>(name.horizontalAdvance(title_)) + kTitleGap;

        if (!subtitle_.isEmpty()) {
            p.setFont(sans(kTitlePx));
            p.setPen(tokens_.textDim);
            p.drawText(QRect(x, 0, width() - x, kTitleHeight - 1), Qt::AlignVCenter | Qt::AlignLeft,
                       subtitle_);
        }

        int right = width() - kTitlePadX - kTitleMark;
        drawMark(p, right, Glyph::Close, hot_ == 1);
        if (help_) {
            right -= kTitleMark;
            drawMark(p, right, Glyph::Help, hot_ == 0);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int was = hot_;
        hot_          = markAt(event->position().toPoint().x());
        if (hot_ != was) update();
        if (hot_ < 0) owner_->DialogFrame::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent*) override
    {
        hot_ = -1;
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        switch (markAt(event->position().toPoint().x())) {
        case 0: emit owner_->helpRequested(); return;
        case 1: owner_->reject(); return;
        default: owner_->DialogFrame::mousePressEvent(event); return;
        }
    }

private:
    void drawMark(QPainter& p, int left, Glyph glyph, bool hot)
    {
        const QRect box(left, (kTitleHeight - kTitleMark) / 2, kTitleMark, kTitleMark);
        if (hot) {
            p.setPen(Qt::NoPen);
            p.setBrush(tokens_.hoverChip);
            p.drawRoundedRect(box, 4, 4);
        }
        p.drawPixmap(QRect(box.left() + 4, box.top() + 4, kTitleIcon, kTitleIcon),
                     glyph_pixmap(glyph, hot ? tokens_.text : tokens_.textDim, kTitleIcon,
                                  devicePixelRatioF()));
    }

    int markAt(int x) const
    {
        const int close = width() - kTitlePadX - kTitleMark;
        if (x >= close && x < close + kTitleMark) return 1;
        if (help_ && x >= close - kTitleMark && x < close) return 0;
        return -1;
    }

    DialogFrame* owner_;
    Tokens tokens_{};
    Glyph glyph_ = Glyph::Settings;
    QString title_;
    QString subtitle_;
    bool help_ = false;
    int hot_   = -1;
};

DialogFrame::DialogFrame(QWidget* parent) : QDialog(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setObjectName(QStringLiteral("dialogFrame"));

    stack_ = new QVBoxLayout(this);
    stack_->setContentsMargins(1, 1, 1, 1);
    stack_->setSpacing(0);

    titleBar_ = new DialogTitleBar(this);
    stack_->addWidget(titleBar_);

    footerBar_ = new QWidget(this);
    footerBar_->setObjectName(QStringLiteral("dialogFooter"));
    footerBar_->setFixedHeight(kFooterHeight);

    footer_ = new QHBoxLayout(footerBar_);
    footer_->setContentsMargins(kTitlePadX, 0, kTitlePadX, 0);
    footer_->setSpacing(8);
    footer_->addStretch(1);

    stack_->addWidget(footerBar_);
}

void DialogFrame::setHeading(Glyph glyph, const QString& title, const QString& subtitle)
{
    static_cast<DialogTitleBar*>(titleBar_)->setHeading(glyph, title, subtitle);
    setWindowTitle(subtitle.isEmpty() ? title : title + QLatin1Char(' ') + subtitle);
}

void DialogFrame::setHelpVisible(bool on)
{
    static_cast<DialogTitleBar*>(titleBar_)->setHelpVisible(on);
}

void DialogFrame::setBody(QWidget* body)
{
    if (body_) {
        stack_->removeWidget(body_);
        body_->deleteLater();
    }
    body_ = body;
    body_->setParent(this);
    stack_->insertWidget(1, body_, 1);
}

void DialogFrame::setFooterHeight(int px)
{
    footerBar_->setFixedHeight(px);
}

void DialogFrame::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    static_cast<DialogTitleBar*>(titleBar_)->setTokens(tokensOf(mode));
    update();
}

void DialogFrame::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.fillRect(rect(), t.bgWindow);

    // The 1 px outline the reference draws around every dialog. It is what
    // separates a frameless window from the desktop behind it, and without it a
    // dark dialog on a dark desktop has no edge at all.
    p.setPen(QPen(t.windowEdge, 1.0));
    p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
}

void DialogFrame::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (QWindow* handle = windowHandle())
        if (handle->startSystemMove()) return;
}

void DialogFrame::mouseMoveEvent(QMouseEvent*) {}

// =============================================================================
// SectionList
// =============================================================================

SectionList::SectionList(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("sectionList"));
    setMouseTracking(true);
}

int SectionList::addSection(Glyph glyph, const QString& label)
{
    sections_.push_back(
        Section{glyph, label, core::turkish_fold_key(label.toStdString()), false, true});
    updateGeometry();
    update();
    return static_cast<int>(sections_.size()) - 1;
}

void SectionList::setMarked(int index, bool marked)
{
    if (index < 0 || index >= sections_.size()) return;
    sections_[index].marked = marked;
    update();
}

void SectionList::setCurrent(int index)
{
    if (index < 0 || index >= sections_.size() || index == current_) return;
    current_ = index;
    update();
    emit currentChanged(index);
}

void SectionList::setFilter(const QString& needle)
{
    // The parser's own Turkish folding, so searching `olcek` finds `Ölçek` —
    // the same rule the command line matches names by (CLAUDE.md 5.6).
    const std::string folded = core::turkish_fold_key(needle.toStdString());
    for (Section& section : sections_)
        section.shown = folded.empty() || section.key.find(folded) != std::string::npos;
    update();
}

QSize SectionList::sizeHint() const
{
    int shown = 0;
    for (const Section& section : sections_)
        if (section.shown) ++shown;
    return QSize(0, shown * kSectionRow);
}

int SectionList::rowAt(int y) const
{
    int top = 0;
    for (int i = 0; i < sections_.size(); ++i) {
        if (!sections_[i].shown) continue;
        if (y >= top && y < top + kSectionRow) return i;
        top += kSectionRow;
    }
    return -1;
}

void SectionList::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void SectionList::mouseMoveEvent(QMouseEvent* event)
{
    const int was = hot_;
    hot_          = rowAt(static_cast<int>(event->position().y()));
    if (hot_ != was) update();
}

void SectionList::leaveEvent(QEvent*)
{
    hot_ = -1;
    update();
}

void SectionList::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    const int row = rowAt(static_cast<int>(event->position().y()));
    if (row >= 0) setCurrent(row);
}

void SectionList::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), t.bgStrip);

    int top = 0;
    for (int i = 0; i < sections_.size(); ++i) {
        const Section& section = sections_[i];
        if (!section.shown) continue;

        const QRect box(0, top, width(), kSectionRow);
        const bool chosen = i == current_;

        if (chosen) {
            p.fillRect(box, t.accentWash);
            p.fillRect(QRect(0, top, kSectionEdge, kSectionRow), t.accent);
        } else if (i == hot_) {
            p.fillRect(box, t.hoverRow);
        }

        const QColor ink = chosen ? t.accentHi : t.textDim;
        p.drawPixmap(
            QRect(kSectionPadX, top + (kSectionRow - kSectionIcon) / 2, kSectionIcon, kSectionIcon),
            glyph_pixmap(section.glyph, ink, kSectionIcon, devicePixelRatioF()));

        p.setFont(sans(kSectionPx, chosen ? QFont::Medium : QFont::Normal));
        p.setPen(chosen ? t.text : t.textDim);
        p.drawText(box.adjusted(kSectionPadX + kSectionIcon + kSectionGap, 0, -20, 0),
                   Qt::AlignVCenter | Qt::AlignLeft, section.label);

        // The warn dot: something in this section is changed and not applied.
        // §13 forbids saying anything with colour alone, so the dot is a SHAPE
        // that is either there or not — the colour only says how urgent it is.
        if (section.marked) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.warn);
            p.drawEllipse(QPointF(width() - 16.0, top + kSectionRow / 2.0), kSectionDot / 2.0,
                          kSectionDot / 2.0);
        }

        top += kSectionRow;
    }
}

// =============================================================================
// ToggleSwitch
// =============================================================================

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(kSwitchWidth, kSwitchHeight);
    setFocusPolicy(Qt::StrongFocus);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(kSwitchWidth, kSwitchHeight);
}

void ToggleSwitch::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool on = isChecked();

    p.setPen(on ? Qt::NoPen : QPen(t.border, 1.0));
    p.setBrush(on ? t.accent : t.bgInput);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kSwitchHeight / 2.0,
                      kSwitchHeight / 2.0);

    // The knob's SIDE is the second statement of the state, which is what makes
    // this readable without colour (§13).
    const qreal cx = on ? width() - kSwitchHeight / 2.0 : kSwitchHeight / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(on ? t.onAccent : t.textDim);
    p.drawEllipse(QPointF(cx, height() / 2.0), kSwitchKnob, kSwitchKnob);

    if (hasFocus()) {
        p.setPen(QPen(t.accentHi, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(-1.5, -1.5, 1.5, 1.5), kSwitchHeight / 2.0 + 2,
                          kSwitchHeight / 2.0 + 2);
    }
}

} // namespace piricad::app
