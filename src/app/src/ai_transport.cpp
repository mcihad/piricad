// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/ai_transport.hpp"

#include "kentos_cad/ai/redact.hpp"
#include "kentos_cad/app/secret_resolver.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

#include <utility>

namespace kentos::app {
namespace {

/// What a cancelled turn is called on the sink. One string, because a cancel that
/// arrives before the request was built and one that aborts a live reply are the
/// same event to whoever is reading the transcript.
constexpr std::string_view kCancelled = "İstek iptal edildi.";

/// What is said when the credential a profile names cannot be found anywhere.
///
/// IT NAMES THE ENTRY, NEVER THE VALUE: the `key_ref` is the name of a key-store
/// record, and a message is one of the places CLAUDE.md 5.21 forbids a secret to
/// reach.
std::string missing_key(const QString& key_ref)
{
    return ("Anahtar bulunamadı: '" + key_ref +
            "'. Anahtarı Seçenekler ▸ Yapay Zeka Modelleri sayfasından "
            "girin ya da aynı adlı ortam değişkenini tanımlayın.")
        .toStdString();
}

/// The handle a caller cancels a turn with.
///
/// IT OUTLIVES THE REPLY ON PURPOSE. `cancel()` may arrive after the reply has
/// finished and been deleted (the user pressed Escape as the last chunk landed),
/// so the handle holds a `QPointer` and a flag rather than a raw reply pointer:
/// cancelling a finished request must be a no-op, not a crash (ai.md R18 —
/// cancelling is expected, not exceptional).
///
/// AND IT EXISTS BEFORE THE REPLY DOES. The credential is looked up off this
/// thread (secret_resolver.hpp), so between `send` returning and the first byte
/// leaving there is a window with a handle and no request in it. Cancelling in
/// that window must still answer the sink, and must answer it SYNCHRONOUSLY —
/// that is what `QNetworkReply::abort()` does, and `ProviderDialog::Listing`
/// cancels from its own destructor, so an answer posted for later would land on
/// a sink that no longer exists.
class ReplyCancel : public ai::Cancellation
{
public:
    /// `sink` is the caller's and outlives this handle (ai/transport.hpp). It is
    /// held so that an ending which never becomes a reply still has somewhere to
    /// be reported.
    explicit ReplyCancel(ai::StreamSink& sink) : sink_(&sink) {}

    void cancel() override
    {
        if (cancelled_) return;
        cancelled_ = true;
        // A LIVE REPLY ANSWERS FOR ITSELF: `abort()` emits `finished`, and the
        // handler below turns that into the one `on_finished` this turn gets.
        if (reply_ != nullptr) {
            reply_->abort();
            return;
        }
        settle(0, kCancelled);
    }

    bool cancelled() const override { return cancelled_; }

    /// Answers the sink, once and once only.
    ///
    /// EVERYTHING THAT ENDS A REQUEST COMES THROUGH HERE — the reply finishing, a
    /// cancel that arrived before there was a request, a credential that could
    /// not be found — because `ai::StreamSink` promises EXACTLY ONE
    /// `on_finished` (ai/transport.hpp) and there are now three places that can
    /// produce one.
    void settle(int status, std::string_view trouble)
    {
        ai::StreamSink* waiting = sink_;
        sink_                   = nullptr;
        if (waiting != nullptr) waiting->on_finished(status, trouble);
    }

    /// The request exists from now on, so a later `cancel()` aborts it.
    void adopt(QNetworkReply* reply)
    {
        reply_ = reply;
        if (cancelled_ && reply != nullptr) reply->abort();
    }

    /// Called when the reply is finishing, so a later `cancel()` touches nothing.
    void forget() { reply_ = nullptr; }

private:
    QPointer<QNetworkReply> reply_;
    ai::StreamSink* sink_{nullptr};
    bool cancelled_{false};
};

/// Puts `request` on the wire and streams the answer into `sink`.
///
/// Separate from `send` because it is reached from two places now: straight away
/// when the credential is already known, and from the resolver's callback when it
/// was not.
QNetworkReply* start_reply(QNetworkAccessManager& net, const QNetworkRequest& request,
                           const std::string& method, const QByteArray& body, ai::StreamSink& sink,
                           const std::shared_ptr<ReplyCancel>& handle)
{
    QNetworkReply* reply = method == "GET" ? net.get(request) : net.post(request, body);

    // BYTES STRAIGHT THROUGH, in order, unframed. A chunk may split an SSE event
    // mid-line; `ai::SseParser` is written for exactly that and is the half a
    // test can drive (`tests/unit/test_ai_chat.cpp`).
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, &sink] {
        const QByteArray chunk = reply->readAll();
        if (!chunk.isEmpty())
            sink.on_chunk(
                std::string_view(chunk.constData(), static_cast<std::size_t>(chunk.size())));
    });

    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, &sink, handle] {
        handle->forget();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        std::string trouble;
        if (handle->cancelled()) {
            trouble = std::string(kCancelled);
        } else if (reply->error() != QNetworkReply::NoError) {
            // THROUGH THE REDACTOR FIRST: a provider's error body sometimes
            // echoes the request headers back, and the authorisation header is
            // one of them (ai.md P11).
            trouble               = ai::redact_text(reply->errorString().toStdString());
            const QByteArray tail = reply->readAll();
            if (!tail.isEmpty())
                trouble += " — " + ai::redact_text(std::string(
                                       tail.constData(), static_cast<std::size_t>(tail.size())));
        } else {
            // Whatever arrived between the last `readyRead` and `finished`.
            const QByteArray last = reply->readAll();
            if (!last.isEmpty())
                sink.on_chunk(
                    std::string_view(last.constData(), static_cast<std::size_t>(last.size())));
        }

        handle->settle(status, trouble);
        reply->deleteLater();
    });

    return reply;
}

