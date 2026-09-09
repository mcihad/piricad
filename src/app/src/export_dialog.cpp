// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/export_dialog.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/selection.hpp"
#include "kentos_cad/io/vector.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kColumnGap = 26; ///< the form grammar's gutter, design.md §16.1
constexpr int kBodyPad   = 18;

/// A value quoted for the command line: the parser treats a quoted value as
/// literal, so this is the whole escaping rule.
QString quoted(const QString& value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

} // namespace

ExportDialog::ExportDialog(Controller& controller, ExportSubject subject, QString context,
                           QWidget* parent)
    : DialogFrame(parent), controller_(controller), subject_(subject), context_(std::move(context))
{
    // The corners' owners are read ONCE, here, and written into the line. The
    // selection is session state (model.md R43); a journal line that said "what
    // was selected" would replay differently on another day.
    if (subject_ == ExportSubject::Coordinates) {
        QStringList ids;
        for (const core::EntityKey key : controller_.bus().selection().keys())
            ids << QString::number(static_cast<qulonglong>(key));
        selection_ = ids.join(QLatin1Char(','));
    }

    QString what;
    switch (subject_) {
    case ExportSubject::Drawing: what = tr("— çizim"); break;
    case ExportSubject::Coordinates:
        what = selection_.contains(QLatin1Char(','))
                   ? tr("— %1 nesnenin köşeleri").arg(selection_.count(QLatin1Char(',')) + 1)
                   : tr("— nesne %1 köşeleri").arg(selection_);
        break;
    case ExportSubject::Style: what = tr("— %1 stili").arg(context_); break;
    }
    setHeading(Glyph::Export, tr("Dışa Aktar"), what);
    setMinimumWidth(760);

    collectFormats();
    build();
    refreshCommand();
}

void ExportDialog::collectFormats()
{
    formats_.clear();
    switch (subject_) {
    case ExportSubject::Drawing:
        // The io layer's allow-list, writers only — the same list the command
        // resolves `bicim` against, so the window cannot offer what the command
        // would refuse (io.md P7).
        for (const io::VectorFormat& f : io::vector_formats()) {
            if (!f.write) continue;
            formats_.push_back({QString::fromStdString(f.driver), QString::fromStdString(f.label),
                                QString::fromStdString(f.extension),
                                f.driver == "GPKG"
                                    ? tr("Geometriyle birlikte öznitelikleri de taşır")
                                    : tr("Katmanlar ve çizgi tipleri korunur")});
        }
        break;
    case ExportSubject::Coordinates:
        formats_.push_back({QStringLiteral("noktalar"), tr("Nokta listesi"), QStringLiteral(".txt"),
                            tr("nokta no; Y; X — noktalı virgülle ayrılmış, ondalık noktalı")});
        break;
    case ExportSubject::Style:
        formats_.push_back(
            {QStringLiteral("qml"), tr("QGIS stil dosyası"), QStringLiteral(".qml"),
             tr("Katmanın sembolojisi; QGIS'te Katman ▸ Özellikler ▸ Stil ▸ Yükle")});
        break;
    }
}

