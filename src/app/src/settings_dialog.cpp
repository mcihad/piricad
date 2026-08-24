// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/settings_dialog.hpp"

#include "piricad/app/controller.hpp"

#include "piricad/command/bus.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

namespace piricad::app {
namespace {

using core::SettingScope;
using core::SettingSpec;
using core::SettingType;

/// The group a setting belongs to, taken from its own id.
///
/// `core.izgara.adim` groups under `izgara`.
std::string group_of(const std::string& id)
{
    const auto first = id.find('.');
    if (first == std::string::npos) return id;

    const auto second = id.find('.', first + 1);
    if (second == std::string::npos) return id.substr(first + 1);
    return id.substr(first + 1, second - first - 1);
}

/// Turkish first-letter capitalisation. `QLocale` and not `std::toupper`, which
/// cannot do the dotted and dotless i (CLAUDE.md 5.6): `ızgara` has to become
/// `Izgara` and `imleç` has to become `İmleç`, and an ASCII classifier gets both
/// of those wrong.
QString capitalised(QString text)
{
    if (text.isEmpty()) return text;
    return QLocale(QLocale::Turkish).toUpper(text.left(1)) + text.mid(1);
}

/// What a heading calls one group, in Turkish.
///
/// An id is ASCII by construction — `core.cizim`, `core.crs`, `core.aci` — and a
/// heading made of it reads as `Cizim`, `Crs`, `Aci`. A settings window is read,
/// not typed, so its headings are written in the language the rest of it is in.
///
/// A TABLE, and deliberately: what a group of settings is CALLED has no
/// declaration anywhere to derive it from, exactly as the snap marker's glyph
/// has none (see `map_canvas.cpp`). Both are decisions about the product. An id
/// with no row here still gets a heading — its own component, capitalised — so a
/// new group appears readable rather than disappearing.
QString group_title(const std::string& group)
{
    static const std::pair<const char*, const char*> kTitles[] = {
        {"crs", "Koordinat sistemi"},
        {"cizim", "Çizim"},
        {"katalog", "Katalog"},
        {"topoloji", "Topoloji"},
        {"plan", "Pafta"},
        {"aci", "Açı"},
        {"alan", "Alan"},
        {"arayuz", "Arayüz"},
        {"dosya", "Dosya"},
        {"tuval", "Tuval"},
        {"izgara", "Izgara"},
        {"yakalama", "Nesne yakalama"},
        {"secim", "Seçim"},
        {"stil", "Gösterim rafı"}, // ui-label
        {"cetvel", "Cetvel"},
        {"harita", "Harita"},
        {"veritabani", "Veritabanı"},
    };

    for (const auto& [id, title] : kTitles)
        if (group == id) return QString::fromUtf8(title);

    return capitalised(QString::fromStdString(group));
}

/// What a row calls one setting, in Turkish.
///
/// Derived from the setting's OWN primary name rather than written out beside it:
/// `ızgara_adımı` becomes `Izgara adımı`. The underscores are there because that
/// name is what a user TYPES, and a window is not a command line — but the two
/// must not drift apart either, so the label is the name with its punctuation
/// relaxed and nothing else. The typed form stays one hover away.
QString row_label(const SettingSpec& spec)
{
    QString name = QString::fromStdString(spec.names.front());
    name.replace('_', QLatin1Char(' '));
    return capitalised(name);
}

/// The label a picker's title bar shows: the setting's own primary name.
QString spec_label(const SettingSpec& spec)
{
    return QString::fromStdString(spec.names.front());
}

/// A value the parser will read back as one token.
QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

/// The widest a Length editor may go. `Mm` is int64 and the ranges reach 1e9, so
/// the editor is a double spin box counting millimetres; a double holds every
/// millimetre in that span exactly.
constexpr double kLengthCeiling = 1.0e9;

} // namespace

SettingsDialog::SettingsDialog(Controller& controller, QWidget* parent)
    : QDialog(parent), controller_(controller)
{
    setWindowTitle(tr("Ayarlar"));
    setMinimumSize(760, 620);
    resize(880, 700);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Ara — ayar adı, kimlik ve açıklama")); // ui-label
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString&) { applyFilter(); });

    tabs_ = new QTabWidget(this);

    // One page per scope, in the order a user meets them: what belongs to the
    // drawing, what belongs to the machine, what belongs to this sitting.
    struct Page
    {
        SettingScope scope;
        QString title;
        QString help;
    };

    const Page pages[] = {
        {SettingScope::Project, tr("Proje"),
         tr("Çizimin kendi özellikleri. Dosyayla birlikte gider ve başka bir "
            "bilgisayarda açıldığında aynı kalır.")},
        {SettingScope::App, tr("Uygulama"),
         tr("Bu bilgisayardaki tercihleriniz. Çizimin tek baytını değiştirmez; "
            "dosyayı paylaştığınızda karşı tarafa geçmez.")},
        {SettingScope::Session, tr("Oturum"),
         tr("Yalnız bu açık pencere için geçerli. Program kapanınca varsayılana "
            "döner; çizim yaparken sık sık değiştirilen anahtarlar buradadır.")},
    };

    for (const Page& page : pages) {
        QWidget* body = buildScope(page.scope);
        if (body == nullptr) continue;

        auto* wrap   = new QWidget(tabs_);
        auto* layout = new QVBoxLayout(wrap);
        layout->setContentsMargins(0, 8, 0, 0);

        // The scope, said in the user's own words at the top of its own page.
        // "Proje kapsamı" means nothing until somebody explains that it travels
        // with the file, and every summary in the catalogue has to repeat the
        // sentence because nowhere else says it.
        auto* help = new QLabel(page.help, wrap);
        help->setWordWrap(true);
        help->setObjectName(QStringLiteral("quiet"));
        layout->addWidget(help);
        layout->addWidget(body, 1);

        tabs_->addTab(wrap, page.title);
    }

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Kapat"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);
    root->addWidget(search_);
    root->addWidget(tabs_, 1);
    root->addWidget(buttons);

    setStyleSheet(QStringLiteral(R"(
        QLabel#quiet    { color: palette(dark); }
        QLabel#stateTag { color: palette(dark); }
        QGroupBox {
            border: 1px solid palette(mid);
            border-radius: 6px;
            margin-top: 9px;
            padding: 10px 10px 9px 10px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
        }
    )"));

    // A setting written from the command line while this window is open has to
    // show through: the store is the truth and this window is one of its readers.
    connect(&controller_, &Controller::settingChanged, this, [this](const QString&) { refresh(); });

    refresh();
}

