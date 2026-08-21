// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — application entry point.
#include "piricad/app/main_window.hpp"
#include "piricad/core/log.hpp"

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

    piricad::core::set_log_sink([](piricad::core::LogLevel level, std::string_view message) {
        // Diagnostics; see core/log.cpp for why the result is discarded.
        (void)std::fprintf(level >= piricad::core::LogLevel::Warn ? stderr : stdout,
                           "[piricad] %.*s\n", static_cast<int>(message.size()), message.data());
    });

    piricad::app::MainWindow window;
    window.show();

    if (parser.isSet(scriptOption)) {
        // Same road as the GUI button and the command line: no client is special.
        const QString path = parser.value(scriptOption);
        QTimer::singleShot(0, &window, [&window, path] { window.runScriptFile(path); });
    }

    return QApplication::exec();
}
