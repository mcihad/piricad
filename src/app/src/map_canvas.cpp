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
    publishViewScale();
}

void MapCanvas::publishViewScale()
{
    // The only number the aid layer cannot work out for itself. Everything else
    // about snapping — modes, ortho, polar step, grid — lives in the settings and
    // is readable by every client (piricad/command/aids.hpp).
    controller_.bus().aids().set_view_scale(view_.mm_per_pixel());
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
    publishViewScale();
    emit viewChanged();
    update();
}

void MapCanvas::zoomBy(double factor)
{
    view_.zoom_at(render::ScreenPoint{width() * 0.5, height() * 0.5}, factor);
    publishViewScale();
    emit viewChanged();
    update();
}

void MapCanvas::resetView()
{
    view_.set_centre(core::Point2{485350000, 4310235000}, 40.0);
    publishViewScale();
    emit viewChanged();
    update();
}

void MapCanvas::resizeEvent(QResizeEvent* event)
{
    view_.set_viewport(width(), height());
    publishViewScale();
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

    const auto first = [step_mm](core::Mm v) {
        return std::floor(static_cast<double>(v) / step_mm) * step_mm;
    };

    // Which lines are major is decided in world coordinates, not by counting from
    // the left edge: counting from the edge would make the dark lines crawl as the
    // user pans, and a grid whose emphasis moves is worse than one without any.
    const auto is_major = [this, step_mm](double world) {
        const double index = std::floor(world / step_mm + 0.5);
        const auto n       = static_cast<long long>(index);
        const auto m       = static_cast<long long>(grid_.major);
        return ((n % m) + m) % m == 0;
    };

    for (double x = first(vis.min_x); x <= static_cast<double>(vis.max_x); x += step_mm) {
        painter.setPen(is_major(x) ? major : minor);
        const double sx = view_.to_screen(core::Point2{static_cast<core::Mm>(x), vis.min_y}).x;
        painter.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }
    for (double y = first(vis.min_y); y <= static_cast<double>(vis.max_y); y += step_mm) {
        painter.setPen(is_major(y) ? major : minor);
        const double sy = view_.to_screen(core::Point2{vis.min_x, static_cast<core::Mm>(y)}).y;
        painter.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }
}

void MapCanvas::reloadSnapSettings()
{
    updateSnapPreview();
    update();
}

void MapCanvas::updateSnapPreview()
{
    snap_preview_valid_ = false;
    if (!cursor_valid_) return;

    // The marker is only meaningful while a command is asking for a point: it
    // promises "click here and this is what you get", and there is nothing to
    // promise when nothing is being drawn.
    command::Session* session = controller_.session();
    if (!session || !session->waiting()) return;

    command::Bus& bus                = controller_.bus();
    const command::AidSettings& aids = bus.aid_settings();
    const core::Point2 aim = view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    const command::Prompt& prompt = session->prompt();
    const core::SnapResult r      = bus.aids().resolve(controller_.document(), aids, aim,
                                                       prompt.has_rubber_band, prompt.rubber_origin);
    if (r.mode == core::SnapNone) return;

    snap_preview_       = r;
    snap_preview_valid_ = true;
}

void MapCanvas::dispatchSelection(const QPointF& from, const QPointF& to,
                                  Qt::KeyboardModifiers mods)
{
    const core::Point2 a = view_.to_world(render::ScreenPoint{from.x(), from.y()});
    const core::Point2 b = view_.to_world(render::ScreenPoint{to.x(), to.y()});

    // A drag shorter than the pick box is a click, not a box. The threshold is the
    // declared seçim toleransı, so the mouse obeys the same preference the command
    // line does rather than a number invented here.
    const double slack =
        static_cast<double>(controller_.bus().app_settings().get("core.secim.tolerans").as_int());
    const bool is_box = std::abs(to.x() - from.x()) > slack || std::abs(to.y() - from.y()) > slack;

    command::Args args;
    args.set("mod", command::Value::text(is_box ? "KUTU" : "NOKTA"));
    args.set("noktalar", is_box ? command::Value::points({a, b}) : command::Value::points({a}));

    // QGIS keys, because that is where the CBS half of this product's users come
    // from: Shift adds, Ctrl removes, a plain click replaces.
    if (mods.testFlag(Qt::ShiftModifier))
        args.set("islem", command::Value::text("EKLE"));
    else if (mods.testFlag(Qt::ControlModifier))
        args.set("islem", command::Value::text("ÇIKAR"));

    controller_.runInvocation(
        command::Invocation{"core.select", std::move(args), command::Origin::Gui});
}

