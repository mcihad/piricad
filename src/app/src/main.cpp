// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — application entry point.
#include "kentos_cad/app/attribute_panel.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/import_wizard.hpp"
#include "kentos_cad/app/layout_designer.hpp"
#include "kentos_cad/app/layout_render.hpp"
#include "kentos_cad/app/main_window.hpp"
#include "kentos_cad/app/map_canvas.hpp"
#include "kentos_cad/app/provider_dialog.hpp"
#include "kentos_cad/app/settings_dialog.hpp"
#include "kentos_cad/app/suggestion_card.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/command/log.hpp"
#include "kentos_cad/core/circle.hpp"

#include <csignal>

// glibc and the BSDs carry `<execinfo.h>`; MSVC does not, and there the handler
// still catches the signal and still dies correctly, just without the trace.
#if defined(__has_include)
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#include <unistd.h>
#define KENTOS_HAVE_BACKTRACE 1
#endif
#endif
#ifndef KENTOS_HAVE_BACKTRACE
#define KENTOS_HAVE_BACKTRACE 0
#endif

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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

/// Writes a stack trace and dies, so the NEXT crash is a report rather than a
/// shrug.
///
/// WHY THIS EXISTS. A user reported the program "falling into a segfault at a
/// meaningless point". Two teardown defects were found with the sanitizers and
/// fixed, but neither could be provoked from the probes — and a crash that cannot
/// be reproduced cannot be fixed from a description of where the mouse was. A
/// stack trace turns the next one into a bug report.
///
/// ASYNC-SIGNAL-SAFE, which rules out almost everything: no `printf`, no
/// allocation, no Qt. `backtrace_symbols_fd` is the one member of the pair that
/// writes without allocating, which is why the trace goes to a descriptor rather
/// than into a string. The default handler is restored and the signal re-raised,
/// so the process still dies the way the system expects and a core file is still
/// written.
extern "C" void kentos_crash_handler(int signal_number)
{
#if KENTOS_HAVE_BACKTRACE
    static const char banner[] =
        "\n[kentos] ÇÖKME. Aşağıdaki yığın izini hata bildirimine ekleyin.\n";
    // The result is discarded on purpose and the cast is not enough for GCC: a
    // handler that branched on a failed write would be a handler doing more work
    // inside a signal, which is the one thing it must not do.
    if (::write(STDERR_FILENO, banner, sizeof(banner) - 1) < 0) { /* nothing to do */
    }

    void* frames[64];
    const int depth = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, depth, STDERR_FILENO);
#endif

    // Back to the system's own handler, then let the signal through: a process
    // that swallowed its own SIGSEGV would leave no core file and no exit status
    // anybody could act on.
    // Neither result is actionable INSIDE A SIGNAL HANDLER, which is the whole
    // reason they are discarded: if restoring the default handler failed there is
    // no second mechanism to try, and reporting it would mean doing more work in
    // the one place that must do as little as possible.
    (void)std::signal(signal_number, SIG_DFL);
    (void)std::raise(signal_number);
}

/// Installs the handler for the signals a defect in this program can raise.
void install_crash_handler()
{
    for (const int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
        (void)std::signal(sig, &kentos_crash_handler);
}

} // namespace

