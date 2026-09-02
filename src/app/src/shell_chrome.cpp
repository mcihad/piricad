// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/shell_chrome.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace kentos::app {
namespace {

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// `design.md` §3: pixel sizes, never points. A point size is multiplied by
/// whatever DPI the screen reports, and two machines reporting different DPI then
/// draw the same bar at two different heights — which §12 forbids outright.
QFont sans(int px, QFont::Weight weight = QFont::Normal, qreal tracking = 0.0)
{
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(px);
    f.setWeight(weight);
    if (tracking != 0.0) f.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    return f;
}

QFont mono(int px, QFont::Weight weight = QFont::Normal)
{
    QFont f(QStringLiteral("IBM Plex Mono"));
    f.setPixelSize(px);
    f.setWeight(weight);
    f.setStyleHint(QFont::Monospace);
    return f;
}

// ---- readout strip, §7 -------------------------------------------------------
constexpr int kReadoutCaptionPx = 10;
constexpr int kReadoutValuePx   = 12;

// BASELINES, not rect tops. `drawText(rect, AlignTop)` places the text by the
// font's ascent inside a rectangle the layout may have moved, and the two
// roundings together put this eleven pixels low: the reading ended one pixel
// above the bottom of the bar with nothing under it. The reference measures the
// caption's ink at rows 11..17 of the 45 px band and the value's at 26..33, so
// the baselines are 17 and 33 and `drawText(QPointF, …)` puts them exactly there.
// And relative to the widget's own CENTRE, not its top. Where a tool bar puts a
// widget added to it is the tool bar's business — Fusion insets it, a stylesheet
// changes the inset, and neither is reachable from here. What IS reachable is
// that the two lines sit centred in whatever height it gets, which is what the
// reference measures: the pair spans rows 11..33 of a 45 px band, centred on
// 22 — the band's own centre.
// AS TALL AS A BUTTON, because the bar treats it like one. Measured rather than
// assumed: whatever height this widget asks for, the bar puts its top 8 px down
// — a fixed margin, not centring — so a 45 px strip started 8 px low and ran 8
// px past the rule, and the value ended one pixel above the bottom of the bar
// with no air under it. At 30 px it sits in the same 8/7 band as every tool
// button and the two lines land where the reference measures them.
constexpr int kReadoutHeight   = 30;
constexpr int kReadoutCapLift  = 5;  ///< caption baseline, above the centre
constexpr int kReadoutValDrop  = 12; ///< value baseline, below it
constexpr int kReadoutGap      = 10; ///< each side of the cell divider
constexpr int kReadoutRightPad = 12;
constexpr int kRuleHeight      = 22;

// ---- document tabs, §7 -------------------------------------------------------
constexpr int kTabHeight  = 30;
constexpr int kTabPadX    = 12;
constexpr int kTabGap     = 7;
constexpr int kTabIcon    = 14;
constexpr int kTabLabelPx = 12;
constexpr int kTabButtons = 2; ///< split view and expand, at the right end
constexpr int kTabBtnBox  = 26;
constexpr int kTabBtnPad  = 8;

} // namespace

// =============================================================================
// ReadoutStrip
// =============================================================================

ReadoutStrip::ReadoutStrip(QWidget* parent) : QWidget(parent)
{
    scale_.caption = tr("ÖLÇEK");
    scale_.value   = QStringLiteral("1 : 1 000");
    crs_.caption   = tr("KOORDİNAT SİSTEMİ");
    crs_.value     = QStringLiteral("—");
    setFixedHeight(kReadoutHeight);
}

void ReadoutStrip::setScale(const QString& text)
{
    if (scale_.value == text) return;
    scale_.value = text;
    updateGeometry();
    update();
}

void ReadoutStrip::setCrs(const QString& text)
{
    if (crs_.value == text) return;
    crs_.value = text;
    updateGeometry();
    update();
}

void ReadoutStrip::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

