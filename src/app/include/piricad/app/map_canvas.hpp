// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the map canvas.
//
// Target (piricad.md §6.3): QRhiWidget with our own GPU pipeline.
// Phase 0 (CLAUDE.md Article 8): a QPainter backend behind render::Backend,
// because `qsb` is unavailable and shader packs cannot be baked. The scene is
// already built by piricad_render in exactly the form the GPU path needs —
// screen-space floats produced after the origin offset (§10.3) — so replacing the
// backend touches this file only.
#pragma once

#include "piricad/app/theme.hpp"
#include "piricad/app/tokens.hpp"
#include "piricad/core/snap.hpp"
#include "piricad/render/backend.hpp"
#include "piricad/render/drawlist.hpp"
#include "piricad/render/scene.hpp"
#include "piricad/render/view.hpp"

#include <QRectF>
#include <QWidget>

#include <initializer_list>
#include <memory>

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

class MapCanvas : public QWidget, public Themed
{
    Q_OBJECT
    Q_INTERFACES(piricad::app::Themed)

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
    void zoomToExtents();
    void zoomBy(double factor);
    void resetView();

    QString backendName() const;

signals:
    /// Emitted as the pointer moves, in DOCUMENT coordinates. The status bar
    /// labels them `sağa değer` (Y) and `yukarı değer` (X), which is the Turkish
    /// convention and the reverse of the member names (model.md R37a).
    void cursorMoved(core::Point2 world);

    /// Emitted after a pan or a zoom, so the scale readout can follow.
    void viewChanged();

protected:
    /// Qt event handlers. Every one of them either changes the VIEW — which is
    /// not document state — or feeds a point to the running command through the
    /// controller. None of them edits the document, because a mouse is a client
    /// like any other and gets no private road (Article 1.2, 5.9).
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

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
        bool ruler{true};     ///< the two scales along the top and the left
        int ruler_px{22};     ///< their thickness
        int ruler_unit{0};    ///< 0 metre, 1 santimetre, 2 kilometre
        bool scale_bar{true}; ///< the bar that says what the zoom means
        bool north{true};     ///< the north arrow
        bool readout{true};   ///< the cursor's own easting and northing
        int cursor{0};        ///< 0 full screen, 1 short, 2 none
        int cursor_px{30};    ///< arm length of the short cursor
        int zoom_percent{20}; ///< how much one wheel notch changes the scale
        bool invert_wheel{false};
        int marker_px{12};                ///< half size of the snap marker
        bool snap_tip{true};              ///< name the mode beside the marker
        std::uint32_t marker_rgba{0};     ///< 0 = take the theme's own colour
        std::uint32_t grid_rgba{0};       ///< 0 = the theme's
        std::uint32_t grid_major_rgba{0}; ///< 0 = the theme's
        std::uint32_t selection_rgba{0};  ///< 0 = the theme's
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
    /// thing only the view knows (`piricad/command/aids.hpp`).
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

    bool panning_{false};
    QPointF pan_anchor_{};
    QPointF cursor_{};
    bool cursor_valid_{false};

    /// Rubber-band selection gesture. Session state, drawn only (model.md R43).
    bool selecting_{false};
    QPointF select_anchor_{};

    /// The aid that would fire if the user clicked now. A preview, never an input:
    /// the value a click supplies is the raw world point, and the aids are applied
    /// once, inside the command layer, for every client alike.
    core::SnapResult snap_preview_{};
    bool snap_preview_valid_{false};
    int last_frame_us_{0};
    bool debug_hud_{false};
};

} // namespace piricad::app
