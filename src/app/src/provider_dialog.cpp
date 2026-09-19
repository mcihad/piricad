// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/provider_dialog.hpp"

#include "kentos_cad/app/ai_transport.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/provider_service.hpp"
#include "kentos_cad/app/secret_store.hpp"

#include "kentos_cad/ai/redact.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

/// A value the parser will read back as one token. The same helper the settings
/// page uses; duplicated rather than shared because it is four lines and the
/// alternative is a header that exists to hold four lines.
QString quoted(const QString& raw)
{
    QString out = raw;
    out.replace('\\', QStringLiteral("\\\\"));
    out.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(out);
}

/// `128 b` — a context window as the combo prints it beside a model id.
QString windowCaption(std::int64_t tokens)
{
    if (tokens <= 0) return {};
    if (tokens < 1000) return QString::number(tokens);
    return QStringLiteral("%1 b").arg(tokens / 1000);
}

/// The reasoning words the dialog offers, in `ai::ReasoningMode` order.
QString reasoningCaption(ai::ReasoningMode mode)
{
    switch (mode) {
    case ai::ReasoningMode::None: return ProviderDialog::tr("kapalı");
    case ai::ReasoningMode::Effort: return ProviderDialog::tr("çaba (reasoning_effort)");
    case ai::ReasoningMode::ResponsesSummary: return ProviderDialog::tr("Responses özeti");
    case ai::ReasoningMode::AnthropicBudget: return ProviderDialog::tr("Anthropic bütçesi");
    case ai::ReasoningMode::QwenBudget: return ProviderDialog::tr("Qwen bütçesi");
    }
    return {};
}

} // namespace

/// One model listing in flight.
///
/// THE SINK OUTLIVES THE REQUEST, for the reason `ProviderService::Probe` gives:
/// the transport holds it by reference and promises one `on_finished`, and a
/// dialog the user closed mid-fetch must not take the sink with it.
struct ProviderDialog::Listing : ai::StreamSink
{
    explicit Listing(ProviderDialog* owner) : dialog(owner) {}

    ~Listing() override
    {
        if (handle && !done) handle->cancel();
    }

    void on_chunk(std::string_view bytes) override { body += bytes; }

    void on_finished(int status, std::string_view error) override
    {
        done = true;
        if (dialog == nullptr) return; // the window went; nobody is waiting
        dialog->finishListing(status, QString::fromStdString(std::string(error)), body);
    }

    ProviderDialog* dialog;
    std::string body;
    std::shared_ptr<ai::Cancellation> handle;
    bool done{false};
};

