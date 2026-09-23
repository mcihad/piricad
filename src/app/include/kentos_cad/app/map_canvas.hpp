// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the map canvas.
//
// Target (kentoscad.md §6.3): QRhiWidget with our own GPU pipeline.
//
// WHICH SURFACE THIS IS depends on one build option and nothing else. With
// `KENTOS_WITH_RHI=ON` the canvas is a `QRhiWidget` and hands the backend a
// command buffer; without it the canvas is a `QWidget` and hands the backend a
// paint device — CLAUDE.md Article 8.1, the Phase-0 deviation. Everything between
// those two lines is identical, because the scene is built by kentos_render in
// exactly the form the GPU path needs: screen-space floats produced after the
// origin offset (§10.3).
//
// The base class is the ONLY backend fact this file carries, which is what
// render.md R1 allows it. Nothing here names a backend implementation type; the
// frame handles are packed by `backend_factory.hpp`.
#pragma once

#include "kentos_cad/app/theme.hpp"
#include "kentos_cad/app/tokens.hpp"
#include "kentos_cad/command/measure_mark.hpp"
#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/core/transform.hpp"
#include "kentos_cad/core/trim_curve.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <QCursor>
#include <QImage>
#include <QRectF>

#if KENTOS_HAVE_RHI
#include <QRhiWidget>
#else
#include <QWidget>
#endif

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class QLineEdit;

namespace kentos::core {
/// The setting store; declared here so the wheel helper below can name it without
/// pulling `core/settings.hpp` into every translation unit that draws a canvas.
class Settings;
} // namespace kentos::core

namespace kentos::core {
struct AreaGhost; ///< core/area_edit.hpp; the .cpp includes the definition
/// Forward-declared on purpose: `entity_kind.hpp` names a member `emit`, which
/// Qt's keyword macro would erase in any translation unit that includes Qt
/// first. Only the .cpp includes the full definition.
struct EmitBuffer;
} // namespace kentos::core

namespace kentos::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

/// What one wheel turn does to the scale, read from the two map settings.
///
/// ONE FUNCTION BECAUSE THERE ARE TWO MAP VIEWS. The canvas is not the only
/// widget with a wheel: the import wizard's preview has one too, and it used to
/// carry its own hard-coded 20 % — IN THE OPPOSITE DIRECTION. Pushing the wheel
/// away zoomed in on the drawing and out on the preview of the very file about to
/// become that drawing, and neither honoured `core.harita.tekerlek_ters`, so a
/// user who inverted the wheel got one view inverted and the other not.
///
/// `notches` is `angleDelta().y() / 120.0` — fractional on a trackpad, which is
/// why it is a double and why the step is raised to it rather than multiplied by
/// it. The result goes straight to `ViewTransform::zoom_at`, where above one
/// means closer.
double wheel_zoom_factor(const core::Settings& store, double notches);

/// The widget the canvas IS. See the header note: one build option, two surfaces,
/// one set of event handlers above them.
#if KENTOS_HAVE_RHI
using CanvasSurface = QRhiWidget;
#else
using CanvasSurface = QWidget;
#endif

class MapCanvas : public CanvasSurface, public Themed
{
    Q_OBJECT
    Q_INTERFACES(kentos::app::Themed)

public:
    /// Builds the canvas over a controller. The controller outlives it — the main
    /// window owns both — so it is held by reference rather than by pointer.
    explicit MapCanvas(Controller& controller, QWidget* parent = nullptr);

    /// The view transform: centre, scale and the origin offset that keeps a TUREF
    /// coordinate out of a float (§10.3). View state is NOT document state
    /// (model.md R43), which is why it lives here and not in the document.
    render::ViewTransform& view() noexcept { return view_; }

    const render::ViewTransform& view() const noexcept { return view_; }

    /// Screen pixels per PAPER millimetre, from the screen this canvas is on.
    /// The plot-scale reading is this number times the ground millimetres one
    /// pixel covers, which is what "1 : 1 000" on a pafta means.
    double pixelsPerPaperMm() const noexcept { return options_.pixels_per_paper_mm; }

