// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/settings_dialog.hpp"

#include "kentos_cad/app/icons.hpp"

#include "kentos_cad/app/controller.hpp"

#include "kentos_cad/command/bus.hpp"

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
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

namespace kentos::app {
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

/// The namespace whose glyph stands for a whole page.
///
/// A page holds several namespaces and the list shows one icon, so this names
/// the one that carries the page's meaning. Unknown titles fall through to the
/// settings glyph, which is what a page declared tomorrow gets.
std::string section_group(const std::string& title)
{
    static const std::pair<const char*, const char*> kFaces[] = {
        {"Genel", "dosya"},
        {"Görünüm ve Tema", "arayuz"},
        {"Çizim ve Yakalama", "yakalama"},
        {"Koordinat Sistemleri", "crs"},
        {"Veri Kaynakları", "veritabani"},
        {"Plot ve Çıktı", "plan"},
        {"Etiketleme", "cizim"},
        {"Kısayollar", "secim"},
        {"Performans ve GPU", "tuval"},
        {"Ağ ve Kimlik", "veritabani"},
    };
    for (const auto& [name, group] : kFaces)
        if (title == name) return group;
    return {};
}

/// The glyph a section wears in the list. Chosen from the group's own id, so a
/// new group arrives with an icon rather than with a blank.
Glyph group_glyph(const std::string& group)
{
    static const std::pair<const char*, Glyph> kGlyphs[] = {
        {"crs", Glyph::Globe},         {"cizim", Glyph::Polyline},   {"katalog", Glyph::Table},
        {"topoloji", Glyph::Topology}, {"plan", Glyph::Print},       {"aci", Glyph::Rotate},
        {"alan", Glyph::MeasureArea},  {"arayuz", Glyph::Palette},   {"dosya", Glyph::Open},
        {"tuval", Glyph::Grid},        {"izgara", Glyph::Grid},      {"yakalama", Glyph::Snap},
        {"secim", Glyph::Select},      {"stil", Glyph::Layer},       {"cetvel", Glyph::Measure},
        {"harita", Glyph::Terrain},    {"veritabani", Glyph::Cloud},
    };
    for (const auto& [id, glyph] : kGlyphs)
        if (group == id) return glyph;
    return Glyph::Settings;
}

/// What a section's page says under its heading.
///
/// Derived from the SCOPES the group's settings actually declare, not written
/// out per group: the sentence a user needs is "does this travel with the file",
/// and only the scope answers it (model.md R39, R40).
QString scope_summary(bool project, bool app, bool session)
{
    // ONE line, and it answers the only question the heading leaves open: does
    // this travel with the file. A paragraph here pushes the first setting off
    // the top of the page, which is the opposite of what a heading is for; the
    // per-setting summary under each name carries the detail.
    if (project && !app && !session)
        return SettingsDialog::tr("Bu ayarlar çizimin kendisine aittir: dosyayla birlikte gider "
                                  "ve başka bir bilgisayarda açıldığında aynı kalır.");
    if (app && !project && !session)
        return SettingsDialog::tr("Bu ayarlar yalnızca geçerli profil için geçerlidir; çizimin "
                                  "tek baytını değiştirmez.");
    if (session && !project && !app)
        return SettingsDialog::tr("Bu ayarlar yalnız bu oturum için geçerlidir ve program "
                                  "kapanınca varsayılana döner.");
    return SettingsDialog::tr("Bu bölümde hem çizime hem bu bilgisayara ait ayarlar var; her "
                              "satırın altındaki açıklama hangisi olduğunu söyler.");
}

} // namespace

