// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/schema_page.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/core/attribute.hpp"

#include <algorithm>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

/// The columns of the schema table, in the order they help: what it is called in
/// a command, what it is called on screen, what it holds, and the three
/// qualifiers that only some types carry.
enum Column : int { Id = 0, Label, Type, Detail, Required, About, ColumnCount };

/// The attribute table's own metrics, because this IS that table at page size —
/// the same grid the pick chooser wears, for the same reason: a program with two
/// kinds of data table looks like two programs.
constexpr int kHeaderRow = 30;
constexpr int kTableRow  = 28;

constexpr int kIdFloor    = 150;
constexpr int kTypeFloor  = 120;
constexpr int kCellMargin = 24;

/// The type words `SÜTUN` takes, in the order a person meets them.
///
/// LISTED ONCE, HERE, and every one of them round-trips through
/// `core::attr_type_from_name` — the gate checks that, because a word in this
/// combo that the command does not know is a row the user can fill in and not
/// send.
const QStringList& typeWords()
{
    static const QStringList words{
        QStringLiteral("metin"),   QStringLiteral("tam_sayi"),   QStringLiteral("ondalik"),
        QStringLiteral("uzunluk"), QStringLiteral("evet_hayir"), QStringLiteral("tarih"),
        QStringLiteral("kod"),
    };
    return words;
}

/// The word for a stored type, for the table's `Tür` column.
QString wordFor(core::AttrType type)
{
    for (const QString& word : typeWords())
        if (core::attr_type_from_name(word.toStdString()) == type) return word;
    return QString::fromUtf8(core::attr_type_name(type));
}

/// Quotes a value for a command line: backslash and quote, and nothing else.
QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

} // namespace

// =============================================================================
// ColumnDialog
// =============================================================================

ColumnDialog::ColumnDialog(Controller& controller, QString existing, QWidget* parent)
    : DialogFrame(parent), controller_(controller), existing_(std::move(existing))
{
    const bool editing = !existing_.isEmpty();
    setHeading(Glyph::Table, editing ? tr("Sütunu Düzenle") : tr("Yeni Sütun"),
               editing ? QStringLiteral("— %1").arg(existing_) : QString());
    setFooterHeight(52);
    setModal(true);

    auto* body = new QWidget(this);
    auto* form = new QFormLayout(body);
    form->setContentsMargins(18, 16, 18, 16);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const auto add = [this, form](const QString& label, Field* editor) {
        auto* text = new QLabel(label, this);
        editor->setFixedHeight(26);
        form->addRow(text, editor);
        return text;
    };

    FieldSpec idSpec;
    idSpec.placeholder = tr("ada_no");
    id_                = new Field(idSpec, this);
    add(tr("Kimlik"), id_);

    FieldSpec labelSpec;
    labelSpec.placeholder = tr("Panelde görünecek ad");
    label_                = new Field(labelSpec, this);
    add(tr("Ad"), label_);

    type_ = new Field(combo_of(typeWords()), this);
    add(tr("Tür"), type_);

    digits_      = new Field(number_of(0, core::kMaxScale), this);
    digitsLabel_ = add(tr("Basamak"), digits_);

    FieldSpec catalogSpec;
    catalogSpec.placeholder = tr("Katalog kimliği");
    catalog_                = new Field(catalogSpec, this);
    catalogLabel_           = add(tr("Katalog"), catalog_);

    required_ = new Field(field_of(FieldKind::Bool), this);
    add(tr("Zorunlu"), required_);

    FieldSpec aboutSpec;
    aboutSpec.placeholder = tr("Tek satırlık açıklama");
    about_                = new Field(aboutSpec, this);
    add(tr("Açıklama"), about_);

    setBody(body);

    // ---- what the dialog opens with ----
    const core::AttrTable& table = controller_.document().attributes();
    if (editing) {
        const core::AttrId at = table.find(existing_.toStdString());
        if (at != core::kNoAttr) {
            const core::AttrSpec& spec = table.column(at)->spec();
            id_->setValue(QString::fromStdString(spec.id));
            label_->setValue(QString::fromStdString(spec.name_tr));
            type_->setValue(wordFor(spec.type));
            digits_->setValue(QString::number(spec.scale));
            catalog_->setValue(QString::fromStdString(spec.catalog));
            required_->setValue(spec.required ? tr("evet") : tr("hayır"));
            about_->setValue(QString::fromStdString(spec.summary_tr));
        }

        // THE ID AND THE TYPE ARE NOT EDITABLE, and the dialog says so by being
        // unable to change them rather than by refusing later: the id is what
        // every symbol binding and every rule names, and the type is what the
        // stored integers MEAN. `AttrColumn::amend` refuses both, and a form that
        // let a user type into a box that will be ignored is a form that lies.
        id_->setEnabled(false);
        type_->setEnabled(false);
    } else {
        required_->setValue(tr("hayır"));
        digits_->setValue(QStringLiteral("2"));
        type_->setValue(QStringLiteral("metin"));
    }

    connect(type_, &Field::committed, this, [this](const QString&) { syncTypeRows(); });
    syncTypeRows();

    auto* cancel = new QPushButton(tr("Vazgeç"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);

    auto* ok = new QPushButton(editing ? tr("Kaydet") : tr("Tanımla"), this);
    ok->setObjectName(QStringLiteral("primary"));
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    footer()->addWidget(ok);

    resize(460, 360);
    if (!editing) id_->beginEditing();
}

void ColumnDialog::syncTypeRows()
{
    const QString word   = type_->value();
    const bool isDecimal = word == QStringLiteral("ondalik");
    const bool isCode    = word == QStringLiteral("kod");

    // A ROW THAT DOES NOTHING IS NOT SHOWN. `basamak` on a text column and
    // `katalog` on an integer are boxes whose value is discarded, and a form full
    // of those teaches the user that the form does not mean what it says.
    digitsLabel_->setVisible(isDecimal);
    digits_->setVisible(isDecimal);
    catalogLabel_->setVisible(isCode);
    catalog_->setVisible(isCode);
}

QString ColumnDialog::line() const
{
    const QString id = existing_.isEmpty() ? id_->value().trimmed() : existing_;
    if (id.isEmpty()) return {};

    QString out = QStringLiteral("SÜTUN kimlik=%1").arg(quoted(id));
    if (existing_.isEmpty()) out += QStringLiteral(" tur=%1").arg(type_->value());

    if (!label_->value().trimmed().isEmpty())
        out += QStringLiteral(" ad=%1").arg(quoted(label_->value().trimmed()));
    if (!about_->value().trimmed().isEmpty())
        out += QStringLiteral(" aciklama=%1").arg(quoted(about_->value().trimmed()));

    out += QStringLiteral(" zorunlu=%1").arg(required_->value());

    if (type_->value() == QStringLiteral("ondalik"))
        out += QStringLiteral(" basamak=%1").arg(digits_->value());
    if (type_->value() == QStringLiteral("kod") && !catalog_->value().trimmed().isEmpty())
        out += QStringLiteral(" katalog=%1").arg(quoted(catalog_->value().trimmed()));

    return out;
}

void ColumnDialog::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    DialogFrame::applyTheme(mode);
    for (Field* editor : findChildren<Field*>())
        editor->applyTheme(mode);
}

