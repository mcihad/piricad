// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/settings_dialog.hpp"

#include "kentos_cad/app/datagrid.hpp"
#include "kentos_cad/app/fields.hpp"
#include "kentos_cad/app/icons.hpp"
#include "kentos_cad/app/print_service.hpp"
#include "kentos_cad/app/provider_dialog.hpp"
#include "kentos_cad/app/provider_service.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/ai/provider.hpp"

#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/schema_page.hpp"

#include "kentos_cad/command/bus.hpp"

#if KENTOS_HAVE_MCP
// AGPL, and included only where the listener exists. A build without it has no
// `McpService` to ask, and the block says so rather than pretending.
#include "kentos_cad/app/mcp_service.hpp"
#endif

#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
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
        {"duzenleme", "Düzenleme"},
        {"ai", "Yapay zeka"},
        {"mcp", "MCP sunucusu"},
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

SettingsDialog::SettingsDialog(Controller& controller, Mode mode, QWidget* parent)
    : DialogFrame(parent), controller_(controller), mode_(mode)
{
    const bool project = mode_ == Mode::Project;

    // design.md §10 measures this window at 1180 × 740, with a 232 px left
    // column and a 52 px footer. Every one of those numbers is the reference's.
    setHeading(project ? Glyph::Document : Glyph::Settings,
               project ? tr("Proje Ayarları") : tr("Seçenekler"),
               project ? tr("— dosyayla birlikte giden her şey") : QString());
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
        if (project) break; // the project window has its own two pages, below
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

    // THE PROJECT WINDOW'S OWN TWO PAGES, and neither is a declared section.
    //
    // The settings half gathers every project-scoped setting whatever topic it
    // was declared under: `Seçenekler` cuts by topic, which is how a person looks
    // for ONE setting, and that scatters "what travels with this file" across
    // five pages. The schema half is not settings at all — it is the document's
    // own columns — but it is the other half of the same question, and a page
    // reachable only from a layer's properties would be the wrong shelf for the
    // columns every object carries.
    if (project) {
        Section settings;
        settings.group = "Proje Ayarları";
        settings.title = tr("Ayarlar");
        settings.page  = buildProjectPage();
        pages_->addWidget(settings.page);
        sections_->addSection(Glyph::Settings, settings.title);
        order_.push_back(settings);

        Section schema;
        schema.group = "Proje Öznitelikleri";
        schema.title = tr("Öznitelikler");
        schema.page  = new SchemaPage(controller_, QString(), this);
        pages_->addWidget(schema.page);
        sections_->addSection(Glyph::Table, schema.title);
        order_.push_back(schema);
    }

    connect(sections_, &SectionList::currentChanged, this, [this](int index) {
        if (index < 0 || index >= static_cast<int>(order_.size())) return;
        const auto at = static_cast<std::size_t>(index);
        pages_->setCurrentIndex(index);
        heading_->setText(order_[at].title);

        // THE TWO PAGES THAT ARE NOT A DECLARED SECTION say what they are
        // themselves; the loop below would find no setting under their name and
        // fall through to the "this page is mixed" sentence, which is exactly
        // wrong for a page whose entire point is that it is not.
        if (order_[at].group == "Proje Ayarları") {
            summary_->setText(scope_summary(true, false, false));
            return;
        }
        if (order_[at].group == "Proje Öznitelikleri") {
            summary_->setText(tr("Çizimdeki her nesnenin taşıdığı sütunlar. Bunlar ayar değil, "
                                 "belgenin şemasıdır: dosyanın içinde saklanır ve dosyayla "
                                 "birlikte gider."));
            return;
        }

        bool inProject = false, app = false, session = false;
        for (const SettingSpec& spec : core::builtin_settings().all()) {
            if (spec.section != order_[at].group) continue;
            inProject |= spec.scope == SettingScope::Project;
            app |= spec.scope == SettingScope::App;
            session |= spec.scope == SettingScope::Session;
        }
        summary_->setText(scope_summary(inProject, app, session));
    });

    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        sections_->setFilter(text);
        applyFilter();
    });

    // ---- the footer ----
    // Roles from the standard, and ONE primary. `Tamam` is the answer; `İptal`
    // and `Uygula` stand beside it as secondaries; the two housekeeping actions
    // at the far left are ghosts — present, low, never competing with the answer.
    auto* defaults = new Button(ButtonRole::Ghost, tr("Varsayılanlara dön"), std::nullopt, this);
    connect(defaults, &QPushButton::clicked, this, [this] {
        for (const Row& r : rows_)
            write(*r.spec, QStringLiteral("varsayılan"));
    });

    auto* exportProfile =
        new Button(ButtonRole::Ghost, tr("Profili dışa aktar"), std::nullopt, this);
    connect(exportProfile, &QPushButton::clicked, this, [this] {
        // Phase 2. Said in the window rather than in a tooltip, because a button
        // that does nothing silently is worse than one that says why.
        QMessageBox::information(this, tr("Profili dışa aktar"),
                                 tr("Profil dışa aktarma Faz 2'de gelecek. Şimdilik her ayar "
                                    "TERCİH, AYAR ve MOD komutlarıyla yazılabilir ve komut "
                                    "günlüğü zaten taşınabilir bir kayıttır."));
    });

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    // EVERY EDIT IS ALREADY WRITTEN. A setting changes the moment its control
    // does, through the bus — so `Uygula` has nothing left to apply and `Tamam`
    // only closes. Both are here because §10 draws them and because a user who
    // does not see them wonders whether anything was saved; neither pretends to
    // do work it does not do, and the tooltip says so.
    auto* apply = new Button(ButtonRole::Secondary, tr("Uygula"), std::nullopt, this);
    apply->setToolTip(tr("Her değişiklik yazıldığı anda uygulanır; bu düğme pencereyi açık "
                         "bırakır."));
    connect(apply, &QPushButton::clicked, this, [this] {
        for (Section& section : order_)
            (void)section;
        refresh();
    });

    auto* ok = new Button(ButtonRole::Primary, tr("Tamam"), std::nullopt, this);
    ok->setDefault(true);
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

    // THE PROFILES COME FIRST on the plot page, because they are what the rest
    // of it is about: the plan scale below decides what a paper millimetre
    // means, and a profile decides which paper.
    if (mode_ == Mode::All && section == "Plot ve Çıktı") layout->addWidget(buildPrintProfiles());

    // AND THE MODEL PROFILES COME FIRST on the AI page, for the same reason:
    // the two settings below it — whether the project is sensitive, whether the
    // thinking text is shown — are about the model the profiles choose.
    if (mode_ == Mode::All && section == "Yapay Zeka Modelleri")
        layout->addWidget(buildProviderProfiles());

    // AND THE LISTENER COMES FIRST on the agent page: the port and the token
    // requirement below it are what the address above is made of, and the
    // address is what a person came to this page to copy.
    if (mode_ == Mode::All && section == "MCP Sunucusu") layout->addWidget(buildAgentServer());

    // Inside a page the settings keep their NAMESPACE grouping, which is the
    // second axis: `Çizim ve Yakalama` holds a YAKALAMA block and a TOPOLOJİ
    // block, exactly as the reference draws it.
    std::string open_group;
    for (const SettingSpec& spec : core::builtin_settings().all()) {
        if (spec.section != section) continue;

        const std::string group = group_of(spec.id);
        if (group != open_group) {
            // THE STANDARD'S SECTION HEADING — small caps, a rule to the right
            // edge — and the TURKISH upper case: `toUpper()` without a locale
            // wrote `DUZENLEME` and `ACI` over `DÜZENLEME` and `AÇI` (CLAUDE.md
            // 5.6, ui.md P4).
            auto* caption = new FormSection(QLocale(QLocale::Turkish).toUpper(group_title(group)),
                                            QString(), page);
            caption->setContentsMargins(0, open_group.empty() ? 0 : 14, 0, 2);
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

QWidget* SettingsDialog::buildPrintProfiles()
{
    auto* block  = new QWidget(this);
    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 14);
    column->setSpacing(8);

    column->addWidget(new FormSection(
        QStringLiteral("YAZDIRMA PROFİLLERİ"),
        QStringLiteral("her satır bir kâğıt; ● olan varsayılandır ve Yazdır düğmesi onu kullanır"),
        block));

    // THE ONE TABLE (ui.md R29), over a plain item model: the rows are the
    // store's and nothing is edited in place — a cell that could be typed into
    // would be a second road to a profile, and the road is `YAZDIRMAPROFİLİ`.
    profiles_model_ = new QStandardItemModel(0, 5, block);
    profiles_model_->setHorizontalHeaderLabels({QStringLiteral("Ad"), QStringLiteral("Kâğıt"),
                                                QStringLiteral("Yön"), QStringLiteral("dpi"),
                                                QStringLiteral("Kenar")});
    profiles_table_ = new DataGrid(block);
    profiles_table_->setModel(profiles_model_);
    profiles_table_->setRowNumbers(false);
    profiles_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    profiles_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    profiles_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    profiles_table_->horizontalHeader()->setStretchLastSection(true);
    profiles_table_->setMinimumHeight(148);
    profiles_table_->setAccessibleName(QStringLiteral("Yazdırma profilleri"));
    profiles_table_->gridDelegate()->setTheme(theme());
    column->addWidget(profiles_table_);

    // ---- what the selected row can have done to it --------------------------
    auto* rowActions = new QWidget(block);
    auto* actionRow  = new QHBoxLayout(rowActions);
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    profile_default_ = new Button(ButtonRole::Secondary, QStringLiteral("Varsayılan yap"),
                                  Glyph::Check, rowActions);
    profile_remove_ =
        new Button(ButtonRole::Danger, QStringLiteral("Sil"), Glyph::Trash, rowActions);
    for (Button* b : {profile_default_, profile_remove_}) {
        b->setControlSize(ControlSize::Compact);
        b->setEnabled(false);
        actionRow->addWidget(b);
    }
    actionRow->addStretch(1);
    column->addWidget(rowActions);

    const auto selected = [this]() -> QString {
        const QModelIndexList rows = profiles_table_->selectionModel()->selectedRows();
        if (rows.isEmpty()) return {};
        return profiles_model_->item(rows.front().row(), 0)->data(Qt::UserRole + 1).toString();
    };
    connect(profiles_table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this, selected](const QItemSelection&, const QItemSelection&) {
                const bool any = !selected().isEmpty();
                profile_default_->setEnabled(any);
                profile_remove_->setEnabled(any &&
                                            controller_.printService().profiles().all().size() > 1);
            });
    connect(profile_default_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (name.isEmpty()) return;
        controller_.runLine(QStringLiteral("YAZDIRMAPROFİLİ islem=varsayilan ad=\"%1\"").arg(name),
                            command::Origin::Gui);
    });
    connect(profile_remove_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (name.isEmpty()) return;
        // A DANGER BUTTON ALWAYS ASKS (design.md: the role confirms), and a
        // profile is a thing somebody set up once and uses every week.
        if (QMessageBox::question(this, QStringLiteral("Profili sil"),
                                  QStringLiteral("'%1' profili silinsin mi?").arg(name)) !=
            QMessageBox::Yes)
            return;
        controller_.runLine(QStringLiteral("YAZDIRMAPROFİLİ islem=sil ad=\"%1\"").arg(name),
                            command::Origin::Gui);
    });

    // ---- and the row that adds one ------------------------------------------
    column->addWidget(new FormSection(QStringLiteral("YENİ PROFİL"), QString(), block));
    auto* form    = new QWidget(block);
    auto* formRow = new QHBoxLayout(form);
    formRow->setContentsMargins(0, 0, 0, 0);
    formRow->setSpacing(8);

    FieldSpec nameSpec   = field_of(FieldKind::Text);
    nameSpec.placeholder = QStringLiteral("profil adı");
    profile_name_        = new Field(nameSpec, form);
    profile_name_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    profile_name_->setAccessibleName(QStringLiteral("Profil adı"));
    formRow->addWidget(profile_name_, 2);

    profile_paper_ = new ComboBox(form);
    for (const char* paper : io::paper_names())
        profile_paper_->addItem(QString::fromUtf8(paper));
    profile_paper_->setCurrentText(QStringLiteral("A4"));
    profile_paper_->setAccessibleName(QStringLiteral("Kâğıt"));
    formRow->addWidget(profile_paper_, 1);

    profile_orientation_ = new Segment(form);
    profile_orientation_->addOption(QStringLiteral("Dikey"), QStringLiteral("Uzun kenar yukarı"));
    profile_orientation_->addOption(QStringLiteral("Yatay"), QStringLiteral("Uzun kenar yana"));
    profile_orientation_->setControlSize(ControlSize::Regular);
    profile_orientation_->setAccessibleName(QStringLiteral("Yön"));
    formRow->addWidget(profile_orientation_, 1);

    profile_dpi_ = new Field(number_of(72, 4800, QStringLiteral("dpi")), form);
    profile_dpi_->setValue(QStringLiteral("300"));
    profile_dpi_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    profile_dpi_->setAccessibleName(QStringLiteral("Çözünürlük"));
    formRow->addWidget(profile_dpi_, 1);

    profile_margin_ = new Field(number_of(0, 200, QStringLiteral("mm")), form);
    profile_margin_->setValue(QStringLiteral("10"));
    profile_margin_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    profile_margin_->setAccessibleName(QStringLiteral("Kenar boşluğu"));
    formRow->addWidget(profile_margin_, 1);

    auto* add = new Button(ButtonRole::Secondary, QStringLiteral("Ekle"), Glyph::Plus, form);
    connect(add, &QPushButton::clicked, this, [this] {
        const QString name = profile_name_->value().trimmed();
        if (name.isEmpty()) {
            profile_name_->setState(FieldState::Invalid);
            return;
        }
        profile_name_->setState(FieldState::Normal);
        // ONE LINE, and it is the line a script would type. An existing name
        // replaces that profile, which is what `ekle` means (print_profiles.hpp).
        controller_.runLine(QStringLiteral("YAZDIRMAPROFİLİ islem=ekle ad=\"%1\" kagit=%2 "
                                           "yon=%3 dpi=%4 kenar=%5")
                                .arg(name, profile_paper_->currentText(),
                                     profile_orientation_->current() == 1 ? QStringLiteral("yatay")
                                                                          : QStringLiteral("dikey"),
                                     profile_dpi_->value(), profile_margin_->value()),
                            command::Origin::Gui);
        profile_name_->setValue(QString());
    });
    formRow->addWidget(add);
    column->addWidget(form);

    auto* note = new QLabel(QStringLiteral("Profiller kullanıcı profilinizde tutulur: %1")
                                .arg(controller_.printService().profilesPath()),
                            block);
    note->setObjectName(QStringLiteral("formHelp"));
    note->setWordWrap(true);
    column->addWidget(note);

    // The store is the source: an edit made at the command line while this
    // window is open redraws the table (Article 1.2).
    connect(&controller_.printService(), &PrintService::profilesChanged, this,
            &SettingsDialog::refreshPrintProfiles);
    refreshPrintProfiles();
    return block;
}

