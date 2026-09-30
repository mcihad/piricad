// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_designer.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/layout_render.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/layout_table.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QDate>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWheelEvent>

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
/// keyhole.
constexpr int kRulerPx = 24;

/// How close, in device pixels, an edge has to come to a guide to snap to it:
/// near enough to catch without aiming, far enough to let a box sit 2 mm off an
/// edge on purpose at a normal zoom.
constexpr double kSnapPx = 6.0;

/// A drag that is not caught by a guide still lands on a whole millimetre. A
/// title block at 20.0 mm is a title block somebody can describe; one at
/// 19.83 mm is an accident.
constexpr core::Um kWhole = 1000;

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

core::Um whole(core::Um value)
{
    return static_cast<core::Um>(std::lround(static_cast<double>(value) / kWhole) * kWhole);
}

/// Paper micrometres as the millimetres a command line and a field carry.
QString mm_text(core::Um um, int decimals = 1)
{
    return QString::number(static_cast<double>(um) / 1000.0, 'f', decimals);
}

/// The same in the Turkish way a reader sees it, with the decimal comma.
QString mm_shown(core::Um um)
{
    return mm_text(um).replace(QLatin1Char('.'), QLatin1Char(','));
}

/// The nine item kinds, in the order the strip offers them.
///
/// ONE TABLE, THREE READERS: the strip draws a tool per row, the item list
/// draws a row's glyph beside each placed item, and a click with a tool uses the
/// row's size — so a kind cannot gain an icon in one place and keep the wrong
/// one in another.
struct Kind
{
    const char* word;           ///< what `tur=` takes
    core::LayoutItemKind which; ///< what the model calls it
    const char* label;          ///< what the tool says
    Glyph glyph;                ///< what both the tool and the list draw
    double w_mm;                ///< how wide a click with the tool makes it
    double h_mm;                ///< and how tall
};

constexpr Kind kKinds[] = {
    {"harita", core::LayoutItemKind::Map, "Harita", Glyph::LayoutMap, 160.0, 110.0},
    {"metin", core::LayoutItemKind::Label, "Metin", Glyph::LayoutLabel, 70.0, 12.0},
    {"lejant", core::LayoutItemKind::Legend, "Lejant", Glyph::LayoutLegend, 55.0, 45.0},
    {"olcek", core::LayoutItemKind::ScaleBar, "Ölçek", Glyph::LayoutScaleBar, 70.0, 10.0},
    {"kuzey", core::LayoutItemKind::NorthArrow, "Kuzey", Glyph::LayoutNorth, 16.0, 20.0},
    {"resim", core::LayoutItemKind::Picture, "Resim", Glyph::LayoutPicture, 30.0, 30.0},
    {"sekil", core::LayoutItemKind::Shape, "Şekil", Glyph::LayoutShape, 40.0, 25.0},
    {"tablo", core::LayoutItemKind::Table, "Tablo", Glyph::LayoutTable, 90.0, 40.0},
    {"grafik", core::LayoutItemKind::Chart, "Grafik", Glyph::LayoutChart, 70.0, 45.0},
};

const Kind* kind_of(core::LayoutItemKind kind)
{
    for (const Kind& one : kKinds)
        if (one.which == kind) return &one;
    return nullptr;
}

const Kind* kind_named(const QString& word)
{
    for (const Kind& one : kKinds)
        if (word == QString::fromUtf8(one.word)) return &one;
    return nullptr;
}

Glyph glyph_of(core::LayoutItemKind kind)
{
    const Kind* one = kind_of(kind);
    return one != nullptr ? one->glyph : Glyph::LayoutShape;
}

/// The round plan scales a pafta is drawn at, for the scale menu.
constexpr long long kScales[] = {500, 1000, 2000, 2500, 5000, 10000, 25000, 50000};

/// What a placed item is called on screen.
///
/// A LABEL IS CALLED BY WHAT IT SAYS. `baslik` is a key; "Ada 1284 / Pafta 3" is
/// the thing on the paper. Everything else has no text of its own and is called
/// by its kind, which is what a user would point at it and say.
QString item_name(const core::LayoutItem& item)
{
    if (item.kind == core::LayoutItemKind::Label) {
        const QString written = QString::fromStdString(item.text).simplified();
        if (!written.isEmpty() && written.size() <= 40) return written;
    }
    return QString::fromUtf8(core::layout_item_kind_label(item.kind));
}

QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

/// A colour as the command line takes it and the journal keeps it.
QString colour_word(std::uint32_t argb)
{
    if ((argb >> 24) == 0xFFu)
        return QStringLiteral("#%1").arg(argb & 0xFFFFFFu, 6, 16, QLatin1Char('0')).toUpper();
    return QStringLiteral("#%1").arg(argb, 8, 16, QLatin1Char('0')).toUpper();
}

/// The coarsest whole step whose marks stay at least `apart` pixels from each
/// other, at `per_mm` device pixels to the paper millimetre.
///
/// ONE LADDER FOR THE RULER'S TICKS AND ITS NUMBERS, so a tick is where a number
/// would be. Two separate answers to "how far apart" would drift the moment one
/// was tuned.
core::Um ruler_step(double per_mm, double apart)
{
    static constexpr core::Um kLadder[] = {1000, 5000, 10000, 25000, 50000, 100000, 250000};
    for (const core::Um candidate : kLadder)
        if (per_mm * (static_cast<double>(candidate) / 1000.0) >= apart) return candidate;
    return kLadder[std::size(kLadder) - 1];
}

/// The box around several.
core::PaperRect union_of(const std::vector<ItemFrame>& frames)
{
    if (frames.empty()) return {};
    core::Um left   = frames.front().second.x;
    core::Um top    = frames.front().second.y;
    core::Um right  = frames.front().second.right();
    core::Um bottom = frames.front().second.bottom();
    for (const auto& [id, f] : frames) {
        left   = std::min(left, f.x);
        top    = std::min(top, f.y);
        right  = std::max(right, f.right());
        bottom = std::max(bottom, f.bottom());
    }
    return core::PaperRect{left, top, right - left, bottom - top};
}

/// Draws one item row: the kind's glyph, the item's name, its size in
/// millimetres, and a padlock at the far end that locks it with a click.
///
/// WHY NOT A PLAIN STRING. What a user looks for is "the title" and "the map",
/// and what they check next is how big it is and whether it will move; the id is
/// a detail they need only when they write a command line, so it moves to the
/// tooltip. The padlock is a CONTROL, not a mark: it is where QGIS puts it and
/// it is where a hand goes to pin a title block.
class ItemRow : public QStyledItemDelegate
{
public:
    explicit ItemRow(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setTheme(ThemeMode mode) { theme_ = mode; }

    /// Called with the row's id when its padlock is clicked.
    std::function<void(const QString&)> onLock;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return {QStyledItemDelegate::sizeHint(option, index).width(), 30};
    }

    static QRect lockRect(const QRect& row)
    {
        return QRect(row.right() - 26, row.top() + 7, 16, 16);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease && onLock) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (lockRect(option.rect).adjusted(-4, -4, 4, 4).contains(mouse->pos())) {
                onLock(index.data(Qt::UserRole).toString());
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        const bool on   = (option.state & QStyle::State_Selected) != 0;
        const bool over = (option.state & QStyle::State_MouseOver) != 0;
        p->save();
        if (on) {
            p->fillRect(option.rect, t.accentWash);
            p->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                        t.accent);
        } else if (over) {
            p->fillRect(option.rect, t.hoverRow);
        }

        const qreal dpr  = p->device()->devicePixelRatioF();
        QRect box        = option.rect.adjusted(10, 0, -8, 0);
        const auto glyph = static_cast<Glyph>(index.data(Qt::UserRole + 1).toInt());
        p->drawPixmap(QRect(box.left(), box.top() + 7, 16, 16),
                      glyph_pixmap(glyph, on ? t.accent : t.textDim, 16, dpr));
        box.setLeft(box.left() + 26);

        // THE PADLOCK, SHUT OR OPEN. Shut is drawn at full ink so a locked item
        // reads as one from across the panel; open is drawn only under the
        // pointer, so the column is not a fence of padlocks.
        const bool locked = index.data(Qt::UserRole + 3).toBool();
        if (locked || over)
            p->drawPixmap(lockRect(option.rect),
                          glyph_pixmap(locked ? Glyph::Lock : Glyph::Unlock,
                                       locked ? t.warn : t.textFaint, 16, dpr));
        box.setRight(option.rect.right() - 34);

        // THE SIZE IS RESERVED FIRST, so a long name is elided rather than
        // pushing the measurement off the panel.
        const QString size = index.data(Qt::UserRole + 2).toString();
        QFont mono         = p->font();
        mono.setFamily(QStringLiteral("IBM Plex Mono"));
        mono.setPointSizeF(std::max(7.5, mono.pointSizeF() - 1.5));
        const QFontMetrics mm(mono);
        const int wide = mm.horizontalAdvance(size) + 8;
        p->setFont(mono);
        p->setPen(t.textFaint);
        p->drawText(QRect(box.right() - wide, box.top(), wide, box.height()),
                    Qt::AlignRight | Qt::AlignVCenter, size);
        box.setRight(box.right() - wide);

        p->setFont(option.font);
        p->setPen(t.text);
        p->drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
                    option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                  Qt::ElideRight, box.width()));
        p->restore();
    }

private:
    ThemeMode theme_{ThemeMode::Dark};
};

/// The hairline between two groups of a strip — the ribbon's own separator:
/// upright in the tool row, lying down in the rail.
QWidget* strip_rule(QWidget* parent, bool upright)
{
    auto* rule = new QWidget(parent);
    rule->setObjectName(QStringLiteral("layoutStripRule"));
    rule->setAttribute(Qt::WA_StyledBackground, true);
    if (upright)
        rule->setFixedSize(1, 20);
    else
        rule->setFixedSize(20, 1);
    return rule;
}

/// A bare 32 px icon button for the strips (`Button::setBare`), its name in
/// the tooltip and in what a screen reader says.
Button* strip_button(Glyph glyph, const QString& tip, QWidget* parent)
{
    auto* b = new Button(glyph, tip, parent);
    b->setBare(true);
    return b;
}

/// The caption column of the inspector's rows — design.md §8's `110px | 1fr`.
constexpr int kCaption = 110;

/// The inspector's width: §8's symbol-editor column, which this is a sibling
/// of — a property list read down its values beside the thing being edited.
constexpr int kInspector = 352;

/// How many item rows the list shows before it scrolls.
constexpr int kListRows = 7;

} // namespace

// ========================================================== LayoutCanvas =====

LayoutCanvas::LayoutCanvas(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("layoutCanvas"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(420, 320);
    setAccessibleName(tr("Çıktı yerleşiminin kâğıdı"));
    setAccessibleDescription(
        tr("Öğeleri seçin, sürükleyin, köşelerinden boyutlandırın; ok tuşları kaydırır, "
           "tekerlek yakınlaştırır"));
}

void LayoutCanvas::setSheet(const QString& layout, int page)
{
    if (sheet_ != layout || page_ != page) {
        selection_.clear();
        primary_.clear();
    }
    sheet_     = layout;
    page_      = page;
    cachePage_ = -1;
    update();
}

const core::Layout* LayoutCanvas::layout() const
{
    return controller_.document().layouts().find(sheet_.toStdString());
}

void LayoutCanvas::select(const QString& id)
{
    setSelection(id.isEmpty() ? QStringList{} : QStringList{id});
}

void LayoutCanvas::setSelection(const QStringList& ids)
{
    const QString was = primary_;
    selection_        = ids;
    primary_          = ids.isEmpty() ? QString() : ids.back();
    update();
    if (primary_ != was || ids.size() != 1) emit selectionChanged(primary_);
}

void LayoutCanvas::refresh()
{
    // THE PICK IS CHECKED AGAINST THE DOCUMENT, not kept on faith: an item may
    // have been deleted by a command typed on the command line while this window
    // was open, and a pick naming nothing would draw handles around empty paper.
    const core::Layout* l = layout();
    QStringList kept;
    if (l != nullptr)
        for (const QString& id : std::as_const(selection_))
            if (l->find(id.toStdString()) != nullptr) kept << id;
    selection_ = kept;
    primary_   = kept.isEmpty() ? QString() : kept.back();
    cachePage_ = -1; ///< the document moved: draw the sheet again
    update();
}

double LayoutCanvas::pixelsPerMm() const
{
    const QRectF box             = pageRect();
    const core::LayoutPage* page = activePage();
    if (page == nullptr || page->w <= 0) return 0.0;
    return box.width() / (static_cast<double>(page->w) / 1000.0);
}

QRectF LayoutCanvas::pageRect() const
{
    const core::LayoutPage* page = activePage();
    if (page == nullptr) return {};

    if (fit_) {
        // FITTED AND CENTRED INSIDE THE RULERS, with air around it — which is
        // what makes the sheet read as a sheet on a table rather than as the
        // window's background.
        const double air     = 24.0;
        const double avail_w = std::max(1.0, width() - kRulerPx - 2 * air);
        const double avail_h = std::max(1.0, height() - kRulerPx - 2 * air);
        const double scale   = std::min(avail_w / static_cast<double>(page->w),
                                        avail_h / static_cast<double>(page->h));
        const double w       = static_cast<double>(page->w) * scale;
        const double h       = static_cast<double>(page->h) * scale;
        return QRectF(kRulerPx + air + (avail_w - w) / 2.0, kRulerPx + air + (avail_h - h) / 2.0, w,
                      h);
    }
    return QRectF(corner_.x(), corner_.y(), static_cast<double>(page->w) * scale_,
                  static_cast<double>(page->h) * scale_);
}

QRect LayoutCanvas::sheetRect() const
{
    return pageRect().toAlignedRect().intersected(rect());
}

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
    const QRectF box             = pageRect();
    const core::LayoutPage* page = activePage();
    if (page == nullptr || box.isEmpty()) return {};
    const double sx = box.width() / static_cast<double>(page->w);
    const double sy = box.height() / static_cast<double>(page->h);
    return QRectF(box.left() + paper.x * sx, box.top() + paper.y * sy, paper.w * sx, paper.h * sy);
}

QPointF LayoutCanvas::paperAt(QPointF widget) const
{
    const QRectF box             = pageRect();
    const core::LayoutPage* page = activePage();
    if (page == nullptr || box.isEmpty()) return {};
    return QPointF((widget.x() - box.left()) * static_cast<double>(page->w) / box.width(),
                   (widget.y() - box.top()) * static_cast<double>(page->h) / box.height());
}

void LayoutCanvas::zoomBy(double factor, QPointF about)
{
    const core::LayoutPage* page = activePage();
    if (page == nullptr) return;
    const QRectF box    = pageRect();
    const double before = box.width() / static_cast<double>(page->w);
    // A TENTH OF REAL SIZE TO FORTY TIMES IT: from an A0 sheet whole on a laptop
    // to a hairline at the width of a finger.
    const double real  = (logicalDpiX() > 0 ? logicalDpiX() : 96.0) / 25400.0;
    const double after = std::clamp(before * factor, real * 0.05, real * 40.0);
    const QPointF paper((about.x() - box.left()) / before, (about.y() - box.top()) / before);
    scale_     = after;
    corner_    = QPointF(about.x() - paper.x() * after, about.y() - paper.y() * after);
    fit_       = false;
    cachePage_ = -1;
    update();
    emit zoomChanged(zoomPercent());
}

void LayoutCanvas::zoomBy(double factor)
{
    zoomBy(factor, QPointF(width() / 2.0, height() / 2.0));
}

void LayoutCanvas::zoomToFit()
{
    fit_       = true;
    cachePage_ = -1;
    update();
    emit zoomChanged(zoomPercent());
}

void LayoutCanvas::zoomToRealSize()
{
    const core::LayoutPage* page = activePage();
    if (page == nullptr) return;
    scale_         = (logicalDpiX() > 0 ? logicalDpiX() : 96.0) / 25400.0;
    const double w = static_cast<double>(page->w) * scale_;
    const double h = static_cast<double>(page->h) * scale_;
    corner_        = QPointF(kRulerPx + std::max(24.0, (width() - kRulerPx - w) / 2.0),
                             kRulerPx + std::max(24.0, (height() - kRulerPx - h) / 2.0));
    fit_           = false;
    cachePage_     = -1;
    update();
    emit zoomChanged(zoomPercent());
}

double LayoutCanvas::zoomPercent() const
{
    const double real = (logicalDpiX() > 0 ? logicalDpiX() : 96.0) / 25.4;
    return real > 0.0 ? 100.0 * pixelsPerMm() / real : 100.0;
}