// =============================================================================
// SchemaPage
// =============================================================================

SchemaPage::SchemaPage(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(16, 14, 16, 14);
    column->setSpacing(10);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("quiet"));
    note_->setWordWrap(true);

    // SAID PLAINLY, because the window it sits in is a LAYER's. The schema
    // belongs to the document (model.md R27) and a user who declared `taks` here
    // will find it offered on a road object too; better to read that than to
    // discover it.
    note_->setText(tr("Sütunlar çizimin tamamına tanımlanır: burada tanımladığınız bir sütun "
                      "her katmandaki nesnede görünür. Değerler nesne nesne girilir."));
    column->addWidget(note_);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("pickList"));
    table_->setColumnCount(Column::ColumnCount);
    table_->setHorizontalHeaderLabels(
        {tr("Kimlik"), tr("Ad"), tr("Tür"), tr("Ayrıntı"), tr("Zorunlu"), tr("Açıklama")});
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(kTableRow);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->horizontalHeader()->setFixedHeight(kHeaderRow);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setShowGrid(true);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    column->addWidget(table_, 1);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);

    add_ = new QPushButton(tr("Ekle…"), this);
    add_->setObjectName(QStringLiteral("primary"));
    connect(add_, &QPushButton::clicked, this, [this] { declareOrEdit(QString()); });
    buttons->addWidget(add_);

    edit_ = new QPushButton(tr("Düzenle…"), this);
    connect(edit_, &QPushButton::clicked, this, [this] { declareOrEdit(currentId()); });
    buttons->addWidget(edit_);

    drop_ = new QPushButton(tr("Sil"), this);
    drop_->setObjectName(QStringLiteral("danger"));
    connect(drop_, &QPushButton::clicked, this, &SchemaPage::dropSelected);
    buttons->addWidget(drop_);

    buttons->addStretch(1);
    column->addLayout(buttons);

    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { declareOrEdit(currentId()); });

    refresh();
}

