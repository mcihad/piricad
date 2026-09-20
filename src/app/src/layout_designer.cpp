// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_designer.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/flow_layout.hpp"
#include "kentos_cad/app/layout_render.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/core/document.hpp"

#include <QDate>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kentos::app {
namespace {

/// The handle's side, in device pixels. Big enough to hit with a mouse, small
/// enough not to swallow a narrow item.
constexpr int kGripPx = 7;

/// The millimetre rulers along the top and the left edge, in device pixels.
///
/// WHY A PAGE EDITOR NEEDS ONE AND A PANEL DOES NOT. Every number in this
/// window is a paper millimetre and the sheet is a legal document: a frame at
/// 20 mm from the edge is a frame somebody can check against a regulation. A
/// canvas with no scale beside it asks the user to read those millimetres out
/// of a property field, one at a time, which is reading a drawing through a
/// keyhole. The ruler is the cheapest way to make the unit visible at all
/// times, and it is the one piece of chrome here that would be wrong on any
/// other panel in this program.
constexpr int kRulerPx = 22;

/// A drag snaps to whole paper millimetres. A title block at 20.0 mm is a title
/// block somebody can describe; one at 19.83 mm is an accident.
constexpr core::Um kSnap = 1000;

core::Um snapped(core::Um value)
{
    return static_cast<core::Um>(std::lround(static_cast<double>(value) / kSnap) * kSnap);
}

/// Paper micrometres as the millimetres a command line and a field carry.
QString mm_text(core::Um um)
{
    return QString::number(static_cast<double>(um) / 1000.0, 'f', 1);
}

/// The nine item kinds, in the order the bar offers them.
///
/// ONE TABLE, TWO READERS: the add bar draws a button per row and the item list
/// draws a row's glyph beside each placed item, so a kind cannot gain an icon in
/// one place and keep the wrong one in the other. `grafik` used to be missing
/// from the bar entirely — the command accepts `tur=grafik` and there was no way
/// to ask for one with the mouse, which is a capability that existed for a
/// script and not for a hand (CLAUDE.md 5.15).
struct Kind
{
    const char* word;           ///< what `tur=` takes
    core::LayoutItemKind which; ///< what the model calls it
    const char* label;          ///< what the button says
    Glyph glyph;                ///< what both the button and the list draw
};

constexpr Kind kKinds[] = {
    {"harita", core::LayoutItemKind::Map, "Harita", Glyph::Rectangle},
    {"metin", core::LayoutItemKind::Label, "Metin", Glyph::Text},
    {"olcek", core::LayoutItemKind::ScaleBar, "Ölçek", Glyph::Measure},
    {"kuzey", core::LayoutItemKind::NorthArrow, "Kuzey", Glyph::Locate},
    {"lejant", core::LayoutItemKind::Legend, "Lejant", Glyph::Table},
    {"resim", core::LayoutItemKind::Picture, "Resim", Glyph::Palette},
    {"sekil", core::LayoutItemKind::Shape, "Şekil", Glyph::Polygon},
    {"tablo", core::LayoutItemKind::Table, "Tablo", Glyph::Grid},
    {"grafik", core::LayoutItemKind::Chart, "Grafik", Glyph::Sigma},
};

Glyph glyph_of(core::LayoutItemKind kind)
{
    for (const Kind& one : kKinds)
        if (one.which == kind) return one.glyph;
    return Glyph::Rectangle;
}

/// Draws one item row: the kind's glyph, the item's name, its size in
/// millimetres at the far end, and a padlock when it is locked.
///
/// WHY NOT A PLAIN STRING. The rows read `baslik · Metin` — the MACHINE id
/// first and the kind second, which is a list of identifiers rather than a table
/// of contents. What a user looks for is "the title" and "the map", and what
/// they check next is how big it is; the id is a detail they need only when they
/// write a command line, so it moves to the tooltip. The glyph is the same one
/// the add bar used to place it, so the list and the bar name a kind the same
/// way.
class ItemRow : public QStyledItemDelegate
{
public:
    explicit ItemRow(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setTheme(ThemeMode mode) { theme_ = mode; }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return {QStyledItemDelegate::sizeHint(option, index).width(), 28};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        const bool on   = (option.state & QStyle::State_Selected) != 0;
        p->save();
        if (on) {
            p->fillRect(option.rect, t.accentWash);
            p->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                        t.accent);
        } else if ((option.state & QStyle::State_MouseOver) != 0) {
            p->fillRect(option.rect, t.hoverRow);
        }

        QRect box        = option.rect.adjusted(9, 0, -8, 0);
        const auto glyph = static_cast<Glyph>(index.data(Qt::UserRole + 1).toInt());
        p->drawPixmap(
            QRect(box.left(), box.top() + 6, 16, 16),
            glyph_pixmap(glyph, on ? t.accent : t.textDim, 16, p->device()->devicePixelRatioF()));
        box.setLeft(box.left() + 24);

        // THE SIZE IS RESERVED FIRST, so a long name is elided rather than
        // pushing the measurement off the panel.
        const QString size = index.data(Qt::UserRole + 2).toString();
        const int wide     = option.fontMetrics.horizontalAdvance(size) + 8;
        p->setPen(t.textFaint);
        p->drawText(QRect(box.right() - wide, box.top(), wide, box.height()),
                    Qt::AlignRight | Qt::AlignVCenter, size);
        box.setRight(box.right() - wide);

        if (index.data(Qt::UserRole + 3).toBool()) {
            p->drawPixmap(
                QRect(box.right() - 14, box.top() + 7, 14, 14),
                glyph_pixmap(Glyph::Lock, t.textFaint, 14, p->device()->devicePixelRatioF()));
            box.setRight(box.right() - 18);
        }

        p->setPen(on ? t.text : t.text);
        p->drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
                    option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, box.width()));
        p->restore();
    }

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// What a placed item is called on screen.
///
/// A LABEL IS CALLED BY WHAT IT SAYS. `baslik` is a key; "Ada 1284 / Pafta 3" is
/// the thing on the paper. Everything else has no text of its own and is called
/// by its kind, which is what a user would point at it and say.
QString item_name(const core::LayoutItem& item)
{
    const QString written = QString::fromStdString(item.text).simplified();
    if (!written.isEmpty() && written.size() <= 40) return written;
    return QString::fromUtf8(core::layout_item_kind_label(item.kind));
}

/// A list-valued argument as the one line a user edits, and as the command line
/// takes it back: `PARSEL, BINA`. Empty means "every visible one", which is the
/// default the command itself applies.
QString joined(const std::vector<std::string>& words)
{
    QStringList out;
    out.reserve(static_cast<qsizetype>(words.size()));
    for (const std::string& one : words)
        out << QString::fromStdString(one);
    return out.join(QStringLiteral(", "));
}

QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

} // namespace

// ========================================================== LayoutCanvas =====

