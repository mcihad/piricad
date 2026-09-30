// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/import_wizard.hpp"

#include "kentos_cad/app/backend_factory.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/map_canvas.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/transaction.hpp"
#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/text.hpp"
#include "kentos_cad/io/dwg.hpp"
#include "kentos_cad/io/vector.hpp"
#include "kentos_cad/render/backend.hpp"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace kentos::app {
namespace {

/// `1 482` — the thin-space thousands `panels.cpp` prints an entity count with.
/// Restated rather than shared because the two files agree on a FORMAT, not on a
/// function, and a header holding one three-line helper is a header nobody reads.
QString grouped(std::uint64_t value)
{
    QString digits = QString::number(static_cast<qulonglong>(value));
    for (qsizetype at = digits.size() - 3; at > 0; at -= 3)
        digits.insert(at, QLatin1Char(' '));
    return digits;
}

/// `2,4 MB`, in the units a Turkish user reads.
QString readableSize(qint64 bytes)
{
    const auto scaled = static_cast<double>(bytes);
    if (bytes < 1024) return ImportWizard::tr("%1 B").arg(bytes);
    if (bytes < 1024LL * 1024)
        return ImportWizard::tr("%1 KB").arg(QString::number(scaled / 1024.0, 'f', 1));
    return ImportWizard::tr("%1 MB").arg(QString::number(scaled / (1024.0 * 1024.0), 'f', 1));
}

constexpr int kRow    = 30; ///< design.md §4: a list row
constexpr int kRowTwo = 42; ///< a row of two lines: a name over its detail
constexpr int kBox    = 14; ///< §15.3: the tick box, radius 3
constexpr int kPadX   = 10;
constexpr int kGap    = 9;
constexpr int kCntW   = 62;

/// The caption column and the settings column, as in the print window and the
/// layout designer's inspector (design.md §8).
constexpr int kCaption = 110;
constexpr int kColumn  = 352;

/// The stage's three faces: the invitation, the read, the drawing.
constexpr int kStageInvite  = 0;
constexpr int kStageReading = 1;
constexpr int kStageDrawing = 2;

/// How long a typed path has to stand still before it is read: long enough that
/// a path typed a letter at a time is not read at every letter.
constexpr int kSettleMs = 450;

/// A value as the command line takes it, with the escapes the parser reads back.
/// Inside quotes a backslash escapes: a Windows path written raw, `C:\Users\nora`,
/// arrived as `C:Users` and a new line.
QString quoted(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

const Tokens& tk(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

/// The attribute type as the user reads it. `attr_type_name` is the token a
/// script writes (`SÜTUN tur=`); this is the word beside a field on a page.
QString type_label(core::AttrType type)
{
    switch (type) {
    case core::AttrType::Int64: return QCoreApplication::translate("ImportWizard", "Tam sayı");
    case core::AttrType::Length: return QCoreApplication::translate("ImportWizard", "Uzunluk");
    case core::AttrType::Bool: return QCoreApplication::translate("ImportWizard", "Evet / hayır");
    case core::AttrType::Text: return QCoreApplication::translate("ImportWizard", "Metin");
    case core::AttrType::CodeRef: return QCoreApplication::translate("ImportWizard", "Kod");
    case core::AttrType::Decimal: return QCoreApplication::translate("ImportWizard", "Ondalık");
    case core::AttrType::Date: return QCoreApplication::translate("ImportWizard", "Tarih");
    }
    return QString::fromUtf8(core::attr_type_name(type));
}

/// One checklist row: the tick box, the name and a detail — a layer's entity
/// count at the right, or, over two lines, a field's layer, type and first value
/// under its name. design.md §15.3 — the state is said with a SHAPE (a tick) and
/// not with colour alone (§13).
class LayerCheckDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setTheme(ThemeMode mode) { theme_ = mode; }

    /// Two lines to a row: the name over its detail. A field's detail — its
    /// layer, its type and a first value — does not fit beside its name in a
    /// 352 px column, and a detail elided to nothing says nothing.
    void setTwoLines(bool on) { twoLines_ = on; }

    /// Where the row's tick box is: at the left, halfway down the row.
    static QRect boxRect(const QRect& row)
    {
        return {row.left() + kPadX, row.top() + ((row.height() - kBox) / 2), kBox, kBox};
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return {0, twoLines_ ? kRowTwo : kRow};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = tk(theme_);
        const QRect box = option.rect;

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        if (option.state.testFlag(QStyle::State_MouseOver)) p->fillRect(box, t.hoverRow);
        // THE ROW THE KEYBOARD IS ON, because Space ticks it: the list selects
        // nothing, so without this a keyboard user ticks a row they cannot see.
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            p->setPen(QPen(t.accent, 1.0));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5));
        }

        const bool on = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;

        const QRect tick = boxRect(box);
        p->setPen(QPen(on ? t.accent : t.border, 1.0));
        p->setBrush(on ? t.accent : t.bgInput);
        p->drawRoundedRect(QRectF(tick).adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);

        if (on) {
            // The SHAPE that says "chosen" — §13 forbids saying it with the fill
            // alone, which a colour-blind reader cannot separate from the border.
            QPen stroke(t.accentHi, 1.8);
            stroke.setCapStyle(Qt::RoundCap);
            stroke.setJoinStyle(Qt::RoundJoin);
            p->setPen(stroke);
            p->setBrush(Qt::NoBrush);
            const QPointF a(tick.left() + 3.2, tick.top() + 7.2);
            const QPointF b(tick.left() + 5.7, tick.top() + 9.8);
            const QPointF c(tick.left() + 10.6, tick.top() + 4.2);
            p->drawPolyline(QPolygonF{a, b, c});
        }

        const int x = tick.right() + kGap;
        QFont face(QStringLiteral("IBM Plex Sans"));
        face.setPixelSize(12);
        QFont digits(QStringLiteral("IBM Plex Mono"));
        digits.setPixelSize(11);
        const QString name   = index.data(Qt::DisplayRole).toString();
        const QString detail = index.data(Qt::UserRole + 1).toString();

        if (twoLines_) {
            const int width = box.right() - kPadX - x;
            p->setFont(face);
            p->setPen(on ? t.text : t.textDim);
            p->drawText(QRect(x, box.top() + 5, width, 17), Qt::AlignVCenter | Qt::AlignLeft,
                        p->fontMetrics().elidedText(name, Qt::ElideMiddle, width));
            p->setFont(digits);
            p->setPen(t.textFaint);
            p->drawText(QRect(x, box.top() + 22, width, 15), Qt::AlignVCenter | Qt::AlignLeft,
                        p->fontMetrics().elidedText(detail, Qt::ElideRight, width));
        } else {
            const int right = box.right() - kPadX - kCntW;
            p->setFont(face);
            p->setPen(on ? t.text : t.textDim);
            p->drawText(QRect(x, box.top(), right - x, box.height()),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        p->fontMetrics().elidedText(name, Qt::ElideMiddle, right - x));
            p->setFont(digits);
            p->setPen(t.textFaint);
            p->drawText(QRect(right, box.top(), kCntW, box.height()),
                        Qt::AlignVCenter | Qt::AlignRight,
                        p->fontMetrics().elidedText(detail, Qt::ElideRight, kCntW));
        }

