// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/find_replace_dialog.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/widgets.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cstdint>

namespace kentos::app {
namespace {

enum Column : std::uint8_t { Key = 0, Before, After, ColumnCount };

constexpr int kHeaderRow   = 30;
constexpr int kTableRow    = 28;
constexpr int kWindowWidth = 720;
constexpr int kWindowTall  = 560;

/// A field's text as the lexer reads it inside quotes: a typed `\n` is a line
/// break, and a quote or a backslash cannot end the argument it is put into.
QString quoted(const QString& typed)
{
    QString text = typed;
    text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return QLatin1Char('"') + text + QLatin1Char('"');
}

/// A caption on one line of the table.
QString one_line(const std::string& words)
{
    QString text = QString::fromStdString(words);
    text.replace(QLatin1Char('\n'), QStringLiteral(" ⏎ "));
    return text;
}

} // namespace

FindReplaceDialog::FindReplaceDialog(Controller& controller, QWidget* parent)
    : DialogFrame(parent), controller_(controller)
{
    setHeading(Glyph::Search, tr("Bul ve Değiştir"), tr("— çizimdeki yazılarda"));

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(20, 16, 20, 12);
    column->setSpacing(10);

    FieldSpec text   = field_of(FieldKind::Text);
    text.placeholder = tr("Aranacak yazı; \\n satır sonudur");
    find_            = new Field(text, body);
    column->addWidget(new FormRow(tr("Bul"), find_, body));

    text.placeholder = tr("Boş bırakılırsa bulunan sözcük silinir");
    replace_         = new Field(text, body);
    column->addWidget(new FormRow(tr("Değiştir"), replace_, body));

    // THE LAYERS THIS DRAWING HAS, and the whole drawing first.
    QStringList layers{tr("bütün çizim")};
    for (const core::Layer& l : controller_.document().layers())
        layers << QString::fromStdString(l.name);
    layer_ = new Field(combo_of(layers), body);
    layer_->setValue(layers.front());
    column->addWidget(new FormRow(tr("Katman"), layer_, body));

    auto* options = new QHBoxLayout();
    matchCase_    = new CheckBox(tr("Büyük/küçük harf ayrılsın"), body);
    wholeWord_    = new CheckBox(tr("Yalnız tam kelime"), body);
    options->addWidget(matchCase_);
    options->addWidget(wholeWord_);
    options->addStretch(1);
    column->addLayout(options);

    summary_ = new QLabel(tr("Önizle'ye basın: değişecek her yazı burada, önce ve sonra. "
                             "Tümünü Değiştir önizlemeden sonra açılır."),
                          body);
    summary_->setWordWrap(true);
    column->addWidget(summary_);

    table_ = new QTableWidget(body);
    table_->setObjectName(QStringLiteral("pickList"));
    table_->setColumnCount(Column::ColumnCount);
    table_->setHorizontalHeaderLabels({tr("Kimlik"), tr("Şimdi"), tr("Olacak")});
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kTableRow);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->horizontalHeader()->setFixedHeight(kHeaderRow);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(Column::Before, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(Column::After, QHeaderView::Stretch);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    column->addWidget(table_, 1);
    setBody(body);

    // A ROW PICKED IS THE CAPTION SHOWN: selected on the canvas, as SEÇ does.
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0) return;
        if (const QTableWidgetItem* key = table_->item(row, Column::Key); key != nullptr)
            controller_.runLine(QStringLiteral("SEÇ NESNE nesneler=%1").arg(key->text()),
                                command::Origin::Gui);
    });

    findAll_ = new Button(ButtonRole::Secondary, tr("Tümünü Bul"), Glyph::Search, this);
    footer()->addWidget(findAll_);
    connect(findAll_, &QPushButton::clicked, this, [this] { run(Step::Find); });

    // Enter previews: the one key a hand presses without looking never writes.
    preview_ = new Button(ButtonRole::Secondary, tr("Önizle"), Glyph::Eye, this);
    preview_->setDefault(true);
    footer()->addWidget(preview_);
    connect(preview_, &QPushButton::clicked, this, [this] { run(Step::Preview); });

    apply_ = new Button(ButtonRole::Primary, tr("Tümünü Değiştir"), Glyph::Check, this);
    apply_->setAutoDefault(false);
    apply_->setEnabled(false);
    footer()->addWidget(apply_);
    connect(apply_, &QPushButton::clicked, this, [this] { run(Step::Apply); });

    // ANY FIELD CHANGED IS A PREVIEW GONE STALE.
    for (const Field* f : {find_, replace_, layer_})
        connect(f, &Field::committed, this, [this](const QString&) { refreshApply(); });
    for (const CheckBox* box : {matchCase_, wholeWord_})
        connect(box, &QAbstractButton::toggled, this, [this](bool) { refreshApply(); });

    auto* close = new Button(ButtonRole::Ghost, tr("Kapat"), std::nullopt, this);
    footer()->addWidget(close);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    find_->setAccessibleName(tr("Aranacak yazı"));
    replace_->setAccessibleName(tr("Yerine yazılacak; boşsa bulunan silinir"));
    layer_->setAccessibleName(tr("Aranacak katman"));
    table_->setAccessibleName(tr("Önizleme: değişecek yazılar"));
    resize(kWindowWidth, kWindowTall);
    find_->beginEditing();
}

void FindReplaceDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

QString FindReplaceDialog::commandLine(Step step) const
{
    QString line = QStringLiteral("BULDEĞİŞTİR bul=") + quoted(find_->value());
    if (step != Step::Find) line += QStringLiteral(" degistir=") + quoted(replace_->value());
    if (const QString layer = layer_->value(); !layer.isEmpty() && layer != tr("bütün çizim"))
        line += QStringLiteral(" katman=") + quoted(layer);
    if (matchCase_->isChecked()) line += QStringLiteral(" buyuk_kucuk=evet");
    if (wholeWord_->isChecked()) line += QStringLiteral(" tam_kelime=evet");
    if (step == Step::Preview) line += QStringLiteral(" uygula=hayır");
    if (step == Step::Apply) line += QStringLiteral(" uygula=evet");
    return line;
}

void FindReplaceDialog::refreshApply()
{
    apply_->setEnabled(!previewed_.isEmpty() && commandLine(Step::Preview) == previewed_);
}

void FindReplaceDialog::run(Step step)
{
    if (find_->value().isEmpty()) {
        summary_->setText(tr("Aranacak bir yazı yazın."));
        find_->beginEditing();
        return;
    }
    // Pressed by a key while the fields moved on: show what they say now.
    if (step == Step::Apply && commandLine(Step::Preview) != previewed_) step = Step::Preview;

    auto result = controller_.runLineResult(commandLine(step), command::Origin::Gui);
    table_->setRowCount(0);
    table_->setColumnHidden(Column::After, step == Step::Find);
    // The columns say WHEN: what a caption says now and would say, or — once
    // written — what it said and says.
    switch (step) {
    case Step::Find: table_->setHorizontalHeaderLabels({tr("Kimlik"), tr("Yazı")}); break;
    case Step::Preview:
        table_->setHorizontalHeaderLabels({tr("Kimlik"), tr("Şimdi"), tr("Olacak")});
        break;
    case Step::Apply:
        table_->setHorizontalHeaderLabels({tr("Kimlik"), tr("Önce"), tr("Sonra")});
        break;
    }
    previewed_.clear();
    refreshApply();
    if (!result) {
        summary_->setText(QString::fromStdString(result.error().message));
        return;
    }
    const core::Json& report = result.value().report;
    const core::Json* rows   = report.find("satirlar");
    const auto count         = [&report](const char* key) {
        const core::Json* v = report.find(key);
        return v == nullptr ? std::int64_t{0} : v->as_int();
    };
    if (rows != nullptr)
        for (const core::Json& row : rows->as_array()) {
            const int at = table_->rowCount();
            table_->insertRow(at);
            const auto put = [this, at](int column, const QString& text) {
                auto* item = new QTableWidgetItem(text);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                table_->setItem(at, column, item);
            };
            const core::Json* key    = row.find("nesne");
            const core::Json* before = row.find("once");
            const core::Json* after  = row.find("sonra");
            put(Column::Key, QString::number(key == nullptr ? 0 : key->as_int()));
            put(Column::Before, before == nullptr ? QString() : one_line(before->as_string()));
            put(Column::After, after == nullptr ? QString() : one_line(after->as_string()));
        }
    table_->resizeColumnToContents(Column::Key);

    const std::int64_t texts   = count("yazi");
    const std::int64_t matches = count("eslesme");
    QString said;
    if (texts == 0)
        said = tr("Hiçbir yazıda bulunmadı.");
    else if (step == Step::Find)
        said = tr("%1 yazıda %2 eşleşme; bulunanlar çizimde seçildi.").arg(texts).arg(matches);
    else if (step == Step::Apply)
        said = tr("%1 yazıda %2 eşleşme değiştirildi. Geri almak için Ctrl+Z.")
                   .arg(texts)
                   .arg(matches);
    else
        said = tr("%1 yazıda %2 eşleşme değişecek. Uygulamak için Tümünü Değiştir'e basın.")
                   .arg(texts)
                   .arg(matches);
    if (const std::int64_t filled = count("atlanan"); filled > 0)
        said += tr(" Kalıptan doldurulan %1 yazı atlandı: kalıbı BAĞLA ya da ETİKET değiştirir.")
                    .arg(filled);
    if (const std::int64_t emptied = count("bos_kalacak"); emptied > 0)
        said += tr(" Boş kalacak %1 yazı atlandı: bir yazıyı SİL kaldırır.").arg(emptied);
    summary_->setText(said);

    // ONLY A PREVIEW THAT FOUND SOMETHING OPENS THE WAY TO WRITING IT.
    if (step == Step::Preview && texts > 0) previewed_ = commandLine(Step::Preview);
    refreshApply();
}

void FindReplaceDialog::runForProbe(const QString& find, const QString& replace, Step step)
{
    find_->setValue(find);
    replace_->setValue(replace);
    refreshApply();
    switch (step) {
    case Step::Find: findAll_->click(); break;
    case Step::Preview: preview_->click(); break;
    case Step::Apply: apply_->click(); break;
    }
}

int FindReplaceDialog::previewRows() const
{
    return table_->rowCount();
}

bool FindReplaceDialog::applyEnabled() const
{
    return apply_->isEnabled();
}

} // namespace kentos::app
