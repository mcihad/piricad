// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the print preview window.
//
// WHAT IT IS. The sheet, drawn, and beside it the three things that are still
// open once the frame has been aimed: which PROFILE the sheet is, where the
// output goes, and — for a PDF — its title, its author and its password.
//
// THE SHEET IS THE PROFILE'S, AND ONLY THE PROFILE'S. Paper, orientation,
// resolution and margin live in one place (`Seçenekler ▸ Plot ve Çıktı`, or
// `YAZDIRMAPROFİLİ`), and this window shows what the chosen profile says rather
// than offering the same four choices a second time. Two places to set the
// orientation is two answers to one question, and the one the user did not
// look at wins.
//
// THE PREVIEW IS THE SHEET, not a picture of one: `PrintService::renderPreview`
// puts the same window through the same scene builder and the same backend the
// plot itself uses. What is on screen is what comes out, minus the resolution.
//
// WHAT THE FRAME CAPTURED IS WHAT IS PRINTED. The area arrives exact and stays
// exact: the sheet, the readout and the command line all show the box the canvas
// frame held, to the millimetre. An earlier cut opened on the frame's scale
// ROUNDED UP to the nearest plan scale — 1:184 became 1:200 — which quietly put
// a fifth more ground on the paper than the user had aimed at, and the sheet no
// longer matched the frame they had just dragged. A round scale is worth having,
// but it is a thing the user asks for, not a thing the window does behind them:
// the button beside the scale offers it and the preview shows what it costs.
//
// SO THE AREA IS SAID TWO WAYS, and which one is in force is visible on the
// command line. Untouched, the window is the frame's two corners and the line
// reads `pencere=… pencere=…`. Type a scale or a centre — or press the rounding
// button — and the window becomes that centre at that scale, the line reads
// `merkez=… olcek=…`, and the frame's box is left behind. Both are the same
// command; `core.print` takes either (commands/print.cpp).
//
// AND THE WINDOW IS THE COMMAND, exactly as the export window is. The line under
// the form is shown as it will be sent and sent through
// `Controller::runLineResult` — so a script that types the same line gets the
// same sheet (Article 1.2). The two passwords are the one thing the line does
// NOT carry into the journal; `core.print` reads them and records the
// permissions instead (commands/print.cpp).
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/core/units.hpp"
#include "kentos_cad/io/print_profiles.hpp"

#include <QString>

class QLabel;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp and fields.hpp.
class Banner;
class Button;
class CheckBox;
class ComboBox;
class Field;
class Segment;

/// Collects the arguments of a plot, shows the sheet, and runs `YAZDIR`.
class PrintDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// `window` is the part of the drawing that goes on the sheet — the frame
    /// the canvas captured, or the current view; it is re-fitted to the chosen
    /// profile's printable aspect, so the drawing is never stretched.
    /// `profile` is the profile to open on — the one the toolbar menu named and
    /// the one the frame was shaped by; empty means the default.
    PrintDialog(Controller& controller, core::Box2 window, QString profile = QString(),
                QWidget* parent = nullptr);

    /// The line the window will run, as it stands. Empty until the output is named.
    QString commandLine() const;

    void applyTheme(ThemeMode mode) override;

signals:
    /// The user asked for the profile editor. The window has already closed;
    /// the shell opens `Seçenekler ▸ Plot ve Çıktı`.
    ///
    /// A SIGNAL RATHER THAN A CALL BY NAME: `QMetaObject::invokeMethod` with a
    /// string only finds a slot or a `Q_INVOKABLE`, and a dialog that reached
    /// for its parent's ordinary method by name would silently do nothing.
    void manageProfilesRequested();

    /// The user pressed the centre field's pick button: they want to aim again
    /// on the canvas. The window has closed; the shell re-opens the frame.
    void reaimRequested();

private:
    /// Builds the sheet on the left and the compact form on the right.
    void build();

    /// The profile the chooser names, or the default one.
    io::PrintProfile chosen() const;

    /// The centre the form holds, or the captured window's centre when the box
    /// is empty or unreadable.
    core::Point2 centre() const;

    /// The scale denominator the form holds, or the captured window's own.
    std::int64_t denominator() const;

    /// What goes on the sheet as it stands, and the ONE place that decides it,
    /// so the preview, the readout and the command line cannot disagree.
    ///
    /// The captured box while the fields are untouched — re-fitted to the
    /// profile's printable aspect, which is a no-op for the paper the frame was
    /// shaped by and grows the box when the paper is changed under it. Once a
    /// scale or a centre has been typed, that centre at that scale instead.
    core::Box2 window() const;

    /// True while the area is still the frame's own box rather than a typed
    /// centre and scale. Turned off by the first edit that changes either field,
    /// which is why `shown_scale_` and `shown_centre_` exist: a field that is
    /// merely tabbed through commits its unchanged value, and that is not a
    /// user choosing a scale.
    bool fromFrame() const noexcept { return from_frame_; }

    /// Puts `text` in a field without it counting as an edit.
    void show(Field* field, const QString& text, QString& remembered);

    /// Redraws the sheet and rewrites the readout and the command line.
    void refresh();

    /// Opens the file chooser for the PDF path.
    void browse();

    /// Runs the line; closes on success, says why on failure.
    void run();

    Controller& controller_;
    core::Box2 window_{};

    QLabel* sheet_{nullptr}; ///< the preview image
    QLabel* paper_{nullptr}; ///< `A3 420×297 mm yatay · 150 dpi · kenar 5 mm`
    ComboBox* profile_{nullptr};

    /// THE SHEET'S PLACE. Both open on what the frame captured — its centre and
    /// its own scale, exactly — and both can be typed over, after which they
    /// decide the area instead of the frame. A pafta is "1:1000, centred on this
    /// corner", and this is where that is said.
    Field* centre_{nullptr};
    Field* denominator_{nullptr};

    /// Offers the nearest plan scale above the frame's own — 1:184 becomes
    /// 1:200 — and is hidden when the scale is already one. Pressing it types
    /// the round figure, so the area grows where the user can see it grow.
    Button* round_{nullptr};

    /// The last values this window itself put in the two fields, so an edit can
    /// be told from a field that was only passed through. See `fromFrame`.
    QString shown_scale_;
    QString shown_centre_;
    bool from_frame_{true};

    Segment* target_{nullptr}; ///< PDF file / printer
    Field* path_{nullptr};
    ComboBox* printer_{nullptr};
    QWidget* pdfRows_{nullptr};
    QWidget* printerRows_{nullptr};
    Field* title_{nullptr};
    Field* author_{nullptr};
    Field* password_{nullptr};
    Field* ownerPassword_{nullptr};
    QWidget* permissions_{nullptr};
    CheckBox* allowPrint_{nullptr};
    CheckBox* allowCopy_{nullptr};
    CheckBox* allowModify_{nullptr};
    Banner* trouble_{nullptr};
    QLabel* command_{nullptr};
    Button* go_{nullptr};
};

} // namespace kentos::app