void LayoutCanvas::setDrawKind(const QString& kind)
{
    drawKind_ = kind;
    setCursor(kind.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void LayoutCanvas::setSnapping(bool on)
{
    snapping_ = on;
}

const core::LayoutItem* LayoutCanvas::itemAt(QPoint at) const
{
    const core::Layout* l = layout();
    if (l == nullptr) return nullptr;
    // THE TOPMOST, by paint order: what is in front is what the eye means.
    const core::LayoutItem* hit = nullptr;
    for (std::size_t i = 0; i < l->items.size(); ++i) {
        if (l->page_of(i) != page_) continue;
        const core::LayoutItem& item = l->items[i];
        if (!deviceFrom(item.frame).adjusted(-2, -2, 2, 2).contains(at)) continue;
        if (hit == nullptr || item.z >= hit->z) hit = &item;
    }
    return hit;
}

LayoutCanvas::Grip LayoutCanvas::gripAt(const QPoint& at) const
{
    const core::Layout* l = layout();
    if (l == nullptr || selection_.size() != 1) return Grip::None;
    const core::LayoutItem* item = l->find(primary_.toStdString());
    if (item == nullptr || item->locked) return Grip::None;

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

void LayoutCanvas::snapTargets(std::vector<core::Um>& xs, std::vector<core::Um>& ys) const
{
    const core::Layout* l        = layout();
    const core::LayoutPage* page = activePage();
    if (l == nullptr || page == nullptr) return;

    // THE PAGE: its edges, its middle, and the margin the sheet is drawn to.
    xs = {0, page->w / 2, page->w};
    ys = {0, page->h / 2, page->h};
    if (l->margin > 0) {
        xs.push_back(l->margin);
        xs.push_back(page->w - l->margin);
        ys.push_back(l->margin);
        ys.push_back(page->h - l->margin);
    }
    // AND EVERY ITEM THAT IS NOT MOVING: its edges and its centre — the lines a
    // title block is lined up with and a legend is hung from.
    for (std::size_t i = 0; i < l->items.size(); ++i) {
        if (l->page_of(i) != page_) continue;
        const core::LayoutItem& item = l->items[i];
        const QString id             = QString::fromStdString(item.id);
        if (std::any_of(starts_.begin(), starts_.end(),
                        [&id](const ItemFrame& moving) { return moving.first == id; }))
            continue;
        xs.insert(xs.end(), {item.frame.x, item.frame.x + item.frame.w / 2, item.frame.right()});
        ys.insert(ys.end(), {item.frame.y, item.frame.y + item.frame.h / 2, item.frame.bottom()});
    }
}

core::Um LayoutCanvas::snapAxis(std::initializer_list<core::Um> edges,
                                const std::vector<core::Um>& targets, bool vertical)
{
    const double per_um = pixelsPerMm() / 1000.0;
    if (per_um <= 0.0) return 0;
    const auto reach = static_cast<core::Um>(kSnapPx / per_um);
    core::Um best    = reach + 1;
    core::Um move    = 0;
    core::Um at      = 0;
    for (const core::Um edge : edges)
        for (const core::Um target : targets) {
            const core::Um d = target - edge;
            if (std::abs(d) < std::abs(best)) {
                best = d;
                move = d;
                at   = target;
            }
        }
    if (std::abs(best) > reach) return 0;
    guides_.push_back(SnapLine{vertical, at});
    return move;
}

void LayoutCanvas::dragTo(const QPoint& at, Qt::KeyboardModifiers modifiers)
{
    const core::LayoutPage* page = activePage();
    const double per_um          = pixelsPerMm() / 1000.0;
    if (page == nullptr || per_um <= 0.0) return;
    guides_.clear();

    auto dx = static_cast<core::Um>((at.x() - press_.x()) / per_um);
    auto dy = static_cast<core::Um>((at.y() - press_.y()) / per_um);

    std::vector<core::Um> xs;
    std::vector<core::Um> ys;
    if (snapping_) snapTargets(xs, ys);

    if (gesture_ == Gesture::Move) {
        // SHIFT KEEPS THE MOVE ON ONE AXIS — the one it is mostly along.
        if ((modifiers & Qt::ShiftModifier) != 0) {
            if (std::abs(dx) > std::abs(dy))
                dy = 0;
            else
                dx = 0;
        }
        const core::PaperRect group = union_of(starts_);
        core::Um snapped_x          = 0;
        core::Um snapped_y          = 0;
        if (snapping_) {
            const core::Um left = group.x + dx;
            const core::Um top  = group.y + dy;
            snapped_x           = snapAxis({left, left + group.w / 2, left + group.w}, xs, true);
            snapped_y           = snapAxis({top, top + group.h / 2, top + group.h}, ys, false);
        }
        // CAUGHT BY A GUIDE, it goes exactly there; free, it lands on a whole
        // millimetre measured from where it started.
        dx    = snapped_x != 0 || !guides_.empty() ? dx + snapped_x : whole(dx);
        dy    = snapped_y != 0 || !guides_.empty() ? dy + snapped_y : whole(dy);
        live_ = starts_;
        for (auto& [id, frame] : live_) {
            frame.x += dx;
            frame.y += dy;
        }
        return;
    }

    if (gesture_ == Gesture::Resize && !starts_.empty()) {
        const core::PaperRect start = starts_.front().second;
        core::PaperRect out         = start;
        const bool west =
            grip_ == Grip::Left || grip_ == Grip::TopLeft || grip_ == Grip::BottomLeft;
        const bool east =
            grip_ == Grip::Right || grip_ == Grip::TopRight || grip_ == Grip::BottomRight;
        const bool north = grip_ == Grip::Top || grip_ == Grip::TopLeft || grip_ == Grip::TopRight;
        const bool south =
            grip_ == Grip::Bottom || grip_ == Grip::BottomLeft || grip_ == Grip::BottomRight;
        if (west) {
            core::Um edge = start.x + dx;
            edge += snapping_ ? snapAxis({edge}, xs, true) : 0;
            edge  = guides_.empty() ? whole(edge) : edge;
            out.x = std::min(edge, start.right() - kWhole);
            out.w = start.right() - out.x;
        }
        if (east) {
            core::Um edge = start.right() + dx;
            edge += snapping_ ? snapAxis({edge}, xs, true) : 0;
            out.w = std::max<core::Um>(kWhole, (guides_.empty() ? whole(edge) : edge) - start.x);
        }
        const std::size_t vertical_guides = guides_.size();
        if (north) {
            core::Um edge = start.y + dy;
            edge += snapping_ ? snapAxis({edge}, ys, false) : 0;
            edge  = guides_.size() == vertical_guides ? whole(edge) : edge;
            out.y = std::min(edge, start.bottom() - kWhole);
            out.h = start.bottom() - out.y;
        }
        if (south) {
            core::Um edge = start.bottom() + dy;
            edge += snapping_ ? snapAxis({edge}, ys, false) : 0;
            out.h = std::max<core::Um>(
                kWhole, (guides_.size() == vertical_guides ? whole(edge) : edge) - start.y);
        }
        // SHIFT KEEPS THE PROPORTION from a corner — a logo does not squash.
        if ((modifiers & Qt::ShiftModifier) != 0 && (west || east) && (north || south) &&
            start.w > 0 && start.h > 0) {
            const double ratio = static_cast<double>(start.h) / static_cast<double>(start.w);
            const auto h       = static_cast<core::Um>(std::lround(out.w * ratio));
            if (north) out.y = start.bottom() - h;
            out.h = h;
        }
        live_ = {ItemFrame{starts_.front().first, out}};
        return;
    }

    if (gesture_ == Gesture::Draw) {
        const QPointF here = paperAt(at);
        core::Um x         = static_cast<core::Um>(here.x());
        core::Um y         = static_cast<core::Um>(here.y());
        if (snapping_) {
            x += snapAxis({x}, xs, true);
            y += snapAxis({y}, ys, false);
        }
        x          = whole(x);
        y          = whole(y);
        core::Um w = std::abs(x - drawn_.x);
        core::Um h = std::abs(y - drawn_.y);
        if ((modifiers & Qt::ShiftModifier) != 0) w = h = std::max(w, h);
        live_ = {
            ItemFrame{drawKind_, core::PaperRect{x < drawn_.x ? drawn_.x - w : drawn_.x,
                                                 y < drawn_.y ? drawn_.y - h : drawn_.y, w, h}}};
    }
}

void LayoutCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    const core::Layout* l = layout();
    if (l == nullptr) return;
    const QPoint at = event->pos();
    press_          = at;
    guides_.clear();

    // A HAND ON THE PAPER: the middle button, or Space held, pans.
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && spaceHeld_)) {
        if (fit_) {
            const QRectF box = pageRect();
            scale_           = box.width() / static_cast<double>(activePage()->w);
            corner_          = box.topLeft();
            fit_             = false;
        }
        gesture_ = Gesture::Pan;
        panFrom_ = corner_;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton) return;

    // A TOOL DRAWS A BOX, from where it was pressed.
    if (!drawKind_.isEmpty()) {
        const QPointF paper = paperAt(at);
        drawn_              = core::PaperRect{whole(static_cast<core::Um>(paper.x())),
                                 whole(static_cast<core::Um>(paper.y())), 0, 0};
        live_               = {ItemFrame{drawKind_, drawn_}};
        gesture_            = Gesture::Draw;
        return;
    }

    // THE PICKED ITEM'S HANDLES WIN over anything under them: a handle sitting
    // on top of another item is still this item's handle, which is what lets a
    // small box be resized while it overlaps a big one.
    if (const Grip grip = gripAt(at); grip != Grip::None && grip != Grip::Body) {
        const core::LayoutItem* item = l->find(primary_.toStdString());
        if (item != nullptr) {
            grip_    = grip;
            gesture_ = Gesture::Resize;
            starts_  = {ItemFrame{primary_, item->frame}};
            live_    = starts_;
            return;
        }
    }

    const core::LayoutItem* hit = itemAt(at);
    const bool adding           = (event->modifiers() & Qt::ShiftModifier) != 0;
    if (hit == nullptr) {
        // EMPTY PAPER STARTS A MARQUEE; Shift keeps what was picked.
        if (!adding) setSelection({});
        gesture_ = Gesture::Marquee;
        marquee_ = QRectF(at, at);
        return;
    }

    const QString id = QString::fromStdString(hit->id);
    if (adding) {
        // SHIFT TOGGLES ONE ITEM IN OR OUT, and moves nothing.
        QStringList next = selection_;
        if (next.contains(id))
            next.removeAll(id);
        else
            next << id;
        setSelection(next);
        return;
    }
    if (!selection_.contains(id)) setSelection({id});

    // THE WHOLE PICK MOVES, less what is locked.
    starts_.clear();
    for (const QString& one : std::as_const(selection_))
        if (const core::LayoutItem* item = l->find(one.toStdString());
            item != nullptr && !item->locked)
            starts_.push_back(ItemFrame{one, item->frame});
    if (!starts_.empty()) {
        gesture_ = Gesture::Move;
        live_    = starts_;
    }
}

void LayoutCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint at     = event->pos();
    const QPointF paper = paperAt(at);
    if (const core::LayoutPage* page = activePage(); page != nullptr)
        emit cursorAt(paper.x() / 1000.0, paper.y() / 1000.0,
                      paper.x() >= 0 && paper.y() >= 0 && paper.x() <= page->w &&
                          paper.y() <= page->h);

    switch (gesture_) {
    case Gesture::Pan:
        corner_    = panFrom_ + (at - press_);
        cachePage_ = -1;
        update();
        return;
    case Gesture::Marquee:
        marquee_ = QRectF(press_, at).normalized();
        update();
        return;
    case Gesture::Move:
    case Gesture::Resize:
    case Gesture::Draw:
        dragTo(at, event->modifiers());
        update();
        return;
    case Gesture::None: break;
    }

    // NOT PRESSED: say what a press here would do.
    if (!drawKind_.isEmpty()) {
        setCursor(Qt::CrossCursor);
        return;
    }
    if (spaceHeld_) {
        setCursor(Qt::OpenHandCursor);
        return;
    }
    const Grip grip              = gripAt(at);
    const core::LayoutItem* over = itemAt(at);
    setCursor(grip != Grip::None
                  ? cursorFor(grip)
                  : (over != nullptr && !over->locked ? Qt::SizeAllCursor : Qt::ArrowCursor));
    const QString hovered = over != nullptr ? QString::fromStdString(over->id) : QString();
    if (hovered != hover_) {
        hover_ = hovered;
        update();
    }
}

void LayoutCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    const Gesture was = gesture_;
    gesture_          = Gesture::None;
    grip_             = Grip::None;
    guides_.clear();
    if (was == Gesture::None) return;

    if (was == Gesture::Pan) {
        setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if (was == Gesture::Marquee) {
        // EVERY ITEM THE MARQUEE TOUCHES, the way a frame picks on the map.
        const core::Layout* l = layout();
        QStringList next =
            (event->modifiers() & Qt::ShiftModifier) != 0 ? selection_ : QStringList{};
        if (l != nullptr && marquee_.width() > 2 && marquee_.height() > 2)
            for (std::size_t i = 0; i < l->items.size(); ++i) {
                if (l->page_of(i) != page_) continue;
                const QString id = QString::fromStdString(l->items[i].id);
                if (deviceFrom(l->items[i].frame).intersects(marquee_) && !next.contains(id))
                    next << id;
            }
        marquee_ = {};
        setSelection(next);
        return;
    }
    if (was == Gesture::Draw) {
        const core::PaperRect box = live_.empty() ? core::PaperRect{} : live_.front().second;
        const QString kind        = drawKind_;
        live_.clear();
        setDrawKind(QString());
        // A CLICK, NOT A DRAG, is a request for the kind's own size about the
        // click — what `emit itemDrawn` with an empty frame says.
        if (box.w < 2 * kWhole || box.h < 2 * kWhole)
            emit itemDrawn(kind, core::PaperRect{drawn_.x, drawn_.y, 0, 0});
        else
            emit itemDrawn(kind, box);
        emit drawFinished();
        update();
        return;
    }

    // A MOVE OR A RESIZE: ONE COMMAND, AT THE END OF THE GESTURE. The motion
    // was drawn; only the result is recorded, or a drag across the page would
    // be four hundred undo entries.
    QVector<ItemFrame> changed;
    for (std::size_t i = 0; i < live_.size() && i < starts_.size(); ++i)
        if (!(live_[i].second == starts_[i].second)) changed.push_back(live_[i]);
    live_.clear();
    starts_.clear();
    update();
    if (changed.size() == 1)
        emit itemMoved(changed.front().first, changed.front().second);
    else if (!changed.isEmpty())
        emit itemsMoved(changed);
}

void LayoutCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (const core::LayoutItem* hit = itemAt(event->pos()); hit != nullptr)
        emit itemActivated(QString::fromStdString(hit->id));
}

void LayoutCanvas::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) return;
    zoomBy(std::pow(1.2, steps), event->position());
    event->accept();
}

void LayoutCanvas::keyPressEvent(QKeyEvent* event)
{
    const core::Layout* l = layout();
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceHeld_ = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (!drawKind_.isEmpty()) {
            setDrawKind(QString());
            emit drawFinished();
        } else {
            setSelection({});
        }
        return;
    }
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoomBy(1.25);
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        zoomBy(0.8);
        return;
    }
    if (event->key() == Qt::Key_0) {
        zoomToFit();
        return;
    }
    if (l == nullptr) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        QStringList all;
        for (std::size_t i = 0; i < l->items.size(); ++i)
            if (l->page_of(i) == page_) all << QString::fromStdString(l->items[i].id);
        setSelection(all);
        return;
    }
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        !selection_.isEmpty()) {
        emit deleteRequested();
        return;
    }

    // ARROW KEYS NUDGE BY A MILLIMETRE, Shift by ten, the whole pick at once.
    const core::Um step = (event->modifiers() & Qt::ShiftModifier) != 0 ? 10 * kWhole : kWhole;
    core::Um dx         = 0;
    core::Um dy         = 0;
    switch (event->key()) {
    case Qt::Key_Left: dx = -step; break;
    case Qt::Key_Right: dx = step; break;
    case Qt::Key_Up: dy = -step; break;
    case Qt::Key_Down: dy = step; break;
    default: QWidget::keyPressEvent(event); return;
    }
    QVector<ItemFrame> moved;
    for (const QString& id : std::as_const(selection_))
        if (const core::LayoutItem* item = l->find(id.toStdString());
            item != nullptr && !item->locked) {
            core::PaperRect frame = item->frame;
            frame.x += dx;
            frame.y += dy;
            moved.push_back(ItemFrame{id, frame});
        }
    if (moved.size() == 1)
        emit itemMoved(moved.front().first, moved.front().second);
    else if (!moved.isEmpty())
        emit itemsMoved(moved);
}

void LayoutCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceHeld_ = false;
        setCursor(drawKind_.isEmpty() ? Qt::ArrowCursor : Qt::CrossCursor);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void LayoutCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    cachePage_ = -1;
    if (fit_) emit zoomChanged(zoomPercent());
}

void LayoutCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    if (const core::LayoutItem* hit = itemAt(event->pos()); hit != nullptr) {
        const QString id = QString::fromStdString(hit->id);
        if (!selection_.contains(id)) setSelection({id});
    }
    if (!selection_.isEmpty()) emit contextRequested(event->globalPos());
}

void LayoutCanvas::leaveEvent(QEvent* event)
{
    hover_.clear();
    update();
    QWidget::leaveEvent(event);
}

void LayoutCanvas::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void LayoutCanvas::paintSheet(QPainter& p, const QRectF& box)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    const qreal dpr        = devicePixelRatioF();
    const QSize wanted     = (box.size() * dpr).toSize();
    const std::uint64_t at = controller_.document().revision();
    if (cache_.isNull() || cachePage_ != page_ || cacheRevision_ != at || cacheSize_ != wanted ||
        cacheSheet_ != sheet_) {
        cache_ = QImage(wanted, QImage::Format_ARGB32_Premultiplied);
        cache_.setDevicePixelRatio(dpr);
        cache_.fill(Qt::white);
        QPainter sheet(&cache_);
        sheet.setRenderHint(QPainter::Antialiasing, true);
        sheet.setRenderHint(QPainter::TextAntialiasing, true);

        LayoutFacts facts;
        facts.sheet = QString::fromStdString(l->name);
        // `Bus::on_current_file` is what KAYDET reads to know where the drawing
        // came from; an unsaved drawing answers with nothing and `<proje>` is
        // then empty, which is the truth rather than a made-up name.
        if (controller_.bus().on_current_file) {
            const QFileInfo file(QString::fromStdString(controller_.bus().on_current_file()));
            facts.project     = file.fileName();
            facts.project_dir = file.absolutePath();
        }
        facts.crs  = QString::fromStdString(controller_.document().crs().id());
        facts.date = QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy"));

        // THE SCREEN'S OWN DPI, so a 0.25 mm hairline on the sheet is a hairline
        // here too — the designer shows what the printer will do, at a different
        // size (`layout_render.hpp`). The SAME function the PDF, the printer and
        // the image export go through (ui.md R36).
        const double dpi = logicalDpiX() > 0 ? logicalDpiX() : 96.0;
        paint_layout_page(sheet, QRectF(QPointF(0, 0), box.size()), controller_.document(), *l,
                          page_, dpi, facts, /*margin_guide=*/true);
        sheet.end();
        cachePage_     = page_;
        cacheRevision_ = at;
        cacheSize_     = wanted;
        cacheSheet_    = sheet_;
    }
    p.drawImage(box.topLeft(), cache_);
}