    /// Re-reads the palette. Called when any client writes the theme preference,
    /// not only when the menu item is toggled.
    void applyTheme(ThemeMode mode) override;

    /// Re-reads the `ızgara`.* preferences. Called at start-up and whenever any
    /// client writes one — the menu, the command line, a script or the AI, which
    /// is the whole point of routing the write through the bus (CLAUDE.md 1.2).
    void reloadGridSettings();

    /// Re-reads the aid settings and re-evaluates the marker under the cursor.
    /// Called whenever any client writes `core.yakalama.*` — the F-keys, the
    /// command line, a script or the AI (Article 1.2).
    void reloadSnapSettings();
    void setDebugHud(bool on);

    /// Turns the 45 degree lock on or off through the bus.
    void setDiagonalLock(bool on);

    /// Turns the surface-normal lock on or off through the bus.
    ///
    /// The lock draws PERPENDICULAR TO THE SURFACE the line started on, not
    /// perpendicular to the page: on a parcel edge running at 37 degrees it gives
    /// 127, which is what a setback line, a frontage line and a section line all
    /// are. Ortho cannot express that, and measuring one by eye is how a 0.4 m
    /// error gets into a legal document.
    void setSurfaceNormalLock(bool on);

    /// Repaints `rounds` times and returns the frame costs, in microseconds.
    ///
    /// Developer tooling, the same category as `KENTOS_FRAME_DUMP`: there is no
    /// user-facing feature here and so no `/docs` page (CLAUDE.md 5.17). It
    /// exists because "which backend is faster" and "does the QRhi one earn its
    /// keep" are questions that must be ANSWERED rather than argued, and the
    /// number was already measured — it just had nowhere to go but a debug HUD
    /// nobody can read from a headless run.
    std::vector<int> timeFrames(int rounds);

    /// Scene-rebuild costs from the last `timeFrames`, in microseconds.
    const std::vector<int>& sceneCosts() const noexcept { return scene_costs_; }

    void zoomToExtents();

    /// Frames `box` the way KAPSAM frames the drawing, publishing the new scale
    /// to the aids — a probe that must see what it draws, at the scale a hand
    /// would draw it at, goes through here rather than past the aperture.
    void zoomToBox(const core::Box2& box);

    /// Keeps what a measurement measured on the canvas (`Bus::on_measure_mark`):
    /// the run with its lengths, the face with its area, the angle, the point
    /// with its coordinates. They stay until the drawing changes or Esc is
    /// pressed with nothing running — the answer is where it was asked.
    void addMeasureMark(const command::MeasureMark& mark);

    /// Takes every measurement mark off the canvas.
    void clearMeasureMarks();

    /// Tells the canvas the document may have changed. The shell calls it after
    /// every command; a mark taken before a change describes a drawing that is
    /// gone and is dropped at the next frame.
    void noteDocumentChange();

    /// How many measurement marks are on the canvas, for the probes.
    std::size_t measureMarkCount() const noexcept { return marks_.size(); }

    void zoomBy(double factor);

    /// Moves the view's centre without changing its scale. KAYDIR's landing point.
    void setCentre(core::Point2 centre);
    void resetView();

    QString backendName() const;

    /// Whether the live backend draws on the GPU. The draw-call budget is only
    /// meaningful where it does; see `render::FrameStats::draw_calls`.
    bool backendIsGpu() const;

    /// Whether the ON-SCREEN canvas has the graphics context it paints with.
    ///
    /// A `QRhiWidget` gets its `QRhi` from the top-level window, and the window
    /// decides ONCE, when its native window is created, whether it composites
    /// through QRhi at all. A window created before this widget existed never
    /// does, the widget paints nothing and says so only on stderr ("QRhiWidget:
    /// No QRhi"). The smoke test asks this after the first paint. Always true on
    /// the QPainter surface, which needs no context.
    bool hasGpuContext() const;

