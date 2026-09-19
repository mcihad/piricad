// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/chat_panel.hpp"

#include "kentos_cad/app/ai_service.hpp"
#include "kentos_cad/app/ai_transport.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/flow_layout.hpp"
#include "kentos_cad/app/provider_service.hpp"
#include "kentos_cad/app/suggestion_card.hpp"
#include "kentos_cad/app/tokens.hpp"

#include "kentos_cad/ai/llmstxt.hpp"
#include "kentos_cad/ai/redact.hpp"
#include "kentos_cad/command/registry.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace kentos::app {
namespace {

/// How many read-tool rounds one question may take before the panel stops.
constexpr int kMaxRounds = 8;

/// The biggest file the panel will attach, and it says the number when it
/// refuses. A provider's own limit is smaller than this for images and larger
/// for text; this is the point past which the context meter stops being
/// advisory and the turn simply fails at the endpoint.
constexpr qint64 kMaxAttachment = 8LL * 1024 * 1024;

/// How often the seconds counter under the dots moves.
constexpr int kTickMs = 1000;

/// The composer: Enter sends, Shift+Enter breaks the line.
///
/// A SUBCLASS RATHER THAN AN EVENT FILTER because the decision is the widget's
/// own: a text box in which Enter sends is a different control from one in which
/// Enter is a newline, and which one this is should be readable where it is
/// built. `QPlainTextEdit` is not one of the controls Article 5.19 names, and
/// this adds no chrome of its own — the one stylesheet still draws it.
class Composer : public QPlainTextEdit
{
public:
    using Send = std::function<void()>;

    Composer(Send send, QWidget* parent) : QPlainTextEdit(parent), send_(std::move(send))
    {
        setObjectName(QStringLiteral("chatComposer"));
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setTabChangesFocus(true);
        // Three lines before it scrolls: enough for a sentence about a parcel and
        // its constraint, and it grows with the text up to that.
        setFixedHeight(64);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
        const bool plain = (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
        if (enter && plain) {
            send_();
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }

private:
    Send send_;
};

} // namespace

/// Bytes from the transport into the decoder, on the GUI thread.
///
/// A MEMBER TYPE RATHER THAN A LAMBDA because `HttpTransport::send` takes the
/// sink BY REFERENCE and keeps using it until the request finishes: it has to
/// outlive the call, and a stack object would be gone by the time the first
/// chunk arrived.
struct ChatPanel::Sink : ai::StreamSink
{
    explicit Sink(ChatPanel& owner) : panel(&owner) {}

    void on_chunk(std::string_view bytes) override
    {
        if (panel->decoder_ == nullptr) return;
        panel->consume(panel->decoder_->feed(bytes));
    }

    void on_finished(int status, std::string_view error) override
    {
        if (panel->decoder_ != nullptr) panel->consume(panel->decoder_->finish());
        panel->finishTurn(status, QString::fromStdString(std::string(error)));
    }

    ChatPanel* panel;
};

ChatPanel::ChatPanel(Controller& controller, AiService& service, AiTransport& transport,
                     QWidget* parent)
    : QWidget(parent), controller_(controller), service_(service), transport_(transport)
{
    setObjectName(QStringLiteral("chatPanel"));

    chat_ = std::make_unique<ai::Conversation>();
    sink_ = std::make_unique<Sink>(*this);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // ---- the head: which model, and a new conversation ----
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("chatHead"));
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(8, 6, 8, 6);
    headRow->setSpacing(6);

    chooser_ = new ComboBox(head);
    chooser_->setAccessibleName(tr("Yapay zeka modeli"));
    headRow->addWidget(chooser_, 1);

    // THE KEY IS FETCHED WHEN SOMEBODY PICKS A MODEL, not when they press send.
    // The lookup is off-thread either way (secret_resolver.hpp), so this is not
    // about speed: it is about WHERE the operating system's authorisation prompt
    // appears. Asked here it belongs to a deliberate act; asked three sentences
    // later it is a prompt the user cannot account for.
    //
    // `activated` RATHER THAN `currentIndexChanged`: the second fires while the
    // list is being filled from the profile store, and a keychain prompt at
    // start-up that nobody asked for is exactly the surprise being avoided.
    connect(chooser_, &QComboBox::activated, this, [this](int) {
        const ai::ProviderProfile* picked = chosen();
        if (picked == nullptr || picked->key_ref.empty()) return;
        controller_.providerService().secrets().prime(QString::fromStdString(picked->key_ref));
    });
    clear_ = new Button(Glyph::New, tr("Yeni sohbet"), head);
    headRow->addWidget(clear_);
    column->addWidget(head);

