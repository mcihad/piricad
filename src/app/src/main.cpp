// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — application entry point.
#include "piricad/app/main_window.hpp"
#include "piricad/app/map_canvas.hpp"
#include "piricad/app/theme.hpp"
#include "piricad/command/log.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <cstdio>
#include <vector>

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

    QApplication::setApplicationName(QStringLiteral("PiriCAD"));
    QApplication::setApplicationVersion(QStringLiteral(PIRICAD_VERSION));
    QApplication::setOrganizationName(QStringLiteral("PiriCAD"));
    QApplication::setOrganizationDomain(QStringLiteral("piricad.org"));

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
    piricad::app::installShellStyle();

    QString fontDir;
    if (!piricad::app::loadShellFonts(&fontDir)) {
        qWarning("PiriCAD: IBM Plex yüklenemedi (%s). Arayüz bu makinede tasarlandığı gibi "
                 "görünmeyecek; PIRICAD_DATA ile veri dizinini gösterin.",
                 fontDir.toUtf8().constData());
    }

    // Turkish is the source language. Case conversion of user-visible text goes
    // through QLocale, never std::toupper — 'i' upper-cases to 'İ', not 'I'
    // (piricad.md §13).
    QLocale::setDefault(QLocale(QLocale::Turkish, QLocale::Turkey));

    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("piricad"), QStringLiteral("_"),
                        QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("PiriCAD — Türkiye odaklı CBS + CAD harita yazılımı"));
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
    if (qEnvironmentVariableIsSet("PIRICAD_SMOKE")) {
        static QtMessageHandler chained = qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext& where, const QString& text) {
                if (text.contains(QLatin1String("stylesheet"))) sheet_refused = true;
                if (chained) chained(type, where, text);
            });
    }

    piricad::command::set_log_sink([](piricad::command::LogLevel level, std::string_view message) {
        // Diagnostics; see core/log.cpp for why the result is discarded.
        (void)std::fprintf(level >= piricad::command::LogLevel::Warn ? stderr : stdout,
                           "[piricad] %.*s\n", static_cast<int>(message.size()), message.data());
    });

    piricad::app::MainWindow window;
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
    // With PIRICAD_FRAME_DUMP set, the window paints once, is written to that PNG
    // path and the process exits. An ENVIRONMENT VARIABLE and not a command-line
    // option on purpose: a CLI flag is a user-facing feature and CLAUDE.md 5.17
    // would require its own /docs page, and this is not a feature a surveyor has
    // any use for. It is the same category as PIRICAD_BENCH_RECORD.
    //
    // Why it exists at all: the canvas is the one part of this program whose
    // correctness is a picture, and until now the only way to see that picture was
    // to sit in front of the machine. Under QT_QPA_PLATFORM=offscreen this
    // produces the picture without a display, which is what makes a rendering
    // change reviewable.
    // Opens the style designer on one layer, so its own frame can be dumped.
    // Same category as PIRICAD_FRAME_DUMP: developer tooling, not a feature.
    // Opens every window the shell has, one after another, and closes each.
    //
    // Developer tooling, same category as PIRICAD_FRAME_DUMP: not a feature a
    // surveyor has any use for, so an environment variable rather than a CLI flag
    // (a flag would be user-facing and CLAUDE.md 5.17 would want a /docs page).
    //
    // WHY IT EXISTS. `shell-starts` proved the main window comes up, and a
    // dialog that crashed the moment it opened still passed it — twice. A window
    // that is never constructed in any test is a window nothing is checking.
    if (qEnvironmentVariableIsSet("PIRICAD_SMOKE")) {
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
                (void)std::fprintf(stderr, "[piricad] duman testi: stil sayfası REDDEDİLDİ\n");
                QApplication::exit(2);
                return;
            }
            (void)std::fprintf(stdout, "[piricad] duman testi: bütün pencereler açıldı\n");
            QApplication::exit(0);
        });
    }

    // Every window, photographed, from the REAL binary.
    //
    // `PIRICAD_FRAME_DUMP` grabs one frame of whatever is in front. This opens
    // each window in turn, grabs it, closes it, and writes a numbered PNG into
    // the named directory — so a reviewer sees what the program actually draws
    // rather than what a test harness draws. Same category as the other two:
    // developer tooling, an environment variable rather than a CLI flag.
    if (const QByteArray dir = qgetenv("PIRICAD_SHOT_DIR"); !dir.isEmpty()) {
        const QString into = QString::fromLocal8Bit(dir);
        QDir().mkpath(into);

        int at          = kFrameDumpSettleMs;
        const auto shot = [into](const QString& name, QWidget* subject) {
            if (subject == nullptr) return;
            const QString path = into + QLatin1Char('/') + name + QStringLiteral(".png");
            (void)std::fprintf(subject->grab().save(path) ? stdout : stderr, "[piricad] %s\n",
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

    if (const QByteArray layer = qgetenv("PIRICAD_OPEN_DESIGNER"); !layer.isEmpty()) {
        const QString name = QString::fromLocal8Bit(layer);
        QTimer::singleShot(kFrameDumpSettleMs / 2, &window,
                           [&window, name] { window.openStyleDesigner(name); });
    }

    // Headless frame timing, for choosing between backends and for judging
    // whether a new one earns its keep. Same category as PIRICAD_FRAME_DUMP:
    // developer tooling, no /docs page, no user-facing flag.
    if (const QByteArray rounds = qgetenv("PIRICAD_FRAME_TIMES"); !rounds.isEmpty()) {
        const int n = std::max(1, rounds.toInt());
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, n] {
            piricad::app::MapCanvas* canvas = window.canvas();
            if (canvas == nullptr) {
                (void)std::fprintf(stderr, "[piricad] tuval yok\n");
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
                               "[piricad] %d kare  cizim ortanca %d us  en iyi %d us"
                               "  |  sahne ortanca %d us\n",
                               n, costs[costs.size() / 2], costs.front(),
                               scene.empty() ? 0 : scene[scene.size() / 2]);
            QApplication::exit(0);
        });
    }

    if (const QByteArray dump = qgetenv("PIRICAD_FRAME_DUMP"); !dump.isEmpty()) {
        const QString path = QString::fromLocal8Bit(dump);
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, path] {
            // The ACTIVE window, not the main one: a dialog under review is the
            // thing to photograph, and a modal `exec()` runs a nested event loop
            // so this timer still fires while it is up.
            QWidget* subject = QApplication::activeWindow();
            if (subject == nullptr) subject = &window;

            const bool saved = subject->grab().save(path);
            (void)std::fprintf(saved ? stdout : stderr, "[piricad] kare %s: %s\n",
                               saved ? "yazıldı" : "YAZILAMADI", qPrintable(path));
            QApplication::exit(saved ? 0 : 1);
        });
    }

    return QApplication::exec();
}