    /// The document this canvas draws. READ ONLY, like every other reader outside
    /// a command (Article 5.9): the canvas edits through the bus and so does
    /// anyone holding this.
    const core::Document& document() const;

    /// How many vertices the LAST overlay build put in the running command's
    /// guide. Zero when no guide was drawn.
    ///
    /// A guide's correctness is a picture, and a picture is what a headless test
    /// cannot look at — but "the circle guide is a circle" is answerable as a
    /// count: a line has two vertices and a circle has many. Developer tooling in
    /// the same category as `timeFrames`.
    std::size_t guideVertexCountForProbe() const noexcept { return guide_vertices_; }

    /// Enter while a face is being pulled to a wanted area: sends the point that
    /// lands the figure exactly. True when it did; false when nothing of the
    /// kind is being asked, so the caller can go on to what Enter means next.
    bool acceptGuide();

    /// Enter's answer when nothing else claims it: finishes a run that is asking
    /// for a POINT, the way the right button does, leaving the tool in the hand.
    /// False when no such run is waiting. Reached from the canvas and from the
    /// command line, because focus is on the line far more often than on the
    /// drawing — both roads, one answer.
    bool finishPointRun();

    /// Opens the PRINT FRAME: an inner window of the viewport at `aspect`
    /// (printable width over printable height), with everything outside it
    /// greyed. The frame keeps its size on screen — it is the SHEET, not a
    /// rectangle in the drawing — and the map moves under it: dragging pans,
    /// the wheel zooms, exactly as they do with no frame up. Pressing YAZDIR
    /// again captures what is inside (`printFrameWindow`); Esc or the right
    /// button puts it away.
    ///
    /// This is a VIEW state and nothing else (model.md R43): no command runs,
    /// the document is not touched, and the selection is left alone.
    void beginPrintFrame(double aspect);

    /// Changes the frame's shape without closing it — the print dialog's paper
    /// or orientation changed while the frame was up.
    void setPrintFrameAspect(double aspect);

    /// Puts the frame away. `printFrameEnded` follows when one was up.
    void endPrintFrame();

    bool printFraming() const noexcept { return print_aspect_ > 0.0; }

    /// What is inside the frame, in document millimetres — the window a plot
    /// prints. An empty box when no frame is up.
    core::Box2 printFrameWindow() const;

    /// What a FORM FIELD can ask the scene for (fields.hpp `FieldKind::Point`
    /// and `FieldKind::Object`).
    enum class Capture : std::uint8_t {
        Point,  ///< one coordinate, with the snap aids so a corner is a corner
        Object, ///< one object under the click
    };

    /// Arms the canvas for ONE pick of `kind` on behalf of a form field. The
    /// pointer becomes the pick mark, the status line says what is wanted, and
    /// the next left click answers: `pointCaptured` or `objectCaptured`. A
    /// click on more than one object asks the shell (`captureAmbiguous`), Esc
    /// and the right button give up. NOT a command and not a selection: the
    /// value goes into a text box, and the document is not touched (model.md
    /// R43). A field's pick and a running command's prompt are never both live:
    /// the pick takes the click while it is armed.
    void beginCapture(Capture kind);

    /// Puts the pick away with no answer. `captureEnded` follows.
    void cancelCapture();

    /// Answers an object pick with `key` — what the shell chose from the list it
    /// opened on `captureAmbiguous`.
    void finishCapture(core::EntityKey key);

    bool capturing() const noexcept { return capture_.has_value(); }

    /// The dynamic-input label the guide last carried, for `KENTOS_EDIT_PROBE`.
    /// Empty when nothing is being dragged or the reading is switched off.
    const std::string& guideLabelForProbe() const noexcept { return guide_label_; }

    /// The canvas frame as an image, whichever surface this build has.
    ///
    /// `QWidget::grab()` renders through the BACKING STORE, and a `QRhiWidget`'s
    /// frame is not there — it is on the GPU. So a window grab of a GPU build
    /// comes out with a hole exactly where the drawing is, which is what made
    /// `KENTOS_FRAME_DUMP` report an empty canvas on a canvas that was drawing
    /// correctly, and what made `ci-gate-render-desen.py` unable to measure the
    /// GPU path at all.
    QImage grabCanvas();