int ReadoutStrip::cellWidth(const Cell& cell) const
{
    // The wider of the two lines, because the caption and the value are left
    // aligned to the same edge and the cell is exactly as wide as it needs to be.
    const QFontMetrics caption(sans(kReadoutCaptionPx, QFont::DemiBold, 0.7));
    const QFontMetrics value(mono(kReadoutValuePx));
    return std::max(caption.horizontalAdvance(cell.caption), value.horizontalAdvance(cell.value));
}

QSize ReadoutStrip::sizeHint() const
{
    return QSize(
        cellWidth(scale_) + kReadoutGap + 1 + kReadoutGap + cellWidth(crs_) + kReadoutRightPad, 46);
}

void ReadoutStrip::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);

    const int scaleW = cellWidth(scale_);
    const int crsW   = cellWidth(crs_);
    int x            = width() - kReadoutRightPad - crsW;

    const auto cell = [&](const Cell& c, int left) {
        p.setFont(sans(kReadoutCaptionPx, QFont::DemiBold, 0.7));
        p.setPen(t.textFaint);
        p.drawText(QPointF(left, height() / 2.0 - kReadoutCapLift), c.caption);

        p.setFont(mono(kReadoutValuePx));
        p.setPen(t.readout);
        p.drawText(QPointF(left, height() / 2.0 + kReadoutValDrop), c.value);
    };

    cell(crs_, x);

    const int rule = x - kReadoutGap;
    p.fillRect(QRect(rule, (height() - kRuleHeight) / 2, 1, kRuleHeight), t.separator);

    cell(scale_, rule - kReadoutGap - scaleW);
}

// =============================================================================
// DocumentTabs
// =============================================================================

DocumentTabs::DocumentTabs(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kTabHeight);
    setMouseTracking(true);
}

QSize DocumentTabs::sizeHint() const
{
    return QSize(0, kTabHeight);
}

void DocumentTabs::setDocuments(const QStringList& names, int active)
{
    tabs_.clear();
    for (const QString& name : names)
        tabs_.push_back(Tab{name, 0, 0});
    active_ = active;
    relayout();
    update();
}

void DocumentTabs::relayout()
{
    const QFontMetrics label(sans(kTabLabelPx));

    int x = 0;
    for (int i = 0; i < tabs_.size(); ++i) {
        Tab& tab = tabs_[i];

        // The active tab carries a close mark, so it is wider than the same name
        // would be sitting inactive. That is the reference's own arithmetic.
        int w = kTabPadX + kTabIcon + kTabGap +
                static_cast<int>(label.horizontalAdvance(tab.name)) + kTabPadX;
        if (i == active_) w += kTabGap + kTabIcon;

        tab.left  = x;
        tab.width = w;
        x += w + 1; // the 1 px hard rule between tabs
    }
}

int DocumentTabs::tabAt(QPoint at) const
{
    for (int i = 0; i < tabs_.size(); ++i)
        if (at.x() >= tabs_[i].left && at.x() < tabs_[i].left + tabs_[i].width) return i;
    return -1;
}

void DocumentTabs::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void DocumentTabs::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint at  = event->position().toPoint();
    const int wasTab = hot_, wasClose = hotClose_, wasBtn = hotButton_;

    hot_       = tabAt(at);
    hotClose_  = -1;
    hotButton_ = -1;

    if (hot_ == active_ && hot_ >= 0) {
        const Tab& tab = tabs_[hot_];
        const int cx   = tab.left + tab.width - kTabPadX - kTabIcon;
        if (at.x() >= cx && at.x() < cx + kTabIcon) hotClose_ = hot_;
    }

    const int right = width() - kTabBtnPad - kTabButtons * kTabBtnBox;
    if (at.x() >= right) hotButton_ = std::min(kTabButtons - 1, (at.x() - right) / kTabBtnBox);

    if (hot_ != wasTab || hotClose_ != wasClose || hotButton_ != wasBtn) update();
}

void DocumentTabs::leaveEvent(QEvent*)
{
    hot_ = hotClose_ = hotButton_ = -1;
    update();
}

