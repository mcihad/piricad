// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the machinery behind `ÖNERİ` and the agent surface.
//
// `/src/ai` is sans-IO on purpose: it knows how to project the command
// catalogue, how to compile a tool call into a plan, how to resolve a handle and
// what an audit record must contain, and it can do none of it without somebody
// to run a command, read a viewport or write a line to a file. This service is
// that somebody — the `PrintService` of the AI layer, in the same shape and for
// the same reason (`Bus::on_print_request` there, `Bus::on_ai_request` here).
//
// IT IS ALSO THE THREAD ANSWER. `Bus`, `Session`, `Document`, `Registry` and
// `UndoStack` take no locks, and `Document::spatial_index()` rebuilds a cache
// through `mutable` members even though it is `const`. Every `ai::Dispatcher`
// call therefore happens on the GUI thread: the MCP listener is a `QHttpServer`
// on this thread's own event loop, so a request is handled between events rather
// than beside them. The cost is stated plainly: a slow tool would hold the
// window, so anything slow must become a `command::Job` like every other long
// read in this program.
//
// AND IT CANNOT APPLY ANYTHING BY ITSELF. `ai::Gate` demands an `ai::Approval`,
// and the only factory for one is called from the suggestion card the person
// clicked (ai.md P15, CLAUDE.md 5.7). This service files plans, tells clients
// their state, and applies one when — and only when — that click has happened.
#pragma once

#include "kentos_cad/ai/audit.hpp"
#include "kentos_cad/ai/catalog.hpp"
#include "kentos_cad/ai/dispatcher.hpp"
#include "kentos_cad/ai/endpoint.hpp"
#include "kentos_cad/ai/gate.hpp"
#include "kentos_cad/ai/handles.hpp"
#include "kentos_cad/ai/plan.hpp"
#include "kentos_cad/ai/policy_path.hpp"

#include "kentos_cad/command/bus.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace kentos::app {

/// Owns the plan store, the audit log, the handles and the catalogue, and is the
/// one door from the AI layer to the running document.
class AiService : public QObject, public ai::Dispatcher
{
    Q_OBJECT

public:
    /// Installs `Bus::on_ai_request` and clears it on destruction. The audit log
    /// is opened under the user's configuration directory; a directory that
    /// cannot be written is reported by `announce()` rather than swallowed.
    AiService(command::Bus& bus, QObject* parent = nullptr);
    ~AiService() override;

    AiService(const AiService&)            = delete;
    AiService& operator=(const AiService&) = delete;

    /// Says anything the constructor could not — the audit log's directory, a
    /// profile file that would not parse. Called once the bus can be heard, for
    /// the reason `PrintService::announce` exists.
    void announce();

    /// Where the audit log is being written, for the settings page's note and
    /// for the tests.
    const QString& auditPath() const noexcept { return audit_path_; }

    /// The plans as they stand; read by the suggestion card and the tests.
    const ai::PlanStore& plans() const noexcept { return plans_; }

    /// Installed by `McpService` so that `MCPSUNUCU` reaches the listener.
    ///
    /// ONE HOOK, ONE OWNER. `Bus::on_ai_request` is a single `std::function` and
    /// two services cannot both install it; this one owns it and delegates the
    /// four server verbs. Unset means this build has no listener, and the
    /// command says exactly that rather than pretending.
    using ServerHandler = std::function<core::Result<std::string>(const command::Bus::AiRequest&)>;
    void setServerHandler(ServerHandler handler);

    /// Writes the audit record for a coordinate literal the protocol layer
    /// refused (.claude/ai.md R10: the rejection must be logged, and it never
    /// becomes a plan, so this is its only trace).
    void recordCoordinateRefusal(const ai::AuditNote& note);

    /// Writes the audit record for a call the protocol layer refused as a
    /// widening of the caller's own authority (CLAUDE.md 5.23, TODOS S-04).
    void recordEscalationRefusal(const ai::AuditNote& note);

    /// The application-scope settings, for the card that has to name the
    /// responsible engineer (`core.ai.sorumlu`).
    const core::Settings& appSettings() const noexcept { return bus_.app_settings(); }

    /// The gate, for the ONE caller that may mint an approval.
    ///
    /// EXPOSED RATHER THAN WRAPPED, DELIBERATELY. An earlier shape had this
    /// service take a plan id and a `Decision` and call `Gate::approve` itself —
    /// which moved the approval factory's one call site out of the suggestion
    /// card and into a service every panel can reach, and `scripts/ci-gate-ai.sh`
    /// said so. The card mints the `ai::Approval`, because the card is where the
    /// person clicked, and hands it back to `settle` (ai.md P15).
    ai::Gate& gate() noexcept { return *gate_; }

