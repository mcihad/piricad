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

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
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

constexpr int kRow  = 30; ///< design.md §4: a list row
constexpr int kBox  = 14; ///< §15.3: the tick box, radius 3
constexpr int kPadX = 10;
constexpr int kGap  = 9;
constexpr int kCntW = 62;

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

/// One layer row: tick box, name, entity count. design.md §15.3 — the state is
/// said with a SHAPE (a tick) and not with colour alone (§13).
class LayerCheckDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setTheme(ThemeMode mode) { theme_ = mode; }

    /// How wide the right-hand column is. The layer page prints a count in it
    /// (62 px); the field page prints a type and a sample value, which need more.
    void setDetailWidth(int px) { detail_ = px; }

    /// Where the row's tick box is, so a click can be routed to it. The whole row
    /// toggles, so this is used for painting and for nothing else — but stating
    /// the geometry once is what keeps the two from drifting.
    static QRect boxRect(const QRect& row)
    {
        return QRect(row.left() + kPadX, row.top() + (kRow - kBox) / 2, kBox, kBox);
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(0, kRow);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const Tokens& t = tk(theme_);
        const QRect box = option.rect;

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        if (option.state & QStyle::State_MouseOver) p->fillRect(box, t.hoverRow);

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

        const int x     = tick.right() + kGap;
        const int right = box.right() - kPadX - detail_;

        QFont face(QStringLiteral("IBM Plex Sans"));
        face.setPixelSize(12);
        p->setFont(face);
        p->setPen(on ? t.text : t.textDim);
        p->drawText(QRect(x, box.top(), right - x, kRow), Qt::AlignVCenter | Qt::AlignLeft,
                    p->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(),
                                                Qt::ElideMiddle, right - x));

        QFont digits(QStringLiteral("IBM Plex Mono"));
        digits.setPixelSize(11);
        p->setFont(digits);
        p->setPen(t.textFaint);
        p->drawText(QRect(right, box.top(), detail_, kRow), Qt::AlignVCenter | Qt::AlignRight,
                    p->fontMetrics().elidedText(index.data(Qt::UserRole + 1).toString(),
                                                Qt::ElideRight, detail_));

        p->restore();
    }