void SchemaPage::refresh()
{
    const core::AttrTable& schema = controller_.document().attributes();

    table_->setRowCount(static_cast<int>(schema.columns()));
    for (std::size_t i = 0; i < schema.columns(); ++i) {
        const core::AttrSpec& spec = schema.column(static_cast<core::AttrId>(i))->spec();
        const auto row             = static_cast<int>(i);

        // What only some types carry, in one column rather than three mostly
        // empty ones: the digits of a decimal, the catalogue of a code.
        QString detail;
        if (spec.type == core::AttrType::Decimal)
            detail = tr("%1 basamak").arg(spec.scale);
        else if (spec.type == core::AttrType::CodeRef)
            detail = QString::fromStdString(spec.catalog);

        const auto cell = [this, row](int at, const QString& text, bool numeric) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setTextAlignment(numeric ? (Qt::AlignRight | Qt::AlignVCenter)
                                           : (Qt::AlignLeft | Qt::AlignVCenter));
            if (numeric) {
                QFont face(QStringLiteral("IBM Plex Mono"));
                face.setStyleHint(QFont::Monospace);
                face.setPixelSize(12);
                item->setFont(face);
            }
            table_->setItem(row, at, item);
        };

        cell(Column::Id, QString::fromStdString(spec.id), true);
        cell(Column::Label, QString::fromStdString(spec.name_tr), false);
        cell(Column::Type, wordFor(spec.type), false);
        cell(Column::Detail, detail, false);
        cell(Column::Required, spec.required ? tr("evet") : QString(), false);
        cell(Column::About, QString::fromStdString(spec.summary_tr), false);
    }

    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setSectionResizeMode(Column::About, QHeaderView::Stretch);

    const auto atLeast = [this](int at, int floor) {
        table_->setColumnWidth(at, std::max(table_->columnWidth(at) + kCellMargin, floor));
    };
    atLeast(Column::Id, kIdFloor);
    atLeast(Column::Type, kTypeFloor);

    if (table_->rowCount() > 0 && table_->currentRow() < 0) table_->setCurrentCell(0, Column::Id);

    const bool any = table_->rowCount() > 0;
    edit_->setEnabled(any);
    drop_->setEnabled(any);
}

QString SchemaPage::currentId() const
{
    const int row = table_->currentRow();
    if (row < 0 || table_->item(row, Column::Id) == nullptr) return {};
    return table_->item(row, Column::Id)->text();
}

void SchemaPage::declareOrEdit(const QString& existing)
{
    ColumnDialog dialog(controller_, existing, this);
    dialog.applyTheme(theme_);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString line = dialog.line();
    if (line.isEmpty()) return;

    controller_.runLine(line, command::Origin::Gui);
    refresh();
}

void SchemaPage::dropSelected()
{
    const QString id = currentId();
    if (id.isEmpty()) return;

    // ASKED, AND NAMED. Dropping a column destroys every value in it and there is
    // no undo record that could put them back — schema changes are not undoable
    // (model.md R27). It is the one edit in this program that loses entered data
    // silently if nobody stops to read.
    const auto answer =
        QMessageBox::warning(this, tr("Sütunu sil"),
                             tr("'%1' sütunu ve içindeki bütün değerler silinecek.\n\n"
                                "Bu geri alınamaz: şema değişiklikleri GERİAL ile geri gelmez.")
                                 .arg(id),
                             QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) return;

    controller_.runLine(QStringLiteral("SÜTUN kimlik=%1 sil=evet").arg(quoted(id)),
                        command::Origin::Gui);
    refresh();
}

QStringList SchemaPage::probeRows() const
{
    QStringList out;
    for (int row = 0; row < table_->rowCount(); ++row) {
        QStringList cells;
        for (int at = 0; at < Column::ColumnCount; ++at)
            cells << (table_->item(row, at) != nullptr ? table_->item(row, at)->text() : QString());
        out << cells.join(QStringLiteral(" · "));
    }
    return out;
}

bool SchemaPage::probeAction(const QString& action, int row, const QString& line)
{
    if (row >= 0 && row < table_->rowCount()) table_->setCurrentCell(row, Column::Id);

    if (action == QStringLiteral("sil")) {
        const QString id = currentId();
        if (id.isEmpty()) return false;
        controller_.runLine(QStringLiteral("SÜTUN kimlik=%1 sil=evet").arg(quoted(id)),
                            command::Origin::Gui);
        refresh();
        return true;
    }
    if (action == QStringLiteral("satir")) {
        // The line the dialog WOULD build, sent without opening it: the probe
        // drives the same road a person does and skips only the modal loop.
        if (line.isEmpty()) return false;
        controller_.runLine(line, command::Origin::Gui);
        refresh();
        return true;
    }
    return false;
}

void SchemaPage::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    update();
}

} // namespace kentos::app