    /// What the last frame cost the BACKEND (render.md R7). Zero before the
    /// first paint, and zero for a backend that is not on the GPU.
    render::FrameStats frameStats() const;

signals:
    /// Emitted as the pointer moves, in DOCUMENT coordinates. The status bar
    /// labels them `sağa değer` (Y) and `yukarı değer` (X), which is the Turkish
    /// convention and the reverse of the member names (model.md R37a).
    void cursorMoved(core::Point2 world);

    /// Emitted after a pan or a zoom, so the scale readout can follow.
    void viewChanged();

    /// A click that hit more than one object, with the candidates nearest first
    /// and the modifiers that were down.
    ///
    /// THE CANVAS DOES NOT OPEN THE WINDOW, for the reason `LayerPanel` does not
    /// open the style designer: a widget that opened a dialog would be a widget
    /// that has to know what is in it. This says what happened; the shell decides
    /// what to show, and whatever it shows sends `SEÇ` like every other client.
    void pickAmbiguous(const std::vector<core::EntityId>& candidates,
                       Qt::KeyboardModifiers modifiers);

    /// A line for the transcript and the status strip, from the canvas itself.
    ///
    /// Used for the one thing the canvas knows and the bus cannot: that Enter was
    /// pressed with nothing picked while a command was asking which objects to act
    /// on. Everything else a user reads comes from a command, and must.
    void echoRequested(const QString& text);

    /// A form field's pick began; `prompt` is what the status line should say.
    void captureBegan(const QString& prompt);

    /// The pick landed on a coordinate (`Capture::Point`), aids applied.
    void pointCaptured(core::Point2 world);

    /// The pick landed on exactly one object (`Capture::Object`).
    void objectCaptured(core::EntityKey key);

    /// The pick landed on several objects, nearest first. The shell asks which
    /// and answers with `finishCapture`, or gives up with `cancelCapture`.
    void captureAmbiguous(const std::vector<core::EntityId>& candidates);

    /// The pick is over, answered or not. The status line goes back to what the
    /// running command says, if one is running.
    void captureEnded();

    /// The print frame opened; `prompt` is what the status line should say.
    void printFrameBegan(const QString& prompt);

    /// The print frame went away, captured or cancelled.
    void printFrameEnded();

    /// Enter was pressed while the frame was up: take this sheet. The same
    /// thing the toolbar's second press means, from the keyboard.
    void printFrameAccepted();

protected:
    /// Qt event handlers. Every one of them either changes the VIEW — which is
    /// not document state — or feeds a point to the running command through the
    /// controller. None of them edits the document, because a mouse is a client
    /// like any other and gets no private road (Article 1.2, 5.9).
#if KENTOS_HAVE_RHI
    /// The GPU frame. `QRhiWidget` calls this with the frame's command buffer
    /// already open, which is exactly what the backend's `FrameContext::target`
    /// carries in this build. `paintEvent` belongs to the base class here and is
    /// not overridden — a widget that painted over its own swapchain would be
    /// drawing twice.
    void render(QRhiCommandBuffer* cb) override;
#else
    void paintEvent(QPaintEvent* event) override;
#endif
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void rebuildScene();

    /// Builds the screen-space overlay for this frame: grid, selection, rubber
    /// band, selection box, crosshair, snap glyph, developer HUD.
    ///
    /// This widget decides WHAT is drawn over the document, because it is what
    /// knows where the cursor is and what is selected. It decides nothing about
    /// HOW — that is the backend's, and keeping the line there is what render.md
    /// R1 is about.
    void buildOverlay();
    void buildGrid();
    void buildSelection();
    void buildSelectionBox();
    void buildSnapMarker();
    void buildCrosshair();
    void buildRuler();

    /// Sets the platform pointer for the moment: blank over the canvas, because
    /// the drawn crosshair is the pointer; a closed hand while panning or
    /// dragging a corner; the arrow only when the crosshair is turned off.
    void applyPointer();

