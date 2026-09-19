// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/provider_service.hpp"

#include "kentos_cad/app/data_root.hpp"

#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/redact.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace kentos::app {
namespace {

/// How long an endpoint is given to answer a probe. Twenty seconds is long
/// enough for a cold local model to load and short enough that a wrong port does
/// not leave a line the user stopped waiting for.
constexpr int kProbeTimeoutMs = 20000;

/// The one word the probe sends. SHORT ON PURPOSE: this is a connection test, not
/// a conversation, and a provider that bills by the token is being tested on
/// somebody's account.
constexpr const char* kProbeText = "ping";

QString utf8(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/// The endpoint path and credential header a dialect uses, when the line did not
/// say.
///
/// PROTOCOL, NOT CONFIGURATION, and this is where the same distinction
/// `dialect_anthropic.cpp` makes about `anthropic-version` lands for a profile:
/// `/chat/completions`, `/responses`, `/messages` and `/api/chat` are the four
/// dialects' own endpoints (`ai/dialect.hpp` documents each one), and Anthropic
/// takes the bare key in `x-api-key` with NO scheme in front of it — sending
/// `Bearer` there is a 401 nobody can read. So `ekle` fills them in from the
/// chosen dialect rather than making an operator restate a protocol: the path is
/// still overridable by naming `yol` on the line, and a gateway that wants a
/// different credential header is an edit to the profile file, which is a plain
/// JSON document a person may edit.
///
/// It is not a second provider list: it says nothing about a vendor, only about
/// the four wire languages `ai::Dialect` already enumerates.
void apply_dialect_defaults(ai::ProviderProfile& profile)
{
    switch (profile.dialect) {
    case ai::Dialect::OpenAiChat:
        if (profile.path.empty()) profile.path = "/chat/completions";
        break;
    case ai::Dialect::OpenAiResponses:
        if (profile.path.empty()) profile.path = "/responses";
        break;
    case ai::Dialect::AnthropicMessages:
        if (profile.path.empty()) profile.path = "/messages";
        profile.auth_header = "x-api-key";
        profile.auth_scheme = "";
        break;
    case ai::Dialect::OllamaNative:
        if (profile.path.empty()) profile.path = "/api/chat";
        break;
    }
}

} // namespace

// ------------------------------------------------------------------- Probe ----

/// One connection test in flight.
///
/// IT COLLECTS AND COUNTS, AND DECODES NOTHING. What a probe has to establish is
/// that the address resolves, the endpoint is there, the credential is accepted
/// and the model id exists — all of which the HTTP status says. Running the
/// stream decoder over the answer would be testing `dialect.cpp`, which
/// `tests/unit/test_ai_chat.cpp` already does from recorded bytes.
class ProviderService::Probe final : public ai::StreamSink
{
public:
    /// Records what the finished line has to name. `bus` outlives this object:
    /// the controller owns the bus and builds the service after it.
    Probe(command::Bus& bus, std::string profile, std::string url, std::string model)
        : bus_(bus), profile_(std::move(profile)), url_(std::move(url)), model_(std::move(model))
    {
        clock_.start();
    }

    /// Cancels an unfinished request, so that a sink is never destroyed while a
    /// transport still holds it (`ai/transport.hpp`: the sink must outlive the
    /// handle, and exactly one `on_finished` arrives after a cancel).
    ~Probe() override { cancel(); }

    Probe(const Probe&)            = delete;
    Probe& operator=(const Probe&) = delete;

    /// Takes ownership of the handle that stops the request.
    void hold(std::shared_ptr<ai::Cancellation> handle) { handle_ = std::move(handle); }

    /// THE SINK KEEPS ITSELF ALIVE UNTIL THE REQUEST ENDS, and this is not
    /// belt-and-braces.
    ///
    /// The transport holds the sink by REFERENCE (`ai::HttpTransport::send`) and
    /// promises exactly one `on_finished`; `QNetworkReply::abort()` normally
    /// delivers it there and then, but "normally" is not a lifetime rule. If the
    /// window closes with a probe in flight, the service is destroyed while the
    /// reply still exists — so the probe holds a reference to itself, released in
    /// `on_finished`, and the last owner is therefore whoever is still there when
    /// the endpoint answers. `probe_` in the service is the other owner.
    void keepAlive(std::shared_ptr<Probe> self) { self_ = std::move(self); }

