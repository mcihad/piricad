// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — application entry point.
#include "kentos_cad/app/attribute_panel.hpp"
#include "kentos_cad/app/main_window.hpp"
#include "kentos_cad/app/map_canvas.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/command/log.hpp"
#include "kentos_cad/core/circle.hpp"

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

/// A window's picture, WITH the canvas frame in it.
///
/// `QWidget::grab()` walks the widget tree through the BACKING STORE, and a
/// `QRhiWidget` has nothing there: its frame lives on the GPU. A plain window grab
/// of a GPU build therefore comes out with a hole exactly where the drawing is —
/// which is what made a frame dump report an empty canvas on a canvas that was
/// drawing correctly. So the canvas is asked for its own frame and composited in.
/// Moves this user's settings, config and data from the program's former name.
///
/// The program was called KentOSCad until Faz 0 and Qt derives every per-user path
/// from the application and organisation names — the settings file, the config
/// directory the style designer writes a saved gösterim into, and the data
/// directory the autosave uses. Renaming without this would not lose the files,
/// which is worse than losing them: they would sit at the old path and the
/// program would report an empty style shelf and no preferences, and the user
/// would have no way to know why.
///
/// The old paths are ASKED FOR rather than guessed. Each platform lays these out
/// differently — `~/.config/<org>` on Linux, `Application Support` on macOS,
/// `AppData` on Windows — so the names are set to the old ones, Qt is asked, the
/// names are set to the new ones, and Qt is asked again. Guessing the layout here
/// would be a third place that has to agree with Qt about it.
///
/// Never overwrites: a destination that already exists is a user who has already
/// run the new build, and the old copy is left where it is rather than merged.
void migrate_user_data()
{
    struct Move
    {
        QStandardPaths::StandardLocation where;
        const char* what;
    };

    static const Move kMoves[] = {
        {QStandardPaths::AppConfigLocation, "ayar"},
        {QStandardPaths::AppDataLocation, "veri"},
    };

    const auto paths_under = [](const char* org, const char* app) {
        QApplication::setOrganizationName(QString::fromLatin1(org));
        QApplication::setApplicationName(QString::fromLatin1(app));

        QStringList out;
        for (const Move& m : kMoves)
            out << QStandardPaths::writableLocation(m.where);
        out << QSettings().fileName();
        return out;
    };

    const QStringList from = paths_under("KentOSCad", "KentOSCad");
    const QStringList to   = paths_under("KentOSCad", "KentOSCad");

    for (int i = 0; i < from.size() && i < to.size(); ++i) {
        if (from[i].isEmpty() || to[i].isEmpty() || from[i] == to[i]) continue;
        if (!QFileInfo::exists(from[i]) || QFileInfo::exists(to[i])) continue;

        // The parent has to exist before a rename into it, and on a fresh machine
        // it does not: nothing has written a KentOSCad path yet.
        QDir().mkpath(QFileInfo(to[i]).absolutePath());
        if (QFile::rename(from[i], to[i]))
            (void)std::fprintf(stderr, "[kentoscad] taşındı: %s -> %s\n",
                               from[i].toUtf8().constData(), to[i].toUtf8().constData());
    }
}

QImage window_shot(QWidget* subject)
{
    QImage shot = subject->grab().toImage();
    if (shot.isNull()) return shot;

    auto* canvas = subject->findChild<kentos::app::MapCanvas*>();
    if (canvas == nullptr || !canvas->isVisible()) return shot;

    const QImage frame = canvas->grabCanvas();
    if (frame.isNull()) return shot;

    QPainter painter(&shot);
    painter.drawImage(QRect(canvas->mapTo(subject, QPoint(0, 0)), canvas->size()), frame);
    return shot;
}

} // namespace

