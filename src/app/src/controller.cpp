// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/controller.hpp"

#include "kentos_cad/command/job.hpp"

#include "kentos_cad/command/log.hpp"
#include "kentos_cad/domain/cadastre/commands.hpp"
#include "kentos_cad/domain/geodesy/commands.hpp"
#include "kentos_cad/domain/surface/commands.hpp"

#include "kentos_cad/command/parser.hpp"

#include "kentos_cad/core/identity.hpp"

#include <QDir>
#include <QStandardPaths>

#include <filesystem>

#include <QThread>

namespace kentos::app {
namespace {

/// The thread a hosted job runs on. It reads the job through the session and
/// touches nothing else of it; the session is resumed on the GUI thread when
/// `finished` arrives there (a queued connection, because the signal is emitted
/// from this thread).
class JobRunner final : public QThread
{
public:
    JobRunner(command::Session& session, QObject* parent) : QThread(parent), session_(session) {}

protected:
    void run() override
    {
        if (command::Job* job = session_.job(); job != nullptr)
            job->work(command::JobControl{job->stop.get_token()});
    }

private:
    command::Session& session_;
};

} // namespace

Controller::Controller(QObject* parent)
    : QObject(parent), bus_(document_, registry_, journal_, undo_), files_(bus_), database_(bus_),
      runner_(bus_, script::Sandbox::Project)
#if KENTOS_HAVE_LUA
      ,
      lua_runner_(bus_, script::Sandbox::Project)
#endif
{
    command::register_builtin_commands(registry_);

    // The domain modules own commands too, and `/src/command` may not name them
    // (Article 3.2). `/src/app` depends on everything, so this is the one place
    // both lists can be put on one registry.
    domain::geodesy::register_geodesy_commands(registry_);
    domain::cadastre::register_cadastre_commands(registry_);
    domain::surface::register_surface_commands(registry_);

    // The CRS resolver, so a drawing knows that TUREF/TM30 is EPSG:5254 without
    // the user restating it. A missing or unreadable /data/crs package leaves the
    // hook uninstalled: an unresolved CRS keeps its id and says so, which is the
    // truthful state, and guessing a zone would move every coordinate by
    // kilometres while still looking like Turkish coordinates.
    if (auto catalogue = domain::geodesy::CrsCatalog::load("data/crs"); catalogue) {
        crs_.emplace(bus_, std::move(catalogue.value()));

        // Resolve the CRS the document was CONSTRUCTED with. The document exists
        // before the resolver does, so without this a fresh drawing would carry an
        // unresolved default forever and `DIŞAAKTAR` would refuse it — which is
        // exactly the bug this change is here to fix, reintroduced one step later.
        core::Op discard;
        if (auto st = document_.set_crs(crs_->resolve(document_.crs().id()), discard); !st)
            command::log_warn("başlangıç koordinat sistemi çözülemedi: " + st.error().message);
    }
#if KENTOS_HAVE_LUA
    // `proje` for both hosts, and the project directory is the working directory
    // until a document has a path of its own. A jail with no walls denies
    // everything (`.claude/script.md` P8), which is the safe direction to be wrong
    // in while the document-path wiring lands.
    lua_runner_.set_project_root(std::filesystem::current_path().string());
    script::install(bus_, runner_, lua_runner_);
#else
    script::install(bus_, runner_);
#endif
    wireBus();

    // The journal is written asynchronously on its own thread; the UI never waits
    // on a disk flush (kentoscad.md §10.4).
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/oturum.jsonl";
    if (auto st = journal_.open_sink(path.toStdString()); !st)
        emit echoed(
            tr("Günlük dosyası açılamadı: %1").arg(QString::fromStdString(st.error().message)));
    else
        emit echoed(tr("Komut günlüğü: %1").arg(path));
}

Controller::~Controller()
{
    // A worker still reading would write into a frame the session is about to
    // free: stop it and wait, then let the session go.
    if (jobThread_ != nullptr) {
        if (session_) session_->cancel();
        jobThread_->wait();
        jobThread_ = nullptr;
    }
    session_.reset();
    journal_.close_sink();
}

void Controller::wireBus()
{
    bus_.on_echo = [this](std::string_view text) {
        emit echoed(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
    };
    bus_.on_document_changed = [this] {
        // An erase retires slots and an undo brings them back, so the resolved
        // list is rebuilt with the document rather than only with the selection.
        refreshSelection();
        emit documentChanged();
    };
    bus_.on_job_host = [this](command::Session& session) { hostJob(session); };
    bus_.on_prompt   = [this](const command::Prompt& p) {
        emit promptChanged(QString::fromStdString(p.message));
    };
    bus_.on_selection_changed = [this] {
        refreshSelection();
        emit selectionChanged();
    };
    bus_.on_setting_changed = [this](std::string_view id, core::SettingScope) {
        emit settingChanged(QString::fromUtf8(id.data(), static_cast<int>(id.size())));
    };
    bus_.on_view_request = [this](std::string_view mode, double factor) {
        emit viewRequested(QString::fromUtf8(mode.data(), static_cast<int>(mode.size())), factor);
    };
    bus_.on_pan_request = [this](core::Point2 from, core::Point2 to) {
        emit panRequested(from, to);
    };
}

void Controller::settle()
{
    emit undoStateChanged(undo_.can_undo(), undo_.can_redo());
}

void Controller::runLine(const QString& line, command::Origin origin)
{
    (void)runLineResult(line, origin);
}

core::Result<command::DispatchResult> Controller::runLineResult(const QString& line,
                                                                command::Origin origin)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return core::err(core::ErrorCode::InvalidArgument, "Komut satırı boş olamaz.");

    // A running interactive command gets the typed value first, unless the typed
    // text names a transparent command such as ZOOM (kentoscad.md §3).
    //
    // EVERY KIND OF ANSWER, not only a coordinate. This used to intercept a typed
    // point and nothing else, so a command waiting for a NUMBER could not be
    // answered at all from the command line: OFSET armed, printed "Ofset mesafesi
    // (metre)", and a user who typed `5` got "Bilinmeyen komut: '5'" because the
    // line fell through to `execute_line`. The distance had no other road in —
    // there is no on-canvas box for a number the way the text command has one —
    // so the tool could be started and never finished.
    //
    // The prompt already says what would satisfy it (`Prompt::kind`), so the
    // answer is converted to that and handed over. Nothing about which client is
    // asking enters into it (Article 1.2).
    if (session_ && session_->waiting()) {
        // THE FIRST WORD DECIDES. A word the registry knows is a COMMAND: a
        // transparent one (`YAKINLAŞ KAPSAM`) runs beside the waiting command; any
        // other finishes the waiting one first — the way Enter finishes it — and
        // then starts on a clean bus. Running the typed command THROUGH the parked
        // one is what this used to do: `ÇİZGİ 60,0 60,40` typed while ALAN waited
        // for a corner handed `60,0` to ALAN as its next corner and drew nothing.
        // A word the registry does not know is an ANSWER: the whole line is the
        // value the prompt asked for — a coordinate, a number, a caption.
        auto parsed = command::parse_line(trimmed.toStdString());
        const command::CommandSpec* spec =
            parsed ? registry_.resolve(parsed.value().command) : nullptr;
        const bool transparent = spec && has_flag(spec->flags, command::Flags::Transparent);
        if (spec != nullptr && !transparent) {
            // Picking objects for the waiting command IS its answer, so a
            // selection command runs beside it (`dispatchSelection`).
            if (spec->id != "core.select") cancelInteractive();
        } else if (spec == nullptr) {
            const command::Prompt& asking = session_->prompt();

            // The line tokenised as VALUES: the parser takes the first word as a
            // command, so a stand-in word goes in front and every token that
            // follows is the answer.
            auto answer = command::parse_line("YANIT " + trimmed.toStdString());
            if (answer && !answer.value().tokens.empty() &&
                command::is_coordinate(answer.value().tokens.front())) {
                auto pt =
                    command::resolve_point(answer.value().tokens.front(), asking.rubber_origin);
                if (pt) {
                    supplyPoint(pt.value());
                    return command::DispatchResult{};
                }
            }

            switch (asking.kind) {
            case command::ParamKind::Number:
            case command::ParamKind::Integer: {
                // The Turkish decimal comma is what a Turkish keyboard produces
                // and what every other number in this program is written with.
                bool ok          = false;
                QString number   = trimmed;
                const double val = number.replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
                if (ok) {
                    supplyNumber(val);
                    return command::DispatchResult{};
                }
                break;
            }
            case command::ParamKind::Text:
            case command::ParamKind::Bool:
                // A keyword or a caption. Refused as a command name first — above —
                // so `ZOOM` still zooms rather than becoming somebody's label.
                supplyText(trimmed);
                return command::DispatchResult{};
            case command::ParamKind::Point:
            case command::ParamKind::PointList:
            case command::ParamKind::Selection: break;
            }
        }
    }

    // A TYPED INTERACTIVE COMMAND STARTS THE WAY A BUTTON STARTS IT. `execute_line`
    // drives a command to completion in one call, which is right for a script and
    // a test and wrong for a live user: `İÇEAKTAR` typed with a 48 MB DXF froze
    // the window for the read, and `ÇİZGİ` typed alone could not prompt for its
    // points. Through `begin_interactive` the same body prompts, hands its read to
    // a worker thread, and is recorded with the origin it came from — nothing
    // else about it changes (Article 1.2). A batch keeps the one-shot road, because
    // a batch is a script.
    //
    // A LINE THAT RUNS STRAIGHT THROUGH STILL ANSWERS SYNCHRONOUSLY: when the
    // session finishes inside `begin_interactive` — every argument was on the
    // line and nothing was handed to a worker — it is finished here and its result
    // returned, exactly as `execute_line` returned it. Only a session that parks,
    // on a prompt or on a job, is kept.
    if (!bus_.in_batch() && !session_) {
        if (auto parsed = command::parse_line(trimmed.toStdString()); parsed) {
            const command::CommandSpec* spec = registry_.resolve(parsed.value().command);
            if (spec != nullptr && has_flag(spec->flags, command::Flags::Interactive) &&
                !spec->params.empty()) {
                auto started = bus_.begin_interactive(trimmed.toStdString(), origin);
                if (!started) {
                    emit echoed(
                        tr("Hata: %1").arg(QString::fromStdString(started.error().message)));
                    settle();
                    return started.error();
                }
                session_ = std::move(started.value());
                if (!session_->finished()) {
                    settleSession();
                    return command::DispatchResult{};
                }
                auto done = bus_.finish(*session_);
                session_.reset();
                if (!done) {
                    emit echoed(tr("Hata: %1").arg(QString::fromStdString(done.error().message)));
                } else if (!done.value().message.empty()) {
                    emit echoed(QString::fromStdString(done.value().message));
                }
                settle();
                return done;
            }
        }
    }

    auto result = bus_.execute_line(trimmed.toStdString(), origin);
    if (!result) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(result.error().message)));
    } else if (!result.value().message.empty()) {
        emit echoed(QString::fromStdString(result.value().message));
    }
    settle();
    return result;
}