private:
    ThemeMode theme_ = ThemeMode::Dark;
    int detail_      = kCntW;
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
    // WIDE ENOUGH FOR THE LAYER PAGE'S TWO COLUMNS; the HEIGHT is the page's own
    // (see `showPage`). One size for every step gave the first page — a path
    // field and a reference list — the height the layer page needs for a preview
    // and a checklist, so more than half of the first thing a user ever sees in
    // this window was empty.
    resize(1080, 420);

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    column->addWidget(buildStepper());

    pages_ = new QStackedWidget(body);
    pages_->addWidget(buildFilePage());
    pages_->addWidget(buildLayerPage());
    pages_->addWidget(buildFieldPage());
    column->addWidget(pages_, 1);

    setBody(body);
    setFooterHeight(52);

    back_ = new Button(ButtonRole::Secondary, tr("Geri"), std::nullopt, this);
    back_->setEnabled(false);
    connect(back_, &QPushButton::clicked, this,
            [this] { showPage(std::max(0, pages_->currentIndex() - 1)); });

    cancel_ = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    // WHILE THE PROBE RUNS THIS BUTTON IS THE STOP BUTTON, and it has to mean it:
    // pressing it used to close the whole window under a thread that was still
    // reading. The read is stopped; the window stays, and the button turns back
    // into the cancel button when the thread returns (`probeFinished`).
    connect(cancel_, &QPushButton::clicked, this, [this] {
        if (probe_ != nullptr && probe_->isRunning()) {
            probe_->cancel();
            return;
        }
        reject();
    });

    next_ = new Button(ButtonRole::Primary, tr("İleri"), std::nullopt, this);
    next_->setDefault(true);
    next_->setEnabled(false);
    connect(next_, &QPushButton::clicked, this, [this] {
        if (pages_->currentIndex() == 0) {
            startProbe();
            return;
        }
        if (pages_->currentIndex() == 1) {
            showPage(2);
            return;
        }

        // THE WHOLE POINT OF THE WINDOW, in one line: the ticks become two
        // arguments, and the arguments go on the same command line a script
        // would write (Article 1.2).
        const QStringList picked = chosen();
        line_ = QStringLiteral("İÇEAKTAR dosya=\"%1\"").arg(pathField_->text().trimmed());
        if (picked.size() != static_cast<int>(found_.layers.size()))
            line_ += QStringLiteral(" katmanlar=\"%1\"").arg(picked.join(QLatin1Char(',')));

        // The fields, by the names the file gives them: every one ticked is
        // `*`, none ticked is nothing said — the import then reads geometry
        // alone, as every drawing before this page did.
        const QStringList wanted = chosenFields();
        if (fields_->count() > 0 && wanted.size() == fields_->count())
            line_ += QStringLiteral(" alanlar=*");
        else if (!wanted.isEmpty())
            line_ += QStringLiteral(" alanlar=\"%1\"").arg(wanted.join(QLatin1Char(',')));
        accept();
    });

    // ALL THREE AT THE RIGHT, after the stretch the frame already holds. `Geri`
    // used to be added before a second stretch and sat in the middle of the
    // footer, belonging to neither end.
    footer()->addWidget(back_);
    footer()->addWidget(cancel_);
    footer()->addWidget(next_);

    ticker_ = new QTimer(this);
    ticker_->setInterval(100);
    connect(ticker_, &QTimer::timeout, this, [this] {
        progressText_->setText(
            tr("Dosya okunuyor…  %1 sn")
                .arg(QString::number(static_cast<double>(elapsed_.elapsed()) / 1000.0, 'f', 1)));
    });

    // QUALIFIED, and not because a linter asked. This runs inside the
    // constructor, where the object is not yet an `ImportWizard` for dispatch
    // purposes; naming the function outright says the non-virtual call is the
    // intent rather than an accident a future subclass would silently inherit.
    ImportWizard::applyTheme(theme);
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

QWidget* ImportWizard::buildStepper()
{
    // §16.3's "window parts" band, cut down to what a two-page wizard needs: the
    // two step names and a rule between them. A stepper that showed a percentage
    // would be inventing a number — there are two pages, and the user is on one.
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("wizardStepper"));
    bar->setFixedHeight(46);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(20, 0, 20, 0);
    row->setSpacing(12);

    stepOne_ = new QLabel(tr("1 · DOSYA"), bar);
    stepOne_->setObjectName(QStringLiteral("wizardStepOn"));

    stepRule_ = new QWidget(bar);
    stepRule_->setObjectName(QStringLiteral("wizardStepRule"));
    stepRule_->setFixedHeight(1);

    stepTwo_ = new QLabel(tr("2 · KATMANLAR"), bar);
    stepTwo_->setObjectName(QStringLiteral("wizardStepOff"));

    stepRuleTwo_ = new QWidget(bar);
    stepRuleTwo_->setObjectName(QStringLiteral("wizardStepRule"));
    stepRuleTwo_->setFixedHeight(1);

    stepThree_ = new QLabel(tr("3 · ALANLAR"), bar);
    stepThree_->setObjectName(QStringLiteral("wizardStepOff"));

    row->addWidget(stepOne_);
    row->addWidget(stepRule_, 1);
    row->addWidget(stepTwo_);
    row->addWidget(stepRuleTwo_, 1);
    row->addWidget(stepThree_);
    return bar;
}