int main(int argc, char** argv)
{
    // FIRST, before Qt and before anything that can fault.
    install_crash_handler();

    // Before QApplication, which is the only place Qt reads it.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // High DPI: pass the scale factor through rather than rounding it, so a 1.25
    // or 1.5 display gets the layout at its own scale instead of the nearest
    // integer one. `design.md` §12 asks for this by name, and the icons are SVG
    // for the same reason.
    QApplication app(argc, argv);

    // THE SHELL DRAWS ITS OWN MENUS (design.md 7: they sit in the title bar),
    // so no window ever wants the platform's menu bar — and on macOS a native
    // `QMenuBar` is not merely unwanted, it BLANKS THE CANVAS. Qt's
    // `QMenuBarPrivate::handleReparent` forces `createWinId()` on the window a
    // native menu bar is parented into, so the main window's native window was
    // created the moment `TitleBar` was — before `MapCanvas` existed. Qt decides
    // ONCE, at that creation, whether the window composites through QRhi
    // (`QWidgetPrivate::create` → `q_evaluateRhiConfig`), found no `QRhiWidget`
    // child yet, and never asked again: every frame then ended in "QRhiWidget:
    // No QRhi" and the drawing, the grid and the rulers were simply not there.
    // Linux has no native menu bar, so the same code drew fine — which is why it
    // was found on a Mac. The smoke test below asserts the canvas has its RHI.
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);

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

    // THE PRINT PROBE MUST NOT TOUCH THE USER'S PROFILES. It adds a profile and
    // makes it the default, and both are persisted — into the very file the
    // person's own sheets live in. Qt's test mode redirects every
    // `QStandardPaths` lookup into a sandbox, and it has to be set BEFORE the
    // window is built, because the print service resolves its path once in its
    // constructor.
    if (qEnvironmentVariableIsSet("KENTOS_PRINT_PROBE")) QStandardPaths::setTestModeEnabled(true);

    kentos::app::MainWindow window;
    window.show();

    if (parser.isSet(scriptOption)) {
        // Same road as the GUI button and the command line: no client is special.
        const QString path = parser.value(scriptOption);
        QTimer::singleShot(0, &window, [&window, path] { window.runScriptFile(path); });
    }

    // ONE TYPED LINE AT START-UP, for the frame proof below. A script runs through
    // the script runner, which drives every command to completion in one call;
    // this runs through the command line's own road, so a hosted job — an import
    // reading on a worker thread — can be photographed with its Durdur on the
    // status strip. Developer tooling, an environment variable for the reason
    // KENTOS_FRAME_DUMP is one.
    if (const QByteArray typed = qgetenv("KENTOS_PROBE_LINE"); !typed.isEmpty()) {
        const QString line = QString::fromUtf8(typed);
        QTimer::singleShot(0, &window, [&window, line] { window.runScriptLine(line); });
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
    // ONE SETTINGS PAGE, PHOTOGRAPHED. `KENTOS_SMOKE` proves every window opens
    // and says nothing about what is ON one — which is how a page shipped
    // carrying two switches and none of the block that was the point of it. The
    // variable names a directory; `KENTOS_SETTINGS_PAGE` names the page, and the
    // picture lands as `ayarlar.png`. Developer tooling, same category as
    // `KENTOS_FRAME_DUMP`.
    // ONE LAYOUT, RENDERED TO A PNG. The layout renderer is the one piece of this
    // subsystem whose output cannot be asserted in a unit test — what matters is
    // whether the sheet LOOKS like a pafta — so it gets the same treatment the
    // canvas and the component sheet get: a probe that draws it and leaves a
    // picture a person can look at.
    if (qEnvironmentVariableIsSet("KENTOS_LAYOUT_SHOT")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            const QString dir                   = qEnvironmentVariable("KENTOS_LAYOUT_SHOT");
            kentos::app::Controller* controller = window.controller();
            window.seedProbeDrawing();

            const QString sheet =
                qEnvironmentVariable("KENTOS_LAYOUT_NAME", QStringLiteral("Deneme Paftası"));
            controller->runLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=\"%1\" kagit=A3 yon=yatay").arg(sheet),
                kentos::command::Origin::Gui);

            const kentos::core::Layout* layout =
                controller->document().layouts().find(sheet.toStdString());
            if (layout == nullptr) {
                (void)std::fprintf(stdout, "[yerlesim] çıktı yerleşimi kurulamadı\n");
                QApplication::exit(1);
                return;
            }

            // A TABLE OF THE SEEDED PARCELS, so the picture shows the one item
            // whose content comes from the drawing's attributes rather than from
            // its geometry.
            controller->runLine(
                QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=\"%1\" tur=tablo ad=tablo").arg(sheet),
                kentos::command::Origin::Gui);
            controller->runLine(
                QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=\"%1\" ad=tablo "
                               "metin=\"Kadastro Parselleri\" x=250 y=35 genislik=155 "
                               "yukseklik=60 yazi=3 cerceve=evet")
                    .arg(sheet),
                kentos::command::Origin::Gui);

            // AIM THE MAP AT THE DRAWING — through the COMMAND, which is the
            // road the canvas's print frame takes too. A probe that wrote the
            // extent straight into the document would be proving a path no user
            // has (Article 1.2).
            const kentos::core::Box2 extent = controller->document().extent();
            // METRES ON THE LINE, not `Mm`. A command line carries ground
            // coordinates in the drawing's own unit and the parser turns them
            // into millimetres; writing the millimetres straight out made the
            // frame a thousand times too wide (`print_dialog.cpp` does the same
            // conversion for `YAZDIR pencere=`).
            const auto metres = [](kentos::core::Mm v) {
                return QString::number(static_cast<double>(v) / 1000.0, 'f', 3);
            };
            controller->runLine(QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=\"%1\" ad=harita "
                                               "pencere=%2,%3 pencere=%4,%5")
                                    .arg(sheet, metres(extent.min_x), metres(extent.min_y),
                                         metres(extent.max_x), metres(extent.max_y)),
                                kentos::command::Origin::Gui);

            const kentos::core::Layout* aimed =
                controller->document().layouts().find(sheet.toStdString());
            const kentos::core::LayoutPage& page = aimed->pages.front();

            // 150 dpi: big enough to judge line weights and text, small enough to
            // open in a viewer.
            constexpr double kDpi = 150.0;
            const int w_px        = static_cast<int>(page.w / 1000.0 * kDpi / 25.4);
            const int h_px        = static_cast<int>(page.h / 1000.0 * kDpi / 25.4);
            QImage out(w_px, h_px, QImage::Format_ARGB32_Premultiplied);
            out.fill(Qt::white);

            kentos::app::LayoutFacts facts;
            facts.sheet   = sheet;
            facts.project = QStringLiteral("deneme.pcad");
            facts.crs     = QString::fromStdString(controller->document().crs().id());
            facts.date    = QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy"));

            QPainter painter(&out);
            kentos::app::paint_layout_page(painter, QRectF(0, 0, w_px, h_px),
                                           controller->document(), *aimed, 0, kDpi, facts);
            painter.end();

            QDir().mkpath(dir);
            const bool saved = out.save(dir + QStringLiteral("/yerlesim.png"));
            for (const kentos::core::LayoutItem& item : aimed->items) {
                const kentos::core::Box2 win = kentos::core::map_window(item);
                (void)std::fprintf(
                    stdout,
                    "[yerlesim] öğe %-8s %s  kutu %d,%d %dx%d um  pencere %lld,%lld "
                    "%lld,%lld  olcek 1:%lld\n",
                    item.id.c_str(), kentos::core::layout_item_kind_id(item.kind), item.frame.x,
                    item.frame.y, item.frame.w, item.frame.h, static_cast<long long>(win.min_x),
                    static_cast<long long>(win.min_y), static_cast<long long>(win.max_x),
                    static_cast<long long>(win.max_y),
                    static_cast<long long>(kentos::core::map_scale(item)));
            }
            (void)std::fprintf(
                stdout, "[yerlesim] çizim kapsamı %lld,%lld %lld,%lld — %zu nesne\n",
                static_cast<long long>(extent.min_x), static_cast<long long>(extent.min_y),
                static_cast<long long>(extent.max_x), static_cast<long long>(extent.max_y),
                controller->document().live_entity_count());
            (void)std::fprintf(stdout, "[yerlesim] %s — %d×%d px, %zu öğe\n",
                               saved ? "kare: yerlesim.png" : "kare yazılamadı", w_px, h_px,
                               aimed->items.size());
            (void)std::fflush(stdout);
            QApplication::exit(saved ? 0 : 1);
        });
    }

    // THE LAYOUT DESIGNER, driven the way a hand drives it and then exported.
    //
    // WHAT IT PROVES that nothing else can: that a drag on the page becomes a
    // `ÇIKTIÖĞE` line, that the line reaches the document, that the window
    // redraws from the document afterwards, and that the sheet then prints. The
    // unit suite proves the model and the commands; this is the seam between
    // them and the mouse.
    if (qEnvironmentVariableIsSet("KENTOS_LAYOUT_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            const QString dir = qEnvironmentVariable("KENTOS_LAYOUT_PROBE");
            QDir().mkpath(dir);
            kentos::app::Controller* controller = window.controller();
            window.seedProbeDrawing();

            int failures     = 0;
            const auto check = [&failures](bool held, const char* what) {
                if (held) return;
                ++failures;
                (void)std::fprintf(stdout, "[tasarim] BASARISIZ — %s\n", what);
                (void)std::fflush(stdout);
            };

            controller->runLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=\"Ada 1284\" kagit=A3 yon=yatay"),
                kentos::command::Origin::Gui);
            check(controller->document().layouts().find("Ada 1284") != nullptr,
                  "çıktı yerleşimi kurulamadı");

            kentos::app::LayoutDesigner designer(*controller, QStringLiteral("Ada 1284"), &window);
            designer.applyTheme(window.themeMode());
            designer.show();
            QCoreApplication::processEvents();

            // AIM IT, the way the canvas frame does.
            designer.aimAt(controller->document().extent());
            const kentos::core::LayoutItem* map =
                controller->document().layouts().find("Ada 1284")->first_map();
            check(map != nullptr && !map->extent.empty(), "harita hedeflenmedi");

            for (const QString& line : designer.probeDrive()) {
                (void)std::fprintf(stdout, "[tasarim] %s\n", line.toUtf8().constData());
                (void)std::fflush(stdout);
            }

            const kentos::core::Layout* after = controller->document().layouts().find("Ada 1284");
            check(after != nullptr && after->items.size() == 5, "lejant eklenmedi");
            if (after == nullptr) {
                QApplication::exit(1);
                return;
            }
            if (const kentos::core::LayoutItem* title = after->find("baslik"); title != nullptr)
                check(title->text == "<yerlesim> — <olcek>", "başlık metni yazılmadı");

            // AND ONE Ctrl+Z UNDOES THE LAST GESTURE, which is the claim a
            // designer with its own edit path could not make.
            const std::size_t before = after->items.size();
            controller->runLine(QStringLiteral("GERİAL"), kentos::command::Origin::Gui);
            check(controller->document().layouts().find("Ada 1284")->items.size() == before - 1,
                  "GERİAL son jesti geri almadı");

            // THE MENU, WALKED. "Integrated into the main menu" means the entries
            // are there and connected, which a picture of a closed menu cannot
            // show.
            for (const QString& line : window.probeLayoutMenu()) {
                (void)std::fprintf(stdout, "[menu] %s\n", line.toUtf8().constData());
                (void)std::fflush(stdout);
            }
            check(window.probeLayoutMenu().contains(QStringLiteral("Çıktı Yerleşimi Yöneticisi…")),
                  "Çıktı Yerleşimi Yöneticisi menüde yok");
            check(window.probeLayoutMenu().contains(QStringLiteral("    Tasarımcıyı Aç")),
                  "yerleşimin alt menüsünde Tasarımcıyı Aç yok");

            // ---- AND THE TOOLBAR'S PRINT LIST, AFTER A ROUND TRIP -----------
            //
            // A REPORTED BUG: a layout saved into a project and read back showed
            // up under `Dosya ▸ Çıktı Yerleşimleri` and NOT in the list beside
            // the toolbar's print button. That list was rebuilt from three
            // places — startup, a print PROFILE changing, and the one menu entry
            // that makes a layout — and opening a project is none of them. It is
            // rebuilt on `aboutToShow` now, so the round trip is what this
            // checks: save, open, walk the menu.
            check(window.probePrintMenu().contains(QStringLiteral("Ada 1284")),
                  "yerleşim yazdırma listesinde yok");

            // A LAYOUT MADE BY A PATH THAT NEVER TOUCHES THIS MENU, which is
            // the shape of the bug: the command line, a script, a template and
            // MCP all reach the document without going near the toolbar.
            window.runScriptLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=\"Komuttan\" kagit=A4"));
            QCoreApplication::processEvents();
            check(window.probePrintMenu().contains(QStringLiteral("Komuttan")),
                  "komut satırından kurulan yerleşim yazdırma listesinde yok");

            // AND THE ROUND TRIP THE USER REPORTED: save it, open it again.
            const QString saved = dir + QStringLiteral("/yerlesimli.pcad");
            QFile::remove(saved);
            window.runScriptLine(QStringLiteral("FARKLIKAYDET \"%1\"").arg(saved));
            window.runScriptLine(QStringLiteral("AÇ \"%1\"").arg(saved));
            QCoreApplication::processEvents();
            for (const QString& line : window.probePrintMenu()) {
                (void)std::fprintf(stdout, "[yazdirma-listesi] %s\n", line.toUtf8().constData());
                (void)std::fflush(stdout);
            }
            check(window.probePrintMenu().contains(QStringLiteral("Ada 1284")),
                  "dosyadan okunan yerleşim yazdırma listesinde yok");

            const QString pdf = dir + QStringLiteral("/yerlesim.pdf");
            controller->runLine(
                QStringLiteral("YAZDIR yerlesim=\"Ada 1284\" dosya=\"%1\"").arg(pdf),
                kentos::command::Origin::Gui);
            const QFileInfo written(pdf);
            check(written.exists() && written.size() > 1000, "PDF yazılmadı");

            // THE MAP IS NOT A PHOTOGRAPH OF A MAP.
            //
            // It used to be: the frame was rendered into a `QImage` and blitted,
            // so a 1:1000 parcel boundary reached the PDF as pixels —
            // unmeasurable, unselectable, and resolution-bound — on a document a
            // licensed engineer signs. Only the file can refute that, so the file
            // is what is read: a layout of vector geometry carries no image at
            // all (TODOS L-12).
            const QByteArray sheet_bytes = [&pdf] {
                QFile f(pdf);
                return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            }();
            check(!sheet_bytes.contains("/Subtype /Image") &&
                      !sheet_bytes.contains("/Subtype/Image"),
                  "yerleşim PDF'i haritayı raster olarak taşıyor");

            // AND NOTHING WAS LEFT BESIDE IT. The sheet is written to a sibling
            // and moved into place; a `.yeni` still sitting there would mean a
            // publish that did not finish and nobody noticed (TODOS C-05).
            check(!QFileInfo::exists(pdf + QStringLiteral(".yeni")),
                  "yayımlanmamış geçici PDF kaldı");

            // A TABLE THAT RAN OUT OF BOX SAYS SO IN THE ANSWER, not only on the
            // paper. A client that exported the sheet and read "tamam" would file
            // a table believing it complete (TODOS L-08).
            window.runScriptLine(
                QStringLiteral("ÇIKTIÖĞE islem=ekle yerlesim=\"Ada 1284\" tur=tablo ad=dar"));
            window.runScriptLine(QStringLiteral(
                "ÇIKTIÖĞE islem=tasi yerlesim=\"Ada 1284\" ad=dar x=10 y=10 genislik=60 "
                "yukseklik=8"));
            window.runScriptLine(
                QStringLiteral("ÇIKTIÖĞE islem=ayarla yerlesim=\"Ada 1284\" ad=dar metin=PARSEL"));
            const QString narrow = dir + QStringLiteral("/dar.pdf");
            controller->runLine(
                QStringLiteral("YAZDIR yerlesim=\"Ada 1284\" dosya=\"%1\"").arg(narrow),
                kentos::command::Origin::Gui);
            QCoreApplication::processEvents();
            check(QFileInfo::exists(narrow), "dar tablolu PDF yazılmadı");

            // ---- AN ATLAS: ONE SHEET, ONE PAGE PER PARCEL -------------------
            //
            // The thing a cadastral office actually asks for. The claim is about
            // the FILE — five parcels have to produce five pages — so the file is
            // what is read (TODOS L-10).
            window.runScriptLine(QStringLiteral("KATMAN ad=ATLASPARSEL"));
            for (int i = 0; i < 5; ++i)
                window.runScriptLine(QStringLiteral("ALAN noktalar=%1,0 %2,0 %2,20 %1,20")
                                         .arg(i * 40)
                                         .arg(i * 40 + 20));
            window.runScriptLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=Askı kagit=A4 yon=dikey"));
            window.runScriptLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=atlas ad=Askı katman=ATLASPARSEL"));

            const QString atlas = dir + QStringLiteral("/atlas.pdf");
            QFile::remove(atlas);
            controller->runLine(QStringLiteral("YAZDIR yerlesim=Askı dosya=\"%1\"").arg(atlas),
                                kentos::command::Origin::Gui);
            QCoreApplication::processEvents();

            check(QFileInfo::exists(atlas), "atlas PDF yazılmadı");
            const auto page_count = [](const QString& file) {
                QFile f(file);
                if (!f.open(QIODevice::ReadOnly)) return qsizetype{0};
                const QByteArray bytes = f.readAll();
                qsizetype n            = 0;
                qsizetype at           = 0;
                while ((at = bytes.indexOf("/MediaBox", at)) >= 0) {
                    ++n;
                    at += 9;
                }
                return n;
            };
            const qsizetype atlas_pages_n = page_count(atlas);
            (void)std::fprintf(stdout, "[tasarim] atlas sayfa sayısı: %lld\n",
                               static_cast<long long>(atlas_pages_n));
            check(atlas_pages_n == 5, "atlas beş parsel için beş sayfa yazmadı");
            (void)std::fprintf(stdout, "[tasarim] pdf rasterı yok, %lld bayt\n",
                               static_cast<long long>(written.size()));
            (void)std::fprintf(stdout, "[tasarim] pdf %lld bayt\n",
                               static_cast<long long>(written.size()));

            const bool shot = designer.grab().save(dir + QStringLiteral("/tasarimci.png"));
            (void)std::fprintf(stdout, "[tasarim] %s — %s\n", failures == 0 ? "TAMAM" : "BASARISIZ",
                               shot ? "kare: tasarimci.png" : "kare yazılamadı");
            (void)std::fflush(stdout);
            QApplication::exit(failures == 0 ? 0 : 1);
        });
    }

    // THE MODEL PROFILE WINDOW, photographed on its own. Same category as the
    // settings shot beside it: it is the window this change is about, and a
    // window nothing looks at is a window nobody checked.
    if (qEnvironmentVariableIsSet("KENTOS_PROVIDER_SHOT")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            const QString dir = qEnvironmentVariable("KENTOS_PROVIDER_SHOT");
            kentos::app::ProviderDialog dialog(
                *window.controller(), qEnvironmentVariable("KENTOS_PROVIDER_EDIT"), &window);
            dialog.applyTheme(window.themeMode());
            dialog.show();
            QCoreApplication::processEvents();
            // WHAT THE MODEL CHOOSER IS ACTUALLY OFFERING, printed: the defect
            // this window was built for was a model list that stayed on one
            // vendor's names whatever endpoint was chosen, and a screenshot of
            // a closed combo cannot show that.
            for (const QString& line : dialog.probeModels())
                (void)std::fprintf(stdout, "[profil] model %s\n", line.toUtf8().constData());
            QDir().mkpath(dir);
            const bool saved = dialog.grab().save(dir + QStringLiteral("/model-profili.png"));
            (void)std::fprintf(stdout, "[profil] %s\n",
                               saved ? "kare: model-profili.png" : "kare yazılamadı");
            (void)std::fflush(stdout);
            QApplication::exit(saved ? 0 : 1);
        });
    }

    if (qEnvironmentVariableIsSet("KENTOS_SETTINGS_SHOT")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            const QString dir = qEnvironmentVariable("KENTOS_SETTINGS_SHOT");
            const QString page =
                qEnvironmentVariable("KENTOS_SETTINGS_PAGE", QStringLiteral("MCP Sunucusu"));
            window.openSettingsSection(page);
            QCoreApplication::processEvents();
            QDir().mkpath(dir);
            int rc = 1;
            // FOUND AS A CHILD, not as the active window: the settings window is
            // MODELESS, so on a fresh start the platform has not given it focus
            // yet and `activeWindow()` is still the shell — which grabs the wrong
            // picture, or none at all.
            if (QWidget* top = window.findChild<kentos::app::SettingsDialog*>(); top != nullptr) {
                rc = top->grab().save(dir + QStringLiteral("/ayarlar.png")) ? 0 : 1;
                (void)std::fprintf(stdout, "[ayarlar] %s — %s\n",
                                   rc == 0 ? "kare: ayarlar.png" : "kare yazılamadı",
                                   page.toUtf8().constData());
                (void)std::fflush(stdout);
            }
            QApplication::exit(rc);
        });
    }

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
        // THE LAYOUT WINDOWS, which are windows like any other and so belong in
        // the smoke test: a dialog that crashes on construction passed
        // `shell-starts` twice before this test existed.
        later([&window] {
            window.controller()->runLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=\"Duman\" kagit=A4"),
                kentos::command::Origin::Gui);
            window.openLayoutManager();
        });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });
        later([&window] { window.openLayoutDesigner(QStringLiteral("Duman")); });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });

        later([&window] { window.openCommandSearch(); });
        later([] {
            if (QWidget* top = QApplication::activePopupWidget()) top->close();
        });
        later([&window] { window.openImportWizard(); });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });
        // THE PRINT PREVIEW, constructed like every other window: it renders a
        // sheet on construction, and a sheet renderer that crashes on an empty
        // drawing is exactly what this test is for.
        later([&window] { window.openPrintDialog(kentos::core::Box2{0, 0, 40000, 30000}); });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });
        later([&window] {
            if (sheet_refused) {
                (void)std::fprintf(stderr, "[kentos] duman testi: stil sayfası REDDEDİLDİ\n");
                QApplication::exit(2);
                return;
            }
#if KENTOS_HAVE_RHI
            // THE CANVAS HAS ITS GPU CONTEXT. A `QRhiWidget` whose window was
            // created before it existed paints nothing and says so only on
            // stderr (see `AA_DontUseNativeMenuBar` above); a shell test that
            // opened every window and never noticed the drawing was missing
            // is the test this line makes honest. Not under a platform that
            // has no graphics API at all — `offscreen`, which ctest uses, says
            // "QRhi is not supported on this platform" for every QRhiWidget —
            // because there the absence proves nothing about the window.
            const QString platform = QGuiApplication::platformName();
            const bool can_have_rhi =
                platform != QLatin1String("offscreen") && platform != QLatin1String("minimal");
            if (can_have_rhi && window.canvas() != nullptr && !window.canvas()->hasGpuContext()) {
                (void)std::fprintf(stderr, "[kentos] duman testi: tuval GPU bağlamı ALAMADI "
                                           "(QRhiWidget: No QRhi) — çizim boş kalır\n");
                QApplication::exit(2);
                return;
            }
#endif
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
        later([&window] { window.openPrintDialog(kentos::core::Box2{0, 0, 40000, 30000}); });
        later([shot] { shot(QStringLiteral("2b-yazdir"), QApplication::activeModalWidget()); });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
        });
        // AND THE FRAME ON THE CANVAS, which is the half of printing that
        // happens before the window: the sheet-shaped hole the user aims with.
        later([&window] { window.printWithProfile(); });
        later([&window, shot] { shot(QStringLiteral("2c-yazdirma-alani"), &window); });
        later([&window] { window.canvas()->endPrintFrame(); });
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
        // The plot page, because the print profiles live on it and a table
        // nobody has photographed is a table nobody has looked at.
        later([&window] { window.openSettingsSection(QStringLiteral("Plot ve Çıktı")); });
        later([shot] { shot(QStringLiteral("4b-plot-ve-cikti"), QApplication::activeWindow()); });
        later([] {
            if (QWidget* top = QApplication::activeWindow()) top->close();
        });

        later([&window] { window.openCommandSearch(); });
        later([shot] { shot(QStringLiteral("5-komut-arama"), QApplication::activePopupWidget()); });
        later([] {
            if (QWidget* top = QApplication::activePopupWidget()) top->close();
        });

        // The import wizard, both pages. `KENTOS_IMPORT_SAMPLE` names a file to
        // read; without it only the file page can be photographed, because there
        // is nothing to list on the second one.
        static kentos::app::ImportWizard* wizard = nullptr;
        later([&window] { wizard = window.openImportWizard(); });
        later([] { wizard->setPath(QString::fromLocal8Bit(qgetenv("KENTOS_IMPORT_SAMPLE"))); });
        later([shot] {
            shot(QStringLiteral("6-ice-aktarma-dosya"), QApplication::activeModalWidget());
        });
        // The read only starts HERE, so the page above is photographed before the
        // wizard leaves it. Two extra beats after: the read runs on its own
        // thread and the layer page does not exist until it has finished.
        later([] {
            if (wizard != nullptr)
                wizard->beginWith(QString::fromLocal8Bit(qgetenv("KENTOS_IMPORT_SAMPLE")));
        });
        later([] {});
        later([] {});
        later([shot] {
            shot(QStringLiteral("7-ice-aktarma-katmanlar"), QApplication::activeModalWidget());
        });
        later([] {
            if (QWidget* top = QApplication::activeModalWidget()) top->close();
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

            // ZOOM BEFORE TIMING, when asked. `--betik` ends by fitting the
            // drawing to the canvas, so a zoom inside the script is overwritten
            // before a single frame is measured — which made four runs at four
            // magnifications report the same vertex count to the digit. A
            // symbology cost that depends on magnification cannot be found at one
            // magnification.
            if (const QByteArray zoom = qgetenv("KENTOS_ZOOM"); !zoom.isEmpty()) {
                bool ok            = false;
                const double times = QString::fromLocal8Bit(zoom).toDouble(&ok);
                if (ok && times > 0.0) canvas->zoomBy(times);
            }

            std::vector<int> costs = canvas->timeFrames(n);
            std::vector<int> scene = canvas->sceneCosts();
            std::sort(costs.begin(), costs.end());
            std::sort(scene.begin(), scene.end());
            // The MEDIAN and the best, not the mean: a headless run shares the
            // machine and one descheduled frame drags an average anywhere.
            // THE DRAW-CALL COUNT COMES WITH THE TIME (render.md R7). A batching
            // regression does not show up in a screenshot and shows up in the
            // frame time long after it is cheap to find; printing the two
            // together is what lets the < 100 budget be measured on the same run
            // that measures the 16 ms one.
            const kentos::render::FrameStats fs = canvas->frameStats();
            // THE SCALE COMES WITH THE NUMBERS. A frame cost without the
            // magnification it was measured at cannot be compared with another
            // one, and a probe that silently failed to zoom looks exactly like a
            // cost that does not depend on zoom.
            (void)std::fprintf(stdout,
                               "[kentos] %d kare  cizim ortanca %d us  en iyi %d us"
                               "  |  sahne ortanca %d us  |  cizim cagrisi %u"
                               "  gecis %u  kose %u  |  1:%.0f\n",
                               n, costs[costs.size() / 2], costs.front(),
                               scene.empty() ? 0 : scene[scene.size() / 2], fs.draw_calls,
                               fs.passes, fs.vertices, canvas->view().scale_denominator());
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
    // THE §10.1 AND R7 BUDGETS, ASSERTED. `KENTOS_FRAME_TIMES` measures and
    // prints; this one measures and FAILS, which is what a gate needs.
    //
    // It lives here rather than in `/tests/bench` because of Article 3.4, not
    // because of taste: `/tests` links no Qt and every backend is Qt by
    // definition, so the bench binary has nowhere to construct one and could only
    // ever measure the scene builder. The budgets are about what reaches the
    // screen. `scripts/ci-gate-butce.sh` drives this.
    if (const QByteArray rounds = qgetenv("KENTOS_BUDGET_PROBE"); !rounds.isEmpty()) {
        const int n = std::max(1, rounds.toInt());
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, n] {
            kentos::app::MapCanvas* canvas = window.canvas();
            if (canvas == nullptr) {
                (void)std::fprintf(stderr, "[butce] tuval yok\n");
                QApplication::exit(2);
                return;
            }

            std::vector<int> costs = canvas->timeFrames(n);
            std::sort(costs.begin(), costs.end());
            const int median = costs[costs.size() / 2];

            const kentos::render::FrameStats fs = canvas->frameStats();

            // The two numbers `render.md` names: R13's 16 ms and R7's hundred.
            // The draw-call budget is asserted only where it MEANS something —
            // `FrameStats::draw_calls` is zero on a backend that is not on the
            // GPU, and zero is not "fewer than a hundred", it is "not measured".
            constexpr int kFrameBudgetUs            = 16000;
            constexpr std::uint32_t kDrawCallBudget = 100;

            // A THIRD BUDGET, and it exists because the other two did not catch
            // the defect they were standing next to. This scene styles seven
            // layers with pattern fills and drew 188 144 vertices a frame for a
            // picture that needs 8 432: every glyph of every stamp was expanded
            // on the CPU and re-uploaded, every frame. It stayed inside 16 ms on
            // a fast machine and inside a hundred draw calls, so both gates were
            // green while zooming a styled plan had become unusable.
            //
            // Vertices are the honest unit for that failure: it is work that
            // scales with the number of stamps, and a machine slower than this
            // one is where it stops fitting in 16 ms. Twenty-five thousand is
            // three times what this scene needs and a twentieth of what it used
            // to ask for — loose enough not to fire on a legitimate change,
            // tight enough that per-stamp expansion cannot come back.
            constexpr std::uint32_t kVertexBudget = 25000;

            int kusur = 0;
            if (median > kFrameBudgetUs) {
                (void)std::fprintf(stderr, "[butce] kare ortancasi %d us — 10.1 butcesi %d us\n",
                                   median, kFrameBudgetUs);
                kusur = 1;
            }
            const bool gpu = canvas->backendIsGpu();
            if (gpu && fs.draw_calls >= kDrawCallBudget) {
                (void)std::fprintf(stderr, "[butce] cizim cagrisi %u — R7 butcesi %u\n",
                                   fs.draw_calls, kDrawCallBudget);
                kusur = 1;
            }
            if (gpu && fs.vertices >= kVertexBudget) {
                (void)std::fprintf(stderr,
                                   "[butce] kose %u — butce %u. Bir sembol katmani damga basina "
                                   "geometri uretiyor olabilir; KENTOS_FRAME_PARTS=1 hangisi "
                                   "oldugunu soyler.\n",
                                   fs.vertices, kVertexBudget);
                kusur = 1;
            }

            (void)std::fprintf(stdout,
                               "[butce] %s — kare ortancasi %d us (butce %d)"
                               "  ·  cizim cagrisi %u (butce %u%s)  ·  kose %u (butce %u)\n",
                               kusur != 0 ? "ASILDI" : "TAMAM", median, kFrameBudgetUs,
                               fs.draw_calls, kDrawCallBudget, gpu ? "" : ", GPU degil: olculmedi",
                               fs.vertices, kVertexBudget);
            QApplication::exit(kusur);
        });
    }

    // Presses every button on the tool column and prints what came back. Same
    // category as KENTOS_EDIT_PROBE below: developer tooling, not a feature.
    // THE CHAT DOCK, driven by a recorded provider stream. Same category as the
    // probes around it: developer tooling, not a feature. It is what proves the
    // seam no unit test can reach — a decoded tool call becoming a bubble, a
    // card and an undo entry in a running shell.
    if (qEnvironmentVariableIsSet("KENTOS_CHAT_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window,
                           [&window] { QApplication::exit(window.probeChat()); });
    }

    if (qEnvironmentVariableIsSet("KENTOS_TOOL_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeToolBox();
            QApplication::exit(0);
        });
    }

    // The layer panel, clicked rather than called. Same category as the two
    // probes around it: developer tooling, an environment variable, no /docs page.
    if (qEnvironmentVariableIsSet("KENTOS_LAYER_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeLayerPanel();
            QApplication::exit(0);
        });
    }

    // The pick chooser, opened by a real click rather than by a call. Same
    // category as the probes around it: developer tooling, an environment
    // variable, no /docs page.
    if (qEnvironmentVariableIsSet("KENTOS_PICK_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probePickList();
            QApplication::exit(0);
        });
    }

    // The surface-normal lock, engaged by HOLDING A KEY on a real canvas. The
    // engine half has a unit test; this is the half only a screen can answer.
    if (qEnvironmentVariableIsSet("KENTOS_NORMAL_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeSurfaceNormal();
            QApplication::exit(0);
        });
    }

    // The tool family flyout, opened from the button rather than by a call.
    if (qEnvironmentVariableIsSet("KENTOS_FAMILY_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeToolFamily();
            QApplication::exit(0);
        });
    }

    // The attribute schema page of Katman Özellikleri, driven through its own
    // dialog lines.
    if (qEnvironmentVariableIsSet("KENTOS_SCHEMA_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeSchemaPage();
            QApplication::exit(0);
        });
    }

    // The attribute grid: the edit-mode gate, our editors, and Enter walking the
    // row the way a ledger is filled in.
    if (qEnvironmentVariableIsSet("KENTOS_TABLE_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeAttributeGrid();
            QApplication::exit(0);
        });
    }

    // The living component standard: every control in every state, its
    // inventory printed for the gate and its picture saved for the manual.
    if (qEnvironmentVariableIsSet("KENTOS_WIDGETS_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeWidgets();
            QApplication::exit(0);
        });
    }

    // Every window, photographed into a directory, for looking at.
    if (qEnvironmentVariableIsSet("KENTOS_DIALOG_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeDialogs();
            QApplication::exit(0);
        });
    }

    // The layer properties window: classify a layer by a column and apply it.
    if (qEnvironmentVariableIsSet("KENTOS_DESIGNER_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            window.probeDesigner();
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

    // THE PLOT, ASSERTED. `/tests` links no Qt and a PDF is written by Qt, so
    // the only place this can be proved is here (the reason KENTOS_EDIT_PROBE
    // lives here too). Draws a parcel, prints it to a PDF on the named profile,
    // and checks what came out: one page, the sheet's own size in points, and
    // — with a password — an encrypted file. Exits non-zero on any of it.
    // Developer tooling and an environment variable, not a feature.
    if (const QByteArray into = qgetenv("KENTOS_PRINT_PROBE"); !into.isEmpty()) {
        const QString dir = QString::fromLocal8Bit(into);
        QDir().mkpath(dir);
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window, dir] {
            int failures     = 0;
            const auto check = [&failures](bool ok, const char* what) {
                if (ok) return;
                ++failures;
                (void)std::fprintf(stderr, "[kentos] BAŞARISIZ: %s\n", what);
            };

            window.runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
            window.runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));
            window.runScriptLine(QStringLiteral(
                "YAZDIRMAPROFİLİ islem=ekle ad=\"Sınama A3\" kagit=A3 yon=yatay dpi=150 kenar=5"));

            const QString plain  = dir + QStringLiteral("/duz.pdf");
            const QString sealed = dir + QStringLiteral("/sifreli.pdf");

            // REMOVED FIRST, and this is not tidiness. Both files are checked by
            // existence and by content, so a leftover from an earlier run makes
            // every one of those checks pass without a single byte being written
            // this time — which is exactly what happened: the probe went green
            // over a stale encrypted PDF while the command it was testing was
            // refusing an argument that no longer exists.
            QFile::remove(plain);
            QFile::remove(sealed);
            window.runScriptLine(
                QStringLiteral("YAZDIR pencere=-5,-5 pencere=45,35 profil=\"Sınama A3\" "
                               "dosya=\"%1\" baslik=\"Sınama paftası\" yazar=KentOSCad")
                    .arg(plain));
            window.runScriptLine(
                QStringLiteral("YAZDIR pencere=-5,-5 pencere=45,35 profil=\"Sınama A3\" "
                               "dosya=\"%1\" sifre=gizli kopyalanabilir=hayır")
                    .arg(sealed));
            QCoreApplication::processEvents();

            // A3 landscape is 420×297 mm, which is 1190.55×841.89 PostScript
            // points. The MediaBox is what a plotter reads, so that is what is
            // checked rather than the file's size on disk.
            const auto media_box = [](const QString& path) {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) return QSizeF();
                const QByteArray bytes = file.readAll();
                // `qsizetype` rather than `int`: a byte offset into a file is
                // 64-bit and narrowing it is a warning this tree does not carry.
                const qsizetype at = bytes.indexOf("/MediaBox");
                if (at < 0) return QSizeF();
                const qsizetype open  = bytes.indexOf('[', at);
                const qsizetype close = bytes.indexOf(']', open);
                if (open < 0 || close < 0) return QSizeF();
                const QList<QByteArray> parts =
                    bytes.mid(open + 1, close - open - 1).simplified().split(' ');
                if (parts.size() < 4) return QSizeF();
                return QSizeF(parts[2].toDouble(), parts[3].toDouble());
            };

            check(QFileInfo::exists(plain), "PDF yazılmadı");
            const QSizeF box = media_box(plain);
            check(std::abs(box.width() - 1190.55) < 2.0 && std::abs(box.height() - 841.89) < 2.0,
                  "PDF sayfa ölçüsü A3 yatay değil");
            const QByteArray plain_bytes = [&plain] {
                QFile f(plain);
                return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            }();
            check(plain_bytes.contains("/Author (KentOSCad)"), "PDF yazar alanı yazılmadı");
            check(!plain_bytes.contains("/Encrypt"), "şifresiz PDF şifreli çıktı");

            check(QFileInfo::exists(sealed), "şifreli PDF yazılmadı");
            const QByteArray sealed_bytes = [&sealed] {
                QFile f(sealed);
                return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            }();
            check(sealed_bytes.contains("/Encrypt"), "şifreli PDF şifrelenmedi");

            // ---- A LAYOUT WHOSE PAGES ARE NOT THE SAME SIZE ----------------
            //
            // The device page size used to be taken once from `pages.front()`
            // and never set again, so every page of a mixed A4/A3 layout was
            // written at A4: the second page's content was drawn at A3
            // dimensions into an A4 MediaBox and ran off the paper. Only the
            // file can refute that, so the file is what is read.
            const QString mixed = dir + QStringLiteral("/karma.pdf");
            QFile::remove(mixed);
            window.runScriptLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=ekle ad=Karma kagit=A4 yon=dikey"));
            window.runScriptLine(
                QStringLiteral("ÇIKTIYERLEŞİMİ islem=sayfaekle ad=Karma kagit=A3 yon=yatay"));
            window.runScriptLine(QStringLiteral("YAZDIR yerlesim=Karma dosya=\"%1\"").arg(mixed));
            QCoreApplication::processEvents();

            const auto every_media_box = [](const QString& path) {
                QList<QSizeF> out;
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) return out;
                const QByteArray bytes = file.readAll();
                qsizetype at           = 0;
                while ((at = bytes.indexOf("/MediaBox", at)) >= 0) {
                    const qsizetype open  = bytes.indexOf('[', at);
                    const qsizetype close = bytes.indexOf(']', open);
                    at                    = open < 0 ? bytes.size() : open + 1;
                    if (open < 0 || close < 0) break;
                    const QList<QByteArray> parts =
                        bytes.mid(open + 1, close - open - 1).simplified().split(' ');
                    if (parts.size() >= 4)
                        out.append(QSizeF(parts[2].toDouble(), parts[3].toDouble()));
                }
                return out;
            };

            check(QFileInfo::exists(mixed), "karma sayfalı PDF yazılmadı");
            const QList<QSizeF> boxes = every_media_box(mixed);
            check(boxes.size() == 2, "karma PDF iki sayfa yazmadı");
            if (boxes.size() == 2) {
                // A4 portrait is 595×842 pt, A3 landscape 1191×842.
                check(std::abs(boxes[0].width() - 595.0) < 2.0, "1. sayfa A4 dikey değil");
                check(std::abs(boxes[1].width() - 1191.0) < 2.0, "2. sayfa A3 yatay değil");
            }
            (void)std::fprintf(stdout, "[yazdirma] karma sayfa kutuları: %s\n",
                               [&boxes] {
                                   QStringList said;
                                   for (const QSizeF& one : boxes)
                                       said << QStringLiteral("%1x%2")
                                                   .arg(one.width(), 0, 'f', 0)
                                                   .arg(one.height(), 0, 'f', 0);
                                   return said.join(QStringLiteral(", ")).toUtf8();
                               }()
                                   .constData());

            // ---- THE FRAME, DRIVEN BY THE MOUSE ----------------------------
            //
            // A REGRESSION TEST FOR A REPORTED BUG: the frame pans with the LEFT
            // button, and the release handler only cleared `panning_` for the
            // MIDDLE one — so taking hold of the map to aim it meant the map
            // followed the mouse for ever after. Nothing in a unit test can see
            // that; it needs real press, move and release events on the widget.
            kentos::app::MapCanvas* canvas = window.canvas();
            check(canvas != nullptr, "tuval yok");
            if (canvas != nullptr) {
                const auto send = [canvas](QEvent::Type type, const QPointF& at,
                                           Qt::MouseButton button, Qt::MouseButtons held) {
                    QMouseEvent event(type, at, canvas->mapToGlobal(at), button, held,
                                      Qt::NoModifier);
                    QCoreApplication::sendEvent(canvas, &event);
                    QCoreApplication::processEvents();
                };

                window.printWithProfile();
                check(canvas->printFraming(), "yazdırma çerçevesi açılmadı");
                const kentos::core::Box2 framed = canvas->printFrameWindow();
                check(!framed.empty(), "çerçeve boş bir pencere verdi");

                const QPointF from(canvas->width() / 2.0, canvas->height() / 2.0);
                send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseMove, from + QPointF(60, 40), Qt::NoButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, from + QPointF(60, 40), Qt::LeftButton,
                     Qt::NoButton);
                const kentos::core::Point2 settled = canvas->view().centre();

                // THE BUTTON IS UP: moving the mouse must move nothing.
                send(QEvent::MouseMove, from + QPointF(200, 150), Qt::NoButton, Qt::NoButton);
                check(canvas->view().centre() == settled,
                      "düğme bırakıldıktan sonra harita fareyi izlemeye devam etti");
                check(canvas->printFraming(), "çerçeve sürükleme sonunda kapandı");

                // And Esc puts it away.
                QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &esc);
                QCoreApplication::processEvents();
                check(!canvas->printFraming(), "Esc çerçeveyi kapatmadı");

                // ---- WHAT THE FRAME HELD IS WHAT THE SHEET GETS ------------
                //
                // REPORTED BY HAND: the preview covered more ground than the
                // frame. The window took the frame's scale, rounded it UP to the
                // nearest plan scale and printed THAT — 1:184 became 1:200, so
                // the sheet held a fifth more than had been aimed at. The line
                // the preview would send is where the frame and the sheet meet,
                // so the line is what is read here: the frame's own corners, no
                // scale, and a box that still has the frame's centre and size.
                const QString aimed = window.probePrintLine(
                    framed, QString(), dir + QStringLiteral("/onizleme.pdf"), false);
                check(aimed.contains(QStringLiteral("pencere=")),
                      "önizleme çerçevenin penceresini göndermiyor");
                check(!aimed.contains(QStringLiteral("olcek=")),
                      "önizleme çerçeveyi kendi başına bir ölçeğe çevirdi");

                // The two corners back out of the line, in millimetres.
                const auto corners = [](const QString& line) {
                    QList<double> out;
                    for (const QString& part : line.split(QLatin1Char(' ')))
                        if (part.startsWith(QStringLiteral("pencere=")))
                            for (const QString& number : part.mid(8).split(QLatin1Char(',')))
                                out << number.toDouble() * 1000.0;
                    return out;
                }(aimed);
                check(corners.size() == 4, "gönderilen satırda iki köşe yok");
                if (corners.size() == 4) {
                    const double sent_w  = corners[2] - corners[0];
                    const double sent_h  = corners[3] - corners[1];
                    const auto framed_w  = static_cast<double>(framed.max_x - framed.min_x);
                    const auto framed_h  = static_cast<double>(framed.max_y - framed.min_y);
                    const double sent_cx = (corners[0] + corners[2]) / 2.0;
                    const double sent_cy = (corners[1] + corners[3]) / 2.0;
                    // A PERCENT, because fitting the box to the paper's aspect
                    // moves one edge by the hair the frame's pixel size rounds
                    // to — and the bug this guards against was a fifth.
                    check(std::abs(sent_w - framed_w) < framed_w * 0.01 &&
                              std::abs(sent_h - framed_h) < framed_h * 0.01,
                          "kâğıda giden alan çerçevenin alanı değil");
                    check(std::abs(sent_cx -
                                   static_cast<double>(framed.min_x + framed.max_x) / 2.0) < 2.0 &&
                              std::abs(sent_cy - static_cast<double>(framed.min_y + framed.max_y) /
                                                     2.0) < 2.0,
                          "kâğıda giden alan çerçevenin merkezinde değil");
                }

                // AND THE ROUND FIGURE IS A PRESS: the button turns the same
                // frame into a plan scale, and then the line says so.
                const QString rounded = window.probePrintLine(
                    framed, QString(), dir + QStringLiteral("/onizleme.pdf"), true);
                check(rounded.contains(QStringLiteral("olcek=")),
                      "yuvarlama düğmesi ölçeğe çevirmedi");
                check(!rounded.contains(QStringLiteral("pencere=")),
                      "yuvarlamadan sonra satır hem pencere hem ölçek taşıyor");
            }

            (void)std::fprintf(stdout, "[yazdir] %s — sayfa %.0f×%.0f pt, sifreli %s\n",
                               failures == 0 ? "TAMAM" : "BASARISIZ", box.width(), box.height(),
                               sealed_bytes.contains("/Encrypt") ? "evet" : "hayir");
            QApplication::exit(failures == 0 ? 0 : 1);
        });
    }

