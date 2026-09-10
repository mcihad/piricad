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
#include "kentos_cad/core/snap.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/scene.hpp"
#include "kentos_cad/render/view.hpp"

#include <QImage>
#include <QRectF>

#if KENTOS_HAVE_RHI
#include <QRhiWidget>
#else
#include <QWidget>
#endif

#include <initializer_list>
#include <memory>
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
    void zoomBy(double factor);

    /// Moves the view's centre without changing its scale. KAYDIR's landing point.
    void setCentre(core::Point2 centre);
    void resetView();

    QString backendName() const;

    /// Whether the live backend draws on the GPU. The draw-call budget is only
    /// meaningful where it does; see `render::FrameStats::draw_calls`.
    bool backendIsGpu() const;

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

    /// Appends one run of document points, shifted by `(dx, dy)`, to an overlay batch.
    void addWorldRun(std::size_t batch, std::span<const core::Mm> xs, std::span<const core::Mm> ys,
                     bool closed, core::Mm dx, core::Mm dy);

    /// Appends every run of an outline buffer, shifted, to an overlay batch.
    void addEmitRuns(std::size_t batch, const core::EmitBuffer& buf, core::Mm dx, core::Mm dy);

    /// The selected objects' outlines shifted by `(dx, dy)`: the ghost TAŞI and
    /// KOPYALA carry under the cursor.
    void addGhost(std::size_t batch, core::Mm dx, core::Mm dy);

    /// Draws the drafting guides across the whole canvas, under everything else.
    ///
    /// Under, because a guide is furniture: it must never sit on top of the
    /// drawing it is there to help place. Dashed and in the aid colour, so it
    /// cannot be mistaken for a line the plot will print.
    void buildGuides();
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

        /// The angle unit the reading is written in: 0 grad, 1 degree, 2 radian.
        /// GRAD is the default because Turkish traverse, triangulation and
        /// setting-out arithmetic is done in grad — a full circle is 400.
        int angle_unit{0};

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