QWidget* ImportWizard::buildFieldPage()
{
    // THE TABLE BESIDE THE GEOMETRY. A Shapefile or a GeoPackage carries fields
    // — ada, parsel, nitelik — and a parcel imported without them is half a
    // parcel. Each row: which layer, which field, what it becomes, and a first
    // value so `ada_no` and `parsel_no` can be told apart before anything is
    // written. The ticked rows become `alanlar=` on the command line.
    auto* page   = new QWidget(this);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(20, 18, 20, 18);
    column->setSpacing(10);

    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    auto* heading = new QLabel(tr("ALANLAR"), page);
    heading->setObjectName(QStringLiteral("groupCaption"));
    auto* all = new Button(ButtonRole::Ghost, tr("Tümü"), std::nullopt, page);
    all->setControlSize(ControlSize::Compact);
    connect(all, &QPushButton::clicked, this, [this] { setAllFieldsChecked(true); });
    auto* none = new Button(ButtonRole::Ghost, tr("Hiçbiri"), std::nullopt, page);
    none->setControlSize(ControlSize::Compact);
    connect(none, &QPushButton::clicked, this, [this] { setAllFieldsChecked(false); });
    head->addWidget(heading);
    head->addStretch(1);
    head->addWidget(all);
    head->addWidget(none);
    column->addLayout(head);

    fields_ = new QListWidget(page);
    fields_->setObjectName(QStringLiteral("importLayerList"));
    fields_->setFrameShape(QFrame::NoFrame);
    fields_->setMouseTracking(true);
    fields_->setUniformItemSizes(true);
    fields_->setSelectionMode(QAbstractItemView::NoSelection);
    auto* delegate = new LayerCheckDelegate(fields_);
    delegate->setDetailWidth(260);
    fields_->setItemDelegate(delegate);
    connect(fields_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        refreshFieldTally();
    });
    column->addWidget(fields_, 1);

    fieldTally_ = new QLabel(page);
    fieldTally_->setObjectName(QStringLiteral("quiet"));
    fieldTally_->setWordWrap(true);
    column->addWidget(fieldTally_);
    return page;
}