        p->restore();
    }

protected:
    /// THE WHOLE ROW TOGGLES — on a click anywhere in it and on Space — and
    /// only here. The list used to toggle on `itemClicked` while the base
    /// delegate ALSO toggled a click that landed on the style's own check
    /// indicator, which sits under the drawn box: a click on the box itself was
    /// two toggles and changed nothing, and a tick given with Space never
    /// reached the tally.
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override
    {
        const auto flip = [&] {
            const bool on = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            return model->setData(index, on ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
        };
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() != Qt::LeftButton ||
                !option.rect.contains(mouse->position().toPoint()))
                return false;
            return flip();
        }
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Space || key == Qt::Key_Select) return flip();
        }
        return false;
    }

private:
    ThemeMode theme_ = ThemeMode::Dark;
    bool twoLines_   = false;
};

} // namespace

// ------------------------------------------------------- ImportProbeThread --

ImportProbeThread::ImportProbeThread(core::Document& into, QString path, io::ImportOptions options,
                                     QObject* parent)
    : QThread(parent), into_(into), path_(std::move(path)), options_(std::move(options)),
      outcome_(core::Error{core::ErrorCode::Internal, "Okuma başlamadı."})
{}

void ImportProbeThread::cancel()
{
    stop_.request_stop();
}

void ImportProbeThread::run()
{
    outcome_ = io::probe_import(into_, path_.toStdString(), options_, stop_.get_token());
}

// ----------------------------------------------------------- ImportPreview --

ImportPreview::ImportPreview(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("importPreview"));
    setMinimumSize(360, 280);
    setCursor(Qt::OpenHandCursor);

    // The SAME renderer the symbol shelf uses, for the same reason: a second
    // preview renderer is a second answer to "what does this look like".
    backend_ = make_preview_backend();
}

ImportPreview::~ImportPreview() = default;

void ImportPreview::setDocument(const core::Document* doc)
{
    doc_ = doc;
    refit();
}

void ImportPreview::refit()
{
    if (doc_ != nullptr) {
        const core::Box2 extent = doc_->extent();
        if (!extent.empty()) view_.fit(extent, 0.06);
    }
    update();
}

void ImportPreview::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

void ImportPreview::resizeEvent(QResizeEvent* event)
{
    view_.set_viewport(width(), height());
    QWidget::resizeEvent(event);
    refit();
}

void ImportPreview::paintEvent(QPaintEvent*)
{
    const Tokens& t = tk(theme_);

    QPainter ground(this);
    ground.fillRect(rect(), t.bgCanvas);
    ground.end();

    if (doc_ == nullptr || backend_ == nullptr) return;

    render::build_scene(*doc_, view_, options_, draw_);

    render::FrameContext ctx;
    ctx.width_px           = width();
    ctx.height_px          = height();
    ctx.device_pixel_ratio = static_cast<float>(devicePixelRatioF());
    // Cast HERE for the reason `map_canvas.cpp` states: QWidget inherits QObject
    // and QPaintDevice both, so the two pointers are different addresses.
    ctx.target = static_cast<QPaintDevice*>(this);

    // TRANSPARENT, and this is what made the preview black in the light theme.
    // `Overlay::background_rgba` defaults to OPAQUE BLACK, so handing a default
    // overlay to the backend clears the whole widget before a single entity is
    // drawn — over the themed ground filled three lines above. In the dark theme
    // the canvas token is nearly black anyway and nobody could see it happening.
    //
    // Zero alpha means "leave what is already there", the same contract the
    // symbol shelf uses to keep its checkerboard, and here the ground is already
    // painted.
    render::Overlay empty;
    empty.background_rgba = 0;
    backend_->render(draw_, empty, ctx);
}

void ImportPreview::wheelEvent(QWheelEvent* event)
{
    const double notches = event->angleDelta().y() / 120.0;
    if (notches == 0.0) return;

    // THE CANVAS'S OWN FUNCTION, not a copy of it. This used to zoom the other
    // way from the main view and ignore the wheel settings besides, so pushing the
    // wheel away pulled back from a drawing that was about to be imported and
    // pushed into it once it was.
    const double factor =
        settings_ != nullptr ? wheel_zoom_factor(*settings_, notches) : std::pow(1.2, notches);

    const QPointF at = event->position();
    view_.zoom_at(render::ScreenPoint{at.x(), at.y()}, factor);
    update();
    event->accept();
}

void ImportPreview::mousePressEvent(QMouseEvent* event)
{
    dragging_ = true;
    dragFrom_ = event->position().toPoint();
    setCursor(Qt::ClosedHandCursor);
}

void ImportPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging_) return;
    const QPoint now = event->position().toPoint();

    // THE SAME SIGN THE CANVAS USES, and it was the other one here. The canvas
    // pans by `position - anchor`, which is the grab-and-drag every map has: the
    // drawing follows the hand. This subtracted the other way round, so the same
    // drag moved the preview one way and the imported drawing the other.
    view_.pan_pixels(now.x() - dragFrom_.x(), now.y() - dragFrom_.y());
    dragFrom_ = now;
    update();
}

void ImportPreview::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = false;
    setCursor(Qt::OpenHandCursor);
}

// ------------------------------------------------------------ ImportWizard --

ImportWizard::ImportWizard(Controller& controller, ThemeMode theme, QWidget* parent)
    : DialogFrame(parent), controller_(controller)
{
    setHeading(Glyph::Open, tr("İçe Aktar"));
    setModal(true);
    // A FILE DROPPED ON THE WINDOW IS A PICK, the one a hand makes from the
    // file manager. It is an addition: the path field and `Gözat…` are the
    // road the keyboard has (CLAUDE.md 5.15).
    setAcceptDrops(true);

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(buildFileStrip());

    auto* middle = new QWidget(body);
    auto* row    = new QHBoxLayout(middle);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(buildStage(), 1);
    row->addWidget(buildColumn());
    column->addWidget(middle, 1);
    column->addWidget(buildCommandStrip());
    setBody(body);

    cancel_ = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    // CLOSING STOPS A READ UNDER WAY: the destructor asks the thread to stop and
    // waits for it, which io.md R15 bounds at 100 ms.
    connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);

    go_ = new Button(ButtonRole::Primary, tr("İçe aktar"), std::nullopt, this);
    go_->setDefault(true);
    go_->setEnabled(false);
    connect(go_, &QPushButton::clicked, this, [this] {
        // THE WHOLE POINT OF THE WINDOW, in one line: the ticks become the
        // arguments, and the arguments go on the same command line a script
        // would write (Article 1.2).
        const QString line = lineFor();
        if (line.isEmpty()) return;
        line_ = line;
        accept();
    });
    footer()->addWidget(cancel_);
    footer()->addWidget(go_);

    ticker_ = new QTimer(this);
    ticker_->setInterval(100);
    connect(ticker_, &QTimer::timeout, this, [this] {
        progressText_->setText(
            tr("%1 okunuyor…  %2 sn")
                .arg(QFileInfo(probed_).fileName(),
                     QString::number(static_cast<double>(elapsed_.elapsed()) / 1000.0, 'f', 1)));
    });

    settle_ = new QTimer(this);
    settle_->setSingleShot(true);
    settle_->setInterval(kSettleMs);
    connect(settle_, &QTimer::timeout, this, &ImportWizard::startProbe);

    // THE SETTINGS WINDOW'S SIZE (design.md §4), the print window's sibling:
    // room for a drawing that can be read beside a column of choices. One size
    // for the whole job — the wizard grew a page at a time and the window
    // reflowed under the hand at every step. Never taller than the screen, and
    // never smaller than 940 × 600, where the drawing still reads.
    setMinimumSize(940, 600);
    const QRect room = QApplication::primaryScreen() != nullptr
                           ? QApplication::primaryScreen()->availableGeometry()
                           : QRect(0, 0, 1280, 800);
    resize(QSize(std::min(1180, room.width() - 80), std::min(740, room.height() - 80))
               .expandedTo(minimumSize()));

    // QUALIFIED, and not because a linter asked. This runs inside the
    // constructor, where the object is not yet an `ImportWizard` for dispatch
    // purposes; naming the function outright says the non-virtual call is the
    // intent rather than an accident a future subclass would silently inherit.
    ImportWizard::applyTheme(theme);
    refreshLine();
}