void DocumentTabs::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    if (hotClose_ >= 0) {
        emit closeRequested(hotClose_);
        return;
    }
    if (hotButton_ == 0) {
        emit splitRequested();
        return;
    }
    if (hotButton_ == 1) {
        emit expandRequested();
        return;
    }
    const int index = tabAt(event->position().toPoint());
    if (index >= 0) emit activated(index);
}

void DocumentTabs::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), t.bgStrip);

    for (int i = 0; i < tabs_.size(); ++i) {
        const Tab& tab      = tabs_[i];
        const bool isActive = i == active_;
        const QRect box(tab.left, 0, tab.width, kTabHeight);

        if (isActive)
            p.fillRect(box, t.bgTabActive);
        else if (i == hot_)
            p.fillRect(box, t.hoverRow);

        // fillRect, never drawLine, for a 1 px rule. Antialiasing is on for the
        // glyphs, and an antialiased hairline spreads itself over two rows at
        // half intensity — which is a 2 px grey smear where the reference has one
        // crisp line, and one pixel of drift for everything below it.
        p.fillRect(QRect(box.right() + 1, 0, 1, kTabHeight), t.lineHard);

        int x = box.left() + kTabPadX;
        p.drawPixmap(QRect(x, (kTabHeight - kTabIcon) / 2, kTabIcon, kTabIcon),
                     glyph_pixmap(Glyph::Document, isActive ? t.text : t.textDim, kTabIcon,
                                  devicePixelRatioF()));
        x += kTabIcon + kTabGap;

        p.setFont(sans(kTabLabelPx));
        p.setPen(isActive ? t.text : t.textDim);
        const QFontMetrics label(p.font());
        p.drawText(QRect(x, 0, label.horizontalAdvance(tab.name), kTabHeight),
                   Qt::AlignVCenter | Qt::AlignLeft, tab.name);

        if (isActive) {
            const int cx = box.right() + 1 - kTabPadX - kTabIcon;
            p.drawPixmap(QRect(cx, (kTabHeight - kTabIcon) / 2, kTabIcon, kTabIcon),
                         glyph_pixmap(Glyph::Close, hotClose_ == i ? t.text : t.textFaint, kTabIcon,
                                      devicePixelRatioF()));
        }
    }

    // Split view and expand, pinned to the right end of the strip.
    static const Glyph kButtons[kTabButtons] = {Glyph::SplitView, Glyph::Fullscreen};
    int bx                                   = width() - kTabBtnPad - kTabButtons * kTabBtnBox;
    for (int i = 0; i < kTabButtons; ++i) {
        const QRect box(bx, (kTabHeight - kTabBtnBox) / 2, kTabBtnBox, kTabBtnBox);
        if (hotButton_ == i) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverIcon);
            p.drawRoundedRect(box, 4, 4);
        }
        p.drawPixmap(QRect(box.left() + 5, box.top() + 5, 16, 16),
                     glyph_pixmap(kButtons[i], hotButton_ == i ? t.text : t.textDim, 16,
                                  devicePixelRatioF()));
        bx += kTabBtnBox;
    }

    p.fillRect(QRect(0, kTabHeight - 1, width(), 1), t.lineHard);
}

// =============================================================================
// StatusStrip
// =============================================================================

namespace {

constexpr int kStatusHeight = 26;
constexpr int kStatusPadX   = 12;
constexpr int kStatusGap    = 7;
constexpr int kStatusIcon   = 13;
constexpr int kStatusPx     = 11;

} // namespace

StatusStrip::StatusStrip(QWidget* parent) : QStatusBar(parent)
{
    setObjectName(QStringLiteral("statusHost"));
    setSizeGripEnabled(false);
    setFixedHeight(kStatusHeight);
    setMouseTracking(true);
}

QSize StatusStrip::sizeHint() const
{
    return QSize(0, kStatusHeight);
}