SettingsDialog::SettingsDialog(Controller& controller, QWidget* parent)
    : DialogFrame(parent), controller_(controller)
{
    // design.md §10 measures this window at 1180 × 740, with a 232 px left
    // column and a 52 px footer. Every one of those numbers is the reference's.
    setHeading(Glyph::Settings, tr("Seçenekler"));
    setFooterHeight(52);
    setModal(true);
    resize(1180, 740);

    auto* body = new QWidget(this);
    auto* row  = new QHBoxLayout(body);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    // ---- the left column ----
    auto* sidebar = new QWidget(body);
    sidebar->setObjectName(QStringLiteral("settingsSidebar"));
    sidebar->setFixedWidth(232);

    auto* column = new QVBoxLayout(sidebar);
    column->setContentsMargins(0, 0, 1, 0);
    column->setSpacing(0);

    search_ = new QLineEdit(sidebar);
    search_->setObjectName(QStringLiteral("settingsSearch"));
    search_->setPlaceholderText(tr("Ayarlarda ara…")); // ui-label
    search_->setClearButtonEnabled(true);

    auto* searchRow = new QWidget(sidebar);
    auto* searchBox = new QHBoxLayout(searchRow);
    searchBox->setContentsMargins(12, 12, 12, 10);
    searchBox->addWidget(search_);
    column->addWidget(searchRow);

    sections_ = new SectionList(sidebar);
    column->addWidget(sections_);
    column->addStretch(1);

    // §10 puts the profile at the foot of the column, because "which profile am
    // I editing" is the question every one of these answers belongs to.
    profile_ = new QLabel(sidebar);
    profile_->setObjectName(QStringLiteral("settingsProfile"));
    profile_->setContentsMargins(12, 0, 12, 0);
    profile_->setFixedHeight(34);
    column->addWidget(profile_);

    row->addWidget(sidebar);

    // ---- the page ----
    auto* right = new QWidget(body);
    auto* stack = new QVBoxLayout(right);
    stack->setContentsMargins(24, 18, 24, 0);
    stack->setSpacing(0);

    heading_ = new QLabel(right);
    heading_->setObjectName(QStringLiteral("settingsHeading"));
    stack->addWidget(heading_);

    summary_ = new QLabel(right);
    summary_->setObjectName(QStringLiteral("quiet"));
    summary_->setWordWrap(true);
    summary_->setContentsMargins(0, 4, 0, 14);
    stack->addWidget(summary_);

    pages_ = new QStackedWidget(right);
    stack->addWidget(pages_, 1);
    row->addWidget(right, 1);

    setBody(body);

    // One page per DECLARED SECTION, in the catalogue's own order. Not a list
    // kept here: `settings.cpp` declares the pages beside the settings, so a new
    // page arrives with no edit to this file (CLAUDE.md 5.10, the same rule the
    // command list lives under).
    for (const core::SettingSection& declared : core::builtin_settings().sections()) {
        Section section;
        section.group = declared.title;
        section.title = QString::fromStdString(declared.title);
        section.page  = declared.phase.empty() ? buildGroup(declared.title, section.title)
                                               : buildPending(QString::fromStdString(declared.phase),
                                                              QString::fromStdString(declared.note));
        pages_->addWidget(section.page);
        sections_->addSection(group_glyph(section_group(declared.title)), section.title);
        order_.push_back(section);
    }

    connect(sections_, &SectionList::currentChanged, this, [this](int index) {
        if (index < 0 || index >= static_cast<int>(order_.size())) return;
        const auto at = static_cast<std::size_t>(index);
        pages_->setCurrentIndex(index);
        heading_->setText(order_[at].title);

        bool project = false, app = false, session = false;
        for (const SettingSpec& spec : core::builtin_settings().all()) {
            if (spec.section != order_[at].group) continue;
            project |= spec.scope == SettingScope::Project;
            app |= spec.scope == SettingScope::App;
            session |= spec.scope == SettingScope::Session;
        }
        summary_->setText(scope_summary(project, app, session));
    });

    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        sections_->setFilter(text);
        applyFilter();
    });

    // ---- the footer ----
    const auto footerButton = [this](const QString& text, bool primary) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(primary ? QStringLiteral("primary") : QString());
        button->setDefault(primary);
        return button;
    };

    auto* defaults = footerButton(tr("Varsayılanlara dön"), false);
    connect(defaults, &QPushButton::clicked, this, [this] {
        for (const Row& r : rows_)
            write(*r.spec, QStringLiteral("varsayılan"));
    });

    auto* exportProfile = footerButton(tr("Profili dışa aktar"), false);
    connect(exportProfile, &QPushButton::clicked, this, [this] {
        // Phase 2. Said in the window rather than in a tooltip, because a button
        // that does nothing silently is worse than one that says why.
        QMessageBox::information(this, tr("Profili dışa aktar"),
                                 tr("Profil dışa aktarma Faz 2'de gelecek. Şimdilik her ayar "
                                    "TERCİH, AYAR ve MOD komutlarıyla yazılabilir ve komut "
                                    "günlüğü zaten taşınabilir bir kayıttır."));
    });

    auto* cancel = footerButton(tr("İptal"), false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    // EVERY EDIT IS ALREADY WRITTEN. A setting changes the moment its control
    // does, through the bus — so `Uygula` has nothing left to apply and `Tamam`
    // only closes. Both are here because §10 draws them and because a user who
    // does not see them wonders whether anything was saved; neither pretends to
    // do work it does not do, and the tooltip says so.
    auto* apply = footerButton(tr("Uygula"), false);
    apply->setToolTip(tr("Her değişiklik yazıldığı anda uygulanır; bu düğme pencereyi açık "
                         "bırakır."));
    connect(apply, &QPushButton::clicked, this, [this] {
        for (Section& section : order_)
            (void)section;
        refresh();
    });

    auto* ok = footerButton(tr("Tamam"), true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* bar = footer();
    bar->insertWidget(0, defaults);
    bar->insertWidget(1, exportProfile);
    bar->addWidget(cancel);
    bar->addWidget(apply);
    bar->addWidget(ok);

    // A setting written from the command line while this window is open has to
    // show through: the store is the truth and this window is one of its readers.
    connect(&controller_, &Controller::settingChanged, this, [this](const QString&) { refresh(); });

    if (!order_.empty()) {
        sections_->setCurrent(0);
        pages_->setCurrentIndex(0);
        heading_->setText(order_.front().title);
        emit sections_->currentChanged(0);
    }

    refresh();
}

QWidget* SettingsDialog::buildPending(const QString& phase, const QString& note)
{
    // A page that holds nothing YET. §11.8 forbids the aspirational present
    // tense, so it says what will be here and which phase brings it, in the
    // future tense, rather than showing an empty box.
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 24, 12, 12);
    layout->setSpacing(8);

    auto* badge = new QLabel(phase, page);
    badge->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(badge);

    auto* words = new QLabel(note, page);
    words->setObjectName(QStringLiteral("quiet"));
    words->setWordWrap(true);
    words->setMaximumWidth(560);
    layout->addWidget(words);
    layout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

QWidget* SettingsDialog::buildGroup(const std::string& section, const QString& title)
{
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 12, 12);
    layout->setSpacing(0);

    (void)title;

    // Inside a page the settings keep their NAMESPACE grouping, which is the
    // second axis: `Çizim ve Yakalama` holds a YAKALAMA block and a TOPOLOJİ
    // block, exactly as the reference draws it.
    std::string open_group;
    for (const SettingSpec& spec : core::builtin_settings().all()) {
        if (spec.section != section) continue;

        const std::string group = group_of(spec.id);
        if (group != open_group) {
            auto* caption = new QLabel(group_title(group).toUpper(), page);
            caption->setObjectName(QStringLiteral("sectionTitle"));
            caption->setContentsMargins(0, open_group.empty() ? 0 : 18, 0, 6);
            layout->addWidget(caption);
            open_group = group;
        }
        addRow(layout, spec);
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
    // §10's row: the name over its one-line help on the left, the control
    // right-aligned, and the reset mark after it. The help is the SETTING'S OWN
    // summary — the sentence the catalogue already carries — so a row explains
    // itself without this file knowing what any of them mean.
    auto* line   = new QWidget(into->parentWidget());
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 7, 0, 7);
    layout->setSpacing(8);

    auto* words = new QVBoxLayout;
    words->setContentsMargins(0, 0, 0, 0);
    words->setSpacing(2);

    auto* label = new QLabel(row_label(spec), line);
    label->setObjectName(QStringLiteral("rowName"));

    // Both the typed name and the machine id go in the tooltip, because the
    // window has a second job: somebody who finds a setting here has to be able
    // to write the line that sets it. A script writes the id, a person types the
    // name, and the row shows neither until asked.
    label->setToolTip(QStringLiteral("%1\n%2").arg(QString::fromStdString(spec.names.front()),
                                                   QString::fromStdString(spec.id)));
    words->addWidget(label);

    if (!spec.summary.empty()) {
        auto* help = new QLabel(QString::fromStdString(spec.summary), line);
        help->setObjectName(QStringLiteral("rowHelp"));
        help->setWordWrap(true);
        words->addWidget(help);
    }

    QWidget* editor = nullptr;

    switch (spec.type) {
    case SettingType::Bool: {
        // §10 draws a pill switch, not a tick box: the fill AND the knob's side
        // both say the state, which is what §13 asks of a yes/no control.
        auto* box = new ToggleSwitch(line);
        connect(box, &ToggleSwitch::toggled, this, [this, &spec](bool on) {
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

    // §10's proportions: the words take the left two thirds and the control sits
    // at a fixed width against the right margin, so every control in the page
    // starts at the same x. A ragged column of controls reads as a mistake.
    auto* words_host = new QWidget(line);
    words_host->setLayout(words);
    words_host->setMinimumWidth(360);
    words_host->setMaximumWidth(560);

    layout->addWidget(words_host, 1);
    layout->addStretch(1);
    if (editor) {
        editor->setFixedWidth(qobject_cast<ToggleSwitch*>(editor) ? 38 : 250);
        layout->addWidget(editor, 0, Qt::AlignRight | Qt::AlignVCenter);
    }

    // NO PER-ROW RESET MARK AND NO STATE TAG. The reference has neither, and both
    // were noise beside every row. The capability stays: the row's context menu
    // offers the reset and names the current state, which is where a user looks
    // for "put this back" and nowhere near where they look for the value.
    line->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(line, &QWidget::customContextMenuRequested, this,
            [this, &spec, line](const QPoint& at) {
                QMenu menu(line);
                menu.addAction(tr("Kimlik: %1").arg(QString::fromStdString(spec.id)))
                    ->setEnabled(false);
                menu.addSeparator();
                QAction* reset_to_default = menu.addAction(tr("Bildirilen varsayılana döndür"));
                if (menu.exec(line->mapToGlobal(at)) == reset_to_default)
                    write(spec, QStringLiteral("varsayılan"));
            });

    state->setVisible(false);
    reset->setVisible(false);
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
            // A `ToggleSwitch` now, not a `QCheckBox`. The cast was unchecked and
            // the null it returned after the control changed was dereferenced
            // straight away — the window did not fail to draw, it crashed.
            if (auto* box = qobject_cast<ToggleSwitch*>(row.editor))
                box->setChecked(value.as_bool());
            break;
        case SettingType::Enum: {
            auto* box = qobject_cast<QComboBox*>(row.editor);
            if (box && value.as_enum() < row.spec->values.size())
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

} // namespace kentos::app