QWidget* ImportWizard::buildFilePage()
{
    auto* page = new QWidget(this);
    auto* col  = new QVBoxLayout(page);
    col->setContentsMargins(20, 18, 20, 18);
    col->setSpacing(0);

    // ONE MEASURED COLUMN. Every line of prose here used to run the full width of
    // the window — around 170 characters — because the page had no column at all;
    // the field was measured and nothing else was. A line nobody can track back
    // to its own left edge is not read (§16.1's 650 px text measure).
    auto* body    = new QWidget(page);
    auto* measure = new QVBoxLayout(body);
    measure->setContentsMargins(0, 0, 0, 0);
    measure->setSpacing(0);
    body->setMaximumWidth(720);

    // WHAT THIS WINDOW DOES, before the field rather than after the progress bar.
    // The sentence was the last thing on the page, under a progress band that is
    // hidden until a read starts — so the one promise that makes the window safe
    // to use ("nothing is added to the drawing yet") was read, if at all, after
    // the decision it is about.
    auto* lead = new QLabel(tr("Dosya önce yalnızca okunur; çizime hiçbir şey eklenmez. "
                               "Katmanları seçtikten sonra içe aktarma tek bir adımda yapılır "
                               "ve tek adımda geri alınır."),
                            body);
    lead->setWordWrap(true);
    measure->addWidget(lead);
    measure->addSpacing(16);

    auto* heading = new QLabel(tr("DOSYA"), body);
    heading->setObjectName(QStringLiteral("groupCaption"));
    measure->addWidget(heading);
    measure->addSpacing(10);

    auto* pick = new QHBoxLayout;
    pick->setSpacing(8);

    pathField_ = new QLineEdit(body);
    pathField_->setFixedHeight(30);
    pathField_->setPlaceholderText(io::dwg_backend_available()
                                       ? tr("DXF, DWG, Shapefile veya GeoPackage yolu")
                                       : tr("DXF, Shapefile veya GeoPackage yolu"));
    connect(pathField_, &QLineEdit::textChanged, this,
            [this](const QString& text) { judgePick(text); });

    auto* browse = new Button(ButtonRole::Secondary, tr("Gözat…"), Glyph::Open, body);
    connect(browse, &QPushButton::clicked, this, &ImportWizard::browse);

    pick->addWidget(pathField_, 1);
    pick->addWidget(browse);
    measure->addLayout(pick);

    measure->addSpacing(6);
    fileFacts_ = new QLabel(body);
    fileFacts_->setObjectName(QStringLiteral("quiet"));
    measure->addWidget(fileFacts_);

    // THE VERDICT, where the pick is made. Hidden while the pick is fine, because
    // a banner that is always up says nothing.
    verdict_ = new Banner(Tone::Danger, QString(), QString(), body);
    verdict_->setVisible(false);
    measure->addSpacing(10);
    measure->addWidget(verdict_);

    measure->addSpacing(18);

    // The progress band. Hidden until a read starts, because a bar sitting still
    // at zero is a promise the window has not made yet.
    progressBox_ = new QWidget(body);
    auto* pcol   = new QVBoxLayout(progressBox_);
    pcol->setContentsMargins(0, 0, 0, 0);
    pcol->setSpacing(8);

    // The 2 px accent strip of design.md §11, indeterminate on purpose: the
    // readers stream and report no fraction, and a bar that filled itself on a
    // timer would be a lie told at the exact moment the user is deciding
    // whether to wait. It runs while `progressBox_` is shown and stops when it
    // is hidden; the honest figures — elapsed seconds, a stop that works — are
    // under it.
    progress_ = new ProgressStrip(progressBox_);
    pcol->addWidget(progress_);

    progressText_ = new QLabel(progressBox_);
    progressText_->setObjectName(QStringLiteral("quiet"));
    pcol->addWidget(progressText_);

    progressBox_->setVisible(false);
    measure->addWidget(progressBox_);

    col->addWidget(body);

    // REFERENCE SITS AT THE BOTTOM. The formats used to follow the prose in the
    // upper half and left the lower half of a 1320 px window empty, which reads
    // as an unfinished page. What a user acts on is at the top; what they look
    // up is at the foot.
    col->addStretch(1);

    auto* formatHeading = new QLabel(tr("DESTEKLENEN BİÇİMLER"), page);
    formatHeading->setObjectName(QStringLiteral("groupCaption"));
    col->addWidget(formatHeading);
    col->addSpacing(8);

    // FROM THE ALLOW-LIST, NEVER FROM A LIST IN THIS FILE. `io.md` P7 holds the
    // driver set in /cmake and hands it to /src/io as a compile definition; a
    // second copy here would be the "hand-maintained second list" 5.10 forbids,
    // and it would go stale the first time a driver is added.
    //
    // EACH ROW ENDS IN A BADGE, and that is the whole change: the rows were
    // dot-joined sentences in one grey weight, so "okunur ve yazılır" and "bu
    // sürümde okunmaz" read alike and the one fact a user scans this list for —
    // can this program open my file — had to be read word by word.
    auto* grid = new QVBoxLayout;
    grid->setSpacing(4);
    const auto formatRow = [this, &grid](const QString& extension, const QString& label,
                                         const QString& capability, Tone tone,
                                         const QString& hint = QString()) {
        auto* line = new QHBoxLayout;
        line->setSpacing(8);

        auto* name = new QLabel(extension, this);
        name->setMinimumWidth(64);
        line->addWidget(name);

        auto* what = new QLabel(label, this);
        what->setObjectName(QStringLiteral("quiet"));
        line->addWidget(what, 1);

        auto* mark = new Badge(capability, tone, this);
        if (!hint.isEmpty()) mark->setToolTip(hint);
        line->addWidget(mark);

        auto* holder = new QWidget(this);
        holder->setLayout(line);
        holder->setMaximumWidth(720);
        grid->addWidget(holder);
    };

    for (const io::VectorFormat& f : io::vector_formats()) {
        if (!f.read) continue;
        formatRow(QString::fromStdString(f.extension).toUpper(), QString::fromStdString(f.label),
                  f.write ? tr("okunur · yazılır") : tr("okunur"),
                  f.write ? Tone::Neutral : Tone::Accent);
    }

    // The io layer's status names a CMake flag, which is for the person who
    // builds the program; the person who uses it is told what to do instead.
    if (io::dwg_backend_available())
        formatRow(tr(".DWG"), tr("AutoCAD çizimi"), tr("okunur"), Tone::Accent,
                  QString::fromStdString(io::dwg_backend_status()));
    else
        formatRow(tr(".DWG"), tr("AutoCAD çizimi — çizimi DXF olarak kaydedip aktarın"),
                  tr("okunmaz"), Tone::Danger, QString::fromStdString(io::dwg_backend_status()));

    col->addLayout(grid);
    return page;
}

