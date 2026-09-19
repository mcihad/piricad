// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the chat surface. The SECOND road to the same command system.
//
// TWO ROADS, ONE ENGINE. An outside agent reaches this program through the MCP
// server (`mcp_service.hpp`); the engineer at the workstation reaches the same
// place through this panel. They share the catalogue (`ai::build_catalog` over
// `Registry`), the handle store, the plan store, the audit log, the approval gate
// and the transaction — everything except the wire. Both roads were required to
// share one infrastructure and one command system, and the reason is Article
// 1.2: a capability this panel has and the server lacks is a capability nobody
// can teach an agent (CLAUDE.md 2.8).
//
// THE MODEL COMPOSES; THE PERSON APPLIES. A decoded tool call never dispatches
// anything. A read tool — `Flags::NoEffect`, nothing else — runs at once and its
// result goes back to the model, because reading the drawing is what lets a model
// stop inventing it. Everything that writes becomes ONE plan for the turn, is
// filed as a suggestion, and appears as a `SuggestionCard` inside the answer's own
// bubble, where `Uygula` is the only thing that can turn it into an edit
// (CLAUDE.md 5.7, ai.md R3/R4/P1).
//
// AND IT IS A DOCK, NOT A WINDOW. The preview is on the canvas and the drawing is
// what the conversation is about, so the transcript sits beside it; a modal chat
// would put the program's own subject behind the conversation about it.
#pragma once

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/widgets.hpp"

#include "kentos_cad/ai/chat.hpp"
#include "kentos_cad/ai/dialect.hpp"
#include "kentos_cad/ai/provider.hpp"

#include <QString>
#include <QWidget>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

class QPlainTextEdit;

namespace kentos::app {

/// The AI layer's door to the document; see ai_service.hpp.
class AiService;
/// The outbound wire; see ai_transport.hpp.
class AiTransport;
/// The one road from a widget to the document; see controller.hpp.
class Controller;
/// The key store; see secret_store.hpp.
class SecretStore;

/// The conversation panel: a provider chooser, the transcript, the composer and
/// the context meter.
class ChatPanel : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Takes the shell's ONE transport rather than making its own: it is the only
    /// object that reads a credential, and a second one would be a second place
    /// to audit (`Controller::aiTransport`).
    ChatPanel(Controller& controller, AiService& service, AiTransport& transport,
              QWidget* parent = nullptr);
    ~ChatPanel() override;

    ChatPanel(const ChatPanel&)            = delete;
    ChatPanel& operator=(const ChatPanel&) = delete;

    /// Where the panel gets the endpoints it can talk to.
    ///
    /// A FUNCTION RATHER THAN A STORE, so that the panel holds no copy that can
    /// go stale: the settings page edits the profiles through `YAPAYZEKAMODELİ`
    /// and this panel re-reads them on every send. One source, and it is the
    /// same file the command writes (5.10, in spirit).
    using ProfileSource = std::function<ai::ProviderProfiles()>;
    void setProfileSource(ProfileSource source);

    /// Re-reads the profiles and rebuilds the chooser. Called when the settings
    /// page or the command has changed them.
    void refreshProfiles();

    /// Puts `text` in the composer and sends it. What a toolbar shortcut or a
    /// probe uses; it is the same path the keyboard takes.
    void ask(const QString& text);

    /// Whether a turn is in flight.
    bool busy() const noexcept;

    /// Stops the turn in flight, if any: the stream is aborted, the half-built
    /// answer is kept as it stands and marked cancelled, and nothing is applied
    /// (ai.md R18, R20).
    void stop();

    /// Forgets the conversation — messages, attachments and accounting.
    void newChat();

    /// The transcript, for the probe.
    Transcript* transcript() const noexcept { return transcript_; }

    /// The conversation as the PROVIDER will see it, for `KENTOS_CHAT_PROBE`.
    ///
    /// NOT THE SAME AS THE TRANSCRIPT. The transcript is what a person reads and
    /// holds bubbles the model never sees; this is the message list that goes on
    /// the wire, which is where "did the job carry on after the approval" can
    /// actually be answered (TODOS A-04).
    std::span<const ai::Message> probeMessages() const;

    /// Drives RECORDED BYTES through the panel with no socket, as if they had
    /// arrived from `profile`'s endpoint.
    ///
    /// FOR `KENTOS_CHAT_PROBE` AND THE TESTS, and it is not a way round
    /// anything: the bytes go through the same decoder, the same assembler, the
    /// same tool loop and the same approval path a real answer does — which is
    /// what makes the probe worth running (`.claude/test.md`, and ai.md P10's
    /// rule that no test calls a live provider).
    void probeStream(const ai::ProviderProfile& profile, std::string_view bytes);