#if KENTOS_HAVE_MCP
    // ---- THE AGENT SERVER, OVER A REAL SOCKET --------------------------------
    //
    // The protocol itself is proved by 23 Qt-free cases over `ai::McpServer`
    // (tests/unit/test_ai_mcp.cpp): a socket cannot tell you whether a header
    // rule is right. What a socket CAN tell you is whether the listener, the
    // routing, the Qt request conversion and the response writing are wired
    // together at all — which is what this one case is for (.claude/test.md R24).
    //
    // AND IT PROVES THE CONSTITUTIONAL CLAIM END TO END: a write tool called
    // over HTTP changes nothing until a person decides. That is CLAUDE.md 5.7 and
    // .claude/ai.md R3/P1, and it is the sentence a reviewer will most want to
    // see demonstrated rather than asserted.
    if (qEnvironmentVariableIsSet("KENTOS_MCP_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            int failures     = 0;
            const auto check = [&failures](bool ok, const char* what) {
                if (ok) return;
                ++failures;
                (void)std::fprintf(stderr, "[kentos] BAŞARISIZ: %s\n", what);
            };

            kentos::app::Controller* controller = window.controller();
            check(controller != nullptr, "denetleyici yok");
            if (controller == nullptr) {
                QApplication::exit(1);
                return;
            }
            auto* server = controller->mcpService();
            check(server != nullptr, "MCP servisi yok");
            if (server == nullptr) {
                QApplication::exit(1);
                return;
            }

            window.runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
            window.runScriptLine(QStringLiteral("ALAN 0,0 40,0 40,30 0,30"));

            // An ephemeral-ish port well above the privileged range; the
            // configured default is left alone so a developer's own listener is
            // not disturbed.
            auto started = server->start(18765);
            check(started.ok(), "sunucu başlamadı");
            if (!started.ok()) {
                (void)std::fprintf(stderr, "[kentos]   %s\n", started.error().message.c_str());
                QApplication::exit(1);
                return;
            }
            check(server->listening(), "sunucu dinlemiyor");

            const QString base = QStringLiteral("http://127.0.0.1:%1/mcp/%2")
                                     .arg(server->port())
                                     .arg(server->token());

            QNetworkAccessManager net;
            // ONE REQUEST, SYNCHRONOUSLY, and the nested loop is what makes it
            // readable: this is a test, not the program's own client, and the
            // program's own client (the chat) is asynchronous exactly as ai.md
            // R18 demands.
            const auto post = [&net](const QString& url, const QByteArray& body,
                                     const QByteArray& method, const QByteArray& name,
                                     int* status) {
                // Braces, not parentheses: `QNetworkRequest request(QUrl(url))`
                // declares a FUNCTION taking a QUrl (the most vexing parse), and
                // the errors that follow talk about member access on a function
                // type rather than about the line that is wrong.
                QNetworkRequest request{QUrl(url)};
                request.setRawHeader("Content-Type", "application/json");
                request.setRawHeader("Accept", "application/json, text/event-stream");
                request.setRawHeader("MCP-Protocol-Version", "2026-07-28");
                request.setRawHeader("Mcp-Method", method);
                if (!name.isEmpty()) request.setRawHeader("Mcp-Name", name);
                QNetworkReply* reply = net.post(request, body);
                QEventLoop loop;
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
                if (status != nullptr)
                    *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray answer = reply->readAll();
                reply->deleteLater();
                return answer;
            };

            // Every answer is printed while the probe is being read by a person:
            // a status and the first line of a body turn "it failed" into "it
            // failed with -32602 because the tool name was wrong".
            const auto say = [](const char* what, int code, const QByteArray& body) {
                (void)std::fprintf(stdout, "[mcp] %-22s %3d  %s\n", what, code,
                                   body.left(220).constData());
                // FLUSHED, because the first run of this probe crashed and took
                // every buffered diagnostic with it — leaving two failures and
                // no evidence.
                (void)std::fflush(stdout);
            };

            const auto envelope = [](const char* method, const QByteArray& params) {
                return QByteArray("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"") + method +
                       "\",\"params\":" + params + "}";
            };
            const QByteArray meta =
                "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\","
                "\"io.modelcontextprotocol/clientInfo\":{\"name\":\"kentos-probe\","
                "\"version\":\"1\"}}";

            // 1. DISCOVERY, which is mandatory in this revision.
            int status                  = 0;
            const QByteArray discovered = post(base, envelope("server/discover", "{" + meta + "}"),
                                               "server/discover", {}, &status);
            say("server/discover", status, discovered);
            check(status == 200, "server/discover 200 vermedi");
            check(discovered.contains("2026-07-28"), "server/discover sürümü bildirmedi");

            // 2. THE CATALOGUE, generated from the registry.
            const QByteArray listed =
                post(base, envelope("tools/list", "{" + meta + "}"), "tools/list", {}, &status);
            say("tools/list", status, listed);
            check(status == 200, "tools/list 200 vermedi");
            check(listed.contains("katmanlari_listele"), "okuma aracı katalogda yok");
            check(listed.contains("core_line"), "çizim aracı katalogda yok");

            // 3. A READ TOOL RUNS NOW and answers with data.
            const QByteArray read =
                post(base,
                     envelope("tools/call",
                              "{\"name\":\"katmanlari_listele\",\"arguments\":{}," + meta + "}"),
                     "tools/call", "katmanlari_listele", &status);
            say("read tool", status, read);
            check(status == 200, "okuma aracı 200 vermedi");
            check(read.contains("PARSEL"), "okuma aracı katmanı bildirmedi");

            // 4. A WRITE TOOL CHANGES NOTHING. It answers with a plan id and the
            //    command lines, and the drawing is untouched until a person acts.
            const std::size_t before = controller->document().live_entity_count();
            const QByteArray wrote   = post(
                base,
                envelope("tools/call",
                           "{\"name\":\"core_layer\",\"arguments\":{\"ad\":\"AJAN\"}," + meta + "}"),
                "tools/call", "core_layer", &status);
            say("write tool", status, wrote);
            check(status == 200, "yazma aracı 200 vermedi");
            check(wrote.contains("oneri") || wrote.contains("öneri"),
                  "yazma aracı öneri kimliği döndürmedi");
            check(controller->document().find_layer("AJAN") == kentos::core::kNoLayer,
                  "YAZMA ARACI UYGULANDI — onay beklemesi gerekirdi");
            check(controller->document().live_entity_count() == before, "çizim değişti");
            check(controller->aiService().plans().pending().size() == 1,
                  "öneri defterinde bekleyen öneri yok");

            // 5. A COORDINATE LITERAL IS REFUSED where a handle is declared.
            const QByteArray literal = post(
                base,
                envelope("tools/call",
                         "{\"name\":\"core_line\",\"arguments\":{\"noktalar\":[[0,0],[1000,0]]}," +
                             meta + "}"),
                "tools/call", "core_line", &status);
            say("coordinate literal", status, literal);
            check(literal.contains("isError") || literal.contains("tutamak"),
                  "koordinat literali reddedilmedi");

            // 6. AND THE PERSON'S DECISION IS WHAT APPLIES IT — THROUGH THE CARD.
            //    The probe builds the same `SuggestionCard` the panel does and
            //    presses its `Uygula`, because that widget is the only thing in
            //    the program that can mint an `ai::Approval` (ai.md P15). A
            //    probe that called a service method instead would be proving a
            //    path no user has.
            const std::vector<const kentos::ai::Plan*> open =
                controller->aiService().plans().pending();
            if (!open.empty()) {
                const QString plan = QString::fromStdString(open.front()->id);
                kentos::app::SuggestionCard card(controller->aiService(), plan);
                check(card.pending(), "öneri kartı bekleyen öneriyi bulamadı");
                const auto decided = card.probeApply();
                check(decided.ok(), "onaylanan öneri uygulanamadı");
                check(!card.pending(), "karar verilen kart hâlâ bekliyor");
                check(controller->document().find_layer("AJAN") != kentos::core::kNoLayer,
                      "onaydan sonra katman yok");
            }

            // 7. THE HEADER RULES REACH THE SOCKET: a mismatch is 400.
            (void)post(base, envelope("tools/list", "{" + meta + "}"), "tools/call", {}, &status);
            check(status == 400, "başlık uyuşmazlığı 400 vermedi");

            // 8. AND A WRONG TOKEN IS REFUSED.
            const QString wrong = QStringLiteral("http://127.0.0.1:%1/mcp/%2")
                                      .arg(server->port())
                                      .arg(QStringLiteral("00000000000000000000000000000000"));
            (void)post(wrong, envelope("tools/list", "{" + meta + "}"), "tools/list", {}, &status);
            say("wrong token", status, QByteArray());
            check(status == 401 || status == 403 || status == 404, "yanlış belirteç kabul edildi");

            // 9. THE LEDGER SAW EVERY ONE OF THEM (TODOS M-08). Nothing here is
            //    a session — 2026-07-28 has none — so what a person is shown is
            //    who SPOKE and when, and this is where that is proved over a
            //    real socket rather than against a test double.
            const kentos::ai::ClientLedger& ledger = server->clients();
            check(ledger.size() >= 1, "istemci defteri boş");
            std::string probe_label;
            for (const kentos::ai::ClientRecord& one : ledger.clients())
                if (one.calls > 0) probe_label = one.label;
            check(!probe_label.empty(), "defterde çağrısı olan istemci yok");
            const kentos::ai::ClientRecord* row = ledger.find(probe_label);
            check(row != nullptr && row->calls >= 5, "çağrılar sayılmadı");
            check(row != nullptr && row->plans >= 1, "açılan öneri sayılmadı");
            check(row != nullptr && row->last_seen != 0, "son görülme zamanı yazılmadı");
            // THE TOKEN IS NOT IN THE LABEL, only its fingerprint (5.21). This
            // is the string that reaches the audit record and the settings
            // table, so it is checked where it is actually produced.
            check(probe_label.find(server->token().toStdString()) == std::string::npos,
                  "İSTEMCİ ADINDA BELİRTEÇ VAR");

            // 10. THE PROBE VERB WORKS OVER THE REAL SOCKET, which is the half
            //     the protocol engine cannot test about itself.
            auto tried = server->probe();
            check(tried.ok(), "MCPSUNUCU islem=sina başarısız");
            if (!tried.ok())
                (void)std::fprintf(stderr, "[kentos]   %s\n", tried.error().message.c_str());

            // 11. ONE CLIENT IS SHUT OUT AND THE TOKEN DOES NOT MOVE. A 403 for
            //     the revoked label, a 200 for a different one, and the address
            //     on the settings page unchanged.
            const QString held_token = server->token();
            check(server->revokeClient(QString::fromStdString(probe_label)).ok(),
                  "yetki kaldırılamadı");
            (void)post(base, envelope("tools/list", "{" + meta + "}"), "tools/list", {}, &status);
            say("revoked client", status, QByteArray());
            check(status == 403, "yetkisi kaldırılan istemci 403 almadı");
            check(server->token() == held_token, "yetki kaldırmak belirteci değiştirdi");

            const QByteArray other_meta =
                "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\","
                "\"cad.kentos/client\":\"baska-ajan\"}";
            (void)post(base, envelope("tools/list", "{" + other_meta + "}"), "tools/list", {},
                       &status);
            say("other client", status, QByteArray());
            check(status == 200, "başka bir istemci de durduruldu");

            check(server->restoreClient(QString::fromStdString(probe_label)).ok(),
                  "yetki geri verilemedi");
            (void)post(base, envelope("tools/list", "{" + meta + "}"), "tools/list", {}, &status);
            check(status == 200, "yetki geri verilince istemci hâlâ reddediliyor");

            server->stop();
            check(!server->listening(), "sunucu kapanmadı");

            (void)std::fprintf(stdout, "[mcp] %s — port %u, %zu araç\n",
                               failures == 0 ? "TAMAM" : "BASARISIZ", static_cast<unsigned>(18765),
                               controller->aiService().catalog().tools.size());
            QApplication::exit(failures == 0 ? 0 : 1);
        });
    }