/// THE TWO MILLIMETRE SCALES, and the pick's span lit on them.
///
/// WHAT IT ANSWERS. "Where on the paper is this, and how wide is it" — asked of
/// the drawing rather than of a property field. The lit span turns four numbers
/// a user would otherwise read one at a time into one picture, and it moves
/// live while a frame is dragged, so the drag itself is measured.
///
/// THE STEP IS CHOSEN FROM THE ZOOM, not fixed: the coarsest step whose ticks
/// stay at least four pixels apart, so the scale stays readable at every paper
/// size and every zoom this window allows.
void LayoutCanvas::paintRulers(QPainter& p, const QRectF& box, const QRectF* lit) const
{
    const Tokens& t              = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
    const core::LayoutPage* page = activePage();
    if (page == nullptr || box.width() <= 0.0) return;
    p.save();

    const double per_mm     = box.width() / (static_cast<double>(page->w) / 1000.0);
    const core::Um step     = ruler_step(per_mm, 4.0);
    const core::Um labelled = std::max(ruler_step(per_mm, 46.0), step);

    p.setPen(Qt::NoPen);
    p.setBrush(t.bgStrip);
    p.drawRect(QRectF(0, 0, width(), kRulerPx));
    p.drawRect(QRectF(0, 0, kRulerPx, height()));

    // THE PAPER'S OWN SPAN on each scale, a step lighter than the pasteboard's,
    // so the ruler says where the sheet begins and ends as well as where the
    // pick sits.
    p.setBrush(t.bgSunken);
    p.drawRect(QRectF(box.left(), 0, box.width(), kRulerPx));
    p.drawRect(QRectF(0, box.top(), kRulerPx, box.height()));

    if (lit != nullptr) {
        p.setBrush(t.accentWash);
        p.drawRect(QRectF(lit->left(), 0, lit->width(), kRulerPx));
        p.drawRect(QRectF(0, lit->top(), kRulerPx, lit->height()));
        p.setPen(QPen(t.accent, 2.0));
        p.drawLine(QPointF(lit->left(), kRulerPx - 1.0), QPointF(lit->right(), kRulerPx - 1.0));
        p.drawLine(QPointF(kRulerPx - 1.0, lit->top()), QPointF(kRulerPx - 1.0, lit->bottom()));
        p.setPen(Qt::NoPen);
    }

    QFont small = p.font();
    small.setFamily(QStringLiteral("IBM Plex Mono"));
    small.setPointSizeF(std::max(7.0, small.pointSizeF() - 2.5));
    p.setFont(small);

    const auto ticks = [&](bool horizontal) {
        const core::Um extent = horizontal ? page->w : page->h;
        const double origin   = horizontal ? box.left() : box.top();
        const double scale    = horizontal ? box.width() / static_cast<double>(extent)
                                           : box.height() / static_cast<double>(extent);
        for (core::Um at = 0; at <= extent; at += step) {
            const double pos = origin + static_cast<double>(at) * scale;
            if (pos < kRulerPx || pos > (horizontal ? width() : height())) continue;
            const bool named  = at % labelled == 0;
            const double from = named ? 4.0 : kRulerPx - 5.0;
            p.setPen(QPen(named ? t.textFaint : t.rulerTick, 1.0));
            if (horizontal)
                p.drawLine(QPointF(pos, from), QPointF(pos, kRulerPx - 1.0));
            else
                p.drawLine(QPointF(from, pos), QPointF(kRulerPx - 1.0, pos));
            if (!named) continue;
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
    p.fillRect(rect(), t.bgCanvas);

    const core::Layout* l = layout();
    const QRectF box      = pageRect();
    if (l == nullptr || box.isEmpty()) {
        p.setPen(t.textFaint);
        p.drawText(rect(), Qt::AlignCenter, tr("Çıktı yerleşimi yok."));
        return;
    }

    // A SHADOW UNDER THE SHEET: the one piece of chrome that says "this is
    // paper on a table" without a word — the print window's sheet wears the
    // same one (`paint_paper_shadow`).
    paint_paper_shadow(p, box);

    paintSheet(p, box);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(t.lineHard, 1.0));
    p.drawRect(box);

    p.setRenderHint(QPainter::Antialiasing, false);

    // THE ITEM UNDER THE POINTER, outlined faintly: what a click would take.
    if (!hover_.isEmpty() && !selection_.contains(hover_) && gesture_ == Gesture::None)
        if (const core::LayoutItem* over = l->find(hover_.toStdString()); over != nullptr) {
            QColor faint = t.accent;
            faint.setAlpha(120);
            p.setPen(QPen(faint, 1.0, Qt::DashLine));
            p.drawRect(deviceFrom(over->frame));
        }

    // THE PICKS: an outline each — dashed when locked, because a locked box
    // does not move — and, while a gesture carries them, the box they will end
    // at, washed, over the place they are leaving.
    std::vector<QRectF> picked;
    for (const QString& id : std::as_const(selection_))
        if (const core::LayoutItem* item = l->find(id.toStdString()); item != nullptr) {
            QRectF shown = deviceFrom(item->frame);
            for (const auto& [moving, frame] : live_)
                if (moving == id) shown = deviceFrom(frame);
            picked.push_back(shown);
            if (!live_.empty() && gesture_ != Gesture::Draw) {
                p.fillRect(shown, t.accentWash);
            }
            p.setPen(QPen(t.accent, 1.0, item->locked ? Qt::DashLine : Qt::SolidLine));
            p.drawRect(shown);
        }

    // THE BOX A TOOL IS DRAWING, and its size beside it.
    if (gesture_ == Gesture::Draw && !live_.empty()) {
        const QRectF drawn = deviceFrom(live_.front().second);
        p.fillRect(drawn, t.accentWash);
        p.setPen(QPen(t.accent, 1.0, Qt::DashLine));
        p.drawRect(drawn);
    }

    // SMART GUIDES: the page line, margin or edge the drag has caught.
    if (!guides_.empty()) {
        p.setPen(QPen(t.accent, 1.0));
        for (const SnapLine& guide : guides_) {
            const QRectF line =
                deviceFrom(guide.vertical ? core::PaperRect{guide.at, 0, 0, activePage()->h}
                                          : core::PaperRect{0, guide.at, activePage()->w, 0});
            p.drawLine(line.topLeft(), line.bottomRight());
        }
    }

    // A MARQUEE, washed.
    if (gesture_ == Gesture::Marquee && marquee_.width() > 1) {
        p.fillRect(marquee_, t.accentWash);
        p.setPen(QPen(t.accent, 1.0, Qt::DashLine));
        p.drawRect(marquee_);
    }

    // THE HANDLES, on one unlocked pick. Several picks move together and show
    // their shared box instead; resizing one of a group is a job for the panel.
    const core::LayoutItem* one =
        selection_.size() == 1 ? l->find(primary_.toStdString()) : nullptr;
    if (one != nullptr && !one->locked && !picked.empty()) {
        const QRectF chosen = picked.back();
        p.setBrush(t.accent);
        p.setPen(QPen(Qt::white, 1.0));
        const double g = kGripPx / 2.0;
        for (const QPointF& corner :
             {chosen.topLeft(), QPointF(chosen.center().x(), chosen.top()), chosen.topRight(),
              QPointF(chosen.right(), chosen.center().y()), chosen.bottomRight(),
              QPointF(chosen.center().x(), chosen.bottom()), chosen.bottomLeft(),
              QPointF(chosen.left(), chosen.center().y())})
            p.drawRect(QRectF(corner.x() - g, corner.y() - g, 2 * g, 2 * g));
    } else if (picked.size() > 1) {
        QRectF group = picked.front();
        for (const QRectF& r : picked)
            group = group.united(r);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(t.accent, 1.0, Qt::DotLine));
        p.drawRect(group.adjusted(-3, -3, 3, 3));
    }

    // THE MEASUREMENT OF A GESTURE, beside it: where the box is and how big,
    // in the millimetres the fields will say once it is let go.
    if (!live_.empty() && gesture_ != Gesture::None && gesture_ != Gesture::Pan) {
        const core::PaperRect f =
            gesture_ == Gesture::Move ? union_of(live_) : live_.front().second;
        const QString said = gesture_ == Gesture::Move
                                 ? tr("%1, %2 mm").arg(mm_shown(f.x), mm_shown(f.y))
                                 : tr("%1 × %2 mm").arg(mm_shown(f.w), mm_shown(f.h));
        QFont mono         = p.font();
        mono.setFamily(QStringLiteral("IBM Plex Mono"));
        mono.setPointSizeF(std::max(8.0, mono.pointSizeF() - 1.0));
        p.setFont(mono);
        const QRectF at = deviceFrom(f);
        const QFontMetricsF fm(mono);
        const QRectF chip(at.right() + 8, at.bottom() + 6, fm.horizontalAdvance(said) + 12, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(t.accent);
        p.drawRoundedRect(chip, 3, 3);
        p.setPen(t.onAccent);
        p.drawText(chip, Qt::AlignCenter, said);
    }

    // THE RULERS LAST, over the pasteboard, with the pick's span lit.
    QRectF span;
    for (const QRectF& r : picked)
        span = span.isNull() ? r : span.united(r);
    if (gesture_ == Gesture::Draw && !live_.empty()) span = deviceFrom(live_.front().second);
    paintRulers(p, box, span.isNull() ? nullptr : &span);
}

// ======================================================== LayoutDesigner =====

LayoutDesigner::LayoutDesigner(Controller& controller, QString layout, QWidget* parent)
    : DialogFrame(parent), controller_(controller), name_(std::move(layout))
{
    // THE NAME IS THE SYSTEM TITLE BAR'S, the way §8 has it for the style
    // designer: `Çıktı Yerleşimi Tasarımcısı — Pafta`. The window draws only
    // its own 48 px footer.
    setHeading(Glyph::Layout, tr("Çıktı Yerleşimi Tasarımcısı"), QStringLiteral("— %1").arg(name_));
    setFooterHeight(48);
    setBody(buildBody());

    // THE STYLE DESIGNER'S SIZE, because it is the same kind of window — a
    // thing to work on with a property column beside it (design.md §4, §8).
    // It opens at 1280 × 820 where the screen has that and never wider than
    // the screen. 1040 × 680 is the least at which the tool row, the rail, a
    // sheet that can be read and the inspector all fit without cutting any of
    // them: every row below is measured against it.
    setMinimumSize(1040, 680);
    QSize room(1280, 820);
    if (const QScreen* screen = QGuiApplication::primaryScreen(); screen != nullptr) {
        const QSize free = screen->availableSize();
        room             = QSize(std::min(room.width(), free.width() - 48),
                                 std::min(room.height(), free.height() - 64));
    }
    resize(room.expandedTo(minimumSize()));

    auto* close = new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer()->addWidget(close);

    auto* pdf = new Button(ButtonRole::Primary, tr("PDF'e aktar…"), Glyph::Export, this);
    pdf->setToolTip(tr("Yerleşimi PDF olarak yazar: YAZDIR yerlesim=… dosya=…"));
    connect(pdf, &QPushButton::clicked, this, [this] { exportSheet(); });
    footer()->addWidget(pdf);

    // THE KEYBOARD OF A PAGE EDITOR. A modal window takes the keys the main
    // window's actions would have had, so the ones a user reaches for here are
    // declared here. A field being typed in keeps Ctrl+Z for its own text: a
    // line edit claims its standard keys before a shortcut sees them.
    const auto key = [this](const QKeySequence& keys, auto&& act) {
        auto* shortcut = new QShortcut(keys, this);
        connect(shortcut, &QShortcut::activated, this, std::forward<decltype(act)>(act));
    };
    key(QKeySequence::Undo, [this] {
        controller_.runLine(QStringLiteral("GERİAL"), command::Origin::Gui);
        refresh();
    });
    key(QKeySequence::Redo, [this] {
        controller_.runLine(QStringLiteral("YİNELE"), command::Origin::Gui);
        refresh();
    });
    key(QKeySequence(Qt::CTRL | Qt::Key_D), [this] { duplicatePicked(); });
    key(QKeySequence(Qt::CTRL | Qt::Key_L), [this] { toggleLockPicked(); });

    // THE WINDOW FOLLOWS THE DOCUMENT, not its own record of it: a `ÇIKTIÖĞE`
    // line typed on the command line while this is open redraws it, which is
    // what makes the two clients equal rather than merely both present.
    connect(&controller_, &Controller::documentChanged, this, [this] { refresh(); });

    refresh();
    LayoutDesigner::applyTheme(theme());
    canvas_->setFocus();
}

const core::Layout* LayoutDesigner::layout() const
{
    return controller_.document().layouts().find(name_.toStdString());
}

void LayoutDesigner::setViewWindow(core::Box2 window)
{
    viewWindow_ = window;
    buildProperties();
}

QWidget* LayoutDesigner::buildBody()
{
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // The canvas first: the strips and the inspector all talk to it.
    canvas_ = new LayoutCanvas(controller_, body);
    canvas_->setSheet(name_, 0);

    column->addWidget(buildToolRow());

    auto* middle = new QWidget(body);
    auto* row    = new QHBoxLayout(middle);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(buildToolRail());
    row->addWidget(canvas_, 1);
    row->addWidget(buildInspector());
    column->addWidget(middle, 1);
    column->addWidget(buildStatusStrip());

    // ---- what the canvas reports -------------------------------------------
    connect(canvas_, &LayoutCanvas::selectionChanged, this, [this](const QString&) {
        pane_ = canvas_->selection().isEmpty() ? Pane::Sheet : Pane::Item;
        refresh();
    });
    connect(canvas_, &LayoutCanvas::itemMoved, this,
            [this](const QString& id, core::PaperRect frame) {
                // THE GESTURE BECOMES THE COMMAND. This is the line a script
                // would type, so what a hand can do a batch job can do too.
                controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=tasi yerlesim=%1 ad=%2 x=%3 "
                                                   "y=%4 genislik=%5 yukseklik=%6")
                                        .arg(quoted(name_), quoted(id), mm_text(frame.x),
                                             mm_text(frame.y), mm_text(frame.w), mm_text(frame.h)),
                                    command::Origin::Gui);
                refresh();
            });
    connect(canvas_, &LayoutCanvas::itemsMoved, this, [this](const QVector<ItemFrame>& frames) {
        QStringList lines;
        for (const auto& [id, frame] : frames)
            lines << QStringLiteral("ÇIKTIÖĞE islem=tasi yerlesim=%1 ad=%2 x=%3 y=%4")
                         .arg(quoted(name_), quoted(id), mm_text(frame.x), mm_text(frame.y));
        controller_.runLines(lines, tr("%1 öğeyi taşı").arg(frames.size()), command::Origin::Gui);
        refresh();
    });
    connect(canvas_, &LayoutCanvas::itemDrawn, this, &LayoutDesigner::placeItem);
    connect(canvas_, &LayoutCanvas::drawFinished, this, [this] {
        if (tools_ != nullptr && tools_->button(0) != nullptr) tools_->button(0)->setChecked(true);
        if (const core::Layout* l = layout(); l != nullptr) refreshStatus(*l);
    });
    connect(canvas_, &LayoutCanvas::itemActivated, this, [this](const QString& id) {
        canvas_->select(id);
        pane_ = Pane::Item;
        refresh();
        // THE FIRST FIELD TAKES THE KEYBOARD, the way R53 has a tool opened for
        // its figure put its value in the first field.
        if (properties_ != nullptr)
            if (auto* first = properties_->findChild<Field*>(); first != nullptr) first->setFocus();
    });
    connect(canvas_, &LayoutCanvas::deleteRequested, this, &LayoutDesigner::deletePicked);
    connect(canvas_, &LayoutCanvas::contextRequested, this,
            [this](const QPoint& at) { itemMenu()->popup(at); });
    connect(canvas_, &LayoutCanvas::cursorAt, this, [this](double x, double y, bool on) {
        if (cursorReadout_ == nullptr) return;
        cursorReadout_->setText(on ? tr("X %1   Y %2 mm")
                                         .arg(QString::number(x, 'f', 1).replace('.', ','),
                                              QString::number(y, 'f', 1).replace('.', ','))
                                   : tr("kâğıdın dışında"));
    });
    connect(canvas_, &LayoutCanvas::zoomChanged, this, [this](double percent) {
        if (zoomReadout_ != nullptr)
            zoomReadout_->setText(QStringLiteral("%%1").arg(std::lround(percent)));
    });
    return body;
}

QWidget* LayoutDesigner::buildToolRow()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("layoutToolRow"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(40);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(8, 0, 8, 1);
    row->setSpacing(2);

    const auto rule = [bar, row] {
        row->addSpacing(6);
        row->addWidget(strip_rule(bar, true));
        row->addSpacing(6);
    };
    const auto tool = [bar, row](Glyph glyph, const QString& tip) {
        Button* b = strip_button(glyph, tip, bar);
        row->addWidget(b);
        return b;
    };

    // ---- history ------------------------------------------------------------
    connect(tool(Glyph::Undo, tr("Geri al (Ctrl+Z)")), &QPushButton::clicked, this, [this] {
        controller_.runLine(QStringLiteral("GERİAL"), command::Origin::Gui);
        refresh();
    });
    connect(tool(Glyph::Redo, tr("Yinele (Ctrl+Shift+Z)")), &QPushButton::clicked, this, [this] {
        controller_.runLine(QStringLiteral("YİNELE"), command::Origin::Gui);
        refresh();
    });
    rule();

    // ---- lining the pick up -------------------------------------------------
    //
    // SIX BUTTONS, NOT A MENU. Alignment is done a dozen times on one sheet,
    // and a menu makes every one of them two clicks and a read. Horizontal
    // three, a gap, vertical three — the way the glyphs themselves are drawn.
    struct Align
    {
        Glyph glyph;
        const char* tip;
        int how;
    };

    static constexpr Align kAligns[] = {
        {Glyph::LayoutAlignLeft, "Sol kenarları hizala", 0},
        {Glyph::LayoutAlignCentre, "Yatayda ortala", 1},
        {Glyph::LayoutAlignRight, "Sağ kenarları hizala", 2},
        {Glyph::LayoutAlignTop, "Üst kenarları hizala", 3},
        {Glyph::LayoutAlignMiddle, "Dikeyde ortala", 4},
        {Glyph::LayoutAlignBottom, "Alt kenarları hizala", 5},
    };
    for (const Align& one : kAligns) {
        Button* b =
            tool(one.glyph, tr("%1 — tek öğe kenar payına göre hizalanır").arg(tr(one.tip)));
        connect(b, &QPushButton::clicked, this, [this, how = one.how] { alignPicked(how); });
        needOne_.push_back(b);
        if (one.how == 2) row->addSpacing(6);
    }
    rule();

    Button* across = tool(Glyph::LayoutSpreadAcross,
                          tr("Yatayda dağıt — aradaki boşluklar eşitlenir; en az üç öğe"));
    connect(across, &QPushButton::clicked, this, [this] { spreadPicked(true); });
    Button* down = tool(Glyph::LayoutSpreadDown,
                        tr("Dikeyde dağıt — aradaki boşluklar eşitlenir; en az üç öğe"));
    connect(down, &QPushButton::clicked, this, [this] { spreadPicked(false); });
    needThree_ = {across, down};
    rule();

    // ---- the stack and the pick itself --------------------------------------
    Button* front = tool(Glyph::LayoutFront, tr("En öne getir"));
    connect(front, &QPushButton::clicked, this, [this] { restackPicked(QStringLiteral("on")); });
    Button* back = tool(Glyph::LayoutBack, tr("En arkaya gönder"));
    connect(back, &QPushButton::clicked, this, [this] { restackPicked(QStringLiteral("back")); });
    rule();
    Button* copy = tool(Glyph::Duplicate, tr("Çoğalt — kopyası yanına (Ctrl+D)"));
    connect(copy, &QPushButton::clicked, this, &LayoutDesigner::duplicatePicked);
    lockButton_ = tool(Glyph::Lock, tr("Kilitle (Ctrl+L)"));
    connect(lockButton_, &QPushButton::clicked, this, &LayoutDesigner::toggleLockPicked);
    Button* remove = tool(Glyph::Trash, tr("Sil (Delete)"));
    connect(remove, &QPushButton::clicked, this, &LayoutDesigner::deletePicked);
    needOne_.insert(needOne_.end(), {front, back, copy, lockButton_, remove});

    row->addStretch(1);

    // ---- the page -----------------------------------------------------------
    //
    // WHICH PAGE, AND WHAT A PAGE CAN HAVE DONE TO IT, in one place: the
    // arrows step, the name opens the list of pages and the three page verbs.
    pagePrev_ = tool(Glyph::ChevronLeft, tr("Önceki sayfa"));
    connect(pagePrev_, &QPushButton::clicked, this, [this] { showPage(canvas_->page() - 1); });
    pageMenu_ = new Button(ButtonRole::Ghost, tr("Sayfa 1 / 1"), std::nullopt, bar);
    pageMenu_->setControlSize(ControlSize::Compact);
    pageMenu_->setToolTip(tr("Sayfalar: gidin, ekleyin, çoğaltın, silin"));
    auto* pages = new QMenu(pageMenu_);
    connect(pages, &QMenu::aboutToShow, this, [this, pages] {
        pages->clear();
        const core::Layout* l = layout();
        if (l == nullptr) return;
        for (std::size_t i = 0; i < l->pages.size(); ++i) {
            const core::LayoutPage& page = l->pages[i];
            QAction* go                  = pages->addAction(
                tr("Sayfa %1 — %2 × %3 mm").arg(i + 1).arg(page.w / 1000).arg(page.h / 1000));
            go->setCheckable(true);
            go->setChecked(static_cast<int>(i) == canvas_->page());
            connect(go, &QAction::triggered, this, [this, i] { showPage(static_cast<int>(i)); });
        }
        pages->addSeparator();
        connect(pages->addAction(tr("Sayfa ekle")), &QAction::triggered, this,
                [this] { pageVerb("sayfaekle"); });
        connect(pages->addAction(tr("Bu sayfayı çoğalt")), &QAction::triggered, this,
                [this] { pageVerb("sayfacogalt"); });
        QAction* drop = pages->addAction(tr("Bu sayfayı sil"));
        drop->setEnabled(l->pages.size() > 1);
        connect(drop, &QAction::triggered, this, [this] { pageVerb("sayfasil"); });
    });
    pageMenu_->setMenuArrow(pages);
    row->addWidget(pageMenu_);
    pageNext_ = tool(Glyph::ChevronRight, tr("Sonraki sayfa"));
    connect(pageNext_, &QPushButton::clicked, this, [this] { showPage(canvas_->page() + 1); });
    rule();

    // ---- the view -----------------------------------------------------------
    snapSwitch_ = tool(Glyph::Snap, tr("Yakala — sürüklerken sayfanın kenarına, ortasına, kenar "
                                       "payına ve öteki öğelerin kenar ve ortalarına"));
    snapSwitch_->setCheckable(true);
    snapSwitch_->setChecked(true);
    connect(snapSwitch_, &QAbstractButton::toggled, this,
            [this](bool on) { canvas_->setSnapping(on); });
    rule();

    connect(tool(Glyph::ZoomOut, tr("Uzaklaş (−)")), &QPushButton::clicked, this,
            [this] { canvas_->zoomBy(0.8); });
    zoomReadout_ = new Button(ButtonRole::Ghost, QStringLiteral("%100"), std::nullopt, bar);
    zoomReadout_->setControlSize(ControlSize::Compact);
    zoomReadout_->setToolTip(tr("Yakınlaştırma: %100 kâğıdın ekrandaki gerçek boyudur"));
    zoomReadout_->setMinimumWidth(64);
    auto* zooms = new QMenu(zoomReadout_);
    connect(zooms->addAction(tr("Sayfayı sığdır")), &QAction::triggered, this,
            [this] { canvas_->zoomToFit(); });
    zooms->addSeparator();
    for (const int percent : {50, 100, 200, 400})
        connect(zooms->addAction(QStringLiteral("%%1").arg(percent)), &QAction::triggered, this,
                [this, percent] {
                    const double now = canvas_->zoomPercent();
                    if (now > 0.0) canvas_->zoomBy(percent / now);
                });
    zoomReadout_->setMenuArrow(zooms);
    row->addWidget(zoomReadout_);
    connect(tool(Glyph::ZoomIn, tr("Yakınlaş (+)")), &QPushButton::clicked, this,
            [this] { canvas_->zoomBy(1.25); });
    connect(tool(Glyph::Fit, tr("Sayfayı sığdır (0)")), &QPushButton::clicked, this,
            [this] { canvas_->zoomToFit(); });
    connect(tool(Glyph::LayoutRealSize, tr("Gerçek boy — kâğıt ekranda kendi ölçüsünde")),
            &QPushButton::clicked, this, [this] { canvas_->zoomToRealSize(); });
    return bar;
}