LayoutCanvas::LayoutCanvas(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("layoutCanvas"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
}

void LayoutCanvas::setSheet(const QString& layout, int page)
{
    sheet_ = layout;
    page_  = page;
    selected_.clear();
    update();
}

const core::Layout* LayoutCanvas::layout() const
{
    return controller_.document().layouts().find(sheet_.toStdString());
}

void LayoutCanvas::select(const QString& id)
{
    if (selected_ == id) return;
    selected_ = id;
    update();
    emit selectionChanged(selected_);
}

void LayoutCanvas::refresh()
{
    // THE SELECTION IS CHECKED AGAINST THE DOCUMENT, not kept on faith: the item
    // may have been deleted by a command typed on the command line while this
    // window was open, and a selection naming nothing would draw handles around
    // an empty box.
    if (const core::Layout* l = layout();
        l == nullptr || l->find(selected_.toStdString()) == nullptr)
        selected_.clear();
    update();
}

QRectF LayoutCanvas::pageRect() const
{
    const core::Layout* l = layout();
    if (l == nullptr || l->pages.empty()) return {};
    const core::LayoutPage& page = *activePage();

    // FITTED AND CENTRED INSIDE THE RULERS, with air around it — which is what
    // makes the sheet read as a sheet rather than as the window's background.
    // The rulers take a gutter off the top and the left, so the paper is
    // centred in what is LEFT, not in the widget: centring it in the widget
    // would slide it half a gutter under the scale and the two would disagree.
    const double air     = 18.0;
    const double left    = kRulerPx;
    const double top     = kRulerPx;
    const double avail_w = std::max(1.0, width() - left - 2 * air);
    const double avail_h = std::max(1.0, height() - top - 2 * air);
    const double scale =
        std::min(avail_w / static_cast<double>(page.w), avail_h / static_cast<double>(page.h));
    const double w = static_cast<double>(page.w) * scale;
    const double h = static_cast<double>(page.h) * scale;
    return QRectF(left + (avail_w - w) / 2.0 + air, top + (avail_h - h) / 2.0 + air, w, h);
}

/// THE PAGE THE CANVAS IS SHOWING, clamped, never null when a layout is set.
///
/// `pageRect` sized the sheet from this while `deviceFrom`, `paperFrom` and
/// `dragged` scaled with `pages.front()`. On a layout whose second page is a
/// different size that meant every box was drawn — and dragged — somewhere other
/// than where it is. One question, one answer.
const core::LayoutPage* LayoutCanvas::activePage() const
{
    const core::Layout* l = layout();
    if (l == nullptr || l->pages.empty()) return nullptr;
    const auto index =
        static_cast<std::size_t>(std::clamp<int>(page_, 0, static_cast<int>(l->pages.size()) - 1));
    return &l->pages[index];
}

QRectF LayoutCanvas::deviceFrom(const core::PaperRect& paper) const
{
    const core::Layout* l = layout();
    const QRectF box      = pageRect();
    if (l == nullptr || l->pages.empty() || box.isEmpty()) return {};
    const core::LayoutPage& page = *activePage();
    const double sx              = box.width() / static_cast<double>(page.w);
    const double sy              = box.height() / static_cast<double>(page.h);
    return QRectF(box.left() + paper.x * sx, box.top() + paper.y * sy, paper.w * sx, paper.h * sy);
}

core::PaperRect LayoutCanvas::paperFrom(const QRectF& device) const
{
    const core::Layout* l = layout();
    const QRectF box      = pageRect();
    if (l == nullptr || l->pages.empty() || box.isEmpty()) return {};
    const core::LayoutPage& page = *activePage();
    const double sx              = static_cast<double>(page.w) / box.width();
    const double sy              = static_cast<double>(page.h) / box.height();
    return core::PaperRect{static_cast<core::Um>((device.left() - box.left()) * sx),
                           static_cast<core::Um>((device.top() - box.top()) * sy),
                           static_cast<core::Um>(device.width() * sx),
                           static_cast<core::Um>(device.height() * sy)};
}

LayoutCanvas::Grip LayoutCanvas::gripAt(const QPoint& at) const
{
    const core::Layout* l = layout();
    if (l == nullptr || selected_.isEmpty()) return Grip::None;
    const core::LayoutItem* item = l->find(selected_.toStdString());
    if (item == nullptr) return Grip::None;

    const QRectF box = deviceFrom(item->frame);
    if (box.isEmpty()) return Grip::None;

    const double g      = kGripPx;
    const bool left     = std::abs(at.x() - box.left()) <= g;
    const bool right    = std::abs(at.x() - box.right()) <= g;
    const bool top      = std::abs(at.y() - box.top()) <= g;
    const bool bottom   = std::abs(at.y() - box.bottom()) <= g;
    const bool inside_x = at.x() >= box.left() - g && at.x() <= box.right() + g;
    const bool inside_y = at.y() >= box.top() - g && at.y() <= box.bottom() + g;

    if (!inside_x || !inside_y) return Grip::None;
    if (left && top) return Grip::TopLeft;
    if (right && top) return Grip::TopRight;
    if (left && bottom) return Grip::BottomLeft;
    if (right && bottom) return Grip::BottomRight;
    if (left) return Grip::Left;
    if (right) return Grip::Right;
    if (top) return Grip::Top;
    if (bottom) return Grip::Bottom;
    return box.contains(at) ? Grip::Body : Grip::None;
}

Qt::CursorShape LayoutCanvas::cursorFor(Grip grip)
{
    switch (grip) {
    case Grip::Body: return Qt::SizeAllCursor;
    case Grip::TopLeft:
    case Grip::BottomRight: return Qt::SizeFDiagCursor;
    case Grip::TopRight:
    case Grip::BottomLeft: return Qt::SizeBDiagCursor;
    case Grip::Left:
    case Grip::Right: return Qt::SizeHorCursor;
    case Grip::Top:
    case Grip::Bottom: return Qt::SizeVerCursor;
    case Grip::None: break;
    }
    return Qt::ArrowCursor;
}

core::PaperRect LayoutCanvas::dragged(const QPoint& at) const
{
    const QRectF box      = pageRect();
    const core::Layout* l = layout();
    if (l == nullptr || box.isEmpty()) return start_;
    const core::LayoutPage& page = *activePage();

    const double sx = static_cast<double>(page.w) / box.width();
    const double sy = static_cast<double>(page.h) / box.height();
    const auto dx   = static_cast<core::Um>((at.x() - press_.x()) * sx);
    const auto dy   = static_cast<core::Um>((at.y() - press_.y()) * sy);

    core::PaperRect out = start_;
    switch (grip_) {
    case Grip::Body:
        out.x = snapped(start_.x + dx);
        out.y = snapped(start_.y + dy);
        break;
    case Grip::Left:
    case Grip::TopLeft:
    case Grip::BottomLeft:
        out.x = snapped(start_.x + dx);
        out.w = start_.w - (out.x - start_.x);
        break;
    case Grip::Right:
    case Grip::TopRight:
    case Grip::BottomRight: out.w = snapped(start_.w + dx); break;
    default: break;
    }
    switch (grip_) {
    case Grip::Top:
    case Grip::TopLeft:
    case Grip::TopRight:
        out.y = snapped(start_.y + dy);
        out.h = start_.h - (out.y - start_.y);
        break;
    case Grip::Bottom:
    case Grip::BottomLeft:
    case Grip::BottomRight: out.h = snapped(start_.h + dy); break;
    default: break;
    }

    // A MINIMUM OF ONE MILLIMETRE, so a resize that crosses its own edge leaves
    // something to grab rather than an item nobody can ever select again.
    out.w = std::max<core::Um>(out.w, kSnap);
    out.h = std::max<core::Um>(out.h, kSnap);
    return out;
}

void LayoutCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    const core::Layout* l = layout();
    if (l == nullptr) return;

    const QPoint at = event->pos();

    // THE SELECTED ITEM'S HANDLES WIN over anything under them: a handle sitting
    // on top of another item is still this item's handle, which is what lets a
    // small box be resized while it overlaps a big one.
    if (const Grip grip = gripAt(at); grip != Grip::None) {
        const core::LayoutItem* item = l->find(selected_.toStdString());
        if (item != nullptr && !item->locked) {
            grip_     = grip;
            dragging_ = true;
            press_    = at;
            start_    = item->frame;
            live_     = start_;
            return;
        }
    }

    // OTHERWISE THE TOPMOST ITEM UNDER THE POINTER, by paint order: what is in
    // front is what the eye means.
    const core::LayoutItem* hit = nullptr;
    for (const core::LayoutItem& item : l->items) {
        if (!deviceFrom(item.frame).contains(at)) continue;
        if (hit == nullptr || item.z >= hit->z) hit = &item;
    }
    select(hit != nullptr ? QString::fromStdString(hit->id) : QString());
}

void LayoutCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        live_ = dragged(event->pos());
        update();
        return;
    }
    setCursor(cursorFor(gripAt(event->pos())));
}

void LayoutCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (!dragging_ || event->button() != Qt::LeftButton) return;
    dragging_ = false;
    grip_     = Grip::None;

    // ONE COMMAND, AT THE END OF THE GESTURE. The motion was drawn; only the
    // result is recorded, or a drag across the page would be four hundred undo
    // entries.
    if (live_.x != start_.x || live_.y != start_.y || live_.w != start_.w || live_.h != start_.h)
        emit itemMoved(selected_, live_);
    update();
}

void LayoutCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !selected_.isEmpty()) emit itemActivated(selected_);
}

void LayoutCanvas::keyPressEvent(QKeyEvent* event)
{
    const core::Layout* l = layout();
    if (l == nullptr || selected_.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }
    const core::LayoutItem* item = l->find(selected_.toStdString());
    if (item == nullptr || item->locked) {
        QWidget::keyPressEvent(event);
        return;
    }

    // ARROW KEYS NUDGE BY A MILLIMETRE, Shift by ten. The keyboard reaches every
    // gesture the mouse does (ui.md R21), and a millimetre is the unit the rest
    // of the window is written in.
    const core::Um step   = (event->modifiers() & Qt::ShiftModifier) != 0 ? 10 * kSnap : kSnap;
    core::PaperRect moved = item->frame;
    switch (event->key()) {
    case Qt::Key_Left: moved.x -= step; break;
    case Qt::Key_Right: moved.x += step; break;
    case Qt::Key_Up: moved.y -= step; break;
    case Qt::Key_Down: moved.y += step; break;
    default: QWidget::keyPressEvent(event); return;
    }
    emit itemMoved(selected_, moved);
}

void LayoutCanvas::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

