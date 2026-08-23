// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — application entry point.
#include "piricad/app/main_window.hpp"
#include "piricad/command/log.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

#include <cstdio>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QApplication::setApplicationName(QStringLiteral("PiriCAD"));
    QApplication::setApplicationVersion(QStringLiteral(PIRICAD_VERSION));
    QApplication::setOrganizationName(QStringLiteral("PiriCAD"));
    QApplication::setOrganizationDomain(QStringLiteral("piricad.org"));

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
    if (const QByteArray layer = qgetenv("PIRICAD_OPEN_DESIGNER"); !layer.isEmpty()) {
        const QString name = QString::fromLocal8Bit(layer);
        QTimer::singleShot(kFrameDumpSettleMs / 2, &window,
                           [&window, name] { window.openStyleDesigner(name); });
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