    /// Carries out a decision that has already been made.
    ///
    /// Takes the `ai::Approval` rather than its parts: the value cannot be
    /// forged — no default constructor, no aggregate initialisation, one private
    /// factory — so a caller holding one has been through the card. Writes the
    /// audit record for a rejection as well as an application (ai.md R6).
    core::Status settle(const ai::Approval& approval);

    // ---- ai::Dispatcher ----
    core::Result<ai::ToolOutcome> run_read_only(const std::string& command_id,
                                                const command::Args& args,
                                                const std::string& requester) override;
    ai::PolicyPreferences preferences() const override;
    core::Result<std::string> propose(ai::Plan plan) override;
    std::string existing_plan(const std::string& key, const std::string& requester) const override;
    core::Result<ai::Plan> plan_state(const std::string& id,
                                      const std::string& requester) const override;
    void withdraw(const std::string& id, const std::string& requester) override;
    std::uint64_t revision() const override;
    std::optional<command::ViewInfo> view() const override;
    ai::HandleStore& handles(const std::string& requester) override;
    const ai::Catalog& catalog() const override;

    /// The registry the catalogue is projected from, for the chat's step
    /// compiler: a tool call is checked against the command it names.
    const command::Registry& registry() const { return bus_.registry(); }

    /// WHERE A STEP WOULD WRITE, for the overwrite policy (`core.ai.uzerine_yazma`).
    ///
    /// A step that writes a named file answers with the file's path and the
    /// argument that names it; `by_name` when that argument is a NAME the file
    /// is made from (a layout template) rather than the path itself. The shell
    /// installs this, because only the shell knows where a template lives.
    struct WriteTarget
    {
        QString path;        ///< the file the step would write
        std::string param;   ///< the argument that decides it
        bool by_name{false}; ///< `param` is a name the path is made from
    };

    /// Answers where a step would write, or nothing for a step that writes no
    /// named file.
    using WriteTargets = std::function<std::optional<WriteTarget>(const ai::PlanStep&)>;

    /// Installs the answer; the shell's controller does, once.
    void setWriteTargets(WriteTargets targets) { write_targets_ = std::move(targets); }

signals:
    /// A client has proposed something and a person has to look at it. The shell
    /// raises the suggestion card; nothing is applied until it is answered.
    void suggestionFiled(const QString& planId);

    /// A plan has been applied, rejected, withdrawn or refused — the card and
    /// the status strip both follow this.
    void suggestionSettled(const QString& planId);

private:
    /// Runs an approved plan: one batch, one undo entry, all or nothing.
    core::Status applyPlan(const ai::Plan& plan);

    /// Offers the plan to the user's standing approval policy, and says what it
    /// decided and why.
    ///
    /// NOT APPLIED IS THE ORDINARY ANSWER and not a failure: under the default
    /// `her_degisiklikte` every plan waits for a person, so a user who never
    /// touched the setting sees exactly the behaviour they always saw. See
    /// `ai::decide_by_policy` for why the second road is safe (CLAUDE.md 5.23).
    /// Before deciding it settles the overwrite question: with `yeni_ad_uret` a
    /// step that would write over a file is given a fresh name, and said so.
    core::Result<ai::PolicyOutcome> applyByPolicy(ai::Plan& plan);

    /// Whether a step would write over a file that is there, after giving it a
    /// fresh name when the overwrite policy asks for one.
    bool resolveOverwrites(ai::Plan& plan, ai::OverwritePolicy overwrite);

    /// Mints handles from a read command's structured report, so the client can
    /// point at what it just learned (dispatcher.hpp explains why this is the
    /// dispatcher's job and not the command's). They land in `requester`'s own
    /// store.
    std::vector<std::string> mintFrom(const core::Json& report, const std::string& tool,
                                      const std::string& requester);

    /// Rebuilds the catalogue when the registry's fingerprint has moved.
    void refreshCatalog() const;

    command::Bus& bus_;

    ai::PlanStore plans_;

    /// One handle store per client; see `ai::HandleScopes` for why that is not
    /// one store with a label on each value.
    ai::HandleScopes handles_;
    std::unique_ptr<ai::AuditLog> audit_;
    std::unique_ptr<ai::Gate> gate_;

    /// Cached, because projecting 89 commands into JSON Schema on every
    /// `tools/list` would be work nobody asked for; the fingerprint says when it
    /// is stale (`mutable` because `catalog()` is a const accessor that may have
    /// to rebuild).
    WriteTargets write_targets_;

    mutable ai::Catalog catalog_{};
    mutable std::uint64_t catalog_fingerprint_{0};

    ServerHandler server_;
    QString audit_path_;
    QString trouble_;
};

} // namespace kentos::app