ImportWizard::~ImportWizard()
{
    if (probe_ != nullptr && probe_->isRunning()) {
        probe_->cancel();
        probe_->wait();
    }
}

void ImportWizard::applyTheme(ThemeMode mode)
{
    mode_ = mode;
    DialogFrame::applyTheme(mode);

    // A delegate is a QObject and not a QWidget, so `applyThemeToChildren` walks
    // straight past it — the same trap `SettingsDialog` fell into with its
    // sidebar, which is why `Themed` exists at all.
    // A static_cast and not a qobject_cast: the delegate has no Q_OBJECT — it
    // lives in an anonymous namespace where moc cannot follow — and this window
    // is the only thing that ever sets it.
    if (layers_ != nullptr)
        static_cast<LayerCheckDelegate*>(layers_->itemDelegate())->setTheme(mode);
    if (fields_ != nullptr)
        static_cast<LayerCheckDelegate*>(fields_->itemDelegate())->setTheme(mode);
    if (preview_ != nullptr) preview_->applyTheme(mode);
    update();
}

// ---- the window's parts ------------------------------------------------------

QWidget* ImportWizard::buildFileStrip()
{
    // THE PICK, ACROSS THE TOP: the one thing every import starts with, in the
    // place a toolbar has, so the window reads top to bottom in the order the
    // work is done — the file, what is in it, what to take.
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("importFileStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* column = new QVBoxLayout(strip);
    column->setContentsMargins(16, 12, 16, 10);
    column->setSpacing(4);

    auto* line = new QHBoxLayout;
    line->setSpacing(6);
    auto* caption = new QLabel(tr("Dosya"), strip);
    caption->setObjectName(QStringLiteral("formCaption"));
    caption->setFixedWidth(kCaption);
    line->addWidget(caption);

    pathField_ = new QLineEdit(strip);
    pathField_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    pathField_->setPlaceholderText(io::dwg_backend_available()
                                       ? tr("DXF, DWG, NCZ, Shapefile ya da GeoPackage yolu")
                                       : tr("DXF, NCZ, Shapefile ya da GeoPackage yolu"));
    pathField_->setAccessibleName(tr("İçe aktarılacak dosya"));
    // THE DROP IS THE WINDOW'S. A line edit takes a dropped file as the text of
    // its address, `file:///…`, which is no path this program can open.
    pathField_->setAcceptDrops(false);
    caption->setBuddy(pathField_);
    connect(pathField_, &QLineEdit::textChanged, this,
            [this](const QString& text) { judgePick(text); });
    line->addWidget(pathField_, 1);

    auto* browse = new Button(ButtonRole::Secondary, tr("Gözat…"), Glyph::Open, strip);
    connect(browse, &QPushButton::clicked, this, &ImportWizard::browse);
    line->addWidget(browse);
    column->addLayout(line);

    // WHAT THE FILE IS, under its path and in the value column: an indent held
    // by a layout, because a label's own margins do not survive its first
    // polish (`LayoutDesigner::help`).
    fileFacts_ = new QLabel(strip);
    fileFacts_->setObjectName(QStringLiteral("formHelp"));
    fileFacts_->setVisible(false);
    auto* facts = new QHBoxLayout;
    facts->setContentsMargins(kCaption + 6, 0, 0, 0);
    facts->addWidget(fileFacts_);
    column->addLayout(facts);
    return strip;
}

QWidget* ImportWizard::buildStage()
{
    stage_ = new QStackedWidget(this);
    stage_->addWidget(buildInvitation());
    stage_->addWidget(buildReading());
    stage_->addWidget(buildDrawing());
    return stage_;
}

QWidget* ImportWizard::buildInvitation()
{
    // AN EMPTY WINDOW IS AN INVITATION: what to do, what happens when it is
    // done, and — the dashed frame round it — that a file can be dropped right
    // here. The page used to be a lone path field over a reference list, in a
    // window a thousand pixels wide; the reference is now in the column, where
    // the layers will be once there is a file.
    auto* page  = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 24, 24, 24);

    dropZone_ = new QWidget(page);
    dropZone_->setObjectName(QStringLiteral("importDropZone"));
    dropZone_->setAttribute(Qt::WA_StyledBackground, true);
    auto* inner = new QVBoxLayout(dropZone_);
    inner->setContentsMargins(40, 32, 40, 32);
    inner->addStretch(2);

    auto* card   = new QWidget(dropZone_);
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    card->setMaximumWidth(520);

    auto* title = new QLabel(tr("Bir dosya seçin"), card);
    title->setObjectName(QStringLiteral("importInviteTitle"));
    column->addWidget(title);
    column->addSpacing(8);

    // WHAT THIS WINDOW PROMISES, before anything is chosen: reading changes
    // nothing, and the import is one step that one undo takes back.
    auto* lead = new QLabel(tr("Yolu yukarıya yazın, Gözat… ile seçin ya da dosyayı buraya "
                               "bırakın. Dosya önce yalnızca okunur: katmanlarını ve alanlarını "
                               "seçip İçe aktar'a basana kadar çizime hiçbir şey eklenmez, "
                               "aktarılan da tek adımda geri alınır."),
                            card);
    lead->setObjectName(QStringLiteral("importInviteText"));
    lead->setWordWrap(true);
    column->addWidget(lead);

    // THE VERDICT, where the pick is judged. Hidden while there is nothing to
    // say, because a banner that is always up says nothing.
    verdict_ = new Banner(Tone::Danger, QString(), QString(), card);
    verdict_->setVisible(false);
    column->addSpacing(16);
    column->addWidget(verdict_);

    auto* centre = new QHBoxLayout;
    centre->addStretch(1);
    centre->addWidget(card, 6);
    centre->addStretch(1);
    inner->addLayout(centre);
    inner->addStretch(3);
    outer->addWidget(dropZone_);
    return page;
}

