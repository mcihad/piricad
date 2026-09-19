// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/ai_service.hpp"

#include "kentos_cad/ai/commands.hpp"
#include "kentos_cad/ai/policy.hpp"
#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/command/session.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace kentos::app {
namespace {

/// The month's audit file: one file per month keeps a long-lived installation's
/// log readable without a database, and a month is the unit a municipality
/// archives by.
QString audit_file_for(const QString& dir)
{
    return dir + QStringLiteral("/denetim/%1.jsonl")
                     .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM")));
}

} // namespace

AiService::AiService(command::Bus& bus, QObject* parent) : QObject(parent), bus_(bus)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    audit_path_       = audit_file_for(dir);
    QDir().mkpath(QFileInfo(audit_path_).absolutePath());

    // THE SINK IS DURABLE BY CONTRACT (audit.hpp): opened, written and FLUSHED
    // per line. A decision that is lost in a crash is a decision nobody can be
    // held to, and this is the record that answers "who approved this parcel".
    const QString path = audit_path_;
    audit_             = std::make_unique<ai::AuditLog>([path, this](const std::string& line) {
        QFile file(path);
        if (!file.open(QIODevice::Append | QIODevice::Text)) {
            if (trouble_.isEmpty())
                trouble_ = tr("Denetim kaydı yazılamıyor: %1. Yapay zeka önerileri "
                                                      "uygulanabilir ama kaydı tutulamaz.")
                               .arg(path);
            return;
        }
        file.write(line.data(), static_cast<qint64>(line.size()));
        file.flush();
    });

    gate_ = std::make_unique<ai::Gate>(plans_, *audit_,
                                       [this](const ai::Plan& plan) { return applyPlan(plan); });

    bus_.on_ai_request =
        [this](const command::Bus::AiRequest& request) -> command::Task<core::Result<std::string>> {
        using Verb = command::Bus::AiRequest::Verb;
        switch (request.verb) {
        case Verb::SuggestionList: {
            const std::vector<const ai::Plan*> open = plans_.pending();
            if (open.empty()) co_return std::string("Bekleyen öneri yok.");
            std::string out = std::to_string(open.size()) + " bekleyen öneri:";
            for (const ai::Plan* plan : open) {
                out += "\n  " + plan->id + " — " + std::to_string(plan->steps.size()) +
                       " adım, isteyen: " +
                       (plan->requester.empty() ? "(bilinmiyor)" : plan->requester);
                for (const ai::PlanStep& step : plan->steps)
                    out += "\n      " + step.line;
            }
            co_return out;
        }

        case Verb::SuggestionState: {
            const ai::Plan* plan = plans_.find(request.plan);
            if (plan == nullptr)
                co_return core::err(core::ErrorCode::NotFound,
                                    "Böyle bir öneri yok: '" + request.plan + "'.");
            std::string said = "Öneri " + plan->id + ": " + ai::plan_state_name(plan->state) +
                               ", " + std::to_string(plan->steps.size()) + " adım.";
            // HOW FAR IT HAS GOT, while it is running — the same two counts the
            // client's structured answer carries, so the command line and the
            // protocol say one thing (M-06).
            if (plan->state == ai::PlanState::Running)
                said += " " + std::to_string(plan->done_steps) +
                        " adım bitti; henüz "
                        "tamamlanmadı.";
            if (!plan->outputs.empty()) {
                said += " Yazılan dosyalar:";
                for (const std::string& one : plan->outputs)
                    said += "\n    " + one;
            }
            co_return said;
        }

        // THE TWO DECIDING VERBS CARRY OUT A DECISION THAT HAS ALREADY BEEN
        // TAKEN. They cannot be the decision: an `ai::Approval` is made by the
        // suggestion card and by nothing else (ai.md P15), so a command line
        // that reached here without one is a client trying to skip the person.
        case Verb::SuggestionApply:
        case Verb::SuggestionReject:
            co_return core::err(core::ErrorCode::Unsupported,
                                "Bir öneri ancak öneri kartındaki düğmeyle uygulanır ya da "
                                "reddedilir; komut satırı kararı veremez. Kartı görmek için "
                                "Pencere ▸ Yapay Zeka'yı açın.");

        case Verb::ServerStart:
        case Verb::ServerStop:
        case Verb::ServerState:
        case Verb::ServerToken:
        case Verb::ServerClients:
        case Verb::ServerRevoke:
        case Verb::ServerRestore:
        case Verb::ServerProbe:
            // DELEGATED TO THE LISTENER, which installs itself through
            // `setServerHandler`. Unset means this build genuinely has no
            // listener — the option is off, or nothing wired it — and saying so
            // is better than a command that appears to work and opens no port.
            if (!server_)
                co_return core::err(core::ErrorCode::Unsupported,
                                    "Bu yapıda MCP sunucusu yok (KENTOS_WITH_MCP kapalı).");
            co_return server_(request);
        }
        co_return core::err(core::ErrorCode::Internal, "İşlenmemiş yapay zeka isteği.");
    };
}

