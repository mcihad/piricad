// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the import window.
//
// WHAT THIS WINDOW IS ALLOWED TO DO, stated once, because it is the same rule
// `database_dialog.hpp` opens with. It collects three arguments — a path, a list
// of layer names and a list of field names — and then runs ONE `İÇEAKTAR` line
// through the controller. It holds no GDAL header, no LibreDWG header and no
// import loop. The import button produces a journal line a script could have
// written by hand, which is what Article 1.2 means by "the GUI is just one
// client" — and the line is on screen, under everything, as the print window
// shows its own.
//
// THE PROBE IS THE EXCEPTION THAT PROVES IT. The window cannot honestly ask
// "which layers do you want" until something has read the file, so it reads it
// once into a SCRATCH document it owns — never the user's — through
// `io::probe_import`. Nothing in that scratch document is journalled, undoable or
// saveable; it exists to fill a checklist and to draw a picture. The real import
// runs afterwards, through the bus, with the ticked names.
//
// ONE PAGE, NOT THREE. This was a wizard: a page holding one path field, a page
// of layers, a page of fields, with `İleri` between them — so the first thing a
// user saw was a field and a reference list in a window a thousand pixels wide,
// and the read, which changes nothing, waited for a press. Now naming a file the
// program can read reads it: the drawing fills the window the moment it has been
// read, and the layers and the fields are two panes of one column beside it.
#pragma once

#include "piricad/app/dialog_chrome.hpp"
#include "piricad/app/theme.hpp"
#include "piricad/app/widgets.hpp"
#include "piricad/core/document.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/io/service.hpp"
#include "piricad/render/drawlist.hpp"
#include "piricad/render/scene.hpp"
#include "piricad/render/view.hpp"

#include <memory>
#include <stop_token>
#include <vector>

#include <QElapsedTimer>
#include <QString>
#include <QThread>
#include <QWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace piricad::render {
/// The renderer this window draws its preview through; see `backend.hpp`.
class Backend;
} // namespace piricad::render

namespace piricad::app {

/// The one road from a widget to the document; see `controller.hpp`.
class Controller;

/// Reads a file into a scratch document without blocking the window.
///
/// A thread rather than a coroutine on the GUI loop: `io::probe_import` is a
/// synchronous read of a file that may hold a million entities, and the one thing
/// a progress bar must not do is stop moving. The thread owns nothing but the
/// stop source; the document belongs to the wizard and is not touched from here
/// after `finished()` (io.md R15 answers the stop inside 100 ms).
class ImportProbeThread : public QThread
{
    Q_OBJECT

public:
    /// Reads `path` into `into` when started. Nothing happens until `start()`.
    /// `options` carries the drawing's CRS and the unit to assume for a file
    /// that names none — the same two facts the command hands the reader.
    ImportProbeThread(core::Document& into, QString path, io::ImportOptions options,
                      QObject* parent = nullptr);

    /// Asks the read to stop. The thread returns within 100 ms.
    void cancel();

    /// Valid only after `finished()`.
    const core::Result<io::ImportProbe>& outcome() const noexcept { return outcome_; }

protected:
    /// The read itself, on this thread.
    void run() override;

private:
    core::Document& into_;
    QString path_;
    io::ImportOptions options_;
    std::stop_source stop_;
    core::Result<io::ImportProbe> outcome_;
};

/// The window's stage once a file has been read: the file's own geometry, drawn
/// by the SAME renderer the canvas uses.
///
/// A second preview renderer would be a second answer to "what does this look
/// like", and the two would drift — the note on `FrameContext::target` says the
/// same thing about the symbol shelf.
class ImportPreview : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

public:
    /// Builds an empty preview pointing at no document.
    explicit ImportPreview(QWidget* parent = nullptr);
    ~ImportPreview() override;

    /// Points the preview at a document and fits it. Null clears the view.
    void setDocument(const core::Document* doc);