QWidget* ImportWizard::buildReading()
{
    // THE READ, AND WHAT BECAME OF IT, in the place the drawing will be. The
    // 2 px accent strip of design.md §11 is indeterminate on purpose: the
    // readers stream and report no fraction, and a bar that filled itself on a
    // timer would be a lie told at the exact moment the user is deciding
    // whether to wait. The honest figures — the seconds, a stop that works —
    // are beside it.
    auto* page  = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(32, 28, 32, 28);
    outer->addStretch(2);

    auto* card   = new QWidget(page);
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);
    card->setMaximumWidth(460);

    progressText_ = new QLabel(card);
    progressText_->setObjectName(QStringLiteral("importInviteText"));
    column->addWidget(progressText_);
    progress_ = new ProgressStrip(card);
    column->addWidget(progress_);

    // THE STOP IS A BUTTON OF ITS OWN. It used to be `İptal` renamed for the
    // length of the read, so the one button a user reaches for to close a
    // window meant something else for as long as the file took.
    stopRead_ = new Button(ButtonRole::Secondary, tr("Okumayı durdur"), std::nullopt, card);
    stopRead_->setControlSize(ControlSize::Compact);
    connect(stopRead_, &QPushButton::clicked, this, [this] {
        if (probe_ == nullptr) return;
        restart_ = false;
        probe_->cancel();
    });
    auto* stopRow = new QHBoxLayout;
    stopRow->addWidget(stopRead_);
    stopRow->addStretch(1);
    column->addLayout(stopRow);

    // WHY THE READ DID NOT FINISH — the reader's own sentence, verbatim, since
    // rewriting it here would put a second wording of every io failure in the
    // shell — or that the user stopped it. Either way the same file is one
    // press from being read again.
    failure_ = new Banner(Tone::Danger, tr("Dosya okunamadı"), QString(), card);
    connect(failure_->addButton(tr("Yeniden oku")), &QPushButton::clicked, this,
            &ImportWizard::startProbe);
    failure_->setVisible(false);
    column->addWidget(failure_);
    stopped_ = new Banner(Tone::Neutral, tr("Okuma durduruldu"),
                          tr("Çizime hiçbir şey eklenmedi. Aynı dosyayı yeniden okuyabilir ya da "
                             "başka bir dosya seçebilirsiniz."),
                          card);
    connect(stopped_->addButton(tr("Yeniden oku")), &QPushButton::clicked, this,
            &ImportWizard::startProbe);
    stopped_->setVisible(false);
    column->addWidget(stopped_);

    auto* centre = new QHBoxLayout;
    centre->addStretch(1);
    centre->addWidget(card, 6);
    centre->addStretch(1);
    outer->addLayout(centre);
    outer->addStretch(3);
    return page;
}

QWidget* ImportWizard::buildDrawing()
{
    auto* page   = new QWidget(this);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // THE READER'S NOTICES, ABOVE THE DRAWING AND ONE BANNER EACH.
    //
    // They were flattened into one grey paragraph UNDER the preview, with their
    // level spelled as a lower-case word in front of the sentence — so "this
    // file declares no coordinate system" and "this file declares no units" sat
    // in the same weight as the object count, below the picture, at the bottom
    // of the page. Those two are the ones that put a parcel in the wrong place
    // at the wrong scale, and what leaves this program is a document somebody
    // signs. They are the most consequential words in the window and they were
    // the quietest.
    //
    // Above the drawing because they are about the FILE, not about the picture;
    // and the preview is the stretching item, so a file with four notices gives
    // them their room and keeps the drawing smaller. That is the right
    // priority: the drawing is a reassurance, the notices are a decision.
    auto* noticeBox = new QWidget(page);
    notices_        = new QVBoxLayout(noticeBox);
    notices_->setSpacing(8);
    notices_->setContentsMargins(16, 12, 16, 12);
    noticeBox->setVisible(false);
    column->addWidget(noticeBox);

    // THE DRAWING, EDGE TO EDGE: what is in the file, drawn by the canvas's own
    // renderer, and the one large thing in the window.
    preview_ = new ImportPreview(page);
    preview_->useSettings(&controller_.bus().app_settings());
    preview_->setAccessibleName(tr("Dosyanın çizimi"));
    column->addWidget(preview_, 1);

    // THE FACTS STAY QUIET, because they are facts: what was read, how much of
    // it, in which system. Metadata reads as metadata — in the darkest strip,
    // as the print window's summary and the designer's status line do.
    summary_ = new QLabel(page);
    summary_->setObjectName(QStringLiteral("importSummary"));
    summary_->setAttribute(Qt::WA_StyledBackground, true);
    summary_->setWordWrap(true);
    summary_->setMinimumHeight(26);
    summary_->setAccessibleName(tr("Dosyanın özeti"));
    column->addWidget(summary_);
    return page;
}

