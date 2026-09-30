// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/print_dialog.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/fields.hpp"
#include "piricad/app/layout_render.hpp"
#include "piricad/app/print_service.hpp"
#include "piricad/app/tokens.hpp"
#include "piricad/app/widgets.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace piricad::app {

/// THE SHEET ON ITS PASTEBOARD: the paper in its own proportion, a shadow under
/// it, the profile's margin as a dashed guide and the centre the plot is placed
/// by. The picture on the paper is `PrintService::renderPreview`'s — the plot's
/// own pipeline (ui.md R36) — drawn for exactly the pixels the paper takes.
class PrintSheet : public QWidget, public Themed
{
public:
    explicit PrintSheet(QWidget* parent) : QWidget(parent)
    {
        setMinimumSize(420, 380);
        // A NEW PICTURE WHEN THE SIZE HAS SETTLED, not on every step of a drag:
        // each one is a pass of the plot's own pipeline over the drawing. Until
        // then the last picture is scaled into the new paper.
        settle_.setSingleShot(true);
        settle_.setInterval(60);
        QObject::connect(&settle_, &QTimer::timeout, this, [this] {
            if (onResize) onResize();
        });
    }

    /// Called when the paper's size on screen changed and wants a new picture.
    std::function<void()> onResize;

    /// The paper, in millimetres, and its margin.
    void setPaper(double width_mm, double height_mm, double margin_mm)
    {
        width_mm_  = std::max(1.0, width_mm);
        height_mm_ = std::max(1.0, height_mm);
        margin_mm_ = std::max(0.0, margin_mm);
        update();
    }

    void setImage(const QImage& image)
    {
        image_ = image;
        update();
    }

    /// Where the paper stands in the widget: fitted with air round it.
    QRectF paperRect() const
    {
        constexpr double air = 28.0;
        const double w       = std::max(1.0, width() - (2 * air));
        const double h       = std::max(1.0, height() - (2 * air));
        const double scale   = std::min(w / width_mm_, h / height_mm_);
        const QSizeF size(width_mm_ * scale, height_mm_ * scale);
        return {QPointF((width() - size.width()) / 2.0, (height() - size.height()) / 2.0), size};
    }

    /// The paper's longer side in device pixels: what the picture is drawn at.
    int wantedPixels() const
    {
        const QRectF paper = paperRect();
        return static_cast<int>(
            std::lround(std::max(paper.width(), paper.height()) * devicePixelRatioF()));
    }

    void applyTheme(ThemeMode mode) override
    {
        theme_ = mode;
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        settle_.start();
    }

