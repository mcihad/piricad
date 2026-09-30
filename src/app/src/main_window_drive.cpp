// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: every ribbon tool, USED (`PIRICAD_TOOL_DRIVE=<dir>`).
//
// THE USER SAID SOME TOOLS DO NOT WORK AND SOME WORK WRONGLY, and named none.
// Pressing a button proves it starts a command; it does not prove the command
// can be finished by somebody who answers its questions the obvious way. This
// drive does that for every tool the ribbon shows, family members included:
// on one drawing that holds an object of every class, each tool is pressed
// with nothing selected and every question it asks is answered the way a hand
// would — an object of the class the command is declared for, a point on that
// object when the question says to click one, a free point otherwise, a
// number, the first word it offers — until the command ends. What it did is
// written down: made or changed something, said it could not and why, asked
// forever, or opened a window. Then it is undone, so the next tool meets the
// same drawing.
//
// It is a MEASUREMENT for a reviewer, not a gate: a tool whose obvious answers
// are not the right ones is listed and read, not failed.
#include "piricad/app/main_window.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/app/map_canvas.hpp"
#include "piricad/app/ribbon.hpp"
#include "piricad/command/registry.hpp"
#include "piricad/command/session.hpp"
#include "piricad/command/targets.hpp"
#include "piricad/core/curve_path.hpp"
#include "piricad/core/document.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include <array>
#include <cstdio>
#include <optional>
#include <vector>

namespace piricad::app {
namespace {

/// The property every tool action carries (ribbon.hpp).
constexpr const char* kCommandProperty = kToolCommandProperty;

/// What one tool did.
struct Outcome
{
    QString label;
    QString command;
    QString result; ///< `yaptı`, `değişmedi`, `reddetti`, `takıldı`, `okudu`, `pencere`, `soluk`
    QString detail;
};

} // namespace

int MainWindow::probeToolDrive()
{
    const QString into = QString::fromLocal8Bit(qgetenv("PIRICAD_TOOL_DRIVE"));
    QDir().mkpath(into);
    const auto settle = [] {
        for (int i = 0; i < 3; ++i)
            QCoreApplication::processEvents();
    };

    // ---- one drawing with an object of every class ----
    runScriptLine(QStringLiteral("YENİ"));
    endCommand();
    for (const char* line : {
             "KATMAN ad=PARSEL",
             "ALAN 0,0 20,0 20,20 0,20",                     // 1: a parcel
             "ALAN 20,0 40,0 40,20 20,20",                   // 2: its neighbour
             "ÇOKLUÇİZGİ 0,30 20,30 20,45",                  // 3: a line with a corner
             "ÇİZGİ 10,25 10,50",                            // 4: a line across it
             "DAİRE 60,10 65,10",                            // 5: a circle
             "YAY merkez=60,35 baslangic=66,35 bitis=54,35", // 6: an arc
             "METİN 5,10 \"12/3\" 1500",                     // 7: a caption
             "NOKTA 30,40",                                  // 8: a point
             "ÖLÇÜ 0,-5 20,-5 10,-8",                        // 9: a dimension
         }) {
        runScriptLine(QString::fromUtf8(line));
        endCommand();
    }
    canvas_->zoomToBox(
        core::Box2{.min_x = -10'000, .min_y = -15'000, .max_x = 80'000, .max_y = 60'000});
    settle();
    const std::size_t base    = controller_->undoStack().undo_depth();
    const core::Document& doc = controller_->document();

    // THE OBJECT A QUESTION IS ANSWERED WITH: the first of the class the
    // command takes, by key.
    const auto object_for = [&doc](command::Targets targets) -> std::optional<std::int64_t> {
        for (std::int64_t k = 1; k <= 9; ++k) {
            const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(k));
            if (e == core::kNoEntity || !doc.alive(e)) continue;
            if (targets == command::Targets::Any || command::acts_on(targets, doc, e)) return k;
        }
        return std::nullopt;
    };
    // A point ON it: a corner when the question names one, else the middle of
    // its first piece.
    const auto point_on = [&doc](std::int64_t key, bool corner) -> std::optional<core::Point2> {
        const core::EntityId e = doc.slot_of(static_cast<core::EntityKey>(key));
        if (e == core::kNoEntity || !doc.alive(e)) return std::nullopt;
        const auto path = core::path_of(doc, e, core::PathScope::Curves);
        if (!path || path->pieces.empty()) {
            const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
            if (rs.count == 0) return std::nullopt;
            return doc.geometry().vertex(rs.first, 0);
        }
        if (corner && path->pieces.size() > 1) return path->pieces[1].from;
        // A quarter along, not the middle: the scene's lines cross at their
        // middles, and a click on a crossing names two objects.
        return core::point_at(*path, core::PathPlace{0, 0.25});
    };
    // Free ground, in turn, clear of the objects.
    const std::vector<core::Point2> free_points{
        {45'000, 45'000}, {55'000, 52'000}, {70'000, 45'000}, {72'000, 55'000}, {48'000, 55'000},
    };

