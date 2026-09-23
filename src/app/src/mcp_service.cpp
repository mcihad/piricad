// SPDX-License-Identifier: AGPL-3.0-or-later
//
// AGPL because this file is the server (CLAUDE.md 2.1, §1); see the header.
#include "kentos_cad/app/mcp_service.hpp"

#include "kentos_cad/app/ai_service.hpp"

#include <QDateTime>
#include <QEventLoop>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

namespace kentos::app {
namespace {

/// Qt's status enum from a bare number, for the four statuses this server sends.
///
/// A SWITCH RATHER THAN A CAST, because a number that is not in the enum is
/// undefined behaviour waiting for the one status we forgot; and because these
/// four are the whole conformance surface (the protocol layer decides which).
QHttpServerResponder::StatusCode status_of(int code)
{
    using Code = QHttpServerResponder::StatusCode;
    switch (code) {
    case 200: return Code::Ok;
    case 202: return Code::Accepted;
    case 400: return Code::BadRequest;
    case 401: return Code::Unauthorized;
    case 403: return Code::Forbidden;
    case 404: return Code::NotFound;
    case 405: return Code::MethodNotAllowed;
    case 406: return Code::NotAcceptable;
    case 413: return Code::PayloadTooLarge;
    case 415: return Code::UnsupportedMediaType;
    case 429: return Code::TooManyRequests;
    case 503: return Code::ServiceUnavailable;
    default:
        // A STATUS THIS SWITCH DOES NOT KNOW BECOMES 500, and that is worth
        // knowing about rather than papering over: a wrong token was answered
        // 500 instead of 401 the first time this ran, because 401 was missing
        // from the list. The protocol layer decides the status; this function
        // only spells it in Qt's vocabulary.
        return Code::InternalServerError;
    }
}

/// The verb as the protocol layer wants it: a plain word.
const char* verb_of(QHttpServerRequest::Method method)
{
    switch (method) {
    case QHttpServerRequest::Method::Post: return "POST";
    case QHttpServerRequest::Method::Get: return "GET";
    case QHttpServerRequest::Method::Delete: return "DELETE";
    case QHttpServerRequest::Method::Put: return "PUT";
    case QHttpServerRequest::Method::Head: return "HEAD";
    case QHttpServerRequest::Method::Options: return "OPTIONS";
    case QHttpServerRequest::Method::Patch: return "PATCH";
    default: return "OTHER";
    }
}

} // namespace

McpService::McpService(command::Bus& bus, AiService& ai, QObject* parent)
    : QObject(parent), bus_(bus), ai_(ai)
{
    ai_.setServerHandler(
        [this](const command::Bus::AiRequest& request) { return handleVerb(request); });
}

McpService::~McpService()
{
    stop();
    ai_.setServerHandler(nullptr);
}

bool McpService::listening() const noexcept
{
    return tcp_ != nullptr && tcp_->isListening();
}

bool McpService::tokenRequired() const noexcept
{
    return require_token_;
}

QString McpService::tokenFingerprint() const
{
    if (token_.isEmpty()) return QStringLiteral("(yok)");
    // ENOUGH TO TELL TWO TOKENS APART, USELESS TO AN INTERCEPTOR. The full token
    // exists in one place a person can read it — the settings page's copy field
    // — and nowhere a log, a transcript or an audit record can reach
    // (CLAUDE.md 5.21).
    return token_.left(6) + QStringLiteral("… (%1 karakter)").arg(token_.size());
}

QString McpService::mintToken()
{
    // 128 bits from the system's own generator. `QRandomGenerator::system()` is
    // the CSPRNG; the global one is seeded for speed and is not for a secret.
    QString out;
    out.reserve(32);
    for (int i = 0; i < 4; ++i)
        out += QStringLiteral("%1").arg(QRandomGenerator::system()->generate(), 8, 16,
                                        QLatin1Char('0'));
    return out;
}

core::Result<QString> McpService::start(quint16 wanted)
{
    // THE SENSITIVITY FLAG STOPS THE LISTENER, and it stops it here rather than
    // filtering tools later: a project whose data may not leave the institution
    // has no business accepting a connection from a client that might be a cloud
    // agent (.claude/ai.md R14, R30, P3).
    if (bus_.project_settings().get("core.ai.hassas").as_bool())
        return core::err(core::ErrorCode::Unsupported,
                         "Bu proje hassas işaretli: MCP sunucusu başlatılmaz. Ayarı "
                         "Seçenekler ▸ Yapay Zeka Modelleri sayfasından değiştirebilirsiniz.");

    const auto configured = static_cast<quint16>(bus_.app_settings().get("core.mcp.port").as_int());
    const quint16 port    = wanted != 0 ? wanted : configured;
    require_token_        = bus_.app_settings().get("core.mcp.belirtec_zorunlu").as_bool();

    if (listening()) {
        if (port_ == port) return QStringLiteral("MCP sunucusu zaten açık: %1").arg(port_);
        stop(); // a different port means a restart, not a second listener
    }

    if (require_token_ && token_.isEmpty()) token_ = mintToken();

    ai::ServerPolicy policy;
    policy.path          = "/mcp";
    policy.token         = require_token_ ? token_.toStdString() : std::string();
    policy.require_token = require_token_;

    ai::ServerInfo info;
    info.version = KENTOS_VERSION;

    mcp_ =
        std::make_unique<ai::McpServer>(ai_, bus_.registry(), std::move(info), std::move(policy));

    // THE LEDGER OUTLIVES THE ENGINE. Stopping to change a port must not undo a
    // revocation somebody made an hour ago, so the record lives here and the
    // engine is handed a pointer to it (`ai::McpServer::set_ledger`).
    mcp_->set_ledger(&clients_);

    http_ = std::make_unique<QHttpServer>();
    // ONE HANDLER FOR EVERY REQUEST, no routes. The path rule belongs to
    // `ai::endpoint_match` — it is the thing that knows about the token segment
    // — and a Qt route table beside it would be a second answer to the same
    // question, with the two disagreeing the day the path changes.
    http_->setMissingHandler(
        this, [this](const QHttpServerRequest& request, QHttpServerResponder& responder) {
            serve(request, responder);
        });

    auto tcp = std::make_unique<QTcpServer>();
    // 127.0.0.1 ONLY, and the specification says SHOULD where we say MUST: a
    // listener on 0.0.0.0 is reachable from the network the machine is on, and
    // this one drives somebody's cadastral drawing. There is no setting that
    // widens it (CLAUDE.md 2.10).
    if (!tcp->listen(QHostAddress::LocalHost, port)) {
        const QString why = tcp->errorString();
        http_.reset();
        mcp_.reset();
        return core::err(core::ErrorCode::IoFailure,
                         ("MCP sunucusu " + QString::number(port) + " portunda açılamadı: " + why +
                          ". Başka bir port deneyin: MCPSUNUCU islem=baslat port=<numara>")
                             .toStdString());
    }
    // BIND TAKES OWNERSHIP, and forgetting that is a double free: `QHttpServer`
    // reparents the `QTcpServer`, so destroying the server destroys the socket
    // too. Holding it in a `unique_ptr` as well crashed `stop()` on the second
    // delete — which is exactly what happened the first time this ran. The raw
    // pointer below is a handle for `isListening()` and `close()`, not ownership.
    if (!http_->bind(tcp.get())) {
        http_.reset();
        mcp_.reset();
        return core::err(core::ErrorCode::Internal, "MCP sunucusu porta bağlanamadı.");
    }
    tcp_ = tcp.release();

    port_ = port;
    emit stateChanged();

    const QString where = require_token_
                              ? QStringLiteral("http://127.0.0.1:%1/mcp/<belirteç>").arg(port_)
                              : QStringLiteral("http://127.0.0.1:%1/mcp").arg(port_);
    return QStringLiteral("MCP sunucusu açık: %1 (MCP %2). %3")
        .arg(where, QString::fromLatin1(ai::Catalog::kProtocolVersion),
             require_token_ ? QStringLiteral("Belirteç: %1 — tamamını Seçenekler ▸ MCP "
                                             "Sunucusu sayfasından kopyalayın.")
                                  .arg(tokenFingerprint())
                            : QStringLiteral("KORUMASIZ: belirteç istenmiyor, bu makinedeki "
                                             "her süreç bağlanabilir."));
}

void McpService::stop()
{
    if (tcp_ != nullptr) tcp_->close();
    // The socket belongs to the server (see `bind` above): destroying the server
    // destroys it, and this pointer is only a handle.
    http_.reset();
    tcp_ = nullptr;
    mcp_.reset();
    port_ = 0;
    emit stateChanged();
}

void McpService::serve(const QHttpServerRequest& request, QHttpServerResponder& responder)
{
    if (mcp_ == nullptr) {
        responder.write(QByteArray(), "text/plain", status_of(503));
        return;
    }

    // The body and the header values must outlive the view, which holds views
    // into them.
    const QByteArray body   = request.body();
    const QByteArray path   = request.url().path().toUtf8();
    const QHttpHeaders head = request.headers();
    const auto value = [&head](QByteArrayView name) { return head.value(name).toByteArray(); };
    const QByteArray origin  = value("origin");
    const QByteArray version = value("mcp-protocol-version");
    const QByteArray method  = value("mcp-method");
    const QByteArray name    = value("mcp-name");
    const QByteArray session = value("mcp-session-id");
    const QByteArray lastId  = value("last-event-id");
    const QByteArray accept  = value("accept");
    const QByteArray auth    = value("authorization");

    ai::HttpRequestView view;
    view.method = verb_of(request.method());
    view.path   = std::string_view(path.constData(), static_cast<std::size_t>(path.size()));
    view.origin = std::string_view(origin.constData(), static_cast<std::size_t>(origin.size()));
    view.protocol_version =
        std::string_view(version.constData(), static_cast<std::size_t>(version.size()));
    view.mcp_method = std::string_view(method.constData(), static_cast<std::size_t>(method.size()));
    view.mcp_name   = std::string_view(name.constData(), static_cast<std::size_t>(name.size()));
    view.session_id =
        std::string_view(session.constData(), static_cast<std::size_t>(session.size()));
    view.last_event_id =
        std::string_view(lastId.constData(), static_cast<std::size_t>(lastId.size()));
    view.accept = std::string_view(accept.constData(), static_cast<std::size_t>(accept.size()));
    view.authorization = std::string_view(auth.constData(), static_cast<std::size_t>(auth.size()));
    view.body          = std::string_view(body.constData(), static_cast<std::size_t>(body.size()));

    // THE ENGINE HAS NO CLOCK (it is sans-IO), so the "last seen" column can only
    // be what the transport tells it. This is the transport.
    view.received_at = static_cast<std::uint64_t>(QDateTime::currentSecsSinceEpoch());

    const ai::HttpOutcome outcome = mcp_->handle(view);

    // THE LEDGER MOVED, so anything showing it must be told. `handle` recorded
    // the call; this is the only place that knows the recording just happened.
    if (!outcome.audit.requester.empty()) emit clientsChanged();

    // THE AUDIT NOTE IS WRITTEN WHATEVER HAPPENED, and the coordinate refusal is
    // the one the rulebook asks for by name: a call that tried to pass a number
    // where a handle was declared never becomes a plan, so this is the only
    // place it can leave a trace (.claude/ai.md R10).
    if (outcome.audit.coordinate_refusal) ai_.recordCoordinateRefusal(outcome.audit);
    if (outcome.audit.escalation_refusal) ai_.recordEscalationRefusal(outcome.audit);

    QHttpHeaders headers;
    for (const auto& [key, held] : outcome.headers)
        headers.append(QByteArray::fromStdString(key), QByteArray::fromStdString(held));

    switch (outcome.delivery) {
    case ai::Delivery::Json:
        if (headers.value("content-type").isEmpty())
            headers.append("content-type", "application/json");
        responder.write(QByteArray::fromStdString(outcome.body), headers,
                        status_of(outcome.status));
        return;

    case ai::Delivery::Accepted:
    case ai::Delivery::Empty:
        responder.write(QByteArray(), headers, status_of(outcome.status));
        return;

    case ai::Delivery::Sse: {
        // ONE SHOT, AND THE LIMIT IS DOCUMENTED. Every frame the protocol layer
        // produced is written and the stream is closed; a `keep_open` stream —
        // only `subscriptions/listen` asks for one — is answered with its
        // acknowledgement and then closed, so a client that wants change
        // notifications reconnects instead of holding a socket. Holding one
        // needs a stored `QHttpServerResponder` and a keep-alive timer per
        // client, which is the next landing's work rather than something to
        // fake now (CLAUDE.md 11.8: no aspirational present tense).
        QHttpHeaders sse = headers;
        if (sse.value("content-type").isEmpty()) sse.append("content-type", "text/event-stream");
        if (sse.value("x-accel-buffering").isEmpty()) sse.append("x-accel-buffering", "no");

        QByteArray payload;
        for (const ai::SseFrame& frame : outcome.stream.initial)
            payload += QByteArray::fromStdString(frame.render());
        responder.write(payload, sse, status_of(outcome.status));

        // ONLY A STREAM WE WERE HOLDING COUNTS AS A DISCONNECT. Closing the SSE
        // response of a FINISHED request is not a cancellation: the request was
        // answered. Treating it as one withdrew the very suggestion the call had
        // just filed — the plan appeared on the engineer's screen and vanished
        // before they could read it, and the client was told it had been
        // cancelled. Only a `keep_open` stream (a `subscriptions/listen`) is a
        // socket the client expected us to hold, so only that one reports a
        // disconnect when we let it go.
        if (outcome.stream.keep_open) mcp_->stream_closed(outcome.stream);
        return;
    }
    }
}

core::Status McpService::revokeClient(const QString& label)
{
    if (auto done = clients_.revoke(label.toStdString()); !done) return done;
    emit clientsChanged();
    return core::ok();
}

core::Status McpService::restoreClient(const QString& label)
{
    if (auto done = clients_.restore(label.toStdString()); !done) return done;
    emit clientsChanged();
    return core::ok();
}

core::Result<QString> McpService::probe()
{
    if (!listening())
        return core::err(core::ErrorCode::Unsupported,
                         "Sunucu kapalı; sınanacak bir bağlantı yok. Açmak için: MCPSUNUCU "
                         "islem=baslat");

    // ---- WHY A NESTED EVENT LOOP, AND WHY IT IS SAFE HERE ---------------------
    //
    // This server serves on THIS thread (see the header: the document takes no
    // locks). So a probe that blocked — `waitForReadyRead` on the socket — would
    // wait for an answer that only this thread can produce: a deadlock, and one
    // that would freeze the window rather than fail. The loop below lets the
    // listener answer while the probe waits.
    //
    // USER INPUT IS EXCLUDED, which is what keeps re-entrancy out: no click, no
    // keystroke and no menu reaches the program while the loop spins, so nothing
    // can start a second command underneath this one. `probing_` refuses the one
    // path that is left — a probe started by something the loop DID deliver.
    //
    // AND IT IS BOUNDED. Two seconds against a listener on this same machine is
    // a long time; if nothing came back, something is wrong and saying so beats
    // waiting.
    if (probing_) return core::err(core::ErrorCode::Unsupported, "Bir sınama zaten sürüyor.");

    struct Guard
    {
        bool& flag;

        ~Guard() { flag = false; }
    };

    probing_ = true;
    Guard guard{probing_};

    // THE ADDRESS A CLIENT WOULD BE GIVEN, token segment and all, because what
    // is being tested is the address on the settings page and not some other one
    // this function made up.
    QString path = QStringLiteral("/mcp");
    if (require_token_ && !token_.isEmpty()) path += QStringLiteral("/") + token_;

    QNetworkRequest asked{QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port_).arg(path))};
    asked.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    asked.setRawHeader("Accept", "application/json");
    asked.setRawHeader("MCP-Protocol-Version", ai::Catalog::kProtocolVersion);
    asked.setRawHeader("Mcp-Method", "server/discover");

    // `server/discover` is the one method every conforming client calls first,
    // so a probe that succeeds has proved exactly what a client needs.
    const QByteArray body =
        QStringLiteral(
            R"({"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"%1":"%2"}}})")
            .arg(QString::fromLatin1(ai::kProtocolVersionMetaKey),
                 QString::fromLatin1(ai::Catalog::kProtocolVersion))
            .toUtf8();

    QNetworkAccessManager net;
    QNetworkReply* reply = net.post(asked, body);

    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(2000);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    const bool answered = reply->isFinished();
    const int status    = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString why   = reply->errorString();
    const QByteArray in = reply->readAll();
    reply->deleteLater();

    if (!answered)
        return core::err(core::ErrorCode::IoFailure,
                         ("Sunucu 2 saniyede cevap vermedi (127.0.0.1:" + QString::number(port_) +
                          "). Sunucuyu durdurup yeniden başlatmayı deneyin.")
                             .toStdString());

    if (status == 401)
        return core::err(core::ErrorCode::ValidationFailed,
                         "Sunucu açık ama belirteci kabul etmedi (401). Ayarlar sayfasındaki "
                         "adresi yeniden kopyalayın.");

    if (status != 200)
        return core::err(core::ErrorCode::IoFailure,
                         ("Sunucu " + QString::number(status) + " döndü: " + why).toStdString());

    // THE ANSWER IS READ, not merely counted. A 200 carrying something that is
    // not this server's discovery document means something else is on that port,
    // and telling the person "çalışıyor" would be worse than telling them
    // nothing.
    if (!in.contains("\"tools\"") && !in.contains("protocolVersion"))
        return core::err(core::ErrorCode::ValidationFailed,
                         "127.0.0.1:" + std::to_string(port_) +
                             " cevap verdi ama bu bir KentOSCad MCP sunucusu değil. Portu "
                             "başka bir program kullanıyor olabilir.");

    return QStringLiteral(
               "Bağlantı çalışıyor: 127.0.0.1:%1, MCP %2, %3 araç. Ajana verilecek adres bu "
               "sayfadaki adrestir.")
        .arg(port_)
        .arg(QString::fromLatin1(ai::Catalog::kProtocolVersion))
        .arg(ai_.catalog().tools.size());
}

