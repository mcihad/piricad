// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/map_canvas.hpp"

#include "piricad/app/controller.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/render/backend.hpp"

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

namespace piricad::app {
namespace {

QColor from_rgba(std::uint32_t rgba)
{
    return QColor::fromRgba(static_cast<QRgb>(rgba));
}

} // namespace

MapCanvas::MapCanvas(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(320, 240);

    view_.set_viewport(width(), height());
    view_.set_centre(core::Point2{485350000, 4310235000}, 40.0); // TUREF/TM30, 30. dilim

    reloadGridSettings();
}

void MapCanvas::setDebugHud(bool on)
{
    debug_hud_ = on;
    update();
}

void MapCanvas::applyTheme(ThemeMode mode)
{
    palette_ = themePalette(mode);
    update();
}

QString MapCanvas::backendName() const
{
    return render::gpu_backend_status().empty() ? QStringLiteral("QRhi (GPU)")
                                                : QStringLiteral("QPainter (Faz 0)");
}

void MapCanvas::zoomToExtents()
{
    const core::Box2 box = controller_.document().extent();
    if (box.empty()) {
        resetView();
        return;
    }
    view_.fit(box, 0.08);
    emit viewChanged();
    update();
}

void MapCanvas::zoomBy(double factor)
{
    view_.zoom_at(render::ScreenPoint{width() * 0.5, height() * 0.5}, factor);
    emit viewChanged();
    update();
}

void MapCanvas::resetView()
{
    view_.set_centre(core::Point2{485350000, 4310235000}, 40.0);
    emit viewChanged();
    update();
}

void MapCanvas::resizeEvent(QResizeEvent* event)
{
    view_.set_viewport(width(), height());
    QWidget::resizeEvent(event);
    emit viewChanged();
}

void MapCanvas::rebuildScene()
{
    render::build_scene(controller_.document(), view_, options_, draw_);
}

void MapCanvas::reloadGridSettings()
{
    const core::Settings& store = controller_.bus().app_settings();

    grid_.visible  = store.get("core.izgara.gorunur").as_bool();
    grid_.adaptive = store.get("core.izgara.mod").as_enum() == 0;
    grid_.step     = store.get("core.izgara.adim").as_length();
    grid_.major    = static_cast<int>(store.get("core.izgara.ana_cizgi").as_int());

    // The declared ranges already exclude zero, and R42 clamps a file written by
    // another version into range. The floors here are the last line: a zero step
    // is a non-terminating loop below and a zero major interval is a division by
    // zero, and neither may depend on a catalogue staying correct.
    if (grid_.step < 1) grid_.step = 1;
    if (grid_.major < 1) grid_.major = 1;
}

void MapCanvas::drawGrid(QPainter& painter) const
{
    if (!grid_.visible) return;

    double step_mm = static_cast<double>(grid_.step);
    if (grid_.adaptive) {
        // Adaptive spacing snaps to 1/2/5 x 10^n metres so the label stays readable.
        const double target_px = 90.0;
        step_mm                = target_px * view_.mm_per_pixel();
        const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(step_mm, 1.0))));
        const double norm      = step_mm / magnitude;
        step_mm                = (norm < 2.0 ? 1.0 : norm < 5.0 ? 2.0 : 5.0) * magnitude;
    }
    if (step_mm < 1.0) return;

    // A fixed step the user chose is still subject to the screen: below a couple of
    // pixels the lines merge into a flat wash that hides the drawing. Refusing to
    // draw is the honest answer; silently substituting another step would make the
    // grid lie about the distance it represents.
    if (step_mm / view_.mm_per_pixel() < 2.0) return;

    const core::Box2 vis = view_.visible_box();
    if (vis.empty()) return;

    QPen minor(palette_.grid);
    minor.setWidth(1);
    QPen major(palette_.gridMajor);
    major.setWidth(1);

    // The loop counts grid lines; it does not accumulate a position. The `x += step`
    // version it replaces was in fact exact for every step this code can produce —
    // measured over 200000 steps at a TUREF northing, the running sum never left
    // the closed form — because both the origin and the step are integers far below
    // 2^53. That is the problem with it: it was correct by an argument about the
    // inputs, not by construction, and the argument stops holding the moment a
    // fractional step arrives. Counting is exact without needing the argument.
    const auto line_index = [step_mm](core::Mm v) {
        return static_cast<long long>(std::floor(static_cast<double>(v) / step_mm));
    };
    const auto position = [step_mm](long long index) {
        return static_cast<double>(index) * step_mm;
    };

    // Which lines are major is decided by the index, not by counting from the left
    // edge: counting from the edge would make the dark lines crawl as the user
    // pans, and a grid whose emphasis moves is worse than one without any.
    const auto m        = static_cast<long long>(grid_.major);
    const auto is_major = [m](long long index) { return ((index % m) + m) % m == 0; };

    const long long last_x = line_index(vis.max_x);
    for (long long i = line_index(vis.min_x); i <= last_x; ++i) {
        painter.setPen(is_major(i) ? major : minor);
        const auto wx   = static_cast<core::Mm>(position(i));
        const double sx = view_.to_screen(core::Point2{wx, vis.min_y}).x;
        painter.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }

    const long long last_y = line_index(vis.max_y);
    for (long long i = line_index(vis.min_y); i <= last_y; ++i) {
        painter.setPen(is_major(i) ? major : minor);
        const auto wy   = static_cast<core::Mm>(position(i));
        const double sy = view_.to_screen(core::Point2{vis.min_x, wy}).y;
        painter.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }
}

