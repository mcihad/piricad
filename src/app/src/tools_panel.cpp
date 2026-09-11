// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/tools_panel.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/command/journal.hpp"
#include "kentos_cad/core/area_edit.hpp"
#include "kentos_cad/processing/registry.hpp"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

constexpr int kToolRole = Qt::UserRole + 1; ///< the tool's command id on a tree row

const Tokens& tokensOf(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? darkTokens() : lightTokens();
}

QString utf8(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/// A value as the command line takes it: quoted when it has a space, so
/// `bicim="L = {}"` survives the parser exactly as typed.
QString quoted(const QString& value)
{
    if (value.contains(QLatin1Char(' ')) || value.isEmpty())
        return QLatin1Char('"') + value + QLatin1Char('"');
    return value;
}

/// A document millimetre as the command line's metre: `485320.150`.
QString metres(core::Mm mm)
{
    return QString::number(static_cast<double>(mm) / 1000.0, 'f', 3);
}

/// A journalled value as the text a field shows and the command line takes.
QString value_text(const command::Value& v)
{
    using command::Value;
    switch (v.kind()) {
    case Value::Kind::Bool: return v.as_bool() ? QStringLiteral("evet") : QStringLiteral("hayır");
    case Value::Kind::Int: return QString::number(v.as_int());
    case Value::Kind::Number:
        return QString::number(v.as_number(), 'f', 3)
            .remove(QRegularExpression(QStringLiteral("\\.?0+$")));
    case Value::Kind::Text: return utf8(v.as_text());
    case Value::Kind::Point: {
        const core::Point2 p = v.as_point();
        return metres(p.x) + QLatin1Char(',') + metres(p.y);
    }
    default: return QString();
    }
}

/// The glyph a spec's icon NAME means. The processing module is Qt-free and
/// names its mark in words; this is the one place the words become a picture,
/// and a word it does not know gets the generic tool mark rather than nothing.
Glyph glyph_named(const std::string& name)
{
    if (name == "cetvel") return Glyph::Ruler;
    if (name == "koordinat") return Glyph::Coordinate;
    if (name == "yazi") return Glyph::Text;
    if (name == "alan") return Glyph::Polygon;
    if (name == "cizgi") return Glyph::Line;
    if (name == "nokta") return Glyph::Point;
    if (name == "sigma") return Glyph::Sigma;
    return Glyph::Function;
}

/// The field a parameter is edited with. Its kind decides the editor and its
/// choices, range and default decide what the editor offers.
FieldSpec field_for(const processing::ToolParam& p)
{
    using command::ParamKind;
    FieldSpec spec;
    switch (p.kind) {
    case ParamKind::Integer:
        spec = p.bounded ? number_of(p.low, p.high) : field_of(FieldKind::Number);
        break;
    case ParamKind::Number:
        spec          = field_of(FieldKind::Decimal);
        spec.decimals = 3;
        break;
    case ParamKind::Bool: spec = field_of(FieldKind::Bool); break;
    case ParamKind::Text:
        if (!p.choices.empty()) {
            QStringList words;
            for (const std::string& c : p.choices)
                words << utf8(c);
            spec = combo_of(words);
        } else {
            spec = field_of(FieldKind::Text);
        }
        break;
    default: spec = field_of(FieldKind::Text); break;
    }
    // The placeholder says what the box WANTS, never what it holds: a default
    // is put in as the value, so it reads as a value, and the help line under
    // the row already says what the box is for.
    if (p.kind == ParamKind::Point) spec.placeholder = QStringLiteral("x,y");
    return spec;
}

} // namespace

// =============================================================================
// ToolCard
// =============================================================================