    /// The service is going away: stop the request and say nothing when it ends.
    ///
    /// The bus outlives this object, but `Bus::on_echo` is a lambda owned by the
    /// controller that is being destroyed around us, so a line said now would be
    /// a signal emitted from a half-destroyed window.
    void detach()
    {
        detached_ = true;
        cancel();
    }

    /// Abandons the request if it has not already ended. Idempotent, like the
    /// handle it calls.
    void cancel()
    {
        if (handle_ && !finished_) handle_->cancel();
    }

    /// Says the answer never came, before cancelling, so the line the sink
    /// prints is about the silence rather than about the cancel that ended it.
    void giveUp()
    {
        timed_out_ = true;
        cancel();
    }

    bool finished() const noexcept { return finished_; }

    /// Bytes are counted, not kept: an answer of any size proves the endpoint
    /// spoke, and keeping a model's words for a connection test would put them
    /// somewhere nobody asked for them to be.
    void on_chunk(std::string_view bytes) override { received_ += bytes.size(); }

    /// The one line the user reads, on the transcript the command's own line
    /// went to.
    void on_finished(int status, std::string_view error) override
    {
        // THE SELF-REFERENCE IS RELEASED HERE AND NOWHERE ELSE, into a local, so
        // that `this` is certainly alive for the whole function and is destroyed
        // on the way out when the service has already let go (see `keepAlive`).
        const std::shared_ptr<Probe> self = std::move(self_);

        if (finished_) return; // the contract promises one, but a sink is cheap to make safe
        finished_                 = true;
        const std::int64_t millis = clock_.elapsed();
        if (detached_) return;

        std::string said = "Model denemesi — " + profile_ + " (" + url_ + ", " + model_ + "): ";
        if (timed_out_) {
            said += "cevap gelmedi. Uç nokta " + std::to_string(kProbeTimeoutMs / 1000) +
                    " saniyede yanıt vermedi; adresi ve portu denetleyin.";
        } else if (status == 200) {
            said += "bağlantı kuruldu. HTTP 200, " + std::to_string(received_) + " bayt, " +
                    std::to_string(millis) + " ms.";
        } else if (status == 401 || status == 403) {
            said += "uç nokta kimliği kabul etmedi (HTTP " + std::to_string(status) +
                    "). Anahtarı Seçenekler ▸ Yapay Zeka Modelleri sayfasından girin; profilin "
                    "anahtar adı doğru kaydı gösteriyor mu?";
        } else if (status == 404) {
            said += "uç nokta bulunamadı (HTTP 404). Adres, yol ve model adını denetleyin.";
        } else if (status > 0) {
            said += "uç nokta HTTP " + std::to_string(status) + " döndürdü.";
        } else {
            said += "istek gönderilemedi.";
        }
        // THE PROVIDER'S OWN WORDS, THROUGH THE FUNNEL. An error body sometimes
        // quotes the request's headers back, so everything that leaves goes
        // through `redact_text` (ai/redact.hpp) — a masked hash in a transcript
        // costs nothing and a leaked key costs everything (CLAUDE.md 5.21).
        if (!error.empty() && !timed_out_) said += " " + ai::redact_text(error);
        bus_.echo(said);
    }

private:
    command::Bus& bus_;
    std::string profile_;
    std::string url_;
    std::string model_;
    std::shared_ptr<ai::Cancellation> handle_;
    std::shared_ptr<Probe> self_;
    QElapsedTimer clock_;
    std::size_t received_{0};
    bool finished_{false};
    bool timed_out_{false};
    bool detached_{false};
};

// --------------------------------------------------------- ProviderService ----

ProviderService::ProviderService(command::Bus& bus, ai::HttpTransport* transport, QObject* parent)
    : QObject(parent), bus_(bus), transport_(transport)
{
    // ---- the shipped vendor catalogue, before anything else -----------------
    //
    // IT IS WHERE THE STARTING PROFILES COME FROM, so it is read first. An
    // endpoint address and a model id rot in weeks; keeping them in `/data`
    // makes a vendor's rename a data release rather than a rebuild
    // (`ai/provider_catalog.hpp`). A missing package is REPORTED and leaves the
    // store empty rather than falling back to a copy compiled into the binary —
    // there is no such copy any more, and inventing one here would put the two
    // lists back (CLAUDE.md 5.10).
    const std::string catalogue = data_path("catalogs/ai/saglayicilar.json");
    if (catalogue.empty()) {
        trouble_ = "Sağlayıcı kataloğu bulunamadı (data/catalogs/ai/saglayicilar.json). "
                   "Model profili eklemek için adresi elle yazmanız gerekir.";
    } else {
        QFile shipped(QString::fromStdString(catalogue));
        if (!shipped.open(QIODevice::ReadOnly | QIODevice::Text)) {
            trouble_ = "Sağlayıcı kataloğu okunamadı: " + catalogue + ".";
        } else {
            const QByteArray text = shipped.readAll();
            auto parsed           = ai::ProviderCatalog::from_json(
                std::string_view(text.constData(), static_cast<std::size_t>(text.size())));
            if (parsed)
                catalog_ = std::move(parsed.value());
            else
                trouble_ = parsed.error().message;
        }
    }
    profiles_ = ai::seed_profiles(catalog_);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    path_             = dir + QStringLiteral("/ai-modelleri.json");

    QFile file(path_);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            trouble_ = "Model sağlayıcı dosyası okunamadı: " + path_.toStdString() + ".";
        } else {
            const QByteArray text = file.readAll();
            auto loaded           = ai::ProviderProfiles::from_json(
                std::string_view(text.constData(), static_cast<std::size_t>(text.size())));
            if (loaded) {
                profiles_ = std::move(loaded.value());
            } else {
                // A file that does not parse is REPORTED and left alone; the
                // built-in set stands in for this run and is not written over
                // the user's file. Kept rather than echoed, because the bus
                // cannot be heard yet (see `announce`).
                trouble_ = loaded.error().message;
            }
        }
    }

    probe_timeout_.setSingleShot(true);
    connect(&probe_timeout_, &QTimer::timeout, this, [this] {
        if (probe_ && !probe_->finished()) probe_->giveUp();
    });

    // `request` by value into the coroutine, for the reason `PrintService` gives.
    bus_.on_ai_provider_request = [this](const command::AiProviderRequest& request)
        -> command::Task<core::Result<std::string>> { return this->handle(request); };
}