#endif

    if (qEnvironmentVariableIsSet("KENTOS_EDIT_PROBE")) {
        QTimer::singleShot(kFrameDumpSettleMs, &window, [&window] {
            auto* canvas = window.canvas();
            if (canvas == nullptr) {
                (void)std::fprintf(stderr, "[kentos] tuval yok\n");
                QApplication::exit(1);
                return;
            }

            const auto send = [&](QEvent::Type t, const QPointF& at, Qt::MouseButton b,
                                  Qt::MouseButtons held) {
                QMouseEvent ev(t, at, canvas->mapToGlobal(at), b, held, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &ev);
                QCoreApplication::processEvents();
            };

            window.runScriptLine(QStringLiteral("KATMAN ad=PARSEL"));
            window.runScriptLine(QStringLiteral("ALAN 485300,4310200 485360,4310200 "
                                                "485360,4310245 485300,4310245"));

            // AND THE RIGHT BUTTON FINISHES IT. `ALAN` is open-ended — it asks
            // for corner after corner — so a typed line with four of them parks
            // the command on the fifth prompt rather than drawing the parcel.
            // Without this the probe went on to select an object that did not
            // exist yet and then read row 0 of an empty table: a crash, in the
            // one test that exists to prove the mouse reaches the document.
            const QPointF middle(canvas->width() / 2.0, canvas->height() / 2.0);
            send(QEvent::MouseButtonPress, middle, Qt::RightButton, Qt::RightButton);
            send(QEvent::MouseButtonRelease, middle, Qt::RightButton, Qt::NoButton);

            // AND THEN ESC, because the right button FINISHES a shape and leaves
            // the tool in the hand for the next one (`Controller::finishInteractive`).
            // A re-armed ALAN is a command waiting for a corner, and every press
            // below would have gone to it as one — which is exactly what happened:
            // the grip drags fed corners to ALAN, nothing moved, and ALAN then
            // refused a two-corner face. Esc is what puts a tool away.
            {
                QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QCoreApplication::sendEvent(canvas, &esc);
                QCoreApplication::processEvents();
            }

            canvas->zoomToExtents();
            window.runScriptLine(QStringLiteral("SEÇ nesneler=1"));
            QCoreApplication::processEvents();

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

                // THE RIGHT BUTTON FINISHES AND THE TOOL STAYS IN THE HAND; Esc
                // is what puts it away. This used to press Esc here and then
                // expect the tool to still be lit, which is the OTHER model —
                // the one the shell had before a tool became modal — so the
                // check failed against behaviour that is deliberately this way
                // (`Controller::finishInteractive` versus `cancelInteractive`,
                // and `docs/baslangic/arayuz.md`).
                send(QEvent::MouseButtonPress, QPointF(220, 220), Qt::RightButton, Qt::RightButton);
                send(QEvent::MouseButtonRelease, QPointF(220, 220), Qt::RightButton, Qt::NoButton);
                check(doc.live_entity_count() == drawn_before + 1, "arayüzden alan çizilemedi");
                check(polygon->isChecked(), "alan bitince araç sönüyor (yeniden kurulmuyor)");

                esc();
                check(!polygon->isChecked(), "Esc'ten sonra araç hâlâ yanıyor");
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

            // The same magnification hook the timing probe uses. A rendering
            // change that only shows at 1:1 cannot be reviewed from a picture
            // taken at the drawing's full extent.
            if (const QByteArray zoom = qgetenv("KENTOS_ZOOM"); !zoom.isEmpty()) {
                bool ok            = false;
                const double times = QString::fromLocal8Bit(zoom).toDouble(&ok);
                if (ok && times > 0.0 && window.canvas() != nullptr) {
                    window.canvas()->zoomBy(times);
                    QCoreApplication::processEvents();
                }
            }

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