    /// The cursor in document millimetres: the snapped point when an aid has
    /// fired, else the raw position — where a click would land.
    core::Point2 cursorWorld() const;

    /// The face-at-wanted-area ghost for the running ALANDÜZENLE prompt, or an
    /// empty one when no such prompt is up (core/area_edit.hpp).
    core::AreaGhost areaGhost() const;

    /// The sweep between two arms, drawn at a fixed number of pixels from the
    /// vertex, in the session's angle rule; returns the sweep in turns. Shared by
    /// AÇIÖLÇ's preview and the mark it leaves, so the two are one picture.
    double addAngleSweep(std::size_t batch, core::Point2 vertex, core::Point2 arm_a,
                         core::Point2 arm_b);

    /// A label at a pixel position in the readout ink.
    void addReadout(float x, float y, const std::string& text);

    /// Draws `path` into `batch`: segments as they are, arcs by `arc_outline`.
    void addCurve(std::size_t batch, const core::CurvePath& path);

    /// Marks every cut of a trim — a cross, or a ring for a touch — and returns
    /// how many only touch.
    std::size_t addCutMarks(std::span<const core::PathCrossing> cuts);

    /// Dots the stretch each carried-on edge runs past its real end to reach a
    /// cut in `cuts` (`core::TrimGuide::carry`).
    void addImpliedEdges(const core::Document& doc, core::EntityId target,
                         const core::TrimGuide& guide, std::span<const core::PathCrossing> cuts);

    /// Draws the measurements the session has left (`addMeasureMark`) and drops
    /// the ones the drawing has since moved on from.
    void buildMeasureMarks();

    /// Appends one run of document points, mapped through `map`, to an overlay
    /// batch. The default is the identity, which is what every caller but the
    /// ghost wants.
    void addWorldRun(std::size_t batch, std::span<const core::Mm> xs, std::span<const core::Mm> ys,
                     bool closed, const core::Xform& map = {});

    /// Appends every run of an outline buffer, mapped, to an overlay batch.
    void addEmitRuns(std::size_t batch, const core::EmitBuffer& buf, const core::Xform& map = {});

    /// The selected objects' outlines under `map`: the ghost TAŞI, KOPYALA,
    /// DÖNDÜR, ÖLÇEKLE and AYNALA carry under the cursor. `map` is the verb's own
    /// transform, so what is drawn is the result rather than a guess at it.
    void addGhost(std::size_t batch, const core::Xform& map,
                  std::span<const std::int64_t> keys = {});

    /// Draws the drafting guides across the whole canvas, under everything else.
    ///
    /// Under, because a guide is furniture: it must never sit on top of the
    /// drawing it is there to help place. Dashed and in the aid colour, so it
    /// cannot be mistaken for a line the plot will print.
    void buildGuides();

    /// The tracking traces and the marks they run from (`core::SnapTracking`).
    /// Drawn so a mark is visible as a LINE and not only as a place the cursor
    /// jumps to: a user who cannot see what they acquired cannot tell a trace
    /// from a snap that happened to agree with it.
    void buildTracking();
    void buildScaleBar();
    void buildNorthArrow();
    void buildZoomStack();
    void buildReadout();

    /// The colour a setting names, or the theme's own when the setting is zero.
    static std::uint32_t chosen(std::uint32_t declared, std::uint32_t fallback) noexcept
    {
        return declared != 0 ? declared : fallback;
    }

    /// Takes the next overlay batch, reusing the one that position held on the
    /// previous frame so the draw path allocates nothing (render.md R20).
    /// AN INDEX, NOT A REFERENCE, and that is the whole point.
    ///
    /// The batches live in a `std::vector` that this grows, so a reference handed
    /// out before a later call is a reference into freed memory the moment the
    /// vector reallocates. It DID: `buildGrid` held the minor-grid batch across
    /// the call that made the major-grid one, and every frame that needed a new
    /// batch wrote its grid lines into a dangling pointer. AddressSanitizer found
    /// it; nothing else could, because a vector with spare capacity does not
    /// reallocate and the bug slept until the overlay grew.
    ///
    /// An index cannot dangle. `addRun` and `addCircle` take one and look the
    /// batch up, which is one indexed load per call and no way to get it wrong.
    std::size_t nextBatch(std::uint32_t rgba, float width_px, bool dashed,
                          std::uint32_t fill_rgba = 0);