QWidget* ImportWizard::buildColumn()
{
    // THE CHOICES, IN A PROPERTY COLUMN: the print window's and the layout
    // designer's, so the three windows that stand between a drawing and a file
    // read as one program.
    auto* side = new QWidget(this);
    side->setObjectName(QStringLiteral("importColumn"));
    side->setAttribute(Qt::WA_StyledBackground, true);
    side->setFixedWidth(kColumn);
    auto* outer = new QVBoxLayout(side);
    outer->setContentsMargins(13, 12, 14, 12); // 1 of the 13 is the edge
    outer->setSpacing(0);

    column_ = new QStackedWidget(side);

    // BEFORE A FILE: what the column will hold, and which files this build
    // reads, each with its answer.
    auto* waiting = new QWidget(side);
    auto* waitCol = new QVBoxLayout(waiting);
    waitCol->setContentsMargins(0, 0, 0, 0);
    waitCol->setSpacing(0);
    auto* note = new QLabel(tr("Dosya okununca katmanlarını ve alanlarını burada seçersiniz. Ne "
                               "işaretlerseniz çizime o gelir."),
                            waiting);
    note->setObjectName(QStringLiteral("formHelp"));
    note->setWordWrap(true);
    waitCol->addWidget(note);
    waitCol->addSpacing(20);
    auto* heading = new QLabel(tr("OKUNAN BİÇİMLER"), waiting);
    heading->setObjectName(QStringLiteral("groupCaption"));
    waitCol->addWidget(heading);
    waitCol->addSpacing(6);

    // FROM THE ALLOW-LIST, NEVER FROM A LIST IN THIS FILE. `io.md` P7 holds the
    // driver set in /cmake and hands it to /src/io as a compile definition; a
    // second copy here would be the "hand-maintained second list" 5.10 forbids,
    // and it would go stale the first time a driver is added.
    //
    // EACH ROW ENDS IN A BADGE: "okunur ve yazılır" and "bu sürümde okunmaz"
    // are the one fact a user scans this list for, and a badge says it at a
    // glance where a sentence has to be read.
    const auto formatRow = [waiting, waitCol](const QString& extension, const QString& label,
                                              const QString& capability, Tone tone,
                                              const QString& hint = QString()) {
        auto* holder = new QWidget(waiting);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(0, 4, 0, 4);
        line->setSpacing(10);

        auto* name = new QLabel(extension, holder);
        name->setObjectName(QStringLiteral("importFormatExt"));
        name->setFixedWidth(48);
        line->addWidget(name, 0, Qt::AlignTop);

        auto* what = new QLabel(label, holder);
        what->setObjectName(QStringLiteral("importInviteText"));
        what->setWordWrap(true);
        what->setMinimumWidth(1);
        line->addWidget(what, 1);

        auto* mark = new Badge(capability, tone, holder);
        if (!hint.isEmpty()) mark->setToolTip(hint);
        line->addWidget(mark, 0, Qt::AlignTop);
        waitCol->addWidget(holder);
    };

    for (const io::VectorFormat& f : io::vector_formats()) {
        if (!f.read) continue;
        formatRow(QString::fromStdString(f.extension).toUpper(), QString::fromStdString(f.label),
                  f.write ? tr("okunur, yazılır") : tr("okunur"),
                  f.write ? Tone::Neutral : Tone::Accent);
    }

    // A Netcad drawing: read by this program's own reader, in every build
    // (io/ncz.hpp), so it is on the list whatever else the build has.
    formatRow(tr(".NCZ"), tr("Netcad çizimi"), tr("okunur"), Tone::Accent);

    // The io layer's status names a CMake flag, which is for the person who
    // builds the program; the person who uses it is told what to do instead.
    if (io::dwg_backend_available())
        formatRow(tr(".DWG"), tr("AutoCAD çizimi"), tr("okunur"), Tone::Accent,
                  QString::fromStdString(io::dwg_backend_status()));
    else
        formatRow(tr(".DWG"), tr("AutoCAD çizimi; DXF olarak kaydedip aktarın"), tr("okunmaz"),
                  Tone::Danger, QString::fromStdString(io::dwg_backend_status()));
    waitCol->addStretch(1);
    column_->addWidget(waiting);

    auto* chosenBox = new QWidget(side);
    auto* col       = new QVBoxLayout(chosenBox);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(12);
    // TWO PANES AND A SWITCH, the designer's `Öğe | Sayfa`: the layers decide
    // what geometry comes, the fields decide what table comes with it.
    paneSwitch_ = new Segment(chosenBox);
    paneSwitch_->addOption(tr("Katmanlar"), tr("Hangi katmanların geleceği"));
    paneSwitch_->addOption(tr("Alanlar"), tr("Hangi öznitelik alanlarının sütun olacağı"));
    paneSwitch_->setControlSize(ControlSize::Compact);
    paneSwitch_->setAccessibleName(tr("Seçim bölmesi"));
    connect(paneSwitch_, &Segment::currentChanged, this,
            [this](int at) { panes_->setCurrentIndex(std::max(0, at)); });
    col->addWidget(paneSwitch_);

    panes_ = new QStackedWidget(chosenBox);
    panes_->addWidget(buildLayerPane());
    panes_->addWidget(buildFieldPane());
    col->addWidget(panes_, 1);
    column_->addWidget(chosenBox);

    outer->addWidget(column_, 1);
    return side;
}

QWidget* ImportWizard::buildLayerPane()
{
    auto* pane   = new QWidget(this);
    auto* column = new QVBoxLayout(pane);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setSpacing(2);
    auto* heading = new QLabel(tr("KATMANLAR"), pane);
    heading->setObjectName(QStringLiteral("groupCaption"));
    head->addWidget(heading);
    head->addStretch(1);
    auto* all = new Button(ButtonRole::Ghost, tr("Tümü"), std::nullopt, pane);
    all->setControlSize(ControlSize::Compact);
    all->setToolTip(tr("Görünen bütün katmanları işaretler"));
    connect(all, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    auto* none = new Button(ButtonRole::Ghost, tr("Hiçbiri"), std::nullopt, pane);
    none->setControlSize(ControlSize::Compact);
    none->setToolTip(tr("Görünen bütün katmanların işaretini kaldırır"));
    connect(none, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    head->addWidget(all);
    head->addWidget(none);
    column->addLayout(head);

    filter_ = new QLineEdit(pane);
    filter_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    filter_->setPlaceholderText(tr("Katman ara"));
    filter_->setAccessibleName(tr("Katman ara"));
    connect(filter_, &QLineEdit::textChanged, this, [this](const QString& needle) {
        // Turkish-folded, because `İMAR` and `imar` are the same layer to a user
        // and `std::tolower` gets exactly that pair wrong (CLAUDE.md 5.6).
        const std::string want = core::turkish_fold_key(needle.trimmed().toStdString());
        for (int i = 0; i < layers_->count(); ++i) {
            QListWidgetItem* line = layers_->item(i);
            const std::string has =
                core::turkish_fold_key(line->data(Qt::DisplayRole).toString().toStdString());
            line->setHidden(!want.empty() && has.find(want) == std::string::npos);
        }
    });
    column->addWidget(filter_);

    layers_ = new QListWidget(pane);
    layers_->setObjectName(QStringLiteral("importLayerList"));
    layers_->setFrameShape(QFrame::NoFrame);
    layers_->setMouseTracking(true);
    layers_->setUniformItemSizes(true);
    layers_->setSelectionMode(QAbstractItemView::NoSelection);
    layers_->setItemDelegate(new LayerCheckDelegate(layers_));
    layers_->setAccessibleName(tr("Gelecek katmanlar"));
    // EVERY TICK, HOWEVER GIVEN — a click, Space, `Tümü` — arrives here as the
    // item changing, so the drawing, the tally and the line cannot miss one.
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        if (filling_) return;
        applyVisibility();
        refreshTally();
        refreshLine();
    });
    column->addWidget(layers_, 1);

    tally_ = new QLabel(pane);
    tally_->setObjectName(QStringLiteral("formHelp"));
    tally_->setWordWrap(true);
    column->addWidget(tally_);
    return pane;
}