void Controller::runInvocation(const command::Invocation& invocation)
{
    auto result = bus_.dispatch(invocation);
    if (!result) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(result.error().message)));
    } else if (!result.value().message.empty()) {
        emit echoed(QString::fromStdString(result.value().message));
    }
    settle();
}

void Controller::refreshSelection()
{
    // Key -> slot is a binary search over a column the document already keeps, and
    // it runs once per selection change rather than once per frame (model.md R2).
    const command::Selection& selection = bus_.selection();

    selected_slots_.clear();
    selected_slots_.reserve(selection.size());
    for (core::EntityKey key : selection.keys()) {
        const core::EntityId slot = document_.slot_of(key);
        if (slot != core::kNoEntity && document_.alive(slot)) selected_slots_.push_back(slot);
    }
    selection_revision_ = selection.revision();
}

void Controller::runCommand(const QString& line)
{
    // A BUTTON MAY CARRY A WHOLE LINE, not just a name — `SEÇ mod=KUTU` is one
    // tool and `SEÇ` is another. Resolving the whole string as a name, which is
    // what this did, made any such button a no-op: the registry lookup is exact,
    // so "SEÇ mod=KUTU" was simply an unknown command. The command word is parsed
    // out with the one parser (CLAUDE.md 5.11) and the rest travels with it.
    auto parsed = command::parse_line(line.toStdString());
    if (!parsed) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(parsed.error().message)));
        return;
    }

    const command::CommandSpec* spec = registry_.resolve(parsed.value().command);
    if (!spec) {
        emit echoed(tr("Bilinmeyen komut: %1").arg(QString::fromStdString(parsed.value().command)));
        return;
    }

    // Interactive commands started from a button behave exactly as if typed.
    if (has_flag(spec->flags, command::Flags::Interactive) && !spec->params.empty()) {
        beginInteractive(line);
        return;
    }
    runLine(line, command::Origin::Gui);
}