    /// Appends one run of widget-space points.
    void addRun(std::size_t batch, std::span<const render::ScreenPointF> points, bool closed);
    void addRun(std::size_t batch, std::initializer_list<render::ScreenPointF> points, bool closed);

    /// Narrows a widget-space Qt point through the render module's one sanctioned
    /// conversion, so the canvas has exactly one place where a coordinate becomes
    /// a float (render.md P1).
    static render::ScreenPointF toScreenF(const QPointF& p);

    /// Appends a circle as a closed polygon: the overlay carries runs of points
    /// and nothing else, so no backend needs an ellipse call of its own.
    void addCircle(std::size_t batch, float cx, float cy, float radius);

    /// Re-runs the aid pipeline for the current cursor so the marker on screen is
    /// the point a click would actually produce. Reads the document; never writes.
    void updateSnapPreview();

    /// Turns one finished left-drag into a `SEÇ` invocation. The box, the single
    /// pick and the Shift/Ctrl modifiers all become arguments — there is no
    /// selection path that does not go through the bus (Article 1.2).
    void dispatchSelection(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods);

    /// Everything the aids look like, cached from the preferences so paintEvent
    /// does no setting lookups. Refreshed by `reloadGridSettings()` whenever the
    /// bus reports that a store moved.
    struct AidLook
    {
        bool ruler{true};                 ///< the two scales along the top and the left
        int ruler_px{22};                 ///< their thickness
        int ruler_unit{0};                ///< 0 metre, 1 santimetre, 2 kilometre
        bool scale_bar{true};             ///< the bar that says what the zoom means
        bool north{true};                 ///< the north arrow
        bool readout{true};               ///< the cursor's own easting and northing
        int cursor{0};                    ///< 0 full screen, 1 short, 2 none
        int cursor_px{30};                ///< arm length of the short cursor
        double pick_px{6.0};              ///< half side of the pick box: the selection tolerance
        int marker_px{12};                ///< half size of the snap marker
        bool snap_tip{true};              ///< name the mode beside the marker
        std::uint32_t marker_rgba{0};     ///< 0 = take the theme's own colour
        std::uint32_t grid_rgba{0};       ///< 0 = the theme's
        std::uint32_t grid_major_rgba{0}; ///< 0 = the theme's
        std::uint32_t selection_rgba{0};  ///< 0 = the theme's

        /// Whether the guide carries its own length and bearing while it drags.
        bool dynamic_input{true};

        /// How big the figures beside the cursor are drawn, in pixels.
        ///
        /// Its own setting rather than the interface font, because these are read
        /// WHILE THE HAND IS MOVING, over a drawing, often at arm's length — and
        /// the interface font is sized for text read still and close.
        int hint_px{14};

        /// How the reading's angle is written: the unit (`core.aci.birim`) and
        /// the rule (`core.aci.kural`) — semt + grad by default, so the figure on
        /// the dragged line is the one a Turkish instrument shows, and the same
        /// one `@mesafe<açı` typed at the command line means (TODOS-CAD P0-4).
        core::AngleConvention angle{};

        /// The step the cursor's DISTANCE from the previous point is rounded to,
        /// in millimetres. 0 is off. A user who says "12 cm" gets 12, 24, 36…
        core::Mm step{0};
    };

    /// Grid shape, cached from the preferences so paintEvent does no lookups.
    struct GridSetup
    {
        bool visible{true};
        bool adaptive{true};
        core::Mm step{10000};
        int major{5};
    };

    /// Publishes the view scale to the bus. The snap and pick tolerances are
    /// declared in screen pixels, and turning pixels into millimetres is the one
    /// thing only the view knows (`kentos_cad/command/aids.hpp`).
    void publishViewScale();