AiService::~AiService()
{
    bus_.on_ai_request = nullptr;
}

void AiService::setServerHandler(ServerHandler handler)
{
    server_ = std::move(handler);
}

void AiService::recordCoordinateRefusal(const ai::AuditNote& note)
{
    audit_->write_coordinate_refusal(note.requester, note.tool, note.detail,
                                     QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

void AiService::announce()
{
    if (trouble_.isEmpty()) return;
    bus_.echo(trouble_.toStdString());
    trouble_.clear();
}

core::Status AiService::settle(const ai::Approval& approval)
{
    const core::Status settled = gate_->decide(approval);
    emit suggestionSettled(QString::fromStdString(approval.plan_id()));
    return settled;
}

core::Status AiService::applyPlan(const ai::Plan& plan)
{
    if (plan.steps.empty())
        return core::err(core::ErrorCode::InvalidArgument, "Öneri boş; uygulanacak adım yok.");

    // THE DOCUMENT IS CHECKED AGAINST THE ONE THE PLAN WAS COMPOSED FOR.
    //
    // A plan carries the revision it was built against and nothing compared it.
    // Between composing and applying, the drawing can move: the person typed a
    // command, another client edited it, an undo ran. The handles inside the plan
    // resolve against slots that have since been reused, and a suggestion about
    // parcel 21 applies to whatever is in that slot now — which is the one way an
    // approval can become an edit nobody approved (TODOS C-04).
    //
    // A STALE PLAN IS REFUSED, NOT RE-AIMED. The old intent against new ground is
    // a guess; the honest answer is a fresh plan against what is actually there.
    if (plan.revision != 0 && plan.revision != bus_.document().revision())
        return core::err(core::ErrorCode::Conflict,
                         "Bu öneri hazırlandığından beri çizim değişti (sürüm " +
                             std::to_string(plan.revision) + " → " +
                             std::to_string(bus_.document().revision()) +
                             "). Öneri uygulanmadı; yeniden hazırlanması gerekiyor.");

    // ONE BATCH, ONE UNDO ENTRY, ALL OR NOTHING (ai.md R4, R5; Article 1.6).
    // The steps are dispatched with their resolved arguments rather than by
    // re-parsing their lines: the line is what a person reads, the arguments are
    // what was validated when the plan was compiled, and re-parsing would be a
    // second chance for them to differ.
    if (auto opened = bus_.begin_batch("Yapay zeka önerisi"); !opened) return opened.error();

    // WHAT THE STEPS PRODUCED, collected as they run. A client that has to read
    // "uygulandı" and then guess whether a file appeared will guess wrong.
    std::vector<std::string> wrote;
    std::vector<std::string> notes;

    for (const ai::PlanStep& step : plan.steps) {
        auto ran =
            bus_.dispatch(command::Invocation{step.command_id, step.args, command::Origin::Ai});
        if (!ran) {
            bus_.abort_batch();
            return ran.error();
        }
        for (const std::string& one : ran.value().outputs)
            wrote.push_back(one);
        for (const std::string& one : ran.value().warnings)
            notes.push_back(one);
    }

    auto done = bus_.end_batch();
    if (!done) return done.error();

    // `plan` IS CONST HERE because applying must not be able to rewrite what was
    // approved; the outcome goes back through the store, which is the only thing
    // allowed to move a plan (`PlanStore::settle`).
    if (ai::Plan* filed = plans_.find(plan.id); filed != nullptr) {
        filed->applied_revision = bus_.document().revision();
        filed->outputs          = std::move(wrote);
        filed->warnings         = std::move(notes);
        filed->undo_label       = "Yapay zeka önerisi";
    }
    return core::ok();
}

core::Result<ai::ToolOutcome> AiService::run_read_only(const std::string& command_id,
                                                       const command::Args& args,
                                                       const std::string& requester)
{
    const command::CommandSpec* spec = bus_.registry().by_id(command_id);
    if (spec == nullptr)
        return core::err(core::ErrorCode::NotFound, "Bilinmeyen komut: '" + command_id + "'.");

    // THE DOOR CHECKS WHAT THIS CALL WOULD DO, not what its command can do.
    //
    // `Flags::NoEffect` is the narrow claim — changes no document, no file, no
    // setting — and `ReadOnly` is not it: `core.undo`, `core.save` and
    // `core.export` all carry `ReadOnly` and none may run for an agent without a
    // person deciding.
    //
    // BUT A FLAG IS PER COMMAND AND SOME COMMANDS ARE BOTH. `ÇIKTIYERLEŞİMİ` adds
    // pages and deletes them — and it also answers `islem=denetle`, which reads a
    // sheet and reports what will print wrong. The flag cannot say "this verb
    // reads", so the whole command was refused and the MCP preflight resource,
    // which runs exactly that verb, silently answered with an empty list for
    // every sheet: a preflight that reported nothing and looked like a clean
    // one (TODOS A-05, L-15).
    //
    // `command::effect_of(spec, args)` is the answer built for this (C-02): it
    // reads the verb out of THESE arguments and falls back to the command's
    // whole effect when the verb is not one it declared — so a command with an
    // incomplete verb table is refused rather than let through.
    //
    // MOVING THE VIEW COUNTS AS NO EFFECT, and that is CLAUDE.md 2.10's own
    // wording: an agent "may read anything and move the view".
    constexpr command::Effect harmless = command::Effect::Query | command::Effect::ViewChange;
    const command::Effect would        = command::effect_of(*spec, args);
    const auto outside = static_cast<std::uint32_t>(would) & ~static_cast<std::uint32_t>(harmless);
    if (!has_flag(spec->flags, command::Flags::NoEffect) && outside != 0u)
        return core::err(core::ErrorCode::Unsupported,
                         "'" + command_id +
                             "' bir şeyi değiştirir; doğrudan çalıştırılamaz, öneri olur.");

    // AND IT CHECKS THE WIDENING SEPARATELY, because the two refusals answer
    // different questions. `NoEffect` asks "does this change anything"; this
    // asks "would this change who may do what" — and a call can be both
    // harmless-looking and a widening, which is exactly how an agent removes an
    // obstacle it met a moment ago (TODOS S-04).
    if (std::string why = ai::escalation_refusal(*spec, args); !why.empty())
        return core::err(core::ErrorCode::Unsupported, why);

    auto ran = bus_.dispatch(command::Invocation{command_id, args, command::Origin::Ai});
    if (!ran) return ran.error();

    ai::ToolOutcome outcome;
    outcome.command_id = ran.value().command_id;
    outcome.lines      = ran.value().lines;
    outcome.report     = ran.value().report;
    outcome.mutated    = ran.value().mutated;
    outcome.minted     = mintFrom(outcome.report, command_id, requester);
    return outcome;
}

std::vector<std::string> AiService::mintFrom(const core::Json& report, const std::string& tool,
                                             const std::string& requester)
{
    std::vector<std::string> minted;
    if (report.is_null()) return minted;

    const std::uint64_t at   = revision();
    ai::HandleStore& handles = this->handles(requester);

    // OBJECT KEYS become an entity handle, so the next call can say "those" —
    // and cannot say a number it made up (ai.md R26).
    if (const core::Json* keys = report.find("nesneler"); keys != nullptr && keys->is_array()) {
        std::vector<std::int64_t> list;
        for (const core::Json& entry : keys->as_array())
            if (entry.is_int()) list.push_back(entry.as_int());
        if (!list.empty()) minted.push_back(handles.mint_entities(std::move(list), tool, at).id);
    }

    // A RECTANGLE becomes a window handle. This is the maintainer's "corner
    // coordinates of the visible area", and the one handle whose numbers a
    // client may read: they are the screen's own corners, not the drawing's
    // contents (handles.cpp says why that exception is safe).
    if (const core::Json* box = report.find("kutu_mm"); box != nullptr && box->is_array()) {
        const core::JsonArray& corners = box->as_array();
        if (corners.size() == 4)
            minted.push_back(handles
                                 .mint_window(core::Box2{corners[0].as_int(), corners[1].as_int(),
                                                         corners[2].as_int(), corners[3].as_int()},
                                              tool, at)
                                 .id);
    }
    return minted;
}

core::Result<std::string> AiService::propose(ai::Plan plan)
{
    if (plan.steps.empty())
        return core::err(core::ErrorCode::InvalidArgument,
                         "Boş öneri kaydedilmez; en az bir adım gerekir.");

    plan.revision = revision();

    // THE SAME GUARD ON THE ROAD A WRITE TAKES. A widening cannot be smuggled in
    // as a step of a suggestion either: a person approving a plan is approving
    // the DRAWING work they read on the card, and "and also turn the approval
    // policy off" is not something a card can meaningfully ask (S-04).
    for (const ai::PlanStep& step : plan.steps) {
        const command::CommandSpec* spec = bus_.registry().by_id(step.command_id);
        if (spec == nullptr) continue;
        if (std::string why = ai::escalation_refusal(*spec, step.args); !why.empty())
            return core::err(core::ErrorCode::Unsupported, why);
    }

    // ---- HAVE I ALREADY BEEN ASKED THIS? (TODOS M-06) ----------------------
    //
    // An agent that loses its connection mid-call cannot tell whether the call
    // arrived, and retrying is the only thing it can do. Without a key the retry
    // files a SECOND suggestion, so the person at the workstation gets two
    // identical cards for one piece of work and has to work out which to apply.
    // With one, the retry is handed the id of the card already on the screen.
    //
    // SCOPED TO THE REQUESTER (`PlanStore::find_by_key`): two clients may use
    // the same word for two different jobs, and one must not reach another's
    // plan by guessing a key (M-07).
    if (!plan.idempotency_key.empty())
        if (const ai::Plan* already = plans_.find_by_key(plan.idempotency_key, plan.requester);
            already != nullptr)
            return already->id;

    // ---- APPENDING TO A SUGGESTION THAT IS ALREADY ON SOMEBODY'S SCREEN ------
    //
    // A non-empty `Plan::id` names a plan to EXTEND rather than a second one to
    // file, which is how an agent composes a sequence that one approval applies
    // as one transaction and one Ctrl+Z (ai.md R4). The id comes back unchanged
    // so the caller can tell an append from a new filing; `McpServer` compares
    // the two and tells the client plainly when the extension did not happen.
    //
    // AND IT IS OWNERSHIP-CHECKED, which is the whole reason this is not a bare
    // `append`. The approval a person gives is for the command lines they READ
    // on the card. A second client that could push a step into a pending plan
    // would be having its work signed by somebody who never saw it, and the
    // audit record would name the wrong requester for that step (TODOS M-07).
    if (!plan.id.empty()) {
        const std::string target = plan.id;
        for (ai::PlanStep& step : plan.steps)
            if (core::Status added = plans_.append_for(target, plan.requester, std::move(step));
                !added)
                return added.error();

        emit suggestionFiled(QString::fromStdString(target));
        return target;
    }

    const std::string filed = plans_.add(std::move(plan));
    emit suggestionFiled(QString::fromStdString(filed));

    // SAID ON THE TRANSCRIPT TOO, because a suggestion that arrived while the
    // user was looking at the drawing must not be a silent modal surprise: the
    // line is the same one the card shows, and it stays in the transcript after
    // the card is answered.
    const ai::Plan* held = plans_.find(filed);
    if (held != nullptr) {
        std::string said = "Yapay zeka önerisi " + filed +
                           " (öneri — uygulanmadı): " + std::to_string(held->steps.size()) +
                           " adım";
        for (const ai::PlanStep& step : held->steps)
            said += "\n    " + step.line;
        bus_.echo(said);
    }
    return filed;
}

std::string AiService::existing_plan(const std::string& key, const std::string& requester) const
{
    const ai::Plan* held = plans_.find_by_key(key, requester);
    return held != nullptr ? held->id : std::string();
}

core::Result<ai::Plan> AiService::plan_state(const std::string& id,
                                             const std::string& requester) const
{
    // ONE ANSWER FOR "NOT YOURS" AND FOR "NOT THERE"; `find_for` says why.
    const ai::Plan* plan = plans_.find_for(id, requester);
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound, "Böyle bir öneri yok: '" + id + "'.");
    return *plan;
}

void AiService::withdraw(const std::string& id, const std::string& requester)
{
    // CLOSING THE STREAM IS THE CANCELLATION SIGNAL in MCP 2026-07-28, and this
    // is what it means here: the client that asked has gone, so the card comes
    // off the person's screen rather than waiting for a decision nobody will
    // read. The audit record says it was withdrawn, not rejected.
    if (plans_.find_for(id, requester) == nullptr) return;
    (void)plans_.settle(id, ai::PlanState::Withdrawn, "İstemci bağlantıyı kapattı.");
    emit suggestionSettled(QString::fromStdString(id));
}

std::uint64_t AiService::revision() const
{
    return bus_.document().revision();
}

std::optional<command::ViewInfo> AiService::view() const
{
    if (!bus_.on_view_query) return std::nullopt;
    return bus_.on_view_query();
}

ai::HandleStore& AiService::handles(const std::string& requester)
{
    return handles_.for_client(requester);
}

void AiService::refreshCatalog() const
{
    const std::uint64_t now = bus_.registry().fingerprint();
    if (now == catalog_fingerprint_ && !catalog_.tools.empty()) return;
    catalog_             = ai::build_catalog(bus_.registry());
    catalog_fingerprint_ = now;
}

const ai::Catalog& AiService::catalog() const
{
    refreshCatalog();
    return catalog_;
}

} // namespace kentos::app