void SettingsDialog::refreshPrintProfiles()
{
    if (profiles_model_ == nullptr) return;
    const io::PrintProfiles& store = controller_.printService().profiles();

    profiles_model_->removeRows(0, profiles_model_->rowCount());
    for (const io::PrintProfile& p : store.all()) {
        const QString name    = QString::fromStdString(p.name);
        const bool is_default = store.default_name() == p.name;
        QList<QStandardItem*> row;
        // The default is MARKED, not coloured: a state told by colour alone is
        // a state a colour-blind reader cannot read (design.md §13, ui.md R31).
        auto* first = new QStandardItem(is_default ? QStringLiteral("● %1").arg(name) : name);
        first->setData(name, Qt::UserRole + 1);
        row << first;
        row << new QStandardItem(QString::fromStdString(p.paper) +
                                 QStringLiteral(" %1×%2").arg(p.width_mm).arg(p.height_mm));
        row << new QStandardItem(p.landscape ? QStringLiteral("Yatay") : QStringLiteral("Dikey"));
        row << new QStandardItem(QString::number(p.dpi));
        row << new QStandardItem(QStringLiteral("%1 mm").arg(p.margin_mm));
        profiles_model_->appendRow(row);
    }
    profiles_table_->resizeColumnsToContents();
    profile_default_->setEnabled(false);
    profile_remove_->setEnabled(false);
}