void ImportWizard::judgePick(const QString& text)
{
    const QFileInfo about(text.trimmed());
    const bool exists = about.isFile();

    fileFacts_->setText(
        exists ? tr("%1 · %2 · son değişiklik %3")
                     .arg(about.suffix().toUpper(), readableSize(about.size()),
                          about.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm")))
               : QString());

    // WHETHER THIS PROGRAM CAN OPEN IT, decided HERE. The window has the
    // allow-list and used to ignore it: any existing file enabled `İleri`, so a
    // `.dwg` on a build without the reader, or a `.txt`, was accepted on this
    // page and refused on the next one — after the user had committed to the
    // flow. A window that knows the answer has to give it where the question is
    // asked.
    QString why;
    if (exists) {
        const QString suffix = QStringLiteral(".") + about.suffix().toLower();
        bool readable        = false;
        for (const io::VectorFormat& f : io::vector_formats())
            if (f.read && QString::fromStdString(f.extension).toLower() == suffix) readable = true;
        if (suffix == QStringLiteral(".dwg")) readable = io::dwg_backend_available();

        if (!readable)
            why = suffix == QStringLiteral(".dwg")
                      ? tr("Bu sürüm DWG okumuyor. Çizimi AutoCAD'de DXF olarak kaydedip onu "
                           "aktarın.")
                      : tr("Desteklenen biçimler aşağıda listeli. Dosyanın uzantısını "
                           "denetleyin.");
    }

    verdict_->setTitle(why.isEmpty() ? QString()
                                     : tr("%1 dosyası okunamıyor").arg(about.suffix().toUpper()));
    verdict_->setText(why);
    verdict_->applyTheme(mode_);
    verdict_->setVisible(!why.isEmpty());
    next_->setEnabled(exists && why.isEmpty());
}

QWidget* ImportWizard::buildLayerPage()
{
    auto* page = new QWidget(this);
    auto* row  = new QHBoxLayout(page);
    row->setContentsMargins(20, 18, 20, 18);
    row->setSpacing(18);

    // ---- left: the drawing ----
    auto* left = new QVBoxLayout;
    left->setSpacing(10);

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
    // them their room and keeps the thumbnail smaller. That is the right
    // priority: the thumbnail is a reassurance, the notices are a decision.
    notices_ = new QVBoxLayout;
    notices_->setSpacing(8);
    notices_->setContentsMargins(0, 0, 0, 0);
    left->addLayout(notices_);

    auto* drawnHeading = new QLabel(tr("ÇİZİM"), page);
    drawnHeading->setObjectName(QStringLiteral("groupCaption"));
    left->addWidget(drawnHeading);

    preview_ = new ImportPreview(page);
    preview_->useSettings(&controller_.bus().app_settings());
    left->addWidget(preview_, 1);

    summary_ = new QLabel(page);
    summary_->setObjectName(QStringLiteral("quiet"));
    summary_->setWordWrap(true);
    left->addWidget(summary_);

    row->addLayout(left, 1);

    // ---- right: the checklist ----
    auto* right = new QVBoxLayout;
    right->setSpacing(10);
    right->setContentsMargins(0, 0, 0, 0);

    auto* head = new QHBoxLayout;
    head->setSpacing(8);

    auto* listHeading = new QLabel(tr("KATMANLAR"), page);
    listHeading->setObjectName(QStringLiteral("groupCaption"));

    auto* all = new Button(ButtonRole::Ghost, tr("Tümü"), std::nullopt, page);
    all->setControlSize(ControlSize::Compact);
    connect(all, &QPushButton::clicked, this, [this] { setAllChecked(true); });

    auto* none = new Button(ButtonRole::Ghost, tr("Hiçbiri"), std::nullopt, page);
    none->setControlSize(ControlSize::Compact);
    connect(none, &QPushButton::clicked, this, [this] { setAllChecked(false); });

    head->addWidget(listHeading);
    head->addStretch(1);
    head->addWidget(all);
    head->addWidget(none);
    right->addLayout(head);

    filter_ = new QLineEdit(page);
    filter_->setFixedHeight(30);
    filter_->setPlaceholderText(tr("Katman ara"));
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
    right->addWidget(filter_);

    layers_ = new QListWidget(page);
    layers_->setObjectName(QStringLiteral("importLayerList"));
    layers_->setFrameShape(QFrame::NoFrame);
    layers_->setMouseTracking(true);
    layers_->setUniformItemSizes(true);
    layers_->setSelectionMode(QAbstractItemView::NoSelection);
    layers_->setItemDelegate(new LayerCheckDelegate(layers_));
    layers_->setFixedWidth(312); // design.md §4: the standard right-hand column
    connect(layers_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        // THE WHOLE ROW IS THE TARGET. A 14 px box is the drawn size, not the
        // hit size — the layer panel's eye had exactly this problem.
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        applyVisibility();
        refreshTally();
    });
    right->addWidget(layers_, 1);

    tally_ = new QLabel(page);
    tally_->setObjectName(QStringLiteral("quiet"));
    right->addWidget(tally_);

    row->addLayout(right);
    return page;
}