QWidget* LayoutDesigner::buildToolRail()
{
    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("layoutRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setFixedWidth(47); // 32 + 7 each side + the 1 px edge
    auto* column = new QVBoxLayout(rail);
    column->setContentsMargins(7, 8, 8, 8);
    column->setSpacing(2);

    // ---- the tools: pick, or draw one of the nine ---------------------------
    //
    // A TOOL IS A MODE, as it is in every page editor: press Harita, drag a box
    // on the paper, and the map frame is where the box was; a click without a
    // drag puts the kind's own size there. Down the sheet's edge, as a toolbox,
    // because what is ADDED to a sheet and what is DONE to the pick are two
    // different questions and one row for both was a row wider than a laptop.
    tools_ = new QButtonGroup(rail);
    tools_->setExclusive(true);
    Button* pick = strip_button(Glyph::Select, tr("Seç ve taşı (Esc)"), rail);
    pick->setCheckable(true);
    pick->setChecked(true);
    tools_->addButton(pick, 0);
    column->addWidget(pick);

    const auto rule = [rail, column] {
        column->addSpacing(5);
        column->addWidget(strip_rule(rail, false), 0, Qt::AlignHCenter);
        column->addSpacing(5);
    };
    rule();
    int at = 1;
    for (const Kind& one : kKinds) {
        Button* b =
            strip_button(one.glyph,
                         tr("%1 ekle — kâğıtta sürükleyerek çizin; tıklamak öntanımlı boyda koyar")
                             .arg(tr(one.label)),
                         rail);
        b->setCheckable(true);
        b->setProperty("tool", QString::fromUtf8(one.word));
        tools_->addButton(b, at++);
        column->addWidget(b);
        // THREE FAMILIES: what describes the map, what decorates the sheet,
        // what reports the data.
        if (one.which == core::LayoutItemKind::NorthArrow ||
            one.which == core::LayoutItemKind::Shape)
            rule();
    }
    column->addStretch(1);

    connect(tools_, &QButtonGroup::idClicked, this, [this](int id) {
        canvas_->setDrawKind(
            id <= 0 ? QString() : QString::fromUtf8(kKinds[static_cast<std::size_t>(id - 1)].word));
        canvas_->setFocus();
        if (const core::Layout* l = layout(); l != nullptr) refreshStatus(*l);
    });
    return rail;
}

QWidget* LayoutDesigner::buildInspector()
{
    auto* inspector = new QWidget(this);
    inspector->setObjectName(QStringLiteral("layoutInspector"));
    inspector->setAttribute(Qt::WA_StyledBackground, true);
    inspector->setFixedWidth(kInspector);
    auto* column = new QVBoxLayout(inspector);
    column->setContentsMargins(13, 10, 8, 0); // 1 of the 13 is the edge
    column->setSpacing(6);

    // ---- what is on the page, top first -------------------------------------
    auto* head    = new QWidget(inspector);
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(0, 0, 6, 0);
    headRow->setSpacing(8);
    auto* listed = new QLabel(tr("ÖĞELER"), head);
    listed->setObjectName(QStringLiteral("groupCaption"));
    headRow->addWidget(listed);
    headRow->addStretch(1);
    itemsCount_ = new QLabel(head);
    itemsCount_->setObjectName(QStringLiteral("formHelp"));
    headRow->addWidget(itemsCount_);
    column->addWidget(head);

    items_ = new QListWidget(inspector);
    items_->setObjectName(QStringLiteral("layoutItems"));
    items_->setFrameShape(QFrame::NoFrame);
    items_->setMouseTracking(true);
    items_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    items_->setContextMenuPolicy(Qt::CustomContextMenu);
    items_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    items_->setFixedHeight(kListRows * 28 + 2);
    items_->setAccessibleName(tr("Öğeler"));
    auto* rows   = new ItemRow(items_);
    rows->onLock = [this](const QString& id) {
        const core::Layout* l = layout();
        if (l == nullptr) return;
        const core::LayoutItem* item = l->find(id.toStdString());
        if (item == nullptr) return;
        controller_.runLine(
            QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 kilit=%3")
                .arg(quoted(name_), quoted(id),
                     item->locked ? QStringLiteral("hayir") : QStringLiteral("evet")),
            command::Origin::Gui);
        refresh();
    };
    rows_ = rows;
    items_->setItemDelegate(rows);
    connect(items_, &QListWidget::itemSelectionChanged, this, [this] {
        if (filling_) return;
        QStringList picked;
        for (int i = 0; i < items_->count(); ++i)
            if (items_->item(i)->isSelected())
                picked << items_->item(i)->data(Qt::UserRole).toString();
        // THE ROW LAST CLICKED is the one the inspector describes.
        if (QListWidgetItem* current = items_->currentItem();
            current != nullptr && current->isSelected()) {
            const QString id = current->data(Qt::UserRole).toString();
            picked.removeAll(id);
            picked << id;
        }
        canvas_->setSelection(picked);
    });
    connect(items_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        if (QListWidgetItem* row = items_->itemAt(at); row != nullptr && !row->isSelected())
            canvas_->select(row->data(Qt::UserRole).toString());
        if (!canvas_->selection().isEmpty()) itemMenu()->popup(items_->viewport()->mapToGlobal(at));
    });
    column->addWidget(items_);

    auto* divide = new QWidget(inspector);
    divide->setObjectName(QStringLiteral("layoutStripRule"));
    divide->setAttribute(Qt::WA_StyledBackground, true);
    divide->setFixedHeight(1);
    column->addSpacing(4);
    column->addWidget(divide);
    column->addSpacing(4);

    // ---- the inspector ------------------------------------------------------
    //
    // `ÖĞE | SAYFA`: what is picked, or the sheet itself. The sheet always
    // exists, so there is never an empty column telling the user to click.
    paneSwitch_ = new Segment(inspector);
    paneSwitch_->setControlSize(ControlSize::Compact);
    paneSwitch_->addOption(tr("Öğe"), tr("Seçili öğenin ayarları"));
    paneSwitch_->addOption(tr("Sayfa"), tr("Kâğıt, yön, kenar payı, çözünürlük, sayfalar"));
    paneSwitch_->setContentsMargins(0, 0, 6, 0);
    connect(paneSwitch_, &Segment::currentChanged, this, [this](int at) {
        if (filling_) return;
        pane_ = at == 0 ? Pane::Item : Pane::Sheet;
        buildProperties();
    });
    column->addWidget(paneSwitch_);

    // A NAME, NOT AN EYEBROW: which of the boxes on the sheet these numbers
    // belong to — its picture, its name, and under them its kind, its id and
    // its size.
    auto* who    = new QWidget(inspector);
    auto* whoRow = new QHBoxLayout(who);
    whoRow->setContentsMargins(0, 6, 6, 2);
    whoRow->setSpacing(10);
    headGlyph_ = new QLabel(who);
    headGlyph_->setFixedSize(24, 24);
    whoRow->addWidget(headGlyph_, 0, Qt::AlignTop);
    auto* names     = new QWidget(who);
    auto* nameStack = new QVBoxLayout(names);
    nameStack->setContentsMargins(0, 0, 0, 0);
    nameStack->setSpacing(1);
    headName_   = new QLabel(names);
    QFont named = headName_->font();
    named.setPointSizeF(named.pointSizeF() + 1.5);
    named.setWeight(QFont::DemiBold);
    headName_->setFont(named);
    nameStack->addWidget(headName_);
    headKind_ = new QLabel(names);
    headKind_->setObjectName(QStringLiteral("formHelp"));
    nameStack->addWidget(headKind_);
    whoRow->addWidget(names, 1);
    column->addWidget(who);

    // INSIDE A SCROLL AREA, with room for the bar and under the last row:
    // flush against the viewport the bar is drawn ON the editors, and the last
    // row is cut in half by the edge the moment the page is a pixel too tall
    // (the style designer's own two lessons, `style_designer.cpp`).
    scroll_ = new QScrollArea(inspector);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    properties_     = new QWidget(scroll_);
    propertyColumn_ = new QVBoxLayout(properties_);
    propertyColumn_->setContentsMargins(0, 0, 14, 16);
    propertyColumn_->setSpacing(6);
    scroll_->setWidget(properties_);
    column->addWidget(scroll_, 1);
    return inspector;
}

QWidget* LayoutDesigner::buildStatusStrip()
{
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("layoutStatus"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    strip->setFixedHeight(26);
    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(12, 1, 12, 0);
    row->setSpacing(18);

    // WHERE THE POINTER IS, in paper millimetres, in mono so the digits stand
    // still while it moves.
    cursorReadout_ = new QLabel(tr("kâğıdın dışında"), strip);
    cursorReadout_->setObjectName(QStringLiteral("layoutCursor"));
    cursorReadout_->setMinimumWidth(150);
    row->addWidget(cursorReadout_);

    pickReadout_ = new QLabel(strip);
    pickReadout_->setObjectName(QStringLiteral("formHelp"));
    row->addWidget(pickReadout_);

    // WHAT THE CHECKS FOUND, as a control: it opens the sheet's page of the
    // inspector where the list is. A count nobody can open is a number to
    // worry about.
    troubleReadout_ = new Button(ButtonRole::Ghost, QString(), Glyph::Warning, strip);
    troubleReadout_->setControlSize(ControlSize::Compact);
    connect(troubleReadout_, &QPushButton::clicked, this, [this] {
        pane_ = Pane::Sheet;
        buildProperties();
    });
    row->addWidget(troubleReadout_);

    row->addStretch(1);
    // THE HINT GIVES WAY FIRST. It is advice; what is picked and where the
    // pointer is are facts, and at the window's least width the advice was
    // eating the last letters of the pick's size.
    hint_ = new QLabel(strip);
    hint_->setObjectName(QStringLiteral("formHelp"));
    hint_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hint_->setMinimumWidth(1);
    hint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(hint_, 1);
    return strip;
}

// ---------------------------------------------------------------- the edits --

void LayoutDesigner::edit(const QString& arguments, const QString& verb)
{
    if (canvas_->selected().isEmpty()) return;
    controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=%1 yerlesim=%2 ad=%3 %4")
                            .arg(verb, quoted(name_), quoted(canvas_->selected()), arguments),
                        command::Origin::Gui);
    refresh();
}

void LayoutDesigner::editAll(const QString& arguments, const QString& label)
{
    QStringList lines;
    for (const QString& id : canvas_->selection())
        lines << QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 %3")
                     .arg(quoted(name_), quoted(id), arguments);
    if (lines.isEmpty()) return;
    controller_.runLines(lines, label, command::Origin::Gui);
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

    // ALL FOUR, EVERY TIME — and this is not belt and braces. `islem=sayfa`
    // DEFAULTS what it is not told: an omitted `kagit` is A4, an omitted `yon`
    // is dikey, an omitted `kenar` is 10. So the panel writes the sheet as it
    // should END UP, with the field the user touched overriding what is there.
    const QString paper =
        l->paper.empty() ? QStringLiteral("ozel") : QString::fromStdString(l->paper);
    QString whole =
        QStringLiteral("kagit=%1 yon=%2 kenar=%3 dpi=%4")
            .arg(paper, page.w > page.h ? QStringLiteral("yatay") : QStringLiteral("dikey"))
            .arg(l->margin / 1000)
            .arg(l->dpi);
    // A CUSTOM PAPER CARRIES ITS SIZE, because `ozel` without one is refused.
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
/// hand would type, and the command does the work.
void LayoutDesigner::pageVerb(const char* verb)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    const std::size_t was = l->pages.size();
    const int shown       = std::clamp(canvas_->page(), 0, static_cast<int>(was) - 1);
    controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=%1 ad=%2 sayfa=%3")
                            .arg(QString::fromUtf8(verb), quoted(name_))
                            .arg(shown + 1),
                        command::Origin::Gui);

    // THE PAGE THAT IS NOW SHOWING may not be the one that was: deleting the
    // last page has to leave the canvas on a page that exists, and a new page
    // is the one a user wants to look at. `l` is stale from the line above on.
    const core::Layout* after = layout();
    if (after != nullptr && !after->pages.empty()) {
        int next = shown;
        if (std::string_view(verb) != "sayfasil" && after->pages.size() > was) next = shown + 1;
        canvas_->setSheet(name_, std::clamp(next, 0, static_cast<int>(after->pages.size()) - 1));
    }
    pane_ = Pane::Sheet;
    refresh();
}

void LayoutDesigner::showPage(int index)
{
    const core::Layout* l = layout();
    if (l == nullptr || index < 0 || index >= static_cast<int>(l->pages.size()) ||
        index == canvas_->page())
        return;
    canvas_->setSheet(name_, index);
    pane_ = Pane::Sheet;
    refresh();
}

void LayoutDesigner::addItem(const QString& kind)
{
    QString line =
        QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=%1 tur=%2").arg(quoted(name_), kind);
    if (canvas_->page() > 0) line += QStringLiteral(" sayfa=%1").arg(canvas_->page() + 1);
    controller_.runLine(line, command::Origin::Gui);

    // THE NEW ITEM IS PICKED, because adding one and then having to find it is
    // two gestures for one intention. It is the last in the list: `ekle` appends.
    if (const core::Layout* l = layout(); l != nullptr && !l->items.empty())
        canvas_->select(QString::fromStdString(l->items.back().id));
    pane_ = Pane::Item;
    refresh();
}

void LayoutDesigner::placeItem(const QString& kind, core::PaperRect frame)
{
    const core::Layout* l        = layout();
    const core::LayoutPage* page = canvas_->activePage();
    const Kind* one              = kind_named(kind);
    if (l == nullptr || page == nullptr || one == nullptr) return;

    // A CLICK PUTS THE KIND'S OWN SIZE ABOUT THE CLICK, kept on the paper.
    if (frame.w <= 0 || frame.h <= 0) {
        const auto w = core::um_from_mm(static_cast<std::int64_t>(one->w_mm));
        const auto h = core::um_from_mm(static_cast<std::int64_t>(one->h_mm));
        frame        = core::PaperRect{std::clamp<core::Um>(frame.x - w / 2, 0, page->w - w),
                                std::clamp<core::Um>(frame.y - h / 2, 0, page->h - h), w, h};
    }
    QString line = QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=%1 tur=%2 x=%3 y=%4 genislik=%5 "
                                  "yukseklik=%6")
                       .arg(quoted(name_), kind, mm_text(frame.x), mm_text(frame.y),
                            mm_text(frame.w), mm_text(frame.h));
    if (canvas_->page() > 0) line += QStringLiteral(" sayfa=%1").arg(canvas_->page() + 1);
    controller_.runLine(line, command::Origin::Gui);
    if (const core::Layout* after = layout(); after != nullptr && !after->items.empty())
        canvas_->select(QString::fromStdString(after->items.back().id));
    pane_ = Pane::Item;
    refresh();
}

void LayoutDesigner::alignPicked(int how)
{
    const core::Layout* l = layout();
    if (l == nullptr || canvas_->selection().isEmpty()) return;
    std::vector<ItemFrame> picked;
    for (const QString& id : canvas_->selection())
        if (const core::LayoutItem* item = l->find(id.toStdString());
            item != nullptr && !item->locked)
            picked.push_back(ItemFrame{id, item->frame});
    if (picked.empty()) return;

    // SEVERAL LINE UP WITH THEIR SHARED BOX; ONE WITH THE MARGIN — QGIS's rule,
    // and the only one that makes a single title block's `ortala` mean centre
    // it on the sheet.
    core::PaperRect bounds = union_of(picked);
    if (picked.size() == 1) {
        const core::LayoutPage* page = canvas_->activePage();
        bounds =
            core::PaperRect{l->margin, l->margin, page->w - 2 * l->margin, page->h - 2 * l->margin};
    }
    QStringList lines;
    for (auto& [id, f] : picked) {
        core::PaperRect to = f;
        switch (how) {
        case 0: to.x = bounds.x; break;
        case 1: to.x = bounds.x + (bounds.w - f.w) / 2; break;
        case 2: to.x = bounds.right() - f.w; break;
        case 3: to.y = bounds.y; break;
        case 4: to.y = bounds.y + (bounds.h - f.h) / 2; break;
        default: to.y = bounds.bottom() - f.h; break;
        }
        if (to == f) continue;
        lines << QStringLiteral("ÇIKTIÖĞE islem=tasi yerlesim=%1 ad=%2 x=%3 y=%4")
                     .arg(quoted(name_), quoted(id), mm_text(to.x, 2), mm_text(to.y, 2));
    }
    if (lines.isEmpty()) return;
    controller_.runLines(lines, tr("hizala"), command::Origin::Gui);
    refresh();
}

void LayoutDesigner::spreadPicked(bool across)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    std::vector<ItemFrame> picked;
    for (const QString& id : canvas_->selection())
        if (const core::LayoutItem* item = l->find(id.toStdString());
            item != nullptr && !item->locked)
            picked.push_back(ItemFrame{id, item->frame});
    if (picked.size() < 3) {
        if (hint_ != nullptr) hint_->setText(tr("Dağıtmak için en az üç öğe seçin."));
        return;
    }
    std::sort(picked.begin(), picked.end(), [across](const ItemFrame& a, const ItemFrame& b) {
        return across ? a.second.x < b.second.x : a.second.y < b.second.y;
    });
    // THE GAPS, not the positions, are made equal: the first and the last stay
    // where they are and what lies between them is shared out.
    core::Um filled = 0;
    for (const auto& [id, f] : picked)
        filled += across ? f.w : f.h;
    const core::PaperRect bounds = union_of(picked);
    const core::Um span          = across ? bounds.w : bounds.h;
    const core::Um gap           = (span - filled) / static_cast<core::Um>(picked.size() - 1);
    core::Um at                  = across ? bounds.x : bounds.y;
    QStringList lines;
    for (auto& [id, f] : picked) {
        core::PaperRect to = f;
        if (across)
            to.x = at;
        else
            to.y = at;
        at += (across ? f.w : f.h) + gap;
        if (to == f) continue;
        lines << QStringLiteral("ÇIKTIÖĞE islem=tasi yerlesim=%1 ad=%2 x=%3 y=%4")
                     .arg(quoted(name_), quoted(id), mm_text(to.x, 2), mm_text(to.y, 2));
    }
    if (lines.isEmpty()) return;
    controller_.runLines(lines, tr("dağıt"), command::Origin::Gui);
    refresh();
}

