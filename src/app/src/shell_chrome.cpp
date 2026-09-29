// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/shell_chrome.hpp"

#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"

#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QToolTip>

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
    // An icon with no text beside it — a cell shortened to its mark — needs no
    // gap after the mark.
    return kStatusPadX + (withIcon ? kStatusIcon + (text.isEmpty() ? 0 : kStatusGap) : 0) +
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
    return width() - rightEdge_;
}

QVector<QRect> StatusStrip::probeRegions() const
{
    QVector<QRect> out;
    out.push_back(QRect(0, 0, coordWidth_, kStatusHeight));
    for (const Chip& chip : chips_)
        if (chip.width > 0) out.push_back(QRect(chip.left, 0, chip.width, kStatusHeight));
    for (const Cell* cell : {&sheetCell_, &agentCell_, &connCell_, &perfCell_})
        if (cell->shown) out.push_back(QRect(cell->left, 0, cell->width, kStatusHeight));
    return out;
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
    relayout();
    update();
}

void StatusStrip::setAgent(const QString& text, AgentState state)
{
    if (agent_ == text && agentState_ == state) return;
    agent_      = text;
    agentState_ = state;
    relayout();
    update();
}

void StatusStrip::setScale(const QString& text)
{
    if (scale_ == text) return;
    scale_ = text;
    relayout();
    update();
}

void StatusStrip::setCrs(const QString& text)
{
    if (crs_ == text) return;
    crs_ = text;
    relayout();
    update();
}