/// THE TWO MILLIMETRE SCALES, and the selection's span lit on them.
///
/// WHAT IT ANSWERS. "Where on the paper is this, and how wide is it" — asked of
/// the drawing rather than of a property field. The lit span is the whole point:
/// it turns four numbers a user would otherwise read one at a time into one
/// picture, and it moves live while a frame is dragged, so the drag itself is
/// measured rather than merely watched.
///
/// THE STEP IS CHOSEN FROM THE ZOOM, not fixed. A 5 mm tick on an A0 sheet
/// fitted to a laptop is a grey smear; the coarsest step whose ticks stay at
/// least four pixels apart is used instead, so the scale stays readable at every
/// paper size this program can open.
void LayoutCanvas::paintRulers(QPainter& p, const QRectF& box, const core::LayoutItem* item) const
{
    const Tokens& t              = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
    const core::LayoutPage* page = activePage();
    if (page == nullptr || box.width() <= 0.0) return;

    // THE PEN AND THE BRUSH GO BACK AS THEY WERE.
    //
    // Without this the corner's `bgStrip` brush was still set when the caller
    // drew the selection's outline next, so the picked box came out FILLED with
    // the ruler's own grey — barely visible in the light theme and a black hole
    // over the map frame in the dark one. A helper that paints has to leave the
    // painter as it found it.
    p.save();

    const double per_mm = box.width() / (static_cast<double>(page->w) / 1000.0);

    // TICKS AND NUMBERS ARE CHOSEN SEPARATELY, from the same ladder. A tick is
    // legible at four pixels apart; a three-digit number needs closer to fifty,
    // and pinning the numbers to "every fifth tick" put 25 mm labels shoulder to
    // shoulder on an A3 and left an A0 with almost none.
    static constexpr core::Um kLadder[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000};
    const auto coarsest                 = [&](double apart) {
        core::Um chosen = kLadder[std::size(kLadder) - 1];
        for (const core::Um candidate : kLadder)
            if (per_mm * (static_cast<double>(candidate) / 1000.0) >= apart) {
                chosen = candidate;
                break;
            }
        return chosen;
    };
    const core::Um step     = coarsest(4.0);
    const core::Um labelled = std::max(coarsest(46.0), step);

    p.setPen(Qt::NoPen);
    p.setBrush(t.bgStrip);
    p.drawRect(QRectF(0, 0, width(), kRulerPx));
    p.drawRect(QRectF(0, 0, kRulerPx, height()));

    // THE LIT SPAN, under the ticks so the numbers stay legible on top of it.
    if (item != nullptr) {
        const QRectF lit = dragging_ ? deviceFrom(live_) : deviceFrom(item->frame);
        p.setBrush(t.accentWash);
        p.drawRect(QRectF(lit.left(), 0, lit.width(), kRulerPx));
        p.drawRect(QRectF(0, lit.top(), kRulerPx, lit.height()));
        p.setPen(QPen(t.accent, 2.0));
        p.drawLine(QPointF(lit.left(), kRulerPx - 1.0), QPointF(lit.right(), kRulerPx - 1.0));
        p.drawLine(QPointF(kRulerPx - 1.0, lit.top()), QPointF(kRulerPx - 1.0, lit.bottom()));
        p.setPen(Qt::NoPen);
    }

    QFont small = p.font();
    small.setPointSizeF(std::max(7.0, small.pointSizeF() - 2.0));
    p.setFont(small);

    const auto ticks = [&](bool horizontal) {
        const core::Um extent = horizontal ? page->w : page->h;
        const double origin   = horizontal ? box.left() : box.top();
        const double scale    = horizontal ? box.width() / static_cast<double>(extent)
                                           : box.height() / static_cast<double>(extent);
        for (core::Um at = 0; at <= extent; at += step) {
            const double pos  = origin + static_cast<double>(at) * scale;
            const bool named  = at % labelled == 0;
            const double from = named ? 4.0 : kRulerPx - 5.0;
            p.setPen(QPen(named ? t.textFaint : t.rulerTick, 1.0));
            if (horizontal)
                p.drawLine(QPointF(pos, from), QPointF(pos, kRulerPx - 1.0));
            else
                p.drawLine(QPointF(from, pos), QPointF(kRulerPx - 1.0, pos));
            if (!named) continue;

            // THE NUMBER RIDES THE TICK, and the vertical scale reads
            // top-to-bottom like the page coordinate it names — no rotated
            // text, which at 7 pt is a smudge on every platform.
            p.setPen(t.textDim);
            const QString text = QString::number(at / 1000);
            if (horizontal)
                p.drawText(QRectF(pos + 3.0, 1.0, 34.0, kRulerPx - 6.0),
                           Qt::AlignLeft | Qt::AlignVCenter, text);
            else
                p.drawText(QRectF(1.0, pos + 1.0, kRulerPx - 5.0, 11.0),
                           Qt::AlignRight | Qt::AlignTop, text);
        }
    };
    ticks(true);
    ticks(false);

    // The corner, where the two scales meet: the unit, said once.
    p.setPen(Qt::NoPen);
    p.setBrush(t.bgStrip);
    p.drawRect(QRectF(0, 0, kRulerPx, kRulerPx));
    p.setPen(t.textFaint);
    p.drawText(QRectF(0, 0, kRulerPx, kRulerPx), Qt::AlignCenter, tr("mm"));

    p.setPen(QPen(t.lineSoft, 1.0));
    p.drawLine(QPointF(0, kRulerPx - 0.5), QPointF(width(), kRulerPx - 0.5));
    p.drawLine(QPointF(kRulerPx - 0.5, 0), QPointF(kRulerPx - 0.5, height()));
    p.restore();
}

void LayoutCanvas::paintEvent(QPaintEvent*)
{
    const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
    QPainter p(this);
    p.fillRect(rect(), t.bgApp);

    const core::Layout* l = layout();
    const QRectF box      = pageRect();
    if (l == nullptr || box.isEmpty()) {
        p.setPen(t.textFaint);
        p.drawText(rect(), Qt::AlignCenter, tr("Çıktı yerleşimi yok."));
        return;
    }

    // A SHADOW UNDER THE SHEET: the one piece of chrome that says "this is
    // paper on a table" without a word. Three falling passes rather than one
    // hard offset — a single 40% rectangle reads as a second sheet behind the
    // first, which is the opposite of what a shadow is for.
    p.setPen(Qt::NoPen);
    for (const auto& [drop, alpha] : {std::pair{6.0, 12}, std::pair{4.0, 18}, std::pair{2.0, 26}}) {
        p.setBrush(QColor(0, 0, 0, alpha));
        p.drawRect(box.adjusted(-drop + 2, -drop + 4, drop + 2, drop + 4));
    }

    LayoutFacts facts;
    facts.sheet = QString::fromStdString(l->name);
    // `Bus::on_current_file` is what KAYDET reads to know where the drawing came
    // from; an unsaved drawing answers with nothing and `<proje>` is then empty,
    // which is the truth rather than a made-up name.
    facts.project =
        controller_.bus().on_current_file
            ? QFileInfo(QString::fromStdString(controller_.bus().on_current_file())).fileName()
            : QString();
    facts.crs  = QString::fromStdString(controller_.document().crs().id());
    facts.date = QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy"));

    // AND WHERE THE PROJECT LIVES, so a logo stored beside it is found after the
    // folder has been copied to somebody else's machine (`LayoutFacts`).
    if (controller_.bus().on_current_file)
        facts.project_dir =
            QFileInfo(QString::fromStdString(controller_.bus().on_current_file())).absolutePath();

    // THE SCREEN'S OWN DPI, so a 0.25 mm hairline on the sheet is a hairline
    // here too — the designer shows what the printer will do, at a different
    // size (`layout_render.hpp`).
    const double dpi = logicalDpiX() > 0 ? logicalDpiX() : 96.0;
    paint_layout_page(p, box, controller_.document(), *l, page_, dpi, facts,
                      /*margin_guide=*/true);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(t.lineHard, 1.0));
    p.drawRect(box);

    const core::LayoutItem* item = selected_.isEmpty() ? nullptr : l->find(selected_.toStdString());
    paintRulers(p, box, item);

    if (item == nullptr) return;

    const QRectF chosen = dragging_ ? deviceFrom(live_) : deviceFrom(item->frame);
    p.setPen(QPen(t.accent, 1.0, item->locked ? Qt::DashLine : Qt::SolidLine));
    p.drawRect(chosen);

    // NO HANDLES ON A LOCKED ITEM: a handle that refuses to drag is a control
    // that lies about what it does.
    if (item->locked) return;
    p.setBrush(t.accent);
    p.setPen(QPen(Qt::white, 1.0));
    const double g = kGripPx / 2.0;
    for (const QPointF& corner :
         {chosen.topLeft(), QPointF(chosen.center().x(), chosen.top()), chosen.topRight(),
          QPointF(chosen.right(), chosen.center().y()), chosen.bottomRight(),
          QPointF(chosen.center().x(), chosen.bottom()), chosen.bottomLeft(),
          QPointF(chosen.left(), chosen.center().y())})
        p.drawRect(QRectF(corner.x() - g, corner.y() - g, 2 * g, 2 * g));
}

} // namespace kentos::app

// ======================================================== LayoutDesigner =====