    Controller& controller_;

    /// The one road from this widget to a pixel. Created by the factory, never
    /// named by type here (render.md R1).
    std::unique_ptr<render::Backend> backend_;

    Palette palette_{themePalette(ThemeMode::Light)};

    /// The same theme in its NAMED form. The overlay reaches for tokens the
    /// legacy `Palette` has no field for — the ruler's sunken ground, its
    /// division mark, the scale bar's fill — and duplicating them into `Palette`
    /// would be a second colour table (`tokens.hpp` is the only one).
    ///
    /// A POINTER, not a value, and the reason is a build failure rather than a
    /// preference. `darkTokens()` returns a reference to a static that outlives
    /// everything, so there is nothing to copy — and a `Tokens` MEMBER makes the
    /// size of this class depend on the size of that struct, so every
    /// translation unit that includes this header has to be rebuilt the day a
    /// token is added. When one is not, two translation units disagree about how
    /// big a `MapCanvas` is and the heap is quietly corrupted; glibc noticed at
    /// shutdown, in an unrelated destructor, with nothing in the trace pointing
    /// here. A pointer cannot go stale.
    const Tokens* tokens_{&darkTokens()};

    /// The zoom stack's screen box, so a click on it can be told from a click on
    /// the drawing. Rebuilt every frame by `buildZoomStack`.
    QRectF zoom_stack_;
    render::ViewTransform view_;
    render::DrawList draw_;
    render::Overlay overlay_;

    /// How many overlay batches this frame has claimed. See `nextBatch`.
    std::size_t overlay_used_{0};
    render::SceneOptions options_{};
    GridSetup grid_{};
    AidLook look_{};

    /// Opens the in-canvas text box at `where` (widget pixels) for a command that
    /// is waiting on a string, and hands what the user typed to the controller.
    ///
    /// A CAD user types a caption where the caption goes, not into a bar at the
    /// bottom of the window — and until this existed the prompt reached the
    /// command line's placeholder while focus stayed here, so METİN took its
    /// anchor and then hung waiting for a value nothing could send.
    void openTextEditor(const QPointF& where);
    void closeTextEditor();

    /// The box itself, created on first use and reused after: a `QLineEdit`
    /// parented to the canvas, so it dies with the canvas and needs no separate
    /// lifetime.
    QLineEdit* text_editor_{nullptr};

    /// What `buildGuide` last wrote on the rubber band; see `guideLabelForProbe`.
    std::string guide_label_;

    /// The guide being dragged off a ruler, or `GuideAxis` count for none.
    ///
    /// Dragging from the ruler is the gesture every drafter knows, and it stays a
    /// gesture: the drop dispatches `KILAVUZ`, so nothing here writes to the
    /// document (CLAUDE.md 5.9).
    int dragging_guide_{-1};       ///< -1 none, 0 horizontal, 1 vertical
    int dragging_guide_index_{-1}; ///< the existing guide being moved, or -1 for a new one

    bool panning_{false};
    QPointF pan_anchor_{};
    QPointF cursor_{};
    bool cursor_valid_{false};

    /// The form-field pick under way, if any; see `beginCapture`.
    std::optional<Capture> capture_{};

    /// The print frame's printable aspect, or 0 when no frame is up. See
    /// `beginPrintFrame`.
    double print_aspect_{0.0};

    /// The frame's rectangle in widget pixels: the largest one of the current
    /// aspect that fits the viewport, inset and centred. Empty with no frame.
    QRectF printFrameRect() const;

    /// Draws the frame: the grey mask outside it, its hairline and the four
    /// corner marks.
    void buildPrintFrame();

    /// The platform pointer for a pick: a cross for a point, a pick box for an
    /// object, in the theme's accent. Drawn once per theme and kind.
    QCursor captureCursor() const;

    /// Rubber-band selection gesture. Session state, drawn only (model.md R43).
    bool selecting_{false};
    QPointF select_anchor_{};