void Controller::supplyObjects(const std::vector<std::int64_t>& ids)
{
    supplyValue(command::Value::ids(ids));
}

bool Controller::supplyPickedObjects()
{
    if (!awaitingInput() || promptKind() != command::ParamKind::Selection) return false;

    std::vector<std::int64_t> ids;
    for (core::EntityKey k : bus_.selection().keys())
        ids.push_back(static_cast<std::int64_t>(core::raw(k)));

    // NOTHING PICKED IS NOT AN ANSWER. Supplying an empty list would end the
    // command with no objects, which reads as the tool being broken; saying so and
    // staying armed lets the user carry on pointing.
    if (ids.empty()) {
        emit echoed(tr("Nesne seçilmedi. Nesneleri tıklayın, sonra Enter'a ya da sağ tuşa "
                       "basın; vazgeçmek için Esc."));
        return true;
    }

    supplyValue(command::Value::ids(ids));
    return true;
}

void Controller::supplyNumber(double value)
{
    supplyValue(command::Value::number(value));
}

void Controller::beginInteractive(const QString& line, command::Origin origin)
{
    // A command whose job is still running cannot be replaced: its worker owns
    // the read. The user stops it first (Durdur), or waits.
    if (session_ && session_->working()) {
        emit echoed(tr("Bir komut hâlâ çalışıyor: %1. Bitmesini bekleyin ya da Durdur.")
                        .arg(QString::fromStdString(session_->spec().id)));
        return;
    }
    cancelInteractive();

    auto started = bus_.begin_interactive(line.toStdString(), origin);
    if (!started) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(started.error().message)));
        return;
    }

    session_ = std::move(started.value());
    settleSession();
}