    /// Refits after the tick boxes changed which layers are visible.
    void refit();

    /// Where the wheel reads its direction and its step from.
    ///
    /// The SAME store the canvas reads, so `core.harita.tekerlek_ters` and
    /// `core.harita.yakinlastirma_adimi` mean one thing in the program rather
    /// than one thing per widget. Without it this preview carried its own
    /// hard-coded step, pointing the other way.
    void useSettings(const core::Settings* store) { settings_ = store; }

    void applyTheme(ThemeMode mode) override;

protected:
    /// Draws the scratch document through the shared renderer.
    void paintEvent(QPaintEvent* event) override;
    /// Re-fits, so the drawing keeps filling the panel.
    void resizeEvent(QResizeEvent* event) override;
    /// Zooms about the pointer.
    void wheelEvent(QWheelEvent* event) override;
    /// Starts a pan.
    void mousePressEvent(QMouseEvent* event) override;
    /// Continues a pan.
    void mouseMoveEvent(QMouseEvent* event) override;
    /// Ends a pan.
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    const core::Document* doc_      = nullptr;
    const core::Settings* settings_ = nullptr;
    std::unique_ptr<render::Backend> backend_;
    render::ViewTransform view_;
    render::SceneOptions options_;
    render::DrawList draw_;
    ThemeMode theme_ = ThemeMode::Dark;
    QPoint dragFrom_;
    bool dragging_ = false;
};

/// A file, its layers and its fields — one window, one command.
class ImportWizard : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on the invitation to pick a file. Nothing is read until one is named.
    ImportWizard(Controller& controller, ThemeMode theme, QWidget* parent = nullptr);
    ~ImportWizard() override;

    /// The file to start on, so the toolbar's "içe aktar" can open the window
    /// already pointing at a chosen path. Empty opens on the invitation. A path
    /// the program can read starts being read a moment later, as a typed one does.
    void setPath(const QString& path);

    /// Points the window at `path` and starts reading it at once. What a drop on
    /// the window — and the screenshot probe — needs: the file is already chosen,
    /// so there is no typing to wait out.
    void beginWith(const QString& path);

    /// For the screenshot probe: waits for a read started by `beginWith` to
    /// finish, at most `msecs`, then shows the column's pane for `page` — 1 the
    /// layers, 2 the fields, the numbers the wizard's pages had. False when the
    /// read did not finish in time — the probe then photographs nothing rather
    /// than a spinner.
    bool probeSettle(int page, int msecs = 15000);

    /// The `İÇEAKTAR` line the user approved, or empty when they cancelled. The
    /// caller runs it: the window states the work, the controller does it.
    QString commandLine() const { return line_; }

    /// Hands the theme to the children AND to the row delegates, which are not
    /// widgets and therefore not reached by the walk `DialogFrame` does.
    void applyTheme(ThemeMode mode) override;

protected:
    /// A file dragged over the window is taken when it is one local file.
    void dragEnterEvent(QDragEnterEvent* event) override;
    /// The drag went elsewhere: the frame stops answering it.
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    /// A file dropped on the window is read at once, as `beginWith` reads it.
    void dropEvent(QDropEvent* event) override;