    void paintEvent(QPaintEvent* /*event*/) override
    {
        const Tokens& t = theme_ == ThemeMode::Dark ? darkTokens() : lightTokens();
        QPainter p(this);
        p.fillRect(rect(), t.bgCanvas);
        const QRectF paper = paperRect();

        // PAPER ON A TABLE, with the layout designer's own shadow: the two
        // windows that put a sheet on paper show one sheet.
        paint_paper_shadow(p, paper);
        p.fillRect(paper, Qt::white);
        if (!image_.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(paper, image_);
        }
        p.setPen(QPen(t.lineHard, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(paper);

        // THE MARGIN, as the designer marks its own: a faint dashed guide the
        // plot does not print. And the CENTRE the sheet is placed by (ui.md
        // R34), in the accent, because it is the aim and not ink.
        const double per_mm = paper.width() / width_mm_;
        const QRectF inner  = paper.adjusted(margin_mm_ * per_mm, margin_mm_ * per_mm,
                                             -margin_mm_ * per_mm, -margin_mm_ * per_mm);
        p.setPen(QPen(QColor(0, 0, 0, 40), 1.0, Qt::DashLine));
        if (margin_mm_ > 0.0) p.drawRect(inner);
        p.setPen(QPen(t.accent, 1.2));
        const QPointF centre = inner.center();
        p.drawLine(centre - QPointF(9, 0), centre + QPointF(9, 0));
        p.drawLine(centre - QPointF(0, 9), centre + QPointF(0, 9));
    }

private:
    QTimer settle_;
    QImage image_;
    double width_mm_{210.0};
    double height_mm_{297.0};
    double margin_mm_{10.0};
    ThemeMode theme_{ThemeMode::Dark};
};

namespace {

/// The caption column of the settings rows — design.md §8, as in the other
/// property columns of this program (`style_designer.cpp`, the layout designer).
constexpr int kCaption = 110;

/// The settings column's width: the style designer's symbol column.
constexpr int kColumn = 352;

QString utf8(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/// A value as the command line takes it, with the escapes the parser reads back.
QString quoted(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

/// A document millimetre as the command line's metre: `485320.150`.
QString metres(core::Mm mm)
{
    return QString::number(static_cast<double>(mm) / 1000.0, 'f', 3);
}

/// The next round plan scale at or above `implied`: 1, 2, 2.5 or 5 times a
/// power of ten, which is the ladder every plan sheet in this country is drawn
/// on (1/200, 1/500, 1/1000, 1/2000, 1/2500, 1/5000).
///
/// UP rather than to the nearest, because this is what the rounding button
/// OFFERS and a plot that grew keeps everything the frame held; one that shrank
/// would cut a corner off the aim. The window never rounds by itself — see the
/// note at the top of print_dialog.hpp.
std::int64_t rounded_scale(double implied)
{
    if (!(implied > 0.0)) return 1000;
    std::int64_t step = 1;
    while (step < 100000000) {
        for (const std::int64_t factor : {1, 2, 5}) {
            const std::int64_t candidate = step * factor;
            if (static_cast<double>(candidate) >= implied - 0.5) return candidate;
            // 2.5 × the decade: 250, 2 500, 25 000 — the cadastral sheet scales.
            const std::int64_t half_step = step * 5 / 2;
            if (factor == 2 && step >= 100 && static_cast<double>(half_step) >= implied - 0.5)
                return half_step;
        }
        step *= 10;
    }
    return 100000000;
}

} // namespace

PrintDialog::PrintDialog(Controller& controller, core::Box2 window, QString profile,
                         QWidget* parent)
    : DialogFrame(parent), controller_(controller), window_(window)
{
    setHeading(Glyph::Print, tr("Yazdır"), tr("— yerleşim olmadan"));
    build();

    // THE PROFILE THE CALLER NAMED, which is the one the toolbar menu picked and
    // the one the canvas frame was shaped by. Without this the window opened on
    // the default profile and a user who chose "A3 Yatay" got a portrait sheet.
    if (!profile.isEmpty()) profile_->setCurrentText(profile);

    // OPENED ON WHAT THE FRAME CAPTURED, and nothing else: `refresh` fills the
    // centre and the scale from that box while `from_frame_` holds, through
    // `show`, so neither counts as an edit and the area stays the frame's exact
    // box until somebody types over it.
    refresh();
}

io::PrintProfile PrintDialog::chosen() const
{
    const io::PrintProfiles& store = controller_.printService().profiles();
    const io::PrintProfile* p =
        profile_ != nullptr ? store.find(profile_->currentText().toStdString()) : nullptr;
    if (p == nullptr) p = store.fallback();
    return p != nullptr ? *p : io::PrintProfile{};
}

core::Point2 PrintDialog::centre() const
{
    if (centre_ != nullptr) {
        // `Y,X` in metres, the way every coordinate is typed in this program.
        const QStringList parts = centre_->value().split(QLatin1Char(','));
        if (parts.size() == 2) {
            bool ok_x      = false;
            bool ok_y      = false;
            const double y = parts[0].trimmed().toDouble(&ok_x);
            const double x = parts[1].trimmed().toDouble(&ok_y);
            if (ok_x && ok_y)
                return core::Point2{core::mm_round(y * 1000.0), core::mm_round(x * 1000.0)};
        }
    }
    return core::Point2{(window_.min_x + window_.max_x) / 2, (window_.min_y + window_.max_y) / 2};
}

std::int64_t PrintDialog::denominator() const
{
    if (denominator_ != nullptr) {
        const std::int64_t typed = denominator_->value().toLongLong();
        if (typed > 0) return typed;
    }
    const double implied = PrintService::scaleDenominator(chosen(), window_);
    return implied > 0.0 ? static_cast<std::int64_t>(std::llround(implied)) : 1000;
}

core::Box2 PrintDialog::window() const
{
    // THE FRAME'S OWN BOX, to the millimetre, while nobody has typed a scale or
    // a centre. `fitWindow` is what keeps it honest across a change of paper:
    // for the profile the frame was shaped by it returns the box unchanged, and
    // for a different one it GROWS the box to that paper's aspect rather than
    // cropping the aim.
    if (from_frame_) return PrintService::fitWindow(chosen(), window_);

    const core::Box2 from_centre = PrintService::windowFor(chosen(), centre(), denominator());
    return from_centre.empty() ? PrintService::fitWindow(chosen(), window_) : from_centre;
}

void PrintDialog::show(Field* field, const QString& text, QString& remembered)
{
    if (field == nullptr) return;
    remembered = text;
    field->setValue(text);
}

void PrintDialog::build()
{
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    auto* middle = new QWidget(body);
    auto* row    = new QHBoxLayout(middle);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // ---- the sheet, on the left: the thing the window is about --------------
    auto* left    = new QWidget(middle);
    auto* leftCol = new QVBoxLayout(left);
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(0);
    sheet_ = new PrintSheet(left);
    sheet_->setAccessibleName(tr("Yazdırma önizlemesi"));
    sheet_->onResize = [this] { renderSheet(); };
    leftCol->addWidget(sheet_, 1);
    // WHAT THE SHEET IS, IN ONE LINE UNDER IT: the paper, the scale and the
    // ground it covers — the three figures a plan sheet's own legend carries.
    summary_ = new QLabel(left);
    summary_->setObjectName(QStringLiteral("printSummary"));
    summary_->setAttribute(Qt::WA_StyledBackground, true);
    summary_->setFixedHeight(26); // the layout designer's status strip
    summary_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    summary_->setAccessibleName(tr("Kâğıdın özeti"));
    leftCol->addWidget(summary_);
    row->addWidget(left, 1);

    // ---- the settings, on the right -------------------------------------------
    //
    // A PROPERTY COLUMN, the one this program's other property windows have:
    // the caption in a 110 px column beside its value (design.md §8), grouped
    // in the order the choices are made — which sheet, where on the ground,
    // where it goes, and then the two things a plot rarely needs, folded.
    auto* settings = new QWidget(middle);
    settings->setObjectName(QStringLiteral("printColumn"));
    settings->setAttribute(Qt::WA_StyledBackground, true);
    settings->setFixedWidth(kColumn);
    auto* settingsCol = new QVBoxLayout(settings);
    settingsCol->setContentsMargins(13, 10, 8, 0); // 1 of the 13 is the edge
    settingsCol->setSpacing(0);
    auto* scroll = new QScrollArea(settings);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    column_       = scroll;
    auto* form    = new QWidget(scroll);
    auto* formCol = new QVBoxLayout(form);
    // ROOM FOR THE BAR AND UNDER THE LAST ROW, as in the layout designer's
    // inspector: flush, the bar is drawn on the editors.
    formCol->setContentsMargins(0, 0, 14, 16);
    formCol->setSpacing(6);
    scroll->setWidget(form);
    settingsCol->addWidget(scroll, 1);
    row->addWidget(settings);
    column->addWidget(middle, 1);

    const auto regular = [](Field* f) {
        f->setFixedHeight(static_cast<int>(ControlSize::Regular));
        return f;
    };
    // A HEADING, with air above it and none below: the rows under it are its.
    const auto group = [form, formCol](const QString& title, QLabel** note_out = nullptr,
                                       Button** toggle_out = nullptr) {
        auto* heading = new QWidget(form);
        auto* line    = new QHBoxLayout(heading);
        line->setContentsMargins(0, formCol->count() == 0 ? 2 : 14, 0, 2);
        line->setSpacing(8);
        auto* caption = new QLabel(title, heading);
        caption->setObjectName(QStringLiteral("groupCaption"));
        line->addWidget(caption);
        line->addStretch(1);
        if (note_out != nullptr) {
            auto* note = new QLabel(heading);
            note->setObjectName(QStringLiteral("formHelp"));
            line->addWidget(note);
            *note_out = note;
        }
        if (toggle_out != nullptr) {
            // A FOLD, AT THE HEADING'S RIGHT END: the ribbon's own convention —
            // the chevron points down while the group is shut, up while it is
            // open. Not checkable: a bare button that is on wears the accent
            // wash, and an open group is not a mode that is on.
            auto* toggle = new Button(Glyph::ChevronDown, title, heading);
            toggle->setBare(true);
            line->setContentsMargins(0, 8, 0, 0);
            line->addWidget(toggle);
            *toggle_out = toggle;
        }
        formCol->addWidget(heading);
        return heading;
    };
    // ONE ROW: the caption beside the value, never wider than the column.
    const auto caption_row = [](QWidget* parent, QVBoxLayout* into, const QString& caption,
                                QWidget* editor) {
        auto* cell = new QWidget(parent);
        auto* line = new QHBoxLayout(cell);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(6);
        auto* label = new QLabel(caption, cell);
        label->setObjectName(QStringLiteral("formCaption"));
        label->setFixedSize(kCaption, static_cast<int>(ControlSize::Regular));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setBuddy(editor);
        line->addWidget(label, 0, Qt::AlignTop);
        editor->setParent(cell);
        editor->setMinimumWidth(1);
        line->addWidget(editor, 1);
        into->addWidget(cell);
        return cell;
    };
    // A DIM LINE UNDER A ROW, in the value column — indented by its holder's
    // layout, not by the label's own margins, which a `QFrame` loses at its
    // first polish (see `LayoutDesigner::help`).
    const auto note_row = [](QWidget* parent, QVBoxLayout* into, QLabel* said) {
        auto* holder = new QWidget(parent);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(kCaption + 6, 0, 0, 2);
        said->setParent(holder);
        said->setObjectName(QStringLiteral("formHelp"));
        said->setWordWrap(true);
        line->addWidget(said);
        into->addWidget(holder);
    };

    trouble_ = new Banner(Tone::Danger, tr("Yazdırılamadı"), QString(), form);
    trouble_->setVisible(false);
    formCol->addWidget(trouble_);

    // ---- the paper: the profile is its one source (ui.md R35) -----------------
    group(tr("KÂĞIT"));
    profile_                       = new ComboBox(form);
    const io::PrintProfiles& store = controller_.printService().profiles();
    for (const io::PrintProfile& p : store.all())
        profile_->addItem(utf8(p.name));
    profile_->setCurrentText(utf8(store.default_name()));
    profile_->setAccessibleName(tr("Yazdırma profili"));
    connect(profile_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    caption_row(form, formCol, tr("Profil"), profile_);
    // WHAT THE PROFILE SAYS, beside it and not offered again: paper, direction,
    // resolution and margin have one home.
    paper_ = new QLabel(form);
    paper_->setAccessibleName(tr("Profilin kâğıdı"));
    note_row(form, formCol, paper_);
    auto* manage =
        new Button(ButtonRole::Secondary, tr("Profilleri düzenle…"), Glyph::Settings, form);
    manage->setControlSize(ControlSize::Compact);
    manage->setToolTip(tr("Kâğıt, yön, çözünürlük ve kenar payı profilde tutulur"));
    connect(manage, &QPushButton::clicked, this, [this] {
        accept();
        emit manageProfilesRequested();
    });
    {
        auto* holder = new QWidget(form);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(kCaption + 6, 0, 0, 0);
        manage->setParent(holder);
        line->addWidget(manage);
        line->addStretch(1);
        formCol->addWidget(holder);
    }

    // ---- where on the ground (ui.md R37) ----------------------------------------
    group(tr("ÖLÇEK VE KONUM"));
    // THE SCALE. It opens on the frame's own — exactly, so the sheet is the
    // frame — and typing over it is what makes the scale decide the area
    // instead. The comparison against `shown_scale_` is what tells an edit from
    // a field that was only tabbed through: `Field` commits its value on focus
    // leaving too, and an unchanged value is not somebody choosing a scale.
    denominator_ = regular(new Field(number_of(1, 1000000), form));
    denominator_->setAccessibleName(tr("Ölçek paydası"));
    denominator_->setToolTip(tr("Çerçevenin ölçeğiyle açılır. Bir değer yazmak alanı o "
                                "ölçeğe göre yeniden kurar."));
    connect(denominator_, &Field::committed, this, [this](const QString& typed) {
        if (typed.trimmed() != shown_scale_.trimmed()) from_frame_ = false;
        refresh();
    });
    caption_row(form, formCol, tr("Ölçek 1 :"), denominator_);

    // THE ROUND FIGURE, OFFERED. A pafta is drawn at a scale somebody can read
    // off a legend, so the ladder is worth one press — but it is a press, and
    // the sheet beside it shows the ground it adds. `refresh` writes the label
    // and hides the button when the scale is already round.
    round_ = new Button(ButtonRole::Secondary, tr("Yuvarla"), std::nullopt, form);
    round_->setControlSize(ControlSize::Compact);
    // ONE NAME FOR A SCREEN READER, whatever figure the button offers — the
    // figure is in its text and its description. `MainWindow::probePrintLine`
    // presses the button by this name to prove the rounding is a press rather
    // than something the window does behind the user. NOT BY OBJECT NAME: a
    // component's object name is its role, the name the one stylesheet draws
    // it by (CLAUDE.md 5.19).
    round_->setAccessibleName(tr("Ölçeği pafta ölçeğine yuvarla"));
    connect(round_, &QPushButton::clicked, this, [this] {
        from_frame_ = false;
        show(denominator_, QString::number(rounded_scale(static_cast<double>(denominator()))),
             shown_scale_);
        refresh();
    });
    {
        auto* holder = new QWidget(form);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(kCaption + 6, 0, 0, 0);
        round_->setParent(holder);
        line->addWidget(round_);
        line->addStretch(1);
        formCol->addWidget(holder);
    }

    // AND THE CENTRE, typed the way every coordinate in this program is typed:
    // `sağa, yukarı` in metres (model.md R37a). It opens on the frame's centre,
    // which is what the canvas was aimed at.
    FieldSpec centreSpec   = field_of(FieldKind::Point);
    centreSpec.placeholder = tr("Y,X");
    centre_                = regular(new Field(centreSpec, form));
    centre_->setAccessibleName(tr("Kâğıdın merkezi"));
    connect(centre_, &Field::committed, this, [this](const QString& typed) {
        if (typed.trimmed() != shown_centre_.trimmed()) from_frame_ = false;
        refresh();
    });
    // THE PICK BUTTON MEANS "let me aim again". This window is modal, so a
    // click on the canvas cannot reach it — and the canvas already has the
    // right tool for aiming: the frame. So the button closes the preview and
    // re-opens the frame, which is what a user reaching for it wants.
    connect(centre_, &Field::pickRequested, this, [this](FieldKind) {
        reject();
        emit reaimRequested();
    });
    centre_->setToolTip(tr("Y,X metre. Nişan düğmesi çerçeveyi yeniden açar."));
    caption_row(form, formCol, tr("Merkez (Y, X)"), centre_);

    // WHAT THE SHEET COVERS ON THE GROUND, as a reading beside the two fields
    // that decide it.
    ground_ = new QLabel(form);
    ground_->setObjectName(QStringLiteral("printReading"));
    ground_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    ground_->setAccessibleName(tr("Kâğıdın zeminde kapladığı alan"));
    caption_row(form, formCol, tr("Zeminde"), ground_);

    // WHICH AREA IS IN FORCE, said in words under the figures that decide it,
    // and the way back: a typed scale is a choice, and a choice can be taken
    // back without aiming the frame again (ui.md R37).
    aimNote_ = new QLabel(form);
    note_row(form, formCol, aimNote_);
    backToFrame_ = new Button(ButtonRole::Ghost, tr("Çerçeveye dön"), Glyph::Undo, form);
    backToFrame_->setControlSize(ControlSize::Compact);
    backToFrame_->setToolTip(tr("Yazılan ölçeği ve merkezi bırakır; çerçevenin tuttuğu alan "
                                "yeniden basılır"));
    connect(backToFrame_, &QPushButton::clicked, this, [this] {
        from_frame_ = true;
        refresh();
    });
    {
        auto* holder = new QWidget(form);
        auto* line   = new QHBoxLayout(holder);
        line->setContentsMargins(kCaption + 6, 0, 0, 0);
        backToFrame_->setParent(holder);
        line->addWidget(backToFrame_);
        line->addStretch(1);
        formCol->addWidget(holder);
    }

    // ---- where it goes ----------------------------------------------------------
    group(tr("HEDEF"));
    target_ = new Segment(form);
    target_->addOption(tr("PDF"), tr("Bir PDF dosyasına yazılır"));
    target_->addOption(tr("Yazıcı"), tr("Bir yazıcıya gönderilir"));
    target_->setControlSize(ControlSize::Compact);
    target_->setAccessibleName(tr("Çıktı yeri"));
    // A PRINTER TAKES NO TITLE AND NO PASSWORD, so the two PDF groups go with
    // the PDF rather than sitting disabled under a printer's name.
    connect(target_, &Segment::currentChanged, this, [this](int which) {
        pdfRows_->setVisible(which == 0);
        printerRows_->setVisible(which == 1);
        documentGroup_->setVisible(which == 0);
        protectGroup_->setVisible(which == 0);
        fold(documentRows_, documentToggle_, documentNote_, documentOpen_);
        fold(protectRows_, protectToggle_, protectNote_, protectOpen_);
        refresh();
    });
    caption_row(form, formCol, tr("Çıktı"), target_);

    pdfRows_     = new QWidget(form);
    auto* pdfCol = new QVBoxLayout(pdfRows_);
    pdfCol->setContentsMargins(0, 0, 0, 0);
    pdfCol->setSpacing(6);
    FieldSpec pathSpec   = field_of(FieldKind::Text);
    pathSpec.placeholder = tr("dosya yolu");
    path_                = regular(new Field(pathSpec, pdfRows_));
    path_->setAccessibleName(tr("PDF dosyası")); // the probe's handle, as `round_`'s is
    connect(path_, &Field::committed, this, [this](const QString&) { refresh(); });
    auto* pathRow    = new QWidget(pdfRows_);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(4);
    path_->setParent(pathRow);
    path_->setMinimumWidth(1);
    pathLayout->addWidget(path_, 1);
    auto* browse = new Button(Glyph::Open, tr("Gözat…"), pathRow);
    connect(browse, &QPushButton::clicked, this, &PrintDialog::browse);
    pathLayout->addWidget(browse);
    caption_row(pdfRows_, pdfCol, tr("Dosya"), pathRow);
    formCol->addWidget(pdfRows_);

    printerRows_     = new QWidget(form);
    auto* printerCol = new QVBoxLayout(printerRows_);
    printerCol->setContentsMargins(0, 0, 0, 0);
    printerCol->setSpacing(6);
    printer_ = new ComboBox(printerRows_);
    printer_->addItem(tr("(sistem varsayılanı)"));
    for (const QString& name : QPrinterInfo::availablePrinterNames())
        printer_->addItem(name);
    printer_->setAccessibleName(tr("Yazıcı"));
    connect(printer_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    caption_row(printerRows_, printerCol, tr("Yazıcı"), printer_);
    formCol->addWidget(printerRows_);
    printerRows_->setVisible(false);

    // ---- the PDF's title and author, folded -------------------------------------
    //
    // FOLDED, because a plot rarely needs them — and the note beside the heading
    // says what is in them while they are closed, so nothing is hidden from a
    // reader who does not open them.
    documentGroup_ = group(tr("BELGE BİLGİLERİ"), &documentNote_, &documentToggle_);
    documentRows_  = new QWidget(form);
    auto* docCol   = new QVBoxLayout(documentRows_);
    docCol->setContentsMargins(0, 0, 0, 0);
    docCol->setSpacing(6);
    title_ = regular(new Field(field_of(FieldKind::Text), documentRows_));
    title_->setAccessibleName(tr("Belge başlığı"));
    connect(title_, &Field::committed, this, [this](const QString&) { refresh(); });
    caption_row(documentRows_, docCol, tr("Başlık"), title_);
    author_ = regular(new Field(field_of(FieldKind::Text), documentRows_));
    author_->setAccessibleName(tr("Yazar"));
    connect(author_, &Field::committed, this, [this](const QString&) { refresh(); });
    caption_row(documentRows_, docCol, tr("Yazar"), author_);
    formCol->addWidget(documentRows_);

    // ---- its password, folded ---------------------------------------------------------
    protectGroup_ = group(tr("KORUMA"), &protectNote_, &protectToggle_);
    protectRows_  = new QWidget(form);
    auto* protCol = new QVBoxLayout(protectRows_);
    protCol->setContentsMargins(0, 0, 0, 0);
    protCol->setSpacing(6);
    const bool can_encrypt = PrintService::encryptionAvailable();
    FieldSpec secret       = field_of(FieldKind::Text);
    secret.secret          = true;
    secret.placeholder     = can_encrypt ? tr("şifresiz") : tr("bu yapıda yok");

    password_ = regular(new Field(secret, protectRows_));
    password_->setAccessibleName(tr("Açma şifresi"));
    password_->setEnabled(can_encrypt);
    connect(password_, &Field::committed, this, [this](const QString&) {
        // The permissions only mean something once a password guards them.
        permissions_->setEnabled(!password_->value().isEmpty() ||
                                 !ownerPassword_->value().isEmpty());
        refresh();
    });
    caption_row(protectRows_, protCol, tr("Açma şifresi"), password_);

    ownerPassword_ = regular(new Field(secret, protectRows_));
    ownerPassword_->setAccessibleName(tr("Sahip şifresi"));
    ownerPassword_->setEnabled(can_encrypt);
    ownerPassword_->setToolTip(tr("İzinleri değiştirmek için istenir; boşsa açma şifresiyle aynı"));
    connect(ownerPassword_, &Field::committed, this, [this](const QString&) {
        permissions_->setEnabled(!password_->value().isEmpty() ||
                                 !ownerPassword_->value().isEmpty());
        refresh();
    });
    caption_row(protectRows_, protCol, tr("Sahip şifresi"), ownerPassword_);

    // THE PERMISSIONS, one under the other in the value column: three words
    // side by side did not fit it.
    permissions_  = new QWidget(protectRows_);
    auto* permCol = new QVBoxLayout(permissions_);
    permCol->setContentsMargins(0, 4, 0, 0);
    permCol->setSpacing(6);
    allowPrint_  = new CheckBox(tr("Yazdırılabilir"), permissions_);
    allowCopy_   = new CheckBox(tr("Kopyalanabilir"), permissions_);
    allowModify_ = new CheckBox(tr("Değiştirilebilir"), permissions_);
    for (CheckBox* box : {allowPrint_, allowCopy_, allowModify_}) {
        box->setChecked(true);
        connect(box, &QAbstractButton::toggled, this, [this](bool) { refresh(); });
        permCol->addWidget(box);
    }
    permissions_->setEnabled(false);
    permissions_->setToolTip(tr("Sahip şifresini bilmeyen bir okuyucunun yapabilecekleri"));
    caption_row(protectRows_, protCol, tr("İzinler"), permissions_);
    if (!can_encrypt) {
        auto* why = new QLabel(tr("Bu yapıda PDF şifrelemesi yok."), protectRows_);
        note_row(protectRows_, protCol, why);
    }
    formCol->addWidget(protectRows_);

    connect(documentToggle_, &QPushButton::clicked, this, [this] {
        documentOpen_ = !documentOpen_;
        fold(documentRows_, documentToggle_, documentNote_, documentOpen_);
    });
    connect(protectToggle_, &QPushButton::clicked, this, [this] {
        protectOpen_ = !protectOpen_;
        fold(protectRows_, protectToggle_, protectNote_, protectOpen_);
    });
    fold(documentRows_, documentToggle_, documentNote_, documentOpen_);
    fold(protectRows_, protectToggle_, protectNote_, protectOpen_);
    formCol->addStretch(1);

    // ---- the command strip, under both (ui.md R38) ----------------------------------
    //
    // THE LINE THE WINDOW WILL SEND, the width of the window: it is what is
    // sent — to the bus, as a script would send it — and a line squeezed into
    // the settings column wrapped four times and could not be read as one.
    auto* strip = new QWidget(body);
    strip->setObjectName(QStringLiteral("printCommandStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    auto* stripRow = new QHBoxLayout(strip);
    stripRow->setContentsMargins(16, 8, 16, 8);
    stripRow->setSpacing(12);
    auto* said = new QLabel(tr("KOMUT"), strip);
    said->setObjectName(QStringLiteral("groupCaption"));
    stripRow->addWidget(said, 0, Qt::AlignVCenter);
    command_ = new QLabel(strip);
    command_->setObjectName(QStringLiteral("commandPreview"));
    command_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    command_->setWordWrap(true);
    command_->setAccessibleName(tr("Gönderilecek komut satırı"));
    stripRow->addWidget(command_, 1);
    column->addWidget(strip);

    setBody(body);

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);
    go_ = new Button(ButtonRole::Primary, tr("Yazdır"), Glyph::Print, this);
    go_->setDefault(true);
    connect(go_, &QPushButton::clicked, this, &PrintDialog::run);
    footer()->addWidget(go_);

    // THE SETTINGS WINDOW'S SIZE, the sibling it is (design.md §4): room for a
    // sheet that can be read beside a column of settings. Never taller than the
    // screen — a dialog whose primary button is under the dock cannot be
    // pressed — and never smaller than 940 × 620, where the sheet still reads
    // and the column scrolls rather than cutting a row.
    setMinimumSize(940, 620);
    const QRect room = QApplication::primaryScreen() != nullptr
                           ? QApplication::primaryScreen()->availableGeometry()
                           : QRect(0, 0, 1280, 800);
    resize(QSize(std::min(1180, room.width() - 80), std::min(760, room.height() - 80))
               .expandedTo(minimumSize()));
}

void PrintDialog::fold(QWidget* rows, Button* toggle, QLabel* note, bool open)
{
    const bool pdf = target_ == nullptr || target_->current() == 0;
    rows->setVisible(open && pdf);
    // THE NOTE STANDS FOR THE ROWS, so it goes when they come: open, the
    // title was written twice, once in its box and once beside the heading.
    note->setVisible(!open);
    // THE CHEVRON SAYS WHERE THE GROUP WILL GO, a shape and not only a state
    // (design.md §13): down to open it, up to put it away.
    toggle->setGlyph(open ? Glyph::ChevronUp : Glyph::ChevronDown);
    const QString verb = open ? tr("Kapat") : tr("Aç");
    toggle->setToolTip(verb);
    toggle->setAccessibleName(verb);
}

QString PrintDialog::commandLine() const
{
    // THE AREA, SAID THE WAY THE USER SAID IT, because the two ways are not the
    // same area. Two corners are the frame's box to the millimetre; a centre and
    // a scale are a round figure that a sheet can be filed under, and rounding
    // the corners into one would put ground on the paper that was never framed.
    // So the line carries whichever the form is holding, and the line is
    // therefore also how a reader can tell which is in force.
    QString line;
    if (from_frame_) {
        const core::Box2 box = window();
        line = QStringLiteral("YAZDIR pencere=%1,%2 pencere=%3,%4")
                   .arg(metres(box.min_x), metres(box.min_y), metres(box.max_x), metres(box.max_y));
    } else {
        const core::Point2 at = centre();
        line = QStringLiteral("YAZDIR merkez=%1,%2 olcek=%3")
                   .arg(metres(at.x), metres(at.y), QString::number(denominator()));
    }

    if (target_ != nullptr && target_->current() == 1) {
        const QString name = printer_->currentIndex() <= 0 ? QString() : printer_->currentText();
        line += QStringLiteral(" yazici=") + quoted(name);
    } else {
        const QString path = path_ != nullptr ? path_->value().trimmed() : QString();
        if (path.isEmpty()) return {};
        line += QStringLiteral(" dosya=") + quoted(path);
    }

    // THE PROFILE BY NAME AND NOTHING ELSE. The sheet is not overridden here, so
    // the line stays short and a replay follows the profile — including after
    // the office changes what "A3 Yatay" means.
    line += QStringLiteral(" profil=") + quoted(profile_->currentText());

    if (target_ != nullptr && target_->current() == 0) {
        if (const QString what = title_->value().trimmed(); !what.isEmpty())
            line += QStringLiteral(" baslik=") + quoted(what);
        if (const QString who = author_->value().trimmed(); !who.isEmpty())
            line += QStringLiteral(" yazar=") + quoted(who);
        if (const QString secret = password_->value(); !secret.isEmpty())
            line += QStringLiteral(" sifre=") + quoted(secret);
        if (const QString owner = ownerPassword_->value(); !owner.isEmpty())
            line += QStringLiteral(" sahip_sifresi=") + quoted(owner);

        // Only what is WITHHELD goes on the line: everything is allowed by
        // default, and a line that spelled out three `evet` would say nothing.
        const bool guarded = !password_->value().isEmpty() || !ownerPassword_->value().isEmpty();
        if (guarded) {
            if (!allowPrint_->isChecked()) line += QStringLiteral(" yazdirilabilir=hayır");
            if (!allowCopy_->isChecked()) line += QStringLiteral(" kopyalanabilir=hayır");
            if (!allowModify_->isChecked()) line += QStringLiteral(" degistirilebilir=hayır");
        }
    }
    return line;
}

void PrintDialog::renderSheet()
{
    if (sheet_ == nullptr) return;
    const io::PrintProfile p = chosen();
    sheet_->setPaper(static_cast<double>(p.sheet_width_mm()),
                     static_cast<double>(p.sheet_height_mm()), static_cast<double>(p.margin_mm));
    const int pixels = sheet_->wantedPixels();
    if (pixels <= 0) return;

    // DRAWN AGAIN ONLY WHEN WHAT IT SHOWS MOVED: the paper, the area or the
    // pixels. A title typed or a permission ticked changes the line and not the
    // sheet, and each picture is a pass of the plot's pipeline over the whole
    // drawing. The drawing itself cannot change under a modal window.
    const core::Box2 box = window();
    const QString key    = QStringLiteral("%1|%2,%3,%4,%5|%6")
                               .arg(utf8(p.name))
                               .arg(box.min_x)
                               .arg(box.min_y)
                               .arg(box.max_x)
                               .arg(box.max_y)
                               .arg(pixels);
    if (key == drawnFor_) return;
    drawnFor_ = key;

    // DRAWN FOR THE PIXELS THE PAPER TAKES, so the picture is never scaled up
    // into a blur on a large window or down into moiré on a small one.
    sheet_->setImage(PrintService::renderPreview(controller_.document(), p, box, pixels));
}

void PrintDialog::refresh()
{
    const io::PrintProfile p  = chosen();
    const core::Box2 on_paper = window();

    // WHILE THE FRAME'S BOX IS IN FORCE the two fields REPORT it rather than
    // decide it, and they are rewritten here because the box can move under
    // them: changing the profile changes the paper, and a scale that no longer
    // describes the sheet would be the same untruth the silent rounding was.
    // `show` keeps this from reading back as an edit.
    if (from_frame_ && !on_paper.empty()) {
        const double implied = PrintService::scaleDenominator(p, on_paper);
        if (implied > 0.0) show(denominator_, QString::number(std::llround(implied)), shown_scale_);
        show(centre_,
             QStringLiteral("%1,%2").arg(metres((on_paper.min_x + on_paper.max_x) / 2),
                                         metres((on_paper.min_y + on_paper.max_y) / 2)),
             shown_centre_);
    }

    // THE LADDER, OFFERED AND PRICED: the button says what it would round to
    // and disappears — with its row — when the scale is already a plan scale.
    const std::int64_t here = denominator();
    if (round_ != nullptr) {
        const std::int64_t rounder = rounded_scale(static_cast<double>(here));
        round_->parentWidget()->setVisible(rounder != here);
        round_->setText(tr("1:%1 ölçeğine yuvarla").arg(rounder));
        round_->setToolTip(tr("Ölçeği pafta ölçeğine yuvarlar. Kâğıda daha çok yer girer; "
                              "çerçevenin dışı da basılır."));
        round_->setAccessibleDescription(round_->text());
    }

    // WHICH AREA IS PRINTED, in words, and the way back once it is not the frame's.
    aimNote_->setText(from_frame_ ? tr("Çerçevenin tuttuğu alan basılır.")
                                  : tr("Alanı merkez ve ölçek belirliyor."));
    backToFrame_->parentWidget()->setVisible(!from_frame_);

    // WHAT THE PROFILE SAYS, once, under its name: the paper, its direction,
    // the resolution and the margin, which this window shows and does not set.
    const QString direction = p.landscape ? tr("yatay") : tr("dikey");
    paper_->setText(tr("%1 %2, %3 × %4 mm\n%5 dpi, kenar payı %6 mm")
                        .arg(utf8(p.paper), direction, QString::number(p.sheet_width_mm()),
                             QString::number(p.sheet_height_mm()), QString::number(p.dpi),
                             QString::number(p.margin_mm)));

    // WHAT THE SHEET COVERS, in metres of ground: the other half of the scale,
    // and the figure a plan sheet's own legend carries.
    const QString wide =
        QString::number(core::mm_to_metres(on_paper.max_x - on_paper.min_x), 'f', 1);
    const QString tall =
        QString::number(core::mm_to_metres(on_paper.max_y - on_paper.min_y), 'f', 1);
    ground_->setText(tr("%1 × %2 m").arg(wide, tall));
    summary_->setText(tr("1:%1 ölçekte %2 %3 kâğıt, zeminde %4 × %5 m")
                          .arg(QString::number(here), utf8(p.paper), direction, wide, tall));

    // THE FOLDED GROUPS SAY WHAT IS IN THEM while they are shut.
    if (documentNote_ != nullptr) {
        const QString what = title_->value().trimmed();
        const QString who  = author_->value().trimmed();
        const QString said = !what.isEmpty() ? what : who;
        documentNote_->setText(
            said.isEmpty() ? tr("boş")
                           : documentNote_->fontMetrics().elidedText(said, Qt::ElideRight, 150));
    }
    if (protectNote_ != nullptr) {
        const bool guarded = !password_->value().isEmpty() || !ownerPassword_->value().isEmpty();
        if (!PrintService::encryptionAvailable())
            protectNote_->setText(tr("bu yapıda yok"));
        else
            protectNote_->setText(guarded ? tr("şifreli") : tr("şifresiz"));
    }

    renderSheet();
    const QString line = commandLine();
    command_->setText(line.isEmpty() ? tr("Dosya adı verilince komut burada görünür.") : line);
    command_->setProperty("empty", line.isEmpty());
    style()->unpolish(command_);
    style()->polish(command_);
    go_->setEnabled(!line.isEmpty());
}

void PrintDialog::browse()
{
    const QString chosen_path = QFileDialog::getSaveFileName(this, tr("PDF olarak kaydet"),
                                                             path_->value(), tr("PDF (*.pdf)"));
    if (chosen_path.isEmpty()) return;
    path_->setValue(chosen_path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)
                        ? chosen_path
                        : chosen_path + QStringLiteral(".pdf"));
    refresh();
}

void PrintDialog::run()
{
    const QString line = commandLine();
    if (line.isEmpty()) return;

    // THE ANSWER IS READ, exactly as the export window reads it: a plot that was
    // refused — no such printer, a directory that is not there, a margin that
    // leaves no paper — leaves its reason above the form.
    auto printed = controller_.runLineResult(line, command::Origin::Gui);
    if (!printed) {
        trouble_->setText(QString::fromStdString(printed.error().message));
        trouble_->setVisible(true);
        // AT THE TOP OF A COLUMN THAT SCROLLS, so it is brought into view: a
        // user who opened the password rows is looking at the bottom of it.
        column_->ensureWidgetVisible(trouble_);
        return;
    }
    accept();
}

void PrintDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

} // namespace piricad::app