namespace kentos::app {

LayoutDesigner::LayoutDesigner(Controller& controller, QString layout, QWidget* parent)
    : DialogFrame(parent), controller_(controller), name_(std::move(layout))
{
    setHeading(Glyph::Print, tr("Çıktı Yerleşimi Tasarımcısı"), QStringLiteral("— %1").arg(name_));
    setBody(buildBody());
    resize(1180, 760);

    auto* close = new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer()->addWidget(close);

    auto* print = new Button(ButtonRole::Secondary, tr("Yazdır…"), Glyph::Print, this);
    connect(print, &QPushButton::clicked, this, [this] { exportSheet(); });
    footer()->addWidget(print);

    auto* pdf = new Button(ButtonRole::Primary, tr("PDF'e aktar…"), Glyph::Export, this);
    connect(pdf, &QPushButton::clicked, this, [this] { exportSheet(); });
    footer()->addWidget(pdf);

    // THE WINDOW FOLLOWS THE DOCUMENT, not its own record of it: a `ÇIKTIÖĞE`
    // line typed on the command line while this is open redraws it, which is
    // what makes the two clients equal rather than merely both present.
    connect(&controller_, &Controller::documentChanged, this, [this] { refresh(); });

    refresh();
    LayoutDesigner::applyTheme(theme());
}

const core::Layout* LayoutDesigner::layout() const
{
    return controller_.document().layouts().find(name_.toStdString());
}

QWidget* LayoutDesigner::buildBody()
{
    auto* body = new QWidget(this);
    auto* row  = new QHBoxLayout(body);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // ---- left: what is on the sheet, and what can be added -------------------
    auto* left = new QWidget(body);
    left->setFixedWidth(236);
    auto* leftColumn = new QVBoxLayout(left);
    leftColumn->setContentsMargins(12, 12, 8, 12);
    leftColumn->setSpacing(8);
    // ---- which page, and the three things one can do to pages --------------
    //
    // ONE PAGE IS SHOWN AT A TIME, which is what a sheet is: the canvas draws
    // it, the item list holds its items and every drag is measured against its
    // paper. Without this the designer could open a two-page layout and only
    // ever reach the first one.
    //
    // A PAGER, NOT A FORM FIELD. This used to be a full-height labelled input
    // under a section heading — three rows and ninety pixels to answer "which
    // of one page". It is one strip now: step back, the number (still typed,
    // because a sixty-page atlas is not paged through by clicking), how many
    // there are, step forward, and the three page verbs. No heading, because a
    // row of arrows around a number does not need to be told it is a pager.
    auto* pager    = new QWidget(left);
    auto* pagerRow = new QHBoxLayout(pager);
    pagerRow->setContentsMargins(0, 0, 0, 0);
    pagerRow->setSpacing(4);

    const auto stepper = [&](Glyph glyph, const QString& tip, int by) {
        auto* button = new Button(ButtonRole::Icon, QString(), glyph, pager);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        button->setControlSize(ControlSize::Compact);
        connect(button, &QPushButton::clicked, this, [this, by] {
            const core::Layout* l = layout();
            if (l == nullptr) return;
            canvas_->setSheet(
                name_, std::clamp(canvas_->page() + by, 0, static_cast<int>(l->pages.size()) - 1));
            refresh();
        });
        pagerRow->addWidget(button);
        return button;
    };
    stepper(Glyph::ChevronLeft, tr("Önceki sayfa"), -1);

    pageField_ = new Field(number_of(1, 9999), pager);
    pageField_->setFixedHeight(static_cast<int>(ControlSize::Compact));
    pageField_->setFixedWidth(46);
    pageField_->setAccessibleName(tr("Sayfa"));
    connect(pageField_, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        const core::Layout* l = layout();
        if (l == nullptr) return;
        const int wanted = std::clamp(typed.toInt() - 1, 0, static_cast<int>(l->pages.size()) - 1);
        canvas_->setSheet(name_, wanted);
        refresh();
    });
    pagerRow->addWidget(pageField_);

    // HOW MANY THERE ARE, beside the number rather than in a help line under
    // it: "3" means nothing without "/ 12".
    pageCount_ = new QLabel(pager);
    pageCount_->setObjectName(QStringLiteral("formHelp"));
    pagerRow->addWidget(pageCount_);

    stepper(Glyph::ChevronRight, tr("Sonraki sayfa"), 1);
    pagerRow->addStretch(1);

    struct PageVerb
    {
        const char* verb;
        const char* label;
        Glyph glyph;
    };

    static constexpr PageVerb kPageVerbs[] = {
        {"sayfaekle", "Sayfa ekle", Glyph::Plus},
        {"sayfacogalt", "Sayfayı çoğalt", Glyph::Copy},
        {"sayfasil", "Sayfayı sil", Glyph::Trash},
    };
    for (const PageVerb& one : kPageVerbs) {
        auto* button = new Button(ButtonRole::Icon, QString(), one.glyph, pager);
        button->setToolTip(tr(one.label));
        button->setAccessibleName(tr(one.label));
        button->setControlSize(ControlSize::Compact);
        connect(button, &Button::clicked, this, [this, verb = one.verb] { pageVerb(verb); });
        pagerRow->addWidget(button);
    }
    leftColumn->addWidget(pager);

    itemsHead_ = new FormSection(tr("ÖĞELER"), QString(), left);
    leftColumn->addWidget(itemsHead_);
    leftColumn->addWidget(buildItemList(), 1);

    // ---- what can be added ---------------------------------------------------
    //
    // LABELLED, IN TWO COLUMNS. These were eight 32 px icon squares in a flow
    // that wrapped 5 + 3, and an icon square is a guessing game: the tooltip
    // only answers a question the user has to think to ask. Nine words in two
    // columns cost the same strip of panel and answer it without being asked.
    leftColumn->addWidget(new FormSection(tr("EKLE"), QString(), left));

    auto* adders = new QWidget(left);
    auto* grid   = new QGridLayout(adders);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    int at = 0;
    for (const Kind& one : kKinds) {
        auto* button = new Button(ButtonRole::Ghost, tr(one.label), one.glyph, adders);
        button->setControlSize(ControlSize::Compact);
        button->setToolTip(tr("%1 ekle").arg(tr(one.label)));
        const QString kind = QString::fromUtf8(one.word);
        connect(button, &QPushButton::clicked, this, [this, kind] { addItem(kind); });
        grid->addWidget(button, at / 2, at % 2);
        ++at;
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    leftColumn->addWidget(adders);

    // DISABLED WITH NOTHING PICKED. It used to be lit at all times and silently
    // do nothing when pressed, which is a control that lies about what it does.
    remove_ = new Button(ButtonRole::Danger, tr("Seçili öğeyi sil"), Glyph::Trash, left);
    remove_->setControlSize(ControlSize::Compact);
    remove_->setEnabled(false);
    connect(remove_, &QPushButton::clicked, this, [this] {
        if (canvas_->selected().isEmpty()) return;
        controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=sil yerlesim=%1 ad=%2")
                                .arg(quoted(name_), quoted(canvas_->selected())),
                            command::Origin::Gui);
        refresh();
    });
    leftColumn->addWidget(remove_);
    row->addWidget(left);

    // ---- middle: the page ----------------------------------------------------
    auto* middle       = new QWidget(body);
    auto* middleColumn = new QVBoxLayout(middle);
    middleColumn->setContentsMargins(0, 0, 0, 0);
    middleColumn->setSpacing(0);

    canvas_ = new LayoutCanvas(controller_, middle);
    canvas_->setSheet(name_, 0);
    connect(canvas_, &LayoutCanvas::selectionChanged, this, [this](const QString& id) {
        if (items_ == nullptr) return;
        filling_ = true;
        for (int i = 0; i < items_->count(); ++i)
            if (items_->item(i)->data(Qt::UserRole).toString() == id) items_->setCurrentRow(i);
        filling_ = false;
        refresh();
    });
    connect(
        canvas_, &LayoutCanvas::itemMoved, this, [this](const QString& id, core::PaperRect frame) {
            // THE GESTURE BECOMES THE COMMAND. This is the line a script
            // would type, so what a hand can do a batch job can do too.
            controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=tasi yerlesim=%1 ad=%2 x=%3 y=%4 "
                                               "genislik=%5 yukseklik=%6")
                                    .arg(quoted(name_), quoted(id), mm_text(frame.x),
                                         mm_text(frame.y), mm_text(frame.w), mm_text(frame.h)),
                                command::Origin::Gui);
            refresh();
        });
    middleColumn->addWidget(canvas_, 1);

    // THE HINT LIVES UNDER THE DRAWING IT IS ABOUT.
    //
    // It used to hang at the bottom of the inspector, a thousand pixels below
    // the panel it shared a column with and nowhere near the sheet whose
    // gestures it describes. It is one line about the canvas; it belongs under
    // the canvas.
    status_ = new QLabel(middle);
    status_->setObjectName(QStringLiteral("formHelp"));
    status_->setWordWrap(true);
    status_->setContentsMargins(kRulerPx + 8, 4, 12, 8);
    middleColumn->addWidget(status_);
    row->addWidget(middle, 1);

    // ---- right: the selected item's properties -------------------------------
    auto* right = new QWidget(body);
    right->setFixedWidth(302);
    auto* rightColumn = new QVBoxLayout(right);
    rightColumn->setContentsMargins(8, 12, 12, 12);
    rightColumn->setSpacing(8);
    // A HEADING THAT SAYS WHAT IS BEING INSPECTED, not one that says the panel
    // is a panel. `ÖZELLİKLER` over a column of property rows is a label naming
    // the obvious; `HARİTA ÇERÇEVESİ · harita` tells the reader which of the
    // eleven boxes on the sheet these numbers belong to, and its note carries
    // the id a command line would take.
    propertiesHead_ = new FormSection(tr("SAYFA"), QString(), right);
    rightColumn->addWidget(propertiesHead_);

    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    properties_     = new QWidget(scroll);
    propertyColumn_ = new QVBoxLayout(properties_);
    propertyColumn_->setContentsMargins(0, 0, 0, 0);
    propertyColumn_->setSpacing(8);
    scroll->setWidget(properties_);
    rightColumn->addWidget(scroll, 1);

    row->addWidget(right);

    return body;
}