private:
    // ---- the window's parts ----
    QWidget* buildFileStrip();
    QWidget* buildStage();
    QWidget* buildInvitation();
    QWidget* buildReading();
    QWidget* buildDrawing();
    QWidget* buildColumn();
    QWidget* buildLayerPane();
    QWidget* buildFieldPane();
    QWidget* buildCommandStrip();

    /// Reads `text` as a path and says, where it was typed, whether this program
    /// can open it — the facts line and the verdict banner — and, when it can,
    /// has it read once the typing has settled.
    void judgePick(const QString& text);

    // ---- acting ----
    void browse();
    void startProbe();
    void probeFinished();

    /// The column's pane: 0 the layers, 1 the fields.
    void showPane(int pane);
    void setAllChecked(bool on);
    void refreshTally();
    void applyVisibility();

    /// Every ticked layer name, in the order the file holds them.
    QStringList chosen() const;

    // ---- the field pane ----
    void setAllFieldsChecked(bool on);
    void refreshFieldTally();

    /// Every ticked field name, once each, in the order the file holds them.
    QStringList chosenFields() const;

    /// The line the window stands for as it stands: empty until a file has been
    /// read and at least one layer is ticked.
    QString lineFor() const;

    /// Writes that line into the command strip and says whether the import
    /// button is live.
    void refreshLine();

    Controller& controller_;

    QLineEdit* pathField_ = nullptr;
    QLabel* fileFacts_    = nullptr;

    /// THE VERDICT ON THE PICK, shown where the pick is made.
    ///
    /// The window knows the allow-list and used to ignore it: any existing file
    /// enabled `İleri`, so choosing a `.dwg` on a build without the reader — or a
    /// `.txt` — was accepted here and refused a page later, after the user had
    /// committed to the flow. The format list was printed at the bottom as
    /// reference the user was expected to check for themselves.
    Banner* verdict_ = nullptr;

    /// The left of the window: the invitation, the read under way (or what
    /// stopped it), and the drawing once it has been read.
    QStackedWidget* stage_ = nullptr;
    QWidget* dropZone_     = nullptr; ///< the invitation's frame, lit while a file is held over it
    ProgressStrip* progress_ = nullptr;
    QLabel* progressText_    = nullptr;
    Banner* failure_         = nullptr; ///< why the last read did not finish
    Banner* stopped_         = nullptr; ///< the last read was stopped by the user
    Button* stopRead_        = nullptr; ///< stops the read under way

    /// The reader's diagnostics, one banner each, above the drawing.
    ///
    /// They used to be flattened into one grey paragraph under the preview, with
    /// their level spelled as a lower-case word in front of the sentence — so
    /// "this file declares no coordinate system" and "this file declares no
    /// units" read as footnotes. In a cadastral drawing those two are the ones
    /// that put a parcel in the wrong place at the wrong scale, and the output
    /// is a document somebody signs.
    QVBoxLayout* notices_ = nullptr;

    /// The theme the window is wearing, kept because the notices are built AFTER
    /// the theme was applied. `applyThemeToChildren` walks the widgets that exist
    /// when it runs, so a banner created when a file is read never heard about
    /// the mode and painted itself in the other one.
    ThemeMode mode_ = ThemeMode::Dark;

    ImportPreview* preview_ = nullptr;
    QLabel* summary_        = nullptr; ///< the file's facts, under the drawing

    /// The right of the window: a note until a file has been read, then the
    /// two panes and the switch between them.
    QStackedWidget* column_ = nullptr;
    Segment* paneSwitch_    = nullptr;
    QStackedWidget* panes_  = nullptr;
    QListWidget* layers_    = nullptr;
    QLineEdit* filter_      = nullptr;
    QLabel* tally_          = nullptr;
    QListWidget* fields_    = nullptr;
    QLabel* fieldTally_     = nullptr;

    QLabel* command_ = nullptr; ///< the line the import button will run
    Button* go_      = nullptr;
    Button* cancel_  = nullptr;

    std::unique_ptr<core::Document> scratch_;
    ImportProbeThread* probe_ = nullptr;
    QTimer* ticker_           = nullptr;
    QTimer* settle_           = nullptr; ///< reads a typed path once the typing stops
    QElapsedTimer elapsed_;

    QString probed_;            ///< the path the stage's read is, or was, of
    bool pickReadable_ = false; ///< the path in the field names a file this build reads
    bool restart_      = false; ///< the pick changed during a read: read the new one after
    bool filling_      = false; ///< the lists are being filled, not ticked

    io::ImportProbe found_;
    QString line_;
};

} // namespace piricad::app