QWidget* SettingsDialog::buildAgentServer()
{
    auto* block  = new QWidget(this);
    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 14);
    column->setSpacing(8);

    column->addWidget(new FormSection(
        tr("DİNLEYİCİ"), tr("yalnız bu makineden erişilir; ajana verilecek adres aşağıdadır"),
        block));

    mcp_state_ = new QLabel(block);
    mcp_state_->setObjectName(QStringLiteral("formRowLabel"));
    mcp_state_->setWordWrap(true);
    column->addWidget(mcp_state_);

    // THE ADDRESS, IN A READ-ONLY FIELD so it can be selected and copied but not
    // typed into: what is in it is assembled from the port and the listener's own
    // token, and a person editing it would be editing nothing.
    FieldSpec address = field_of(FieldKind::Text);
    address.frame     = FieldFrame::Box;
    mcp_address_      = new Field(address, block);
    mcp_address_->setState(FieldState::ReadOnly);
    mcp_address_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    column->addWidget(new FormRow(tr("Ajanın bağlanacağı adres"), mcp_address_, block));

    auto* actions = new QWidget(block);
    auto* row     = new QHBoxLayout(actions);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    mcp_toggle_ = new Button(ButtonRole::Secondary, tr("Başlat"), Glyph::Server, actions);
    mcp_token_ =
        new Button(ButtonRole::Secondary, tr("Yeni belirteç üret"), Glyph::Refresh, actions);
    mcp_token_->setToolTip(
        tr("Eski belirteç geçersiz olur; bağlı ajanların adresi yenilenmelidir."));
    mcp_copy_  = new Button(ButtonRole::Ghost, tr("Adresi kopyala"), Glyph::Copy, actions);
    mcp_probe_ = new Button(ButtonRole::Secondary, tr("Bağlantıyı sına"), Glyph::Plug, actions);
    mcp_probe_->setToolTip(tr("Bu adrese gerçek bir istek gönderir ve cevabı okur."));
    for (Button* b : {mcp_toggle_, mcp_probe_, mcp_token_, mcp_copy_}) {
        b->setControlSize(ControlSize::Compact);
        row->addWidget(b);
    }
    row->addStretch(1);
    column->addWidget(actions);

    mcp_note_ = new QLabel(block);
    mcp_note_->setObjectName(QStringLiteral("formHelp"));
    mcp_note_->setWordWrap(true);
    column->addWidget(mcp_note_);

    // EVERY BUTTON IS THE COMMAND. The menu entry, the status cell and these
    // three all run `MCPSUNUCU`, so the listener has exactly one road in and a
    // script can take it (Article 1.2, CLAUDE.md 5.15).
    connect(mcp_toggle_, &QPushButton::clicked, this, [this] {
#if KENTOS_HAVE_MCP
        const McpService* server = controller_.mcpService();
        controller_.runLine(server != nullptr && server->listening()
                                ? QStringLiteral("MCPSUNUCU islem=durdur")
                                : QStringLiteral("MCPSUNUCU islem=baslat"),
                            command::Origin::Gui);
#endif
        refreshAgentServer();
    });
    connect(mcp_token_, &QPushButton::clicked, this, [this] {
        controller_.runLine(QStringLiteral("MCPSUNUCU islem=belirtec"), command::Origin::Gui);
        refreshAgentServer();
    });
    connect(mcp_copy_, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(mcp_address_->value());
        mcp_note_->setText(tr("Adres panoya kopyalandı. Belirteci bir sohbete, bir hata "
                              "bildirimine ya da paylaşılan bir dosyaya yapıştırmayın."));
    });
    // SINAMA DA BİR KOMUTTUR. It opens a real socket to this listener and reads
    // what comes back, which is the half `ai::McpServer` cannot test about
    // itself: that the port is bound and that the address on this page is the
    // one the listener answers to.
    connect(mcp_probe_, &QPushButton::clicked, this, [this] {
        controller_.runLine(QStringLiteral("MCPSUNUCU islem=sina"), command::Origin::Gui);
        refreshAgentServer();
    });

    // ---- who has been using it ----------------------------------------------
    column->addWidget(new FormSection(
        tr("İSTEMCİLER"),
        tr("bu sunucuya konuşmuş ajanlar; bu protokol sürümünde oturum yoktur, liste "
           "“kim konuştu ve ne zaman” demektir"),
        block));

    mcp_scope_ = new QLabel(block);
    mcp_scope_->setObjectName(QStringLiteral("formHelp"));
    mcp_scope_->setWordWrap(true);
    column->addWidget(mcp_scope_);

    mcp_clients_model_ = new QStandardItemModel(0, 5, block);
    mcp_clients_model_->setHorizontalHeaderLabels(
        {tr("İstemci"), tr("Çağrı"), tr("Öneri"), tr("Son görülme"), tr("Son hata")});
    mcp_clients_ = new DataGrid(block);
    mcp_clients_->setModel(mcp_clients_model_);
    mcp_clients_->setRowNumbers(false);
    mcp_clients_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mcp_clients_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mcp_clients_->setSelectionMode(QAbstractItemView::SingleSelection);
    mcp_clients_->horizontalHeader()->setStretchLastSection(true);
    // A COLUMN NEVER NARROWER THAN ITS OWN HEADING. `resizeColumnsToContents`
    // measures the CELLS, so a table with one short label in it squeezed
    // "İstemci", "Çağrı" and "Son görülme" down to "İstem…", "Çağ…", "Son
    // görül…" — three headings a person cannot read, over data they can.
    mcp_clients_->horizontalHeader()->setMinimumSectionSize(92);
    // THE HEIGHT FOLLOWS THE LIST, up to six rows. A fixed 120 pixels put a
    // hand's width of empty grid under a single client — which reads as a
    // table that failed to load rather than as one with one row in it.
    mcp_clients_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mcp_clients_->setAccessibleName(tr("MCP istemcileri"));
    mcp_clients_->gridDelegate()->setTheme(theme());
    column->addWidget(mcp_clients_);

    // THE EMPTY STATE IS A SENTENCE, NOT AN EMPTY BOX. A fresh installation has
    // no clients and will have none until somebody points an agent at the
    // address above; a 120-pixel grid of nothing says "broken", and a line that
    // explains what will appear there says what is true.
    mcp_empty_ = new QLabel(tr("Bu sunucuya henüz hiçbir istemci konuşmadı. Bağlanan her ajan "
                               "burada adı, çağrı sayısı, açtığı öneri sayısı ve son hatasıyla "
                               "görünür."),
                            block);
    mcp_empty_->setObjectName(QStringLiteral("formHelp"));
    mcp_empty_->setWordWrap(true);
    column->addWidget(mcp_empty_);

    auto* clientActions = new QWidget(block);
    auto* clientRow     = new QHBoxLayout(clientActions);
    clientRow->setContentsMargins(0, 0, 0, 0);
    clientRow->setSpacing(8);
    mcp_revoke_ =
        new Button(ButtonRole::Danger, tr("Yetkisini kaldır"), Glyph::Stop, clientActions);
    mcp_revoke_->setToolTip(tr("Yalnız bu istemci durur; belirteç değişmez, diğer ajanlar "
                               "çalışmaya devam eder."));
    mcp_allow_ =
        new Button(ButtonRole::Secondary, tr("Yetkiyi geri ver"), Glyph::Check, clientActions);
    for (Button* b : {mcp_revoke_, mcp_allow_}) {
        b->setControlSize(ControlSize::Compact);
        b->setEnabled(false);
        clientRow->addWidget(b);
    }
    clientRow->addStretch(1);
    column->addWidget(clientActions);
    mcp_actions_ = clientActions;

    const auto chosenClient = [this]() -> QString {
        if (mcp_clients_->selectionModel() == nullptr) return {};
        const QModelIndexList rows = mcp_clients_->selectionModel()->selectedRows();
        if (rows.isEmpty()) return {};
        return mcp_clients_model_->item(rows.front().row(), 0)->data(Qt::UserRole + 1).toString();
    };
    connect(mcp_clients_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) { refreshAgentClients(); });
    // BOTH BUTTONS RUN THE COMMAND, so a script can shut an agent out too
    // (Article 1.2, CLAUDE.md 5.15).
    connect(mcp_revoke_, &QPushButton::clicked, this, [this, chosenClient] {
        const QString who = chosenClient();
        if (who.isEmpty()) return;
        controller_.runLine(QStringLiteral("MCPSUNUCU islem=iptal ad=\"%1\"").arg(who),
                            command::Origin::Gui);
        refreshAgentClients();
    });
    connect(mcp_allow_, &QPushButton::clicked, this, [this, chosenClient] {
        const QString who = chosenClient();
        if (who.isEmpty()) return;
        controller_.runLine(QStringLiteral("MCPSUNUCU islem=izin ad=\"%1\"").arg(who),
                            command::Origin::Gui);
        refreshAgentClients();
    });

