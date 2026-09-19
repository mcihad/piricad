// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the print engine behind `YAZDIR` and `YAZDIRMAPROFİLİ`.
//
// `/src/command` owns the two commands and knows nothing of Qt; this service
// installs `Bus::on_print_request` and does the work the commands describe: it
// keeps the profiles in the user's configuration directory, and it puts a
// window of the drawing onto a sheet — into a `QPdfWriter` or a `QPrinter` —
// through the SAME render pipeline the canvas draws with (`render::build_scene`
// plus the QPainter backend), so what prints is what was on screen, at the
// sheet's resolution. Nothing here reaches the document except to read it.
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/io/print_profiles.hpp"

#include <QImage>
#include <QObject>
#include <QString>

#include <string>

namespace kentos::core {
class Document; ///< read to draw the sheet; never written by this service
} // namespace kentos::core

namespace kentos::app {

class PrintService : public QObject
{
    Q_OBJECT

public:
    /// Installs the bus hook and loads the profiles from the user's
    /// configuration directory (`yazdirma-profilleri.json`); a missing file is
    /// the built-in set. Clears the hook on destruction.
    PrintService(command::Bus& bus, const core::Document& document, QObject* parent = nullptr);
    ~PrintService() override;

    PrintService(const PrintService&)            = delete;
    PrintService& operator=(const PrintService&) = delete;

    /// The profiles as they stand. Read by the toolbar menu, the settings page
    /// and the print dialog; written only through the bus.
    const io::PrintProfiles& profiles() const noexcept { return profiles_; }

    /// Where the profiles live on disk, for the settings page's note.
    const QString& profilesPath() const noexcept { return path_; }

    /// Says on the transcript anything the CONSTRUCTOR could not say — a
    /// profile file that would not parse.
    ///
    /// WHY IT IS NOT IN THE CONSTRUCTOR: this service is built in the
    /// `Controller`'s member-initialiser list, before `wireBus()` has installed
    /// `Bus::on_echo`, so an echo there goes nowhere. The controller calls this
    /// once the bus can be heard. Nothing happens when there was nothing to say.
    void announce();

    /// Whether this build can encrypt a PDF (qpdf linked).
    static bool encryptionAvailable() noexcept;

    /// The window enlarged — about its centre, on its shorter side — until it
    /// has the profile's PRINTABLE aspect. What the canvas's frame guarantees
    /// by construction, and what the dialog needs again when the paper changes.
    static core::Box2 fitWindow(const io::PrintProfile& profile, core::Box2 window);

    /// The scale denominator `1 : N` a window prints at on a profile: ground
    /// millimetres across the window over printable millimetres across the sheet.
    static double scaleDenominator(const io::PrintProfile& profile, core::Box2 window);

    /// THE WINDOW A CENTRE AND A SCALE STAND FOR: the profile's printable area
    /// in paper millimetres, multiplied by the scale denominator, centred on
    /// `centre`. One implementation, called by the print command's engine and
    /// by the preview window, so a sheet asked for at 1:1000 is the same sheet
    /// whichever of them asked.
    static core::Box2 windowFor(const io::PrintProfile& profile, core::Point2 centre,
                                std::int64_t scale);

    /// The sheet as an image no wider or taller than `max_px`, for the preview.
    /// White paper, the printable area drawn through the render pipeline, a
    /// hairline where the margin is.
    static QImage renderPreview(const core::Document& document, const io::PrintProfile& profile,
                                core::Box2 window, int max_px);

signals:
    /// The profile store changed — added, removed, default moved. The toolbar
    /// menu and the settings page rebuild from `profiles()`.
    void profilesChanged();

private:
    command::Task<core::Result<std::string>> handle(command::PrintRequest request);

    /// Prints an OUTPUT LAYOUT — its own paper, its own pages, its own map frames — to a
    /// PDF or a printer. Takes no profile and no window: a layout carries both
    /// (`core/layout.hpp`, and the refusal in `core.print` that says so).
    core::Result<std::string> printLayout(const command::PrintRequest& request);
    core::Result<std::string> toPdf(const command::PrintRequest& request,
                                    const io::PrintProfile& profile);
    core::Result<std::string> toPrinter(const command::PrintRequest& request,
                                        const io::PrintProfile& profile);
    core::Status persist();

    command::Bus& bus_;
    const core::Document& document_;
    io::PrintProfiles profiles_;
    QString path_;
    std::string trouble_; ///< what `announce` has yet to say; empty when all is well
};

} // namespace kentos::app
