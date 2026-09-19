// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/print_dialog.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/print_service.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <cmath>

namespace kentos::app {
namespace {

constexpr int kBodyPad   = 14;
constexpr int kRowGap    = 6;   ///< between the compact rows; the shell's tight spacing
constexpr int kPreviewPx = 380; ///< the sheet's longer side on screen

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
            if (factor == 2 && step >= 100 && static_cast<double>(step * 5 / 2) >= implied - 0.5)
                return step * 5 / 2;
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
    setHeading(Glyph::Print, tr("Yazdır"), tr("— önizleme"));
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
    auto* body = new QWidget(this);
    auto* row  = new QHBoxLayout(body);
    row->setContentsMargins(kBodyPad, kBodyPad, kBodyPad, kBodyPad);
    row->setSpacing(16);

    // ---- the sheet, on the left ---------------------------------------------
    auto* left    = new QWidget(body);
    auto* leftCol = new QVBoxLayout(left);
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(kRowGap);
    sheet_ = new QLabel(left);
    sheet_->setObjectName(QStringLiteral("printSheet"));
    sheet_->setAlignment(Qt::AlignCenter);
    sheet_->setMinimumSize(kPreviewPx, kPreviewPx);
    sheet_->setAccessibleName(tr("Yazdırma önizlemesi"));
    leftCol->addWidget(sheet_, 1);

    paper_ = new QLabel(left);
    paper_->setObjectName(QStringLiteral("formHelp"));
    paper_->setAlignment(Qt::AlignCenter);
    paper_->setAccessibleName(tr("Kâğıt"));
    leftCol->addWidget(paper_);
    row->addWidget(left, 1);

    // ---- the form, on the right ---------------------------------------------
    //
    // A FORM LAYOUT AND COMPACT CONTROLS: every row is label-left, editor-right
    // at 24 px, which is the standard's dense row (`ControlSize::Compact`). The
    // previous shape put every editor under its own heading at 30 px and ran off
    // the bottom of the screen with the footer below the fold.
    auto* right    = new QWidget(body);
    auto* rightCol = new QVBoxLayout(right);
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(kRowGap);
    // A FORM COLUMN HAS A WIDTH, and it is not "whatever is left": a title box
    // five hundred pixels wide reads as a text editor rather than as a field,
    // and the eye has to travel the whole width to pair a label with its box.
    right->setMinimumWidth(320);
    right->setMaximumWidth(360);

    trouble_ = new Banner(Tone::Danger, tr("Yazdırılamadı"), QString(), right);
    trouble_->setVisible(false);
    rightCol->addWidget(trouble_);

