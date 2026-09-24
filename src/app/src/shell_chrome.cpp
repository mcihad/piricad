// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/shell_chrome.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>

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

} // namespace

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

int StatusStrip::probeRightCellsWidth() const
{
    return cellWidth(performance_, false) + cellWidth(connection_, true) +
           (agent_.isEmpty() ? 0 : cellWidth(agent_, true));
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

void StatusStrip::setAgent(const QString& text, AgentState state)
{
    if (agent_ == text && agentState_ == state) return;
    agent_      = text;
    agentState_ = state;
    update();
}

void StatusStrip::setScale(const QString& text)
{
    if (scale_ == text) return;
    scale_ = text;
    update();
}

void StatusStrip::setCrs(const QString& text)
{
    if (crs_ == text) return;
    crs_ = text;
    update();
}

void StatusStrip::setPerformance(const QString& text)
{
    if (performance_ == text) return;
    performance_ = text;
    update();
}

void StatusStrip::setBusyLabel(const QString& label)
{
    if (!busy_ || busyLabel_ == label) return;
    busyLabel_ = label;
    update();
}

void StatusStrip::setBusy(const QString& label, bool on)
{
    busy_      = on;
    busyLabel_ = label;
    if (on) {
        if (pulse_ == nullptr) {
            pulse_ = new QTimer(this);
            pulse_->setInterval(40);
            connect(pulse_, &QTimer::timeout, this, &StatusStrip::pulse);
        }
        phase_ = 0;
        pulse_->start();
    } else {
        if (pulse_ != nullptr) pulse_->stop();
        stopRect_ = QRect();
        stopHot_  = false;
    }
    update();
}

void StatusStrip::pulse()
{
    phase_ = (phase_ + 3) % 200;
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
    const bool stopWas = stopHot_;
    stopHot_ = busy_ && !stopRect_.isEmpty() && stopRect_.contains(event->position().toPoint());
    const bool agentWas = agentHot_;
    agentHot_           = !agentRect_.isEmpty() && agentRect_.contains(event->position().toPoint());
    setCursor(agentHot_ ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (hot_ != was || stopHot_ != stopWas || agentHot_ != agentWas) update();
}

void StatusStrip::leaveEvent(QEvent*)
{
    hot_      = -1;
    agentHot_ = false;
    update();
}

void StatusStrip::mousePressEvent(QMouseEvent* event)
{
    // DURDUR, while a job runs: the one control on the strip that is not an aid.
    if (busy_ && !stopRect_.isEmpty() && stopRect_.contains(event->position().toPoint())) {
        if (event->button() == Qt::LeftButton) emit stopRequested();
        return;
    }
    if (!agentRect_.isEmpty() && agentRect_.contains(event->position().toPoint())) {
        if (event->button() == Qt::LeftButton) emit agentClicked();
        return;
    }
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

    // ---- the right-hand cells ----
    //
    // ALL THREE OF THEM, measured before anything is drawn. There were two when
    // this was written and the agent listener made it three, but the gap the
    // message is given still subtracted only two: a long line — and `ÖLÇ` writes
    // one, "Mesafe: 58,941 m  ΔY: 57,000 m  ΔX: 15,000 m  Açı: 83,6183 grad" —
    // was elided to a box that ran under the MCP cell and the two were drawn on
    // top of each other. A user reported the measuring tool as broken; what was
    // broken was where its answer landed.
    const int perfWidth  = cellWidth(performance_, false);
    const int connWidth  = cellWidth(connection_, true);
    const int agentWidth = agent_.isEmpty() ? 0 : cellWidth(agent_, true);
    const QString sheet =
        scale_.isEmpty() && crs_.isEmpty() ? QString() : scale_ + QStringLiteral("  ·  ") + crs_;
    const int sheetWidth = sheet.isEmpty() ? 0 : cellWidth(sheet, true);

    /// The left edge of the right-hand cells: nothing may be drawn past it.
    const int rightEdge = width() - perfWidth - connWidth - agentWidth - sheetWidth;

    int x = width() - perfWidth;

    // ---- a job in flight: its label, a live segment, and Durdur ----
    //
    // Takes the message gap while it runs. The moving segment is what says the
    // program is alive when a 48 MB DXF is being read on another thread, and the
    // chip is the only way to stop that read short of closing the window.
    if (busy_ && !chips_.isEmpty()) {
        const int from = chips_.back().left + chips_.back().width + kStatusPadX;
        const int to   = rightEdge - kStatusPadX;
        const QFontMetrics chipMetrics(sans(kStatusPx, QFont::DemiBold, 0.4));
        const int stopWidth = kStatusPadX +
                              static_cast<int>(chipMetrics.horizontalAdvance(tr("Durdur"))) +
                              kStatusPadX;
        if (to - from > stopWidth + kStatusPadX * 4) {
            stopRect_ = QRect(to - stopWidth, 1, stopWidth, kStatusHeight - 1);
            const QRect label(from, 1, stopRect_.left() - kStatusPadX - from, kStatusHeight - 1);

            p.setFont(mono(kStatusPx));
            p.setPen(t.readout);
            p.drawText(
                label, Qt::AlignVCenter | Qt::AlignLeft,
                QFontMetrics(p.font()).elidedText(busyLabel_, Qt::ElideMiddle, label.width()));

            // The segment: a fifth of the label's width, sweeping left to right.
            const int span = std::max(20, label.width() / 5);
            const int lane = label.width() - span;
            const int sx   = label.left() + (lane > 0 ? (phase_ * lane) / 200 : 0);
            p.fillRect(QRect(label.left(), kStatusHeight - 3, label.width(), 2), t.lineSoft);
            p.fillRect(QRect(sx, kStatusHeight - 3, span, 2), t.accent);

            p.fillRect(stopRect_, stopHot_ ? t.hoverRow : t.bgHeader);
            p.setFont(sans(kStatusPx, QFont::DemiBold, 0.4));
            p.setPen(t.accent);
            p.drawText(stopRect_, Qt::AlignCenter, tr("Durdur"));
        } else {
            stopRect_ = QRect();
        }
    }

    // ---- what the last command said ----
    //
    // In the gap the chips leave, elided rather than wrapped: a status line is one
    // line, and a distance the user cannot finish reading is still the fastest
    // place to find it. The full text is in `Geçmiş`.
    if (!busy_ && !message_.isEmpty() && !chips_.isEmpty()) {
        const int from = chips_.back().left + chips_.back().width + kStatusPadX;
        const int to   = rightEdge - kStatusPadX;
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

    // ---- the agent listener ----
    //
    // A MARK WITH A SHAPE, not a colour with a meaning (ui.md R31). Down is a
    // hollow ring, up-and-guarded is a filled dot, and up-with-no-token is a
    // filled TRIANGLE — the one shape in the shell that means "look at this" —
    // beside the word `KORUMASIZ`. Clicking the cell runs `MCPSUNUCU`, which is
    // the same command the menu entry runs.
    if (!agent_.isEmpty()) {
        x -= agentWidth;
        agentRect_ = QRect(x, 1, agentWidth, kStatusHeight - 1);
        if (agentHot_) p.fillRect(agentRect_, t.hoverRow);

        const QRectF mark(x + kStatusPadX, (kStatusHeight - kStatusIcon) / 2.0 + 1.0,
                          kStatusIcon - 2, kStatusIcon - 2);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        switch (agentState_) {
        case AgentState::Off:
            p.setPen(QPen(t.textFaint, 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(mark);
            break;
        case AgentState::Guarded:
            p.setPen(Qt::NoPen);
            p.setBrush(t.ok);
            p.drawEllipse(mark);
            break;
        case AgentState::Unprotected:
            p.setPen(Qt::NoPen);
            p.setBrush(t.danger);
            p.drawPolygon(QPolygonF({QPointF(mark.center().x(), mark.top()),
                                     QPointF(mark.right(), mark.bottom()),
                                     QPointF(mark.left(), mark.bottom())}));
            break;
        }
        p.restore();

        p.setFont(mono(kStatusPx));
        p.setPen(agentState_ == AgentState::Unprotected ? t.danger : t.textDim);
        p.drawText(
            QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, agentWidth, kStatusHeight - 1),
            Qt::AlignVCenter | Qt::AlignLeft, agent_);
        p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
    } else {
        agentRect_ = QRect();
    }

    // ---- the sheet: the plot scale and the coordinate system ----
    //
    // Read, never edited here: the scale is YAZDIR's and AYAR plan_ölçeği's,
    // the system the project's (`design.md` §7).
    if (!sheet.isEmpty()) {
        x -= sheetWidth;
        p.drawPixmap(
            QRect(x + kStatusPadX, (kStatusHeight - kStatusIcon) / 2, kStatusIcon, kStatusIcon),
            glyph_pixmap(Glyph::Globe, t.textFaint, kStatusIcon, devicePixelRatioF()));
        p.setFont(mono(kStatusPx));
        p.setPen(t.readout);
        p.drawText(
            QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, sheetWidth, kStatusHeight - 1),
            Qt::AlignVCenter | Qt::AlignLeft, sheet);
        p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
    }
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

void PanelHeader::setButtons(unsigned mask)
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
    // The panel's own marks first, the dock's after: `+ ⧩ ⋮` reads left to right.
    for (unsigned bit : {Add, Filter, Grip, Collapse, Float, Close})
        if ((buttons_ & bit) != 0U) out.push_back(static_cast<int>(bit));
    return out;
}

void PanelHeader::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

QPoint PanelHeader::handlePoint() const
{
    // THE GRIP FIRST, because it is the handle design.md §6 draws — the
    // `drag_indicator` mark at the left of the dock's three. A header with tabs
    // in it often has no bare strip at all: the properties panel carries three
    // tabs and four marks and they meet in the middle.
    const QVector<int> marks = buttonList();
    const int first =
        width() - kHeaderRight - static_cast<int>(marks.size()) * (kHeaderBtn + kHeaderBtnGap);
    for (int i = 0; i < marks.size(); ++i)
        if (marks[i] == static_cast<int>(Grip))
            return {first + i * (kHeaderBtn + kHeaderBtnGap) + kHeaderBtn / 2, kHeaderHeight / 2};

    int after = 0;
    for (const Tab& tab : tabs_)
        after = std::max(after, tab.left + tab.width);
    if (first - after < 8) return {-1, -1};
    return {(after + first) / 2, kHeaderHeight / 2};
}

PanelHeader::Hit PanelHeader::hitAt(QPoint at) const
{
    Hit found;
    for (int i = 0; i < tabs_.size(); ++i)
        if (at.x() >= tabs_[i].left && at.x() < tabs_[i].left + tabs_[i].width) found.tab = i;

    const QVector<int> marks = buttonList();
    const int first =
        width() - kHeaderRight - static_cast<int>(marks.size()) * (kHeaderBtn + kHeaderBtnGap);
    if (at.x() >= first) {
        const int index = (at.x() - first) / (kHeaderBtn + kHeaderBtnGap);
        if (index >= 0 && index < marks.size()) found.button = marks[index];
    }

    // A MARK BEATS A TAB. They are drawn over the same strip and the marks go
    // down last, so the tab's claim on that x is a claim about pixels somebody
    // else painted. Reported as both, a press on the grip fell through the
    // mark branch into the tab branch and switched tabs instead of dragging.
    if (found.button > 0) found.tab = -1;
    return found;
}

void PanelHeader::mouseMoveEvent(QMouseEvent* event)
{
    // A GESTURE HANDED TO THE DOCK STAYS HANDED OVER.
    //
    // Qt delivers every move of a press to the widget the press landed on, even
    // when that widget refused the press. Swallowing them here would let the
    // dock begin a drag and then never hear where the pointer went.
    if (handedOver_) {
        event->ignore();
        return;
    }

    const int wasTab = hotTab_, wasBtn = hotButton_;
    const Hit under = hitAt(event->position().toPoint());
    hotTab_         = under.tab;
    hotButton_      = under.button;

    // THE POINTER SAYS WHAT CAN BE GRABBED, before anything is pressed. A
    // handle nobody can see is a handle nobody uses.
    const bool grabbable = under.bare() || under.button == static_cast<int>(Grip);
    setCursor(grabbable ? Qt::OpenHandCursor : Qt::ArrowCursor);

    if (hotTab_ != wasTab || hotButton_ != wasBtn) update();
}

void PanelHeader::mouseReleaseEvent(QMouseEvent* event)
{
    if (!handedOver_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    handedOver_ = false;
    event->ignore();
}

void PanelHeader::leaveEvent(QEvent*)
{
    hotTab_    = -1;
    hotButton_ = -1;
    update();
}

void PanelHeader::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // HIT-TESTED FROM THE PRESS, not from whatever the last move left behind.
    // `hotTab_` and `hotButton_` are hover state, and a press can arrive with
    // no move before it — a click on a panel that has just appeared under the
    // pointer, or a tap — in which case the stale values decided what was
    // pressed.
    const Hit under = hitAt(event->position().toPoint());
    if (under.button > 0 && under.button != static_cast<int>(Grip)) {
        emit buttonPressed(under.button);
        return;
    }
    if (under.button < 0 && under.tab >= 0) {
        setCurrent(under.tab);
        return;
    }

    // ---- THE GRIP IS THE HANDLE, AND SO IS THE BARE STRIP -------------------
    //
    // A REPORTED DEFECT: a floated panel could not be dragged anywhere.
    //
    // TWO THINGS WERE WRONG AND BOTH HAD TO GO.
    //
    // `QDockWidget` starts its own drag from a press on its title area, and
    // this widget IS that title area — `setTitleBarWidget` put it there. But a
    // `mousePressEvent` that returns without ignoring the event has ACCEPTED
    // it, so the press stopped here and the dock never heard one. Docked, that
    // cost the user the drag that re-docks a panel elsewhere; floated, where
    // the header is the whole window's title bar, it left a window that could
    // not be moved at all.
    //
    // And handing back only the BARE strip would have fixed nothing for the
    // panel that was reported: design.md §6 gives every header a
    // `drag_indicator` mark and the properties panel carries three tabs and
    // four marks, which meet in the middle with no bare strip between them.
    // The grip was drawn and wired to nothing. It is the handle §6 says it is:
    // a press on it goes to the dock exactly as a press on the bare strip does.
    //
    // The tabs and the other three marks keep their press, because a press on
    // them means something else.
    handedOver_ = true;
    event->ignore();
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

    const auto glyphOf = [](int bit) {
        switch (bit) {
        case Grip: return Glyph::Grip;
        case Collapse: return Glyph::Collapse;
        case Float: return Glyph::Float;
        case Close: return Glyph::Close;
        case Add: return Glyph::Plus;
        case Filter: return Glyph::Filter;
        default: return Glyph::Grip;
        }
    };
    const QVector<int> marks = buttonList();
    int bx = width() - kHeaderRight - static_cast<int>(marks.size()) * (kHeaderBtn + kHeaderBtnGap);
    for (int bit : marks) {
        const QRect box(bx, (kHeaderHeight - kHeaderBtn) / 2, kHeaderBtn, kHeaderBtn);
        if (hotButton_ == bit) {
            p.setPen(Qt::NoPen);
            p.setBrush(t.hoverIcon);
            p.drawRoundedRect(box, 3, 3);
        }
        p.drawPixmap(QRect(box.left() + 3, box.top() + 3, kHeaderIcon, kHeaderIcon),
                     glyph_pixmap(glyphOf(bit), hotButton_ == bit ? t.text : t.textFaint,
                                  kHeaderIcon, devicePixelRatioF()));
        bx += kHeaderBtn + kHeaderBtnGap;
    }

    p.fillRect(QRect(0, kHeaderHeight - 1, width(), 1), t.lineHard);
}

} // namespace kentos::app
