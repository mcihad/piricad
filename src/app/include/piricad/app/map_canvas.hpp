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
#include "piricad/render/drawlist.hpp"
#include "piricad/render/scene.hpp"
#include "piricad/render/view.hpp"

#include <QWidget>

namespace piricad::app {

class Controller;

class MapCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit MapCanvas(Controller& controller, QWidget* parent = nullptr);

    render::ViewTransform& view() noexcept { return view_; }

    const render::ViewTransform& view() const noexcept { return view_; }

    void applyTheme(ThemeMode mode);

    /// Re-reads the ızgara.* preferences. Called at start-up and whenever any
    /// client writes one — the menu, the command line, a script or the AI, which
    /// is the whole point of routing the write through the bus (CLAUDE.md 1.2).
    void reloadGridSettings();
    void setDebugHud(bool on);
    void zoomToExtents();
    void zoomBy(double factor);
    void resetView();

    QString backendName() const;

signals:
    void cursorMoved(core::Point2 world);
    void viewChanged();

protected:
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

    /// Grid shape, cached from the preferences so paintEvent does no lookups.
    struct GridSetup
    {
        bool visible{true};
        bool adaptive{true};
        core::Mm step{10000};
        int major{5};
    };

    void drawCrosshair(QPainter& painter) const;

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
    int last_frame_us_{0};
    bool debug_hud_{false};
};

} // namespace piricad::app