ToolCard::ToolCard(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("toolCard"));
    setAutoFillBackground(false);

    auto* stack = new QVBoxLayout(this);
    stack->setContentsMargins(0, 0, 4, 0);
    stack->setSpacing(6);

    title_ = new QLabel(tr("Bir araç seçin"), this);
    title_->setObjectName(QStringLiteral("toolTitle"));
    QFont bold = title_->font();
    bold.setBold(true);
    title_->setFont(bold);
    title_->setWordWrap(true);
    stack->addWidget(title_);

    summary_ = new QLabel(tr("Ağaçtan bir araç seçin; kartı burada açılır."), this);
    summary_->setWordWrap(true);
    stack->addWidget(summary_);

    chips_       = new QWidget(this);
    chipsLayout_ = new QHBoxLayout(chips_);
    chipsLayout_->setContentsMargins(0, 0, 0, 0);
    chipsLayout_->setSpacing(4);
    chipsLayout_->addStretch(1);
    stack->addWidget(chips_);

    stack->addWidget(new FormSection(tr("Kapsam"), QString(), this));
    scope_ = new Segment(this);
    scope_->addOption(tr("Seçili"), tr("Seçili nesneler; seçim boşsa araç tuvalden seçtirir"));
    scope_->addOption(tr("Görünüm"), tr("Görünümde bulunan nesneler"));
    scope_->addOption(tr("Proje"), tr("Çizimdeki bütün nesneler"));
    scope_->setAccessibleName(tr("Kapsam"));
    connect(scope_, &Segment::currentChanged, this, [this](int) { rebuildPreview(); });
    stack->addWidget(scope_);
    scopeHint_ = new QLabel(this);
    scopeHint_->setWordWrap(true);
    stack->addWidget(scopeHint_);

    stack->addWidget(new FormSection(tr("Parametreler"), QString(), this));
    params_       = new QWidget(this);
    paramsLayout_ = new QVBoxLayout(params_);
    paramsLayout_->setContentsMargins(0, 0, 0, 0);
    paramsLayout_->setSpacing(6);
    stack->addWidget(params_);

    stack->addWidget(new FormSection(tr("Çıktı"), QString(), this));
    FieldSpec layerSpec   = field_of(FieldKind::Text);
    layerSpec.placeholder = tr("etkin katman");
    layer_                = new Field(layerSpec, this);
    layer_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    layer_->setAccessibleName(tr("Çıktı katmanı"));
    connect(layer_, &Field::committed, this, [this](const QString&) { rebuildPreview(); });
    auto* layerRow = new FormRow(tr("Katman"), layer_, this);
    layerRow->setHelp(tr("Boş: etkin katman. Bir ad yazarsanız yoksa oluşturulur."));
    stack->addWidget(layerRow);

    preview_ = new QLabel(this);
    preview_->setObjectName(QStringLiteral("toolPreview"));
    preview_->setWordWrap(true);
    preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    preview_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    preview_->setAccessibleName(tr("Gönderilecek komut satırı"));
    stack->addWidget(preview_);

    run_ = new Button(ButtonRole::Primary, tr("Çalıştır"), Glyph::Function, this);
    run_->setAccessibleName(tr("Aracı çalıştır"));
    run_->setEnabled(false);
    connect(run_, &QAbstractButton::clicked, this, [this] { run(); });
    stack->addWidget(run_);
    stack->addStretch(1);

    refresh();
}

void ToolCard::setViewportProvider(std::function<core::Box2()> provider)
{
    viewport_ = std::move(provider);
}

void ToolCard::setRunButtonVisible(bool on)
{
    run_->setVisible(on);
}

