// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/field_calculator_dialog.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/expression_edit.hpp"
#include "piricad/app/fields.hpp"
#include "piricad/app/widgets.hpp"
#include "piricad/command/expression.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cstdint>

namespace piricad::app {
namespace {

enum Column : std::uint8_t { Key = 0, Before, After, ColumnCount };

constexpr int kHeaderRow    = 30;
constexpr int kTableRow     = 28;
constexpr int kWindowWidth  = 860;
constexpr int kWindowTall   = 640;
constexpr int kListsWidth   = 280;
constexpr int kListsMinTall = 120;

/// A field's text as the lexer reads it inside quotes: a quote or a backslash cannot end the
/// argument it is put into.
QString quoted(const QString& typed)
{
    QString text = typed;
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + text + QLatin1Char('"');
}

/// What one list entry carries besides its words: the text a double-click puts in the expression.
constexpr int kInsertRole = Qt::UserRole;

} // namespace

FieldCalculatorDialog::FieldCalculatorDialog(Controller& controller, QString layerName,
                                             QString filter, QWidget* parent)
    : DialogFrame(parent), controller_(controller), layer_(std::move(layerName))
{
    setHeading(Glyph::Function, tr("Alan Hesaplayıcı"),
               layer_.isEmpty() ? tr("— bütün çizim") : tr("— %1").arg(layer_));

    auto* body = new QWidget(this);
    auto* wide = new QHBoxLayout(body);
    wide->setContentsMargins(20, 16, 20, 12);
    wide->setSpacing(16);

    // ---- the left side: what to write, how, and over which rows ----
    auto* left = new QVBoxLayout();
    left->setSpacing(10);

    // THE COLUMNS THIS SCOPE CARRIES, the same rule the table uses.
    columns_ = new QListWidget(body);
    columns_->setObjectName(QStringLiteral("pickList"));
    columns_->setFrameShape(QFrame::NoFrame);
    columns_->setMinimumHeight(kListsMinTall);
    QStringList targets;
    const core::AttrTable& table = controller_.document().attributes();
    for (std::size_t c = 0; c < table.columns(); ++c) {
        const core::AttrColumn* held = table.column(static_cast<core::AttrId>(c));
        if (held == nullptr) continue;
        if (!layer_.isEmpty() && !core::attr_applies_to(held->spec(), layer_.toStdString()))
            continue;
        targets << QString::fromStdString(held->spec().id);

        auto* item = new QListWidgetItem();
        item->setData(kInsertRole, QLatin1Char('"') + QString::fromStdString(held->spec().id) +
                                       QLatin1Char('"'));
        // THE ID FIRST, because it is what the expression says; the name the table shows after it.
        const QString id = QString::fromStdString(held->spec().id);
        item->setText(held->spec().name_tr.empty() || held->spec().name_tr == held->spec().id
                          ? id
                          : tr("%1 — %2").arg(id, QString::fromStdString(held->spec().name_tr)));
        item->setToolTip(QString::fromStdString(held->spec().summary_tr));
        columns_->addItem(item);
    }
    column_ = new Field(combo_of(targets), body);
    if (!targets.isEmpty()) column_->setValue(targets.front());
    left->addWidget(new FormRow(tr("Yazılacak sütun"), column_, body));

    expression_    = new ExpressionEdit(body);
    expressionRow_ = new FormRow(tr("İfade"), expression_, body);
    expressionRow_->setHelp(tr("Sütunlar \"çift\", metinler 'tek' tırnakta; sağdaki listelerden "
                               "çift tıklayarak eklenir."));
    left->addWidget(expressionRow_);

    FieldSpec text   = field_of(FieldKind::Text);
    text.placeholder = tr("Boşsa bütün satırlar; tablonun süzme ifadesiyle aynı dil");
    filter_          = new Field(text, body);
    filter_->setValue(filter);
    left->addWidget(new FormRow(tr("Yalnız şu satırlar"), filter_, body));

    scope_ = new Field(combo_of({tr("Katmanın bütün satırları"), tr("Seçili nesneler")}), body);
    scope_->setValue(layer_.isEmpty() ? tr("Seçili nesneler") : tr("Katmanın bütün satırları"));
    left->addWidget(new FormRow(tr("Kapsam"), scope_, body));

    summary_ = new QLabel(tr("Önizle'ye basın: değişecek satırlardan örnekler burada, önce ve "
                             "sonra. Uygula önizlemeden sonra açılır."),
                          body);
    summary_->setWordWrap(true);
    left->addWidget(summary_);

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
    left->addWidget(table_, 1);
    wide->addLayout(left, 1);

    // ---- the right side: the columns to name and the functions to call ----
    auto* right = new QVBoxLayout();
    right->setSpacing(10);
    right->addWidget(new FormSection(tr("Sütunlar"), QString(), body));
    // The words every row answers (`$alan`, `$x`, …) are part of the language, not of a schema.
    for (const command::FunctionHelp& word : command::expression_pseudo_columns()) {
        auto* item = new QListWidgetItem(QString::fromStdString(word.usage));
        item->setData(kInsertRole, QString::fromStdString(word.usage));
        item->setToolTip(QString::fromStdString(word.help));
        columns_->addItem(item);
    }
    right->addWidget(columns_, 1);

    right->addWidget(new FormSection(tr("İşlevler"), QString(), body));
    functions_ = new QListWidget(body);
    functions_->setObjectName(QStringLiteral("pickList"));
    functions_->setFrameShape(QFrame::NoFrame);
    functions_->setMinimumHeight(kListsMinTall);
    for (const command::FunctionHelp& f : command::expression_functions()) {
        auto* item = new QListWidgetItem(QString::fromStdString(f.usage));
        item->setData(kInsertRole, QString::fromStdString(f.usage));
        item->setToolTip(QString::fromStdString(f.help));
        functions_->addItem(item);
    }
    right->addWidget(functions_, 2);
    wide->addLayout(right);
    for (QWidget* w : {static_cast<QWidget*>(columns_), static_cast<QWidget*>(functions_)})
        w->setMaximumWidth(kListsWidth);
    setBody(body);

    connect(columns_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { insertText(item->data(kInsertRole).toString()); });
    connect(functions_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { insertText(item->data(kInsertRole).toString()); });

    // A ROW PICKED IS THE OBJECT SHOWN: selected on the canvas, as SEÇ does.
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0) return;
        if (const QTableWidgetItem* key = table_->item(row, Column::Key); key != nullptr)
            controller_.runLine(QStringLiteral("SEÇ NESNE nesneler=%1").arg(key->text()),
                                command::Origin::Gui);
    });

    // Enter in the bar previews: the one key a hand presses without looking never writes.
    connect(expression_, &ExpressionEdit::applied, this, [this] { run(Step::Preview); });

    preview_ = new Button(ButtonRole::Secondary, tr("Önizle"), Glyph::Eye, this);
    preview_->setDefault(true);
    footer()->addWidget(preview_);
    connect(preview_, &QPushButton::clicked, this, [this] { run(Step::Preview); });

    apply_ = new Button(ButtonRole::Primary, tr("Uygula"), Glyph::Check, this);
    apply_->setAutoDefault(false);
    apply_->setEnabled(false);
    footer()->addWidget(apply_);
    connect(apply_, &QPushButton::clicked, this, [this] { run(Step::Apply); });

    // ANY FIELD CHANGED IS A PREVIEW GONE STALE.
    for (const Field* f : {column_, scope_, filter_})
        connect(f, &Field::committed, this, [this](const QString&) { refreshApply(); });
    connect(expression_, &QPlainTextEdit::textChanged, this, [this] { refreshApply(); });

    auto* close = new Button(ButtonRole::Ghost, tr("Kapat"), std::nullopt, this);
    footer()->addWidget(close);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    column_->setAccessibleName(tr("Hesabın yazılacağı sütun"));
    expression_->setAccessibleName(tr("Hesaplanacak ifade"));
    filter_->setAccessibleName(tr("Yalnız bu ifadenin doğru çıktığı satırlar"));
    scope_->setAccessibleName(tr("Hesabın kapsamı"));
    columns_->setAccessibleName(tr("Sütunlar; çift tıklayınca ifadeye eklenir"));
    functions_->setAccessibleName(tr("İşlevler; çift tıklayınca ifadeye eklenir"));
    table_->setAccessibleName(tr("Önizleme: değişecek satırlardan örnekler"));
    resize(kWindowWidth, kWindowTall);
    expression_->setFocus();
}