    trouble_ = new Banner(Tone::Danger, tr("Sağlayıcı hatası"), QString(), this);
    trouble_->setVisible(false);
    column->addWidget(trouble_);

    // ---- the transcript ----
    transcript_ = new Transcript(this);
    transcript_->setPlaceholder(
        tr("Yapmak istediğinizi Türkçe yazın. Model çizimi okur ve komut ÖNERİR; "
           "uygulayan sizsiniz.\n\nÖrnek: \"1284 ada 7 parseli iki eşit parçaya böl.\""));
    column->addWidget(transcript_, 1);

    // ---- the attachments ----
    attachRow_ = new QWidget(this);
    new FlowLayout(attachRow_, 6, 6, 4);
    attachRow_->setVisible(false);
    column->addWidget(attachRow_);

    // ---- the composer ----
    auto* foot = new QWidget(this);
    foot->setObjectName(QStringLiteral("chatFoot"));
    auto* footColumn = new QVBoxLayout(foot);
    footColumn->setContentsMargins(8, 6, 8, 8);
    footColumn->setSpacing(6);

    composer_ = new Composer([this] { ask(composer_->toPlainText()); }, foot);
    composer_->setPlaceholderText(tr("Bir şey sorun ya da yapılacak işi anlatın "
                                     "(Enter gönderir, Shift+Enter satır atlar)"));
    footColumn->addWidget(composer_);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    attachButton_ = new Button(Glyph::Attach, tr("Dosya ekle"), foot);
    buttons->addWidget(attachButton_);
    meter_ = new ContextMeter(foot);
    buttons->addWidget(meter_, 1);
    stopButton_ = new Button(ButtonRole::Secondary, tr("Dur"), Glyph::Stop, foot);
    stopButton_->setControlSize(ControlSize::Compact);
    stopButton_->setVisible(false);
    buttons->addWidget(stopButton_);
    send_ = new Button(ButtonRole::Primary, tr("Gönder"), Glyph::Send, foot);
    send_->setControlSize(ControlSize::Compact);
    buttons->addWidget(send_);
    footColumn->addLayout(buttons);
    column->addWidget(foot);

    connect(send_, &QPushButton::clicked, this, [this] { ask(composer_->toPlainText()); });
    connect(stopButton_, &QPushButton::clicked, this, &ChatPanel::stop);
    connect(clear_, &QPushButton::clicked, this, &ChatPanel::newChat);
    connect(attachButton_, &QPushButton::clicked, this, [this] {
        const QString picked = QFileDialog::getOpenFileName(
            this, tr("Sohbete eklenecek dosya"), QString(),
            tr("Metin, görüntü ve PDF (*.txt *.csv *.json *.md *.png *.jpg *.jpeg *.pdf);;"
               "Tüm dosyalar (*)"));
        if (!picked.isEmpty()) attach(picked);
    });

    tick_ = new QTimer(this);
    tick_->setInterval(kTickMs);
    connect(tick_, &QTimer::timeout, this, [this] {
        if (open_ == nullptr) return;
        const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
        open_->setWaiting(true);
        // The elapsed seconds go on the bubble's own dots; the bubble owns them.
        if (auto* dots = open_->findChild<ThinkingDot*>(); dots != nullptr)
            dots->setElapsedSeconds(static_cast<int>((now - started_ms_) / 1000));
    });