ProviderService::~ProviderService()
{
    bus_.on_ai_provider_request = nullptr;
    cancelProbe();
}

void ProviderService::announce()
{
    if (trouble_.empty()) return;
    // THE SENTENCE SAYS WHAT STANDS IN, and that differs by which file failed: a
    // user file that would not parse falls back to the catalogue's seeded set
    // and leaves their file alone, while a missing catalogue leaves nothing to
    // fall back TO and the honest line says so.
    bus_.echo(trouble_ + (catalog_.empty()
                              ? " Tanımlı model profili yok; Seçenekler ▸ Yapay Zeka "
                                "Modelleri sayfasından elle ekleyebilirsiniz."
                              : " Bu oturumda katalogdan gelen model profilleri kullanılıyor; "
                                "dosyanız olduğu gibi bırakıldı."));
    trouble_.clear();
}

void ProviderService::setTransport(ai::HttpTransport* transport, ProfileBinder binder)
{
    cancelProbe();
    transport_ = transport;
    binder_    = std::move(binder);
}

void ProviderService::cancelProbe()
{
    probe_timeout_.stop();
    if (!probe_) return;
    // `detach` cancels AND silences; releasing our reference afterwards leaves
    // the probe's own reference as the last one, so a reply that answers later
    // finds a live sink (see `Probe::keepAlive`).
    probe_->detach();
    probe_.reset();
}

