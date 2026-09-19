// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/layout_designer.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/flow_layout.hpp"
#include "kentos_cad/app/layout_render.hpp"
#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/core/document.hpp"

#include <QDate>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kentos::app {
namespace {

/// The handle's side, in device pixels. Big enough to hit with a mouse, small
/// enough not to swallow a narrow item.
constexpr int kGripPx = 7;

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

    // FITTED AND CENTRED with a margin of air around it, which is what makes the
    // sheet read as a sheet rather than as the window's background.
    const double air     = 16.0;
    const double avail_w = std::max(1.0, width() - 2 * air);
    const double avail_h = std::max(1.0, height() - 2 * air);
    const double scale =
        std::min(avail_w / static_cast<double>(page.w), avail_h / static_cast<double>(page.h));
    const double w = static_cast<double>(page.w) * scale;
    const double h = static_cast<double>(page.h) * scale;
    return QRectF((width() - w) / 2.0, (height() - h) / 2.0, w, h);
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
    // paper on a table" without a word.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRect(box.translated(3, 3));

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

    if (selected_.isEmpty()) return;
    const core::LayoutItem* item = l->find(selected_.toStdString());
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
    left->setFixedWidth(210);
    auto* leftColumn = new QVBoxLayout(left);
    leftColumn->setContentsMargins(12, 12, 8, 12);
    leftColumn->setSpacing(8);
    // ---- which page, and the four things one can do to pages ---------------
    //
    // ONE PAGE IS SHOWN AT A TIME, which is what a sheet is: the canvas draws
    // it, the item list holds its items and every drag is measured against its
    // paper. Without this the designer could open a two-page layout and only
    // ever reach the first one.
    leftColumn->addWidget(new FormSection(tr("SAYFA"), QString(), left));

    // A NUMBER, NOT A LIST. Pages are counted from one and there is no name to
    // pick from; the row's help says how many there are and how big this one is.
    pageField_ = new Field(number_of(1, 9999), left);
    pageField_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    connect(pageField_, &Field::committed, this, [this](const QString& typed) {
        if (filling_) return;
        const core::Layout* l = layout();
        if (l == nullptr) return;
        const int wanted = std::clamp(typed.toInt() - 1, 0, static_cast<int>(l->pages.size()) - 1);
        canvas_->setSheet(name_, wanted);
        refresh();
    });
    pageRow_ = new FormRow(tr("Sayfa"), pageField_, left);
    leftColumn->addWidget(pageRow_);

    auto* pageButtons = new QWidget(left);
    auto* pageRow     = new FlowLayout(pageButtons, 0, 4, 4);

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
        auto* button = new Button(one.glyph, tr(one.label), pageButtons);
        button->setFixedSize(32, static_cast<int>(ControlSize::Regular));
        button->setToolTip(tr(one.label));
        button->setAccessibleName(tr(one.label));
        connect(button, &Button::clicked, this, [this, verb = one.verb] { pageVerb(verb); });
        pageRow->addWidget(button);
    }
    leftColumn->addWidget(pageButtons);

    leftColumn->addWidget(new FormSection(tr("ÖĞELER"), QString(), left));
    leftColumn->addWidget(buildItemList(), 1);

    auto* adders = new QWidget(left);
    // A FLOW RATHER THAN A ROW: eight 32 px buttons do not fit in a 210 px
    // column, and the row silently cut the last two off — the table and the
    // shape, which are exactly the two a reader would go looking for.
    auto* grid = new FlowLayout(adders, 0, 4, 4);

    // THE EIGHT KINDS, each as an icon button whose tooltip is its whole label
    // (ui.md R22). One row, because a layout has eight kinds and always will.
    struct Adder
    {
        const char* kind;
        const char* label;
        Glyph glyph;
    };

    static constexpr Adder kAdders[] = {
        {"harita", "Harita çerçevesi", Glyph::Rectangle},
        {"metin", "Metin", Glyph::Text},
        {"olcek", "Ölçek çubuğu", Glyph::Measure},
        {"kuzey", "Kuzey oku", Glyph::Locate},
        {"lejant", "Lejant", Glyph::Table},
        {"resim", "Resim", Glyph::Palette},
        {"sekil", "Şekil", Glyph::Polygon},
        {"tablo", "Tablo", Glyph::Grid},
    };
    for (const Adder& one : kAdders) {
        auto* button       = new Button(one.glyph, tr(one.label), adders);
        const QString kind = QString::fromUtf8(one.kind);
        connect(button, &QPushButton::clicked, this, [this, kind] { addItem(kind); });
        grid->addWidget(button);
    }
    leftColumn->addWidget(adders);

    auto* remove = new Button(ButtonRole::Danger, tr("Seçili öğeyi sil"), Glyph::Trash, left);
    remove->setControlSize(ControlSize::Compact);
    connect(remove, &QPushButton::clicked, this, [this] {
        if (canvas_->selected().isEmpty()) return;
        controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=sil yerlesim=%1 ad=%2")
                                .arg(quoted(name_), quoted(canvas_->selected())),
                            command::Origin::Gui);
        refresh();
    });
    leftColumn->addWidget(remove);
    row->addWidget(left);

    // ---- middle: the page ----------------------------------------------------
    canvas_ = new LayoutCanvas(controller_, body);
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
    row->addWidget(canvas_, 1);

    // ---- right: the selected item's properties -------------------------------
    auto* right = new QWidget(body);
    right->setFixedWidth(280);
    auto* rightColumn = new QVBoxLayout(right);
    rightColumn->setContentsMargins(8, 12, 12, 12);
    rightColumn->setSpacing(8);
    rightColumn->addWidget(new FormSection(tr("ÖZELLİKLER"), QString(), right));

    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    properties_     = new QWidget(scroll);
    propertyColumn_ = new QVBoxLayout(properties_);
    propertyColumn_->setContentsMargins(0, 0, 0, 0);
    propertyColumn_->setSpacing(8);
    scroll->setWidget(properties_);
    rightColumn->addWidget(scroll, 1);

    status_ = new QLabel(right);
    status_->setObjectName(QStringLiteral("formHelp"));
    status_->setWordWrap(true);
    rightColumn->addWidget(status_);
    row->addWidget(right);

    return body;
}