void FieldCalculatorDialog::applyTheme(ThemeMode mode)
{
    DialogFrame::applyTheme(mode);
}

void FieldCalculatorDialog::setFilter(const QString& filter)
{
    if (filter_->value() == filter) return;
    filter_->setValue(filter);
    refreshApply();
}

void FieldCalculatorDialog::insertText(const QString& text)
{
    expression_->insertPlainText(text);
    expression_->setFocus();
}

QString FieldCalculatorDialog::commandLine(Step step) const
{
    QString line = QStringLiteral("ÖZNİTELİKHESAPLA ad=") + quoted(column_->value()) +
                   QStringLiteral(" ifade=") + quoted(expression_->expression());
    if (const QString f = filter_->value().trimmed(); !f.isEmpty())
        line += QStringLiteral(" filtre=") + quoted(f);
    // "The layer's rows" names the layer; "the selection" leaves it out, which is how the command
    // is told to use the selection.
    if (scope_->value() == tr("Katmanın bütün satırları") && !layer_.isEmpty())
        line += QStringLiteral(" katman=") + quoted(layer_);
    if (step == Step::Preview) line += QStringLiteral(" onizle=evet");
    return line;
}

void FieldCalculatorDialog::refreshApply()
{
    apply_->setEnabled(!previewed_.isEmpty() && commandLine(Step::Preview) == previewed_);
}