void LayoutDesigner::restackPicked(const QString& to)
{
    const core::Layout* l = layout();
    if (l == nullptr || canvas_->selection().isEmpty()) return;
    std::int32_t top    = 0;
    std::int32_t bottom = 0;
    for (const core::LayoutItem& other : l->items) {
        top    = std::max(top, other.z);
        bottom = std::min(bottom, other.z);
    }
    QStringList lines;
    int offset = 0;
    for (const QString& id : canvas_->selection()) {
        const core::LayoutItem* item = l->find(id.toStdString());
        if (item == nullptr) continue;
        std::int32_t z = item->z;
        // THE ENDS OF THE STACK are computed from what is there, and several
        // picks keep their own order among themselves.
        if (to == QStringLiteral("on")) z = std::min(top + 1 + offset, 1000);
        if (to == QStringLiteral("back")) z = std::max(bottom - 1 - offset, -1000);
        if (to == QStringLiteral("up")) z = std::min(item->z + 1, 1000);
        if (to == QStringLiteral("down")) z = std::max(item->z - 1, -1000);
        ++offset;
        lines << QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 sira=%3")
                     .arg(quoted(name_), quoted(id))
                     .arg(z);
    }
    controller_.runLines(lines, tr("sırala"), command::Origin::Gui);
    refresh();
}

void LayoutDesigner::duplicatePicked()
{
    const core::Layout* l = layout();
    if (l == nullptr || canvas_->selection().isEmpty()) return;
    const std::size_t before = l->items.size();
    QStringList lines;
    for (const QString& id : canvas_->selection())
        lines << QStringLiteral("ÇIKTIÖĞE islem=cogalt yerlesim=%1 ad=%2")
                     .arg(quoted(name_), quoted(id));
    controller_.runLines(lines, tr("çoğalt"), command::Origin::Gui);

    // THE COPIES ARE PICKED, so the next drag moves them and not the originals.
    QStringList copies;
    if (const core::Layout* after = layout(); after != nullptr)
        for (std::size_t i = before; i < after->items.size(); ++i)
            copies << QString::fromStdString(after->items[i].id);
    canvas_->setSelection(copies);
    refresh();
}

void LayoutDesigner::toggleLockPicked()
{
    const core::Layout* l = layout();
    if (l == nullptr || canvas_->selection().isEmpty()) return;
    bool any_open = false;
    for (const QString& id : canvas_->selection())
        if (const core::LayoutItem* item = l->find(id.toStdString());
            item != nullptr && !item->locked)
            any_open = true;
    editAll(any_open ? QStringLiteral("kilit=evet") : QStringLiteral("kilit=hayir"),
            any_open ? tr("kilitle") : tr("kilidi aç"));
}

void LayoutDesigner::deletePicked()
{
    if (canvas_->selection().isEmpty()) return;
    QStringList lines;
    for (const QString& id : canvas_->selection())
        lines << QStringLiteral("ÇIKTIÖĞE islem=sil yerlesim=%1 ad=%2")
                     .arg(quoted(name_), quoted(id));
    controller_.runLines(lines, tr("sil"), command::Origin::Gui);
    canvas_->setSelection({});
    refresh();
}

QMenu* LayoutDesigner::itemMenu()
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("En öne getir")), &QAction::triggered, this,
            [this] { restackPicked(QStringLiteral("on")); });
    connect(menu->addAction(tr("Bir öne")), &QAction::triggered, this,
            [this] { restackPicked(QStringLiteral("up")); });
    connect(menu->addAction(tr("Bir arkaya")), &QAction::triggered, this,
            [this] { restackPicked(QStringLiteral("down")); });
    connect(menu->addAction(tr("En arkaya gönder")), &QAction::triggered, this,
            [this] { restackPicked(QStringLiteral("back")); });
    menu->addSeparator();
    connect(menu->addAction(tr("Çoğalt\tCtrl+D")), &QAction::triggered, this,
            &LayoutDesigner::duplicatePicked);
    const core::Layout* l = layout();
    bool any_open         = false;
    if (l != nullptr)
        for (const QString& id : canvas_->selection())
            if (const core::LayoutItem* item = l->find(id.toStdString());
                item != nullptr && !item->locked)
                any_open = true;
    connect(menu->addAction(any_open ? tr("Kilitle\tCtrl+L") : tr("Kilidi aç\tCtrl+L")),
            &QAction::triggered, this, &LayoutDesigner::toggleLockPicked);
    menu->addSeparator();
    connect(menu->addAction(tr("Sil\tDelete")), &QAction::triggered, this,
            &LayoutDesigner::deletePicked);
    return menu;
}

// ------------------------------------------------------------- the refresh --

void LayoutDesigner::refresh()
{
    const core::Layout* l = layout();
    if (l == nullptr) {
        if (hint_ != nullptr) hint_->setText(tr("Çıktı yerleşimi silinmiş: %1").arg(name_));
        return;
    }
    canvas_->refresh();

    filling_ = true;
    refreshItems(*l);
    filling_ = false;

    if (canvas_->selection().isEmpty()) pane_ = Pane::Sheet;
    buildProperties();
    refreshToolRow(*l);
    refreshStatus(*l);
}

void LayoutDesigner::refreshItems(const core::Layout& l)
{
    // PAINT ORDER, TOP FIRST: the list reads the way the sheet looks, and only
    // this page's items are in it — a row for something on another page would
    // pick a box that is not on the paper in front of the user.
    items_->clear();
    std::vector<std::size_t> ordered(l.items.size());
    std::iota(ordered.begin(), ordered.end(), std::size_t{0});
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&](std::size_t a, std::size_t b) { return l.items[a].z > l.items[b].z; });
    const QStringList picked = canvas_->selection();
    std::size_t here         = 0;
    for (const std::size_t at : ordered) {
        if (l.page_of(at) != canvas_->page()) continue;
        ++here;
        const core::LayoutItem& item = l.items[at];
        const QString id             = QString::fromStdString(item.id);
        auto* row                    = new QListWidgetItem(item_name(item), items_);
        row->setData(Qt::UserRole, id);
        row->setData(Qt::UserRole + 1, static_cast<int>(glyph_of(item.kind)));
        row->setData(Qt::UserRole + 2, QStringLiteral("%1×%2").arg(mm_text(item.frame.w, 0),
                                                                   mm_text(item.frame.h, 0)));
        row->setData(Qt::UserRole + 3, item.locked);
        // THE ID IS STILL REACHABLE, because it is what a command line takes —
        // it is just no longer the first thing a reader has to step over.
        row->setToolTip(tr("%1 · %2%3")
                            .arg(id, QString::fromUtf8(core::layout_item_kind_label(item.kind)),
                                 item.locked ? tr(" · kilitli") : QString()));
        if (picked.contains(id)) row->setSelected(true);
        if (id == canvas_->selected()) items_->setCurrentItem(row, QItemSelectionModel::NoUpdate);
    }
    if (itemsCount_ != nullptr)
        itemsCount_->setText(here == 0 ? tr("bu sayfada öğe yok") : tr("%1 öğe").arg(here));
}

void LayoutDesigner::refreshToolRow(const core::Layout& l)
{
    const QStringList picked = canvas_->selection();
    int open                 = 0;
    for (const QString& id : picked)
        if (const core::LayoutItem* item = l.find(id.toStdString());
            item != nullptr && !item->locked)
            ++open;

    // WHAT THE PICK ALLOWS, said by the buttons themselves: a greyed align is
    // an answer, a live one that does nothing when pressed is a question.
    for (Button* b : needOne_)
        b->setEnabled(!picked.isEmpty());
    for (Button* b : needThree_)
        b->setEnabled(picked.size() >= 3);

    // THE PADLOCK SAYS WHAT IS, pressed while every picked item is locked, so
    // a click reads as the switch it is.
    if (lockButton_ != nullptr) {
        const bool all_locked = !picked.isEmpty() && open == 0;
        lockButton_->setCheckable(true);
        lockButton_->setChecked(all_locked);
        const QString said = all_locked ? tr("Kilidi aç (Ctrl+L)") : tr("Kilitle (Ctrl+L)");
        lockButton_->setToolTip(said);
        lockButton_->setAccessibleName(said);
    }

    const int count = static_cast<int>(l.pages.size());
    const int at    = std::clamp(canvas_->page(), 0, std::max(0, count - 1));
    if (pageMenu_ != nullptr) pageMenu_->setText(tr("Sayfa %1 / %2").arg(at + 1).arg(count));
    if (pagePrev_ != nullptr) pagePrev_->setEnabled(at > 0);
    if (pageNext_ != nullptr) pageNext_->setEnabled(at + 1 < count);
    if (zoomReadout_ != nullptr)
        zoomReadout_->setText(QStringLiteral("%%1").arg(std::lround(canvas_->zoomPercent())));
}

void LayoutDesigner::refreshStatus(const core::Layout& l)
{
    if (pickReadout_ != nullptr) {
        const QStringList picked = canvas_->selection();
        if (picked.isEmpty()) {
            const core::LayoutPage* page = canvas_->activePage();
            pickReadout_->setText(page == nullptr ? QString()
                                                  : tr("Sayfa %1 · %2 × %3 mm")
                                                        .arg(canvas_->page() + 1)
                                                        .arg(page->w / 1000)
                                                        .arg(page->h / 1000));
        } else if (picked.size() == 1) {
            const core::LayoutItem* item = l.find(picked.front().toStdString());
            pickReadout_->setText(
                item == nullptr
                    ? QString()
                    : tr("%1 · %2 × %3 mm")
                          .arg(item_name(*item), mm_shown(item->frame.w), mm_shown(item->frame.h)));
        } else {
            pickReadout_->setText(tr("%1 öğe seçili").arg(picked.size()));
        }
    }
    if (troubleReadout_ != nullptr) {
        const std::vector<std::string> trouble = core::layout_trouble(l, controller_.document());
        troubleReadout_->setVisible(!trouble.empty());
        troubleReadout_->setText(trouble.size() == 1 ? tr("1 uyarı")
                                                     : tr("%1 uyarı").arg(trouble.size()));
        troubleReadout_->setToolTip(tr("Denetim — ayrıntı Sayfa sekmesinde"));
    }
    if (hint_ != nullptr) {
        if (!canvas_->drawKind().isEmpty())
            hint_->setText(tr("Kâğıtta sürükleyerek çizin; tıklamak öntanımlı boyda koyar."));
        else if (canvas_->selection().isEmpty())
            hint_->setText(tr("Tıklayarak seçin ya da boş kâğıtta çerçeve çekin."));
        else
            hint_->setText(tr("Sürükleyin ya da köşeden boyutlandırın; oklar 1 mm kaydırır."));
    }
}

// ------------------------------------------------------- the inspector rows --

void LayoutDesigner::group(const QString& title, const QString& note)
{
    auto* heading = new QWidget(properties_);
    auto* line    = new QHBoxLayout(heading);
    // AIR ABOVE A HEADING, NONE BELOW: the rows under it are its, the rows
    // above are someone else's.
    line->setContentsMargins(0, propertyColumn_->count() == 0 ? 2 : 14, 0, 2);
    line->setSpacing(8);
    auto* caption = new QLabel(title, heading);
    caption->setObjectName(QStringLiteral("groupCaption"));
    line->addWidget(caption);
    line->addStretch(1);
    if (!note.isEmpty()) {
        auto* said = new QLabel(note, heading);
        said->setObjectName(QStringLiteral("formHelp"));
        line->addWidget(said);
    }
    propertyColumn_->addWidget(heading);
}

QWidget* LayoutDesigner::row(const QString& caption, QWidget* editor)
{
    auto* cell = new QWidget(properties_);
    auto* line = new QHBoxLayout(cell);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(6);

    // ONE LINE HIGH AND AT THE TOP: beside an editor of two stacked buttons
    // or a list of ticks, the caption names the first line, not the middle of
    // the stack.
    auto* label = new QLabel(caption, cell);
    label->setObjectName(QStringLiteral("formCaption"));
    label->setFixedSize(kCaption, static_cast<int>(ControlSize::Regular));
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setBuddy(editor);
    line->addWidget(label, 0, Qt::AlignTop);

    // THE ROW NEVER OUTGROWS THE COLUMN. Left to Qt, an editor's minimum is
    // its widest text, and the scroll area answers a row wider than its
    // viewport by widening the page — with no horizontal bar, the right end of
    // every row then disappears under the column's edge. An explicit minimum
    // overrides the hint and the row takes what the column has.
    //
    // REPARENTED AND SHOWN: an editor is built on the column before its row
    // exists, and a widget moved to a new parent is hidden by the move —
    // shown, it comes up with its row.
    editor->setParent(cell);
    editor->setMinimumWidth(1);
    editor->show();
    line->addWidget(editor, 1);
    propertyColumn_->addWidget(cell);
    return cell;
}

void LayoutDesigner::help(const QString& text)
{
    // UNDER THE VALUE IT EXPLAINS, BY A LAYOUT'S MARGIN and not the label's
    // own. A `QLabel` is a `QFrame`, and a frame works its contents margins out
    // again from its frame widths whenever the style reaches it while it is
    // narrower than those margins — which every label is before its first
    // layout pass. The indent was lost at the first polish, and every help
    // line sat under the captions instead.
    auto* holder = new QWidget(properties_);
    auto* line   = new QHBoxLayout(holder);
    line->setContentsMargins(kCaption + 6, 0, 0, 2);
    auto* said = new QLabel(text, holder);
    said->setObjectName(QStringLiteral("formHelp"));
    said->setWordWrap(true);
    line->addWidget(said);
    propertyColumn_->addWidget(holder);
}

QWidget* LayoutDesigner::mmEditor(core::Um value, const char* name, const QString& spoken,
                                  int decimals, const Writer& write)
{
    FieldSpec spec = decimal_of(decimals);
    spec.suffix    = QStringLiteral("mm");
    auto* field    = new Field(spec, properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    const QString shown = mm_text(value, decimals);
    field->setValue(shown);
    field->setAccessibleName(spoken);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this,
            [this, key, decimals, shown, write](const QString& typed) {
                if (filling_) return;
                QString number = typed.trimmed();
                number.replace(QLatin1Char(','), QLatin1Char('.'));
                bool ok            = false;
                const double value = number.toDouble(&ok);
                const QString said = QString::number(value, 'f', decimals);
                // A FIELD LEFT AS IT WAS WRITES NOTHING: a visit is not an edit,
                // and one that ran a command put a no-op on the undo stack.
                if (!ok || said == shown) return;
                const QString line = QStringLiteral("%1=%2").arg(key, said);
                write ? write(line) : edit(line);
            });
    return field;
}

QWidget* LayoutDesigner::countEditor(long long value, const char* name, const QString& spoken,
                                     int least, int most, const QString& unit, const Writer& write)
{
    auto* field = new Field(number_of(least, most, unit), properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    field->setValue(QString::number(value));
    field->setAccessibleName(spoken);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key, value, write](const QString& typed) {
        if (filling_) return;
        bool ok             = false;
        const long long got = typed.trimmed().toLongLong(&ok);
        if (!ok || got == value) return;
        const QString line = QStringLiteral("%1=%2").arg(key).arg(got);
        write ? write(line) : edit(line);
    });
    return field;
}

QWidget* LayoutDesigner::textEditor(const QString& value, const char* name, const QString& spoken,
                                    const QString& hint, const Writer& write)
{
    FieldSpec spec   = field_of(FieldKind::Text);
    spec.placeholder = hint;
    auto* field      = new Field(spec, properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    field->setValue(value);
    field->setAccessibleName(spoken);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key, value, write](const QString& typed) {
        if (filling_ || typed == value) return;
        const QString line = QStringLiteral("%1=%2").arg(key, quoted(typed));
        write ? write(line) : edit(line);
    });
    return field;
}