QWidget* SettingsDialog::buildScope(SettingScope scope)
{
    const core::SettingCatalog& catalogue = core::builtin_settings();

    // Grouped by the id's own second component, in the order the catalogue
    // declares them, so a group's rows keep the order somebody chose.
    std::vector<std::string> order;
    std::vector<QVBoxLayout*> bodies;

    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(10);

    for (const SettingSpec& spec : catalogue.all()) {
        if (spec.scope != scope) continue;

        const std::string group = group_of(spec.id);

        QVBoxLayout* body = nullptr;
        for (std::size_t i = 0; i < order.size(); ++i)
            if (order[i] == group) body = bodies[i];

        if (body == nullptr) {
            auto* box = new QGroupBox(group_title(group), page);
            body      = new QVBoxLayout(box);
            body->setSpacing(6);
            layout->addWidget(box);
            order.push_back(group);
            bodies.push_back(body);
        }

        addRow(body, spec);
    }

    if (order.empty()) {
        delete page;
        return nullptr;
    }

    layout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

void SettingsDialog::addRow(QVBoxLayout* into, const SettingSpec& spec)
{
    auto* line   = new QWidget(into->parentWidget());
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // The setting's own name, read rather than typed; `row_label` says how. Both
    // the typed name and the machine id go in the tooltip, because the window has
    // a second job: somebody who finds a setting here has to be able to write the
    // line that sets it. A script writes the id, a person types the name, and this
    // row shows neither until asked.
    auto* label = new QLabel(row_label(spec), line);
    label->setMinimumWidth(230);
    label->setToolTip(QStringLiteral("%1\n%2\n\n%3")
                          .arg(QString::fromStdString(spec.names.front()),
                               QString::fromStdString(spec.id),
                               QString::fromStdString(spec.summary)));

    QWidget* editor = nullptr;

    switch (spec.type) {
    case SettingType::Bool: {
        auto* box = new QCheckBox(line);
        connect(box, &QCheckBox::toggled, this, [this, &spec](bool on) {
            if (!loading_) write(spec, on ? QStringLiteral("evet") : QStringLiteral("hayır"));
        });
        editor = box;
        break;
    }
    case SettingType::Enum: {
        auto* box = new QComboBox(line);
        for (const std::string& value : spec.values)
            box->addItem(QString::fromStdString(value));
        connect(box, &QComboBox::currentTextChanged, this, [this, &spec](const QString& text) {
            if (!loading_ && !text.isEmpty()) write(spec, text);
        });
        editor = box;
        break;
    }
    case SettingType::Int:
    case SettingType::Length: {
        // A COLOUR gets a colour picker, and the setting says so itself: its unit
        // is `0xAARRGGBB`. Read off the declaration rather than a list of ids kept
        // here, so a colour added to the catalogue arrives with its picker.
        if (spec.unit == "0xAARRGGBB") {
            auto* button = new QToolButton(line);
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            connect(button, &QToolButton::clicked, this, [this, &spec, button] {
                const auto current =
                    static_cast<std::uint32_t>(storeOf(spec.scope).get(spec.id).as_int());

                // Zero means "the theme's own colour" everywhere these settings
                // are read, so the picker opens on the theme rather than on black
                // and an untouched setting does not look like a chosen one.
                const QColor start =
                    current == 0 ? palette().color(QPalette::Highlight) : QColor::fromRgba(current);
                const QColor picked = QColorDialog::getColor(start, this, spec_label(spec),
                                                             QColorDialog::ShowAlphaChannel);
                if (picked.isValid())
                    write(spec, QString::number(static_cast<std::uint32_t>(picked.rgba())));
            });
            editor = button;
            break;
        }

        auto* box = new QDoubleSpinBox(line);
        box->setDecimals(0);
        box->setGroupSeparatorShown(true);
        box->setMinimum(spec.range.bounded() ? static_cast<double>(spec.range.min)
                                             : -kLengthCeiling);
        box->setMaximum(spec.range.bounded() ? static_cast<double>(spec.range.max)
                                             : kLengthCeiling);
        box->setSuffix(spec.unit.empty()
                           ? QString()
                           : QStringLiteral(" %1").arg(QString::fromStdString(spec.unit)));
        // On editingFinished, not on every keystroke: a spin box counting up from
        // 0 to 10000 would otherwise dispatch ten thousand commands and fill the
        // journal with every number on the way.
        connect(box, &QDoubleSpinBox::editingFinished, this, [this, &spec, box] {
            if (!loading_) write(spec, QString::number(box->value(), 'f', 0));
        });
        editor = box;
        break;
    }
    case SettingType::Text: {
        auto* field = new QLineEdit(line);
        connect(field, &QLineEdit::editingFinished, this, [this, &spec, field] {
            if (!loading_) write(spec, quoted(field->text()));
        });
        editor = field;
        break;
    }
    }

    auto* state = new QLabel(line);
    state->setObjectName(QStringLiteral("stateTag"));
    state->setMinimumWidth(84);

    auto* reset = new QToolButton(line);
    reset->setText(QStringLiteral("⟲"));
    reset->setToolTip(tr("Bildirilen varsayılana döndürür"));
    connect(reset, &QToolButton::clicked, this,
            [this, &spec] { write(spec, QStringLiteral("varsayılan")); });

    layout->addWidget(label);
    layout->addWidget(editor, 1);
    layout->addWidget(state);
    layout->addWidget(reset);
    into->addWidget(line);

    rows_.push_back(Row{&spec, line, state, editor});
}

const core::Settings& SettingsDialog::storeOf(SettingScope scope) const
{
    command::Bus& bus = controller_.bus();
    switch (scope) {
    case SettingScope::Project: return bus.project_settings();
    case SettingScope::Session: return bus.session_settings();
    case SettingScope::App: break;
    }
    return bus.app_settings();
}

QString SettingsDialog::commandFor(SettingScope scope)
{
    switch (scope) {
    case SettingScope::Project: return QStringLiteral("AYAR");
    case SettingScope::Session: return QStringLiteral("MOD");
    case SettingScope::App: break;
    }
    return QStringLiteral("TERCİH");
}

void SettingsDialog::write(const SettingSpec& spec, const QString& value)
{
    // The setting's primary name, not its id: this is the line a user would have
    // typed, and it is the line the journal records.
    const QString line =
        QStringLiteral("%1 %2 %3")
            .arg(commandFor(spec.scope), QString::fromStdString(spec.names.front()), value);

    controller_.runLine(line, command::Origin::Gui);
    refresh();
}

void SettingsDialog::refresh()
{
    loading_ = true;

    for (const Row& row : rows_) {
        const core::Settings& store    = storeOf(row.spec->scope);
        const core::SettingValue value = store.get(row.spec->id);

        switch (row.spec->type) {
        case SettingType::Bool:
            qobject_cast<QCheckBox*>(row.editor)->setChecked(value.as_bool());
            break;
        case SettingType::Enum: {
            auto* box = qobject_cast<QComboBox*>(row.editor);
            if (value.as_enum() < row.spec->values.size())
                box->setCurrentIndex(static_cast<int>(value.as_enum()));
            break;
        }
        case SettingType::Int:
        case SettingType::Length:
            if (auto* swatch = qobject_cast<QToolButton*>(row.editor)) {
                const auto rgba = static_cast<std::uint32_t>(value.as_int());
                QPixmap chip(28, 14);
                chip.fill(rgba == 0 ? Qt::transparent : QColor::fromRgba(rgba));

                QPainter painter(&chip);
                painter.setPen(QPen(QColor(0, 0, 0, 90)));
                painter.drawRect(0, 0, chip.width() - 1, chip.height() - 1);
                // "The theme's own" has to look like nothing chosen, not like
                // black — which is a colour a plan sheet uses.
                if (rgba == 0) painter.drawLine(0, chip.height() - 1, chip.width() - 1, 0);
                painter.end();

                swatch->setIcon(QIcon(chip));
                swatch->setIconSize(chip.size());
                swatch->setText(rgba == 0
                                    ? tr("temadan")
                                    : QStringLiteral("#%1").arg(rgba, 8, 16, QLatin1Char('0')));
                break;
            }
            qobject_cast<QDoubleSpinBox*>(row.editor)
                ->setValue(static_cast<double>(value.as_int()));
            break;
        case SettingType::Text:
            qobject_cast<QLineEdit*>(row.editor)
                ->setText(QString::fromUtf8(value.as_text().data(),
                                            static_cast<int>(value.as_text().size())));
            break;
        }

        // Said on every row, because "is this mine or the program's?" is the
        // question a settings window exists to answer.
        row.state->setText(store.is_explicit(row.spec->id) ? tr("ayarlanmış")   // ui-label
                                                           : tr("varsayılan")); // ui-label
    }

    loading_ = false;
}

void SettingsDialog::applyFilter()
{
    const QString needle = search_->text().trimmed();

    for (const Row& row : rows_) {
        if (needle.isEmpty()) {
            row.line->setVisible(true);
            continue;
        }

        // Name, id and summary all searched: a user who remembers "the one about
        // the ruler" and a user who remembers `core.cetvel.birim` are both looking
        // for the same row.
        QString hay = QString::fromStdString(row.spec->id + " " + row.spec->summary);
        for (const std::string& alias : row.spec->names)
            hay += QLatin1Char(' ') + QString::fromStdString(alias);
        hay += QLatin1Char(' ') + row_label(*row.spec);
        row.line->setVisible(hay.contains(needle, Qt::CaseInsensitive));
    }
}

} // namespace piricad::app