/// Adds the profile's authorisation header to `request`.
///
/// THE PROFILE IS PASSED IN RATHER THAN READ FROM THE MEMBER, because the
/// deferred road runs this after `send` has returned and `useProfile` may have
/// named a different profile by then — the header a request carries must be the
/// one belonging to the request, not to whatever is being sent next.
void authorise(QNetworkRequest& request, const ai::ProviderProfile& profile, const QString& secret)
{
    const std::string scheme =
        profile.auth_scheme.empty() ? std::string() : profile.auth_scheme + " ";
    request.setRawHeader(QByteArray::fromStdString(profile.auth_header),
                         QByteArray::fromStdString(scheme + secret.toStdString()));
}

} // namespace

AiTransport::AiTransport(SecretResolver* secrets, QObject* parent)
    : QObject(parent), net_(std::make_unique<QNetworkAccessManager>(this)), secrets_(secrets)
{}

AiTransport::~AiTransport() = default;

void AiTransport::useProfile(const ai::ProviderProfile& profile)
{
    profile_      = profile;
    have_profile_ = true;
}

std::shared_ptr<ai::Cancellation> AiTransport::send(const ai::EndpointPermit& permit,
                                                    ai::HttpRequest request, ai::StreamSink& sink)
{
    // THE PERMIT'S URL, NOT THE REQUEST'S. `ai::permit_for` is what decided this
    // endpoint may be called at all — a sensitive project permits only a local
    // one (ai.md R14, P3) — so the address that was PERMITTED is the address
    // that is used. A request whose url drifted from its permit would be a way
    // round the policy that nothing else would notice.
    QNetworkRequest qt_request{QUrl(QString::fromStdString(permit.url()))};

    for (const auto& [name, value] : request.headers)
        qt_request.setRawHeader(QByteArray::fromStdString(name), QByteArray::fromStdString(value));

    const QByteArray body = QByteArray::fromStdString(request.body);
    auto handle           = std::make_shared<ReplyCancel>(sink);

    // A PROFILE WITH NO `key_ref` ASKS THE KEY STORE NOTHING. A local Ollama or
    // llama.cpp needs no credential, and that is the default this program ships
    // with, so the common case never comes near the paragraphs below.
    if (!have_profile_ || profile_.key_ref.empty()) {
        handle->adopt(start_reply(*net_, qt_request, request.method, body, sink, handle));
        return handle;
    }

    const QString key_ref = QString::fromStdString(profile_.key_ref);
    if (secrets_ == nullptr) {
        // AN HONEST FAILURE rather than an unauthenticated request. A provider
        // answering 401 tells the user their key is wrong; a request sent with no
        // key at all tells them nothing, and the key they carefully entered is
        // not the problem.
        sink.on_finished(0, missing_key(key_ref));
        return nullptr;
    }

    // THE CREDENTIAL IS ADDED HERE AND NOWHERE ELSE. It never existed in
    // `/src/ai`, it is not in the request that arrived, and it is gone again the
    // moment this turn ends (CLAUDE.md 5.21).
    //
    // WHAT THIS DOES NOT DO IS LOOK IT UP. `SecretResolver::known` answers from
    // memory and returns at once; the lookup itself is somebody else's thread
    // (secret_resolver.hpp), because making it here is what used to freeze the
    // window for as long as an unanswered keychain prompt lasted (ai.md R18, P8).
    const SecretResolver::Answer known = secrets_->known(key_ref);
    if (known.settled) {
        if (!known.secret || known.secret->isEmpty()) {
            sink.on_finished(0, missing_key(key_ref));
            return nullptr;
        }
        authorise(qt_request, profile_, *known.secret);
        handle->adopt(start_reply(*net_, qt_request, request.method, body, sink, handle));
        return handle;
    }

    // NOT RESOLVED YET: the handle goes back now and the socket opens when the
    // key store answers. Everything the request needs is carried BY VALUE into
    // the callback — the profile included, because `useProfile` may well have
    // named another one by the time this runs.
    const QPointer<AiTransport> self(this);
    secrets_->resolve(key_ref, [self, qt_request, profile = profile_, method = request.method, body,
                                &sink, handle, key_ref](std::optional<QString> secret) mutable {
        // CANCELLED WHILE WE WAITED: `cancel()` has already answered the sink,
        // and nothing is sent.
        if (handle->cancelled()) return;
        if (self.isNull()) return; // the transport went; the window is going with it
        if (!secret || secret->isEmpty()) {
            handle->settle(0, missing_key(key_ref));
            return;
        }
        authorise(qt_request, profile, *secret);
        handle->adopt(start_reply(*self->net_, qt_request, method, body, sink, handle));
    });

    return handle;
}

} // namespace kentos::app