QWidget* LayoutDesigner::buildItemList()
{
    items_ = new QListWidget(this);
    items_->setObjectName(QStringLiteral("layoutItems"));
    items_->setFrameShape(QFrame::NoFrame);
    items_->setMouseTracking(true);
    rows_ = new ItemRow(items_);
    items_->setItemDelegate(rows_);
    connect(items_, &QListWidget::currentRowChanged, this, [this](int at) {
        if (filling_ || at < 0 || items_->item(at) == nullptr) return;
        canvas_->select(items_->item(at)->data(Qt::UserRole).toString());
    });
    return items_;
}

void LayoutDesigner::addItem(const QString& kind)
{
    controller_.runLine(
        QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=%1 tur=%2").arg(quoted(name_), kind),
        command::Origin::Gui);

    // THE NEW ITEM IS SELECTED, because adding one and then having to find it is
    // two gestures for one intention. It is the last in the list, since `ekle`
    // appends.
    if (const core::Layout* l = layout(); l != nullptr && !l->items.empty())
        canvas_->select(QString::fromStdString(l->items.back().id));
    refresh();
}

void LayoutDesigner::edit(const QString& arguments, const QString& verb)
{
    if (canvas_->selected().isEmpty()) return;
    controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=%1 yerlesim=%2 ad=%3 %4")
                            .arg(verb, quoted(name_), quoted(canvas_->selected()), arguments),
                        command::Origin::Gui);
    refresh();
}

void LayoutDesigner::sheetEdit(const QString& change)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;

    // ONE PAGE: THE WHOLE SHEET. `sayfa=` narrows the change to that page and
    // deliberately leaves the layout's own paper NAME alone, because the name
    // stops being true the moment two pages differ. On a one-page layout the
    // two readings are the same change, so the command is written without it
    // and the sheet keeps a name — `A3 yatay` rather than `420 × 297 mm`.
    const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l->pages.size()) - 1);
    const core::LayoutPage& page = l->pages[static_cast<std::size_t>(shown)];
    const QString at = l->pages.size() > 1 ? QStringLiteral(" sayfa=%1").arg(shown + 1) : QString();

    // ALL FOUR, EVERY TIME — and this is not belt and braces.
    //
    // `islem=sayfa` DEFAULTS what it is not told: an omitted `kagit` is A4, an
    // omitted `yon` is dikey, an omitted `kenar` is 10. So a line that says only
    // `yon=yatay` does not turn an A3 sideways — it turns it into an A4 and
    // resets the margin on the way. Four fields that each quietly undid the
    // other three is not a panel anybody can use, so the panel writes the sheet
    // as it should END UP, with the field the user touched overriding what is
    // there now. The line is also what a hand would type to get this sheet.
    const QString paper =
        l->paper.empty() ? QStringLiteral("ozel") : QString::fromStdString(l->paper);
    QString whole =
        QStringLiteral("kagit=%1 yon=%2 kenar=%3 dpi=%4")
            .arg(paper, page.w > page.h ? QStringLiteral("yatay") : QStringLiteral("dikey"))
            .arg(l->margin / 1000)
            .arg(l->dpi);
    // A CUSTOM PAPER CARRIES ITS SIZE, because `ozel` without one is refused.
    // The command swaps the two for `yon=yatay`, so they are given the way it
    // expects them: the portrait pair.
    if (l->paper.empty())
        whole += QStringLiteral(" genislik=%1 yukseklik=%2")
                     .arg(std::min(page.w, page.h) / 1000)
                     .arg(std::max(page.w, page.h) / 1000);

    controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=sayfa ad=%1%2 %3 %4")
                            .arg(quoted(name_), at, whole, change),
                        command::Origin::Gui);
    refresh();
}

/// ONE PAGE VERB, ON THE PAGE THAT IS SHOWING.
///
/// The designer holds no page logic of its own: it writes the command line a
/// hand would type, and the command does the work. That is what keeps the
/// mouse and the keyboard equal clients (Article 1.2) and what puts the gesture
/// in the journal as something a script can replay.
void LayoutDesigner::pageVerb(const char* verb)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l->pages.size()) - 1);

    controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=%1 ad=%2 sayfa=%3")
                            .arg(QString::fromUtf8(verb), quoted(name_))
                            .arg(shown + 1),
                        command::Origin::Gui);

    // THE PAGE THAT IS NOW SHOWING may not be the one that was: deleting the
    // last page has to leave the canvas on a page that exists.
    const core::Layout* after = layout();
    if (after != nullptr && !after->pages.empty())
        canvas_->setSheet(name_, std::clamp(shown, 0, static_cast<int>(after->pages.size()) - 1));
    refresh();
}

void LayoutDesigner::refresh()
{
    const core::Layout* l = layout();
    if (l == nullptr) {
        if (status_ != nullptr) status_->setText(tr("Çıktı yerleşimi silinmiş: %1").arg(name_));
        return;
    }

    filling_             = true;
    const QString chosen = canvas_->selected();

    // HOW MANY ARE ON THIS PAGE. Counted before the list is filled, because the
    // heading's note and the status strip both say it and a second walk of the
    // same vector to answer the same question twice is a second answer waiting
    // to disagree.
    std::size_t here = 0;
    for (std::size_t i = 0; i < l->items.size(); ++i)
        if (l->page_of(i) == canvas_->page()) ++here;

    items_->clear();
    // PAINT ORDER, TOP FIRST: the list reads the way the sheet looks.
    std::vector<std::size_t> ordered(l->items.size());
    std::iota(ordered.begin(), ordered.end(), std::size_t{0});
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&](std::size_t a, std::size_t b) { return l->items[a].z > l->items[b].z; });
    for (const std::size_t at : ordered) {
        // ONLY THIS PAGE'S. A list that showed every page's items would let a
        // click select something that is not on the sheet in front of it.
        if (l->page_of(at) != canvas_->page()) continue;
        const core::LayoutItem* item = &l->items[at];
        auto* row                    = new QListWidgetItem(item_name(*item), items_);
        row->setData(Qt::UserRole, QString::fromStdString(item->id));
        row->setData(Qt::UserRole + 1, static_cast<int>(glyph_of(item->kind)));
        row->setData(Qt::UserRole + 2,
                     QStringLiteral("%1×%2").arg(mm_text(item->frame.w), mm_text(item->frame.h)));
        row->setData(Qt::UserRole + 3, item->locked);
        // THE ID IS STILL REACHABLE, because it is what a command line takes —
        // it is just no longer the first thing a reader has to step over.
        row->setToolTip(
            item->locked
                ? tr("%1 · %2 · kilitli")
                      .arg(QString::fromStdString(item->id),
                           QString::fromUtf8(core::layout_item_kind_label(item->kind)))
                : tr("%1 · %2").arg(QString::fromStdString(item->id),
                                    QString::fromUtf8(core::layout_item_kind_label(item->kind))));
        if (QString::fromStdString(item->id) == chosen) items_->setCurrentItem(row);
    }
    if (pageField_ != nullptr) {
        const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l->pages.size()) - 1);
        pageField_->setValue(QString::number(shown + 1));
        if (pageCount_ != nullptr) pageCount_->setText(tr("/ %1").arg(l->pages.size()));
    }
    if (itemsHead_ != nullptr) itemsHead_->setNote(here == 0 ? tr("boş") : tr("%1 öğe").arg(here));
    if (remove_ != nullptr) remove_->setEnabled(!chosen.isEmpty());
    filling_ = false;

    canvas_->refresh();
    buildProperties();

    status_->setText(tr("Kutuları sürükleyin, köşelerinden boyutlandırın; ok tuşları 1 mm, "
                        "Shift ile 10 mm kaydırır."));
}