void ToolCard::setTool(const processing::ProcessingTool* tool)
{
    shown_ = tool;

    // The old card's rows go — a row owns its field — and every widget below is
    // rebuilt from the spec.
    bound_.clear();
    while (QLayoutItem* item = paramsLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    while (chipsLayout_->count() > 1) {
        QLayoutItem* item = chipsLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }

    if (tool == nullptr) {
        title_->setText(tr("Bir araç seçin"));
        summary_->setText(tr("Ağaçtan bir araç seçin; kartı burada açılır."));
        run_->setEnabled(false);
        preview_->clear();
        return;
    }

    const auto& spec = tool->spec();
    title_->setText(utf8(spec.title));
    summary_->setText(utf8(spec.summary));

    const Palette& p = themePalette(theme_);
    int at           = 0;
    for (const processing::Applies one :
         {processing::Applies::Points, processing::Applies::Lines, processing::Applies::Faces,
          processing::Applies::Curves, processing::Applies::Texts}) {
        if (!processing::applies_to(spec.applies, one)) continue;
        auto* chip = new Chip(QString::fromUtf8(processing::applies_name(one)), p.accent, chips_);
        chip->setToolTip(tr("Bu araç %1 nesnelere uygulanır").arg(chip->text()));
        chipsLayout_->insertWidget(at++, chip);
    }

    // WHAT WAS USED LAST TIME, when the drawing asks for it: the last run of
    // this tool in the journal carries every parameter it resolved, so the card
    // opens on those values rather than on the defaults (`core.islem.hatirla`).
    const command::Args* last = nullptr;
    if (controller_.bus().project_settings().get("core.islem.hatirla").as_bool()) {
        const auto& entries = controller_.bus().journal().entries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            if (it->command_id == spec.id) {
                last = &it->args;
                break;
            }
    }

    for (const processing::ToolParam& param : spec.params) {
        auto* field = new Field(field_for(param), params_);
        field->setFixedHeight(static_cast<int>(ControlSize::Regular));
        field->setAccessibleName(utf8(param.name));
        if (!param.fallback.empty()) field->setValue(utf8(param.fallback));
        if (last != nullptr)
            if (const command::Value* v = last->find(param.name); v != nullptr && !v->empty())
                field->setValue(value_text(*v));
        connect(field, &Field::committed, this, [this](const QString&) { rebuildPreview(); });
        auto* row = new FormRow(utf8(param.name), field, params_);
        row->setHelp(utf8(param.help));
        paramsLayout_->addWidget(row);
        bound_.push_back(Bound{.param = &param, .field = field});
    }
    layer_->setValue(QString());

    run_->setEnabled(true);
    rebuildPreview();
    applyTheme(theme_);
    applyThemeToChildren(this, theme_);
}

QString ToolCard::commandLine() const
{
    if (shown_ == nullptr) return QString();
    const auto& spec = shown_->spec();
    QString line     = utf8(spec.names.front());

    switch (scope_->current()) {
    case 1: {
        line += QStringLiteral(" kapsam=gorunum");
        if (viewport_) {
            const core::Box2 box = viewport_();
            line += QStringLiteral(" pencere=%1,%2 pencere=%3,%4")
                        .arg(metres(box.min_x), metres(box.min_y), metres(box.max_x),
                             metres(box.max_y));
        }
        break;
    }
    case 2: line += QStringLiteral(" kapsam=proje"); break;
    default: line += QStringLiteral(" kapsam=secili"); break;
    }

    for (const Bound& b : bound_) {
        const QString value = b.field->value().trimmed();
        if (value.isEmpty() || value == utf8(b.param->fallback)) continue;
        line += QLatin1Char(' ') + utf8(b.param->name) + QLatin1Char('=') + quoted(value);
    }
    if (const QString layer = layer_->value().trimmed(); !layer.isEmpty())
        line += QStringLiteral(" katman=") + quoted(layer);
    return line;
}

void ToolCard::rebuildPreview()
{
    preview_->setText(commandLine());
}

void ToolCard::run()
{
    const QString line = commandLine();
    if (line.isEmpty()) return;
    emit runRequested(line);
}

void ToolCard::refresh()
{
    const std::size_t n = controller_.bus().selection().size();
    QString hint        = n == 0
                              ? tr("Seçim boş: Seçili kapsamı tuvalden nesne seçtirir, sağ tık bitirir.")
                              : tr("%1 nesne seçili.").arg(n);
    // The one selected face's area, because the tool that changes an area is
    // asked "to what?" and the answer starts from "from what".
    if (n == 1 && shown_ != nullptr &&
        processing::applies_to(shown_->spec().applies, processing::Applies::Faces)) {
        const auto& picked = controller_.selectedSlots(); // not `slots`: a Qt macro
        if (!picked.empty() && processing::classify(controller_.document(), picked.front()) ==
                                   processing::Applies::Faces)
            hint += tr(" Alanı %1.")
                        .arg(utf8(core::format_square_metres(
                            controller_.document().entity_area(picked.front()))));
    }
    scopeHint_->setText(hint);
}

void ToolCard::applyTheme(ThemeMode mode)
{
    theme_           = mode;
    const Palette& p = themePalette(mode);
    for (QLabel* quiet : {summary_, scopeHint_, preview_}) {
        QPalette pal = quiet->palette();
        pal.setColor(QPalette::WindowText, p.textMuted);
        quiet->setPalette(pal);
    }
    QPalette pal = title_->palette();
    pal.setColor(QPalette::WindowText, p.text);
    title_->setPalette(pal);
    update();
}

// =============================================================================
// ToolDialog
// =============================================================================

ToolDialog::ToolDialog(Controller& controller, const processing::ProcessingTool* tool,
                       std::function<core::Box2()> viewport, QWidget* parent)
    : DialogFrame(parent)
{
    setObjectName(QStringLiteral("toolDialog"));
    setAttribute(Qt::WA_DeleteOnClose);
    setHeading(Glyph::Function, tool != nullptr ? utf8(tool->spec().title) : tr("İşlem aracı"),
               tr("— İşlem aracı"));

    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(16, 12, 16, 12);
    auto* scroll = new QScrollArea(body);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->viewport()->setAutoFillBackground(false);
    card_ = new ToolCard(controller, scroll);
    card_->setViewportProvider(std::move(viewport));
    card_->setRunButtonVisible(false);
    card_->setTool(tool);
    scroll->setWidget(card_);
    column->addWidget(scroll, 1);
    setBody(body);

    auto* close = new Button(ButtonRole::Secondary, tr("Kapat"), std::nullopt, this);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(close);
    auto* run = new Button(ButtonRole::Primary, tr("Çalıştır"), Glyph::Function, this);
    run->setDefault(true);
    connect(run, &QPushButton::clicked, this, [this] { card_->run(); });
    footer()->addWidget(run);

    connect(card_, &ToolCard::runRequested, this, [this](const QString& line) {
        emit runRequested(line);
        accept();
    });
    resize(420, 640);
}

// =============================================================================
// ToolsPanel
// =============================================================================

ToolsPanel::ToolsPanel(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setObjectName(QStringLiteral("toolsPanel"));
    setAccessibleName(tr("Araçlar"));
    setMinimumWidth(240);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ---- the search box and the tree ----
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("toolSearch"));
    search_->setPlaceholderText(tr("Araç ara…"));
    search_->setAccessibleName(tr("Araç arama"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this,
            [this](const QString& text) { rebuildTree(text); });
    root->addWidget(search_);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("toolTree"));
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setRootIsDecorated(true);
    tree_->setIndentation(14);
    tree_->setIconSize(QSize(16, 16));
    tree_->setUniformRowHeights(true);
    tree_->setAccessibleName(tr("İşlem araçları"));
    tree_->setAccessibleDescription(tr("Gruplara ayrılmış işlem araçları; bir satır seçmek "
                                       "aracın kartını açar, Enter çalıştırır"));
    tree_->setMinimumHeight(120);
    // CLICKED, not "selection changed": a row that is already selected does not
    // change the selection, and a second click on the same tool then did
    // nothing — with the card in its window, that was a window that would not
    // come back. Every click shows the tool; the keyboard walk still does too.
    const auto show_row = [this](QTreeWidgetItem* row) {
        if (row == nullptr) return;
        const QString id = row->data(0, kToolRole).toString();
        showTool(id.isEmpty() ? nullptr : processing::find_tool(id.toStdString()));
    };
    connect(tree_, &QTreeWidget::itemClicked, this,
            [show_row](QTreeWidgetItem* row, int) { show_row(row); });
    connect(tree_, &QTreeWidget::currentItemChanged, this,
            [show_row](QTreeWidgetItem* row, QTreeWidgetItem*) { show_row(row); });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* row, int) {
        if (row == nullptr || row->data(0, kToolRole).toString().isEmpty()) return;
        if (opensInWindow()) return; // the window's own run button is the way
        card_->run();
    });
    root->addWidget(tree_, 1);

    // ---- the card under the tree, on the panel's own ground ----
    scroll_ = new QScrollArea(this);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->viewport()->setAutoFillBackground(false);
    card_ = new ToolCard(controller_, scroll_);
    connect(card_, &ToolCard::runRequested, this, &ToolsPanel::runRequested);
    scroll_->setWidget(card_);
    root->addWidget(scroll_, 2);

    rebuildTree(QString());
    refresh();
}