    refreshControls();
    refreshMeter();
}

ChatPanel::~ChatPanel()
{
    // A turn still in flight would call back into a half-destroyed panel.
    if (flight_) flight_->cancel();
}

void ChatPanel::setProfileSource(ProfileSource source)
{
    profiles_ = std::move(source);
    refreshProfiles();
}

void ChatPanel::refreshProfiles()
{
    if (!profiles_) return;
    const QString was = chooser_->currentText();
    known_            = profiles_();

    chooser_->clear();
    for (const ai::ProviderProfile& profile : known_.all())
        chooser_->addItem(QString::fromStdString(profile.name));

    // The default is what a fresh panel is on; a chooser the user has already
    // moved keeps its choice across a profile edit.
    const int keep = chooser_->findText(was);
    if (keep >= 0) {
        chooser_->setCurrentIndex(keep);
    } else {
        const int fallback = chooser_->findText(QString::fromStdString(known_.default_name()));
        chooser_->setCurrentIndex(fallback >= 0 ? fallback : 0);
    }
    refreshControls();
    refreshMeter();
}

const ai::ProviderProfile* ChatPanel::chosen() const
{
    if (chooser_ == nullptr || chooser_->currentText().isEmpty()) return nullptr;
    return known_.find(chooser_->currentText().toStdString());
}

bool ChatPanel::busy() const noexcept
{
    return static_cast<bool>(flight_);
}

void ChatPanel::ask(const QString& text)
{
    const QString said = text.trimmed();
    if (said.isEmpty() || busy()) return;

    const ai::ProviderProfile* profile = chosen();
    if (profile == nullptr) {
        auto* note = new MessageBubble(Speaker::Notice, this);
        note->setNote(tr("Tanımlı bir yapay zeka modeli yok. Seçenekler ▸ Yapay Zeka "
                         "Modelleri sayfasından bir sağlayıcı ekleyin ya da "
                         "YAPAYZEKAMODELİ islem=ekle komutunu kullanın."),
                      Tone::Danger);
        transcript_->append(note);
        note->applyTheme(theme_);
        return;
    }

    auto* mine = new MessageBubble(Speaker::Person, this);
    mine->setText(said);
    transcript_->append(mine);
    mine->applyTheme(theme_);

    ai::Message message = ai::text_message(ai::Role::User, said.toStdString());
    message.attachments = std::move(pending_);
    pending_.clear();
    for (QObject* child : attachRow_->children())
        if (auto* chip = qobject_cast<AttachmentChip*>(child)) chip->deleteLater();
    attachRow_->setVisible(false);

    chat_->add(std::move(message));
    composer_->clear();
    round_ = 0;
    sendRound();
}

void ChatPanel::sendRound()
{
    const ai::ProviderProfile* profile = chosen();
    if (profile == nullptr) return;

    // THE PERMIT IS THE POLICY, and it is a type rather than an `if`. A project
    // marked sensitive (`core.ai.hassas`) permits only a loopback or
    // private-network endpoint, and a caller cannot reach the transport without
    // one (ai.md R14, P3).
    const bool sensitive = controller_.bus().session_settings().get("core.ai.hassas").as_bool();
    auto permitted       = ai::permit_for(*profile, sensitive);
    if (!permitted) {
        auto* note = new MessageBubble(Speaker::Notice, this);
        note->setNote(QString::fromStdString(permitted.error().message), Tone::Danger);
        transcript_->append(note);
        note->applyTheme(theme_);
        emit said(QString::fromStdString(permitted.error().message));
        return;
    }

    trouble_->setVisible(false);

    ai::ChatRequest request;
    request.profile = profile;
    request.chat    = chat_.get();
    request.system  = systemPrompt();
    // TOOLS ONLY WHEN THE PROFILE SAYS THE ENDPOINT CAN TAKE THEM. A small local
    // model that was never tool-trained chokes on the array; the panel then asks
    // in prose, which is a worse conversation but a working one.
    request.catalog = profile->tools ? &service_.catalog() : nullptr;

    beginTurn(*profile);

    transport_.useProfile(*profile);
    flight_ =
        transport_.send(permitted.value(), ai::request_for(request, permitted.value()), *sink_);
    if (!flight_) {
        // The transport refused outright and has already said why on the sink —
        // a profile whose key is KNOWN to be missing is the usual reason. A key
        // nobody has looked up yet does not come back here: the request waits for
        // the key store off-thread and the refusal, if it is one, arrives through
        // `finishTurn` like any other ending (ai_transport.hpp).
        tick_->stop();
    }
    refreshControls();
}

void ChatPanel::beginTurn(const ai::ProviderProfile& profile)
{
    decoder_ = std::make_unique<ai::StreamDecoder>(ai::codec_for(profile.dialect));
    turn_    = std::make_unique<ai::TurnAssembler>(profile.dialect, profile.model);

    open_ = new MessageBubble(Speaker::Model, this);
    open_->setWaiting(true);
    transcript_->append(open_);
    open_->applyTheme(theme_);

    started_ms_ = QDateTime::currentMSecsSinceEpoch();
    tick_->start();
}

void ChatPanel::probeStream(const ai::ProviderProfile& profile, std::string_view bytes)
{
    beginTurn(profile);
    consume(decoder_->feed(bytes));
    consume(decoder_->finish());
    finishTurn(200, QString());
}

void ChatPanel::consume(const std::vector<ai::StreamEvent>& events)
{
    if (turn_ == nullptr || open_ == nullptr) return;

    const bool show = controller_.bus().app_settings().get("core.ai.dusunme_goster").as_bool();

    for (const ai::StreamEvent& event : events) {
        turn_->feed(event);
        switch (event.kind) {
        case ai::EventKind::TextDelta:
            open_->setWaiting(false);
            open_->appendText(QString::fromStdString(event.text));
            break;
        case ai::EventKind::ReasoningDelta:
            // THE SETTING DECIDES WHETHER IT IS SHOWN, never whether it is
            // requested: a profile that asked for thinking gets billed for it
            // either way, and a panel that dropped it silently would be hiding
            // what the user paid for. Off means the dots stay and the text is
            // kept on the message for the audit record.
            if (show) open_->appendReasoning(QString::fromStdString(event.text));
            break;
        case ai::EventKind::Error:
            open_->setNote(tr("Sağlayıcı hatası: %1").arg(QString::fromStdString(event.text)),
                           Tone::Danger);
            break;
        case ai::EventKind::None:
        case ai::EventKind::ToolCallStart:
        case ai::EventKind::ToolCallArguments:
        case ai::EventKind::ToolCallEnd:
        case ai::EventKind::Opaque:
        case ai::EventKind::Usage:
        case ai::EventKind::Done: break;
        }
    }
    transcript_->bumped();
    refreshMeter();
}

int ChatPanel::runReadTools(const std::vector<ai::Block>& calls)
{
    int ran = 0;
    for (const ai::Block& call : calls) {
        const ai::ToolDef* tool = service_.catalog().find(call.tool_name);
        if (tool == nullptr) continue;
        if (tool->mutates) continue;

        auto step = ai::plan_step_for(call, service_.catalog());
        if (!step) {
            // A REFUSED CALL GOES BACK TO THE MODEL AS A FAILURE, not into the
            // void: a model that is not told its argument was rejected asks the
            // same way again (ai.md R19).
            chat_->add(ai::tool_result_message(call, step.error().message, true));
            auto* bubble = new MessageBubble(Speaker::Notice, this);
            bubble->setNote(QString::fromStdString(step.error().message), Tone::Danger);
            transcript_->append(bubble);
            bubble->applyTheme(theme_);
            ++ran;
            continue;
        }

        auto outcome =
            service_.run_read_only(step.value().command_id, step.value().args, requesterLabel());
        if (!outcome) {
            chat_->add(ai::tool_result_message(call, outcome.error().message, true));
            ++ran;
            continue;
        }

        std::string told;
        for (const std::string& line : outcome.value().lines) {
            if (!told.empty()) told += "\n";
            told += line;
        }
        // THE MINTED HANDLES ARE PART OF THE ANSWER. They are how the next call
        // points at what this one found, and the only way it can name a
        // coordinate at all (CLAUDE.md 5.8).
        for (const std::string& handle : outcome.value().minted)
            told += "\n" + handle;

        chat_->add(ai::tool_result_message(call, told, false));

        auto* bubble = new MessageBubble(Speaker::ToolResult, this);
        bubble->setText(QString::fromStdString(tool->title + ": " + told));
        transcript_->append(bubble);
        bubble->applyTheme(theme_);
        ++ran;
    }
    return ran;
}

std::string ChatPanel::requesterLabel() const
{
    const ai::ProviderProfile* profile = chosen();
    return profile != nullptr ? "sohbet · " + profile->name : std::string("sohbet");
}

QString ChatPanel::fileWrites(const std::vector<ai::Block>& calls)
{
    ai::Plan plan;
    plan.requester = requesterLabel();
    plan.model     = turn_ != nullptr ? turn_->message().model : std::string();
    if (const ai::ProviderProfile* profile = chosen(); profile != nullptr)
        plan.endpoint = ai::endpoint_url(*profile);
    for (std::size_t i = chat_->size(); i > 0; --i) {
        const ai::Message& message = chat_->messages()[i - 1];
        if (message.role == ai::Role::User) {
            plan.prompt = message.text();
            break;
        }
    }

    std::vector<QString> refusals;
    for (const ai::Block& call : calls) {
        const ai::ToolDef* tool = service_.catalog().find(call.tool_name);
        if (tool != nullptr && !tool->mutates) continue;

        auto step = ai::plan_step_for(call, service_.catalog());
        if (!step) {
            refusals.push_back(QString::fromStdString(step.error().message));
            chat_->add(ai::tool_result_message(call, step.error().message, true));
            continue;
        }
        plan.steps.push_back(step.value());
    }

    for (const QString& refusal : refusals) {
        auto* bubble = new MessageBubble(Speaker::Notice, this);
        bubble->setNote(refusal, Tone::Danger);
        transcript_->append(bubble);
        bubble->applyTheme(theme_);
        emit said(refusal);
    }

    if (plan.steps.empty()) return {};

    // ONE PLAN FOR THE WHOLE TURN, so a model that asked for eleven commands is
    // one card, one decision and one undo entry (ai.md R4).
    auto filed = service_.propose(std::move(plan));
    if (!filed) {
        emit said(QString::fromStdString(filed.error().message));
        return {};
    }

    const QString id = QString::fromStdString(filed.value());
    for (const ai::Block& call : calls) {
        const ai::ToolDef* tool = service_.catalog().find(call.tool_name);
        if (tool != nullptr && !tool->mutates) continue;
        chat_->add(ai::tool_result_message(
            call,
            "Öneri " + filed.value() +
                " olarak kaydedildi. Uygulanmadı: bilgisayar başındaki mühendis "
                "onaylayana kadar çizim değişmez. Durumu 'oneri_durumu' ile "
                "sorabilirsiniz.",
            false));
    }
    return id;
}

void ChatPanel::finishTurn(int status, const QString& trouble)
{
    tick_->stop();
    flight_.reset();

    if (turn_ == nullptr || open_ == nullptr) {
        refreshControls();
        return;
    }

    open_->setWaiting(false);

    const std::vector<ai::Block> calls = turn_->tool_calls();
    const ai::TokenUsage usage         = turn_->usage();
    ai::Message answer                 = turn_->take();
    const bool failed                  = !trouble.isEmpty() || (status != 0 && status >= 400);

    if (failed) {
        const QString what =
            trouble.isEmpty() ? tr("Sağlayıcı %1 yanıtı verdi.").arg(status) : trouble;
        // A FAILED TURN IS NOT KEPT. The half-answer stays on screen as it
        // stands — the user watched it arrive — but it does not go into the
        // conversation, because replaying a truncated assistant turn is how a
        // model is taught to truncate (ai.md R20's spirit for the chat road).
        open_->setNote(what, Tone::Danger);
        trouble_->setText(what);
        trouble_->setVisible(true);
        emit said(what);
        open_ = nullptr;
        turn_.reset();
        decoder_.reset();
        refreshControls();
        refreshMeter();
        return;
    }

    chat_->add(std::move(answer));
    chat_->reconcile(usage);

    MessageBubble* bubble = open_;
    open_                 = nullptr;
    turn_.reset();
    decoder_.reset();

    if (!calls.empty()) {
        const QString filed = fileWrites(calls);
        if (!filed.isEmpty()) {
            auto* card = new SuggestionCard(service_, filed, bubble);
            connect(card, &SuggestionCard::settled, this, [this](const QString& id, bool applied) {
                emit said(applied ? tr("Öneri %1 uygulandı.").arg(id)
                                  : tr("Öneri %1 reddedildi.").arg(id));
            });
            bubble->setFooter(card);
            bubble->applyTheme(theme_);
        }

        const int reads = runReadTools(calls);
        if (reads > 0 && filed.isEmpty()) {
            if (++round_ < kMaxRounds) {
                sendRound();
                return;
            }
            auto* stopped = new MessageBubble(Speaker::Notice, this);
            stopped->setNote(tr("Model %1 turdur okumaya devam ediyor; durduruldu. "
                                "Sorunuzu daha somut yazmayı deneyin.")
                                 .arg(kMaxRounds),
                             Tone::Warn);
            transcript_->append(stopped);
            stopped->applyTheme(theme_);
        }
    }

    refreshControls();
    refreshMeter();
    transcript_->bumped();
}

void ChatPanel::stop()
{
    if (!flight_) return;
    flight_->cancel();
    flight_.reset();
    tick_->stop();
    if (open_ != nullptr) {
        open_->setWaiting(false);
        open_->setNote(tr("İptal edildi. Çizimde hiçbir şey değişmedi."), Tone::Warn);
        open_ = nullptr;
    }
    // THE HALF-BUILT TURN IS DROPPED, which is the whole reason it lives outside
    // the conversation: a cancelled turn leaves the model's history exactly as
    // it was (ai.md R20).
    turn_.reset();
    decoder_.reset();
    refreshControls();
}

void ChatPanel::newChat()
{
    stop();
    chat_->clear();
    transcript_->clear();
    pending_.clear();
    for (QObject* child : attachRow_->children())
        if (auto* chip = qobject_cast<AttachmentChip*>(child)) chip->deleteLater();
    attachRow_->setVisible(false);
    trouble_->setVisible(false);
    round_ = 0;
    refreshMeter();
    refreshControls();
}

void ChatPanel::attach(const QString& path)
{
    QFile file(path);
    const QFileInfo about(path);
    if (about.size() > kMaxAttachment) {
        emit said(tr("Dosya çok büyük: %1. En çok %2 eklenebilir.")
                      .arg(formatByteCount(about.size()), formatByteCount(kMaxAttachment)));
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        emit said(tr("Dosya okunamadı: %1").arg(path));
        return;
    }
    const QByteArray bytes = file.readAll();

    ai::Attachment attachment;
    attachment.name = about.fileName().toStdString();
    attachment.media_type =
        QMimeDatabase().mimeTypeForFileNameAndData(path, bytes).name().toStdString();
    attachment.bytes.assign(bytes.constData(), bytes.constData() + bytes.size());
    pending_.push_back(std::move(attachment));

    auto* chip = new AttachmentChip(about.fileName(), about.size(),
                                    QString::fromStdString(pending_.back().media_type), attachRow_);
    attachRow_->layout()->addWidget(chip);
    attachRow_->setVisible(true);
    chip->applyTheme(theme_);

    // TWO POINTERS, NO STRING: the chip already knows its own file name, and a
    // captured `std::string` makes the handler's own copy able to throw — inside
    // a signal delivery, where an escaping exception ends the process.
    connect(chip, &AttachmentChip::removeRequested, this, [this, chip] {
        const std::string name = chip->fileName().toStdString();
        std::erase_if(pending_, [&name](const ai::Attachment& a) { return a.name == name; });
        chip->deleteLater();
        attachRow_->setVisible(!pending_.empty());
        refreshMeter();
    });
    refreshMeter();
}

std::string ChatPanel::systemPrompt() const
{
    // THE SAME PARAGRAPHS `docs/llms.txt` OPENS WITH. An outside agent reads the
    // file; the model inside the program is told the identical rules in the
    // identical words, because two wordings would be two programs to learn
    // (`ai/llmstxt.hpp`, CLAUDE.md 5.20).
    std::string out = ai::agent_preamble();

    // AND WHAT IS ON SCREEN RIGHT NOW, which is the one thing the file cannot
    // carry. It answers "which corner coordinates am I looking at" without the
    // model having to spend a tool call on it, and it comes from
    // `Bus::on_view_query` — the same source `GÖRÜNÜMBİLGİSİ` reads, so the
    // prompt and the tool cannot disagree.
    if (const std::optional<command::ViewInfo> view = service_.view(); view) {
        out += "\n## Şu an ekranda\n\n";
        out += "- Görünen alan (mm): Sağa " + std::to_string(view->window.min_x) + " … " +
               std::to_string(view->window.max_x) + ", Yukarı " +
               std::to_string(view->window.min_y) + " … " + std::to_string(view->window.max_y) +
               "\n";
        out += "- Ölçek: 1:" + std::to_string(view->scale) + "\n";
        out += "- CRS: " + view->crs + "\n";
    }
    out += "\n- Belge sürümü: " + std::to_string(service_.revision()) + "\n";
    return out;
}

void ChatPanel::refreshMeter()
{
    const ai::TokenUsage usage = chat_->usage();
    std::int64_t used          = usage.total();
    if (used == 0) used = chat_->estimate().total();

    // The attachments are charged too, and they are the biggest single thing a
    // user adds without noticing.
    for (const ai::Attachment& attachment : pending_)
        used += ai::estimate_tokens(std::string_view(
            reinterpret_cast<const char*>(attachment.bytes.data()), attachment.bytes.size()));

    std::int64_t window = 0;
    if (const ai::ProviderProfile* profile = chosen(); profile != nullptr)
        window = profile->context.tokens;
    meter_->setUsage(used, window, usage.reported);
}

void ChatPanel::refreshControls()
{
    const bool running = busy();
    send_->setEnabled(!running && chosen() != nullptr);
    composer_->setEnabled(!running);
    attachButton_->setEnabled(!running);
    clear_->setEnabled(!running);
    stopButton_->setVisible(running);
    chooser_->setEnabled(!running);
}

void ChatPanel::applyTheme(ThemeMode mode)
{
    theme_ = mode;
    applyThemeToChildren(this, mode);
    update();
}

} // namespace kentos::app