    // A WINDOW THE TOOL OPENS is closed again, and said.
    QString opened;
    QTimer closer;
    closer.setInterval(150);
    connect(&closer, &QTimer::timeout, this, [&opened] {
        if (QWidget* top = QApplication::activeModalWidget(); top != nullptr) {
            opened = top->windowTitle().isEmpty() ? top->objectName() : top->windowTitle();
            top->close();
        }
    });
    closer.start();

    QStringList said;
    const auto listening = connect(controller_, &Controller::echoed, this,
                                   [&said](const QString& text) { said << text; });

    // ---- every tool the ribbon shows ----
    std::vector<QAction*> tools;
    QSet<const QAction*> seen;
    const auto take = [&](QAction* a) {
        if (a == nullptr || seen.contains(a) || a->property(kCommandProperty).toString().isEmpty())
            return;
        seen.insert(a);
        tools.push_back(a);
    };
    for (const QToolButton* button : ribbonButtons()) {
        QAction* shown = button->defaultAction();
        bool member    = false;
        for (const RibbonFamily* f : std::as_const(families_))
            if (f->head() == shown) {
                for (QAction* m : f->members())
                    take(m);
                member = true;
            }
        if (!member) take(shown);
    }
    // THE FILE AND PROGRAM VERBS are left out: they ask the disk or leave.
    const QSet<QString> skip{
        QStringLiteral("YENİ"),         QStringLiteral("AÇ"),       QStringLiteral("KAYDET"),
        QStringLiteral("FARKLIKAYDET"), QStringLiteral("İÇEAKTAR"), QStringLiteral("DIŞAAKTAR"),
        QStringLiteral("YAZDIR"),       QStringLiteral("ÇIKIŞ"),    QStringLiteral("VERİTABANI"),
        QStringLiteral("MCPSUNUCU"),
    };

    // A COMMAND THAT ENDED is not answered again: a tool re-arms itself for
    // the next shape, and the drive is about the first.
    int finished        = 0;
    const auto counting = connect(controller_, &Controller::commandFinished, this,
                                  [&finished](const QString&, const QString&) { ++finished; });