QWidget* LayoutDesigner::colourEditor(std::uint32_t value, const char* name, const QString& spoken,
                                      const Writer& write)
{
    auto* field = new Field(field_of(FieldKind::Colour), properties_);
    field->setFixedHeight(static_cast<int>(ControlSize::Regular));
    // THE FIELD'S OWN FORMAT, `0xAARRGGBB` (`fields.hpp`) — hex digits in
    // capitals, the prefix not: the old line upper-cased the whole string and
    // the field showed `0XFF000000`.
    field->setValue(QStringLiteral("0x%1").arg(
        QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0')).toUpper()));
    field->setAccessibleName(spoken);
    const QString key = QString::fromUtf8(name);
    connect(field, &Field::committed, this, [this, key, value, write](const QString& typed) {
        if (filling_) return;
        bool ok                 = false;
        const std::uint32_t got = typed.mid(2).toUInt(&ok, 16);
        if (!ok || got == value) return;
        const QString line = QStringLiteral("%1=%2").arg(key, colour_word(got));
        write ? write(line) : edit(line);
    });
    return field;
}

QWidget* LayoutDesigner::switchEditor(bool on, const char* name, const QString& spoken,
                                      const Writer& write)
{
    // A SWITCH AND ITS WORD, left-aligned in the editor column: the pill says
    // the state by its knob's side and the word says it in text (§13).
    auto* holder = new QWidget(properties_);
    holder->setFixedHeight(static_cast<int>(ControlSize::Regular));
    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(8);
    auto* pill = new ToggleSwitch(holder);
    pill->setChecked(on);
    pill->setAccessibleName(spoken);
    auto* word = new QLabel(on ? tr("açık") : tr("kapalı"), holder);
    word->setObjectName(QStringLiteral("formHelp"));
    line->addWidget(pill);
    line->addWidget(word);
    line->addStretch(1);
    const QString key = QString::fromUtf8(name);
    connect(pill, &QAbstractButton::toggled, this, [this, key, write](bool checked) {
        if (filling_) return;
        const QString line = QStringLiteral("%1=%2").arg(key, checked ? QStringLiteral("evet")
                                                                      : QStringLiteral("hayir"));
        write ? write(line) : edit(line);
    });
    return holder;
}

QWidget* LayoutDesigner::wordsEditor(const QStringList& shown, const QStringList& words,
                                     int current, const char* name, const QString& spoken,
                                     const Writer& write)
{
    auto* choice = new Segment(properties_);
    choice->setControlSize(ControlSize::Compact);
    for (const QString& one : shown)
        choice->addOption(one);
    choice->setCurrent(current);
    choice->setAccessibleName(spoken);
    const QString key = QString::fromUtf8(name);
    connect(choice, &Segment::currentChanged, this, [this, key, words, write](int at) {
        if (filling_ || at < 0 || at >= words.size()) return;
        const QString line = QStringLiteral("%1=%2").arg(key, words.at(at));
        write ? write(line) : edit(line);
    });
    return choice;
}

QWidget* LayoutDesigner::mapEditor(const core::Layout& l, const core::LayoutItem& item)
{
    // WHICH MAP FRAME IT BELONGS TO. A scale bar, a north arrow and a legend all
    // describe one map; a sheet with two frames had no way to say which, so the
    // second frame's scale bar quietly described the first.
    auto* bound = new ComboBox(properties_);
    bound->addItem(tr("İlk harita"), QStringLiteral("ilk"));
    for (const core::LayoutItem& other : l.items)
        if (other.kind == core::LayoutItemKind::Map)
            bound->addItem(QString::fromStdString(other.id), QString::fromStdString(other.id));
    const int chosen = bound->findData(QString::fromStdString(item.linked_map));
    bound->setCurrentIndex(chosen >= 0 ? chosen : 0);
    bound->setAccessibleName(tr("Bağlı harita"));
    connect(bound, &QComboBox::currentIndexChanged, this, [this, bound](int at) {
        if (filling_ || at < 0) return;
        edit(QStringLiteral("harita=%1").arg(quoted(bound->itemData(at).toString())));
    });
    return bound;
}

// ----------------------------------------------------------- the inspector --

void LayoutDesigner::buildProperties()
{
    const core::Layout* l = layout();
    if (l == nullptr || propertyColumn_ == nullptr) return;
    const QStringList picked = canvas_->selection();
    const core::LayoutItem* item =
        picked.size() == 1 ? l->find(picked.front().toStdString()) : nullptr;
    if (picked.isEmpty()) pane_ = Pane::Sheet;

    // THE SAME SUBJECT KEEPS ITS PLACE. Every committed value rebuilds the
    // column, and a rebuild that went back to the top threw the user out of
    // the grid settings at the bottom after every number they typed; and the
    // field the keyboard was going to next is found again by its name.
    const QString subject = pane_ == Pane::Sheet
                                ? QStringLiteral("sayfa:%1").arg(canvas_->page())
                                : QStringLiteral("öğe:%1").arg(picked.join(QLatin1Char('|')));
    const bool same       = subject == shownFor_;
    if (!same) tableColumn_ = 0; ///< another table starts at its first column
    const int keep = same && scroll_ != nullptr ? scroll_->verticalScrollBar()->value() : 0;
    QString focused;
    for (QWidget* w = QApplication::focusWidget(); w != nullptr; w = w->parentWidget())
        if (auto* field = qobject_cast<Field*>(w);
            field != nullptr && properties_->isAncestorOf(field)) {
            focused = field->accessibleName();
            break;
        }

    // Cleared and rebuilt, rather than kept and reconciled: the fields a Map
    // shows and the fields a Label shows have nothing in common past the frame.
    //
    // HIDDEN, UNPARENTED, THEN DELETED LATER — and each step is a bug it once
    // was. `takeAt` removes a row from the LAYOUT at once, but `deleteLater`
    // leaves it a visible CHILD until the loop next spins, painting under the
    // new rows; so it is unparented. And a row built a moment ago still has
    // the show its layout QUEUED for it (`QLayout::addChildWidget`): unparented
    // without being hidden first, that queued show ran on a widget with no
    // parent and opened it as a window of its own, which took the activation
    // from this one. An explicit `hide` is the one state the queued show
    // respects. Not `delete`: a field's own `committed` is what rebuilds this
    // column, and deleting it inside its own signal is a use after free.
    while (QLayoutItem* old = propertyColumn_->takeAt(0)) {
        if (QWidget* w = old->widget(); w != nullptr) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete old;
    }

    filling_ = true;
    if (paneSwitch_ != nullptr) paneSwitch_->setCurrent(pane_ == Pane::Item ? 0 : 1);
    const Tokens& t = tokensOf(theme());
    const qreal dpr = devicePixelRatioF();
    if (pane_ == Pane::Sheet) {
        buildSheetProperties(*l);
        if (headGlyph_ != nullptr)
            headGlyph_->setPixmap(glyph_pixmap(Glyph::Document, t.textDim, 24, dpr));
    } else if (item == nullptr) {
        buildGroupProperties(*l);
        if (headGlyph_ != nullptr)
            headGlyph_->setPixmap(glyph_pixmap(Glyph::SelectArea, t.accent, 24, dpr));
    } else {
        buildItemProperties(*l, *item);
        if (headGlyph_ != nullptr)
            headGlyph_->setPixmap(glyph_pixmap(glyph_of(item->kind), t.accent, 24, dpr));
    }
    propertyColumn_->addStretch(1);
    filling_ = false;

    // AND THE NEW ROWS ARE TOLD WHICH THEME THEY ARE IN: `DialogFrame::applyTheme`
    // walked the children once, when the window was built, and everything here is
    // built again on every pick.
    applyThemeToChildren(properties_, theme());

    // SHOWN NOW, not when the loop gets round to the show their layout queued:
    // the column is painted, measured and scrolled below, and a row that is
    // still hidden is a row with no height.
    for (QWidget* w : properties_->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
        if (w->isHidden() && !w->testAttribute(Qt::WA_WState_ExplicitShowHide)) w->show();

    shownFor_ = subject;
    if (scroll_ != nullptr) {
        propertyColumn_->activate();
        properties_->resize(scroll_->viewport()->width(), std::max(properties_->sizeHint().height(),
                                                                   scroll_->viewport()->height()));
        scroll_->verticalScrollBar()->setValue(keep);
    }
    if (!focused.isEmpty())
        for (Field* field : properties_->findChildren<Field*>())
            if (field->accessibleName() == focused) {
                field->setFocus(Qt::TabFocusReason);
                break;
            }
}

/// THE SHEET'S OWN SETTINGS, its pages, and what the checks found on it.
void LayoutDesigner::buildSheetProperties(const core::Layout& l)
{
    const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l.pages.size()) - 1);
    const core::LayoutPage& page = l.pages[static_cast<std::size_t>(shown)];

    if (headName_ != nullptr) headName_->setText(QString::fromStdString(l.name));
    if (headKind_ != nullptr)
        headKind_->setText(tr("Sayfa %1 / %2 · %3 × %4 mm")
                               .arg(shown + 1)
                               .arg(l.pages.size())
                               .arg(page.w / 1000)
                               .arg(page.h / 1000));

    group(tr("KÂĞIT"), l.pages.size() > 1 ? tr("bu sayfa") : QString());

    // WHICH PAPER. The command takes the six ISO sizes and `ozel`; a sheet that
    // came from a custom size shows it and keeps it until the user picks another.
    auto* paper = new ComboBox(properties_);
    for (const char* one : {"A5", "A4", "A3", "A2", "A1", "A0"})
        paper->addItem(QString::fromUtf8(one), QString::fromUtf8(one));
    paper->addItem(tr("Özel"), QStringLiteral("ozel"));
    const int at = paper->findData(QString::fromStdString(l.paper));
    paper->setCurrentIndex(at >= 0 ? at : paper->count() - 1);
    paper->setAccessibleName(tr("Kâğıt boyu"));
    connect(paper, &QComboBox::currentIndexChanged, this, [this, paper](int chosen) {
        if (filling_ || chosen < 0) return;
        sheetEdit(QStringLiteral("kagit=%1").arg(paper->itemData(chosen).toString()));
    });
    row(tr("Boy"), paper);

    // READ FROM THE PAGE, not from the layout's flag: on a multi-page sheet the
    // flag describes the sheet as a whole and a page may disagree with it.
    auto* facing = new Segment(properties_);
    facing->setControlSize(ControlSize::Compact);
    facing->addOption(tr("Dikey"));
    facing->addOption(tr("Yatay"));
    facing->setCurrent(page.w > page.h ? 1 : 0);
    facing->setAccessibleName(tr("Yön"));
    connect(facing, &Segment::currentChanged, this, [this](int chosen) {
        if (filling_) return;
        sheetEdit(QStringLiteral("yon=%1").arg(chosen == 1 ? QStringLiteral("yatay")
                                                           : QStringLiteral("dikey")));
    });
    row(tr("Yön"), facing);

    auto* edge = new Field(number_of(0, 200, QStringLiteral("mm")), properties_);
    edge->setFixedHeight(static_cast<int>(ControlSize::Regular));
    edge->setValue(QString::number(l.margin / 1000));
    edge->setAccessibleName(tr("Kenar payı"));
    const long long margin = l.margin / 1000;
    connect(edge, &Field::committed, this, [this, margin](const QString& typed) {
        bool ok             = false;
        const long long got = typed.trimmed().toLongLong(&ok);
        if (filling_ || !ok || got == margin) return;
        sheetEdit(QStringLiteral("kenar=%1").arg(got));
    });
    row(tr("Kenar payı"), edge);
    help(tr("Kılavuz çizgisidir; çıktıyı kırpmaz."));

    auto* dpi = new Field(number_of(72, 4800, QStringLiteral("dpi")), properties_);
    dpi->setFixedHeight(static_cast<int>(ControlSize::Regular));
    dpi->setValue(QString::number(l.dpi));
    dpi->setAccessibleName(tr("Çözünürlük"));
    const long long was_dpi = l.dpi;
    connect(dpi, &Field::committed, this, [this, was_dpi](const QString& typed) {
        bool ok             = false;
        const long long got = typed.trimmed().toLongLong(&ok);
        if (filling_ || !ok || got == was_dpi) return;
        sheetEdit(QStringLiteral("dpi=%1").arg(got));
    });
    row(tr("Çözünürlük"), dpi);

    // ---- the pages ----------------------------------------------------------
    group(tr("SAYFALAR"), l.pages.size() == 1 ? tr("1 sayfa") : tr("%1 sayfa").arg(l.pages.size()));
    // THE WHOLE WIDTH, not the editor column: three labelled buttons do not
    // fit in 190 px, and cut to `Ekl · Çoğ · Si` they said nothing at all.
    auto* verbs = new QWidget(properties_);
    auto* line  = new QHBoxLayout(verbs);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(6);
    auto* add = new Button(ButtonRole::Secondary, tr("Ekle"), Glyph::Plus, verbs);
    add->setControlSize(ControlSize::Compact);
    add->setToolTip(tr("Bu sayfanın ardına boş bir sayfa ekler"));
    connect(add, &QPushButton::clicked, this, [this] { pageVerb("sayfaekle"); });
    auto* twin = new Button(ButtonRole::Secondary, tr("Çoğalt"), Glyph::Duplicate, verbs);
    twin->setControlSize(ControlSize::Compact);
    twin->setToolTip(tr("Bu sayfayı öğeleriyle birlikte kopyalar"));
    connect(twin, &QPushButton::clicked, this, [this] { pageVerb("sayfacogalt"); });
    auto* drop = new Button(ButtonRole::Secondary, tr("Sil"), Glyph::Trash, verbs);
    drop->setControlSize(ControlSize::Compact);
    drop->setEnabled(l.pages.size() > 1);
    drop->setToolTip(l.pages.size() > 1 ? tr("Bu sayfayı öğeleriyle siler; geri alınabilir")
                                        : tr("Yerleşimin tek sayfası silinemez"));
    connect(drop, &QPushButton::clicked, this, [this] { pageVerb("sayfasil"); });
    line->addWidget(add, 1);
    line->addWidget(twin, 1);
    line->addWidget(drop, 1);
    propertyColumn_->addWidget(verbs);

    // ---- the layout ---------------------------------------------------------
    group(tr("YERLEŞİM"));
    auto* named = new Field(field_of(FieldKind::Text), properties_);
    named->setFixedHeight(static_cast<int>(ControlSize::Regular));
    named->setValue(QString::fromStdString(l.name));
    named->setAccessibleName(tr("Yerleşim adı"));
    connect(named, &Field::committed, this, [this](const QString& typed) {
        if (filling_ || typed.trimmed().isEmpty() || typed.trimmed() == name_) return;
        controller_.runLine(QStringLiteral("ÇIKTIYERLEŞİMİ islem=ad ad=%1 yeni_ad=%2")
                                .arg(quoted(name_), quoted(typed.trimmed())),
                            command::Origin::Gui);
        // THE WINDOW FOLLOWS THE RENAME. It holds the sheet BY NAME, so a
        // designer that kept the old one would be looking at a layout that no
        // longer exists and would report it as deleted on the next refresh.
        if (controller_.document().layouts().find(typed.trimmed().toStdString()) == nullptr) return;
        name_ = typed.trimmed();
        canvas_->setSheet(name_, canvas_->page());
        setHeading(Glyph::Layout, tr("Çıktı Yerleşimi Tasarımcısı"),
                   QStringLiteral("— %1").arg(name_));
        refresh();
    });
    row(tr("Ad"), named);

    // ---- what the checks found ----------------------------------------------
    //
    // THE SAME CHECKS `ÇIKTIYERLEŞİMİ islem=denetle` RUNS, here where the sheet
    // is being made: overlapping boxes, a scale bar bound to a map that is gone,
    // a table naming a column the drawing does not have.
    const std::vector<std::string> trouble = core::layout_trouble(l, controller_.document());
    group(tr("DENETİM"), trouble.empty() ? tr("sorun yok") : tr("%1 uyarı").arg(trouble.size()));
    if (trouble.empty()) {
        auto* fine = new QLabel(tr("Her bağ bir öğeye varıyor; tablo ve grafiklerin sütunları "
                                   "çizimde var."),
                                properties_);
        fine->setObjectName(QStringLiteral("formHelp"));
        fine->setWordWrap(true);
        propertyColumn_->addWidget(fine);
    } else {
        // ONE WRAPPED LINE PER FINDING, with the warning's mark beside it. They
        // were banners carrying the finding as their TITLE, and a banner's
        // title does not wrap: the first long one — an unaimed map frame's
        // `… nereye bakacağı söylenmemiş; boş çıkacak.` — made the column wider
        // than itself and the whole page of settings ran out past its edge.
        const Tokens& t = tokensOf(theme());
        for (const std::string& one : trouble) {
            auto* finding = new QWidget(properties_);
            auto* beside  = new QHBoxLayout(finding);
            beside->setContentsMargins(0, 2, 0, 2);
            beside->setSpacing(8);
            auto* mark = new QLabel(finding);
            mark->setPixmap(glyph_pixmap(Glyph::Warning, t.warn, 14, devicePixelRatioF()));
            mark->setFixedSize(14, 16);
            beside->addWidget(mark, 0, Qt::AlignTop);
            auto* said = new QLabel(QString::fromStdString(one), finding);
            said->setObjectName(QStringLiteral("formHelp"));
            said->setProperty("tone", QStringLiteral("warn"));
            said->setWordWrap(true);
            said->setMinimumWidth(1);
            beside->addWidget(said, 1);
            propertyColumn_->addWidget(finding);
        }
    }
}

/// WHAT SEVERAL PICKS SHARE: the list of them and the gestures for all of them.
void LayoutDesigner::buildGroupProperties(const core::Layout& l)
{
    const QStringList picked = canvas_->selection();
    std::vector<ItemFrame> frames;
    for (const QString& id : picked)
        if (const core::LayoutItem* item = l.find(id.toStdString()); item != nullptr)
            frames.push_back(ItemFrame{id, item->frame});
    const core::PaperRect bounds = union_of(frames);
    if (headName_ != nullptr) headName_->setText(tr("%1 öğe seçili").arg(picked.size()));
    if (headKind_ != nullptr)
        headKind_->setText(tr("birlikte %1 × %2 mm").arg(mm_shown(bounds.w), mm_shown(bounds.h)));

    group(tr("SEÇİLENLER"));
    for (const QString& id : picked)
        if (const core::LayoutItem* item = l.find(id.toStdString()); item != nullptr) {
            auto* one = new QLabel(tr("%1 · %2 × %3 mm%4")
                                       .arg(item_name(*item), mm_shown(item->frame.w),
                                            mm_shown(item->frame.h),
                                            item->locked ? tr(" · kilitli") : QString()),
                                   properties_);
            one->setObjectName(QStringLiteral("formCaption"));
            propertyColumn_->addWidget(one);
        }
    auto* said = new QLabel(tr("Hizalamak, dağıtmak, sıralamak, çoğaltmak ve kilitlemek için "
                               "araç satırını kullanın; hepsi seçilenlerin tümüne uygulanır "
                               "ve tek adımda geri alınır."),
                            properties_);
    said->setObjectName(QStringLiteral("formHelp"));
    said->setWordWrap(true);
    propertyColumn_->addWidget(said);
}

/// EVERY SETTING THE COMMAND TAKES, for the item that is picked, in the order a
/// person decides them: where it sits, what it shows, how it looks.
void LayoutDesigner::buildItemProperties(const core::Layout& l, const core::LayoutItem& item)
{
    using core::LayoutItemKind;

    const QString shownAs  = item_name(item);
    const QString kindWord = QString::fromUtf8(core::layout_item_kind_label(item.kind));
    if (headName_ != nullptr) headName_->setText(shownAs);
    if (headKind_ != nullptr) {
        // THE KIND IS SAID ONCE. An item with no writing of its own is CALLED by
        // its kind, so repeating it underneath reads as `Harita · Harita`.
        const QString size = tr("%1 · %2 × %3 mm")
                                 .arg(QString::fromStdString(item.id), mm_shown(item.frame.w),
                                      mm_shown(item.frame.h));
        headKind_->setText(shownAs == kindWord ? size : kindWord + QStringLiteral(" · ") + size);
    }

    // ---- where it sits -----------------------------------------------------
    group(tr("KONUM VE BOYUT"), item.locked ? tr("kilitli — taşınmaz") : QString());
    row(tr("X — soldan"), mmEditor(item.frame.x, "x", tr("X, kâğıdın solundan")));
    row(tr("Y — üstten"), mmEditor(item.frame.y, "y", tr("Y, kâğıdın üstünden")));
    row(tr("Genişlik"), mmEditor(item.frame.w, "genislik", tr("Genişlik")));
    row(tr("Yükseklik"), mmEditor(item.frame.h, "yukseklik", tr("Yükseklik")));

    FieldSpec turnSpec = decimal_of(1);
    turnSpec.suffix    = QStringLiteral("°");
    auto* turn         = new Field(turnSpec, properties_);
    turn->setFixedHeight(static_cast<int>(ControlSize::Regular));
    const QString turned =
        QString::number(static_cast<double>(item.rotation_udeg) / 1000000.0, 'f', 1);
    turn->setValue(turned);
    turn->setAccessibleName(tr("Döndürme, saat yönünde"));
    connect(turn, &Field::committed, this, [this, turned](const QString& typed) {
        if (filling_) return;
        QString number = typed.trimmed();
        number.replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok            = false;
        const QString said = QString::number(number.toDouble(&ok), 'f', 1);
        if (!ok || said == turned) return;
        edit(QStringLiteral("aci=%1").arg(said));
    });
    row(tr("Döndürme"), turn);

    if (l.pages.size() > 1) {
        auto* onPage = new Field(number_of(1, static_cast<int>(l.pages.size())), properties_);
        onPage->setFixedHeight(static_cast<int>(ControlSize::Regular));
        onPage->setValue(QString::number(canvas_->page() + 1));
        onPage->setAccessibleName(tr("Sayfa"));
        const int here = canvas_->page() + 1;
        connect(onPage, &Field::committed, this, [this, here](const QString& typed) {
            bool ok       = false;
            const int got = typed.trimmed().toInt(&ok);
            if (filling_ || !ok || got == here) return;
            // A PAGE MOVE IS A MOVE, so it goes through `tasi` like every other one.
            edit(QStringLiteral("sayfa=%1").arg(got), QStringLiteral("tasi"));
        });
        row(tr("Sayfa"), onPage);
    }

    // ---- what it shows ------------------------------------------------------
    const core::Document& doc = controller_.document();
    switch (item.kind) {
    case LayoutItemKind::Map: {
        group(tr("HARİTA"),
              item.scale == 0 ? tr("ölçek çerçeveye uyar") : tr("1:%1").arg(item.scale));

        // THE SCALE, TYPED OR PICKED. 1:1000 is typed far more often than
        // 1:1316, and the menu beside the box holds the round plan scales a
        // pafta is drawn at — faster than four keystrokes and cannot be
        // mistyped. `0` hands the scale back to the frame.
        auto* scaleBox = new QWidget(properties_);
        auto* scaleRow = new QHBoxLayout(scaleBox);
        scaleRow->setContentsMargins(0, 0, 0, 0);
        scaleRow->setSpacing(6);
        auto* scale = new Field(number_of(0, 100000000), properties_);
        scale->setFixedHeight(static_cast<int>(ControlSize::Regular));
        scale->setValue(QString::number(item.scale));
        scale->setAccessibleName(tr("Ölçek paydası; 0 çerçeveye uyar"));
        const long long was_scale = item.scale;
        connect(scale, &Field::committed, this, [this, was_scale](const QString& typed) {
            bool ok             = false;
            const long long got = typed.trimmed().toLongLong(&ok);
            if (filling_ || !ok || got == was_scale) return;
            edit(QStringLiteral("olcek=%1").arg(got));
        });
        auto* presets = new Button(ButtonRole::Secondary, tr("Seç"), std::nullopt, scaleBox);
        presets->setControlSize(ControlSize::Regular);
        presets->setToolTip(tr("Plan ölçekleri"));
        auto* scales = new QMenu(presets);
        connect(scales->addAction(tr("Çerçeveye uyar")), &QAction::triggered, this,
                [this] { edit(QStringLiteral("olcek=0")); });
        scales->addSeparator();
        for (const long long one : kScales)
            connect(scales->addAction(QStringLiteral("1:%1").arg(one)), &QAction::triggered, this,
                    [this, one] { edit(QStringLiteral("olcek=%1").arg(one)); });
        presets->setMenuArrow(scales);
        scale->setParent(scaleBox);
        scale->setMinimumWidth(1);
        scaleRow->addWidget(scale, 1);
        scaleRow->addWidget(presets);
        row(tr("Ölçek 1 :"), scaleBox);

        // WHERE IT LOOKS: the whole drawing, or what the main window shows.
        auto* aims    = new QWidget(properties_);
        auto* aimsRow = new QVBoxLayout(aims);
        aimsRow->setContentsMargins(0, 0, 0, 0);
        aimsRow->setSpacing(6);
        auto* whole =
            new Button(ButtonRole::Secondary, tr("Çizimin tamamı"), Glyph::ZoomExtents, aims);
        whole->setToolTip(tr("Çerçeveyi çizimin tamamını gösterecek biçimde ayarlar"));
        connect(whole, &QPushButton::clicked, this,
                [this] { aimAt(controller_.document().extent()); });
        aimsRow->addWidget(whole);
        if (!viewWindow_.empty()) {
            auto* view =
                new Button(ButtonRole::Secondary, tr("Ana pencereden al"), Glyph::ViewWindow, aims);
            view->setToolTip(tr("Çerçeveyi ana pencerede o an görünen alana çevirir"));
            connect(view, &QPushButton::clicked, this, [this] { aimAt(viewWindow_); });
            aimsRow->addWidget(view);
        }
        row(tr("Kapsam"), aims);

        // WHICH LAYERS IT DRAWS: a list to tick, not names to type. Nothing
        // ticked means every visible layer, which is the common case, and says
        // so with a switch rather than with an empty box.
        group(tr("KATMANLAR"),
              item.layers.empty() ? tr("görünür hepsi") : tr("%1 katman").arg(item.layers.size()));
        auto* all    = new QWidget(properties_);
        auto* allRow = new QHBoxLayout(all);
        allRow->setContentsMargins(0, 0, 0, 0);
        allRow->setSpacing(8);
        auto* every = new ToggleSwitch(all);
        every->setChecked(item.layers.empty());
        every->setAccessibleName(tr("Görünür bütün katmanlar"));
        auto* everyWord = new QLabel(tr("görünür bütün katmanlar"), all);
        everyWord->setObjectName(QStringLiteral("formHelp"));
        allRow->addWidget(every);
        allRow->addWidget(everyWord);
        allRow->addStretch(1);
        all->setFixedHeight(static_cast<int>(ControlSize::Regular));
        connect(every, &QAbstractButton::toggled, this, [this](bool on) {
            if (filling_) return;
            if (on) {
                edit(QStringLiteral("katmanlar=hepsi"));
                return;
            }
            // SWITCHED OFF, THE LIST STARTS AS WHAT THE MAP DREW: every layer
            // that is visible now, each a tick the user can take away.
            QStringList words;
            for (const core::Layer& layer : controller_.document().layers())
                if (layer.visible)
                    words << QStringLiteral("katmanlar=%1")
                                 .arg(quoted(QString::fromStdString(layer.name)));
            if (!words.isEmpty()) edit(words.join(QLatin1Char(' ')));
        });
        row(tr("Hepsi"), all);
        if (!item.layers.empty())
            for (const core::Layer& layer : doc.layers()) {
                const QString name = QString::fromStdString(layer.name);
                auto* tick         = new CheckBox(name, properties_);
                tick->setChecked(std::find(item.layers.begin(), item.layers.end(), layer.name) !=
                                 item.layers.end());
                connect(tick, &QAbstractButton::toggled, this, [this, name](bool on) {
                    if (filling_) return;
                    const core::Layout* sheet = layout();
                    const core::LayoutItem* map =
                        sheet == nullptr ? nullptr : sheet->find(canvas_->selected().toStdString());
                    if (map == nullptr) return;
                    QStringList kept;
                    for (const std::string& one : map->layers)
                        if (QString::fromStdString(one) != name)
                            kept << QString::fromStdString(one);
                    if (on) kept << name;
                    QStringList words;
                    for (const QString& one : kept)
                        words << QStringLiteral("katmanlar=%1").arg(quoted(one));
                    edit(words.isEmpty() ? QStringLiteral("katmanlar=hepsi")
                                         : words.join(QLatin1Char(' ')));
                });
                row(QString(), tick);
            }

        // ---- the grid ------------------------------------------------------
        group(tr("KOORDİNAT IZGARASI"));
        auto* style = new ComboBox(properties_);
        for (const auto& [word, text] :
             {std::pair{"yok", "Yok"}, std::pair{"arti", "Artı — kesişimlerde"},
              std::pair{"cizgi", "Çizgi — tam ızgara"}, std::pair{"centik", "Çentik — kenarda"}})
            style->addItem(tr(text), QString::fromUtf8(word));
        style->setCurrentIndex(static_cast<int>(item.grid));
        style->setAccessibleName(tr("Izgara biçimi"));
        connect(style, &QComboBox::currentIndexChanged, this, [this, style](int at) {
            if (filling_ || at < 0) return;
            edit(QStringLiteral("izgara=%1").arg(style->itemData(at).toString()));
        });
        row(tr("Biçim"), style);
        if (item.grid != core::GridStyle::None) {
            FieldSpec spacing = decimal_of(1);
            spacing.suffix    = QStringLiteral("m");
            auto* step        = new Field(spacing, properties_);
            step->setFixedHeight(static_cast<int>(ControlSize::Regular));
            const QString stepped =
                QString::number(static_cast<double>(item.grid_interval) / 1000.0, 'f', 1);
            step->setValue(stepped);
            step->setAccessibleName(tr("Izgara aralığı, zeminde metre; 0 ölçeğe göre"));
            connect(step, &Field::committed, this, [this, stepped](const QString& typed) {
                if (filling_) return;
                QString number = typed.trimmed();
                number.replace(QLatin1Char(','), QLatin1Char('.'));
                bool ok            = false;
                const double value = number.toDouble(&ok);
                if (!ok || QString::number(value, 'f', 1) == stepped) return;
                edit(QStringLiteral("izgara_aralik=%1").arg(std::llround(value * 1000.0)));
            });
            row(tr("Aralık"), step);
            help(tr("Zeminde metre; 0 ölçeğe uygun bir aralık seçer."));
            row(tr("Yazılar"),
                wordsEditor({tr("Yok"), tr("Dışta"), tr("İçte")},
                            {QStringLiteral("yok"), QStringLiteral("dis"), QStringLiteral("ic")},
                            static_cast<int>(item.grid_labels), "izgara_etiket",
                            tr("Koordinat yazıları")));
            row(tr("Renk"), colourEditor(item.grid_colour, "izgara_renk", tr("Izgara rengi")));
            row(tr("Çizgi kalınlığı"),
                mmEditor(item.grid_width, "izgara_kalinlik", tr("Izgara çizgi kalınlığı"), 2));
            row(tr("Yazı boyu"),
                mmEditor(item.grid_text_height, "izgara_yazi", tr("Koordinat yazısı boyu")));
        }
        break;
    }
    case LayoutItemKind::Label: {
        group(tr("METİN"));
        row(tr("Yazı"), textEditor(QString::fromStdString(item.text), "metin", tr("Yazı"),
                                   tr("yazı ya da <yerlesim>")));

        // THE FIELDS A LABEL CAN CARRY, named and explained in a menu: `<olcek>`
        // is remembered by nobody and misspelled by everybody.
        auto* fields =
            new Button(ButtonRole::Secondary, tr("Alan ekle"), Glyph::Function, properties_);
        fields->setControlSize(ControlSize::Compact);
        fields->setToolTip(tr("Yazının sonuna, çizilirken çözülen bir alan ekler"));
        auto* menu             = new QMenu(fields);
        const std::string text = item.text;
        for (const auto& [word, what] :
             {std::pair{"<yerlesim>", "yerleşimin adı"}, std::pair{"<olcek>", "haritanın ölçeği"},
              std::pair{"<tarih>", "bugünün tarihi"}, std::pair{"<crs>", "koordinat sistemi"},
              std::pair{"<proje>", "çizim dosyasının adı"}, std::pair{"<sayfa>", "sayfa numarası"}})
            connect(
                menu->addAction(QStringLiteral("%1 — %2").arg(QString::fromUtf8(word), tr(what))),
                &QAction::triggered, this, [this, text, w = QString::fromUtf8(word)] {
                    const QString now = QString::fromStdString(text);
                    edit(QStringLiteral("metin=%1")
                             .arg(quoted(now.isEmpty() ? w : now + QLatin1Char(' ') + w)));
                });
        fields->setMenuArrow(menu);
        auto* holder = new QWidget(properties_);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(0, 0, 0, 0);
        fields->setParent(holder);
        line->addWidget(fields);
        line->addStretch(1);
        row(QString(), holder);

        row(tr("Yazı boyu"), mmEditor(item.text_height, "yazi", tr("Yazı boyu")));
        row(tr("Renk"), colourEditor(item.text_colour, "yazi_renk", tr("Yazı rengi")));
        row(tr("Yatay"),
            wordsEditor({tr("Sol"), tr("Orta"), tr("Sağ")},
                        {QStringLiteral("sol"), QStringLiteral("orta"), QStringLiteral("sag")},
                        item.align_h, "yatay_hizala", tr("Yatay hizalama")));
        row(tr("Dikey"),
            wordsEditor({tr("Üst"), tr("Orta"), tr("Alt")},
                        {QStringLiteral("ust"), QStringLiteral("orta"), QStringLiteral("alt")},
                        item.align_v, "dikey_hizala", tr("Dikey hizalama")));
        break;
    }
    case LayoutItemKind::ScaleBar:
        group(tr("ÖLÇEK ÇUBUĞU"));
        row(tr("Harita"), mapEditor(l, item));
        row(tr("Bölüm"),
            countEditor(item.style > 0 ? item.style : 4, "bolum", tr("Bölüm sayısı"), 1, 10));
        row(tr("Yazı boyu"), mmEditor(item.text_height, "yazi", tr("Yazı boyu")));
        row(tr("Renk"), colourEditor(item.text_colour, "yazi_renk", tr("Çubuk ve yazı rengi")));
        help(tr("Çubuk yuvarlak bir uzunluk seçer ve kutusunun içinde kalır."));
        break;
    case LayoutItemKind::NorthArrow:
        group(tr("KUZEY OKU"));
        row(tr("Harita"), mapEditor(l, item));
        row(tr("Renk"), colourEditor(item.text_colour, "yazi_renk", tr("Ok rengi")));
        break;
    case LayoutItemKind::Legend:
        group(tr("LEJANT"));
        row(tr("Başlık"),
            textEditor(QString::fromStdString(item.text), "metin", tr("Başlık"), tr("başlıksız")));
        row(tr("Harita"), mapEditor(l, item));
        row(tr("Yazı boyu"), mmEditor(item.text_height, "yazi", tr("Yazı boyu")));
        row(tr("Renk"), colourEditor(item.text_colour, "yazi_renk", tr("Yazı rengi")));
        break;
    case LayoutItemKind::Picture: {
        group(tr("RESİM"));
        auto* file    = new QWidget(properties_);
        auto* fileRow = new QHBoxLayout(file);
        fileRow->setContentsMargins(0, 0, 0, 0);
        fileRow->setSpacing(6);
        QWidget* path = textEditor(QString::fromStdString(item.text), "metin", tr("Resim dosyası"),
                                   tr("resim yolu"));
        path->setParent(file);
        path->setMinimumWidth(1);
        auto* browse = new Button(Glyph::Open, tr("Resim seçin…"), file);
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString chosen =
                QFileDialog::getOpenFileName(this, tr("Resim seçin"), QString(),
                                             tr("Resimler (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
            if (!chosen.isEmpty()) edit(QStringLiteral("metin=%1").arg(quoted(chosen)));
        });
        fileRow->addWidget(path, 1);
        fileRow->addWidget(browse);
        row(tr("Dosya"), file);
        help(tr("Çizimin yanındaki bir resim göreli yolla yazılırsa proje taşınınca da bulunur."));
        break;
    }
    case LayoutItemKind::Shape: {
        group(tr("ŞEKİL"));
        auto* shape = new ComboBox(properties_);
        for (const auto& [word, text] : {std::pair{"dikdortgen", "Dikdörtgen"},
                                         std::pair{"elips", "Elips"}, std::pair{"cizgi", "Çizgi"}})
            shape->addItem(tr(text), QString::fromUtf8(word));
        shape->setCurrentIndex(static_cast<int>(item.shape));
        shape->setAccessibleName(tr("Şekil"));
        connect(shape, &QComboBox::currentIndexChanged, this, [this, shape](int at) {
            if (filling_ || at < 0) return;
            edit(QStringLiteral("sekil=%1").arg(shape->itemData(at).toString()));
        });
        row(tr("Biçim"), shape);
        row(tr("Çizgi rengi"), colourEditor(item.frame_colour, "cerceve_renk", tr("Çizgi rengi")));
        row(tr("Kalınlık"),
            mmEditor(item.frame_width, "cerceve_kalinlik", tr("Çizgi kalınlığı"), 2));
        row(tr("Dolgu"), switchEditor(item.background, "zemin", tr("Dolgu")));
        if (item.background)
            row(tr("Dolgu rengi"),
                colourEditor(item.background_colour, "zemin_renk", tr("Dolgu rengi")));
        break;
    }
    case LayoutItemKind::Table: buildTableProperties(l, item); break;
    case LayoutItemKind::Chart: {
        group(tr("GRAFİK"));
        auto* source = new ComboBox(properties_);
        source->addItem(tr("Katman seçin"), QString());
        for (const core::Layer& layer : doc.layers())
            source->addItem(QString::fromStdString(layer.name), QString::fromStdString(layer.name));
        const int chosen = source->findData(QString::fromStdString(item.text));
        source->setCurrentIndex(chosen >= 0 ? chosen : 0);
        source->setAccessibleName(tr("Kaynak katman"));
        connect(source, &QComboBox::currentIndexChanged, this, [this, source](int at) {
            if (filling_ || at <= 0) return;
            edit(QStringLiteral("metin=%1").arg(quoted(source->itemData(at).toString())));
        });
        row(tr("Katman"), source);

        auto* counted = new ComboBox(properties_);
        counted->addItem(tr("Sütun seçin"), QString());
        const core::AttrTable& attributes = doc.attributes();
        for (std::size_t c = 0; c < attributes.columns(); ++c)
            if (const core::AttrColumn* column = attributes.column(static_cast<core::AttrId>(c));
                column != nullptr)
                counted->addItem(QString::fromStdString(column->spec().id),
                                 QString::fromStdString(column->spec().id));
        const int held = item.columns.empty()
                             ? 0
                             : counted->findData(QString::fromStdString(item.columns.front()));
        counted->setCurrentIndex(std::max(held, 0));
        counted->setAccessibleName(tr("Sayılacak sütun"));
        connect(counted, &QComboBox::currentIndexChanged, this, [this, counted](int at) {
            if (filling_ || at <= 0) return;
            edit(QStringLiteral("sutunlar=%1").arg(quoted(counted->itemData(at).toString())));
        });
        row(tr("Sütun"), counted);
        row(tr("Harita"), mapEditor(l, item));
        row(tr("Yazı boyu"), mmEditor(item.text_height, "yazi", tr("Yazı boyu")));
        row(tr("Yazı rengi"), colourEditor(item.text_colour, "yazi_renk", tr("Yazı rengi")));
        break;
    }
    }

    // ---- how it looks -------------------------------------------------------
    if (item.kind != LayoutItemKind::Shape) {
        group(tr("ÇERÇEVE VE ZEMİN"));
        row(tr("Çerçeve"), switchEditor(item.frame_visible, "cerceve", tr("Çerçeve")));
        if (item.frame_visible) {
            row(tr("Çerçeve rengi"),
                colourEditor(item.frame_colour, "cerceve_renk", tr("Çerçeve rengi")));
            row(tr("Kalınlık"),
                mmEditor(item.frame_width, "cerceve_kalinlik", tr("Çerçeve kalınlığı"), 2));
        }
        row(tr("Zemin"), switchEditor(item.background, "zemin", tr("Zemin")));
        if (item.background)
            row(tr("Zemin rengi"),
                colourEditor(item.background_colour, "zemin_renk", tr("Zemin rengi")));
    }

    group(tr("ÖĞE"));
    row(tr("Kilitli"), switchEditor(item.locked, "kilit", tr("Kilitli — taşınmaz, boyutlanmaz")));
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
    row(tr("Ad"), named);
    help(tr("Komut satırı ve betik öğeyi ad= ile bu adla anar."));
}

/// A TABLE'S SECTION OF THE INSPECTOR: what a row is, its columns — the list
/// and the one picked in it — its head, its lines and its type.
///
/// Every control writes one `ÇIKTIÖĞE` line, as everything else here does: a
/// column's settings through `sutunayarla sutun=N`, the list through
/// `sutunekle`, `sutuntasi` and `sutunsil`.
void LayoutDesigner::buildTableProperties(const core::Layout& /*l*/, const core::LayoutItem& item)
{
    const core::Document& doc           = controller_.document();
    const core::LayoutTableStyle& style = item.table;

    group(tr("TABLO"));
    auto* source = new ComboBox(properties_);
    source->addItem(tr("Katman seçin"), QString());
    for (const core::Layer& layer : doc.layers())
        source->addItem(QString::fromStdString(layer.name), QString::fromStdString(layer.name));
    const int chosen = source->findData(QString::fromStdString(item.text));
    source->setCurrentIndex(chosen >= 0 ? chosen : 0);
    source->setAccessibleName(tr("Tablonun okuduğu katman"));
    connect(source, &QComboBox::currentIndexChanged, this, [this, source](int at) {
        if (filling_ || at <= 0) return;
        edit(QStringLiteral("metin=%1").arg(quoted(source->itemData(at).toString())));
    });
    row(tr("Katman"), source);
    row(tr("Bir satır"),
        wordsEditor({tr("Nesne"), tr("Köşe")}, {QStringLiteral("nesne"), QStringLiteral("kose")},
                    style.rows == core::TableRows::Vertices ? 1 : 0, "satirlar",
                    tr("Bir satır: katmandaki bir nesne ya da bir köşe")));
    help(style.rows == core::TableRows::Vertices
             ? tr("Koordinat listesi: her köşe bir satır, iki parselin ortak köşesi bir kez.")
             : tr("Öznitelik tablosu: katmandaki her nesne bir satır."));

    // THE ORDER OF THE ROWS. By number unless told otherwise — a coordinate
    // list is read by number — and by any column's source in natural order.
    auto* order = new ComboBox(properties_);
    order->addItem(tr("Çizimdeki sıra"), QStringLiteral("yok"));
    for (const core::TableSource& one : core::table_sources())
        if (std::string_view(one.word) != "$sira")
            order->addItem(QString::fromUtf8(one.heading), QString::fromUtf8(one.word));
    {
        const core::AttrTable& attrs = doc.attributes();
        for (std::size_t c = 0; c < attrs.columns(); ++c)
            if (const core::AttrColumn* held = attrs.column(static_cast<core::AttrId>(c));
                held != nullptr &&
                (item.text.empty() || core::attr_applies_to(held->spec(), item.text)))
                order->addItem(QString::fromStdString(held->spec().name_tr.empty()
                                                          ? held->spec().id
                                                          : held->spec().name_tr),
                               QString::fromStdString(held->spec().id));
    }
    const int ordered =
        style.sort_by.empty() ? 0 : order->findData(QString::fromStdString(style.sort_by));
    order->setCurrentIndex(std::max(ordered, 0));
    order->setAccessibleName(tr("Satırların sıralandığı sütun"));
    connect(order, &QComboBox::currentIndexChanged, this, [this, order](int i) {
        if (filling_ || i < 0) return;
        edit(QStringLiteral("sirala=%1").arg(quoted(order->itemData(i).toString())));
    });
    row(tr("Sıralama"), order);
    if (!style.sort_by.empty())
        row(tr("Yön"),
            wordsEditor({tr("Artan"), tr("Azalan")},
                        {QStringLiteral("artan"), QStringLiteral("azalan")},
                        style.sort_descending ? 1 : 0, "sirala_yon", tr("Sıralama yönü")));
    help(tr("Doğal sıra: 2, 10'dan önce; K-2, K-10'dan önce. Numarası olmayan satırlar sonda."));

    // ---- the columns: the list, and the one picked in it ----------------------
    const std::vector<core::LayoutColumn> columns = core::table_column_list(doc, item);
    const int count                               = static_cast<int>(columns.size());
    tableColumn_ = std::clamp(tableColumn_, 0, std::max(0, count - 1));
    group(tr("SÜTUNLAR"), count == 1 ? tr("1 sütun") : tr("%1 sütun").arg(count));

    auto* list = new QListWidget(properties_);
    list->setObjectName(QStringLiteral("layoutColumns"));
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setAccessibleName(tr("Tablonun sütunları"));
    for (int c = 0; c < count; ++c) {
        const core::LayoutColumn& column = columns[static_cast<std::size_t>(c)];
        // WHAT THE HEAD SAYS, AND WHAT IT SHOWS: `Sağa (Y)` over `$y` tells a
        // reader what the heading promises and where the figures come from.
        auto* entry =
            new QListWidgetItem(tr("%1.  %2   —   %3")
                                    .arg(c + 1)
                                    .arg(QString::fromStdString(core::table_heading(doc, column)),
                                         QString::fromStdString(column.source)),
                                list);
        entry->setToolTip(QString::fromStdString(column.source));
    }
    // AS TALL AS ITS ROWS, measured rather than assumed: the sheet gives a
    // list row its own height, and a list one row short hides a column.
    list->setUniformItemSizes(true);
    const int rowHeight = count > 0 ? std::max(24, list->sizeHintForRow(0)) : 24;
    list->setFixedHeight(std::max(1, count) * rowHeight + 2 * list->frameWidth() + 2);
    if (count > 0) list->setCurrentRow(tableColumn_);
    connect(list, &QListWidget::currentRowChanged, this, [this](int at) {
        if (filling_ || at < 0 || at == tableColumn_) return;
        tableColumn_ = at;
        buildProperties();
    });
    propertyColumn_->addWidget(list);

    // THE LIST'S OWN VERBS, under it: add from a menu that says what each
    // computed value is, move the picked one, take it away.
    auto* verbs = new QWidget(properties_);
    auto* line  = new QHBoxLayout(verbs);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(4);
    auto* add = new Button(ButtonRole::Secondary, tr("Sütun ekle"), Glyph::Plus, verbs);
    add->setControlSize(ControlSize::Compact);
    auto* sources = new QMenu(add);
    for (const core::TableSource& one : core::table_sources())
        connect(sources->addAction(
                    QStringLiteral("%1 — %2").arg(QString::fromUtf8(one.heading), tr(one.what))),
                &QAction::triggered, this, [this, count, word = QString::fromUtf8(one.word)] {
                    tableColumn_ = count;
                    edit(QStringLiteral("kaynak=%1").arg(word), QStringLiteral("sutunekle"));
                });
    // THE LAYER'S ATTRIBUTES, by what the attribute table calls them.
    const core::AttrTable& attributes = doc.attributes();
    bool first                        = true;
    for (std::size_t c = 0; c < attributes.columns(); ++c) {
        const core::AttrColumn* held = attributes.column(static_cast<core::AttrId>(c));
        if (held == nullptr) continue;
        if (!item.text.empty() && !core::attr_applies_to(held->spec(), item.text)) continue;
        if (first) sources->addSeparator();
        first               = false;
        const QString id    = QString::fromStdString(held->spec().id);
        const QString named = QString::fromStdString(held->spec().name_tr);
        connect(sources->addAction(named.isEmpty() ? id : QStringLiteral("%1 — %2").arg(named, id)),
                &QAction::triggered, this, [this, count, id] {
                    tableColumn_ = count;
                    edit(QStringLiteral("kaynak=%1").arg(quoted(id)), QStringLiteral("sutunekle"));
                });
    }
    add->setMenuArrow(sources);
    line->addWidget(add);
    line->addStretch(1);
    const auto columnVerb = [this, verbs, line](Glyph glyph, const QString& tip, bool enabled,
                                                auto&& act) {
        Button* b = strip_button(glyph, tip, verbs);
        b->setEnabled(enabled);
        connect(b, &QPushButton::clicked, this, std::forward<decltype(act)>(act));
        line->addWidget(b);
    };
    const int at = tableColumn_;
    columnVerb(Glyph::ChevronUp, tr("Sütunu öne al — tabloda bir sola"), count > 1 && at > 0,
               [this, at] {
                   tableColumn_ = at - 1;
                   edit(QStringLiteral("sutun=%1 hedef=%2").arg(at + 1).arg(at),
                        QStringLiteral("sutuntasi"));
               });
    columnVerb(Glyph::ChevronDown, tr("Sütunu arkaya al — tabloda bir sağa"),
               count > 1 && at + 1 < count, [this, at] {
                   tableColumn_ = at + 1;
                   edit(QStringLiteral("sutun=%1 hedef=%2").arg(at + 1).arg(at + 2),
                        QStringLiteral("sutuntasi"));
               });
    columnVerb(Glyph::Trash, tr("Sütunu sil"), count > 0, [this, at] {
        edit(QStringLiteral("sutun=%1").arg(at + 1), QStringLiteral("sutunsil"));
    });
    propertyColumn_->addWidget(verbs);

    // ---- the picked column ------------------------------------------------------
    if (count > 0) {
        const core::LayoutColumn& column = columns[static_cast<std::size_t>(at)];
        const Writer write               = [this, at](const QString& change) {
            edit(QStringLiteral("sutun=%1 %2").arg(at + 1).arg(change),
                               QStringLiteral("sutunayarla"));
        };
        group(tr("%1. SÜTUN").arg(at + 1));
        row(tr("Başlık"),
            textEditor(QString::fromStdString(column.heading), "baslik", tr("Sütun başlığı"),
                       QString::fromStdString(core::table_heading(doc, column)), write));

        auto* shows = new ComboBox(properties_);
        for (const core::TableSource& one : core::table_sources())
            shows->addItem(QStringLiteral("%1 — %2").arg(QString::fromUtf8(one.word),
                                                         QString::fromUtf8(one.heading)),
                           QString::fromUtf8(one.word));
        for (std::size_t c = 0; c < attributes.columns(); ++c)
            if (const core::AttrColumn* held = attributes.column(static_cast<core::AttrId>(c));
                held != nullptr &&
                (item.text.empty() || core::attr_applies_to(held->spec(), item.text)))
                shows->addItem(QString::fromStdString(held->spec().id),
                               QString::fromStdString(held->spec().id));
        const int held = shows->findData(QString::fromStdString(column.source));
        if (held < 0)
            shows->addItem(QString::fromStdString(column.source),
                           QString::fromStdString(column.source));
        shows->setCurrentIndex(held >= 0 ? held : shows->count() - 1);
        shows->setAccessibleName(tr("Sütunun gösterdiği"));
        connect(shows, &QComboBox::currentIndexChanged, this, [shows, write, this](int i) {
            if (filling_ || i < 0) return;
            write(QStringLiteral("kaynak=%1").arg(quoted(shows->itemData(i).toString())));
        });
        row(tr("Gösterdiği"), shows);
        row(tr("Hiza"),
            wordsEditor({tr("Sol"), tr("Orta"), tr("Sağ")},
                        {QStringLiteral("sol"), QStringLiteral("orta"), QStringLiteral("sag")},
                        column.align, "sutun_hiza", tr("Sütunun hizası"), write));
        row(tr("Ondalık"),
            countEditor(column.decimals, "ondalik", tr("Ondalık basamak; -1 olduğu gibi"), -1, 9,
                        QString(), write));
        help(tr("-1 değeri olduğu gibi yazar; koordinat 3, alan ve uzunluk 2 basamak."));
        row(tr("Binlik ayırıcı"),
            switchEditor(column.thousands, "binlik", tr("Binlikleri ayır"), write));
        row(tr("Eş aralıklı"), switchEditor(column.mono, "esaralik",
                                            tr("Eş aralıklı yazı; rakamlar alt alta"), write));
        row(tr("Genişlik"), mmEditor(column.width, "sutun_genislik",
                                     tr("Sütun genişliği; 0 kalan yeri paylaşır"), 1, write));
        help(tr("0, sabit genişlikli sütunlardan kalan yeri eşit paylaşır."));
    }

    // ---- the head -----------------------------------------------------------------
    group(tr("BAŞLIK SATIRI"));
    // A LIST, NOT A SEGMENT: four words do not fit the editor column, and cut
    // to `ütu | Drta` they said nothing.
    auto* headAlign = new ComboBox(properties_);
    for (const auto& [word, text] :
         {std::pair{"sutun", "Sütunun hizasıyla"}, std::pair{"sol", "Sol"},
          std::pair{"orta", "Orta"}, std::pair{"sag", "Sağ"}})
        headAlign->addItem(tr(text), QString::fromUtf8(word));
    headAlign->setCurrentIndex(style.header_align <= 2 ? style.header_align + 1 : 0);
    headAlign->setAccessibleName(tr("Başlıkların hizası"));
    connect(headAlign, &QComboBox::currentIndexChanged, this, [this, headAlign](int i) {
        if (filling_ || i < 0) return;
        edit(QStringLiteral("baslik_hiza=%1").arg(headAlign->itemData(i).toString()));
    });
    row(tr("Hiza"), headAlign);
    row(tr("Kalın"), switchEditor(style.header_bold, "baslik_kalin", tr("Kalın başlık")));
    row(tr("Yazı boyu"),
        mmEditor(style.header_height, "baslik_yazi", tr("Başlığın yazı boyu; 0 tablonunki")));
    row(tr("Renk"), colourEditor(style.header_colour, "baslik_renk", tr("Başlığın rengi")));
    const bool filled = (style.header_fill >> 24) != 0;
    auto* fill        = new QWidget(properties_);
    fill->setFixedHeight(static_cast<int>(ControlSize::Regular));
    auto* fillRow = new QHBoxLayout(fill);
    fillRow->setContentsMargins(0, 0, 0, 0);
    fillRow->setSpacing(8);
    auto* pill = new ToggleSwitch(fill);
    pill->setChecked(filled);
    pill->setAccessibleName(tr("Başlık zemini"));
    fillRow->addWidget(pill);
    auto* pillWord = new QLabel(filled ? tr("açık") : tr("kapalı"), fill);
    pillWord->setObjectName(QStringLiteral("formHelp"));
    fillRow->addWidget(pillWord);
    fillRow->addStretch(1);
    // SWITCHED ON, THE HEAD TAKES THE TABLE'S OWN STRIPE COLOUR — a light fill
    // the sheet already uses — and the colour field under it changes it.
    const std::uint32_t light = style.stripe_colour;
    connect(pill, &QAbstractButton::toggled, this, [this, light](bool on) {
        if (filling_) return;
        edit(on ? QStringLiteral("baslik_zemin=%1").arg(colour_word(light))
                : QStringLiteral("baslik_zemin=yok"));
    });
    row(tr("Zemin"), fill);
    if (filled)
        row(tr("Zemin rengi"),
            colourEditor(style.header_fill, "baslik_zemin", tr("Başlık zemini")));

    // ---- the lines and the rows -----------------------------------------------------
    group(tr("ÇİZGİLER VE SATIRLAR"));
    row(tr("Hücre çizgileri"), switchEditor(style.lines, "cizgiler", tr("Hücre çizgileri")));
    if (style.lines) {
        row(tr("Çizgi rengi"), colourEditor(style.line_colour, "cizgi_renk", tr("Çizgi rengi")));
        row(tr("Kalınlık"),
            mmEditor(style.line_width, "cizgi_kalinlik", tr("Çizgi kalınlığı; 0 kıl çizgi"), 2));
    }
    row(tr("Şeritli"),
        switchEditor(style.stripes, "seritli", tr("Satırları birer atlayarak boya")));
    if (style.stripes)
        row(tr("Şerit rengi"), colourEditor(style.stripe_colour, "serit_renk", tr("Şerit rengi")));
    row(tr("Satır sınırı"), countEditor(item.row_limit, "satir_siniri",
                                        tr("Satır sınırı; 0 kutuya sığdığı kadar"), 0, 100000));
    help(tr("0 kutuya sığdığı kadar satır gösterir; sığmayanlar tablonun altında sayılır."));

    // ---- the type -----------------------------------------------------------------
    group(tr("YAZI"));
    row(tr("Yazı boyu"), mmEditor(item.text_height, "yazi", tr("Tablonun yazı boyu")));
    row(tr("Renk"), colourEditor(item.text_colour, "yazi_renk", tr("Tablonun yazı rengi")));
    row(tr("Ondalık işareti"),
        wordsEditor({tr("Virgül  1,25"), tr("Nokta  1.25")},
                    {QStringLiteral("virgul"), QStringLiteral("nokta")},
                    style.decimal_comma ? 0 : 1, "ondalik_isaret", tr("Ondalık işareti")));
}

void LayoutDesigner::showItem(const QString& id)
{
    if (canvas_ == nullptr) return;
    // `select` reports the change and the column is rebuilt from there; a
    // second refresh here only built it twice.
    pane_ = id.isEmpty() ? Pane::Sheet : Pane::Item;
    if (canvas_->selection() == (id.isEmpty() ? QStringList{} : QStringList{id})) {
        refresh();
        return;
    }
    canvas_->select(id);
}

void LayoutDesigner::aimAt(core::Box2 window)
{
    const core::Layout* l = layout();
    if (l == nullptr) return;
    // THE PICKED MAP, if a map is picked; the first one otherwise.
    const core::LayoutItem* map = nullptr;
    if (const core::LayoutItem* picked = l->find(canvas_->selected().toStdString());
        picked != nullptr && picked->kind == core::LayoutItemKind::Map)
        map = picked;
    if (map == nullptr) map = l->first_map();
    if (map == nullptr) {
        if (hint_ != nullptr)
            hint_->setText(tr("Bu yerleşimde harita çerçevesi yok; ekleyip yeniden deneyin."));
        return;
    }
    if (window.empty()) return;

    // THE NAME IS TAKEN BEFORE THE DOCUMENT MOVES. `map` points INTO the
    // document's layout, and the command below rewrites the item vector — the
    // pointer then dangles. Reading `map->id` after it crashed the program in
    // `strlen`, intermittently, for weeks.
    const QString aimed = QString::fromStdString(map->id);

    // METRES ON THE LINE, and the key written twice — the parser's own shape
    // for a window (`YAZDIR pencere=`).
    const auto metres = [](core::Mm v) {
        return QString::number(static_cast<double>(v) / 1000.0, 'f', 3);
    };
    controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 "
                                       "pencere=%3,%4 pencere=%5,%6")
                            .arg(quoted(name_), quoted(aimed), metres(window.min_x),
                                 metres(window.min_y), metres(window.max_x), metres(window.max_y)),
                        command::Origin::Gui);
    canvas_->select(aimed);
    pane_ = Pane::Item;
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
    // reaches it. A STATIC CAST, and it is sound: `rows_` is only ever assigned
    // the `ItemRow` built in `buildInspector`; `qobject_cast` cannot help —
    // `ItemRow` lives in this file's anonymous namespace and has no meta-object.
    if (rows_ != nullptr) static_cast<ItemRow*>(rows_)->setTheme(mode);
    if (items_ != nullptr) items_->viewport()->update();
    if (layout() != nullptr) buildProperties();
}

