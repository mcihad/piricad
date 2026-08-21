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
    void drawCrosshair(QPainter& painter) const;

    Controller& controller_;
    Palette palette_{themePalette(ThemeMode::Light)};
    render::ViewTransform view_;
    render::DrawList draw_;
    render::SceneOptions options_{};

    bool panning_{false};
    QPointF pan_anchor_{};
    QPointF cursor_{};
    bool cursor_valid_{false};
    int last_frame_us_{0};
    bool debug_hud_{false};
};

} // namespace piricad::app