void MapCanvas::drawSelected(QPainter& painter) const
{
    // Not named `slots`: Qt defines that as a macro (qobjectdefs.h).
    const auto& selected = controller_.selectedSlots();
    if (selected.empty()) return;

    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();

    QPen pen(palette_.selection);
    pen.setWidthF(3.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    // Walked per selected entity, never per document entity: a selection is
    // O(hundreds) and the frame budget belongs to the drawing (§10.1).
    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;

        const core::RingSpan span = geom.rings_of(table.slot[e]);
        QPainterPath path;
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            if (xs.size() < 2) continue;

            const auto at = [&](std::size_t v) {
                const auto q = view_.to_screen(core::Point2{xs[v], ys[v]});
                return QPointF(q.x, q.y);
            };
            path.moveTo(at(0));
            for (std::size_t v = 1; v < xs.size(); ++v)
                path.lineTo(at(v));
            if (geom.ring_role[r] != core::RingRole::Open) path.closeSubpath();
        }
        painter.drawPath(path);
    }
}

void MapCanvas::drawSelectionBox(QPainter& painter) const
{
    if (!selecting_ || !cursor_valid_) return;

    // Left to right is PENCERE (solid outline, what is wholly inside); right to
    // left is KESEN (dashed, whatever the box touches). The two look different on
    // screen because they behave differently, and every CAD user reads that shape
    // before they read any label.
    const bool crossing = cursor_.x() < select_anchor_.x();

    QPen pen(crossing ? palette_.selectCross : palette_.selectWindow);
    pen.setWidth(1);
    pen.setStyle(crossing ? Qt::DashLine : Qt::SolidLine);
    painter.setPen(pen);

    QColor fill = crossing ? palette_.selectCross : palette_.selectWindow;
    fill.setAlpha(38);
    painter.setBrush(fill);
    painter.drawRect(QRectF(select_anchor_, cursor_).normalized());
    painter.setBrush(Qt::NoBrush);
}

