// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/ai_transport.hpp"

#include "kentos_cad/ai/redact.hpp"
#include "kentos_cad/app/secret_store.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace kentos::app {
namespace {

/// The handle a caller cancels a turn with.
///
/// IT OUTLIVES THE REPLY ON PURPOSE. `cancel()` may arrive after the reply has
/// finished and been deleted (the user pressed Escape as the last chunk landed),
/// so the handle holds a `QPointer` and a flag rather than a raw reply pointer:
/// cancelling a finished request must be a no-op, not a crash (ai.md R18 —
/// cancelling is expected, not exceptional).
class ReplyCancel : public ai::Cancellation
{
public:
    explicit ReplyCancel(QNetworkReply* reply) : reply_(reply) {}

    void cancel() override
    {
        cancelled_ = true;
        if (reply_ != nullptr) reply_->abort();
    }

    bool cancelled() const override { return cancelled_; }

    /// Called when the reply is finishing, so a later `cancel()` touches nothing.
    void forget() { reply_ = nullptr; }

private:
    QPointer<QNetworkReply> reply_;
    bool cancelled_{false};
};

} // namespace

AiTransport::AiTransport(SecretStore* secrets, QObject* parent)
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

    // THE CREDENTIAL IS ADDED HERE AND NOWHERE ELSE. It never existed in
    // `/src/ai`, it is not in the request that arrived, and it is gone again the
    // moment this function returns (CLAUDE.md 5.21).
    if (have_profile_ && !profile_.key_ref.empty()) {
        const std::optional<QString> secret =
            secrets_ != nullptr ? secrets_->read(QString::fromStdString(profile_.key_ref))
                                : std::nullopt;
        if (!secret || secret->isEmpty()) {
            // AN HONEST FAILURE rather than an unauthenticated request. A
            // provider answering 401 tells the user their key is wrong; a
            // request sent with no key at all tells them nothing, and the key
            // they carefully entered is not the problem.
            const QString where = QString::fromStdString(profile_.key_ref);
            sink.on_finished(0, ("Anahtar bulunamadı: '" + where +
                                 "'. Anahtarı Seçenekler ▸ Yapay Zeka Modelleri sayfasından "
                                 "girin ya da aynı adlı ortam değişkenini tanımlayın.")
                                    .toStdString());
            return nullptr;
        }
        const std::string scheme =
            profile_.auth_scheme.empty() ? std::string() : profile_.auth_scheme + " ";
        qt_request.setRawHeader(QByteArray::fromStdString(profile_.auth_header),
                                QByteArray::fromStdString(scheme + secret->toStdString()));
    }

    QNetworkReply* reply  = nullptr;
    const QByteArray body = QByteArray::fromStdString(request.body);
    if (request.method == "GET")
        reply = net_->get(qt_request);
    else
        reply = net_->post(qt_request, body);

    auto handle = std::make_shared<ReplyCancel>(reply);

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
            trouble = "İstek iptal edildi.";
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

        sink.on_finished(status, trouble);
        reply->deleteLater();
    });

    return handle;
}

} // namespace kentos::app
