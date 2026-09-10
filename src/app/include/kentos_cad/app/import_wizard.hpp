// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the two-page import wizard.
//
// WHAT THIS WINDOW IS ALLOWED TO DO, stated once, because it is the same rule
// `database_dialog.hpp` opens with. It collects two arguments — a path and a list
// of layer names — and then runs ONE `İÇEAKTAR` line through the controller. It
// holds no GDAL header, no LibreDWG header and no import loop. The OK button
// produces a journal line a script could have written by hand, which is what
// Article 1.2 means by "the GUI is just one client".
//
// THE PROBE IS THE EXCEPTION THAT PROVES IT. Page two cannot honestly ask "which
// layers do you want" until something has read the file, so the wizard reads it
// once into a SCRATCH document it owns — never the user's — through
// `io::probe_import`. Nothing in that scratch document is journalled, undoable or
// saveable; it exists to fill a checklist and to draw a picture. The real import
// runs afterwards, through the bus, with the ticked names.
#pragma once

#include "kentos_cad/app/dialog_chrome.hpp"
#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/widgets.hpp"
#include "kentos_cad/core/document.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/io/service.hpp"
#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <memory>
#include <stop_token>
#include <vector>

#include <QElapsedTimer>
#include <QString>
#include <QThread>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace kentos::render {
/// The renderer this window draws its preview through; see `backend.hpp`.
class Backend;
} // namespace kentos::render

namespace kentos::app {

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

/// The left half of page two: the file's own geometry, drawn by the SAME renderer
/// the canvas uses.
///
/// A second preview renderer would be a second answer to "what does this look
/// like", and the two would drift — the note on `FrameContext::target` says the
/// same thing about the symbol shelf.
class ImportPreview : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

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

/// File, then layers. Two pages, one command.
class ImportWizard : public DialogFrame
{
    Q_OBJECT

public:
    /// Opens on the file page. Nothing is read until the user asks for it.
    ImportWizard(Controller& controller, ThemeMode theme, QWidget* parent = nullptr);
    ~ImportWizard() override;

    /// The file to start on, so the toolbar's "içe aktar" can open the wizard
    /// already pointing at a chosen path. Empty opens on the picker.
    void setPath(const QString& path);

    /// Points the wizard at `path` and starts reading it at once, landing on the
    /// layer page when the read finishes. What a drag-and-drop — and the
    /// screenshot probe — needs: the file is already chosen, so asking for it
    /// again is a page the user has no work to do on.
    void beginWith(const QString& path);

    /// For the screenshot probe: waits for a read started by `beginWith` to
    /// finish, at most `msecs`, then shows `page`. False when the read did not
    /// finish in time — the probe then photographs nothing rather than a spinner.
    bool probeSettle(int page, int msecs = 15000);

    /// The `İÇEAKTAR` line the user approved, or empty when they cancelled. The
    /// caller runs it: the wizard states the work, the controller does it.
    QString commandLine() const { return line_; }

    /// Hands the theme to the children AND to the row delegate, which is not a
    /// widget and therefore not reached by the walk `DialogFrame` does.
    void applyTheme(ThemeMode mode) override;

private:
    // ---- pages ----
    QWidget* buildFilePage();
    QWidget* buildLayerPage();
    QWidget* buildFieldPage();
    QWidget* buildStepper();

    // ---- acting ----
    void browse();
    void startProbe();
    void probeFinished();
    void showPage(int page);
    void setAllChecked(bool on);
    void refreshTally();
    void applyVisibility();

    /// Every ticked layer name, in the order the file holds them.
    QStringList chosen() const;

    // ---- the field page ----
    void setAllFieldsChecked(bool on);
    void refreshFieldTally();

    /// Every ticked field name, once each, in the order the file holds them.
    QStringList chosenFields() const;

    Controller& controller_;

    QStackedWidget* pages_   = nullptr;
    QLineEdit* pathField_    = nullptr;
    QLabel* fileFacts_       = nullptr;
    ProgressStrip* progress_ = nullptr;
    QLabel* progressText_    = nullptr;
    QWidget* progressBox_    = nullptr;
    QLabel* stepOne_         = nullptr;
    QLabel* stepTwo_         = nullptr;
    QLabel* stepThree_       = nullptr;
    QWidget* stepRule_       = nullptr;
    QWidget* stepRuleTwo_    = nullptr;
    QListWidget* fields_     = nullptr;
    QLabel* fieldTally_      = nullptr;

    ImportPreview* preview_ = nullptr;
    QListWidget* layers_    = nullptr;
    QLineEdit* filter_      = nullptr;
    QLabel* summary_        = nullptr;
    QLabel* tally_          = nullptr;

    Button* back_   = nullptr;
    Button* next_   = nullptr;
    Button* cancel_ = nullptr;

    std::unique_ptr<core::Document> scratch_;
    ImportProbeThread* probe_ = nullptr;
    QTimer* ticker_           = nullptr;
    QElapsedTimer elapsed_;

    io::ImportProbe found_;
    QString line_;
};

} // namespace kentos::app