QWidget* LayoutDesigner::buildProperties()
{
    // Cleared and rebuilt, rather than kept and reconciled: the fields a Map
    // shows and the fields a Label shows have nothing in common past the frame,
    // and a panel that hid half its widgets would be a panel whose layout
    // depends on what was selected before.
    while (QLayoutItem* old = propertyColumn_->takeAt(0)) {
        if (QWidget* w = old->widget(); w != nullptr) {
            // UNPARENTED FIRST, THEN DELETED LATER — and the order is the whole
            // bug. `takeAt` removes the widget from the LAYOUT immediately, but
            // `deleteLater` leaves it a visible CHILD of the panel until the
            // event loop next spins. In between it is unmanaged: it keeps
            // drawing at whatever coordinates it last had.
            //
            // So every rebuild painted the new rows ON TOP OF the old ones:
            // the empty-state hint and both toggle rows landed in the same few
            // pixels, which is the pile-up a user sees as a broken panel after
            // touching a switch. Nothing was laid out wrong; the previous panel
            // had simply never left.
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete old;
    }

    const core::Layout* l = layout();
    if (l == nullptr) return properties_;
    const core::LayoutItem* item =
        canvas_->selected().isEmpty() ? nullptr : l->find(canvas_->selected().toStdString());

    filling_ = true;
    if (item == nullptr)
        buildSheetProperties(*l);
    else
        buildItemProperties(*l, *item);
    propertyColumn_->addStretch(1);
    filling_ = false;

    // AND THE NEW ROWS ARE TOLD WHICH THEME THEY ARE IN.
    //
    // `DialogFrame::applyTheme` walks the children ONCE, when the window is
    // built. Everything here is built again on every selection, after that walk
    // has run — so a control made now keeps `Themed`'s default, which is DARK.
    // The grid drop-down came out black-on-white in the light theme for exactly
    // that reason: nobody had told it. The panel tells its own children, since
    // it is the only thing that knows they are new.
    applyThemeToChildren(properties_, theme());
    return properties_;
}

/// TWO FIELDS ON ONE LINE, because `x` and `y` are one fact.
///
/// `FormRow` stacks its label over its editor, so four framing numbers in four
/// rows ran two hundred pixels down a three-hundred-pixel-wide column and still
/// left the rest of it empty. Position is a pair and size is a pair; pairing
/// them halves the run and puts the two numbers a user compares side by side.
QWidget* LayoutDesigner::pairOf(QWidget* left, QWidget* right)
{
    auto* both = new QWidget(properties_);
    auto* row  = new QHBoxLayout(both);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    // TOP-ALIGNED. One of a pair often carries a help line and the other does
    // not; centred, the taller one pushes the shorter one's LABEL down and the
    // two captions sit on different baselines, which reads as a mistake.
    row->addWidget(left, 1, Qt::AlignTop);
    row->addWidget(right, 1, Qt::AlignTop);
    return both;
}

/// A millimetre field that writes `name=` on the selected item when committed.
FormRow* LayoutDesigner::mmRow(const QString& label, core::Um value, const char* name)
{
    FieldSpec spec = decimal_of(1);
    spec.suffix    = QStringLiteral("mm");
    auto* field    = new Field(spec, properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    field->setValue(mm_text(value));
    field->setAccessibleName(label);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key](const QString& typed) {
        if (filling_) return;
        edit(QStringLiteral("%1=%2").arg(key, QString::number(typed.toDouble(), 'f', 1)));
    });
    return new FormRow(label, field, properties_);
}

/// A whole-number field that writes `name=` on the selected item.
FormRow* LayoutDesigner::countRow(const QString& label, long long value, const char* name, int most)
{
    auto* field = new Field(number_of(0, most), properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    field->setValue(QString::number(value));
    field->setAccessibleName(label);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key](const QString& typed) {
        if (filling_) return;
        edit(QStringLiteral("%1=%2").arg(key).arg(typed.toLongLong()));
    });
    return new FormRow(label, field, properties_);
}

/// A free-text field that writes `name=` on the selected item, quoted.
FormRow* LayoutDesigner::textRow(const QString& label, const QString& value, const char* name,
                                 const QString& hint)
{
    FieldSpec spec   = field_of(FieldKind::Text);
    spec.placeholder = hint;
    auto* field      = new Field(spec, properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    field->setValue(value);
    field->setAccessibleName(label);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key](const QString& typed) {
        if (filling_) return;
        edit(QStringLiteral("%1=%2").arg(key, quoted(typed)));
    });
    return new FormRow(label, field, properties_);
}

/// THE SHEET'S OWN PROPERTIES, shown whenever no item is picked.
///
/// WHAT THIS REPLACES. The panel used to say "Bir öğe seçin." into three hundred
/// pixels of width and a thousand of height — a whole column of the window spent
/// telling the user to do something rather than letting them do anything. A
/// sheet always exists, always has a paper size, an orientation, a margin and an
/// export resolution, and NONE of those four were reachable from this window at
/// all: they are arguments of `ÇIKTIYERLEŞİMİ` that only a typed command line
/// could set. The empty state and the missing settings were the same hole.
void LayoutDesigner::buildSheetProperties(const core::Layout& l)
{
    const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l.pages.size()) - 1);
    const core::LayoutPage& page = l.pages[static_cast<std::size_t>(shown)];

    if (propertiesHead_ != nullptr) {
        propertiesHead_->setTitle(tr("SAYFA %1").arg(shown + 1));
        propertiesHead_->setNote(tr("%1 × %2 mm").arg(page.w / 1000).arg(page.h / 1000));
    }

    // WHICH PAPER. The command takes the six ISO sizes and `ozel`; a sheet that
    // came from a custom size shows it and keeps it until the user picks
    // another.
    auto* paper = new ComboBox(properties_);
    for (const char* one : {"A5", "A4", "A3", "A2", "A1", "A0", "ozel"})
        paper->addItem(QString::fromUtf8(one), QString::fromUtf8(one));
    const int at = paper->findData(QString::fromStdString(l.paper));
    paper->setCurrentIndex(at >= 0 ? at : paper->count() - 1);
    connect(paper, &QComboBox::currentIndexChanged, this, [this, paper](int chosen) {
        if (filling_ || chosen < 0) return;
        sheetEdit(QStringLiteral("kagit=%1").arg(paper->itemData(chosen).toString()));
    });

    auto* facing = new Segment(properties_);
    facing->setControlSize(ControlSize::Regular);
    facing->addOption(tr("Dikey"));
    facing->addOption(tr("Yatay"));
    // READ FROM THE PAGE, not from the layout's flag: on a multi-page sheet the
    // flag describes the sheet as a whole and a page may disagree with it.
    facing->setCurrent(page.w > page.h ? 1 : 0);
    connect(facing, &Segment::currentChanged, this, [this](int chosen) {
        if (filling_) return;
        sheetEdit(QStringLiteral("yon=%1").arg(chosen == 1 ? QStringLiteral("yatay")
                                                           : QStringLiteral("dikey")));
    });

    propertyColumn_->addWidget(pairOf(new FormRow(tr("Kâğıt"), paper, properties_),
                                      new FormRow(tr("Yön"), facing, properties_)));

    FieldSpec edgeSpec = number_of(0, 200);
    edgeSpec.suffix    = QStringLiteral("mm");
    auto* edge         = new Field(edgeSpec, properties_);
    edge->setFixedHeight(static_cast<int>(ControlSize::Regular));
    edge->setValue(QString::number(l.margin / 1000));
    edge->setAccessibleName(tr("Kenar boşluğu"));
    connect(edge, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        sheetEdit(QStringLiteral("kenar=%1").arg(typed.toInt()));
    });
    auto* edgeRow = new FormRow(tr("Kenar boşluğu"), edge, properties_);
    edgeRow->setHelp(tr("kılavuz · kırpma değil"));

    FieldSpec dpiSpec = number_of(72, 4800);
    dpiSpec.suffix    = QStringLiteral("dpi");
    auto* dpi         = new Field(dpiSpec, properties_);
    dpi->setFixedHeight(static_cast<int>(ControlSize::Regular));
    dpi->setValue(QString::number(l.dpi));
    dpi->setAccessibleName(tr("Çözünürlük"));
    connect(dpi, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        sheetEdit(QStringLiteral("dpi=%1").arg(typed.toInt()));
    });
    auto* dpiRow = new FormRow(tr("Çözünürlük"), dpi, properties_);
    dpiRow->setHelp(tr("dışa aktarma"));

    propertyColumn_->addWidget(pairOf(edgeRow, dpiRow));

    propertyColumn_->addWidget(new FormSection(tr("YERLEŞİM"), QString(), properties_));

    auto* named = new Field(field_of(FieldKind::Text), properties_);
    named->setFixedHeight(static_cast<int>(ControlSize::Regular));
    named->setValue(QString::fromStdString(l.name));
    named->setAccessibleName(tr("Yerleşim adı"));
    connect(named, &Field::committed, this, [this](const QString& typed) {
        if (filling_ || typed.trimmed().isEmpty() || typed == name_) return;
        controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=ad ad=%1 yeni_ad=%2")
                                .arg(quoted(name_), quoted(typed.trimmed())),
                            command::Origin::Gui);
        // THE WINDOW FOLLOWS THE RENAME. It holds the sheet BY NAME, so a
        // designer that kept the old one would be looking at a layout that no
        // longer exists and would report it as deleted on the next refresh.
        name_ = typed.trimmed();
        canvas_->setSheet(name_, canvas_->page());
        setWindowTitle(tr("Çıktı yerleşimi — %1").arg(name_));
        refresh();
    });
    propertyColumn_->addWidget(new FormRow(tr("Ad"), named, properties_));

    auto* hint = new QLabel(tr("Bir öğeye tıklayın; ayarları burada açılır."), properties_);
    hint->setObjectName(QStringLiteral("formHelp"));
    hint->setWordWrap(true);
    propertyColumn_->addWidget(hint);
}