    std::vector<Outcome> outcomes;
    for (const bool picked_first : {false, true})
        for (QAction* action : tools) {
            const QString line = action->property(kCommandProperty).toString();
            const QString word = line.section(QLatin1Char(' '), 0, 0);
            if (skip.contains(word)) continue;
            const command::CommandSpec* spec = controller_->registry().resolve(word.toStdString());
            const command::Targets targets =
                spec != nullptr ? spec->targets : command::Targets::Any;
            bool takes_objects = false;
            if (spec != nullptr)
                for (const command::Param& p : spec->params)
                    takes_objects = takes_objects || p.kind == command::ParamKind::Selection;
            // THE SECOND PASS is the order a surveyor works in: pick the object,
            // then reach for the tool — only for a tool that takes objects.
            if (picked_first && !takes_objects) continue;

            Outcome out{.label   = QString(action->text()).remove(QLatin1Char('&')) +
                                   (picked_first ? QStringLiteral(" (seçili)") : QString()),
                        .command = line};
            controller_->clearSelection();
            if (picked_first)
                if (const auto key = object_for(targets)) {
                    runScriptLine(QStringLiteral("SEÇ mod=NESNE nesneler=%1").arg(*key));
                    endCommand();
                }
            said.clear();
            opened.clear();
            finished = 0;
            settle();
            if (!action->isEnabled()) {
                out.result = QStringLiteral("soluk");
                outcomes.push_back(out);
                continue;
            }
            action->trigger();
            settle();

            std::size_t free_used = 0;
            std::size_t numbers   = 0;
            QString last_prompt;
            const bool draws = spec != nullptr && spec->category == command::Category::Draw;
            for (int step = 0; step < 14 && finished == 0; ++step) {
                const command::Session* s = controller_->session();
                if (s == nullptr || !s->waiting()) break;
                const command::Prompt& prompt = s->prompt();
                last_prompt                   = QString::fromStdString(prompt.message);
                const QString asked           = last_prompt.toLower();
                switch (prompt.kind) {
                case command::ParamKind::Selection: {
                    if (const auto key = object_for(targets)) controller_->supplyObjects({*key});
                    // Picked: Enter hands the pick over, as a hand's right click does.
                    settle();
                    if (controller_->session() != nullptr && controller_->session()->waiting() &&
                        controller_->promptKind() == command::ParamKind::Selection)
                        controller_->finishInteractive();
                    break;
                }
                case command::ParamKind::Point:
                case command::ParamKind::PointList: {
                    // A DRAWING TOOL is given free ground; an EDIT is given its object.
                    const bool on_object = !draws && (asked.contains(QStringLiteral("tıklayın")) ||
                                                      asked.contains(QStringLiteral("gösterin")) ||
                                                      asked.contains(QStringLiteral("köşe")) ||
                                                      asked.contains(QStringLiteral("kenar")));
                    const bool corner    = asked.contains(QStringLiteral("köşe"));
                    std::optional<core::Point2> at;
                    if (on_object)
                        if (const auto key = object_for(targets)) at = point_on(*key, corner);
                    if (!at) at = free_points[free_used++ % free_points.size()];
                    controller_->supplyPoint(*at);
                    break;
                }
                case command::ParamKind::Number: {
                    // Figures that differ, so two bearings are not parallel and two
                    // distances reach each other.
                    constexpr std::array<double, 4> kFigures{6.0, 7.0, 5.0, 3.0};
                    controller_->supplyNumber(kFigures.at(numbers++ % kFigures.size()));
                    break;
                }
                case command::ParamKind::Integer: controller_->supplyNumber(3.0); break;
                case command::ParamKind::Text:
                    controller_->supplyText(prompt.choices.empty()
                                                ? QStringLiteral("Deneme")
                                                : QString::fromStdString(prompt.choices.front()));
                    break;
                case command::ParamKind::Bool:
                    controller_->supplyText(QStringLiteral("evet"));
                    break;
                default: controller_->supplyText(QString()); break;
                }
                settle();
            }
            const command::Session* left = controller_->session();
            const bool stuck             = finished == 0 && left != nullptr && left->waiting();
            if (stuck) {
                controller_->finishInteractive();
                settle();
            }
            if (controller_->session() != nullptr) {
                controller_->cancelAll();
                settle();
            }

            QString refusal;
            for (const QString& t : std::as_const(said))
                if (t.startsWith(QStringLiteral("Hata:"))) refusal = t.mid(6);
            const bool changed = controller_->undoStack().undo_depth() > base;
            if (!opened.isEmpty()) {
                out.result = QStringLiteral("pencere");
                out.detail = opened;
            } else if (!refusal.isEmpty()) {
                out.result = QStringLiteral("reddetti");
                out.detail = refusal;
            } else if (stuck && !changed && spec != nullptr &&
                       has_flag(spec->flags, command::Flags::ReadOnly)) {
                // A READING TOOL ASKS UNTIL IT IS TOLD TO STOP: ALANÖLÇ, ÖLÇ, AÇIÖLÇ.
                out.result = QStringLiteral("okudu");
                out.detail = said.isEmpty() ? last_prompt : said.back();
            } else if (stuck && !changed) {
                out.result = QStringLiteral("takıldı");
                out.detail = last_prompt;
            } else if (changed) {
                out.result = QStringLiteral("yaptı");
                out.detail = said.isEmpty() ? QString() : said.back();
            } else {
                out.result = QStringLiteral("değişmedi");
                out.detail = said.isEmpty() ? last_prompt : said.back();
            }
            outcomes.push_back(out);

            // THE SAME DRAWING FOR THE NEXT TOOL.
            while (controller_->undoStack().undo_depth() > base) {
                runScriptLine(QStringLiteral("GERİAL"));
                endCommand();
            }
            settle();
        }
    disconnect(listening);
    disconnect(counting);
    closer.stop();

    // ---- the report ----
    QFile file(into + QStringLiteral("/araclar.md"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream md(&file);
        md << "| Araç | Komut | Sonuç | Ayrıntı |\n|---|---|---|---|\n";
        for (const Outcome& o : outcomes)
            md << "| " << o.label << " | `" << o.command << "` | " << o.result << " | "
               << QString(o.detail).replace(QLatin1Char('|'), QLatin1Char('/')).simplified()
               << " |\n";
    }
    int problems = 0;
    for (const Outcome& o : outcomes) {
        const bool bad =
            o.result == QLatin1String("reddetti") || o.result == QLatin1String("takıldı");
        problems += bad ? 1 : 0;
        (void)std::fprintf(stdout, "[araç] %-9s %-32s %s — %s\n", qPrintable(o.result),
                           qPrintable(o.label), qPrintable(o.command),
                           qPrintable(o.detail.simplified().left(140)));
    }
    (void)std::fprintf(stdout, "[araç] %zu araç, %d sorunlu\n", outcomes.size(), problems);
    return 0;
}

} // namespace piricad::app