ProviderDialog::ProviderDialog(Controller& controller, const QString& edit, QWidget* parent)
    : DialogFrame(parent), controller_(controller), editing_(edit)
{
    setHeading(Glyph::Ai, edit.isEmpty() ? tr("Yeni Model Profili") : tr("Model Profili"),
               edit.isEmpty() ? QString() : QStringLiteral("— %1").arg(edit));
    setBody(buildBody());

    auto* cancel = new Button(ButtonRole::Secondary, tr("İptal"), std::nullopt, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    footer()->addWidget(cancel);

    auto* save = new Button(ButtonRole::Primary, tr("Kaydet"), Glyph::Check, this);
    connect(save, &QPushButton::clicked, this, &ProviderDialog::save);
    footer()->addWidget(save);

    // AN EXISTING PROFILE IS LOADED OVER THE TEMPLATE, so editing one shows what
    // it actually is rather than what its vendor's defaults are.
    if (!editing_.isEmpty()) {
        const ai::ProviderProfiles& store = controller_.providerService().profiles();
        if (const ai::ProviderProfile* p = store.find(editing_.toStdString()); p != nullptr) {
            load(*p);

            // THE TEMPLATE FOLLOWS THE PROFILE, and it is set with the signal
            // BLOCKED: letting `currentIndexChanged` fire here would call
            // `applyVendor` and overwrite the profile just loaded with the
            // vendor's defaults — the user's own address, model and caps
            // replaced by the catalogue's, on opening the window.
            const ai::ProviderCatalog& catalog = controller_.providerService().catalog();
            if (const ai::CatalogVendor* v = catalog.for_profile(*p); v != nullptr) {
                const QSignalBlocker quiet(vendor_);
                const int at = vendor_->findData(QString::fromStdString(v->id));
                if (at >= 0) vendor_->setCurrentIndex(at);
                note(v->docs_url.empty()
                         ? QString()
                         : tr("Belgeler: %1").arg(QString::fromStdString(v->docs_url)));
            } else {
                note(tr("Bu adres katalogda yok; model listesi elle yazılır."), Tone::Warn);
            }
        }
    }
    applyTheme(theme());
}

ProviderDialog::~ProviderDialog()
{
    // The sink may outlive this window; tell it there is nobody to answer.
    if (listing_) listing_->dialog = nullptr;
}

QWidget* ProviderDialog::buildBody()
{
    auto* body   = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(20, 16, 20, 12);
    column->setSpacing(10);

    const auto field = [body](FieldSpec spec, const QString& label) {
        auto* f = new Field(spec, body);
        f->setFixedHeight(static_cast<int>(ControlSize::Regular));
        f->setAccessibleName(label);
        return f;
    };
    const auto text = [&field](const QString& placeholder, const QString& label) {
        FieldSpec spec   = field_of(FieldKind::Text);
        spec.placeholder = placeholder;
        return field(spec, label);
    };

    // ---- which vendor ------------------------------------------------------
    column->addWidget(
        new FormSection(tr("SAĞLAYICI"), tr("bir şablon seçin, sonra gerekirse düzeltin"), body));

    vendor_ = new ComboBox(body);
    vendor_->setAccessibleName(tr("Sağlayıcı şablonu"));
    // THE LIST IS THE CATALOGUE, not a list kept here (CLAUDE.md 5.10). A vendor
    // added to `/data/catalogs/ai` appears in this chooser with no code change,
    // which is the whole point of the catalogue being data.
    const ai::ProviderCatalog& catalog = controller_.providerService().catalog();
    for (const ai::CatalogVendor& v : catalog.vendors())
        vendor_->addItem(v.local ? tr("%1 — yerel").arg(QString::fromStdString(v.name))
                                 : QString::fromStdString(v.name),
                         QString::fromStdString(v.id));
    if (catalog.empty()) vendor_->addItem(tr("(sağlayıcı kataloğu yok)"), QString());
    connect(vendor_, &QComboBox::currentIndexChanged, this, [this, &catalog](int at) {
        if (at < 0) return;
        const QString id = vendor_->itemData(at).toString();
        if (const ai::CatalogVendor* v = catalog.find(id.toStdString()); v != nullptr)
            applyVendor(*v);
    });
    column->addWidget(new FormRow(tr("Şablon"), vendor_, body));

    auto* identity = new QHBoxLayout;
    identity->setSpacing(10);
    name_ = text(tr("profil adı"), tr("Profil adı"));
    identity->addWidget(new FormRow(tr("Profil adı"), name_, body), 3);
    dialect_ = new ComboBox(body);
    for (const ai::Dialect d : ai::dialects())
        dialect_->addItem(QString::fromUtf8(ai::dialect_id(d)));
    dialect_->setAccessibleName(tr("Lehçe"));
    identity->addWidget(new FormRow(tr("Lehçe"), dialect_, body), 2);
    column->addLayout(identity);

    // ---- where -------------------------------------------------------------
    column->addWidget(new FormSection(tr("UÇ NOKTA"), QString(), body));
    auto* where = new QHBoxLayout;
    where->setSpacing(10);
    url_ = text(QStringLiteral("https://…"), tr("Adres"));
    where->addWidget(new FormRow(tr("Adres"), url_, body), 4);
    path_ = text(QStringLiteral("/chat/completions"), tr("Yol"));
    where->addWidget(new FormRow(tr("Yol"), path_, body), 2);
    column->addLayout(where);
    connect(url_, &Field::committed, this, [this](const QString&) { refreshModelsFromCatalog(); });

    headers_ = new QLabel(body);
    headers_->setObjectName(QStringLiteral("formHelp"));
    headers_->setWordWrap(true);
    column->addWidget(headers_);

    // ---- which model -------------------------------------------------------
    column->addWidget(new FormSection(tr("MODEL"),
                                      tr("liste katalogdan gelir; getirilen liste uç noktanın "
                                         "kendi cevabıdır"),
                                      body));
    auto* pick = new QHBoxLayout;
    pick->setSpacing(10);
    model_ = new ComboBox(body);
    model_->setEditable(true);
    model_->lineEdit()->setObjectName(QStringLiteral("comboLine"));
    model_->lineEdit()->setFrame(false);
    model_->setAccessibleName(tr("Model"));
    model_->setToolTip(tr("Listeden seçin ya da uç noktanızın sunduğu adı yazın."));
    pick->addWidget(model_, 4);
    fetch_ = new Button(ButtonRole::Secondary, tr("Modelleri getir"), Glyph::Refresh, body);
    fetch_->setToolTip(tr("Uç noktaya modellerini sorar. Anahtar gerekiyorsa önce kaydedin."));
    connect(fetch_, &QPushButton::clicked, this, &ProviderDialog::fetchModels);
    pick->addWidget(fetch_, 1);
    column->addLayout(pick);

    // ---- the knobs ---------------------------------------------------------
    column->addWidget(new FormSection(tr("AYARLAR"), QString(), body));
    auto* knobs = new QHBoxLayout;
    knobs->setSpacing(10);

    FieldSpec windowSpec = number_of(0, 100000000, tr("jeton"));
    context_             = field(windowSpec, tr("Bağlam penceresi"));
    auto* windowRow      = new FormRow(tr("Bağlam penceresi"), context_, body);
    windowRow->setHelp(tr("0 = bilinmiyor"));
    knobs->addWidget(windowRow, 2);

    // `azami` IS A MODEL'S OUTPUT CAP, not a zoning limit: the threshold gate
    // reads that word as an imar bound, and such a number belongs in /data and
    // never in C++ (5.13). This one is the endpoint's `max_tokens`.
    maxTokens_     = field(number_of(0, 10000000, tr("jeton")), tr("Azami çıktı")); // ui-label
    auto* limitRow = new FormRow(tr("Azami çıktı"), maxTokens_, body);              // ui-label
    limitRow->setHelp(tr("0 = gönderme"));
    knobs->addWidget(limitRow, 2);

    FieldSpec tempSpec = decimal_of(2);
    temperature_       = field(tempSpec, tr("Sıcaklık"));
    auto* tempRow      = new FormRow(tr("Sıcaklık"), temperature_, body);
    tempRow->setHelp(tr("boş = gönderme"));
    knobs->addWidget(tempRow, 2);
    column->addLayout(knobs);

    auto* second = new QHBoxLayout;
    second->setSpacing(10);
    reasoning_ = new ComboBox(body);
    for (const ai::ReasoningMode m :
         {ai::ReasoningMode::None, ai::ReasoningMode::Effort, ai::ReasoningMode::ResponsesSummary,
          ai::ReasoningMode::AnthropicBudget, ai::ReasoningMode::QwenBudget})
        reasoning_->addItem(reasoningCaption(m), QString::fromUtf8(ai::reasoning_mode_id(m)));
    reasoning_->setAccessibleName(tr("Düşünme"));
    second->addWidget(new FormRow(tr("Düşünme"), reasoning_, body), 3);

    auto* switches  = new QWidget(body);
    auto* switchRow = new QHBoxLayout(switches);
    switchRow->setContentsMargins(0, 0, 0, 0);
    switchRow->setSpacing(8);
    stream_ = new ToggleSwitch(switches);
    stream_->setChecked(true);
    stream_->setAccessibleName(tr("Akış"));
    switchRow->addWidget(stream_);
    auto* streamLabel = new QLabel(tr("Akış"), switches);
    streamLabel->setObjectName(QStringLiteral("formRowLabel"));
    switchRow->addWidget(streamLabel);
    tools_ = new ToggleSwitch(switches);
    tools_->setChecked(true);
    tools_->setAccessibleName(tr("Araçlar"));
    switchRow->addWidget(tools_);
    auto* toolsLabel = new QLabel(tr("Araçlar"), switches);
    toolsLabel->setObjectName(QStringLiteral("formRowLabel"));
    switchRow->addWidget(toolsLabel);
    switchRow->addStretch(1);
    second->addWidget(switches, 3);
    column->addLayout(second);

    // ---- the key -----------------------------------------------------------
    column->addWidget(new FormSection(tr("ANAHTAR"), QString(), body));
    keyRef_ = text(tr("anahtar adı"), tr("Anahtar adı"));
    keyRef_->setToolTip(tr("Anahtarı TUTAN kaydın adı — anahtar zincirindeki kayıt ya da bir "
                           "ortam değişkeni. Anahtarın kendisi buraya yazılmaz."));
    column->addWidget(new FormRow(tr("Anahtar adı"), keyRef_, body));

    auto* keyRow = new QHBoxLayout;
    keyRow->setSpacing(10);
    FieldSpec keySpec   = field_of(FieldKind::Text);
    keySpec.placeholder = tr("anahtarı yapıştırın");
    keySpec.secret      = true;
    keySpec.glyph       = Glyph::Lock;
    key_                = field(keySpec, tr("API anahtarı"));
    keyRow->addWidget(new FormRow(tr("API anahtarı"), key_, body), 4);
    keySave_ = new Button(ButtonRole::Secondary, tr("Anahtarı kaydet"), Glyph::Lock, body);
    keySave_->setToolTip(tr("Anahtar işletim sisteminin deposuna yazılır; komut satırına, "
                            "günlüğe ya da bu profile girmez."));
    connect(keySave_, &QPushButton::clicked, this, &ProviderDialog::saveKey);
    keyRow->addWidget(keySave_, 1);
    column->addLayout(keyRow);

    note_ = new QLabel(body);
    note_->setObjectName(QStringLiteral("formHelp"));
    note_->setWordWrap(true);
    column->addWidget(note_);
    column->addStretch(1);

    // THE FIRST TEMPLATE IS APPLIED BY HAND, not by setting the index: the combo
    // is ALREADY on row 0 after the first `addItem`, so `setCurrentIndex(0)`
    // changes nothing and `currentIndexChanged` never fires — which is how this
    // window first opened with eleven empty boxes under a chosen template.
    if (!catalog.vendors().empty()) applyVendor(catalog.vendors().front());
    return body;
}

void ProviderDialog::applyVendor(const ai::CatalogVendor& vendor)
{
    const ai::ProviderProfile seeded = ai::profile_for(vendor);
    load(seeded);

    // THE NAME IS THE VENDOR'S UNLESS THE USER HAS TYPED ONE. Retyping a name
    // over what somebody entered because they changed the template afterwards is
    // the kind of helpfulness that loses work.
    if (editing_.isEmpty() && name_->value().trimmed().isEmpty())
        name_->setValue(QString::fromStdString(vendor.name));

    if (!vendor.docs_url.empty())
        note(tr("Belgeler: %1").arg(QString::fromStdString(vendor.docs_url)));
}

void ProviderDialog::load(const ai::ProviderProfile& profile)
{
    if (!profile.name.empty() && !editing_.isEmpty())
        name_->setValue(QString::fromStdString(profile.name));
    else if (!editing_.isEmpty())
        name_->setValue(editing_);

    dialect_->setCurrentText(QString::fromUtf8(ai::dialect_id(profile.dialect)));
    url_->setValue(QString::fromStdString(profile.base_url));
    path_->setValue(QString::fromStdString(profile.path));
    keyRef_->setValue(QString::fromStdString(profile.key_ref));
    context_->setValue(profile.context.tokens > 0 ? QString::number(profile.context.tokens)
                                                  : QStringLiteral("0"));
    maxTokens_->setValue(QString::number(profile.max_tokens));
    temperature_->setValue(profile.temperature ? QString::number(*profile.temperature, 'f', 2)
                                               : QString());
    stream_->setChecked(profile.stream);
    tools_->setChecked(profile.tools);
    reasoning_->setCurrentText(reasoningCaption(profile.reasoning.mode));

    extraHeaders_ = profile.extra_headers;
    refreshHeaderNote();
    refreshModelsFromCatalog();
    if (!profile.model.empty()) model_->setCurrentText(QString::fromStdString(profile.model));
}

void ProviderDialog::refreshHeaderNote()
{
    if (extraHeaders_.empty()) {
        headers_->setVisible(false);
        return;
    }
    QStringList said;
    for (const auto& [name, value] : extraHeaders_)
        said << QStringLiteral("%1: %2").arg(QString::fromStdString(name),
                                             QString::fromStdString(value));
    // THE MANDATORY HEADERS ARE SHOWN BUT NOT EDITED: they are protocol rather
    // than preference — Anthropic answers 400 without `anthropic-version` — and a
    // user debugging a refusal needs to see what is actually being sent.
    headers_->setText(
        tr("Bu sağlayıcı şu başlıkları da gönderir: %1").arg(said.join(QStringLiteral(" · "))));
    headers_->setVisible(true);
}

QString ProviderDialog::modelCaption(const ai::CatalogModel& model) const
{
    QString out = QString::fromStdString(model.id);
    QStringList marks;
    if (!model.label.empty()) marks << QString::fromStdString(model.label);
    if (const QString window = windowCaption(model.context); !window.isEmpty()) marks << window;
    if (model.reasoning) marks << tr("düşünür");
    if (!marks.isEmpty()) out += QStringLiteral("  ·  %1").arg(marks.join(QStringLiteral(" · ")));
    return out;
}

void ProviderDialog::refreshModelsFromCatalog()
{
    if (model_ == nullptr) return;

    // WHAT THE USER HAS TYPED SURVIVES. The list is a convenience; the value is
    // theirs, and a private deployment serves a name no catalogue knows.
    const QString had = model_->currentText();

    ai::ProviderProfile probe;
    probe.base_url = url_->value().trimmed().toStdString();
    probe.dialect  = ai::dialect_from_id(dialect_->currentText().toStdString())
                        .value_or(ai::Dialect::OpenAiChat);

    const ai::ProviderCatalog& catalog = controller_.providerService().catalog();
    const ai::CatalogVendor* vendor    = catalog.for_profile(probe);

    model_->clear();
    if (vendor != nullptr)
        for (const ai::CatalogModel& m : vendor->models)
            model_->addItem(modelCaption(m), QString::fromStdString(m.id));

    fetch_->setEnabled(vendor != nullptr && !vendor->models_path.empty());
    if (vendor == nullptr)
        fetch_->setToolTip(tr("Bu adres katalogda yok; model adını elle yazın."));

    if (!had.isEmpty()) model_->setCurrentText(had);
}

QString ProviderDialog::chosenModelId() const
{
    // THE COMBO SHOWS A CAPTION AND CARRIES THE ID. A row picked from the list
    // answers with its data; a name the user typed answers with itself, and the
    // caption's ` · ` decoration is cut off it in case they typed over a row.
    const int at = model_->currentIndex();
    if (at >= 0 && model_->itemText(at) == model_->currentText())
        return model_->itemData(at).toString();
    return model_->currentText().section(QStringLiteral("  ·  "), 0, 0).trimmed();
}

ai::ProviderProfile ProviderDialog::profile() const
{
    ai::ProviderProfile p;
    p.name    = name_->value().trimmed().toStdString();
    p.dialect = ai::dialect_from_id(dialect_->currentText().toStdString())
                    .value_or(ai::Dialect::OpenAiChat);
    p.base_url      = url_->value().trimmed().toStdString();
    p.path          = path_->value().trimmed().toStdString();
    p.model         = chosenModelId().toStdString();
    p.key_ref       = keyRef_->value().trimmed().toStdString();
    p.extra_headers = extraHeaders_;
    p.max_tokens    = maxTokens_->value().trimmed().toLongLong();
    p.stream        = stream_->isChecked();
    p.tools         = tools_->isChecked();
    p.context       = {context_->value().trimmed().toLongLong(), ai::ContextSource::User};

    if (const QString t = temperature_->value().trimmed(); !t.isEmpty()) {
        bool ok = false;
        // THE C LOCALE, because this is a wire value and not a reading: a Turkish
        // locale writes `0,70` and every endpoint in the world wants `0.7`.
        const double value = QLocale::c().toDouble(t, &ok);
        if (!ok) {
            const double turkish = QLocale(QLocale::Turkish).toDouble(t, &ok);
            if (ok) p.temperature = turkish;
        } else {
            p.temperature = value;
        }
    }

    if (const std::optional<ai::ReasoningMode> mode =
            ai::reasoning_mode_from_id(reasoning_->currentData().toString().toStdString());
        mode)
        p.reasoning.mode = *mode;
    p.reasoning.show_text = true;
    if (p.reasoning.mode == ai::ReasoningMode::AnthropicBudget ||
        p.reasoning.mode == ai::ReasoningMode::QwenBudget)
        p.reasoning.budget_tokens = 4096;
    return p;
}

void ProviderDialog::note(const QString& text, Tone tone)
{
    note_->setText(text);
    note_->setProperty("tone", tone == Tone::Danger   ? QStringLiteral("danger")
                               : tone == Tone::Warn   ? QStringLiteral("warn")
                               : tone == Tone::Accent ? QStringLiteral("accent")
                                                      : QVariant());
    note_->style()->unpolish(note_);
    note_->style()->polish(note_);
    note_->setVisible(!text.isEmpty());
}

void ProviderDialog::saveKey()
{
    const QString ref = keyRef_->value().trimmed();
    if (ref.isEmpty()) {
        note(tr("Önce bir anahtar adı verin; anahtar o adla saklanır."), Tone::Danger);
        return;
    }
    const QString secret = key_->value();
    if (secret.isEmpty()) {
        note(tr("Anahtar alanı boş."), Tone::Danger);
        return;
    }
    const core::Status wrote = controller_.providerService().secrets().write(ref, secret);
    // THE VALUE IS DROPPED FROM THE WIDGET EITHER WAY, success or failure: a
    // secret left in a text field is a secret in a screenshot.
    key_->setValue(QString());
    if (!wrote) {
        note(QString::fromStdString(wrote.error().message), Tone::Danger);
        return;
    }
    note(tr("Anahtar '%1' kaydına yazıldı. %2").arg(ref, SecretStore::describe()), Tone::Accent);
}

void ProviderDialog::fetchModels()
{
    const ai::ProviderProfile p        = profile();
    const ai::ProviderCatalog& catalog = controller_.providerService().catalog();
    const ai::CatalogVendor* vendor    = catalog.for_profile(p);
    if (vendor == nullptr) {
        note(tr("Bu adres katalogda yok; model listesi sorulamaz."), Tone::Warn);
        return;
    }

    const bool sensitive = controller_.bus().session_settings().get("core.ai.hassas").as_bool();
    auto permitted       = ai::permit_for(p, sensitive);
    if (!permitted) {
        note(QString::fromStdString(permitted.error().message), Tone::Danger);
        return;
    }

    std::optional<ai::HttpRequest> request = ai::model_list_request(p, *vendor, permitted.value());
    if (!request) {
        note(tr("'%1' model listesi sunmuyor; adı elle yazın.")
                 .arg(QString::fromStdString(vendor->name)),
             Tone::Warn);
        return;
    }

    // A SECOND PRESS REPLACES THE FIRST. One endpoint is being asked at a time,
    // and a listing nobody is waiting for is a request that should not be open.
    if (listing_) listing_->dialog = nullptr;
    listing_ = std::make_unique<Listing>(this);

    fetch_->setEnabled(false);
    note(tr("Modeller soruluyor…"));

    AiTransport& wire = controller_.aiTransport();
    wire.useProfile(p);
    listing_->handle = wire.send(permitted.value(), std::move(*request), *listing_);
    if (!listing_->handle) fetch_->setEnabled(true); // the transport refused and already said why
}

void ProviderDialog::finishListing(int status, const QString& trouble, const std::string& body)
{
    fetch_->setEnabled(true);
    if (!trouble.isEmpty()) {
        note(tr("Model listesi alınamadı: %1").arg(ai::redact_text(trouble.toStdString()).c_str()),
             Tone::Danger);
        return;
    }
    if (status != 200) {
        note(tr("Model listesi alınamadı: uç nokta HTTP %1 verdi. Anahtar gerekiyorsa "
                "kaydedildi mi?")
                 .arg(status),
             Tone::Danger);
        return;
    }

    const ai::ProviderCatalog& catalog = controller_.providerService().catalog();
    const ai::CatalogVendor* vendor    = catalog.for_profile(profile());
    const std::vector<std::string> ids = ai::parse_model_list(
        vendor != nullptr ? vendor->list_shape : ai::ModelListShape::OpenAi, body);
    if (ids.empty()) {
        note(tr("Uç nokta cevap verdi ama listede model yok."), Tone::Warn);
        return;
    }

    const QString had = model_->currentText();
    model_->clear();
    for (const std::string& id : ids) {
        // THE CATALOGUE STILL SUPPLIES THE CONTEXT WINDOW, because a list-models
        // response almost never carries one: the live list decides WHICH models
        // exist, the catalogue decides what is known about them.
        const QString wire = QString::fromStdString(id);
        QString caption    = wire;
        if (vendor != nullptr)
            for (const ai::CatalogModel& m : vendor->models)
                if (m.id == id) caption = modelCaption(m);
        model_->addItem(caption, wire);
    }
    if (!had.isEmpty()) model_->setCurrentText(had);
    note(tr("Uç nokta %1 model bildirdi.").arg(ids.size()), Tone::Accent);
}

void ProviderDialog::save()
{
    const ai::ProviderProfile p = profile();
    if (p.name.empty()) {
        name_->setState(FieldState::Invalid);
        note(tr("Profil adı boş olamaz."), Tone::Danger);
        return;
    }
    name_->setState(FieldState::Normal);

    // ONE LINE, and it is the line a script would type (Article 1.2). Validation
    // is the store's, not this window's: `upsert` refuses an address that is not
    // http(s), a path that does not start with a slash, an empty model and a
    // key_ref that looks like an actual key — and the refusal comes back as the
    // command's own message, so the window and the command line say the same
    // thing about the same mistake.
    QString line = QStringLiteral("YAPAYZEKAMODELİ islem=ekle ad=%1 lehce=%2")
                       .arg(quoted(QString::fromStdString(p.name)),
                            QString::fromUtf8(ai::dialect_id(p.dialect)));
    const auto add = [&line](const char* key, const QString& value) {
        if (!value.trimmed().isEmpty())
            line += QStringLiteral(" %1=%2").arg(QString::fromUtf8(key), quoted(value.trimmed()));
    };
    add("adres", QString::fromStdString(p.base_url));
    add("yol", QString::fromStdString(p.path));
    add("model", QString::fromStdString(p.model));
    add("anahtar_ref", QString::fromStdString(p.key_ref));
    if (p.context.tokens > 0) line += QStringLiteral(" baglam=%1").arg(p.context.tokens);
    if (p.max_tokens > 0) line += QStringLiteral(" azami=%1").arg(p.max_tokens); // ui-label
    if (p.temperature)
        line += QStringLiteral(" sicaklik=%1").arg(QString::number(*p.temperature, 'f', 2));
    line +=
        QStringLiteral(" akis=%1").arg(p.stream ? QStringLiteral("evet") : QStringLiteral("hayir"));
    line += QStringLiteral(" araclar=%1")
                .arg(p.tools ? QStringLiteral("evet") : QStringLiteral("hayir"));
    line += QStringLiteral(" dusunme=%1")
                .arg(QString::fromUtf8(ai::reasoning_mode_id(p.reasoning.mode)));

    controller_.runLine(line, command::Origin::Gui);
    accept();
}

QStringList ProviderDialog::probeModels() const
{
    QStringList out;
    for (int i = 0; i < model_->count(); ++i)
        out << model_->itemData(i).toString();
    return out;
}

bool ProviderDialog::probeSave()
{
    save();
    return result() == QDialog::Accepted;
}

} // namespace kentos::app