void FieldCalculatorDialog::run(Step step)
{
    if (expression_->expression().isEmpty()) {
        expressionRow_->setError(tr("Bir ifade yazın."));
        expression_->setFocus();
        return;
    }
    if (column_->value().isEmpty()) {
        summary_->setText(tr("Yazılacak bir sütun seçin; yoksa önce SÜTUN ile tanımlayın."));
        return;
    }
    // Pressed by a key while the fields moved on: show what they say now.
    if (step == Step::Apply && commandLine(Step::Preview) != previewed_) step = Step::Preview;

    auto result = controller_.runLineResult(commandLine(step), command::Origin::Gui);
    table_->setRowCount(0);
    table_->setHorizontalHeaderLabels(step == Step::Apply
                                          ? QStringList{tr("Kimlik"), tr("Önce"), tr("Sonra")}
                                          : QStringList{tr("Kimlik"), tr("Şimdi"), tr("Olacak")});
    previewed_.clear();
    refreshApply();
    if (!result) {
        // THE COMMAND'S OWN WORDS, under the bar: they name the row and the place in the text.
        expressionRow_->setError(QString::fromStdString(result.error().message));
        summary_->setText(tr("Hesaplanamadı; hiçbir şey yazılmadı."));
        return;
    }
    expressionRow_->setHelp(tr("Sütunlar \"çift\", metinler 'tek' tırnakta; sağdaki listelerden "
                               "çift tıklayarak eklenir."));

    const core::Json& report = result.value().report;
    const auto count         = [&report](const char* key) {
        const core::Json* v = report.find(key);
        return v == nullptr ? std::int64_t{0} : v->as_int();
    };
    if (const core::Json* rows = report.find("ornekler"); rows != nullptr)
        for (const core::Json& row : rows->as_array()) {
            const int at = table_->rowCount();
            table_->insertRow(at);
            const auto put = [this, at](int column, const QString& text) {
                auto* item = new QTableWidgetItem(text);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                table_->setItem(at, column, item);
            };
            const core::Json* key    = row.find("nesne");
            const core::Json* before = row.find("eski");
            const core::Json* after  = row.find("yeni");
            put(Column::Key, QString::number(key == nullptr ? 0 : key->as_int()));
            put(Column::Before,
                before == nullptr ? QString() : QString::fromStdString(before->as_string()));
            put(Column::After,
                after == nullptr ? QString() : QString::fromStdString(after->as_string()));
        }
    table_->resizeColumnToContents(Column::Key);

    const std::int64_t rows      = count("satir");
    const std::int64_t changed   = count("degisen");
    const std::int64_t same      = count("ayni");
    const std::int64_t emptied   = count("bosaltilan");
    const std::int64_t unmatched = count("suzgece_uymayan");
    QString said;
    if (rows == 0)
        said = tr("Hesaplanacak satır yok: kapsamı ya da süzgeci değiştirin.");
    else if (step == Step::Apply)
        said =
            tr("%1 satır değişti, %2 aynı kaldı. Geri almak için Ctrl+Z.").arg(changed).arg(same);
    else
        said = tr("%1 satır değişecek, %2 aynı kalacak. Uygulamak için Uygula'ya basın.")
                   .arg(changed)
                   .arg(same);
    if (emptied > 0) said += tr(" %1 hücre boşalacak (ifade boş çıktı).").arg(emptied);
    if (unmatched > 0) said += tr(" Süzgece uymayan %1 satıra dokunulmadı.").arg(unmatched);
    summary_->setText(said);

    // ONLY A PREVIEW THAT FOUND SOMETHING OPENS THE WAY TO WRITING IT.
    if (step == Step::Preview && changed > 0) previewed_ = commandLine(Step::Preview);
    refreshApply();
    if (step == Step::Apply) emit applied();
}

void FieldCalculatorDialog::runForProbe(const QString& column, const QString& expression, Step step)
{
    column_->setValue(column);
    expression_->setExpression(expression);
    refreshApply();
    switch (step) {
    case Step::Preview: preview_->click(); break;
    case Step::Apply: apply_->click(); break;
    }
}

int FieldCalculatorDialog::previewRows() const
{
    return table_->rowCount();
}

bool FieldCalculatorDialog::applyEnabled() const
{
    return apply_->isEnabled();
}

QString FieldCalculatorDialog::summaryText() const
{
    return summary_->text();
}

int FieldCalculatorDialog::columnChoices() const
{
    return columns_->count();
}

int FieldCalculatorDialog::functionCount() const
{
    return functions_->count();
}

QString FieldCalculatorDialog::insertFunctionForProbe(int row)
{
    if (row < 0 || row >= functions_->count()) return {};
    emit functions_->itemDoubleClicked(functions_->item(row));
    return expression_->expression();
}

} // namespace piricad::app