void StatusStrip::setPerformance(const QString& text)
{
    if (performance_ == text) return;
    performance_ = text;
    relayout();
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

namespace {

/// The part of an `A · B` reading before its first separator: `EPSG:5256` out
/// of `EPSG:5256 · TUREF/TM36`, `QRhi` out of `QRhi (GPU · geometri · yazı)`.
QString first_part(const QString& text)
{
    qsizetype at = text.indexOf(QStringLiteral(" · "));
    if (const qsizetype paren = text.indexOf(QStringLiteral(" ("));
        paren >= 0 && (at < 0 || paren < at))
        at = paren;
    return at < 0 ? text : text.left(at);
}

/// How many steps `relayout` may take before the chips themselves give way.
constexpr int kCompactionSteps = 10;

} // namespace

void StatusStrip::relayout()
{
    // The coordinate cell is as wide as its own text, so the chips start where
    // the reading ends and the whole strip stays left-packed as §7 draws it.
    coordWidth_ = cellWidth(coordinate_, true);

    // WHAT FITS, IN FULL, AND WHAT GIVES WAY FIRST. Every cell used to be drawn
    // at its full width from its own end — the chips from the left, the readings
    // from the right — and nothing stopped the two meeting. They met as soon as
    // the coordinate appeared under a moving cursor on a laptop-wide window, and
    // the strip drew `KALINLIK` through the middle of `EPSG:5256`. Now the strip
    // takes the fewest of these steps that make everything fit, each one keeping
    // what the step before it kept:
    //
    //   1 the backend keeps its first word              6 the scale goes too
    //   2 the system's name goes, its EPSG code stays   7 the connection goes
    //   3 the connection keeps only its icon            8 the chips close up
    //   4 the listener keeps only its mark              9 the sheet cell goes
    //   5 the backend goes                             10 the listener goes
    //
    // A shortened cell's full text is its tooltip (`event`), and whatever is
    // left over past step 10 is chips that do not fit and are not drawn.
    const QString sheetFull =
        scale_.isEmpty() && crs_.isEmpty() ? QString() : scale_ + QStringLiteral("  ·  ") + crs_;
    const QString crsShort   = first_part(crs_);
    const QString sheetShort = scale_.isEmpty()     ? crsShort
                               : crsShort.isEmpty() ? scale_
                                                    : scale_ + QStringLiteral("  ·  ") + crsShort;
    const QString perfShort  = first_part(performance_);

    const QFontMetrics chipMetrics(sans(kStatusPx, QFont::DemiBold, 0.4));
    int chipsWidth = 0;
    for (int step = 0; step <= kCompactionSteps; ++step) {
        compaction_ = step;
        chipPad_    = step >= 8 ? kStatusPadX / 2 : kStatusPadX;

        sheetCell_.text  = step >= 6 ? crsShort : step >= 2 ? sheetShort : sheetFull;
        sheetCell_.icon  = true;
        sheetCell_.shown = !sheetFull.isEmpty() && step < 9;
        perfCell_.text   = step >= 1 ? perfShort : performance_;
        perfCell_.icon   = false;
        perfCell_.shown  = !performance_.isEmpty() && step < 5;
        connCell_.text   = step >= 3 ? QString() : connection_;
        connCell_.icon   = true;
        connCell_.shown  = !connection_.isEmpty() && step < 7;
        agentCell_.text  = step >= 4 ? QString() : agent_;
        agentCell_.icon  = true;
        agentCell_.shown = !agent_.isEmpty() && step < 10;

        int right = 0;
        for (Cell* cell : {&perfCell_, &connCell_, &agentCell_, &sheetCell_}) {
            cell->width = cell->shown ? cellWidth(cell->text, cell->icon) : 0;
            right += cell->width;
        }
        chipsWidth = 0;
        for (const Chip& chip : chips_)
            chipsWidth +=
                chipPad_ + static_cast<int>(chipMetrics.horizontalAdvance(chip.label)) + chipPad_;

        // One pad of air between the chips and the first reading, always.
        if (coordWidth_ + 1 + chipsWidth + (right > 0 ? kStatusPadX : 0) + right <= width()) break;
    }

    // The readings, from the right edge leftwards.
    int x = width();
    for (Cell* cell : {&perfCell_, &connCell_, &agentCell_, &sheetCell_}) {
        if (!cell->shown) continue;
        x -= cell->width;
        cell->left = x;
    }
    rightEdge_ = x;
    agentRect_ =
        agentCell_.shown ? QRect(agentCell_.left, 1, agentCell_.width, kStatusHeight - 1) : QRect();

    // The chips, from the coordinate rightwards — and a chip that would reach
    // into the readings is not drawn at all rather than drawn under them.
    const int chipLimit = rightEdge_ - (rightEdge_ < width() ? kStatusPadX : 0);
    int cx              = coordWidth_ + 1;
    bool room           = true;
    for (Chip& chip : chips_) {
        chip.left = cx;
        chip.width =
            chipPad_ + static_cast<int>(chipMetrics.horizontalAdvance(chip.label)) + chipPad_;
        if (!room || chip.left + chip.width > chipLimit) {
            room       = false;
            chip.width = 0;
        }
        cx += chip.width;
    }
}

void StatusStrip::resizeEvent(QResizeEvent* event)
{
    QStatusBar::resizeEvent(event);
    relayout();
    update();
}

bool StatusStrip::event(QEvent* event)
{
    if (event->type() != QEvent::ToolTip) return QStatusBar::event(event);

    // THE WHOLE TEXT OF WHAT IS UNDER THE POINTER. A reading shortened to fit, a
    // message elided at the right, a coordinate: the strip gives each of them
    // less room than it wants, and hovering is how the rest is read.
    const auto* help = static_cast<QHelpEvent*>(event);
    const int at     = help->pos().x();
    QString text;
    const auto inside = [at](const Cell& cell) {
        return cell.shown && at >= cell.left && at < cell.left + cell.width;
    };
    if (at < coordWidth_) {
        text = coordinate_;
    } else if (inside(sheetCell_)) {
        text = scale_.isEmpty() ? crs_ : scale_ + QStringLiteral("  ·  ") + crs_;
    } else if (inside(agentCell_)) {
        text = agent_;
    } else if (inside(connCell_)) {
        text = connection_;
    } else if (inside(perfCell_)) {
        text = performance_;
    } else if (!busy_ && !chips_.isEmpty() && at >= chips_.back().left + chips_.back().width &&
               at < rightEdge_) {
        text = message_;
    }
    if (text.isEmpty()) {
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    QToolTip::showText(help->globalPos(), text, this);
    return true;
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
    // A chip under the busy cell is not there to point at (`chipsCovered_`).
    for (int i = 0; i < chips_.size() && !(busy_ && chipsCovered_); ++i)
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
    if (hot_ < 0 || (busy_ && chipsCovered_)) return;
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
        if (chip.width <= 0) continue; // no room for it at this width (`relayout`)

        // An ON aid is stated twice — a lighter ground AND the accent — because
        // colour alone is not a state a colour-blind user can read (§13).
        if (chip.on)
            p.fillRect(box, t.bgHeader);
        else if (i == hot_)
            p.fillRect(box, t.hoverRow);

        p.setPen(chip.on ? t.accent : (i == hot_ ? t.text : t.textFaint));
        if (chip.width > 0) p.drawText(box, Qt::AlignCenter, chip.label);
    }

    // ---- the right-hand cells ----
    //
    // Where `relayout` put them, at the length it chose. The message is given
    // the gap to their left and nothing beyond it: a long line — `ÖLÇ` writes
    // one, "Mesafe: 58,941 m  ΔY: 57,000 m  ΔX: 15,000 m  Açı: 83,6183 grad" —
    // was once elided to a box that ran under the MCP cell, and the two were
    // drawn on top of each other.
    const int rightEdge = rightEdge_;

    // ---- a job in flight: its label, a live segment, and Durdur ----
    //
    // Takes the message gap while it runs. The moving segment is what says the
    // program is alive when a 48 MB DXF is being read on another thread, and the
    // chip is the only way to stop that read short of closing the window.
    chipsCovered_ = false;
    if (busy_ && !chips_.isEmpty()) {
        int from     = chips_.back().left + chips_.back().width + kStatusPadX;
        const int to = rightEdge - kStatusPadX;
        const QFontMetrics chipMetrics(sans(kStatusPx, QFont::DemiBold, 0.4));
        const int stopWidth = kStatusPadX +
                              static_cast<int>(chipMetrics.horizontalAdvance(tr("Durdur"))) +
                              kStatusPadX;
        // NO ROOM BESIDE THE CHIPS, so the job takes theirs (TODOS F-05). A
        // narrow window left the gap too small and drew no Durdur at all, and a
        // job that can be stopped only by a key the user may not know is a job
        // they wait out. The aids are not what matters while it runs; they come
        // back when it ends, and a click there meanwhile toggles nothing.
        if (to - from <= stopWidth + (kStatusPadX * 4)) {
            from          = chips_.front().left + kStatusPadX;
            chipsCovered_ = true;
            p.fillRect(
                QRect(chips_.front().left, 1, rightEdge - chips_.front().left, kStatusHeight - 1),
                t.bgStrip);
        }
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
    if (perfCell_.shown) {
        const int x = perfCell_.left;
        p.setPen(t.textDim);
        p.drawText(QRect(x + kStatusPadX, 1, perfCell_.width, kStatusHeight - 1),
                   Qt::AlignVCenter | Qt::AlignLeft, perfCell_.text);
        p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
    }

    if (connCell_.shown) {
        const int x = connCell_.left;
        p.drawPixmap(
            QRect(x + kStatusPadX, (kStatusHeight - kStatusIcon) / 2, kStatusIcon, kStatusIcon),
            glyph_pixmap(Glyph::Cloud, connected_ ? t.ok : t.textFaint, kStatusIcon,
                         devicePixelRatioF()));
        p.setPen(t.textDim);
        p.drawText(QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, connCell_.width,
                         kStatusHeight - 1),
                   Qt::AlignVCenter | Qt::AlignLeft, connCell_.text);
        p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
    }

    // ---- the agent listener ----
    //
    // A MARK WITH A SHAPE, not a colour with a meaning (ui.md R31). Down is a
    // hollow ring, up-and-guarded is a filled dot, and up-with-no-token is a
    // filled TRIANGLE — the one shape in the shell that means "look at this" —
    // beside the word `KORUMASIZ`. Clicking the cell runs `MCPSUNUCU`, which is
    // the same command the menu entry runs. Short of room it keeps the mark,
    // which carries the state on its own.
    if (agentCell_.shown) {
        const int x = agentCell_.left;
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
        p.drawText(QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, agentCell_.width,
                         kStatusHeight - 1),
                   Qt::AlignVCenter | Qt::AlignLeft, agentCell_.text);
        p.fillRect(QRect(x, 1, 1, kStatusHeight - 1), t.lineSoft);
    }

    // ---- the sheet: the plot scale and the coordinate system ----
    //
    // Read, never edited here: the scale is YAZDIR's and AYAR plan_ölçeği's,
    // the system the project's (`design.md` §7).
    if (sheetCell_.shown) {
        const int x = sheetCell_.left;
        p.drawPixmap(
            QRect(x + kStatusPadX, (kStatusHeight - kStatusIcon) / 2, kStatusIcon, kStatusIcon),
            glyph_pixmap(Glyph::Globe, t.textFaint, kStatusIcon, devicePixelRatioF()));
        p.setFont(mono(kStatusPx));
        p.setPen(t.readout);
        p.drawText(QRect(x + kStatusPadX + kStatusIcon + kStatusGap, 1, sheetCell_.width,
                         kStatusHeight - 1),
                   Qt::AlignVCenter | Qt::AlignLeft, sheetCell_.text);
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
    relayout();
    update();
}

void PanelHeader::setCurrent(int index)
{
    if (index < 0 || index >= tabs_.size() || index == current_) return;
    current_ = index;
    relayout();
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
    const auto natural = [&label](const Tab& tab) {
        return kHeaderPadX + kHeaderIcon + kHeaderGap +
               static_cast<int>(label.horizontalAdvance(tab.label)) + kHeaderPadX;
    };
    constexpr int kIconOnly = kHeaderPadX + kHeaderIcon + kHeaderPadX;

    // WHAT FITS BESIDE THE MARKS. The tabs run from the left and the marks from
    // the right, and a dock with two long labels and five marks — the layers
    // and the external references — laid one over the other: the `+` was
    // painted under a tab. When the labels do not fit, the tabs not in use
    // show their icon alone (named on hover), and the one in use gives up
    // letters last.
    const int marks = static_cast<int>(buttonList().size());
    const int room  = width() - kHeaderRight - marks * (kHeaderBtn + kHeaderBtnGap) - kHeaderGap;
    int total       = 0;
    for (const Tab& tab : tabs_)
        total += natural(tab);
    const bool squeeze = width() > 0 && total > room && tabs_.size() > 1;

    int x = 0;
    for (int i = 0; i < tabs_.size(); ++i) {
        Tab& tab    = tabs_[i];
        tab.compact = squeeze && i != current_;
        tab.left    = x;
        tab.width   = tab.compact ? kIconOnly : natural(tab);
        x += tab.width;
    }
    if (squeeze && x > room && current_ >= 0 && current_ < tabs_.size()) {
        Tab& active  = tabs_[current_];
        active.width = std::max(kIconOnly, active.width - (x - room));
        x            = 0;
        for (Tab& tab : tabs_) {
            tab.left = x;
            x += tab.width;
        }
    }
}

void PanelHeader::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayout();
}

bool PanelHeader::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto* help = static_cast<QHelpEvent*>(event);
        const Hit hit    = hitAt(help->pos());
        if (hit.tab >= 0 && tabs_[hit.tab].compact) {
            QToolTip::showText(help->globalPos(), tabs_[hit.tab].label, this);
            return true;
        }
    }
    return QWidget::event(event);
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

        if (tab.compact) continue;
        p.setFont(sans(kHeaderLabel, isActive ? QFont::Medium : QFont::Normal));
        p.setPen(ink);
        const QRect words = box.adjusted(kHeaderPadX + kHeaderIcon + kHeaderGap,
                                         isActive ? kActiveEdge : 0, -kHeaderPadX, 0);
        p.drawText(words, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(tab.label, Qt::ElideRight, words.width()));
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