/// EVERY SETTING THE COMMAND TAKES, for the item that is picked.
///
/// WHAT WAS MISSING. `ÇIKTIÖĞE islem=ayarla` accepts seventeen arguments and
/// this panel offered nine of them. A user could not rename an item, move it to
/// another page, change what covers what, tell a map frame which layers to draw
/// or how far apart its grid lines run, cap a table's rows, choose its columns,
/// or bind a scale bar to a second map frame — all of it reachable by typing a
/// command and none of it by the window built to do exactly that job. That is
/// what "çok kısır" was: not a style, an absence.
void LayoutDesigner::buildItemProperties(const core::Layout& l, const core::LayoutItem& item)
{
    using core::LayoutItemKind;

    if (propertiesHead_ != nullptr) {
        propertiesHead_->setTitle(
            QString::fromUtf8(core::layout_item_kind_label(item.kind)).toUpper());
        propertiesHead_->setNote(QString::fromStdString(item.id));
    }

    propertyColumn_->addWidget(new FormSection(tr("YERLEŞTİRME"), QString(), properties_));
    propertyColumn_->addWidget(
        pairOf(mmRow(tr("Sol (x)"), item.frame.x, "x"), mmRow(tr("Üst (y)"), item.frame.y, "y")));
    propertyColumn_->addWidget(pairOf(mmRow(tr("Genişlik"), item.frame.w, "genislik"),
                                      mmRow(tr("Yükseklik"), item.frame.h, "yukseklik")));

    // ---- which page it sits on, and what covers what ------------------------
    auto* onPage = new Field(number_of(1, static_cast<int>(l.pages.size())), properties_);
    onPage->setFixedHeight(static_cast<int>(ControlSize::Regular));
    onPage->setValue(QString::number(canvas_->page() + 1));
    onPage->setAccessibleName(tr("Sayfa"));
    connect(onPage, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        // A PAGE MOVE IS A MOVE, so it goes through `tasi` like every other one.
        edit(QStringLiteral("sayfa=%1").arg(typed.toInt()), QStringLiteral("tasi"));
    });

    auto* order    = new QWidget(properties_);
    auto* orderRow = new QHBoxLayout(order);
    orderRow->setContentsMargins(0, 0, 0, 0);
    orderRow->setSpacing(4);
    const auto shove = [&](Glyph glyph, const QString& tip, int to) {
        auto* button = new Button(ButtonRole::Icon, QString(), glyph, order);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        button->setControlSize(ControlSize::Regular);
        connect(button, &QPushButton::clicked, this,
                [this, to] { edit(QStringLiteral("sira=%1").arg(to)); });
        orderRow->addWidget(button);
    };
    // THE TOP AND THE BOTTOM OF THE STACK, computed from what is actually
    // there. A fixed ±1000 would work once and then pile every shoved item on
    // the same number, so a second shove would do nothing.
    std::int32_t top    = item.z;
    std::int32_t bottom = item.z;
    for (const core::LayoutItem& other : l.items) {
        top    = std::max(top, other.z);
        bottom = std::min(bottom, other.z);
    }
    shove(Glyph::ChevronUp, tr("Öne getir"), std::min(top + 1, 1000));
    shove(Glyph::ChevronDown, tr("Arkaya gönder"), std::max(bottom - 1, -1000));

    auto* z = new Field(number_of(-1000, 1000), order);
    z->setFixedHeight(static_cast<int>(ControlSize::Regular));
    z->setValue(QString::number(item.z));
    z->setAccessibleName(tr("Çizim sırası"));
    connect(z, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        edit(QStringLiteral("sira=%1").arg(typed.toInt()));
    });
    orderRow->addWidget(z, 1);

    propertyColumn_->addWidget(pairOf(new FormRow(tr("Sayfa"), onPage, properties_),
                                      new FormRow(tr("Sıra"), order, properties_)));

    // ---- what it is called --------------------------------------------------
    auto* named = new Field(field_of(FieldKind::Text), properties_);
    named->setFixedHeight(static_cast<int>(ControlSize::Regular));
    named->setValue(QString::fromStdString(item.id));
    named->setAccessibleName(tr("Öğe adı"));
    connect(named, &Field::committed, this, [this](const QString& typed) {
        if (filling_ || typed.trimmed().isEmpty()) return;
        const QString was = canvas_->selected();
        if (typed.trimmed() == was) return;
        controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=ad yerlesim=%1 ad=%2 yeni_ad=%3")
                                .arg(quoted(name_), quoted(was), quoted(typed.trimmed())),
                            command::Origin::Gui);
        canvas_->select(typed.trimmed());
        refresh();
    });
    auto* namedRow = new FormRow(tr("Ad"), named, properties_);
    namedRow->setHelp(tr("Komut satırının ad= ile andığı ad"));
    propertyColumn_->addWidget(namedRow);

    // ---- what only some kinds have ------------------------------------------
    //
    // NO HEADING HERE. The panel's own heading already says which kind this is,
    // and a second `HARİTA` four rows under the first is a label repeating what
    // the reader has not had time to forget.

    if (item.kind == LayoutItemKind::Label || item.kind == LayoutItemKind::Picture ||
        item.kind == LayoutItemKind::Table || item.kind == LayoutItemKind::Legend) {
        const QString hint = item.kind == LayoutItemKind::Picture ? tr("resim yolu")
                             : item.kind == LayoutItemKind::Table ? tr("katman adı")
                                                                  : tr("yazı");
        auto* row          = textRow(tr("Metin"), QString::fromStdString(item.text), "metin", hint);
        if (item.kind == LayoutItemKind::Label)
            row->setHelp(tr("<yerlesim>, <olcek>, <tarih>, <crs>, <proje>"));
        propertyColumn_->addWidget(row);
    }

    if (item.kind == LayoutItemKind::Map) {
        auto* scale = new Field(field_of(FieldKind::Number), properties_);
        scale->setFixedHeight(static_cast<int>(ControlSize::Regular));
        scale->setValue(QString::number(item.scale));
        scale->setAccessibleName(tr("Ölçek 1:N"));
        connect(scale, &Field::committed, this, [this](const QString& typed) {
            if (filling_) return;
            edit(QStringLiteral("olcek=%1").arg(typed.toLongLong()));
        });
        auto* scaleRow = new FormRow(tr("Ölçek 1:N"), scale, properties_);
        scaleRow->setHelp(tr("0 = pencereye uyar"));
        propertyColumn_->addWidget(scaleRow);

        auto* grid = new ComboBox(properties_);
        for (const auto& [word, label] :
             {std::pair{"yok", tr("yok")}, std::pair{"arti", tr("artı")},
              std::pair{"cizgi", tr("çizgi")}, std::pair{"centik", tr("çentik")}})
            grid->addItem(label, QString::fromUtf8(word));
        grid->setCurrentIndex(static_cast<int>(item.grid));
        connect(grid, &QComboBox::currentIndexChanged, this, [this, grid](int at) {
            if (filling_ || at < 0) return;
            edit(QStringLiteral("izgara=%1").arg(grid->itemData(at).toString()));
        });

        auto* spacing = countRow(tr("Aralık"), item.grid_interval, "izgara_aralik", 1000000000);
        spacing->setHelp(tr("zemin mm · 0 = otomatik"));
        propertyColumn_->addWidget(pairOf(new FormRow(tr("Izgara"), grid, properties_), spacing));

        // WHICH LAYERS IT DRAWS. Empty means every visible one, which is the
        // default and the common case; `hepsi` is how the command line clears a
        // list back to it, so the help says so rather than leaving a user to
        // guess that deleting the text does the same thing.
        auto* drawn = textRow(tr("Katmanlar"), joined(item.layers), "katmanlar",
                              tr("görünür bütün katmanlar"));
        drawn->setHelp(tr("virgülle ayırın · boş = görünür hepsi"));
        propertyColumn_->addWidget(drawn);
    }

    if (item.kind == LayoutItemKind::Table) {
        auto* columns = textRow(tr("Sütunlar"), joined(item.columns), "sutunlar",
                                tr("katmanın bütün sütunları"));
        columns->setHelp(tr("virgülle ayırın · boş = hepsi"));
        propertyColumn_->addWidget(columns);

        auto* cap = countRow(tr("Satır sınırı"), item.row_limit, "satir_siniri", 100000);
        cap->setHelp(tr("0 = kutuya kaç satır sığarsa"));
        propertyColumn_->addWidget(cap);
    }

    // WHICH MAP FRAME IT BELONGS TO. A scale bar, a north arrow and a legend all
    // describe one map; a sheet with two frames had no way to say which, so the
    // second frame's scale bar quietly described the first.
    if (item.kind == LayoutItemKind::ScaleBar || item.kind == LayoutItemKind::NorthArrow ||
        item.kind == LayoutItemKind::Legend || item.kind == LayoutItemKind::Chart) {
        auto* bound = new ComboBox(properties_);
        bound->addItem(tr("ilk harita"), QStringLiteral("ilk"));
        for (const core::LayoutItem& other : l.items)
            if (other.kind == LayoutItemKind::Map)
                bound->addItem(QString::fromStdString(other.id), QString::fromStdString(other.id));
        const int chosen = bound->findData(QString::fromStdString(item.linked_map));
        bound->setCurrentIndex(chosen >= 0 ? chosen : 0);
        connect(bound, &QComboBox::currentIndexChanged, this, [this, bound](int at) {
            if (filling_ || at < 0) return;
            edit(QStringLiteral("harita=%1").arg(quoted(bound->itemData(at).toString())));
        });
        propertyColumn_->addWidget(new FormRow(tr("Harita"), bound, properties_));
    }

    if (item.kind != LayoutItemKind::Map)
        propertyColumn_->addWidget(mmRow(tr("Yazı yüksekliği"), item.text_height, "yazi"));

    // ---- the two switches ---------------------------------------------------
    //
    // TWO ROWS, NOT ONE PILE. They were a `ToggleSwitch` and a bare `QLabel`
    // pushed into a horizontal strip, which made them the only two controls in
    // the window that did not read like the rest of the form — and made the
    // panel's own rebuild bug show up here first.
    const auto toggle = [&](const QString& label, bool on, const char* argument,
                            const QString& help) {
        auto* box = new ToggleSwitch(properties_);
        box->setChecked(on);
        box->setAccessibleName(label);
        const QString key = QString::fromUtf8(argument);
        connect(box, &QAbstractButton::toggled, this, [this, key](bool checked) {
            if (filling_) return;
            edit(QStringLiteral("%1=%2").arg(key, checked ? QStringLiteral("evet")
                                                          : QStringLiteral("hayir")));
        });
        auto* row = new FormRow(label, box, properties_);
        row->setHelp(help);
        return row;
    };
    propertyColumn_->addWidget(
        pairOf(toggle(tr("Çerçeve"), item.frame_visible, "cerceve", tr("kutuyu çizer")),
               toggle(tr("Kilit"), item.locked, "kilit", tr("taşımayı kapatır"))));
}