QWidget* ImportWizard::buildFieldPane()
{
    // THE TABLE BESIDE THE GEOMETRY. A Shapefile or a GeoPackage carries fields
    // — ada, parsel, nitelik — and a parcel imported without them is half a
    // parcel. Each row: the field, and under it its layer, what it becomes and
    // a first value, so `ada_no` and `parsel_no` can be told apart before
    // anything is written. The ticked rows become `alanlar=` on the line.
    auto* pane   = new QWidget(this);
    auto* column = new QVBoxLayout(pane);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setSpacing(2);
    auto* heading = new QLabel(tr("ALANLAR"), pane);
    heading->setObjectName(QStringLiteral("groupCaption"));
    head->addWidget(heading);
    head->addStretch(1);
    auto* all = new Button(ButtonRole::Ghost, tr("Tümü"), std::nullopt, pane);
    all->setControlSize(ControlSize::Compact);
    all->setToolTip(tr("Bütün alanları işaretler"));
    connect(all, &QPushButton::clicked, this, [this] { setAllFieldsChecked(true); });
    auto* none = new Button(ButtonRole::Ghost, tr("Hiçbiri"), std::nullopt, pane);
    none->setControlSize(ControlSize::Compact);
    none->setToolTip(tr("Bütün alanların işaretini kaldırır; yalnız geometri gelir"));
    connect(none, &QPushButton::clicked, this, [this] { setAllFieldsChecked(false); });
    head->addWidget(all);
    head->addWidget(none);
    column->addLayout(head);

    fields_ = new QListWidget(pane);
    fields_->setObjectName(QStringLiteral("importLayerList"));
    fields_->setFrameShape(QFrame::NoFrame);
    fields_->setMouseTracking(true);
    fields_->setUniformItemSizes(true);
    fields_->setSelectionMode(QAbstractItemView::NoSelection);
    auto* delegate = new LayerCheckDelegate(fields_);
    delegate->setTwoLines(true);
    fields_->setItemDelegate(delegate);
    fields_->setAccessibleName(tr("Sütun olacak alanlar"));
    connect(fields_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        if (filling_) return;
        refreshFieldTally();
        refreshLine();
    });
    column->addWidget(fields_, 1);

    fieldTally_ = new QLabel(pane);
    fieldTally_->setObjectName(QStringLiteral("formHelp"));
    fieldTally_->setWordWrap(true);
    column->addWidget(fieldTally_);
    return pane;
}

QWidget* ImportWizard::buildCommandStrip()
{
    // THE LINE THE WINDOW WILL RUN, the width of the window and under
    // everything, as the print window shows its own: it is what is sent — to
    // the bus, as a script would send it — and a user who reads it has learnt
    // the command.
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("importCommandStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(16, 8, 16, 8);
    row->setSpacing(12);
    auto* said = new QLabel(tr("KOMUT"), strip);
    said->setObjectName(QStringLiteral("groupCaption"));
    row->addWidget(said, 0, Qt::AlignVCenter);
    command_ = new QLabel(strip);
    command_->setObjectName(QStringLiteral("commandPreview"));
    command_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    command_->setWordWrap(true);
    command_->setAccessibleName(tr("Çalıştırılacak komut satırı"));
    row->addWidget(command_, 1);
    return strip;
}

// ---- acting ------------------------------------------------------------------

void ImportWizard::judgePick(const QString& text)
{
    settle_->stop();
    const QFileInfo about(text.trimmed());
    const bool exists = about.isFile();

    fileFacts_->setText(
        exists ? tr("%1 dosyası, %2, son değişiklik %3")
                     .arg(about.suffix().toUpper(), readableSize(about.size()),
                          about.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm")))
               : QString());
    fileFacts_->setVisible(exists);

    // WHETHER THIS PROGRAM CAN OPEN IT, decided HERE. The window has the
    // allow-list and used to ignore it: any existing file was accepted, so a
    // `.dwg` on a build without the reader, or a `.txt`, was refused only after
    // the user had committed to the flow. A window that knows the answer has to
    // give it where the question is asked.
    QString why;
    if (exists) {
        const QString suffix = QStringLiteral(".") + about.suffix().toLower();
        bool readable        = false;
        for (const io::VectorFormat& f : io::vector_formats())
            if (f.read && QString::fromStdString(f.extension).toLower() == suffix) readable = true;
        if (suffix == QStringLiteral(".dwg")) readable = io::dwg_backend_available();
        // A Netcad drawing needs no library: every build reads it (io/ncz.hpp).
        if (suffix == QStringLiteral(".ncz")) readable = true;

        if (!readable)
            why = suffix == QStringLiteral(".dwg")
                      ? tr("Bu sürüm DWG okumuyor. Çizimi AutoCAD'de DXF olarak kaydedip onu "
                           "aktarın.")
                      : tr("Okunan biçimler sağda listeli. Dosyanın uzantısını denetleyin.");
    }
    pickReadable_ = exists && why.isEmpty();

    verdict_->setTitle(why.isEmpty() ? QString()
                                     : tr("%1 dosyası okunamıyor").arg(about.suffix().toUpper()));
    verdict_->setText(why);
    verdict_->applyTheme(mode_);
    verdict_->setVisible(!why.isEmpty());

    // A NEW PICK IS NOT THE DRAWING ON THE STAGE. The moment the path stops
    // naming the file that was read, what is shown and what would be imported
    // part company — so the stage goes back to the invitation, the column to
    // its note, and the line empties, until the new file has been read.
    const bool same = scratch_ && !probed_.isEmpty() && text.trimmed() == probed_ &&
                      stage_->currentIndex() == kStageDrawing;
    if (probe_ != nullptr && text.trimmed() != probed_) {
        // The file being read is no longer the one named: its read is dropped
        // (`probeFinished` sees the path moved on).
        probe_->cancel();
    } else if (!same && probe_ == nullptr) {
        stage_->setCurrentIndex(kStageInvite);
        column_->setCurrentIndex(0);
    }
    refreshLine();

    // AND A FILE THIS BUILD READS IS READ, once the typing has settled: the
    // read changes nothing, so it does not wait for a press.
    if (pickReadable_ && !same) settle_->start();
}

void ImportWizard::setPath(const QString& path)
{
    pathField_->setText(path);
}

void ImportWizard::beginWith(const QString& path)
{
    setPath(path);
    if (pickReadable_) startProbe();
}

bool ImportWizard::probeSettle(int page, int msecs)
{
    // Until the drawing is up, not merely until the thread stops: the thread's
    // `finished` is delivered as an event, and a check made between the two saw
    // a finished read and a window still showing the read.
    QElapsedTimer clock;
    clock.start();
    while ((probe_ != nullptr || settle_->isActive()) && clock.elapsed() < msecs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!scratch_ || stage_->currentIndex() != kStageDrawing) return false;
    showPane(page >= 2 ? 1 : 0);
    QCoreApplication::processEvents();
    return true;
}

void ImportWizard::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.front().isLocalFile()) return;
    event->acceptProposedAction();
    // THE FRAME ANSWERS THE HAND: the dashed edge turns to the accent while a
    // file is held over the window, so the drop is seen to be welcome.
    dropZone_->setProperty("armed", true);
    style()->unpolish(dropZone_);
    style()->polish(dropZone_);
}

void ImportWizard::dragLeaveEvent(QDragLeaveEvent* event)
{
    DialogFrame::dragLeaveEvent(event);
    dropZone_->setProperty("armed", false);
    style()->unpolish(dropZone_);
    style()->polish(dropZone_);
}

void ImportWizard::dropEvent(QDropEvent* event)
{
    dropZone_->setProperty("armed", false);
    style()->unpolish(dropZone_);
    style()->polish(dropZone_);
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.front().isLocalFile()) return;
    event->acceptProposedAction();
    beginWith(urls.front().toLocalFile());
}

