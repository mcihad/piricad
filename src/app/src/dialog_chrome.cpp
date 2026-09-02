// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/dialog_chrome.hpp"

#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/core/text.hpp"

#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

// `design.md` §8–§10, measured off `stil.png` and `seçenekler.png`.
constexpr int kFooterPadX   = 12;
constexpr int kFooterHeight = 48;
constexpr int kWindowIcon   = 32;

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

DialogFrame::DialogFrame(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("dialogFrame"));

    stack_ = new QVBoxLayout(this);
    stack_->setContentsMargins(0, 0, 0, 0);
    stack_->setSpacing(0);

    footerBar_ = new QWidget(this);
    footerBar_->setObjectName(QStringLiteral("dialogFooter"));
    footerBar_->setFixedHeight(kFooterHeight);

    footer_ = new QHBoxLayout(footerBar_);
    footer_->setContentsMargins(kFooterPadX, 0, kFooterPadX, 0);
    footer_->setSpacing(8);

    // The help button the title bar used to carry, at the left end of the footer
    // and before the stretch, so a caller's own left-hand buttons still land
    // beside it and its right-hand ones still land on the right.
    // A `QPushButton`, like every other button in this footer.
    //
    // It was a `QToolButton`, and a tool button in text-only mode elides its own
    // label to whatever it decides its content rect is — which produced a footer
    // reading `Y...m`, a button whose name had been cut in half. Nothing here
    // needed a tool button: it has no icon, no menu and no auto-raise.
    auto* help = new QPushButton(tr("Yardım"), footerBar_);
    help->setObjectName(QStringLiteral("dialogHelp"));
    help->setCursor(Qt::PointingHandCursor);
    help->setVisible(false);
    connect(help, &QPushButton::clicked, this, &DialogFrame::helpRequested);
    help_ = help;
    footer_->addWidget(help);

    footer_->addStretch(1);

    stack_->addWidget(footerBar_);
}

void DialogFrame::setHeading(Glyph glyph, const QString& title, const QString& subtitle)
{
    glyph_ = glyph;
    setWindowTitle(subtitle.isEmpty() ? title : title + QLatin1Char(' ') + subtitle);
    applyWindowIcon();
}

void DialogFrame::setHelpVisible(bool on)
{
    help_->setVisible(on);
}

void DialogFrame::setBody(QWidget* body)
{
    if (body_) {
        stack_->removeWidget(body_);
        body_->deleteLater();
    }
    body_ = body;
    body_->setParent(this);
    stack_->insertWidget(0, body_, 1);
}

void DialogFrame::setFooterHeight(int px)
{
    footerBar_->setFixedHeight(px);
}

void DialogFrame::applyWindowIcon()
{
    setWindowIcon(
        QIcon(glyph_pixmap(glyph_, tokensOf(theme_).accent, kWindowIcon, devicePixelRatioF())));
}

void DialogFrame::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    applyWindowIcon();

    // EVERY painted child, in one walk. A dialog that themed only itself is how
    // the settings window kept a black sidebar in the light theme: the section
    // list was still painting `darkTokens()` because nobody had told it. Nothing
    // here needs to know what those children are.
    applyThemeToChildren(this, mode);

    update();
}

void DialogFrame::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), tokensOf(theme_).bgWindow);
}

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

} // namespace kentos::app
