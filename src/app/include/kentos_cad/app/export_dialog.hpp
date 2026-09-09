// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the one export window.
//
// EVERYTHING THAT LEAVES THE PROGRAM LEAVES THROUGH HERE, and through here means
// through a COMMAND. The drawing goes out as `DIŞAAKTAR`, an object's corners as
// `NOKTALAR yon=yaz`, a layer's symbology as `STİLAKTAR`; this window collects
// the arguments, shows the line it is about to run, and runs it. It writes no
// file itself and knows no format's bytes: a script that types the same line
// gets the same file (Article 1.2), and a user who reads the line at the bottom
// of the window has learnt the command (§5.1 — the command set is what the AI is
// taught from, and what a user is taught from too).
//
// ONE WINDOW FOR THREE SUBJECTS rather than three windows, because "what am I
// writing, in which format, to where" is the same question each time and a
// user who has answered it once should recognise it the next time. The subject
// changes the format list and the options; the shape does not change.
//
// THE PREVIEW IS THE COMMAND. A window that rendered the first lines of the
// file would have to know how the file is written, which is exactly what the
// io layer knows and this window must not duplicate. The line it shows is the
// whole truth of what will happen, and it is true by construction.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"

#include <QString>
#include <QVector>

#include <cstdint>

class QLabel;

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// The component set; see widgets.hpp and fields.hpp.
class Banner;
class Button;
class Field;
class RadioButton;
class Segment;

/// What is being written.
enum class ExportSubject : std::uint8_t {
    Drawing,     ///< the whole drawing, in an external vector format — `DIŞAAKTAR`
    Coordinates, ///< the corners of the selected objects, as a point list — `NOKTALAR yon=yaz`
    Style,       ///< one layer's symbology, as a QGIS style file — `STİLAKTAR`
};

/// Collects the arguments of an export and runs the command they make.
class ExportDialog : public DialogFrame
{
    Q_OBJECT

public:
    /// `context` is the layer name for `Style`; for `Coordinates` the selection
    /// is read from the bus when the window opens, and the ids are written into
    /// the command line so the journal does not depend on what was highlighted.
    ExportDialog(Controller& controller, ExportSubject subject, QString context = QString(),
                 QWidget* parent = nullptr);

    /// The line the window will run, as it stands. Empty until a file is named.
    QString commandLine() const;

    void applyTheme(ThemeMode mode) override;

private:
    /// One entry of the format list.
    struct Format
    {
        QString id;        ///< what the command's `bicim` argument takes
        QString label;     ///< the name a user reads
        QString extension; ///< `.dxf`, with the dot
        QString note;      ///< one line under the name
    };

    /// Fills `formats_` for the subject.
    void collectFormats();

    /// Builds the two columns and the command line under them.
    void build();

    /// Opens the system file chooser filtered to the chosen format.
    void browse();

    /// Rewrites the command line, the extension and the button's state.
    void refreshCommand();

    /// Runs the line; closes on success, says why on failure.
    void runExport();

    Controller& controller_;
    ExportSubject subject_;
    QString context_;
    QString selection_; ///< `1,4,9` — the corners' owners, fixed when the window opened

    QVector<Format> formats_;
    QVector<RadioButton*> choices_;
    int chosen_{0};

    Field* path_{nullptr};
    Segment* axes_{nullptr};
    QLabel* command_{nullptr};
    Banner* trouble_{nullptr};
    Button* go_{nullptr};
};

} // namespace kentos::app