void ImportWizard::browse()
{
    QStringList filters;
    filters << tr("Desteklenen tüm dosyalar (%1)")
                   .arg(QStringLiteral("*.dxf *.shp *.gpkg *.dwg *.ncz"));
    for (const io::VectorFormat& f : io::vector_formats())
        if (f.read)
            filters << QStringLiteral("%1 (*%2)")
                           .arg(QString::fromStdString(f.label),
                                QString::fromStdString(f.extension));
    if (io::dwg_backend_available()) filters << tr("AutoCAD DWG (*.dwg)");
    filters << tr("Netcad çizimi (*.ncz)");

    const QString picked = QFileDialog::getOpenFileName(
        this, tr("İçe aktarılacak dosya"), QFileInfo(pathField_->text()).absolutePath(),
        filters.join(QStringLiteral(";;")));
    if (!picked.isEmpty()) beginWith(picked);
}

void ImportWizard::startProbe()
{
    settle_->stop();
    const QString path = pathField_->text().trimmed();
    if (path.isEmpty() || !pickReadable_) return;

    // A READ OF AN OLDER PICK IS STILL RUNNING: it is stopped, and this one
    // starts when it has returned (`probeFinished`). Two threads writing two
    // scratch documents is how a preview ends up drawing the wrong file.
    if (probe_ != nullptr) {
        restart_ = true;
        probe_->cancel();
        return;
    }

    // BEFORE THE OLD ONE IS FREED. A second read replaces the scratch document,
    // and a preview still pointing at the previous one paints freed memory the
    // moment anything asks it to repaint.
    preview_->setDocument(nullptr);
    filling_ = true;
    layers_->clear();
    fields_->clear();
    filling_ = false;
    scratch_ = std::make_unique<core::Document>();
    found_   = io::ImportProbe{};
    probed_  = path;

    stage_->setCurrentIndex(kStageReading);
    column_->setCurrentIndex(0);
    failure_->setVisible(false);
    stopped_->setVisible(false);
    progressText_->setVisible(true);
    progress_->setVisible(true);
    stopRead_->setVisible(true);
    progressText_->setText(tr("%1 okunuyor…").arg(QFileInfo(path).fileName()));
    elapsed_.start();
    ticker_->start();
    refreshLine();

    // The drawing's own system and its unit, resolved the way `io/service.cpp`
    // resolves them for the command: the probe compares what it is given and
    // never guesses (io.md R20), and a DXF that names no unit is read in the
    // project's unit here exactly as İÇEAKTAR will read it.
    io::ImportOptions options;
    const core::Crs& mine    = controller_.document().crs();
    options.project_crs      = mine.resolved() ? "EPSG:" + std::to_string(mine.epsg()) : mine.id();
    options.project_meridian = mine.central_meridian_deg();
    options.drawing_unit =
        core::drawing_unit_from_setting(controller_.bus().setting("core.cizim.birim").as_enum());

    probe_ = new ImportProbeThread(*scratch_, path, std::move(options), this);
    connect(probe_, &QThread::finished, this, &ImportWizard::probeFinished);
    probe_->start();
}

void ImportWizard::probeFinished()
{
    ticker_->stop();

    const core::Result<io::ImportProbe>& outcome = probe_->outcome();
    probe_->deleteLater();
    probe_ = nullptr;

    // THE PICK MOVED ON DURING THE READ: what came back is the old file's. The
    // new one is read now if the read was asked for, or when the typing settles;
    // a pick this build cannot read goes back to the invitation and its verdict.
    if (restart_ || pathField_->text().trimmed() != probed_) {
        const bool again = restart_ && pickReadable_;
        restart_         = false;
        if (again) {
            startProbe();
            return;
        }
        if (!settle_->isActive()) {
            stage_->setCurrentIndex(kStageInvite);
            column_->setCurrentIndex(0);
        }
        refreshLine();
        return;
    }

    if (!outcome) {
        progressText_->setVisible(false);
        progress_->setVisible(false);
        stopRead_->setVisible(false);
        if (outcome.error().code == core::ErrorCode::Cancelled) {
            stopped_->setVisible(true);
        } else {
            failure_->setText(QString::fromStdString(outcome.error().message));
            failure_->setVisible(true);
        }
        refreshLine();
        return;
    }

    found_ = outcome.value();

    filling_ = true;
    for (const auto& [name, count] : found_.layers) {
        auto* row = new QListWidgetItem(QString::fromStdString(name), layers_);
        row->setData(Qt::UserRole + 1, grouped(count));
        // EVERYTHING TICKED TO BEGIN WITH: the user asked to import this file,
        // and a checklist that starts empty makes them do the work twice.
        row->setCheckState(Qt::Checked);
    }

    // ONE ROW PER FIELD NAME, ticked to begin with for the same reason the
    // layers are. `alanlar=` takes names, and a name that four layers share is
    // one column: the rows used to be one per layer and field, so a user could
    // untick `ada_no` under BİNA while it stayed ticked under the parcels — a
    // box that changed nothing. The name on the first line; what it becomes, a
    // first value and where it comes from on the second.
    struct Named
    {
        const io::VectorField* first = nullptr;
        QStringList layers;
        QString sample;
    };

    std::vector<std::pair<QString, Named>> named;
    for (const io::VectorField& f : found_.fields) {
        const QString name = QString::fromStdString(f.name);
        auto at =
            std::ranges::find_if(named, [&name](const auto& one) { return one.first == name; });
        if (at == named.end()) at = named.insert(named.end(), {name, Named{.first = &f}});
        at->second.layers << QString::fromStdString(f.layer);
        if (at->second.sample.isEmpty()) at->second.sample = QString::fromStdString(f.sample);
    }
    for (const auto& [name, one] : named) {
        auto* row = new QListWidgetItem(name, fields_);
        row->setData(Qt::UserRole, name);
        QStringList detail{type_label(one.first->type)};
        if (!one.sample.isEmpty()) detail << one.sample;
        detail << (one.layers.size() == 1 ? one.layers.front()
                                          : tr("%1 katmanda").arg(one.layers.size()));
        row->setData(Qt::UserRole + 1, detail.join(QStringLiteral(" · ")));
        row->setToolTip(
            tr("Sütun kimliği: %1\nKatmanlar: %2")
                .arg(QString::fromStdString(one.first->id), one.layers.join(QStringLiteral(", "))));
        row->setCheckState(Qt::Checked);
    }
    filling_ = false;

    QStringList said;
    said << tr("%1, %2 nesne, %3 katman")
                .arg(QString::fromStdString(found_.driver), grouped(found_.entities),
                     grouped(static_cast<std::uint64_t>(found_.layers.size())));
    if (!found_.crs.empty()) said << QString::fromStdString(found_.crs);

    // AND THE NOTICES GET THEIR OWN RANK. The level the reader already recorded
    // picks the tone, so the window stops spelling it as a word in front of a
    // sentence: `Warning` is something assumed and the drawing may be wrong,
    // `Degraded` and `Skipped` say something did not come through whole, `Error`
    // is a failure the reader survived, `Info` only informs.
    while (QLayoutItem* old_item = notices_->takeAt(0)) {
        if (QWidget* gone = old_item->widget()) gone->deleteLater();
        delete old_item;
    }
    for (const io::Diagnostic& line : found_.diagnostics.ordered()) {
        // The reader's hint about `alanlar=` is for the command line; here the
        // field pane IS that choice, so the hint would send the user elsewhere.
        if (line.text.find("alanlar=") != std::string::npos) continue;

        // AN `Info` IS A FACT, NOT A NOTICE. "the types read were ARC, CIRCLE,
        // LINE" is the same kind of statement as the object count, and a banner
        // for it costs the notices their rank: four banners on one small file
        // and the two that matter stop standing out. Facts join the facts.
        if (line.level == io::Severity::Info) {
            said << QString::fromStdString(line.text);
            continue;
        }

        const Tone tone = line.level == io::Severity::Error ? Tone::Danger : Tone::Warn;

        // THE FACT IS THE TITLE, THE FIX IS THE BODY. A reader's notice is one
        // long sentence run — what was assumed, and then what to do if the
        // assumption is wrong — so the first sentence is the heading and the
        // rest is the instruction under it. That is the shape a banner has, and
        // the shape an error should have: what happened, then how to fix it.
        const QString whole  = QString::fromStdString(line.text).trimmed();
        const qsizetype stop = whole.indexOf(QStringLiteral(". "));
        const QString title  = stop > 0 ? whole.left(stop + 1) : whole;
        const QString rest   = stop > 0 ? whole.mid(stop + 2).trimmed() : QString();
        auto* notice         = new Banner(tone, title, rest, notices_->parentWidget());
        notice->applyTheme(mode_);
        notices_->addWidget(notice);
    }
    notices_->parentWidget()->setVisible(notices_->count() > 0);
    summary_->setText(said.join(QStringLiteral("  ·  ")));

    preview_->setDocument(scratch_.get());
    stage_->setCurrentIndex(kStageDrawing);
    column_->setCurrentIndex(1);
    showPane(0);
    refreshTally();
    refreshFieldTally();
    refreshLine();
}

