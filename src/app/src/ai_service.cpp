// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/ai_service.hpp"

#include "kentos_cad/ai/commands.hpp"
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
            co_return "Öneri " + plan->id + ": " + ai::plan_state_name(plan->state) + ", " +
                std::to_string(plan->steps.size()) + " adım.";
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
                                                       const command::Args& args)
{
    const command::CommandSpec* spec = bus_.registry().by_id(command_id);
    if (spec == nullptr)
        return core::err(core::ErrorCode::NotFound, "Bilinmeyen komut: '" + command_id + "'.");

    // THE DOOR CHECKS THE FLAG, not the caller. `Flags::NoEffect` is the narrow
    // claim — changes no document, no file, no setting — and `ReadOnly` is not
    // it: `core.undo`, `core.save` and `core.export` all carry `ReadOnly` and
    // none of them may run for an agent without a person deciding.
    if (!has_flag(spec->flags, command::Flags::NoEffect))
        return core::err(core::ErrorCode::Unsupported,
                         "'" + command_id +
                             "' bir şeyi değiştirir; doğrudan çalıştırılamaz, öneri olur.");

    auto ran = bus_.dispatch(command::Invocation{command_id, args, command::Origin::Ai});
    if (!ran) return ran.error();

    ai::ToolOutcome outcome;
    outcome.command_id = ran.value().command_id;
    outcome.lines      = ran.value().lines;
    outcome.report     = ran.value().report;
    outcome.mutated    = ran.value().mutated;
    outcome.minted     = mintFrom(outcome.report, command_id);
    return outcome;
}

std::vector<std::string> AiService::mintFrom(const core::Json& report, const std::string& tool)
{
    std::vector<std::string> minted;
    if (report.is_null()) return minted;

    const std::uint64_t at = revision();

    // OBJECT KEYS become an entity handle, so the next call can say "those" —
    // and cannot say a number it made up (ai.md R26).
    if (const core::Json* keys = report.find("nesneler"); keys != nullptr && keys->is_array()) {
        std::vector<std::int64_t> list;
        for (const core::Json& entry : keys->as_array())
            if (entry.is_int()) list.push_back(entry.as_int());
        if (!list.empty()) minted.push_back(handles_.mint_entities(std::move(list), tool, at).id);
    }

    // A RECTANGLE becomes a window handle. This is the maintainer's "corner
    // coordinates of the visible area", and the one handle whose numbers a
    // client may read: they are the screen's own corners, not the drawing's
    // contents (handles.cpp says why that exception is safe).
    if (const core::Json* box = report.find("kutu_mm"); box != nullptr && box->is_array()) {
        const core::JsonArray& corners = box->as_array();
        if (corners.size() == 4)
            minted.push_back(handles_
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

    plan.revision           = revision();
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

core::Result<ai::Plan> AiService::plan_state(const std::string& id) const
{
    const ai::Plan* plan = plans_.find(id);
    if (plan == nullptr)
        return core::err(core::ErrorCode::NotFound, "Böyle bir öneri yok: '" + id + "'.");
    return *plan;
}

void AiService::withdraw(const std::string& id)
{
    // CLOSING THE STREAM IS THE CANCELLATION SIGNAL in MCP 2026-07-28, and this
    // is what it means here: the client that asked has gone, so the card comes
    // off the person's screen rather than waiting for a decision nobody will
    // read. The audit record says it was withdrawn, not rejected.
    if (const ai::Plan* plan = plans_.find(id); plan == nullptr) return;
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

ai::HandleStore& AiService::handles()
{
    return handles_;
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