void MapCanvas::drawCrosshair(QPainter& painter) const
{
    if (!cursor_valid_) return;

    QPen pen(palette_.crosshair);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawLine(QPointF(cursor_.x(), 0), QPointF(cursor_.x(), height()));
    painter.drawLine(QPointF(0, cursor_.y()), QPointF(width(), cursor_.y()));
}

void MapCanvas::paintEvent(QPaintEvent*)
{
    QElapsedTimer timer;
    timer.start();

    rebuildScene();

    QPainter painter(this);
    painter.fillRect(rect(), palette_.canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    drawGrid(painter);

    const double cx = width() * 0.5;
    const double cy = height() * 0.5;

    for (const auto& batch : draw_.polylines) {
        if (batch.runs.empty()) continue;

        QPen pen(from_rgba(batch.rgba));
        pen.setWidthF(batch.width_px);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);

        std::size_t offset = 0;
        QPainterPath path;
        for (std::uint32_t run : batch.runs) {
            path.moveTo(cx + static_cast<double>(batch.xs[offset]),
                        cy - static_cast<double>(batch.ys[offset]));
            for (std::uint32_t v = 1; v < run; ++v)
                path.lineTo(cx + static_cast<double>(batch.xs[offset + v]),
                            cy - static_cast<double>(batch.ys[offset + v]));
            offset += run;
        }
        painter.drawPath(path);
    }

    // Rubber band for the running interactive command.
    if (auto* s = controller_.session();
        s && s->waiting() && s->prompt().has_rubber_band && cursor_valid_) {
        const auto a = view_.to_screen(s->prompt().rubber_origin);
        QPen pen(palette_.rubberBand);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawLine(QPointF(a.x, a.y), cursor_);
    }

    drawCrosshair(painter);

    last_frame_us_ = static_cast<int>(timer.nsecsElapsed() / 1000);

    // Developer HUD. Dear ImGui replaces this once the GPU canvas lands; it is a
    // debug layer and never a user-facing feature (piricad.md §6.3), so it is off
    // unless the developer asks for it.
    if (!debug_hud_) return;

    painter.setPen(palette_.hud);
    painter.drawText(QRect(8, 8, width() - 16, 60), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("%1  |  %2 nesne  |  %3 tepe  |  %4 elenen  |  %5 µs")
                         .arg(backendName())
                         .arg(draw_.entity_count)
                         .arg(draw_.vertex_count)
                         .arg(draw_.culled_count)
                         .arg(last_frame_us_));
}

void MapCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_    = true;
        pan_anchor_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // A click is one input value for the running command. Where that value
        // came from is invisible to the command body (piricad.md §2.4).
        const core::Point2 world =
            view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
        controller_.supplyPoint(world);
        update();
        return;
    }

    if (event->button() == Qt::RightButton) {
        controller_.cancelInteractive();
        update();
    }
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event)
{
    cursor_       = event->position();
    cursor_valid_ = true;

    if (panning_) {
        const QPointF delta = event->position() - pan_anchor_;
        view_.pan_pixels(delta.x(), delta.y());
        pan_anchor_ = event->position();
        emit viewChanged();
    }

    emit cursorMoved(view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()}));
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        unsetCursor();
    }
}

void MapCanvas::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) return;

    view_.zoom_at(render::ScreenPoint{event->position().x(), event->position().y()},
                  std::pow(1.2, steps));
    emit viewChanged();
    update();
}

void MapCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        controller_.cancelInteractive();
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace piricad::app