int main(int argc, char** argv)
{
    // Before QApplication, which is the only place Qt reads it.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // High DPI: pass the scale factor through rather than rounding it, so a 1.25
    // or 1.5 display gets the layout at its own scale instead of the nearest
    // integer one. `design.md` §12 asks for this by name, and the icons are SVG
    // for the same reason.
    QApplication app(argc, argv);

    // THE NAME THE USER SEES, and the one Qt derives every per-user path from:
    // the settings file, the config directory the style library writes into, and
    // the data directory the autosave uses. Changing it moves all three, so the
    // rename carries a migration and the migration runs BEFORE anything reads a
    // path — see `migrate_user_data` above.
    migrate_user_data();

    QApplication::setApplicationName(QStringLiteral("KentOSCad"));
    QApplication::setApplicationVersion(QStringLiteral(KENTOS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("KentOSCad"));
    QApplication::setOrganizationDomain(QStringLiteral("kentoscad.org"));

    // FUSION, ON EVERY PLATFORM, BEFORE THE FIRST WIDGET EXISTS.
    //
    // `design.md` §12 is explicit and it is the reason this line is here rather
    // than a preference: the native style is not inherited anywhere, so Windows,
    // macOS and Linux draw the same program. Without it Qt picks up the desktop's
    // own style and the same build looks like three different applications — a
    // Windows 11 combo box, a macOS one and whatever GTK theme the user has.
    //
    // AFTER `QApplication`, and that is not a preference either. `ShellStyle`
    // builds a `QProxyStyle` and `loadShellFonts` asks `data_root()` where the
    // executable is; both need an application object to exist. Called before
    // construction they do not fail politely — the program prints
    // "Please instantiate the QApplication object first" and dies on the next
    // line. Before the first widget is still required, which is why they sit
    // here rather than lower down.
    kentos::app::installShellStyle();

    QString fontDir;
    if (!kentos::app::loadShellFonts(&fontDir)) {
        qWarning("KentOSCad: IBM Plex yüklenemedi (%s). Arayüz bu makinede tasarlandığı gibi "
                 "görünmeyecek; KENTOS_DATA ile veri dizinini gösterin.",
                 fontDir.toUtf8().constData());
    }

    // Turkish is the source language. Case conversion of user-visible text goes
    // through QLocale, never std::toupper — 'i' upper-cases to 'İ', not 'I'
    // (kentoscad.md §13).
    QLocale::setDefault(QLocale(QLocale::Turkish, QLocale::Turkey));

    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("kentos"), QStringLiteral("_"),
                        QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("KentOSCad — Türkiye odaklı CBS + CAD harita yazılımı"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption scriptOption({QStringLiteral("b"), QStringLiteral("betik")},
                                    QStringLiteral("Açılışta çalıştırılacak JSON betiği."),
                                    QStringLiteral("dosya"));
    parser.addOption(scriptOption);
    parser.process(app);

    // Under the smoke test, a REFUSED STYLESHEET is a failure and not a warning.
    //
    // Qt answers an unparseable sheet with one line on stderr and then draws the
    // whole application in unstyled Fusion — every window wrong, and the process
    // still exits 0. `ci-gate-theme.sh` catches the cause statically; this
    // catches anything that reaches Qt by another road.
    static bool sheet_refused = false;
    if (qEnvironmentVariableIsSet("KENTOS_SMOKE")) {
        static QtMessageHandler chained = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& where, const QString& text) {
                if (text.contains(QLatin1String("stylesheet"))) sheet_refused = true;
                if (chained) chained(type, where, text);
            });
    }

    kentos::command::set_log_sink([](kentos::command::LogLevel level, std::string_view message) {
        // Diagnostics; see core/log.cpp for why the result is discarded.
        (void)std::fprintf(level >= kentos::command::LogLevel::Warn ? stderr : stdout,
                           "[kentos] %.*s\n", static_cast<int>(message.size()), message.data());
    });

    kentos::app::MainWindow window;
    window.show();

    if (parser.isSet(scriptOption)) {
        // Same road as the GUI button and the command line: no client is special.
        const QString path = parser.value(scriptOption);
        QTimer::singleShot(0, &window, [&window, path] { window.runScriptFile(path); });
    }

    // Long enough for the start-up script to finish and the canvas to paint once.
    // A fixed delay rather than a signal, because "the drawing has settled" is not
    // a thing the application knows: a script can open a file, and a coroutine
    // command can still be waiting for input.
    constexpr int kFrameDumpSettleMs = 600;

    // Headless frame proof, for a developer and for CI.
    //
    // With KENTOS_FRAME_DUMP set, the window paints once, is written to that PNG
    // path and the process exits. An ENVIRONMENT VARIABLE and not a command-line
    // option on purpose: a CLI flag is a user-facing feature and CLAUDE.md 5.17
    // would require its own /docs page, and this is not a feature a surveyor has
    // any use for. It is the same category as KENTOS_BENCH_RECORD.
    //
    // Why it exists at all: the canvas is the one part of this program whose
    // correctness is a picture, and until now the only way to see that picture was
    // to sit in front of the machine. Under QT_QPA_PLATFORM=offscreen this
    // produces the picture without a display, which is what makes a rendering
    // change reviewable.
    // Opens the style designer on one layer, so its own frame can be dumped.
    // Same category as KENTOS_FRAME_DUMP: developer tooling, not a feature.
    // Opens every window the shell has, one after another, and closes each.
    //
    // Developer tooling, same category as KENTOS_FRAME_DUMP: not a feature a
    // surveyor has any use for, so an environment variable rather than a CLI flag
    // (a flag would be user-facing and CLAUDE.md 5.17 would want a /docs page).
    //
    // WHY IT EXISTS. `shell-starts` proved the main window comes up, and a
    // dialog that crashed the moment it opened still passed it — twice. A window
    // that is never constructed in any test is a window nothing is checking.
    if (qEnvironmentVariableIsSet("KENTOS_SMOKE")) {
        int at           = 0;
        const auto later = [&window, &at](auto&& step) {
            at += 120;
            QTimer::singleShot(at, &window, step);
        };

        later([&window] { window.openStyleDesigner(QStringLiteral("0")); });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });
        later([&window] { window.openAttributeTable(); });
        later([] {
            if (QWidget* top = QApplication::activeWindow()) top->close();
        });
        later([&window] { window.openSettings(); });
        later([] {
            if (QWidget* top = QApplication::activeWindow()) top->close();
        });
        later([&window] { window.openCommandSearch(); });
        later([] {
            if (QWidget* top = QApplication::activePopupWidget()) top->close();
        });
        later([] {
            if (sheet_refused) {
                (void)std::fprintf(stderr, "[kentos] duman testi: stil sayfası REDDEDİLDİ\n");
                QApplication::exit(2);
                return;
            }
            (void)std::fprintf(stdout, "[kentos] duman testi: bütün pencereler açıldı\n");
            QApplication::exit(0);
        });
    }

    // Every window, photographed, from the REAL binary.
    //
    // `KENTOS_FRAME_DUMP` grabs one frame of whatever is in front. This opens
    // each window in turn, grabs it, closes it, and writes a numbered PNG into
    // the named directory — so a reviewer sees what the program actually draws
    // rather than what a test harness draws. Same category as the other two:
    // developer tooling, an environment variable rather than a CLI flag.
    if (const QByteArray dir = qgetenv("KENTOS_SHOT_DIR"); !dir.isEmpty()) {
        const QString into = QString::fromLocal8Bit(dir);
        QDir().mkpath(into);

        int at          = kFrameDumpSettleMs;
        const auto shot = [into](const QString& name, QWidget* subject) {
            if (subject == nullptr) return;
            const QString path = into + QLatin1Char('/') + name + QStringLiteral(".png");
            (void)std::fprintf(window_shot(subject).save(path) ? stdout : stderr, "[kentos] %s\n",
                               qPrintable(path));
        };
        const auto later = [&window, &at](auto&& step) {
            at += 260;
            QTimer::singleShot(at, &window, step);
        };

        // A size worth photographing. The window otherwise restores whatever the
        // profile last saved, which for a screenshot is somebody else's laptop.
        later([&window] { window.resize(1880, 1058); });
        // AFTER the resize. `--betik` fits the drawing to the canvas the moment
        // the script ends, which in this mode is before the window has its final
        // size — so the fit was computed against a window nobody will see.
        later([&window] { window.runScriptLine(QStringLiteral("YAKINLAŞ KAPSAM")); });
        later([&window, shot] { shot(QStringLiteral("1-ana-ekran"), &window); });

        later([&window] { window.openStyleDesigner(QStringLiteral("PARSEL")); });
        later([shot] {
            shot(QStringLiteral("2-stil-tasarimcisi"), QApplication::activeModalWidget());
        });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });

        later([&window] { window.openAttributeTable(); });
        later(
            [shot] { shot(QStringLiteral("3-oznitelik-tablosu"), QApplication::activeWindow()); });
        later([] {
            if (QWidget* top = QApplication::activeWindow()) top->close();
        });

        later([&window] { window.openSettings(); });
        later([shot] { shot(QStringLiteral("4-secenekler"), QApplication::activeWindow()); });
        later([] {
            if (QWidget* top = QApplication::activeWindow()) top->close();
        });

        later([&window] { window.openCommandSearch(); });
        later([shot] { shot(QStringLiteral("5-komut-arama"), QApplication::activePopupWidget()); });
        later([] {
            if (QWidget* top = QApplication::activePopupWidget()) top->close();
        });

        later([] { QApplication::exit(0); });
    }

    if (const QByteArray layer = qgetenv("KENTOS_OPEN_DESIGNER"); !layer.isEmpty()) {
        const QString name = QString::fromLocal8Bit(layer);
        QTimer::singleShot(kFrameDumpSettleMs / 2, &window,
                           [&window, name] { window.openStyleDesigner(name); });
    }

    // Headless frame timing, for choosing between backends and for judging
    // whether a new one earns its keep. Same category as KENTOS_FRAME_DUMP:
    // developer tooling, no /docs page, no user-facing flag.
    if (const QByteArray rounds = qgetenv("KENTOS_FRAME_TIMES"); !rounds.isEmpty()) {
        const int n = std::max(1, rounds.toInt());
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, n] {
            kentos::app::MapCanvas* canvas = window.canvas();
            if (canvas == nullptr) {
                (void)std::fprintf(stderr, "[kentos] tuval yok\n");
                QApplication::exit(1);
                return;
            }
            std::vector<int> costs = canvas->timeFrames(n);
            std::vector<int> scene = canvas->sceneCosts();
            std::sort(costs.begin(), costs.end());
            std::sort(scene.begin(), scene.end());
            // The MEDIAN and the best, not the mean: a headless run shares the
            // machine and one descheduled frame drags an average anywhere.
            (void)std::fprintf(stdout,
                               "[kentos] %d kare  cizim ortanca %d us  en iyi %d us"
                               "  |  sahne ortanca %d us\n",
                               n, costs[costs.size() / 2], costs.front(),
                               scene.empty() ? 0 : scene[scene.size() / 2]);
            QApplication::exit(0);
        });
    }

    // Corner editing, driven the way a user drives it: a press, some movement and
    // a release on the canvas widget.
    //
    // Developer tooling and an environment variable rather than a CLI flag, the
    // same category as KENTOS_FRAME_DUMP and KENTOS_SMOKE (CLAUDE.md 5.17 wants
    // a /docs page for a FEATURE, and this is not one).
    //
    // WHY IT EXISTS. The unit suite links no Qt, so it can prove `KÖŞETAŞI` moves
    // a corner but not that dragging a grip ever reaches `KÖŞETAŞI`. That gap was
    // not hypothetical: the canvas sent `nesne` as a bare integer where the
    // declaration says `ParamKind::Selection`, the bus refused the invocation
    // before the body ran, and the only trace was a line in the transcript. Every
    // unit test still passed, and grips could be grabbed and dragged with nothing
    // whatsoever happening on release.
    // Presses every button on the tool column and prints what came back. Same
    // category as KENTOS_EDIT_PROBE below: developer tooling, not a feature.
    if (qEnvironmentVariableIsSet("KENTOS_TOOL_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeToolBox();
            QApplication::exit(0);
        });
    }

    if (qEnvironmentVariableIsSet("KENTOS_HAND_PROBE")) {
        // The value, when it is a path, is the directory every step is
        // photographed into. See `MainWindow::probeToolsByHand`.
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeToolsByHand();
            QApplication::exit(0);
        });
    }

    if (qEnvironmentVariableIsSet("KENTOS_EDIT_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            auto* canvas = window.canvas();
            if (canvas == nullptr) {
                (void)std::fprintf(stderr, "[kentos] tuval yok\n");
                QApplication::exit(1);
                return;
            }

            window.runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
            window.runScriptLine(QStringLiteral("ALAN 485300,4310200 485360,4310200 "
                                                "485360,4310245 485300,4310245"));
            canvas->zoomToExtents();
            window.runScriptLine(QStringLiteral("SEÇ nesneler=1"));
            QCoreApplication::processEvents();

            const auto send = [&](QEvent::Type t, const QPointF& at, Qt::MouseButton b,
                                  Qt::MouseButtons held) {
                QMouseEvent ev(t, at, canvas->mapToGlobal(at), b, held, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &ev);
                QCoreApplication::processEvents();
            };

            /// One press-drag-release on the canvas, in widget coordinates.
            const auto drag = [&](const QPointF& from, const QPointF& to) {
                send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseMove, from + (to - from) * 0.5, Qt::NoButton, Qt::LeftButton);
                send(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
            };

            const auto at = [&](kentos::core::Point2 world) {
                const auto p = canvas->view().to_screen(world);
                return QPointF(p.x, p.y);
            };

            int failures     = 0;
            const auto check = [&](bool ok, const char* what) {
                if (!ok) {
                    ++failures;
                    (void)std::fprintf(stderr, "[kentos] BAŞARISIZ: %s\n", what);
                }
            };

            const auto& doc    = canvas->document();
            const auto corners = [&] { return doc.geometry().rings_of(doc.entities().slot[0]); };
            const auto corner  = [&](std::uint32_t i) {
                return doc.geometry().vertex(corners().first, i);
            };

            // 1. Drag corner 1. It must MOVE, and the object must stay one object
            //    with the same key — a corner correction is not a new parsel.
            const auto key_before = doc.entities().key[0];
            const QPointF grabbed = at(kentos::core::Point2{485300000, 4310200000});
            drag(grabbed, grabbed + QPointF(60, -40));

            check(doc.geometry().ring_xs(corners().first).size() == 4,
                  "köşe sayısı taşımada değişti");
            check(corner(0).x != 485300000 || corner(0).y != 4310200000, "köşe taşınmadı");
            check(doc.entities().key[0] == key_before, "taşıma nesnenin kimliğini değiştirdi");
            check(doc.live_entity_count() == 1, "taşıma nesne sayısını değiştirdi");

            // 2. Drag the MIDDLE of the edge from corner 2 to corner 3. That is not
            //    a corner, so it must INSERT one rather than move either end.
            const QPointF on_edge = at(kentos::core::Point2{485360000, 4310222500});
            drag(on_edge, on_edge + QPointF(50, 0));

            check(doc.geometry().ring_xs(corners().first).size() == 5, "kenara köşe eklenmedi");
            check(corner(1).x == 485360000 && corner(1).y == 4310200000,
                  "ekleme kendinden önceki köşeyi oynattı");
            check(doc.live_entity_count() == 1, "ekleme nesne sayısını değiştirdi");

            // 3. A press and release with no travel is a CLICK. It must write
            //    nothing at all, or every grip a user brushes past becomes an undo
            //    step they have to press Ctrl+Z through.
            const std::uint64_t before = doc.content_hash();
            const QPointF still        = at(corner(0));
            send(QEvent::MouseButtonPress, still, Qt::LeftButton, Qt::LeftButton);
            send(QEvent::MouseButtonRelease, still, Qt::LeftButton, Qt::NoButton);
            check(doc.content_hash() == before, "kıpırdamayan tıklama çizimi değiştirdi");

            // 4. A DRAW TOOL STAYS ARMED. Picking ALAN, drawing a parsel and
            //    finishing it must leave the tool ready for the next parsel; the
            //    Esc after that — with nothing drawn — must put it away. This is
            //    the whole modal-tool contract, and it used to fail twice over:
            //    the tool column lit ÇİZGİ for every command whatever was running,
            //    and finishing a shape dropped the user back on the select tool.
            auto* polygon = window.findChild<QAction*>(QStringLiteral("toolAction.ALAN"));
            check(polygon != nullptr, "ALAN aracı bulunamadı");
            if (polygon != nullptr) {
                const auto esc = [&] {
                    QKeyEvent k(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &k);
                    QCoreApplication::processEvents();
                };

                polygon->trigger();
                QCoreApplication::processEvents();
                check(polygon->isChecked(), "ALAN seçilince araç yanmıyor");

                const std::size_t drawn_before = doc.live_entity_count();
                send(QEvent::MouseButtonPress, QPointF(120, 120), Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, QPointF(120, 120), Qt::LeftButton, Qt::NoButton);
                send(QEvent::MouseButtonPress, QPointF(220, 120), Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, QPointF(220, 120), Qt::LeftButton, Qt::NoButton);
                send(QEvent::MouseButtonPress, QPointF(220, 220), Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, QPointF(220, 220), Qt::LeftButton, Qt::NoButton);

                esc();
                check(doc.live_entity_count() == drawn_before + 1, "arayüzden alan çizilemedi");
                check(polygon->isChecked(), "alan bitince araç sönüyor (yeniden kurulmuyor)");

                esc();
                check(!polygon->isChecked(), "boş Esc'ten sonra araç hâlâ yanıyor");
            }

            // 5. THE GUIDE SHOWS THE SHAPE. A circle previewed as a line from the
            //    centre tells the user nothing about the circle, so the overlay
            //    must carry a many-vertex run while DAİRE waits for its rim point.
            //    Counted rather than looked at: the run is the guide.
            auto* circle = window.findChild<QAction*>(QStringLiteral("toolAction.DAİRE"));
            check(circle != nullptr, "DAİRE aracı bulunamadı");
            if (circle != nullptr) circle->trigger();
            QCoreApplication::processEvents();
            send(QEvent::MouseButtonPress, QPointF(200, 200), Qt::LeftButton, Qt::LeftButton);
            send(QEvent::MouseButtonRelease, QPointF(200, 200), Qt::LeftButton, Qt::NoButton);
            send(QEvent::MouseMove, QPointF(280, 200), Qt::NoButton, Qt::NoButton);
            (void)canvas->grabCanvas(); // the overlay is built while painting

            check(canvas->guideVertexCountForProbe() >= kentos::core::kCircleSegments,
                  "daire kılavuzu çember çizmiyor");

            {
                QKeyEvent k(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &k);
                QCoreApplication::processEvents();
            }

            // 6. METİN WRITES FROM THE CANVAS. The anchor is a click; the string
            //    is typed into a box that opens where the caption goes. This used
            //    to hang: the click supplied the anchor, the prompt turned into a
            //    text prompt, and no client could answer it — focus stayed on the
            //    canvas while the prompt sat in the command line's placeholder.
            auto* textTool = window.findChild<QAction*>(QStringLiteral("toolAction.METİN"));
            check(textTool != nullptr, "METİN aracı bulunamadı");
            if (textTool != nullptr) {
                const std::size_t before_text = doc.live_entity_count();
                textTool->trigger();
                QCoreApplication::processEvents();

                send(QEvent::MouseButtonPress, QPointF(300, 300), Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, QPointF(300, 300), Qt::LeftButton, Qt::NoButton);

                // The anchor went in; the box must now be open and focused.
                auto* box = canvas->findChild<QLineEdit*>(QStringLiteral("canvasTextEditor"));
                check(box != nullptr && box->isVisible(), "metin kutusu açılmadı");

                if (box != nullptr) {
                    box->setText(QStringLiteral("ADA 1284"));
                    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QCoreApplication::sendEvent(box, &enter);
                    QCoreApplication::processEvents();

                    check(!box->isVisible(), "metin girildikten sonra kutu kapanmadı");
                    check(doc.live_entity_count() == before_text + 1, "arayüzden metin yazılamadı");
                    check(doc.texts().pool_size() > 0, "yazılan metin belgede yok");
                }

                {
                    QKeyEvent k(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &k);
                    QCoreApplication::processEvents();
                }
            }

            // 7. DYNAMIC INPUT AND THE STEP. While a guide is dragging, the
            //    overlay must carry a label with the length on it — and with a
            //    step set, the length it shows must be a multiple of that step.
            {
                window.runScriptLine(QStringLiteral("MOD ad=adım deger=1000")); // 1 m
                auto* lineTool = window.findChild<QAction*>(QStringLiteral("toolAction.ÇİZGİ"));
                check(lineTool != nullptr, "ÇİZGİ aracı bulunamadı");
                if (lineTool != nullptr) {
                    lineTool->trigger();
                    QCoreApplication::processEvents();
                    send(QEvent::MouseButtonPress, QPointF(400, 400), Qt::LeftButton,
                         Qt::LeftButton);
                    send(QEvent::MouseButtonRelease, QPointF(400, 400), Qt::LeftButton,
                         Qt::NoButton);
                    send(QEvent::MouseMove, QPointF(520, 400), Qt::NoButton, Qt::NoButton);
                    (void)canvas->grabCanvas();

                    check(canvas->guideLabelForProbe().find(" m") != std::string::npos,
                          "kılavuz uzunluğu yazmıyor");

                    QKeyEvent k(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &k);
                    QCoreApplication::processEvents();
                    QCoreApplication::sendEvent(canvas, &k);
                    QCoreApplication::processEvents();
                }
                window.runScriptLine(QStringLiteral("MOD ad=adım deger=0"));
            }

            // 8. THE ATTRIBUTE PANEL WRITES THROUGH THE BUS. A column is declared,
            //    the parsel is selected, and the cell is edited from the panel —
            //    the value must reach the document, and it must arrive as a
            //    command so it undoes in one step like any other edit.
            {
                window.runScriptLine(QStringLiteral("SÜTUN kimlik=ada_no tur=metin"));
                window.runScriptLine(QStringLiteral("SEÇ nesneler=1"));
                QCoreApplication::processEvents();

                auto* panel = window.findChild<kentos::app::AttributePanel*>();
                check(panel != nullptr, "öznitelik paneli bulunamadı");
                if (panel != nullptr) {
                    check(panel->editRowForProbe(QStringLiteral("ada_no"), QStringLiteral("1284")),
                          "öznitelik satırı düzenlenemedi");
                    QCoreApplication::processEvents();

                    // READ THE WAY THE DOCUMENT READS. An attribute column is
                    // indexed by geometry slot, not by entity slot, and only
                    // `Document::attribute` knows the mapping.
                    const auto col = doc.attributes().find("ada_no");
                    check(col != kentos::core::kNoAttr, "ada_no sütunu tanımlanmadı");

                    const auto stored = doc.attribute(col, doc.slot_of(doc.entities().key[0]));
                    check(stored.ok() && stored.value().present && stored.value().text == "1284",
                          "panelden yazılan öznitelik belgeye ulaşmadı");

                    window.runScriptLine(QStringLiteral("GERİAL"));
                    QCoreApplication::processEvents();
                }
            }

            if (failures == 0) (void)std::fprintf(stdout, "[kentos] tuval düzenleme: tamam\n");
            QApplication::exit(failures == 0 ? 0 : 1);
        });
    }

    if (const QByteArray dump = qgetenv("KENTOS_FRAME_DUMP"); !dump.isEmpty()) {
        const QString path = QString::fromLocal8Bit(dump);
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, path] {
            // The ACTIVE window, not the main one: a dialog under review is the
            // thing to photograph, and a modal `exec()` runs a nested event loop
            // so this timer still fires while it is up.
            QWidget* subject = QApplication::activeWindow();
            if (subject == nullptr) subject = &window;

            // KENTOS_ARM presses one tool-column button before the shot, so a
            // review can see what an ARMED tool looks like. The actions are named
            // `toolAction.<KOMUT>` for exactly this kind of reach; same category
            // as the probes above — developer tooling, not a feature.
            if (const QByteArray arm = qgetenv("KENTOS_ARM"); !arm.isEmpty()) {
                const QString name = QStringLiteral("toolAction.") + QString::fromUtf8(arm);
                if (QAction* action = window.findChild<QAction*>(name)) {
                    action->trigger();
                    QCoreApplication::processEvents();
                } else {
                    (void)std::fprintf(stderr, "[kentos] araç bulunamadı: %s\n", qPrintable(name));
                }
            }

            const bool saved = window_shot(subject).save(path);
            (void)std::fprintf(saved ? stdout : stderr, "[kentos] kare %s: %s\n",
                               saved ? "yazıldı" : "YAZILAMADI", qPrintable(path));
            QApplication::exit(saved ? 0 : 1);
        });
    }

    return QApplication::exec();
}