core::Result<std::string> McpService::handleVerb(const command::Bus::AiRequest& request)
{
    using Verb = command::Bus::AiRequest::Verb;
    switch (request.verb) {
    case Verb::ServerStart: {
        auto started = start(static_cast<quint16>(request.port));
        if (!started) return started.error();
        return started.value().toStdString();
    }

    case Verb::ServerStop:
        if (!listening()) return std::string("MCP sunucusu zaten kapalı.");
        stop();
        return std::string("MCP sunucusu kapatıldı.");

    case Verb::ServerState: {
        if (!listening())
            return std::string("MCP sunucusu kapalı. Açmak için: MCPSUNUCU islem=baslat");
        const ai::Catalog& catalogue = ai_.catalog();
        return "MCP sunucusu açık: 127.0.0.1:" + std::to_string(port_) + ", MCP " +
               std::string(ai::Catalog::kProtocolVersion) + ", " +
               std::to_string(catalogue.tools.size()) + " araç (" +
               std::to_string(catalogue.mutating_count()) + " tanesi onay ister), belirteç " +
               (require_token_ ? tokenFingerprint().toStdString() : std::string("YOK — KORUMASIZ"));
    }

    case Verb::ServerToken: {
        // A NEW TOKEN INVALIDATES THE OLD ONE, which is the point: a token that
        // may have been seen is replaced, and every client has to be told the
        // new one deliberately. The value itself is never in this answer.
        token_ = mintToken();

        // AND THE CLIENT LIST GOES WITH IT. Every label carries the OLD token's
        // fingerprint, so after this not one of them can be presented again:
        // keeping them would be keeping names nobody will answer to, and keeping
        // their revocations would be shutting out labels that cannot recur.
        clients_.forget_all();
        emit clientsChanged();

        if (listening()) {
            const quint16 again = port_;
            stop();
            if (auto restarted = start(again); !restarted) return restarted.error();
        }
        return "Yeni belirteç üretildi (" + tokenFingerprint().toStdString() +
               "). Tamamını Seçenekler ▸ MCP Sunucusu sayfasından kopyalayın; eski belirteç "
               "artık geçersiz.";
    }

    case Verb::ServerClients: {
        const std::vector<ai::ClientRecord> held = clients_.clients();
        if (held.empty())
            return std::string(
                "Bu sunucuya henüz hiçbir istemci bağlanmadı. Bağlanan her ajan burada "
                "adı, çağrı sayısı ve son hatasıyla görünür.");

        // MOST RECENTLY SEEN FIRST, which the ledger already sorted. The token
        // is not here and cannot be: a label is the client's declared name plus
        // the token's FINGERPRINT (CLAUDE.md 5.21).
        std::string told = std::to_string(held.size()) + " istemci (en son konuşan üstte):";
        for (const ai::ClientRecord& one : held) {
            told += "\n  " + one.label + (one.revoked ? "  [YETKİSİZ]" : "");
            told += "  " + std::to_string(one.calls) + " çağrı";
            if (one.refusals != 0) told += ", " + std::to_string(one.refusals) + " ret";
            if (one.plans != 0) told += ", " + std::to_string(one.plans) + " öneri";
            if (!one.last_method.empty()) told += ", son: " + one.last_method;
            if (!one.last_refusal.empty()) told += "\n      son hata: " + one.last_refusal;
        }
        return told;
    }

    case Verb::ServerRevoke: {
        if (auto done = revokeClient(QString::fromStdString(request.client)); !done)
            return done.error();
        return "'" + request.client +
               "' artık bu sunucuya erişemiyor. Belirteç değişmedi: diğer ajanlar çalışmaya "
               "devam eder. Geri vermek için: MCPSUNUCU islem=izin ad=" +
               request.client;
    }

    case Verb::ServerRestore: {
        if (auto done = restoreClient(QString::fromStdString(request.client)); !done)
            return done.error();
        return "'" + request.client + "' yeniden bu sunucuya erişebilir.";
    }

    case Verb::ServerProbe: {
        auto answered = probe();
        if (!answered) return answered.error();
        return answered.value().toStdString();
    }

    default: break;
    }
    return core::err(core::ErrorCode::Internal, "Bu istek MCP sunucusuna ait değil.");
}

} // namespace kentos::app