void Controller::settleSession()
{
    if (!session_) return;

    // Parked on a job: the host resumes it, and `onJobFinished` comes back here.
    if (session_->working()) {
        emit promptChanged(QString());
        settle();
        return;
    }

    if (session_->finished()) {
        // Read BEFORE `finish`, which is free to reset what the session holds.
        const QString id = QString::fromStdString(session_->spec().id);

        auto done    = bus_.finish(*session_);
        bool mutated = false;
        if (!done) {
            emit echoed(tr("Hata: %1").arg(QString::fromStdString(done.error().message)));
        } else {
            mutated = done.value().mutated;
            if (!done.value().message.empty())
                emit echoed(QString::fromStdString(done.value().message));
        }
        const bool asked = asked_;
        asked_           = false;
        session_.reset();
        emit promptChanged(QString());
        emit interactiveFinished(id, mutated, !asked);
    } else if (session_->waiting()) {
        asked_ = true;
        emit promptChanged(QString::fromStdString(session_->prompt().message));
    }

    settle();
    emit documentChanged();
}

void Controller::hostJob(command::Session& session)
{
    // Started from inside `Session::park_job`, before the coroutine has actually
    // suspended, so nothing here may resume the session: the worker only READS
    // the job, and `finished` reaches `onJobFinished` through the event loop.
    auto* runner = new JobRunner(session, this);
    jobThread_   = runner;
    connect(runner, &QThread::finished, this, &Controller::onJobFinished);
    const command::Job* job = session.job();
    emit jobStarted(job != nullptr ? QString::fromStdString(job->label) : QString());
    runner->start();
}