#if KENTOS_HAVE_MCP
    if (McpService* server = controller_.mcpService(); server != nullptr) {
        connect(server, &McpService::stateChanged, this, &SettingsDialog::refreshAgentServer);
        connect(server, &McpService::stateChanged, this, &SettingsDialog::refreshAgentClients);
        connect(server, &McpService::clientsChanged, this, &SettingsDialog::refreshAgentClients);
    }
#endif

    refreshAgentServer();
    refreshAgentClients();
    return block;
}

void SettingsDialog::refreshAgentClients()
{
    if (mcp_clients_model_ == nullptr) return;

#if KENTOS_HAVE_MCP
    McpService* server = controller_.mcpService();
    if (server == nullptr) {
        mcp_clients_model_->removeRows(0, mcp_clients_model_->rowCount());
        mcp_scope_->setText(tr("Bu yapıda MCP sunucusu yok."));
        mcp_clients_->setVisible(false);
        mcp_actions_->setVisible(false);
        mcp_empty_->setVisible(false);
        mcp_revoke_->setEnabled(false);
        mcp_allow_->setEnabled(false);
        return;
    }

    // WHAT EVERY CLIENT MAY DO, said once above the table rather than per row:
    // the scope is the same for all of them today, and pretending otherwise with
    // an identical column would be inventing a distinction that does not exist.
    const ai::Catalog& catalogue = controller_.aiService().catalog();
    mcp_scope_->setText(tr("Her istemci aynı kapsamda çalışır: %1 araçtan %2 tanesi çizimi "
                           "değiştirir ve her biri önizlemeli bir öneriye dönüşür — uygulayan "
                           "bilgisayar başındaki kişidir. Tutamaklar ve öneriler istemciye "
                           "özeldir: hiçbir ajan bir başkasınınkini kullanamaz.")
                            .arg(catalogue.tools.size())
                            .arg(catalogue.mutating_count()));

    const QString wasChosen =
        mcp_clients_->selectionModel() != nullptr &&
                !mcp_clients_->selectionModel()->selectedRows().isEmpty()
            ? mcp_clients_model_
                  ->item(mcp_clients_->selectionModel()->selectedRows().front().row(), 0)
                  ->data(Qt::UserRole + 1)
                  .toString()
            : QString();

    mcp_clients_model_->removeRows(0, mcp_clients_model_->rowCount());
    const std::vector<ai::ClientRecord> held = server->clients().clients();
    mcp_clients_->setVisible(!held.empty());
    mcp_actions_->setVisible(!held.empty());
    mcp_empty_->setVisible(held.empty());
    for (const ai::ClientRecord& one : held) {
        const QString label = QString::fromStdString(one.label);
        QList<QStandardItem*> row;
        // REVOKED IS SAID IN WORDS, never by colour alone (ui.md R31).
        auto* first = new QStandardItem(one.revoked ? tr("%1  [YETKİSİZ]").arg(label) : label);
        first->setData(label, Qt::UserRole + 1);
        row << first;
        row << new QStandardItem(
            one.refusals != 0
                ? tr("%1 (%2 ret)").arg(QString::number(one.calls), QString::number(one.refusals))
                : QString::number(one.calls));
        row << new QStandardItem(QString::number(one.plans));
        row << new QStandardItem(
            one.last_seen != 0 ? QDateTime::fromSecsSinceEpoch(static_cast<qint64>(one.last_seen))
                                     .toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))
                               : tr("—"));
        row << new QStandardItem(QString::fromStdString(one.last_refusal));
        mcp_clients_model_->appendRow(row);
    }
    mcp_clients_->resizeColumnsToContents();

    // Two rows' worth at least, so the empty band under one client is not a
    // hole; six at most, so a busy machine scrolls instead of pushing the
    // settings below it off the page.
    const int rows   = std::clamp(mcp_clients_model_->rowCount(), 2, 6);
    const int perRow = mcp_clients_->verticalHeader()->defaultSectionSize();
    mcp_clients_->setFixedHeight(mcp_clients_->horizontalHeader()->height() + rows * perRow +
                                 2 * mcp_clients_->frameWidth());

    // THE SELECTION SURVIVES A REDRAW. The table is rebuilt on every request the
    // server serves, and a person half way through deciding to shut an agent out
    // must not have the row pulled out from under the cursor.
    bool revoked = false;
    bool any     = false;
    for (int r = 0; r < mcp_clients_model_->rowCount(); ++r) {
        if (mcp_clients_model_->item(r, 0)->data(Qt::UserRole + 1).toString() != wasChosen)
            continue;
        mcp_clients_->selectRow(r);
        any     = true;
        revoked = server->clients().revoked(wasChosen.toStdString());
        break;
    }
    mcp_revoke_->setEnabled(any && !revoked);
    mcp_allow_->setEnabled(any && revoked);
