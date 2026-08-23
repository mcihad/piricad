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
#include "piricad/core/snap.hpp"
#include "piricad/render/drawlist.hpp"
#include "piricad/render/scene.hpp"
#include "piricad/render/view.hpp"

#include <QWidget>

namespace piricad::app {

/// The one road from a widget to the document; see controller.hpp.
class Controller;

class MapCanvas : public QWidget
{
    Q_OBJECT

public:
    /// Builds the canvas over a controller. The controller outlives it — the main
    /// window owns both — so it is held by reference rather than by pointer.
    explicit MapCanvas(Controller& controller, QWidget* parent = nullptr);

    /// The view transform: centre, scale and the origin offset that keeps a TUREF
    /// coordinate out of a float (§10.3). View state is NOT document state
    /// (model.md R43), which is why it lives here and not in the document.
    render::ViewTransform& view() noexcept { return view_; }

    const render::ViewTransform& view() const noexcept { return view_; }

    /// Re-reads the palette. Called when any client writes the theme preference,
    /// not only when the menu item is toggled.
    void applyTheme(ThemeMode mode);

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
    void drawGrid(QPainter& painter) const;
    void drawSelected(QPainter& painter) const;
    void drawSelectionBox(QPainter& painter) const;
    void drawSnapMarker(QPainter& painter) const;

    /// Re-runs the aid pipeline for the current cursor so the marker on screen is
    /// the point a click would actually produce. Reads the document; never writes.
    void updateSnapPreview();

    /// Turns one finished left-drag into a `SEÇ` invocation. The box, the single
    /// pick and the Shift/Ctrl modifiers all become arguments — there is no
    /// selection path that does not go through the bus (Article 1.2).
    void dispatchSelection(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods);

    /// Grid shape, cached from the preferences so paintEvent does no lookups.
    struct GridSetup
    {
        bool visible{true};
        bool adaptive{true};
        core::Mm step{10000};
        int major{5};
    };

    void drawCrosshair(QPainter& painter) const;

    /// Publishes the view scale to the bus. The snap and pick tolerances are
    /// declared in screen pixels, and turning pixels into millimetres is the one
    /// thing only the view knows (`piricad/command/aids.hpp`).
    void publishViewScale();

    Controller& controller_;
    Palette palette_{themePalette(ThemeMode::Light)};
    render::ViewTransform view_;
    render::DrawList draw_;
    render::SceneOptions options_{};
    GridSetup grid_{};

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