int StatusStrip::cellWidth(const QString& text, bool withIcon) const
{
    const QFontMetrics metrics(mono(kStatusPx));
    return kStatusPadX + (withIcon ? kStatusIcon + kStatusGap : 0) +
           static_cast<int>(metrics.horizontalAdvance(text)) + kStatusPadX;
}

void StatusStrip::addToggle(const QString& label, const QString& id)
{
    chips_.push_back(Chip{label, id, false, 0, 0});
    relayout();
    update();
}

void StatusStrip::setToggle(const QString& id, bool on)
{
    for (Chip& chip : chips_)
        if (chip.id == id && chip.on != on) {
            chip.on = on;
            update();
        }
}

void StatusStrip::setCoordinate(const QString& text)
{
    if (coordinate_ == text) return;
    coordinate_ = text;
    relayout();
    update();
}

void StatusStrip::setMessage(const QString& text)
{
    if (message_ == text) return;
    message_ = text;
    update(); // no relayout: the cell takes the room already between the chips
}

void StatusStrip::setConnection(const QString& text, bool connected)
{
    connection_ = text;
    connected_  = connected;
    update();
}

void StatusStrip::setPerformance(const QString& text)
{
    if (performance_ == text) return;
    performance_ = text;
    update();
}

void StatusStrip::relayout()
{
    // The coordinate cell is as wide as its own text, so the chips start where
    // the reading ends and the whole strip stays left-packed as §7 draws it.
    coordWidth_ = cellWidth(coordinate_, true);

    const QFontMetrics metrics(sans(kStatusPx, QFont::DemiBold, 0.4));
    int x = coordWidth_ + 1;
    for (Chip& chip : chips_) {
        chip.left = x;
        chip.width =
            kStatusPadX + static_cast<int>(metrics.horizontalAdvance(chip.label)) + kStatusPadX;
        x += chip.width;
    }
}

void StatusStrip::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void StatusStrip::mouseMoveEvent(QMouseEvent* event)
{
    const int at  = event->position().toPoint().x();
    const int was = hot_;
    hot_          = -1;
    for (int i = 0; i < chips_.size(); ++i)
        if (at >= chips_[i].left && at < chips_[i].left + chips_[i].width) hot_ = i;
    if (hot_ != was) update();
}

void StatusStrip::leaveEvent(QEvent*)
{
    hot_ = -1;
    update();
}