#else
    mcp_clients_model_->removeRows(0, mcp_clients_model_->rowCount());
    mcp_scope_->setText(tr("Bu yapıda MCP sunucusu yok."));
    mcp_clients_->setVisible(false);
    mcp_actions_->setVisible(false);
    mcp_empty_->setVisible(false);
    mcp_revoke_->setEnabled(false);
    mcp_allow_->setEnabled(false);
#endif
}

void SettingsDialog::refreshAgentServer()
{
    if (mcp_state_ == nullptr) return;

#if KENTOS_HAVE_MCP
    McpService* server = controller_.mcpService();
    if (server == nullptr) {
        mcp_state_->setText(tr("Bu yapıda MCP sunucusu yok."));
        mcp_address_->setValue(QString());
        for (Button* b : {mcp_toggle_, mcp_token_, mcp_copy_, mcp_probe_})
            b->setEnabled(false);
        mcp_note_->setText(tr("Sunucu KENTOS_WITH_MCP seçeneğiyle derlenir."));
        return;
    }

    const bool up = server->listening();
    mcp_toggle_->setText(up ? tr("Durdur") : tr("Başlat"));
    mcp_copy_->setEnabled(up);
    mcp_probe_->setEnabled(up);

    if (!up) {
        mcp_state_->setText(tr("Sunucu kapalı. Açılana kadar hiçbir ajan bu çizime erişemez."));
        mcp_address_->setValue(QString());
        mcp_note_->setText(tr("Port ve belirteç zorunluluğu aşağıdaki ayarlardan gelir; "
                              "sunucu bir sonraki başlatmada onları okur."));
        return;
    }

    // THE ADDRESS IS THE TOKENED FORM when a token is required, because that is
    // the one a client can use without setting a header. It is shown ONCE, here,
    // in a field the person copies from; everything else in the program —
    // the transcript, the status cell, the audit record — sees the fingerprint
    // and never the token (CLAUDE.md 5.21).
    const QString base = QStringLiteral("http://127.0.0.1:%1/mcp").arg(server->port());
    if (server->tokenRequired()) {
        mcp_state_->setText(tr("Sunucu açık — port %1, belirteç zorunlu (%2).")
                                .arg(server->port())
                                .arg(server->tokenFingerprint()));
        mcp_address_->setValue(base + QStringLiteral("/") + server->token());
        mcp_note_->setText(tr("Bu adres bir paroladır: belirteci taşır. İstemci isterse "
                              "belirteci 'Authorization: Bearer' başlığında da verebilir; o "
                              "zaman adres %1 olur.")
                               .arg(base));
    } else {
        // AN OPEN ENDPOINT IS SAID IN WORDS, on the page where it was turned off.
        mcp_state_->setText(
            tr("Sunucu açık — port %1, KORUMASIZ: belirteç istenmiyor.").arg(server->port()));
        mcp_address_->setValue(base);
        mcp_note_->setText(tr("Belirteç zorunluluğu kapalı olduğu için bu makinedeki HER program "
                              "çizimi okuyabilir ve öneri açabilir. Aşağıdaki 'belirteç zorunlu' "
                              "ayarını açıp sunucuyu yeniden başlatın."));
    }
#else
    mcp_state_->setText(tr("Bu yapıda MCP sunucusu yok."));
    mcp_address_->setValue(QString());
    for (Button* b : {mcp_toggle_, mcp_token_, mcp_copy_, mcp_probe_})
        b->setEnabled(false);
    mcp_note_->setText(tr("Sunucu KENTOS_WITH_MCP seçeneğiyle derlenir."));
#endif
}