    void applyTheme(ThemeMode mode) override;

signals:
    /// A line the shell should echo on the command line: a refusal, a filed
    /// suggestion, a provider error.
    void said(const QString& line);

private:
    /// Opens the answer's bubble and the decoder for one turn in `dialect`,
    /// naming `model` on the message. Shared by `sendRound` and `probeStream`,
    /// so a recorded turn is assembled exactly as a live one is.
    void beginTurn(const ai::ProviderProfile& profile);

    /// Builds the request for the next round and sends it.
    void sendRound();

    /// One decoded batch, folded into the open bubble.
    void consume(const std::vector<ai::StreamEvent>& events);

    /// The turn ended: run the read tools, file the writes as one plan, and
    /// decide whether another round is owed.
    void finishTurn(int status, const QString& trouble);

    /// Runs the read tools of the finished turn and appends their results as
    /// tool messages. Returns how many ran.
    int runReadTools(const std::vector<ai::Block>& calls);

    /// Files the write tools of the finished turn as ONE plan and puts its card
    /// in the answer's bubble. Returns the plan id, or empty when there were
    /// none.
    QString fileWrites(const std::vector<ai::Block>& calls);

    /// WHAT THIS PANEL IS CALLED AS A CLIENT.
    ///
    /// The chat is an agent like any other: its handles are its own and its
    /// suggestions are filed under this name, so the label has to be the SAME
    /// string in both places or the plan it files would be composed from handles
    /// it cannot reach (TODOS M-07). It reaches the audit record and the
    /// suggestion card, and it never carries a key (ai.md P11).
    std::string requesterLabel() const;

    /// Carries the job on after the person has answered a suggestion card.
    ///
    /// THE LOOP USED TO END AT THE FIRST CARD. A request that needs a layer,
    /// then objects on it, then a sheet, then a PDF stopped after one
    /// suggestion — and the user, having clicked Uygula, watched nothing
    /// happen (TODOS A-04). Nothing is applied here: the person already
    /// decided, and what continues is the conversation.
    void resumeAfterDecision(const QString& planId, bool applied);

    /// The profile the chooser is on, or nothing when none is configured.
    const ai::ProviderProfile* chosen() const;

    /// Refreshes the context meter from the conversation and the profile.
    void refreshMeter();

    /// Enables and disables what a turn in flight allows.
    void refreshControls();

    /// Reads a file the user dropped or picked and adds its chip.
    void attach(const QString& path);

    /// The system prompt: `llms.txt`'s preamble, the catalogue's own rules, and
    /// what is on screen right now.
    std::string systemPrompt() const;

    Controller& controller_;
    AiService& service_;
    AiTransport& transport_;

    ComboBox* chooser_{nullptr};
    Transcript* transcript_{nullptr};
    QPlainTextEdit* composer_{nullptr};
    Button* send_{nullptr};
    Button* stopButton_{nullptr};
    Button* attachButton_{nullptr};
    Button* clear_{nullptr};
    QWidget* attachRow_{nullptr};
    ContextMeter* meter_{nullptr};
    Banner* trouble_{nullptr};

    std::unique_ptr<ai::Conversation> chat_;
    std::unique_ptr<ai::TurnAssembler> turn_;
    std::unique_ptr<ai::StreamDecoder> decoder_;
    std::shared_ptr<ai::Cancellation> flight_;

    /// The sink the transport writes into. Declared as a member so it outlives
    /// the request — the interface takes it by reference (`ai/transport.hpp`).
    struct Sink;
    std::unique_ptr<Sink> sink_;

    MessageBubble* open_{nullptr}; ///< the bubble the stream is being written into
    class QTimer* tick_{nullptr};  ///< the seconds counter under the dots
    std::int64_t started_ms_{0};

    std::vector<ai::Attachment> pending_;
    ProfileSource profiles_;
    ai::ProviderProfiles known_;

    /// How many read-tool rounds this question has already taken.
    ///
    /// BOUNDED, BECAUSE A LOOP IS A BILL. A model that reads, thinks, reads again
    /// is doing the right thing; one that reads the same layer sixty times is
    /// spending the user's money and holding the GUI thread. At the ceiling the
    /// panel says so in Turkish and stops asking.
    int round_{0};

    ThemeMode theme_{ThemeMode::Dark};
};

} // namespace kentos::app