void MapCanvas::drawSnapMarker(QPainter& painter) const
{
    if (!snap_preview_valid_) return;

    const auto p = view_.to_screen(snap_preview_.point);
    const QPointF at(p.x, p.y);
    const double h = 6.0; // half size, layout units

    QPen pen(palette_.snapMarker);
    pen.setWidthF(1.8);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // One glyph per mode, the shapes CAD users already read without a legend.
    switch (snap_preview_.mode) {
    case core::SnapEndpoint: // square
        painter.drawRect(QRectF(at.x() - h, at.y() - h, 2 * h, 2 * h));
        break;
    case core::SnapMidpoint: { // triangle
        QPainterPath tri;
        tri.moveTo(at.x(), at.y() - h);
        tri.lineTo(at.x() + h, at.y() + h);
        tri.lineTo(at.x() - h, at.y() + h);
        tri.closeSubpath();
        painter.drawPath(tri);
        break;
    }
    case core::SnapCenter: // circle
        painter.drawEllipse(at, h, h);
        break;
    case core::SnapIntersection: // cross
        painter.drawLine(QPointF(at.x() - h, at.y() - h), QPointF(at.x() + h, at.y() + h));
        painter.drawLine(QPointF(at.x() - h, at.y() + h), QPointF(at.x() + h, at.y() - h));
        break;
    case core::SnapPerpendicular: // the right-angle mark
        painter.drawLine(QPointF(at.x() - h, at.y() - h), QPointF(at.x() - h, at.y() + h));
        painter.drawLine(QPointF(at.x() - h, at.y() + h), QPointF(at.x() + h, at.y() + h));
        painter.drawLine(QPointF(at.x(), at.y() + h), QPointF(at.x(), at.y()));
        painter.drawLine(QPointF(at.x(), at.y()), QPointF(at.x() - h, at.y()));
        break;
    case core::SnapNearest: { // bowtie
        QPainterPath bow;
        bow.moveTo(at.x() - h, at.y() - h);
        bow.lineTo(at.x() + h, at.y() - h);
        bow.lineTo(at.x() - h, at.y() + h);
        bow.lineTo(at.x() + h, at.y() + h);
        bow.closeSubpath();
        painter.drawPath(bow);
        break;
    }
    case core::SnapGrid: // lattice cell with its centre marked
        painter.drawLine(QPointF(at.x() - h, at.y()), QPointF(at.x() + h, at.y()));
        painter.drawLine(QPointF(at.x(), at.y() - h), QPointF(at.x(), at.y() + h));
        painter.drawRect(QRectF(at.x() - h, at.y() - h, 2 * h, 2 * h));
        break;
    case core::SnapPolar:
    case core::SnapOrtho: { // diamond: the point is on a locked direction
        QPainterPath diamond;
        diamond.moveTo(at.x(), at.y() - h);
        diamond.lineTo(at.x() + h, at.y());
        diamond.lineTo(at.x(), at.y() + h);
        diamond.lineTo(at.x() - h, at.y());
        diamond.closeSubpath();
        painter.drawPath(diamond);
        break;
    }
    default: break;
    }

    // The label says which aid fired. Without it a user cannot tell an endpoint
    // from an intersection when both glyphs sit under the cursor.
    painter.setPen(palette_.snapMarker);
    painter.drawText(QPointF(at.x() + h + 4.0, at.y() - h - 2.0),
                     QString::fromUtf8(core::snap_mode_label(snap_preview_.mode)));
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

    drawSelected(painter);

    // Rubber band for the running interactive command. It runs to the SNAPPED
    // point when an aid has fired, because that is where the segment will land.
    if (auto* s = controller_.session();
        s && s->waiting() && s->prompt().has_rubber_band && cursor_valid_) {
        const auto a = view_.to_screen(s->prompt().rubber_origin);
        QPen pen(palette_.rubberBand);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(1);
        painter.setPen(pen);

        QPointF to = cursor_;
        if (snap_preview_valid_) {
            const auto b = view_.to_screen(snap_preview_.point);
            to           = QPointF(b.x, b.y);
        }
        painter.drawLine(QPointF(a.x, a.y), to);
    }

    drawSelectionBox(painter);
    drawCrosshair(painter);
    drawSnapMarker(painter);

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
        if (controller_.awaitingInput()) {
            // A click is one input value for the running command, and it is the RAW
            // world point. Snapping is not applied here: it happens once, inside
            // the command layer, on the path every client takes (piricad.md §2.4,
            // piricad/command/aids.hpp). A canvas that snapped first would be a
            // client with a private route.
            const core::Point2 world =
                view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
            controller_.supplyPoint(world);
            snap_preview_valid_ = false;
            update();
            return;
        }

        // No command is asking for a point, so the drag is a selection.
        selecting_     = true;
        select_anchor_ = event->position();
        cursor_        = event->position();
        cursor_valid_  = true;
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

    updateSnapPreview();

    emit cursorMoved(view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()}));
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        unsetCursor();
        return;
    }

    if (event->button() == Qt::LeftButton && selecting_) {
        selecting_ = false;
        dispatchSelection(select_anchor_, event->position(), event->modifiers());
        update();
    }
}

void MapCanvas::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) return;

    view_.zoom_at(render::ScreenPoint{event->position().x(), event->position().y()},
                  std::pow(1.2, steps));
    publishViewScale();
    updateSnapPreview();
    emit viewChanged();
    update();
}

void MapCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (selecting_) {
            selecting_ = false;
            update();
            return;
        }
        if (controller_.session()) {
            controller_.cancelInteractive();
            snap_preview_valid_ = false;
            update();
            return;
        }
        // Nothing running: ESC clears the selection, and it does so by sending the
        // command, not by reaching into the bus (Article 1.2).
        if (!controller_.bus().selection().empty()) {
            command::Args args;
            args.set("mod", command::Value::text("TEMİZLE"));
            controller_.runInvocation(
                command::Invocation{"core.select", std::move(args), command::Origin::Gui});
        }
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace piricad::app