QWidget* SettingsDialog::buildProviderProfiles()
{
    auto* block  = new QWidget(this);
    auto* column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 14);
    column->setSpacing(8);

    column->addWidget(new FormSection(
        tr("YAPAY ZEKA MODELLERİ"),
        tr("her satır bir model uç noktası; ● olan varsayılandır ve sohbet onu kullanır"), block));

    // THE ONE TABLE (ui.md R29), over a plain item model, and nothing is edited
    // in place: a cell that could be typed into would be a second road to a
    // profile, and the road is `YAPAYZEKAMODELİ`.
    providers_model_ = new QStandardItemModel(0, 4, block);
    providers_model_->setHorizontalHeaderLabels({tr("Ad"), tr("Lehçe"), tr("Model"), tr("Bağlam")});
    providers_table_ = new DataGrid(block);
    providers_table_->setModel(providers_model_);
    providers_table_->setRowNumbers(false);
    providers_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    providers_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    providers_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    providers_table_->horizontalHeader()->setStretchLastSection(true);
    providers_table_->setMinimumHeight(148);
    providers_table_->setAccessibleName(tr("Yapay zeka model profilleri"));
    providers_table_->gridDelegate()->setTheme(theme());
    column->addWidget(providers_table_);

    // ---- what the selected row can have done to it --------------------------
    auto* rowActions = new QWidget(block);
    auto* actionRow  = new QHBoxLayout(rowActions);
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    provider_edit_ = new Button(ButtonRole::Secondary, tr("Düzenle"), Glyph::Pencil, rowActions);
    provider_default_ =
        new Button(ButtonRole::Secondary, tr("Varsayılan yap"), Glyph::Check, rowActions);
    provider_test_ =
        new Button(ButtonRole::Secondary, tr("Bağlantıyı dene"), Glyph::Cloud, rowActions);
    provider_test_->setToolTip(tr("Uç noktaya tek sözcüklük bir istek gönderir; sonuç komut "
                                  "dökümüne yazılır."));
    provider_remove_ = new Button(ButtonRole::Danger, tr("Sil"), Glyph::Trash, rowActions);
    for (Button* b : {provider_edit_, provider_default_, provider_test_, provider_remove_}) {
        b->setControlSize(ControlSize::Compact);
        b->setEnabled(false);
        actionRow->addWidget(b);
    }
    actionRow->addStretch(1);
    column->addWidget(rowActions);

    /// The profile name of the selected row, or empty when nothing is selected.
    const auto selected = [this]() -> QString {
        const QModelIndexList rows = providers_table_->selectionModel()->selectedRows();
        if (rows.isEmpty()) return {};
        return providers_model_->item(rows.front().row(), 0)->data(Qt::UserRole + 1).toString();
    };

    connect(providers_table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this, selected](const QItemSelection&, const QItemSelection&) {
                const QString name                = selected();
                const ai::ProviderProfiles& store = controller_.providerService().profiles();
                const ai::ProviderProfile* p      = store.find(name.toStdString());
                provider_edit_->setEnabled(p != nullptr);
                provider_default_->setEnabled(p != nullptr);
                provider_test_->setEnabled(p != nullptr);
                // The last profile cannot go (`ProviderProfiles::remove`): a
                // chat with no endpoint cannot be configured back into existence
                // from inside itself, so the button says so by being off.
                provider_remove_->setEnabled(p != nullptr && store.all().size() > 1);

                // WHICH ENTRY THE KEY FIELD WOULD WRITE TO, in words, before
                // anything is typed. A secret field that does not say where its
                // value goes is a secret field nobody should type into.
                const QString ref = p == nullptr ? QString() : QString::fromStdString(p->key_ref);
                provider_key_->setEnabled(!ref.isEmpty());
                provider_key_save_->setEnabled(!ref.isEmpty());
                if (p == nullptr)
                    provider_key_note_->setText(tr("Anahtarı girmek için bir profil seçin."));
                else if (ref.isEmpty())
                    provider_key_note_->setText(
                        tr("'%1' profilinin anahtar adı yok: yerel bir model anahtar "
                           "istemez. Bulut için önce anahtar adı verin.")
                            .arg(name));
                else
                    provider_key_note_->setText(
                        tr("Anahtar '%1' kaydına yazılacak. %2").arg(ref, SecretStore::describe()));
            });

    connect(provider_edit_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (!name.isEmpty()) openProviderDialog(name);
    });
    // A DOUBLE CLICK OPENS IT TOO, because a table of records every other window
    // in this program edits that way would be the one table that does not.
    connect(providers_table_, &QAbstractItemView::doubleClicked, this,
            [this, selected](const QModelIndex&) {
                const QString name = selected();
                if (!name.isEmpty()) openProviderDialog(name);
            });

    connect(provider_default_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (name.isEmpty()) return;
        controller_.runLine(
            QStringLiteral("YAPAYZEKAMODELİ islem=varsayilan ad=%1").arg(quoted(name)),
            command::Origin::Gui);
    });
    connect(provider_test_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (name.isEmpty()) return;
        // THE ANSWER ARRIVES ON THE TRANSCRIPT, not here: the request is
        // asynchronous and this window must not wait on a model (ai.md P8).
        controller_.runLine(QStringLiteral("YAPAYZEKAMODELİ islem=dene ad=%1").arg(quoted(name)),
                            command::Origin::Gui);
    });
    connect(provider_remove_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        if (name.isEmpty()) return;
        // A DANGER BUTTON ALWAYS ASKS (design.md: the role confirms), and the
        // question says what is NOT removed: the key stays in the key store,
        // because it may well be the same key another profile uses.
        if (QMessageBox::question(this, tr("Model profilini sil"),
                                  tr("'%1' profili silinsin mi? Anahtar deposundaki kayıt "
                                     "olduğu gibi kalır.")
                                      .arg(name)) != QMessageBox::Yes)
            return;
        controller_.runLine(QStringLiteral("YAPAYZEKAMODELİ islem=sil ad=%1").arg(quoted(name)),
                            command::Origin::Gui);
    });

    // ---- adding and editing, both in the one window -------------------------
    //
    // WAS A ROW OF FIVE FIELDS at the foot of the table, and the row is gone.
    // A profile has fourteen fields and the row could express five; the nine it
    // dropped — the output cap, the temperature, the context window, the
    // thinking knob, the vendor's mandatory headers — are the ones that decide
    // whether the endpoint answers at all. `ProviderDialog` carries all of them
    // and fills the model list from the catalogue, so a model is CHOSEN rather
    // than spelled (provider_dialog.hpp).
    auto* addRow    = new QWidget(block);
    auto* addLayout = new QHBoxLayout(addRow);
    addLayout->setContentsMargins(0, 0, 0, 0);
    addLayout->setSpacing(8);
    auto* newProfile = new Button(ButtonRole::Secondary, tr("Yeni profil…"), Glyph::Plus, addRow);
    newProfile->setControlSize(ControlSize::Compact);
    connect(newProfile, &QPushButton::clicked, this, [this] { openProviderDialog(QString()); });
    addLayout->addWidget(newProfile);
    addLayout->addStretch(1);
    column->addWidget(addRow);

    // ---- the key, and it is the one value that is NOT a command --------------
    // CLAUDE.md 5.21: a credential may not be in a command argument, a journal
    // line, a log line or a transcript. Every other control on this page writes
    // through the bus precisely so the journal records it; this one must not, so
    // it writes to the operating system's key store and the profile keeps only
    // the NAME of the entry (`secret_store.hpp`).
    column->addWidget(new FormSection(tr("API ANAHTARI"), QString(), block));
    auto* keyRow    = new QWidget(block);
    auto* keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(8);

    FieldSpec keySpec   = field_of(FieldKind::Text);
    keySpec.placeholder = tr("anahtarı yapıştırın");
    keySpec.secret      = true; // dots on screen, never echoed (ui.md R38's rule)
    keySpec.glyph       = Glyph::Lock;
    provider_key_       = new Field(keySpec, keyRow);
    provider_key_->setFixedHeight(static_cast<int>(ControlSize::Regular));
    provider_key_->setAccessibleName(tr("API anahtarı"));
    provider_key_->setEnabled(false);
    keyLayout->addWidget(provider_key_, 3);

    provider_key_save_ =
        new Button(ButtonRole::Secondary, tr("Anahtarı kaydet"), Glyph::Lock, keyRow);
    provider_key_save_->setEnabled(false);
    keyLayout->addWidget(provider_key_save_);
    keyLayout->addStretch(1);
    column->addWidget(keyRow);

    provider_key_note_ = new QLabel(tr("Anahtarı girmek için bir profil seçin."), block);
    provider_key_note_->setObjectName(QStringLiteral("formHelp"));
    provider_key_note_->setWordWrap(true);
    column->addWidget(provider_key_note_);

    connect(provider_key_save_, &QPushButton::clicked, this, [this, selected] {
        const QString name = selected();
        const ai::ProviderProfile* p =
            controller_.providerService().profiles().find(name.toStdString());
        if (p == nullptr || p->key_ref.empty()) return;
        const QString secret = provider_key_->value();
        if (secret.isEmpty()) {
            provider_key_->setState(FieldState::Invalid);
            return;
        }
        const QString ref      = QString::fromStdString(p->key_ref);
        const core::Status put = controller_.providerService().secrets().write(ref, secret);
        // CLEARED WHETHER IT WORKED OR NOT. A key left sitting in a field is a
        // key on somebody's screen, and the failure message says what to do
        // instead (an environment variable, when this build has no key store).
        provider_key_->setValue(QString());
        provider_key_->setState(FieldState::Normal);
        if (!put) {
            QMessageBox::warning(this, tr("Anahtar kaydedilemedi"),
                                 QString::fromStdString(put.error().message));
            return;
        }
        provider_key_note_->setText(
            tr("Anahtar '%1' kaydına yazıldı. %2").arg(ref, SecretStore::describe()));
    });

    auto* note =
        new QLabel(tr("Profiller kullanıcı profilinizde tutulur: %1\nAnahtarlar programın "
                      "dosyalarında değil, işletim sisteminde durur: %2")
                       .arg(controller_.providerService().profilesPath(), SecretStore::describe()),
                   block);
    note->setObjectName(QStringLiteral("formHelp"));
    note->setWordWrap(true);
    column->addWidget(note);

    // The store is the source: a profile added at the command line while this
    // window is open redraws the table (Article 1.2).
    connect(&controller_.providerService(), &ProviderService::profilesChanged, this,
            &SettingsDialog::refreshProviderProfiles);
    refreshProviderProfiles();
    return block;
}