void StatusStrip::mousePressEvent(QMouseEvent* event)
{
    if (hot_ < 0) return;
    if (event->button() == Qt::RightButton) {
        emit configureRequested(chips_[hot_].id);
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    emit toggled(chips_[hot_].id);
}

void StatusStrip::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), t.bgStrip);
    p.fillRect(QRect(0, 0, width(), 1), t.lineHard);

    // ---- the cursor's coordinate ----
    p.drawPixmap(QRect(kStatusPadX, (kStatusHeight - kStatusIcon) / 2, kStatusIcon, kStatusIcon),
                 glyph_pixmap(Glyph::Locate, t.textFaint, kStatusIcon, devicePixelRatioF()));
    p.setFont(mono(kStatusPx));
    p.setPen(t.readoutDim);
    p.drawText(QRect(kStatusPadX + kStatusIcon + kStatusGap, 1, width(), kStatusHeight - 1),
               Qt::AlignVCenter | Qt::AlignLeft, coordinate_);
    p.fillRect(QRect(coordWidth_, 1, 1, kStatusHeight - 1), t.lineSoft);

    // ---- the aid toggles ----
    p.setFont(sans(kStatusPx, QFont::DemiBold, 0.4));
    for (int i = 0; i < chips_.size(); ++i) {
        const Chip& chip = chips_[i];
        const QRect box(chip.left, 1, chip.width, kStatusHeight - 1);

        // An ON aid is stated twice — a lighter ground AND the accent — because
        // colour alone is not a state a colour-blind user can read (§13).
        if (chip.on)
            p.fillRect(box, t.bgHeader);
        else if (i == hot_)
            p.fillRect(box, t.hoverRow);

        p.setPen(chip.on ? t.accent : (i == hot_ ? t.text : t.textFaint));
        p.drawText(box, Qt::AlignCenter, chip.label);
    }

    // ---- the two right-hand cells ----
    const int perfWidth = cellWidth(performance_, false);
    const int connWidth = cellWidth(connection_, true);

    int x = width() - perfWidth;

    // ---- what the last command said ----
    //
    // In the gap the chips leave, elided rather than wrapped: a status line is one
    // line, and a distance the user cannot finish reading is still the fastest
    // place to find it. The full text is in `Geçmiş`.
    if (!message_.isEmpty() && !chips_.isEmpty()) {
        const int from = chips_.back().left + chips_.back().width + kStatusPadX;
        const int to   = x - connWidth - kStatusPadX;
        if (to - from > kStatusPadX * 2) {
            p.setFont(mono(kStatusPx));
            p.setPen(t.readout);
            const QRect box(from, 1, to - from, kStatusHeight - 1);
            p.drawText(box, Qt::AlignVCenter | Qt::AlignLeft,
                       QFontMetrics(p.font()).elidedText(message_, Qt::ElideRight, box.width()));
        }
    }

    p.setFont(mono(kStatusPx));
    p.setPen(t.textDim);
    p.drawText(QRect(x + kStatusPadX, 1, perfWidth, kStatusHeight - 1),
               Qt::AlignVCenter | Qt::AlignLeft, performance_);
    p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);

    x -= connWidth;
    p.drawPixmap(
        QRect(x + kStatusPadX, (kStatusHeight - kStatusIcon) / 2, kStatusIcon, kStatusIcon),
        glyph_pixmap(Glyph::Cloud, connected_ ? t.ok : t.textFaint, kStatusIcon,
                     devicePixelRatioF()));
    p.setPen(t.textDim);
    p.drawText(QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, connWidth, kStatusHeight - 1),
               Qt::AlignVCenter | Qt::AlignLeft, connection_);
    p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
}

// =============================================================================
// PanelHeader
// =============================================================================

namespace {

constexpr int kHeaderHeight = 29;
constexpr int kHeaderPadX   = 10;
constexpr int kHeaderGap    = 6;
constexpr int kHeaderIcon   = 14;
constexpr int kHeaderLabel  = 12;
constexpr int kActiveEdge   = 2; ///< the accent line along the top of the active tab
constexpr int kHeaderBtn    = 20;
constexpr int kHeaderBtnGap = 4;
constexpr int kHeaderRight  = 6;

} // namespace

PanelHeader::PanelHeader(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kHeaderHeight);
    setMouseTracking(true);
}

QSize PanelHeader::sizeHint() const
{
    return QSize(0, kHeaderHeight);
}

QSize PanelHeader::minimumSizeHint() const
{
    return QSize(0, kHeaderHeight);
}

void PanelHeader::addTab(const QString& label, int glyph)
{
    tabs_.push_back(Tab{label, glyph, 0, 0});
    relayout();
    update();
}

void PanelHeader::setButtons(int mask)
{
    buttons_ = mask;
    update();
}

void PanelHeader::setCurrent(int index)
{
    if (index < 0 || index >= tabs_.size() || index == current_) return;
    current_ = index;
    update();
    emit tabChanged(index);
}

void PanelHeader::relayout()
{
    // Measured at the WIDEST weight a tab can be drawn in. The active tab is
    // Medium and the others are Normal; measuring at Normal makes the active
    // label a few pixels wider than the box laid out for it, and the last letter
    // is clipped — which is what "Öznitelikler" became.
    const QFontMetrics label(sans(kHeaderLabel, QFont::Medium));
    int x = 0;
    for (Tab& tab : tabs_) {
        tab.left  = x;
        tab.width = kHeaderPadX + kHeaderIcon + kHeaderGap +
                    static_cast<int>(label.horizontalAdvance(tab.label)) + kHeaderPadX;
        x += tab.width;
    }
}