void ImportWizard::setPath(const QString& path)
{
    pathField_->setText(path);
}

void ImportWizard::beginWith(const QString& path)
{
    setPath(path);
    if (next_->isEnabled()) startProbe();
}

bool ImportWizard::probeSettle(int page, int msecs)
{
    // Until the layer page is up, not merely until the thread stops: the
    // thread's `finished` is delivered as an event, and a check made between the
    // two saw a finished read and a wizard still on its first page.
    QElapsedTimer clock;
    clock.start();
    while (pages_->currentIndex() == 0 && clock.elapsed() < msecs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!scratch_ || pages_->currentIndex() == 0) return false;
    showPage(page);
    QCoreApplication::processEvents();
    return true;
}

void ImportWizard::browse()
{
    QStringList filters;
    filters << tr("Desteklenen tüm dosyalar (%1)").arg(QStringLiteral("*.dxf *.shp *.gpkg *.dwg"));
    for (const io::VectorFormat& f : io::vector_formats())
        if (f.read)
            filters << QStringLiteral("%1 (*%2)")
                           .arg(QString::fromStdString(f.label),
                                QString::fromStdString(f.extension));
    if (io::dwg_backend_available()) filters << tr("AutoCAD DWG (*.dwg)");

    const QString picked = QFileDialog::getOpenFileName(
        this, tr("İçe aktarılacak dosya"), QFileInfo(pathField_->text()).absolutePath(),
        filters.join(QStringLiteral(";;")));
    if (!picked.isEmpty()) pathField_->setText(picked);
}

void ImportWizard::startProbe()
{
    if (probe_ != nullptr && probe_->isRunning()) return;

    // BEFORE THE OLD ONE IS FREED. A second read replaces the scratch document,
    // and a preview still pointing at the previous one paints freed memory the
    // moment anything asks it to repaint.
    if (preview_ != nullptr) preview_->setDocument(nullptr);
    layers_->clear();
    scratch_ = std::make_unique<core::Document>();

    progressBox_->setVisible(true);
    progressText_->setText(tr("Dosya okunuyor…"));
    next_->setEnabled(false);
    pathField_->setEnabled(false);
    elapsed_.start();
    ticker_->start();

    // The cancel button now means "stop the read", not "close the window": the
    // thread answers
    // a stop inside 100 ms (io.md R15), and a user who pressed it wants the file
    // to stop being read more than they want the window gone.
    cancel_->setText(tr("Okumayı durdur"));

    // The drawing's own system and its unit, resolved the way `io/service.cpp`
    // resolves them for the command: the probe compares what it is given and
    // never guesses (io.md R20), and a DXF that names no unit is read in the
    // project's unit here exactly as İÇEAKTAR will read it.
    io::ImportOptions options;
    const core::Crs& mine = controller_.document().crs();
    options.project_crs   = mine.resolved() ? "EPSG:" + std::to_string(mine.epsg()) : mine.id();
    options.drawing_unit =
        core::drawing_unit_from_setting(controller_.bus().setting("core.cizim.birim").as_enum());

    probe_ =
        new ImportProbeThread(*scratch_, pathField_->text().trimmed(), std::move(options), this);
    connect(probe_, &QThread::finished, this, &ImportWizard::probeFinished);
    probe_->start();
}