void SettingsDialog::openProviderDialog(const QString& edit)
{
    ProviderDialog window(controller_, edit, this);
    window.applyTheme(theme());
    (void)window.exec();
    // Nothing to do on the way out: the window wrote through the bus, and the
    // table follows `ProviderService::profilesChanged` like every other reader.
}

void SettingsDialog::refreshProviderProfiles()
{
    if (providers_model_ == nullptr) return;
    const ai::ProviderProfiles& store = controller_.providerService().profiles();

    providers_model_->removeRows(0, providers_model_->rowCount());
    for (const ai::ProviderProfile& p : store.all()) {
        const QString name    = QString::fromStdString(p.name);
        const bool is_default = store.default_name() == p.name;
        QList<QStandardItem*> row;
        // The default is MARKED, not coloured: a state told by colour alone is a
        // state a colour-blind reader cannot read (design.md §13, ui.md R31).
        auto* first = new QStandardItem(is_default ? QStringLiteral("● %1").arg(name) : name);
        first->setData(name, Qt::UserRole + 1);
        row << first;
        row << new QStandardItem(QString::fromUtf8(ai::dialect_id(p.dialect)));
        row << new QStandardItem(QString::fromStdString(p.model));
        // WHO SAID SO, beside the number. "The server reported 128 000" and
        // "somebody typed 128 000" are not the same fact (`ai::ContextSource`),
        // and a window that printed only the number would hide the difference.
        row << new QStandardItem(
            p.context.tokens == 0
                ? tr("bilinmiyor")
                : tr("%1 (%2)").arg(QLocale(QLocale::Turkish).toString(qlonglong{p.context.tokens}),
                                    QString::fromUtf8(ai::context_source_label(p.context.source))));
        providers_model_->appendRow(row);
    }
    providers_table_->resizeColumnsToContents();

    // Nothing is selected after a rebuild, so nothing may be acted on. The
    // selection handler turns these back on.
    provider_edit_->setEnabled(false);
    provider_default_->setEnabled(false);
    provider_test_->setEnabled(false);
    provider_remove_->setEnabled(false);
    provider_key_->setEnabled(false);
    provider_key_save_->setEnabled(false);
    provider_key_note_->setText(tr("Anahtarı girmek için bir profil seçin."));
}

