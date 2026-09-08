// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/schema_page.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/core/attribute.hpp"

#include <algorithm>

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

ColumnDialog::ColumnDialog(Controller& controller, QString existing, QString layerName,
                           QWidget* parent)
    : DialogFrame(parent), controller_(controller), existing_(std::move(existing)),
      layer_(std::move(layerName))
{
    const bool editing = !existing_.isEmpty();
    // THE SCOPE IS IN THE TITLE, because it is the one thing about a new column
    // that the form does not ask and cannot be changed later.
    const QString scope =
        layer_.isEmpty() ? tr("— proje geneli") : tr("— yalnız '%1' katmanı").arg(layer_);
    setHeading(Glyph::Table, editing ? tr("Sütunu Düzenle") : tr("Yeni Sütun"),
               editing ? QStringLiteral("— %1").arg(existing_) : scope);
    setFooterHeight(52);
    setModal(true);

    // THE FORM GRAMMAR of `form_örnek.png`: a label above its control, a
    // required mark on the label, and the control at the standard's regular
    // height. Every row here is a `FormRow`, so the dialog cannot invent a
    // fourth way of putting a name beside an input.
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(18, 16, 18, 16);
    column->setSpacing(12);

    const auto add = [this, column](const QString& label, Field* editor, bool required = false) {
        editor->setFixedHeight(static_cast<int>(ControlSize::Regular));
        auto* row = new FormRow(label, editor, this);
        row->setRequired(required);
        column->addWidget(row);
        return row;
    };

    FieldSpec idSpec;
    idSpec.placeholder = tr("ada_no");
    id_                = new Field(idSpec, this);
    add(tr("Kimlik"), id_, true);

    FieldSpec labelSpec;
    labelSpec.placeholder = tr("Panelde görünecek ad");
    label_                = new Field(labelSpec, this);
    add(tr("Ad"), label_);

    type_ = new Field(combo_of(typeWords()), this);
    add(tr("Tür"), type_, true);

    digits_    = new Field(number_of(0, core::kMaxScale), this);
    digitsRow_ = add(tr("Basamak"), digits_);

    FieldSpec catalogSpec;
    catalogSpec.placeholder = tr("Katalog kimliği");
    catalog_                = new Field(catalogSpec, this);
    catalogRow_             = add(tr("Katalog"), catalog_);

    required_ = new Field(field_of(FieldKind::Bool), this);
    add(tr("Zorunlu"), required_);

    FieldSpec aboutSpec;
    aboutSpec.placeholder = tr("Tek satırlık açıklama");
    about_                = new Field(aboutSpec, this);
    add(tr("Açıklama"), about_);
    column->addStretch(1);

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

    auto* cancel = new Button(ButtonRole::Secondary, tr("Vazgeç"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);

    auto* ok = new Button(ButtonRole::Primary, editing ? tr("Kaydet") : tr("Tanımla"),
                          editing ? Glyph::Save : Glyph::Plus, this);
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
    digitsRow_->setVisible(isDecimal);
    catalogRow_->setVisible(isCode);
}

QString ColumnDialog::line() const
{
    const QString id = existing_.isEmpty() ? id_->value().trimmed() : existing_;
    if (id.isEmpty()) return {};

    QString out = QStringLiteral("SÜTUN kimlik=%1").arg(quoted(id));
    if (existing_.isEmpty()) {
        out += QStringLiteral(" tur=%1").arg(type_->value());
        if (!layer_.isEmpty()) out += QStringLiteral(" katman=%1").arg(quoted(layer_));
    }

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

SchemaPage::SchemaPage(Controller& controller, QString layerName, QWidget* parent)
    : QWidget(parent), controller_(controller), layer_(std::move(layerName))
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(16, 14, 16, 14);
    column->setSpacing(10);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("quiet"));
    note_->setWordWrap(true);

    // SAID PLAINLY, because which page you are on decides what a new column
    // becomes and nothing else on screen says it.
    note_->setText(layer_.isEmpty()
                       ? tr("Proje sütunları: çizimdeki HER nesne bunları taşır. Yalnız bir "
                            "katmana ait bir alan için o katmanın özelliklerini açın.")
                       : tr("'%1' katmanının sütunları: yalnız bu katmandaki nesneler taşır. "
                            "Aşağıda proje sütunları da listelenir — onlar her nesnededir ve "
                            "buradan düzenlenmez.")
                             .arg(layer_));
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

    // SECONDARY, not primary, though it is the page's main action: this page
    // sits inside the settings window and inside the layer properties window,
    // and each of those already has its one primary — `Tamam` — in the footer.
    // The standard allows a screen exactly one.
    add_ = new Button(ButtonRole::Secondary, tr("Ekle…"), Glyph::Plus, this);
    connect(add_, &QPushButton::clicked, this, [this] { declareOrEdit(QString()); });
    buttons->addWidget(add_);

    edit_ = new Button(ButtonRole::Secondary, tr("Düzenle…"), Glyph::Pencil, this);
    connect(edit_, &QPushButton::clicked, this, [this] { declareOrEdit(currentId()); });
    buttons->addWidget(edit_);

    drop_ = new Button(ButtonRole::Danger, tr("Sil"), Glyph::Trash, this);
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

    // THIS PAGE'S OWN COLUMNS FIRST, then the project's as context when the page
    // belongs to a layer: an object on this layer carries both, and a list that
    // showed only half of it would answer "what will I be asked for" wrongly.
    std::vector<core::AttrId> shownColumns;
    for (std::size_t i = 0; i < schema.columns(); ++i) {
        const core::AttrSpec& spec = schema.column(static_cast<core::AttrId>(i))->spec();
        const bool mine =
            layer_.isEmpty() ? spec.layer.empty() : QString::fromStdString(spec.layer) == layer_;
        if (mine) shownColumns.push_back(static_cast<core::AttrId>(i));
    }
    if (!layer_.isEmpty())
        for (std::size_t i = 0; i < schema.columns(); ++i)
            if (schema.column(static_cast<core::AttrId>(i))->spec().layer.empty())
                shownColumns.push_back(static_cast<core::AttrId>(i));

    table_->setRowCount(static_cast<int>(shownColumns.size()));
    for (std::size_t i = 0; i < shownColumns.size(); ++i) {
        const core::AttrSpec& spec = schema.column(shownColumns[i])->spec();
        const auto row             = static_cast<int>(i);
        const bool borrowed        = !layer_.isEmpty() && spec.layer.empty();

        // What only some types carry, in one column rather than three mostly
        // empty ones: the digits of a decimal, the catalogue of a code.
        QString detail;
        if (spec.type == core::AttrType::Decimal)
            detail = tr("%1 basamak").arg(spec.scale);
        else if (spec.type == core::AttrType::CodeRef)
            detail = QString::fromStdString(spec.catalog);

        const auto cell = [this, row, borrowed](int at, const QString& text, bool numeric) {
            auto* item = new QTableWidgetItem(text);
            // A project column shown on a layer page is CONTEXT: it is listed so
            // the reader knows what the objects here carry, and it is not
            // selectable because editing it belongs on the project's own page.
            item->setFlags(borrowed ? Qt::ItemIsEnabled
                                    : (Qt::ItemIsEnabled | Qt::ItemIsSelectable));
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
        const QString about = QString::fromStdString(spec.summary_tr);
        cell(Column::About,
             borrowed ? (about.isEmpty() ? tr("proje sütunu") : tr("proje sütunu · %1").arg(about))
                      : about,
             false);
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
    ColumnDialog dialog(controller_, existing, layer_, this);
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