void ImportWizard::probeFinished()
{
    ticker_->stop();
    progressBox_->setVisible(false);
    pathField_->setEnabled(true);
    next_->setEnabled(true);
    cancel_->setText(tr("İptal"));

    const core::Result<io::ImportProbe>& outcome = probe_->outcome();
    probe_->deleteLater();
    probe_ = nullptr;

    if (!outcome) {
        // The reader's own sentence, verbatim. Rewriting it here would put a
        // second wording of every io failure in the shell.
        summary_->setText(QString::fromStdString(outcome.error().message));
        progressBox_->setVisible(true);
        progress_->setVisible(false);
        progressText_->setObjectName(QStringLiteral("danger"));
        progressText_->style()->unpolish(progressText_);
        progressText_->style()->polish(progressText_);
        progressText_->setText(QString::fromStdString(outcome.error().message));
        return;
    }

    found_ = outcome.value();

    // The failure styling from a previous attempt does not survive a success.
    progress_->setVisible(true);
    progressText_->setObjectName(QStringLiteral("quiet"));
    progressText_->style()->unpolish(progressText_);
    progressText_->style()->polish(progressText_);

    layers_->clear();
    for (const auto& [name, count] : found_.layers) {
        auto* row = new QListWidgetItem(QString::fromStdString(name), layers_);
        row->setData(Qt::UserRole + 1, grouped(count));
        // EVERYTHING TICKED TO BEGIN WITH: the user asked to import this file,
        // and a checklist that starts empty makes them do the work twice.
        row->setCheckState(Qt::Checked);
    }

    // THE FACTS STAY QUIET, because they are facts: what was read, how much of
    // it, in which system. Metadata reads as metadata.
    QStringList said;
    said << tr("%1 · %2 nesne · %3 katman")
                .arg(QString::fromStdString(found_.driver), grouped(found_.entities),
                     grouped(static_cast<std::uint64_t>(found_.layers.size())));
    if (!found_.crs.empty()) said << QString::fromStdString(found_.crs);
    summary_->setText(said.join(QStringLiteral("\n")));

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
        // third page IS that choice, so the hint would send the user elsewhere.
        if (line.text.find("alanlar=") != std::string::npos) continue;

        // AN `Info` IS A FACT, NOT A NOTICE. "the types read were ARC, CIRCLE,
        // LINE" is the same kind of statement as the object count, and a banner
        // for it costs the notices their rank: four banners on one small file
        // and the two that matter stop standing out. Facts join the facts.
        if (line.level == io::Severity::Info) {
            said << QString::fromStdString(line.text);
            summary_->setText(said.join(QStringLiteral("\n")));
            continue;
        }

        const Tone tone = line.level == io::Severity::Error ? Tone::Danger : Tone::Warn;

        // THE FACT IS THE TITLE, THE FIX IS THE BODY. A reader's notice is one
        // long sentence run — what was assumed, and then what to do if the
        // assumption is wrong — so the first sentence is the heading and the
        // rest is the instruction under it. That is the shape a banner has, and
        // the shape an error should have: what happened, then how to fix it.
        const QString whole = QString::fromStdString(line.text).trimmed();
        const int stop      = whole.indexOf(QStringLiteral(". "));
        const QString title = stop > 0 ? whole.left(stop + 1) : whole;
        const QString rest  = stop > 0 ? whole.mid(stop + 2).trimmed() : QString();
        auto* notice        = new Banner(tone, title, rest, this);
        notice->applyTheme(mode_);
        notices_->addWidget(notice);
    }

    preview_->setDocument(scratch_.get());

    // The third page's rows: every field the reader saw, ticked to begin with
    // for the same reason the layers are.
    fields_->clear();
    for (const io::VectorField& f : found_.fields) {
        const QString type = type_label(f.type);
        auto* row =
            new QListWidgetItem(QStringLiteral("%1 · %2").arg(QString::fromStdString(f.layer),
                                                              QString::fromStdString(f.name)),
                                fields_);
        row->setData(Qt::UserRole, QString::fromStdString(f.name));
        row->setData(Qt::UserRole + 1,
                     f.sample.empty()
                         ? type
                         : QStringLiteral("%1 · %2").arg(type, QString::fromStdString(f.sample)));
        row->setToolTip(tr("Sütun kimliği: %1").arg(QString::fromStdString(f.id)));
        row->setCheckState(Qt::Checked);
    }
    refreshTally();
    refreshFieldTally();
    showPage(1);
}