void ImportWizard::showPane(int pane)
{
    paneSwitch_->setCurrent(pane);
    panes_->setCurrentIndex(pane);
}

void ImportWizard::refreshFieldTally()
{
    if (fields_->count() == 0) {
        fieldTally_->setText(tr("Bu dosyada öznitelik alanı yok; yalnız geometri okunur."));
        return;
    }
    int ticked = 0;
    for (int i = 0; i < fields_->count(); ++i)
        if (fields_->item(i)->checkState() == Qt::Checked) ++ticked;
    fieldTally_->setText(tr("%1 / %2 alan sütun olur. Sütun kimliği alan adının küçük harfe "
                            "indirilmiş biçimidir; birkaç katmanda geçen bir alan tek sütundur.")
                             .arg(ticked)
                             .arg(fields_->count()));
}

void ImportWizard::setAllFieldsChecked(bool on)
{
    filling_ = true;
    for (int i = 0; i < fields_->count(); ++i)
        fields_->item(i)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    filling_ = false;
    refreshFieldTally();
    refreshLine();
}

QStringList ImportWizard::chosenFields() const
{
    QStringList picked;
    for (int i = 0; i < fields_->count(); ++i) {
        const QListWidgetItem* row = fields_->item(i);
        if (row->checkState() != Qt::Checked) continue;
        const QString name = row->data(Qt::UserRole).toString();
        if (!picked.contains(name)) picked << name;
    }
    return picked;
}

void ImportWizard::setAllChecked(bool on)
{
    filling_ = true;
    for (int i = 0; i < layers_->count(); ++i) {
        QListWidgetItem* row = layers_->item(i);
        if (row->isHidden()) continue; // the filter is showing a subset; honour it
        row->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    filling_ = false;
    applyVisibility();
    refreshTally();
    refreshLine();
}

QStringList ImportWizard::chosen() const
{
    QStringList picked;
    for (int i = 0; i < layers_->count(); ++i)
        if (layers_->item(i)->checkState() == Qt::Checked) picked << layers_->item(i)->text();
    return picked;
}

void ImportWizard::applyVisibility()
{
    if (!scratch_) return;

    // Through a Transaction, which is the sanctioned route to a document even
    // when the document is a scratch one nobody will ever save (Article 5.9).
    // The inverse Ops are dropped with the transaction: there is no undo stack
    // here and nothing to put on one.
    command::Transaction tx(*scratch_, "Önizleme görünürlüğü");
    for (int i = 0; i < layers_->count(); ++i) {
        const QListWidgetItem* row = layers_->item(i);
        const core::LayerId slot   = scratch_->find_layer(row->text().toStdString());
        if (slot != core::kNoLayer)
            (void)tx.set_layer_visible(slot, row->checkState() == Qt::Checked);
    }
    preview_->update();
}

void ImportWizard::refreshTally()
{
    std::uint64_t entities = 0;
    int ticked             = 0;
    for (int i = 0; i < layers_->count(); ++i) {
        if (layers_->item(i)->checkState() != Qt::Checked) continue;
        ++ticked;
        entities += found_.layers[static_cast<std::size_t>(i)].second;
    }

    tally_->setText(tr("%1 / %2 katman işaretli; %3 nesne aktarılacak.")
                        .arg(ticked)
                        .arg(layers_->count())
                        .arg(grouped(entities)));
}

QString ImportWizard::lineFor() const
{
    if (!scratch_ || probe_ != nullptr || stage_->currentIndex() != kStageDrawing) return {};
    const QStringList picked = chosen();
    if (picked.isEmpty()) return {};

    QString line = QStringLiteral("İÇEAKTAR dosya=") + quoted(probed_);
    // ONLY WHAT NARROWS goes on the line: every layer ticked is the file, and a
    // line that listed forty names would say nothing more.
    if (picked.size() != layers_->count())
        line += QStringLiteral(" katmanlar=") + quoted(picked.join(QLatin1Char(',')));

    // The fields, by the names the file gives them — one row each: every row
    // ticked is `*`, none ticked is nothing said, and the import then reads
    // geometry alone, as every drawing before the field pane did.
    int ticked = 0;
    for (int i = 0; i < fields_->count(); ++i)
        if (fields_->item(i)->checkState() == Qt::Checked) ++ticked;
    if (fields_->count() > 0 && ticked == fields_->count())
        line += QStringLiteral(" alanlar=*");
    else if (ticked > 0)
        line += QStringLiteral(" alanlar=") + quoted(chosenFields().join(QLatin1Char(',')));
    return line;
}

void ImportWizard::refreshLine()
{
    if (command_ == nullptr || go_ == nullptr) return;
    const QString line = lineFor();
    QString waiting    = tr("Dosya okununca komut burada görünür.");
    if (stage_->currentIndex() == kStageDrawing && line.isEmpty())
        waiting = tr("En az bir katman işaretleyin.");
    command_->setText(line.isEmpty() ? waiting : line);
    command_->setProperty("empty", line.isEmpty());
    style()->unpolish(command_);
    style()->polish(command_);
    go_->setEnabled(!line.isEmpty());
}

} // namespace kentos::app