void LayoutDesigner::showItem(const QString& id)
{
    if (canvas_ == nullptr) return;
    canvas_->select(id);
    refresh();
}

void LayoutDesigner::aimAt(core::Box2 window)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    const core::LayoutItem* map = l->first_map();
    if (map == nullptr) {
        if (status_ != nullptr)
            status_->setText(tr("Bu yerleşimde harita çerçevesi yok; ekleyip yeniden deneyin."));
        return;
    }

    // THE NAME IS TAKEN BEFORE THE DOCUMENT MOVES, and that is not tidiness.
    //
    // `map` points INTO the document's layout. `runLine` below dispatches
    // `ÇIKTIÖĞE islem=ayarla`, which rewrites the layout's item vector — and the
    // pointer is then dangling. Reading `map->id` after it crashed the program
    // in `strlen` on a garbage address, intermittently, which is the worst shape
    // a use-after-free takes: it looked like a flaky test for weeks.
    const QString aimed = QString::fromStdString(map->id);

    // METRES ON THE LINE, and the key written twice — the parser's own shape for
    // a window (`YAZDIR pencere=`). Writing `Mm` here made the frame a thousand
    // times too wide the first time this was tried.
    const auto metres = [](core::Mm v) {
        return QString::number(static_cast<double>(v) / 1000.0, 'f', 3);
    };
    controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 "
                                       "pencere=%3,%4 pencere=%5,%6")
                            .arg(quoted(name_), quoted(aimed), metres(window.min_x),
                                 metres(window.min_y), metres(window.max_x), metres(window.max_y)),
                        command::Origin::Gui);
    canvas_->select(aimed);
    refresh();
}

void LayoutDesigner::exportSheet()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Yerleşimi PDF olarak kaydet"), name_ + QStringLiteral(".pdf"), tr("PDF (*.pdf)"));
    if (path.isEmpty()) return;

    controller_.runLine(
        QStringLiteral("YAZDIR yerlesim=%1 dosya=%2").arg(quoted(name_), quoted(path)),
        command::Origin::Gui);
}

void LayoutDesigner::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
    if (canvas_ != nullptr) canvas_->applyTheme(mode);

    // AND THE ITEM ROWS. A delegate is not a widget, so `DialogFrame` never
    // reaches it: it kept the dark tokens it was built with and drew the names
    // in #DFE5EA on a light panel, which is a list that looks disabled.
    // A STATIC CAST, and it is sound: `rows_` is only ever assigned the
    // `ItemRow` built in `buildItemList`. `qobject_cast` cannot help here —
    // `ItemRow` lives in this file's anonymous namespace and has no meta-object.
    if (rows_ != nullptr) static_cast<ItemRow*>(rows_)->setTheme(mode);
    if (items_ != nullptr) items_->viewport()->update();
}

QStringList LayoutDesigner::probeDrive()
{
    QStringList said;
    const core::Layout* l = layout();
    if (l == nullptr) return {QStringLiteral("yerleşim yok")};
    (void)l;

    canvas_->select(QStringLiteral("baslik"));
    said << QStringLiteral("seçim: %1").arg(canvas_->selected());

    // A DRAG, AS THE CANVAS WOULD REPORT IT. The signal is the seam the mouse
    // uses, so driving it drives the real path rather than a shortcut.
    if (const core::LayoutItem* title = l->find("baslik"); title != nullptr) {
        // THE VALUE IS COPIED BEFORE THE COMMAND RUNS. `set_layouts` replaces the
        // whole list, so `title` dangles the moment the move is applied — reading
        // it afterwards reported the start as 0.0 mm, which is what a freed
        // `std::string`'s neighbour happened to hold.
        const core::PaperRect was = title->frame;
        core::PaperRect moved     = was;
        moved.x += core::um_from_mm(25);
        moved.y += core::um_from_mm(5);
        emit canvas_->itemMoved(QStringLiteral("baslik"), moved);
        const core::LayoutItem* after = layout()->find("baslik");
        said << QStringLiteral("taşıma: x %1 → %2 mm")
                    .arg(mm_text(was.x), mm_text(after != nullptr ? after->frame.x : was.x));
    }
    // `l` is stale from here on: every `edit()` below rewrites the list.
    l = nullptr;

    edit(QStringLiteral("metin=%1").arg(quoted(QStringLiteral("<yerlesim> — <olcek>"))));
    if (const core::LayoutItem* after = layout()->find("baslik"); after != nullptr)
        said << QStringLiteral("metin: %1").arg(QString::fromStdString(after->text));

    canvas_->select(QStringLiteral("harita"));
    edit(QStringLiteral("izgara=cizgi olcek=500"));
    if (const core::LayoutItem* map = layout()->find("harita"); map != nullptr)
        said << QStringLiteral("harita: ölçek 1:%1, ızgara %2")
                    .arg(map->scale)
                    .arg(static_cast<int>(map->grid));

    // ---- THE SHEET'S OWN SETTINGS, THROUGH THE PANEL'S OWN PATH -------------
    //
    // WHAT THIS GUARDS. `islem=sayfa` DEFAULTS every argument it is not given,
    // so a line carrying only `kenar=15` also makes the sheet A4 and turns it
    // upright. Four inspector fields that each undid the other three would have
    // shipped as "changing the margin resized my paper". One field is touched
    // here and the other three are checked for having stayed put.
    sheetEdit(QStringLiteral("kenar=15"));
    if (const core::Layout* sheet = layout(); sheet != nullptr)
        said << QStringLiteral("sayfa: %1 %2 · kenar %3 mm · %4 dpi · %5×%6")
                    .arg(QString::fromStdString(sheet->paper),
                         sheet->landscape ? QStringLiteral("yatay") : QStringLiteral("dikey"))
                    .arg(sheet->margin / 1000)
                    .arg(sheet->dpi)
                    .arg(sheet->pages.front().w / 1000)
                    .arg(sheet->pages.front().h / 1000);

    // AND THE RESOLUTION, which until now no client could change after the
    // layout was made.
    //
    // BOTH BEFORE THE LEGEND IS ADDED, deliberately: the caller's next step is
    // a `GERİAL` that must undo the LAST gesture this makes, and it checks the
    // item count. A settings change landing after it would be what came back.
    sheetEdit(QStringLiteral("dpi=600"));
    if (const core::Layout* sheet = layout(); sheet != nullptr)
        said << QStringLiteral("sayfa: %1 %2 · kenar %3 mm · %4 dpi")
                    .arg(QString::fromStdString(sheet->paper),
                         sheet->landscape ? QStringLiteral("yatay") : QStringLiteral("dikey"))
                    .arg(sheet->margin / 1000)
                    .arg(sheet->dpi);
    addItem(QStringLiteral("lejant"));
    said << QStringLiteral("öğe sayısı: %1").arg(layout()->items.size());
    return said;
}

} // namespace kentos::app