void ImportWizard::refreshFieldTally()
{
    if (fields_->count() == 0) {
        fieldTally_->setText(tr("Bu dosyada öznitelik alanı yok; yalnız geometri okunur."));
        return;
    }
    const int ticked = static_cast<int>(chosenFields().size());
    fieldTally_->setText(
        tr("%1 / %2 alan katmanın sütunu olur. Sütun kimliği alan adının küçük harfe "
           "indirilmiş biçimidir; aynı ad iki katmanda geçerse tek sütun olur.")
            .arg(ticked)
            .arg(fields_->count()));
}

void ImportWizard::setAllFieldsChecked(bool on)
{
    for (int i = 0; i < fields_->count(); ++i)
        fields_->item(i)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    refreshFieldTally();
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

void ImportWizard::showPage(int page)
{
    pages_->setCurrentIndex(page);
    back_->setEnabled(page > 0);

    // THE WINDOW TAKES THE HEIGHT ITS PAGE NEEDS — and it GROWS but never
    // shrinks. Growing is what the steps ask for: picking one file needs a
    // field, choosing layers needs a preview beside a checklist. Never shrinking
    // is what keeps stepping back and forth from reflowing the window under the
    // hand: once a size has been shown, it stays available.
    //
    // `QStackedWidget` reports the tallest page whatever is on top, so the page
    // itself is asked instead.
    if (QWidget* live = pages_->widget(page); live != nullptr) {
        const int wanted = live->sizeHint().height() + (height() - pages_->height());
        if (wanted > height()) resize(width(), wanted);
    }
    next_->setText(page == 2 ? tr("İçe Aktar") : tr("İleri"));

    stepOne_->setObjectName(page == 0 ? QStringLiteral("wizardStepOn")
                                      : QStringLiteral("wizardStepDone"));
    stepTwo_->setObjectName(page == 1  ? QStringLiteral("wizardStepOn")
                            : page > 1 ? QStringLiteral("wizardStepDone")
                                       : QStringLiteral("wizardStepOff"));
    stepThree_->setObjectName(page == 2 ? QStringLiteral("wizardStepOn")
                                        : QStringLiteral("wizardStepOff"));
    // A stylesheet is matched at set time, so a changed object name needs the
    // widget re-polished or the rule that now applies never runs.
    for (QLabel* step : {stepOne_, stepTwo_, stepThree_}) {
        step->style()->unpolish(step);
        step->style()->polish(step);
    }
}

void ImportWizard::setAllChecked(bool on)
{
    for (int i = 0; i < layers_->count(); ++i) {
        QListWidgetItem* row = layers_->item(i);
        if (row->isHidden()) continue; // the filter is showing a subset; honour it
        row->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    applyVisibility();
    refreshTally();
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

    tally_->setText(tr("%1 / %2 katman · %3 nesne aktarılacak")
                        .arg(ticked)
                        .arg(layers_->count())
                        .arg(grouped(entities)));
    next_->setEnabled(ticked > 0);
}

} // namespace kentos::app