    const auto compact = [](Field* f) {
        f->setFixedHeight(static_cast<int>(ControlSize::Compact));
        return f;
    };
    const auto form = [](QWidget* parent) {
        auto* layout = new QFormLayout(parent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(kRowGap);
        layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return layout;
    };

    // ---- the profile: the sheet's one source --------------------------------
    auto* sheetBlock = new QWidget(right);
    auto* sheetForm  = form(sheetBlock);
    profile_         = new ComboBox(sheetBlock);
    profile_->setControlSize(ControlSize::Compact);
    const io::PrintProfiles& store = controller_.printService().profiles();
    for (const io::PrintProfile& p : store.all())
        profile_->addItem(utf8(p.name));
    profile_->setCurrentText(utf8(store.default_name()));
    profile_->setAccessibleName(tr("Yazdırma profili"));
    connect(profile_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    sheetForm->addRow(tr("Profil"), profile_);

    // WHERE THE SHEET IS SET, said once and linked under the profile it belongs
    // to: this window shows the paper and does not offer it, so this is how a
    // user changes paper, orientation, resolution or margin.
    auto* manage =
        new Button(ButtonRole::Ghost, tr("Profilleri yönet…"), Glyph::Settings, sheetBlock);
    manage->setControlSize(ControlSize::Compact);
    manage->setToolTip(tr("Kâğıt, yön, çözünürlük ve kenar boşluğu profilde tutulur"));
    connect(manage, &QPushButton::clicked, this, [this] {
        accept();
        emit manageProfilesRequested();
    });
    sheetForm->addRow(QString(), manage);

    // THE SCALE. It opens on the frame's own — exactly, so the sheet is the
    // frame — and typing over it is what makes the scale decide the area
    // instead. The comparison against `shown_scale_` is what tells an edit from
    // a field that was only tabbed through: `Field` commits its value on focus
    // leaving too, and an unchanged value is not somebody choosing a scale.
    denominator_ = compact(new Field(number_of(1, 1000000), sheetBlock));
    denominator_->setAccessibleName(tr("Ölçek paydası"));
    denominator_->setToolTip(tr("Çerçevenin ölçeğiyle açılır. Bir değer yazmak alanı o "
                                "ölçeğe göre yeniden kurar."));
    connect(denominator_, &Field::committed, this, [this](const QString& typed) {
        if (typed.trimmed() != shown_scale_.trimmed()) from_frame_ = false;
        refresh();
    });
    auto* scaleRow = new QWidget(sheetBlock);
    auto* scaleBox = new QHBoxLayout(scaleRow);
    scaleBox->setContentsMargins(0, 0, 0, 0);
    scaleBox->setSpacing(4);
    auto* one = new QLabel(QStringLiteral("1 :"), scaleRow);
    scaleBox->addWidget(one);
    scaleBox->addWidget(denominator_, 1);

    // THE ROUND FIGURE, OFFERED. A pafta is drawn at a scale somebody can read
    // off a legend, so the ladder is worth one press — but it is a press, and
    // the sheet beside it shows the ground it adds. `refresh` writes the label
    // and hides the button when the scale is already round.
    round_ = new Button(ButtonRole::Ghost, tr("Yuvarla"), std::nullopt, scaleRow);
    round_->setControlSize(ControlSize::Compact);
    // A HANDLE FOR THE PROBE, not a role: `MainWindow::probePrintLine` presses
    // this button to prove the rounding is a press rather than something the
    // window does behind the user (CLAUDE.md 5.19 bans an object name that
    // stands in for a role; this one names an instance).
    round_->setObjectName(QStringLiteral("printRoundScale"));
    connect(round_, &QPushButton::clicked, this, [this] {
        from_frame_ = false;
        show(denominator_, QString::number(rounded_scale(static_cast<double>(denominator()))),
             shown_scale_);
        refresh();
    });
    scaleBox->addWidget(round_);
    sheetForm->addRow(tr("Ölçek"), scaleRow);

    // AND THE CENTRE, typed the way every coordinate in this program is typed:
    // `sağa, yukarı` in metres (model.md R37a). It opens on the frame's centre,
    // which is what the canvas was aimed at.
    FieldSpec centreSpec   = field_of(FieldKind::Point);
    centreSpec.placeholder = tr("Y,X");
    centre_                = compact(new Field(centreSpec, sheetBlock));
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
    sheetForm->addRow(tr("Merkez"), centre_);

    rightCol->addWidget(sheetBlock);

    // ---- where it goes -------------------------------------------------------
    auto* targetBlock = new QWidget(right);
    auto* targetForm  = form(targetBlock);
    target_           = new Segment(targetBlock);
    target_->addOption(tr("PDF"), tr("Bir PDF dosyasına yazılır"));
    target_->addOption(tr("Yazıcı"), tr("Bir yazıcıya gönderilir"));
    target_->setControlSize(ControlSize::Compact);
    target_->setAccessibleName(tr("Çıktı yeri"));
    connect(target_, &Segment::currentChanged, this, [this](int which) {
        pdfRows_->setVisible(which == 0);
        printerRows_->setVisible(which == 1);
        refresh();
    });
    targetForm->addRow(tr("Çıktı"), target_);
    rightCol->addWidget(targetBlock);

    // ---- the PDF's own rows --------------------------------------------------
    pdfRows_      = new QWidget(right);
    auto* pdfForm = form(pdfRows_);

    FieldSpec pathSpec   = field_of(FieldKind::Text);
    pathSpec.placeholder = tr("dosya yolu");
    path_                = compact(new Field(pathSpec, pdfRows_));
    path_->setAccessibleName(tr("PDF dosyası"));
    path_->setObjectName(QStringLiteral("printPdfPath")); // see `printRoundScale` above
    connect(path_, &Field::committed, this, [this](const QString&) { refresh(); });
    auto* pathRow    = new QWidget(pdfRows_);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(4);
    pathLayout->addWidget(path_, 1);
    auto* browse = new Button(Glyph::Open, tr("Gözat…"), pathRow);
    connect(browse, &QPushButton::clicked, this, &PrintDialog::browse);
    pathLayout->addWidget(browse);
    pdfForm->addRow(tr("Dosya"), pathRow);

    title_ = compact(new Field(field_of(FieldKind::Text), pdfRows_));
    title_->setAccessibleName(tr("Belge başlığı"));
    connect(title_, &Field::committed, this, [this](const QString&) { refresh(); });
    pdfForm->addRow(tr("Başlık"), title_);

    author_ = compact(new Field(field_of(FieldKind::Text), pdfRows_));
    author_->setAccessibleName(tr("Yazar"));
    connect(author_, &Field::committed, this, [this](const QString&) { refresh(); });
    pdfForm->addRow(tr("Yazar"), author_);

    const bool can_encrypt = PrintService::encryptionAvailable();
    FieldSpec secret       = field_of(FieldKind::Text);
    secret.secret          = true;
    secret.placeholder     = can_encrypt ? tr("şifresiz") : tr("bu yapıda yok");

    password_ = compact(new Field(secret, pdfRows_));
    password_->setAccessibleName(tr("Açma şifresi"));
    password_->setEnabled(can_encrypt);
    connect(password_, &Field::committed, this, [this](const QString&) {
        // The permissions only mean something once a password guards them.
        permissions_->setEnabled(!password_->value().isEmpty() ||
                                 !ownerPassword_->value().isEmpty());
        refresh();
    });
    pdfForm->addRow(tr("Şifre"), password_);

    ownerPassword_ = compact(new Field(secret, pdfRows_));
    ownerPassword_->setAccessibleName(tr("Sahip şifresi"));
    ownerPassword_->setEnabled(can_encrypt);
    ownerPassword_->setToolTip(tr("İzinleri değiştirmek için istenir; boşsa açma şifresiyle aynı"));
    connect(ownerPassword_, &Field::committed, this, [this](const QString&) {
        permissions_->setEnabled(!password_->value().isEmpty() ||
                                 !ownerPassword_->value().isEmpty());
        refresh();
    });
    pdfForm->addRow(tr("Sahip şifresi"), ownerPassword_);

    permissions_  = new QWidget(pdfRows_);
    auto* permRow = new QHBoxLayout(permissions_);
    permRow->setContentsMargins(0, 0, 0, 0);
    permRow->setSpacing(10);
    allowPrint_  = new CheckBox(tr("Yazdır"), permissions_);
    allowCopy_   = new CheckBox(tr("Kopyala"), permissions_);
    allowModify_ = new CheckBox(tr("Değiştir"), permissions_);
    for (CheckBox* box : {allowPrint_, allowCopy_, allowModify_}) {
        box->setChecked(true);
        connect(box, &QAbstractButton::toggled, this, [this](bool) { refresh(); });
        permRow->addWidget(box);
    }
    permRow->addStretch(1);
    permissions_->setEnabled(false);
    permissions_->setToolTip(tr("Sahip şifresini bilmeyen bir okuyucunun yapabilecekleri"));
    pdfForm->addRow(tr("İzinler"), permissions_);
    rightCol->addWidget(pdfRows_);

    // ---- the printer's one row ----------------------------------------------
    printerRows_      = new QWidget(right);
    auto* printerForm = form(printerRows_);
    printer_          = new ComboBox(printerRows_);
    printer_->setControlSize(ControlSize::Compact);
    printer_->addItem(tr("(sistem varsayılanı)"));
    for (const QString& name : QPrinterInfo::availablePrinterNames())
        printer_->addItem(name);
    printer_->setAccessibleName(tr("Yazıcı"));
    connect(printer_, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    printerForm->addRow(tr("Yazıcı"), printer_);
    rightCol->addWidget(printerRows_);
    printerRows_->setVisible(false);

    rightCol->addStretch(1);

    // The line, at the bottom of the form where it is read last and copied.
    command_ = new QLabel(right);
    command_->setObjectName(QStringLiteral("commandPreview"));
    command_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    command_->setWordWrap(true);
    command_->setAccessibleName(tr("Gönderilecek komut satırı"));
    rightCol->addWidget(command_);
    row->addWidget(right, 0);

    setBody(body);

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);
    go_ = new Button(ButtonRole::Primary, tr("Yazdır"), Glyph::Print, this);
    go_->setDefault(true);
    connect(go_, &QPushButton::clicked, this, &PrintDialog::run);
    footer()->addWidget(go_);

    // SIZED TO THE SCREEN, not to the content: the window has a preview in it
    // and a footer under it, and a dialog taller than the display is a dialog
    // whose primary button cannot be pressed — which is what the first cut did.
    const QRect room = QApplication::primaryScreen() != nullptr
                           ? QApplication::primaryScreen()->availableGeometry()
                           : QRect(0, 0, 1280, 800);
    resize(std::min(840, room.width() - 80), std::min(560, room.height() - 80));
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
        line                 = QStringLiteral("YAZDIR pencere=%1,%2 pencere=%3,%4")
                   .arg(metres(box.min_x), metres(box.min_y), metres(box.max_x), metres(box.max_y));
    } else {
        const core::Point2 at = centre();
        line                  = QStringLiteral("YAZDIR merkez=%1,%2 olcek=%3")
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
    // and disappears when the scale is already a plan scale.
    if (round_ != nullptr) {
        const std::int64_t here    = denominator();
        const std::int64_t rounder = rounded_scale(static_cast<double>(here));
        round_->setVisible(rounder != here);
        round_->setToolTip(tr("Ölçeği pafta ölçeğine yuvarlar: 1 : %1. Kâğıda daha çok yer "
                              "girer; çerçevenin dışı da basılır.")
                               .arg(rounder));
        round_->setAccessibleName(tr("Ölçeği 1 : %1'e yuvarla").arg(rounder));
    }

    const QImage sheet =
        PrintService::renderPreview(controller_.document(), p, on_paper, kPreviewPx);
    if (!sheet.isNull()) sheet_->setPixmap(QPixmap::fromImage(sheet));

    // WHAT THE SHEET COVERS, in metres of ground: the other half of the scale,
    // and the figure a plan sheet's own legend carries.
    paper_->setText(
        tr("%1 %2×%3 mm %4  ·  %5 dpi  ·  kenar %6 mm\n%7 × %8 m yer kaplar")
            .arg(utf8(p.paper), QString::number(p.sheet_width_mm()),
                 QString::number(p.sheet_height_mm()), p.landscape ? tr("yatay") : tr("dikey"),
                 QString::number(p.dpi), QString::number(p.margin_mm),
                 QString::number(core::mm_to_metres(on_paper.max_x - on_paper.min_x), 'f', 1),
                 QString::number(core::mm_to_metres(on_paper.max_y - on_paper.min_y), 'f', 1)));

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
        return;
    }
    accept();
}

void PrintDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

} // namespace kentos::app