core::Status ProviderService::persist()
{
    QDir().mkpath(QFileInfo(path_).absolutePath());
    QFile file(path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return core::err(core::ErrorCode::IoFailure,
                         "Model sağlayıcı dosyası yazılamadı: " + path_.toStdString());
    const std::string text = profiles_.to_json();
    const qint64 written   = file.write(text.data(), static_cast<qint64>(text.size()));
    if (written != static_cast<qint64>(text.size()))
        return core::err(core::ErrorCode::IoFailure,
                         "Model sağlayıcı dosyası eksik yazıldı: " + path_.toStdString());
    return core::ok();
}

core::Result<std::string> ProviderService::beginProbe(const ai::ProviderProfile& profile)
{
    // THE SENSITIVITY TEST IS THE FIRST ONE, and it is a type rather than an
    // `if`: `ai::permit_for` is the only source of an `EndpointPermit` and no
    // transport call compiles without one (ai.md R14, R30, P3). A project marked
    // sensitive refuses a cloud endpoint here, whatever the profile calls itself.
    const bool sensitive = bus_.setting("core.ai.hassas").as_bool();
    auto permit          = ai::permit_for(profile, sensitive);
    if (!permit) return permit.error();

    // A CREDENTIAL THAT CANNOT BE FOUND IS SAID PLAINLY, rather than sent as
    // nothing and reported back as the provider's 401. The key itself is not read
    // into any message — only whether one is there (CLAUDE.md 5.21).
    //
    // ONLY WHAT IS ALREADY KNOWN IS TESTED HERE, and the lookup is STARTED rather
    // than waited for. This is a command body: `Bus::run_to_completion` runs it
    // on the GUI thread, and asking the platform key store from here would freeze
    // the window for as long as an authorisation prompt goes unanswered
    // (secret_resolver.hpp, ai.md R18/P8) — the very thing the note at the top of
    // this file says a probe must not do. A key nobody has resolved yet therefore
    // passes this check and is reported by the probe along with everything else:
    // `AiTransport::send` waits for the same lookup and answers the sink when it
    // lands, which is where the rest of this test's outcome comes from anyway.
    if (!profile.key_ref.empty()) {
        const SecretResolver::Answer key = secrets_.known(utf8(profile.key_ref));
        if (key.settled && !key.secret)
            return core::err(core::ErrorCode::NotFound,
                             "'" + profile.name + "' profilinin anahtarı bulunamadı: '" +
                                 profile.key_ref +
                                 "' adıyla ne anahtar deposunda bir kayıt ne de böyle bir ortam "
                                 "değişkeni var. Anahtarı Seçenekler ▸ Yapay Zeka Modelleri "
                                 "sayfasından girin. Anahtar deposu: " +
                                 SecretStore::describe().toStdString());
        if (!key.settled) secrets_.prime(utf8(profile.key_ref));
    }

    if (transport_ == nullptr)
        return core::err(core::ErrorCode::Unsupported,
                         "Bu ortamda ağ taşıyıcısı bağlı değil, bu yüzden bağlantı denenemedi; "
                         "komutu uygulama içinden çalıştırın. Profil olduğu gibi duruyor.");

    // One at a time: a second `dene` abandons the first.
    cancelProbe();

    ai::Conversation probe;
    probe.add(ai::text_message(ai::Role::User, kProbeText));

    ai::ChatRequest ask;
    ask.profile = &profile;
    ask.chat    = &probe;
    // NO TOOL CATALOGUE, whatever the profile says about tools: a probe asks
    // whether the endpoint is there, and sending 80 tool schemas to find that out
    // would make the test cost more than the work.
    ask.catalog = nullptr;

    probe_ = std::make_shared<Probe>(bus_, profile.name, permit.value().url(), profile.model);
    probe_->keepAlive(probe_);
    // The concrete transport adds the credential as the bytes leave and needs to
    // know which profile it comes from (`AiTransport::useProfile`); the interface
    // deliberately carries neither (see `ProfileBinder`).
    if (binder_) binder_(profile);
    probe_->hold(transport_->send(permit.value(), ai::request_for(ask, permit.value()), *probe_));
    if (!probe_->finished()) probe_timeout_.start(kProbeTimeoutMs);

    return "Model denemesi başladı: " + profile.name + " — " + permit.value().url() +
           ". Sonuç geldiğinde bu satırın altına yazılacak (en çok " +
           std::to_string(kProbeTimeoutMs / 1000) + " saniye).";
}

command::Task<core::Result<std::string>> ProviderService::handle(command::AiProviderRequest request)
{
    using Verb = command::AiProviderRequest::Verb;
    switch (request.verb) {
    case Verb::Profiles: co_return profiles_.listing();

    case Verb::SetProfile: {
        // A profile is made from the DEFAULTS plus what was given, not from the
        // current default profile — the `ekle` semantics `YAZDIRMAPROFİLİ`
        // carries (print_profiles.hpp): "ekle ad=X lehce=anthropic_messages"
        // means an Anthropic endpoint in the built-in everything-else, whatever
        // the office's default provider is.
        ai::ProviderProfile p;
        p.name = request.name;
        if (!request.dialect.empty()) {
            const std::optional<ai::Dialect> dialect = ai::dialect_from_id(request.dialect);
            if (!dialect)
                co_return core::err(core::ErrorCode::Unsupported,
                                    "Bu sürüm '" + request.dialect + "' lehçesini bilmiyor.");
            p.dialect = *dialect;
        }
        if (!request.base_url.empty()) p.base_url = request.base_url;
        if (!request.path.empty()) p.path = request.path;
        if (!request.model.empty()) p.model = request.model;
        if (!request.key_ref.empty()) p.key_ref = request.key_ref;
        if (request.context > 0) {
            // TYPED BY THE OPERATOR, and the marker says so: a number somebody
            // wrote is not a number the server reported, and the panel must be
            // able to tell them apart (`ai::ContextSource`).
            p.context = ai::ContextWindow{request.context, ai::ContextSource::User};
        }
        if (request.max_tokens >= 0) p.max_tokens = request.max_tokens;
        if (request.temperature >= 0.0) p.temperature = request.temperature;
        if (request.stream >= 0) p.stream = request.stream == 1;
        if (request.tools >= 0) p.tools = request.tools == 1;
        if (request.reasoning_text >= 0) p.reasoning.show_text = request.reasoning_text == 1;
        apply_dialect_defaults(p);

        if (auto st = profiles_.upsert(std::move(p)); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        co_return "Model profili kaydedildi: " +
            ai::describe_provider(*profiles_.find(request.name)) + "\n" + profiles_.listing();
    }

    case Verb::RemoveProfile: {
        if (auto st = profiles_.remove(request.name); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        // THE KEY IS NOT TOUCHED. A profile is a record; the credential belongs
        // to the operating system's store and may well be shared by a second
        // profile pointing at the same vendor. Deleting somebody's API key
        // because they renamed a profile is not a deletion they asked for.
        co_return "Model profili silindi: " + request.name + "\n" + profiles_.listing();
    }

    case Verb::SetDefault: {
        if (auto st = profiles_.set_default(request.name); !st) co_return st.error();
        if (auto st = persist(); !st) co_return st.error();
        emit profilesChanged();
        co_return "Varsayılan model sağlayıcısı: " + profiles_.default_name() + "\n" +
            profiles_.listing();
    }

    case Verb::Test: {
        const ai::ProviderProfile* profile = profiles_.find(request.name);
        if (profile == nullptr)
            co_return core::err(core::ErrorCode::NotFound,
                                "Böyle bir model sağlayıcısı yok: '" + request.name +
                                    "'. Tanımlı olanları YAPAYZEKAMODELİ islem=listele ile "
                                    "görün.");
        co_return beginProbe(*profile);
    }
    }
    co_return core::err(core::ErrorCode::Internal, "İşlenmemiş model sağlayıcı isteği.");
}

} // namespace kentos::app