void SettingsDialog::showSection(const QString& title)
{
    for (int index = 0; index < static_cast<int>(order_.size()); ++index) {
        if (order_[static_cast<std::size_t>(index)].title != title) continue;
        // Through the list's own signal, so the heading, the page and the
        // highlighted row cannot disagree — the same road a click takes.
        sections_->setCurrent(index);
        emit sections_->currentChanged(index);
        return;
    }
}

QStringList SettingsDialog::probeSections() const
{
    QStringList out;
    for (const Section& section : order_)
        out << section.title;
    return out;
}

QStringList SettingsDialog::probeProjectSettings() const
{
    QStringList out;
    for (const SettingSpec& spec : core::builtin_settings().all())
        if (spec.scope == SettingScope::Project) out << QString::fromStdString(spec.id);
    return out;
}

QWidget* SettingsDialog::buildProjectPage()
{
    auto* page   = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 12, 12);
    layout->setSpacing(0);

    // GROUPED BY THE TOPIC THEY WERE DECLARED UNDER, so a reader who knows a
    // setting from its own page finds it in the same company here.
    std::string open_section;
    for (const SettingSpec& spec : core::builtin_settings().all()) {
        if (spec.scope != SettingScope::Project) continue;

        if (spec.section != open_section) {
            auto* caption = new FormSection(
                QLocale(QLocale::Turkish).toUpper(QString::fromStdString(spec.section)), QString(),
                page);
            caption->setContentsMargins(0, open_section.empty() ? 0 : 14, 0, 2);
            layout->addWidget(caption);
            open_section = spec.section;
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
    auto* line = new QWidget(into->parentWidget());
    // §10's row: a 1 px rule under each, drawn by the sheet.
    line->setObjectName(QStringLiteral("settingRow"));
    line->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 9, 0, 9);
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
        // THE READABLE NAME IS SHOWN, THE VALUE NAME IS WRITTEN (`SettingSpec::
        // labels`): the person reads "Her değişiklikte onay iste", and TERCİH,
        // the journal and a script keep `her_degisiklikte`.
        auto* box = new ComboBox(line);
        for (std::size_t i = 0; i < spec.values.size(); ++i) {
            const std::string& shown =
                i < spec.labels.size() && !spec.labels[i].empty() ? spec.labels[i] : spec.values[i];
            box->addItem(QString::fromStdString(shown), QString::fromStdString(spec.values[i]));
        }
        connect(box, &QComboBox::currentIndexChanged, this, [this, &spec, box](int index) {
            if (!loading_ && index >= 0) write(spec, box->itemData(index).toString());
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
            connect(button, &QToolButton::clicked, this, [this, &spec] {
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

        // The standard's number input — mono digits, the unit dim at the right
        // edge — with the setting's declared bounds as its validator. The spin
        // box it replaces had arrows nobody could hit at row height and a suffix
        // glued to the number.
        const long long low  = spec.range.bounded() ? static_cast<long long>(spec.range.min)
                                                    : -static_cast<long long>(kLengthCeiling);
        const long long high = spec.range.bounded() ? static_cast<long long>(spec.range.max)
                                                    : static_cast<long long>(kLengthCeiling);
        auto* box = new Field(number_of(low, high, QString::fromStdString(spec.unit)), line);
        box->setFixedHeight(static_cast<int>(ControlSize::Regular));
        box->setAccessibleName(row_label(spec));
        // On commit — Enter, or focus leaving — not on every keystroke: a control
        // that reported each digit would dispatch a command per keystroke and
        // fill the journal with every number on the way to the one meant.
        connect(box, &Field::committed, this, [this, &spec](const QString& text) {
            if (!loading_ && !text.isEmpty()) write(spec, text);
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
            if (auto* field = qobject_cast<Field*>(row.editor))
                field->setValue(QString::number(value.as_int()));
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

    // §10's orange dot beside a category that departs from its defaults, so the
    // list says where the changes are before a page is opened.
    if (sections_ != nullptr) {
        for (int i = 0; i < static_cast<int>(order_.size()); ++i) {
            bool departed = false;
            for (const Row& row : rows_) {
                if (QString::fromStdString(row.spec->section) !=
                    order_[static_cast<std::size_t>(i)].title)
                    continue;
                if (storeOf(row.spec->scope).is_explicit(row.spec->id)) {
                    departed = true;
                    break;
                }
            }
            sections_->setMarked(i, departed);
        }
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