QVector<int> PanelHeader::buttonList() const
{
    QVector<int> out;
    for (int bit : {Grip, Collapse, Float, Close})
        if (buttons_ & bit) out.push_back(bit);
    return out;
}

void PanelHeader::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void PanelHeader::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint at  = event->position().toPoint();
    const int wasTab = hotTab_, wasBtn = hotButton_;

    hotTab_    = -1;
    hotButton_ = -1;

    for (int i = 0; i < tabs_.size(); ++i)
        if (at.x() >= tabs_[i].left && at.x() < tabs_[i].left + tabs_[i].width) hotTab_ = i;

    const QVector<int> marks = buttonList();
    const int first =
        width() - kHeaderRight - static_cast<int>(marks.size()) * (kHeaderBtn + kHeaderBtnGap);
    if (at.x() >= first) {
        const int index = (at.x() - first) / (kHeaderBtn + kHeaderBtnGap);
        if (index >= 0 && index < marks.size()) hotButton_ = marks[index];
    }

    if (hotTab_ != wasTab || hotButton_ != wasBtn) update();
}

void PanelHeader::leaveEvent(QEvent*)
{
    hotTab_    = -1;
    hotButton_ = -1;
    update();
}

void PanelHeader::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (hotButton_ > 0) {
        emit buttonPressed(hotButton_);
        return;
    }
    if (hotTab_ >= 0) setCurrent(hotTab_);
}

void PanelHeader::paintEvent(QPaintEvent*)
{
    const Tokens& t = tokensOf(theme_);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), t.bgStrip);

    for (int i = 0; i < tabs_.size(); ++i) {
        const Tab& tab      = tabs_[i];
        const bool isActive = i == current_;
        const QRect box(tab.left, 0, tab.width, kHeaderHeight);

        if (isActive) {
            p.fillRect(box, t.bgPanel);
            p.fillRect(QRect(box.left(), 0, box.width(), kActiveEdge), t.accent);
        } else if (i == hotTab_) {
            p.fillRect(box, t.hoverRow);
        }

        const QColor ink = isActive ? t.text : t.textDim;
        p.drawPixmap(
            QRect(box.left() + kHeaderPadX, (kHeaderHeight - kHeaderIcon) / 2, kHeaderIcon,
                  kHeaderIcon),
            glyph_pixmap(static_cast<Glyph>(tab.glyph), ink, kHeaderIcon, devicePixelRatioF()));

        p.setFont(sans(kHeaderLabel, isActive ? QFont::Medium : QFont::Normal));
        p.setPen(ink);
        p.drawText(box.adjusted(kHeaderPadX + kHeaderIcon + kHeaderGap, isActive ? kActiveEdge : 0,
                                -kHeaderPadX, 0),
                   Qt::AlignVCenter | Qt::AlignLeft, tab.label);
    }

    static const Glyph kMarks[] = {Glyph::Grip, Glyph::Collapse, Glyph::Float, Glyph::Close};
    const QVector<int> marks    = buttonList();
    int bx = width() - kHeaderRight - static_cast<int>(marks.size()) * (kHeaderBtn + kHeaderBtnGap);
    for (int bit : marks) {
        const int which = bit == Grip ? 0 : bit == Collapse ? 1 : bit == Float ? 2 : 3;
        const QRect box(bx, (kHeaderHeight - kHeaderBtn) / 2, kHeaderBtn, kHeaderBtn);
        if (hotButton_ == bit) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverIcon);
            p.drawRoundedRect(box, 3, 3);
        }
        p.drawPixmap(QRect(box.left() + 3, box.top() + 3, kHeaderIcon, kHeaderIcon),
                     glyph_pixmap(kMarks[which], hotButton_ == bit ? t.text : t.textFaint,
                                  kHeaderIcon, devicePixelRatioF()));
        bx += kHeaderBtn + kHeaderBtnGap;
    }

    p.fillRect(QRect(0, kHeaderHeight - 1, width(), 1), t.lineHard);
}

} // namespace kentos::app