    /// A corner of a selected object that the pointer can take hold of.
    ///
    /// Session state and DRAWN ONLY (model.md R43): a grip is a handle on geometry,
    /// never geometry itself. Dragging one changes nothing until the mouse is
    /// released, and what happens then is an ordinary command — `KÖŞETAŞI` or
    /// `KÖŞEEKLE` — dispatched through the bus with the same arguments a script
    /// would send. The canvas gets no private road to the document (Article 1.2).
    struct Grip
    {
        core::EntityId entity{core::kNoEntity};

        /// 1-based, counted across the object's rings in R11 order — the exact
        /// numbering `KÖŞETAŞI` and `KÖŞEEKLE` use, because it IS the argument
        /// they are about to be given.
        std::int64_t corner{0};

        /// The press landed on an edge rather than on a corner, so releasing
        /// creates a corner instead of moving one.
        bool insert{false};

        core::Point2 at{}; ///< where the grip sits now, in document millimetres

        /// What the command will measure direction aids from — its rubber-band
        /// origin. For a corner that is the corner itself; for an edge it is the
        /// corner the edge LEAVES, not the point on the edge that was pressed.
        /// Kept so the snap marker promises what the command will actually do
        /// rather than something close to it.
        core::Point2 base{};

        bool valid() const noexcept { return entity != core::kNoEntity; }
    };

    /// The grip under the pointer, so it can be lit before it is grabbed.
    Grip hover_grip_{};

    /// The grip being dragged, and whether a drag is under way at all.
    Grip drag_grip_{};
    bool dragging_grip_{false};

    /// Vertices the guide contributed to the last overlay build.
    std::size_t guide_vertices_{0};

    /// Scratch for a curve guide, kept so the frame path does not allocate.
    std::vector<core::Mm> curve_scratch_x_;
    std::vector<core::Mm> curve_scratch_y_;

    /// A measurement left on the canvas, and how many document changes the
    /// canvas had seen when it was taken: a mark older than the drawing
    /// describes a drawing that is gone.
    ///
    /// NOT THE REVISION ITSELF. `YENİ` starts the count again, so a new drawing
    /// three edits in carried the revision a measurement of the old one was
    /// taken at, and the old measurement was drawn over the new drawing.
    struct StoredMark
    {
        command::MeasureMark mark;
        std::uint64_t edits{0};
    };

    std::vector<StoredMark> marks_;
    std::uint64_t seen_revision_{0}; ///< the revision `noteDocumentChange` last saw
    std::uint64_t edits_{0};         ///< how many changes it has seen

    /// Where the press landed, so a CLICK on a grip can be told from a DRAG of
    /// one. Without it, taking hold of a corner and letting go without moving
    /// wrote a command that moved the corner onto itself: no visible change, and
    /// an undo step the user has to press Ctrl+Z through to reach the edit they
    /// actually meant to undo.
    QPointF drag_anchor_{};

    /// Finds the grip under a widget-space point, corner before edge: a corner and
    /// the two edges leaving it are all within a few pixels of each other, and a
    /// user aiming at a corner means the corner.
    Grip gripAt(const QPointF& where) const;

private:
    /// Draws the corner handles of every selected object, and the shape a drag
    /// would produce while one is under way.
    void buildGrips();

    /// Sends the drag as a command. Called on release; a drag that never left the
    /// grip sends nothing.
    void commitGripDrag();

    /// The aid that would fire if the user clicked now. A preview, never an input:
    /// the value a click supplies is the raw world point, and the aids are applied
    /// once, inside the command layer, for every client alike.
    core::SnapResult snap_preview_{};
    bool snap_preview_valid_{false};
    /// Ctrl held: the 45 degree lock is on for as long as it is.
    bool diagonal_lock_{false};
    bool normal_lock_{false};

    int last_frame_us_{0};
    int last_scene_us_{0};
    int last_draw_us_{0};
    std::vector<int> scene_costs_; ///< filled beside `timeFrames`
    bool debug_hud_{false};
};

} // namespace kentos::app