void ToolsPanel::setViewportProvider(std::function<core::Box2()> provider)
{
    viewport_ = std::move(provider);
    card_->setViewportProvider(viewport_);
    if (dialog_) dialog_->card()->setViewportProvider(viewport_);
}

bool ToolsPanel::opensInWindow() const
{
    return controller_.bus().app_settings().get("core.islem.pencere").as_bool();
}

void ToolsPanel::rebuildTree(const QString& filter)
{
    const QString remembered =
        card_->tool() != nullptr ? utf8(card_->tool()->spec().id) : QString();
    tree_->clear();
    const Palette& p       = themePalette(theme_);
    QTreeWidgetItem* group = nullptr;
    QString groupName;
    for (const processing::ProcessingTool* tool : processing::processing_tools()) {
        const auto& spec    = tool->spec();
        const QString title = utf8(spec.title);
        const QString gname = utf8(spec.group);
        if (!filter.isEmpty() && !title.contains(filter, Qt::CaseInsensitive) &&
            !utf8(spec.summary).contains(filter, Qt::CaseInsensitive) &&
            !utf8(spec.names.front()).contains(filter, Qt::CaseInsensitive))
            continue;
        if (group == nullptr || groupName != gname) {
            group     = new QTreeWidgetItem(tree_, QStringList{gname});
            groupName = gname;
            group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
            // The group wears its first tool's mark, in the accent: a heading,
            // not one more row.
            group->setIcon(0, icon(glyph_named(spec.icon), p.accent, p.accent, 16));
            group->setExpanded(true);
        }
        auto* row = new QTreeWidgetItem(group, QStringList{title});
        row->setData(0, kToolRole, utf8(spec.id));
        row->setIcon(0, icon(glyph_named(spec.icon), p.textMuted, p.accent, 16));
        // What a user would type: the name beside the title, as the tool flyout does.
        row->setToolTip(0, utf8(spec.names.front()) + QStringLiteral(" — ") + utf8(spec.summary));
        if (utf8(spec.id) == remembered) row->setSelected(true);
    }
}