void Controller::onJobFinished()
{
    if (jobThread_ != nullptr) {
        jobThread_->wait();
        jobThread_->deleteLater();
        jobThread_ = nullptr;
    }
    emit jobFinished();

    if (!session_ || !session_->working()) return;
    session_->resume_job();
    settleSession();
}

bool Controller::awaitingInput() const
{
    return session_ && session_->waiting();
}

command::ParamKind Controller::promptKind() const
{
    if (!session_ || !session_->waiting()) return command::ParamKind::Point;
    return session_->prompt().kind;
}

void Controller::supplyPoint(core::Point2 world)
{
    supplyValue(command::Value::point(world));
}

void Controller::supplyText(const QString& text)
{
    supplyValue(command::Value::text(text.toStdString()));
}

void Controller::supplyValue(command::Value value)
{
    if (!session_ || !session_->waiting()) return;

    auto st = session_->supply(std::move(value));
    if (!st) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(st.error().message)));
    }

    settleSession();
}

void Controller::cancelInteractive()
{
    if (!session_) return;

    // DURDUR. The worker owns the read; the stop is requested and the command
    // finishes — as cancelled, with the document untouched — when the worker
    // returns and `onJobFinished` resumes it. Nothing is finished here.
    if (session_->working()) {
        session_->cancel();
        emit echoed(tr("Durduruluyor…"));
        return;
    }

    const QString id = QString::fromStdString(session_->spec().id);

    session_->cancel();
    auto done = bus_.finish(*session_);
    if (done && !done.value().message.empty())
        emit echoed(QString::fromStdString(done.value().message));

    // Esc is how a shape with an open number of points is FINISHED, not only how
    // it is abandoned: `ALAN` and `ÇİZGİ` read corners until the next one does not
    // come, so the Esc that ends a parsel is the same Esc that cancels an empty
    // run. `mutated` is what tells the two apart, and it is the whole reason the
    // tool can re-arm without trapping the user in it.
    const bool mutated = done && done.value().mutated;

    // Esc, the select arrow and a new tool DISMISS; the right button FINISHES.
    // A run that never asked for anything is dismissed too, whatever ended it.
    const bool dismissed = !finishing_ || !asked_;
    asked_               = false;
    session_.reset();
    emit promptChanged(QString());
    emit interactiveFinished(id, mutated, dismissed);
    settle();
    emit documentChanged();
}

void Controller::finishInteractive()
{
    finishing_ = true;
    cancelInteractive();
    finishing_ = false;
}

QString Controller::currentFile() const
{
    return QString::fromStdString(files_.current_path());
}

bool Controller::isDirty() const
{
    return document_.revision() != files_.saved_revision();
}

QString Controller::activeLayerName() const
{
    if (const core::Layer* l = document_.layer(bus_.active_layer()))
        return QString::fromStdString(l->name);
    return QStringLiteral("0");
}

} // namespace kentos::app
