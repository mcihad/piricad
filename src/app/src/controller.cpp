// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/controller.hpp"

#include "piricad/command/log.hpp"

#include "piricad/command/parser.hpp"

#include <QDir>
#include <QStandardPaths>

#include <filesystem>

namespace piricad::app {

Controller::Controller(QObject* parent)
    : QObject(parent), bus_(document_, registry_, journal_, undo_), files_(bus_), database_(bus_),
      runner_(bus_, script::Sandbox::Project)
#if PIRICAD_HAVE_LUA
      ,
      lua_runner_(bus_, script::Sandbox::Project)
#endif
{
    command::register_builtin_commands(registry_);

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
#if PIRICAD_HAVE_LUA
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
    // on a disk flush (piricad.md §10.4).
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
    bus_.on_prompt = [this](const command::Prompt& p) {
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
    // text names a transparent command such as ZOOM (piricad.md §3).
    if (session_ && session_->waiting()) {
        const command::CommandSpec* spec = registry_.resolve(trimmed.toStdString());
        const bool transparent = spec && has_flag(spec->flags, command::Flags::Transparent);
        if (!transparent) {
            auto parsed = command::parse_line(trimmed.toStdString());
            if (parsed && !parsed.value().tokens.empty() &&
                command::is_coordinate(parsed.value().tokens.front())) {
                auto pt = command::resolve_point(parsed.value().tokens.front(),
                                                 session_->prompt().rubber_origin);
                if (pt) {
                    supplyPoint(pt.value());
                    return command::DispatchResult{};
                }
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

void Controller::runCommand(const QString& name)
{
    const command::CommandSpec* spec = registry_.resolve(name.toStdString());
    if (!spec) {
        emit echoed(tr("Bilinmeyen komut: %1").arg(name));
        return;
    }

    // Interactive commands started from a button behave exactly as if typed.
    if (has_flag(spec->flags, command::Flags::Interactive) && !spec->params.empty()) {
        beginInteractive(name);
        return;
    }
    runLine(name, command::Origin::Gui);
}

void Controller::beginInteractive(const QString& name)
{
    cancelInteractive();

    auto started = bus_.begin_interactive(name.toStdString());
    if (!started) {
        emit echoed(tr("Hata: %1").arg(QString::fromStdString(started.error().message)));
        return;
    }

    session_ = std::move(started.value());
    if (session_->waiting()) {
        emit promptChanged(QString::fromStdString(session_->prompt().message));
    } else {
        auto done = bus_.finish(*session_);
        if (!done) emit echoed(tr("Hata: %1").arg(QString::fromStdString(done.error().message)));
        session_.reset();
        emit promptChanged(QString());
    }
    settle();
    emit documentChanged();
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
        session_.reset();
        emit promptChanged(QString());
        emit interactiveFinished(id, mutated);
    } else if (session_->waiting()) {
        emit promptChanged(QString::fromStdString(session_->prompt().message));
    }

    settle();
    emit documentChanged();
}

void Controller::cancelInteractive()
{
    if (!session_) return;

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

    session_.reset();
    emit promptChanged(QString());
    emit interactiveFinished(id, mutated);
    settle();
    emit documentChanged();
}

QString Controller::currentFile() const
{
    return QString::fromStdString(files_.current_path());
}

QString Controller::activeLayerName() const
{
    if (const core::Layer* l = document_.layer(bus_.active_layer()))
        return QString::fromStdString(l->name);
    return QStringLiteral("0");
}

} // namespace piricad::app