void ToolsPanel::showTool(const processing::ProcessingTool* tool)
{
    card_->setTool(tool);
    if (tool == nullptr || !opensInWindow()) return;

    // IN A WINDOW, when the preference says so: one window, re-pointed at the
    // tool clicked, never a stack of them.
    if (!dialog_) {
        dialog_ = new ToolDialog(controller_, tool, viewport_, window());
        connect(dialog_, &ToolDialog::runRequested, this, &ToolsPanel::runRequested);
        dialog_->applyTheme(theme_);
    } else {
        dialog_->card()->setTool(tool);
        dialog_->setHeading(Glyph::Function, utf8(tool->spec().title), tr("— İşlem aracı"));
    }
    dialog_->show();
    dialog_->raise();
    dialog_->activateWindow();
}

QString ToolsPanel::commandLine() const
{
    if (dialog_ && dialog_->isVisible()) return dialog_->card()->commandLine();
    return card_->commandLine();
}

void ToolsPanel::refresh()
{
    card_->refresh();
    if (dialog_) dialog_->card()->refresh();
    // The card under the tree steps aside when the window is the way.
    scroll_->setVisible(!opensInWindow());
}

bool ToolsPanel::selectTool(const QString& id)
{
    const processing::ProcessingTool* tool = processing::find_tool(id.toStdString());
    if (tool == nullptr) return false;
    for (int g = 0; g < tree_->topLevelItemCount(); ++g) {
        QTreeWidgetItem* group = tree_->topLevelItem(g);
        for (int r = 0; r < group->childCount(); ++r) {
            QTreeWidgetItem* row = group->child(r);
            if (row->data(0, kToolRole).toString() == utf8(tool->spec().id)) {
                tree_->setCurrentItem(row);
                row->setSelected(true);
                return true;
            }
        }
    }
    showTool(tool);
    return true;
}

void ToolsPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), tokensOf(theme_).bgPanel);
}

void ToolsPanel::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    rebuildTree(search_->text()); // the marks re-tint with the theme
    update();
}

} // namespace kentos::app