QWidget* LayoutDesigner::buildItemList()
{
    items_ = new QListWidget(this);
    items_->setObjectName(QStringLiteral("layoutItems"));
    items_->setFrameShape(QFrame::NoFrame);
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

void LayoutDesigner::edit(const QString& arguments)
{
    if (canvas_->selected().isEmpty()) return;
    controller_.runLine(QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=%1 ad=%2 %3")
                            .arg(quoted(name_), quoted(canvas_->selected()), arguments),
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
        auto* row                    = new QListWidgetItem(
            QStringLiteral("%1  ·  %2")
                .arg(QString::fromStdString(item->id),
                                        QString::fromUtf8(core::layout_item_kind_label(item->kind))),
            items_);
        row->setData(Qt::UserRole, QString::fromStdString(item->id));
        if (item->locked) row->setToolTip(tr("Kilitli"));
        if (QString::fromStdString(item->id) == chosen) items_->setCurrentItem(row);
    }
    if (pageField_ != nullptr) {
        const int shown = std::clamp(canvas_->page(), 0, static_cast<int>(l->pages.size()) - 1);
        pageField_->setValue(QString::number(shown + 1));
        if (pageRow_ != nullptr)
            pageRow_->setHelp(tr("%1 sayfadan biri · %2×%3 mm")
                                  .arg(l->pages.size())
                                  .arg(l->pages[static_cast<std::size_t>(shown)].w / 1000)
                                  .arg(l->pages[static_cast<std::size_t>(shown)].h / 1000));
    }
    filling_ = false;

    canvas_->refresh();
    buildProperties();

    std::size_t here = 0;
    for (std::size_t i = 0; i < l->items.size(); ++i)
        if (l->page_of(i) == canvas_->page()) ++here;
    status_->setText(tr("%1 — %2/%3 sayfa, bu sayfada %4 öğe (toplam %5). Sürükleyin ya da ok "
                        "tuşlarıyla kaydırın; Shift on kat.")
                         .arg(QString::fromStdString(l->paper.empty() ? "" : l->paper))
                         .arg(canvas_->page() + 1)
                         .arg(l->pages.size())
                         .arg(here)
                         .arg(l->items.size()));
}

QWidget* LayoutDesigner::buildProperties()
{
    // Cleared and rebuilt, rather than kept and reconciled: the fields a Map
    // shows and the fields a Label shows have nothing in common past the frame,
    // and a panel that hid half its widgets would be a panel whose layout
    // depends on what was selected before.
    while (QLayoutItem* old = propertyColumn_->takeAt(0)) {
        if (QWidget* w = old->widget(); w != nullptr) w->deleteLater();
        delete old;
    }

    const core::Layout* l        = layout();
    const core::LayoutItem* item = l != nullptr && !canvas_->selected().isEmpty()
                                       ? l->find(canvas_->selected().toStdString())
                                       : nullptr;
    if (item == nullptr) {
        auto* nothing = new QLabel(tr("Bir öğe seçin."), properties_);
        nothing->setObjectName(QStringLiteral("formHelp"));
        propertyColumn_->addWidget(nothing);
        propertyColumn_->addStretch(1);
        return properties_;
    }

    filling_ = true;

    const auto number_field = [this](const QString& label, const QString& value,
                                     const char* argument) {
        FieldSpec spec = decimal_of(1);
        spec.suffix    = QStringLiteral("mm");
        auto* field    = new Field(spec, properties_);
        field->setFixedHeight(static_cast<int>(ControlSize::Regular));
        field->setValue(value);
        field->setAccessibleName(label);
        const QString name = QString::fromUtf8(argument);
        connect(field, &Field::committed, this, [this, name](const QString& typed) {
            if (filling_) return;
            edit(QStringLiteral("%1=%2").arg(name, QString::number(typed.toDouble(), 'f', 1)));
        });
        propertyColumn_->addWidget(new FormRow(label, field, properties_));
        return field;
    };

    number_field(tr("Sol (x)"), mm_text(item->frame.x), "x");
    number_field(tr("Üst (y)"), mm_text(item->frame.y), "y");
    number_field(tr("Genişlik"), mm_text(item->frame.w), "genislik");
    number_field(tr("Yükseklik"), mm_text(item->frame.h), "yukseklik");

    // ---- what only some kinds have ------------------------------------------
    if (item->kind == core::LayoutItemKind::Label || item->kind == core::LayoutItemKind::Picture ||
        item->kind == core::LayoutItemKind::Table || item->kind == core::LayoutItemKind::Legend) {
        FieldSpec spec = field_of(FieldKind::Text);
        spec.placeholder =
            item->kind == core::LayoutItemKind::Picture
                ? tr("resim yolu")
                : (item->kind == core::LayoutItemKind::Table ? tr("katman adı") : tr("yazı"));
        auto* text = new Field(spec, properties_);
        text->setFixedHeight(static_cast<int>(ControlSize::Regular));
        text->setValue(QString::fromStdString(item->text));
        connect(text, &Field::committed, this, [this](const QString& typed) {
            if (filling_) return;
            edit(QStringLiteral("metin=%1").arg(quoted(typed)));
        });
        auto* row = new FormRow(tr("Metin"), text, properties_);
        if (item->kind == core::LayoutItemKind::Label)
            row->setHelp(tr("<yerlesim>, <olcek>, <tarih>, <crs>, <proje>"));
        propertyColumn_->addWidget(row);
    }

    if (item->kind == core::LayoutItemKind::Map) {
        FieldSpec scaleSpec = field_of(FieldKind::Number);
        auto* scale         = new Field(scaleSpec, properties_);
        scale->setFixedHeight(static_cast<int>(ControlSize::Regular));
        scale->setValue(QString::number(item->scale));
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
        grid->setCurrentIndex(static_cast<int>(item->grid));
        connect(grid, &QComboBox::currentIndexChanged, this, [this, grid](int at) {
            if (filling_ || at < 0) return;
            edit(QStringLiteral("izgara=%1").arg(grid->itemData(at).toString()));
        });
        propertyColumn_->addWidget(new FormRow(tr("Koordinat ızgarası"), grid, properties_));
    }

    if (item->kind != core::LayoutItemKind::Map) {
        FieldSpec heightSpec = decimal_of(1);
        heightSpec.suffix    = QStringLiteral("mm");
        auto* height         = new Field(heightSpec, properties_);
        height->setFixedHeight(static_cast<int>(ControlSize::Regular));
        height->setValue(mm_text(item->text_height));
        connect(height, &Field::committed, this, [this](const QString& typed) {
            if (filling_) return;
            edit(QStringLiteral("yazi=%1").arg(QString::number(typed.toDouble(), 'f', 1)));
        });
        propertyColumn_->addWidget(new FormRow(tr("Yazı yüksekliği"), height, properties_));
    }

    auto* switches  = new QWidget(properties_);
    auto* switchRow = new QHBoxLayout(switches);
    switchRow->setContentsMargins(0, 0, 0, 0);
    switchRow->setSpacing(8);
    const auto toggle = [&](const QString& label, bool on, const char* argument) {
        auto* box = new ToggleSwitch(switches);
        box->setChecked(on);
        box->setAccessibleName(label);
        const QString name = QString::fromUtf8(argument);
        connect(box, &QAbstractButton::toggled, this, [this, name](bool checked) {
            if (filling_) return;
            edit(QStringLiteral("%1=%2").arg(name, checked ? QStringLiteral("evet")
                                                           : QStringLiteral("hayir")));
        });
        switchRow->addWidget(box);
        auto* text = new QLabel(label, switches);
        text->setObjectName(QStringLiteral("formRowLabel"));
        switchRow->addWidget(text);
    };
    toggle(tr("Çerçeve"), item->frame_visible, "cerceve");
    toggle(tr("Kilit"), item->locked, "kilit");
    switchRow->addStretch(1);
    propertyColumn_->addWidget(switches);
    propertyColumn_->addStretch(1);

    filling_ = false;
    return properties_;
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

    addItem(QStringLiteral("lejant"));
    said << QStringLiteral("öğe sayısı: %1").arg(layout()->items.size());
    return said;
}

} // namespace kentos::app