QStringList LayoutDesigner::probeDrive()
{
    QStringList said;
    const core::Layout* l = layout();
    if (l == nullptr) return {QStringLiteral("yerleşim yok")};

    canvas_->select(QStringLiteral("baslik"));
    said << QStringLiteral("seçim: %1").arg(canvas_->selected());

    // A DRAG, AS THE CANVAS WOULD REPORT IT. The signal is the seam the mouse
    // uses, so driving it drives the real path rather than a shortcut.
    if (const core::LayoutItem* title = l->find("baslik"); title != nullptr) {
        // THE VALUE IS COPIED BEFORE THE COMMAND RUNS. `set_layouts` replaces the
        // whole list, so `title` dangles the moment the move is applied.
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

    // ---- THE PREVIEW DOES NOT FILL WHAT THE SHEET LEAVES EMPTY --------------
    //
    // The canvas once drew the sheet's drop shadow and then called
    // `paint_layout_page` without giving the painter back: every parcel came
    // out filled with the shadow's black at ten percent, a pale grey inside the
    // map frame that appears in no file this program writes. MEASURED AS THE
    // SHARE OF UNTOUCHED PAPER: filled, the probe's parcels would cover a
    // quarter of the page; outlined, they leave it white.
    canvas_->zoomToFit();
    if (canvas_ != nullptr && canvas_->sheetRect().width() > 100) {
        canvas_->repaint();
        const QImage sheet = canvas_->grab(canvas_->sheetRect()).toImage();
        std::size_t paper  = 0;
        const std::size_t all =
            static_cast<std::size_t>(sheet.width()) * static_cast<std::size_t>(sheet.height());
        for (int y = 0; y < sheet.height(); ++y)
            for (int x = 0; x < sheet.width(); ++x)
                if (sheet.pixelColor(x, y) == QColor(Qt::white)) ++paper;
        blankPaperPercent_ = all == 0 ? 0 : static_cast<int>(100 * paper / all);
        said << QStringLiteral("boş kâğıt: %%1").arg(blankPaperPercent_);
    }

    // ---- THE SHEET'S OWN SETTINGS, THROUGH THE PANEL'S OWN PATH -------------
    //
    // `islem=sayfa` DEFAULTS every argument it is not given, so a line carrying
    // only `kenar=15` also makes the sheet A4 and turns it upright. One field is
    // touched here and the other three are checked for having stayed put.
    sheetEdit(QStringLiteral("kenar=15"));
    if (const core::Layout* sheet = layout(); sheet != nullptr)
        said << QStringLiteral("sayfa: %1 %2 · kenar %3 mm · %4 dpi · %5×%6")
                    .arg(QString::fromStdString(sheet->paper),
                         sheet->landscape ? QStringLiteral("yatay") : QStringLiteral("dikey"))
                    .arg(sheet->margin / 1000)
                    .arg(sheet->dpi)
                    .arg(sheet->pages.front().w / 1000)
                    .arg(sheet->pages.front().h / 1000);

    // AND THE RESOLUTION. BOTH BEFORE THE LEGEND IS ADDED, deliberately: the
    // caller's next step is a `GERİAL` that must undo the LAST gesture this
    // makes, and it checks the item count.
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