void ExportDialog::build()
{
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(kBodyPad, 14, kBodyPad, 12);
    column->setSpacing(14);

    // What went wrong, when something did. Hidden until then; a window that
    // opens with an empty warning strip is a window that cries wolf.
    trouble_ = new Banner(Tone::Danger, tr("Dışa aktarılamadı"), QString(), body);
    trouble_->setVisible(false);
    column->addWidget(trouble_);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(kColumnGap);

    // ---- BİÇİM: one choice among the formats the command accepts ----
    auto* left    = new QWidget(body);
    auto* leftCol = new QVBoxLayout(left);
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(6);
    leftCol->addWidget(new FormSection(tr("BİÇİM"), QString(), left));
    for (int i = 0; i < formats_.size(); ++i) {
        const Format& f = formats_[i];
        auto* choice    = new RadioButton(tr("%1 (%2)").arg(f.label, f.extension), left);
        choice->setChecked(i == chosen_);
        connect(choice, &QAbstractButton::toggled, this, [this, i](bool on) {
            if (!on) return;
            chosen_ = i;
            refreshCommand();
        });
        choices_.push_back(choice);
        leftCol->addWidget(choice);

        auto* note = new QLabel(f.note, left);
        note->setObjectName(QStringLiteral("formHelp"));
        note->setWordWrap(true);
        note->setContentsMargins(26, 0, 0, 6);
        leftCol->addWidget(note);
    }
    leftCol->addStretch(1);
    columns->addWidget(left, 1);

    // ---- HEDEF and SEÇENEKLER ----
    auto* right    = new QWidget(body);
    auto* rightCol = new QVBoxLayout(right);
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(10);
    rightCol->addWidget(new FormSection(tr("HEDEF"), QString(), right));

    FieldSpec pathSpec   = field_of(FieldKind::Text);
    pathSpec.placeholder = tr("dosya yolu");
    path_                = new Field(pathSpec, right);
    path_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    path_->setAccessibleName(tr("Yazılacak dosya"));
    connect(path_, &Field::committed, this, [this](const QString&) { refreshCommand(); });

    auto* pathRow    = new QWidget(right);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(8);
    pathLayout->addWidget(path_, 1);
    auto* browse = new Button(ButtonRole::Secondary, tr("Gözat…"), Glyph::Open, right);
    connect(browse, &QPushButton::clicked, this, &ExportDialog::browse);
    pathLayout->addWidget(browse);
    auto* pathForm = new FormRow(tr("Dosya"), pathRow, right);
    pathForm->setRequired(true);
    rightCol->addWidget(pathForm);

    if (subject_ == ExportSubject::Coordinates) {
        rightCol->addWidget(new FormSection(tr("SEÇENEKLER"), QString(), right));
        axes_ = new Segment(right);
        axes_->addOption(tr("Y X"), tr("Türkiye'de olağan sıra: sağa, sonra yukarı"));
        axes_->addOption(tr("X Y"), tr("Matematik sırası, XY isteyen bir program için"));
        axes_->setControlSize(ControlSize::Regular);
        connect(axes_, &Segment::currentChanged, this, [this](int) { refreshCommand(); });
        auto* axesRow = new FormRow(tr("Sütun sırası"), axes_, right);
        axesRow->setHelp(tr("Her köşe bir satır: nesne.köşe numarası, iki koordinat, katman adı."));
        rightCol->addWidget(axesRow);
    }
    if (subject_ == ExportSubject::Style) {
        rightCol->addWidget(new FormSection(tr("SEÇENEKLER"), QString(), right));
        FieldSpec layerSpec = field_of(FieldKind::Text);
        auto* layer         = new Field(layerSpec, right);
        layer->setFixedHeight(static_cast<int>(ControlSize::Regular));
        layer->setValue(context_);
        layer->setState(FieldState::ReadOnly);
        layer->setEnabled(false);
        rightCol->addWidget(new FormRow(tr("Katman"), layer, right));
    }
    rightCol->addStretch(1);
    columns->addWidget(right, 1);
    column->addLayout(columns, 1);

    // ---- KOMUT: the line that will run, and the one thing this window promises ----
    column->addWidget(new FormSection(
        tr("KOMUT"), tr("aynı satır komut satırından ve betikten de yazılır"), body));
    command_ = new QLabel(body);
    command_->setObjectName(QStringLiteral("commandPreview"));
    command_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    command_->setWordWrap(true);
    column->addWidget(command_);

    setBody(body);

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);

    go_ = new Button(ButtonRole::Primary, tr("Dışa aktar"), Glyph::Export, this);
    go_->setDefault(true);
    connect(go_, &QPushButton::clicked, this, &ExportDialog::runExport);
    footer()->addWidget(go_);
}

void ExportDialog::browse()
{
    if (formats_.isEmpty()) return;
    const Format& f      = formats_[chosen_];
    const QString filter = tr("%1 (*%2);;Tüm dosyalar (*)").arg(f.label, f.extension);
    QString path = QFileDialog::getSaveFileName(this, tr("Dışa aktar"), path_->value(), filter);
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += f.extension;
    path_->setValue(path);
    refreshCommand();
}

QString ExportDialog::commandLine() const
{
    const QString path = path_ != nullptr ? path_->value().trimmed() : QString();
    if (path.isEmpty() || formats_.isEmpty()) return {};

    const Format& f = formats_[chosen_];
    switch (subject_) {
    case ExportSubject::Drawing:
        return QStringLiteral("DIŞAAKTAR dosya=%1 bicim=%2").arg(quoted(path), f.id);
    case ExportSubject::Coordinates: {
        QString line = QStringLiteral("NOKTALAR dosya=%1 yon=yaz").arg(quoted(path));
        if (!selection_.isEmpty()) line += QStringLiteral(" nesneler=%1").arg(selection_);
        if (axes_ != nullptr && axes_->current() == 1) line += QStringLiteral(" eksen=XY");
        return line;
    }
    case ExportSubject::Style:
        return QStringLiteral("STİLAKTAR katman=%1 dosya=%2").arg(quoted(context_), quoted(path));
    }
    return {};
}

void ExportDialog::refreshCommand()
{
    const QString line = commandLine();
    command_->setText(line.isEmpty() ? tr("Dosya adı verilince komut burada görünür.") : line);
    command_->setProperty("empty", line.isEmpty());
    style()->unpolish(command_);
    style()->polish(command_);
    go_->setEnabled(!line.isEmpty());
}

void ExportDialog::runExport()
{
    const QString line = commandLine();
    if (line.isEmpty()) return;

    // THE ANSWER IS READ. A command that refused — a driver that is not built, a
    // directory that does not exist, a layer that was renamed — leaves its reason
    // in the window, above the form, where the user can act on it.
    auto written = controller_.runLineResult(line, command::Origin::Gui);
    if (!written) {
        trouble_->setText(QString::fromStdString(written.error().message));
        trouble_->setVisible(true);
        return;
    }
    accept();
}

void ExportDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

} // namespace kentos::app
