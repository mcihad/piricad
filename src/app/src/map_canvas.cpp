// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/map_canvas.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/render/backend.hpp"

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <string>

namespace piricad::app {

MapCanvas::MapCanvas(Controller& controller, QWidget* parent)
    : QWidget(parent), controller_(controller), backend_(make_canvas_backend())
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
    return QString::fromStdString(backend_->name());
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

void MapCanvas::buildGrid()
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

    // Two batches, minor first: the darker lines are drawn over the lighter ones
    // so a major line stays a major line where they cross.
    render::OverlayBatch& minor = nextBatch(palette_.grid.rgba(), 1.0f, false);
    render::OverlayBatch& major = nextBatch(palette_.gridMajor.rgba(), 1.0f, false);

    const auto h = static_cast<float>(height());
    const auto w = static_cast<float>(width());

    const long long last_x = line_index(vis.max_x);
    for (long long i = line_index(vis.min_x); i <= last_x; ++i) {
        const auto wx  = static_cast<core::Mm>(position(i));
        const float sx = render::to_f(view_.to_screen(core::Point2{wx, vis.min_y})).x;
        addRun(is_major(i) ? major : minor, {{sx, 0.0f}, {sx, h}}, false);
    }

    const long long last_y = line_index(vis.max_y);
    for (long long i = line_index(vis.min_y); i <= last_y; ++i) {
        const auto wy  = static_cast<core::Mm>(position(i));
        const float sy = render::to_f(view_.to_screen(core::Point2{vis.min_x, wy})).y;
        addRun(is_major(i) ? major : minor, {{0.0f, sy}, {w, sy}}, false);
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
    // declared `seçim toleransı`, so the mouse obeys the same preference the command
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

void MapCanvas::buildSelection()
{
    // Not named `slots`: Qt defines that as a macro (qobjectdefs.h).
    const auto& selected = controller_.selectedSlots();
    if (selected.empty()) return;

    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();

    render::OverlayBatch& batch = nextBatch(palette_.selection.rgba(), 3.0f, false);

    // Walked per selected entity, never per document entity: a selection is
    // O(hundreds) and the frame budget belongs to the drawing (§10.1).
    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;

        const core::RingSpan span = geom.rings_of(table.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);
            if (xs.size() < 2) continue;

            const auto before = static_cast<std::uint32_t>(batch.xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v) {
                const render::ScreenPointF q =
                    render::to_f(view_.to_screen(core::Point2{xs[v], ys[v]}));
                batch.xs.push_back(q.x);
                batch.ys.push_back(q.y);
            }
            batch.runs.push_back(static_cast<std::uint32_t>(batch.xs.size()) - before);
            batch.closed.push_back(geom.ring_role[r] != core::RingRole::Open ? 1 : 0);
        }
    }
}

void MapCanvas::buildSelectionBox()
{
    if (!selecting_ || !cursor_valid_) return;

    // Left to right is PENCERE (solid outline, what is wholly inside); right to
    // left is KESEN (dashed, whatever the box touches). The two look different on
    // screen because they behave differently, and every CAD user reads that shape
    // before they read any label.
    const bool crossing = cursor_.x() < select_anchor_.x();
    QColor line         = crossing ? palette_.selectCross : palette_.selectWindow;
    QColor fill         = line;
    fill.setAlpha(38);

    render::OverlayBatch& batch = nextBatch(line.rgba(), 1.0f, crossing, fill.rgba());

    const render::ScreenPointF a = toScreenF(select_anchor_);
    const render::ScreenPointF b = toScreenF(cursor_);
    const float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    addRun(batch, {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, true);
}

void MapCanvas::buildSnapMarker()
{
    if (!snap_preview_valid_) return;

    const render::ScreenPointF p = render::to_f(view_.to_screen(snap_preview_.point));
    const float x                = p.x;
    const float y                = p.y;
    const float h                = 6.0f; // half size, layout units

    render::OverlayBatch& batch = nextBatch(palette_.snapMarker.rgba(), 1.8f, false);

    // One glyph per mode, the shapes CAD users already read without a legend. The
    // SHAPE is chosen here rather than in the backend because which glyph means
    // which aid is a decision about the product, and every backend would otherwise
    // have to make the same one and agree.
    switch (snap_preview_.mode) {
    case core::SnapEndpoint: // square
        addRun(batch, {{x - h, y - h}, {x + h, y - h}, {x + h, y + h}, {x - h, y + h}}, true);
        break;
    case core::SnapMidpoint: // triangle
        addRun(batch, {{x, y - h}, {x + h, y + h}, {x - h, y + h}}, true);
        break;
    case core::SnapCenter: addCircle(batch, x, y, h); break;
    case core::SnapIntersection: // cross
        addRun(batch, {{x - h, y - h}, {x + h, y + h}}, false);
        addRun(batch, {{x - h, y + h}, {x + h, y - h}}, false);
        break;
    case core::SnapPerpendicular: // the right-angle mark
        addRun(batch, {{x - h, y - h}, {x - h, y + h}, {x + h, y + h}}, false);
        addRun(batch, {{x, y + h}, {x, y}, {x - h, y}}, false);
        break;
    case core::SnapNearest: // bowtie
        addRun(batch, {{x - h, y - h}, {x + h, y - h}, {x - h, y + h}, {x + h, y + h}}, true);
        break;
    case core::SnapGrid: // lattice cell with its centre marked
        addRun(batch, {{x - h, y}, {x + h, y}}, false);
        addRun(batch, {{x, y - h}, {x, y + h}}, false);
        addRun(batch, {{x - h, y - h}, {x + h, y - h}, {x + h, y + h}, {x - h, y + h}}, true);
        break;
    case core::SnapPolar:
    case core::SnapOrtho: // diamond: the point is on a locked direction
        addRun(batch, {{x, y - h}, {x + h, y}, {x, y + h}, {x - h, y}}, true);
        break;
    default: break;
    }

    // The label says which aid fired. Without it a user cannot tell an endpoint
    // from an intersection when both glyphs sit under the cursor.
    overlay_.labels.push_back(
        render::OverlayLabel{palette_.snapMarker.rgba(), x + h + 4.0f, y - h - 2.0f, 0.0f,
                             std::string(core::snap_mode_label(snap_preview_.mode))});
}

void MapCanvas::buildCrosshair()
{
    if (!cursor_valid_) return;

    render::OverlayBatch& batch  = nextBatch(palette_.crosshair.rgba(), 1.0f, false);
    const render::ScreenPointF c = toScreenF(cursor_);
    const float x = c.x, y = c.y;
    addRun(batch, {{x, 0.0f}, {x, static_cast<float>(height())}}, false);
    addRun(batch, {{0.0f, y}, {static_cast<float>(width()), y}}, false);
}

render::OverlayBatch& MapCanvas::nextBatch(std::uint32_t rgba, float width_px, bool dashed,
                                           std::uint32_t fill_rgba)
{
    // Reuses the batch this position held on the previous frame, buffers and all.
    // The overlay is rebuilt on EVERY MOUSE MOVE, so allocating here would allocate
    // on every mouse move (render.md R20, P6).
    if (overlay_used_ == overlay_.batches.size()) overlay_.batches.emplace_back();

    render::OverlayBatch& batch = overlay_.batches[overlay_used_++];
    batch.rgba                  = rgba;
    batch.fill_rgba             = fill_rgba;
    batch.width_px              = width_px;
    batch.dashed                = dashed;
    return batch;
}

render::ScreenPointF MapCanvas::toScreenF(const QPointF& p)
{
    // A widget-space QPointF is already a pixel — a cursor position, a drag
    // anchor. It goes through the same one narrowing as everything else so there
    // is exactly one place in the canvas where a coordinate becomes a float.
    return render::to_f(render::ScreenPoint{p.x(), p.y()});
}

void MapCanvas::addRun(render::OverlayBatch& batch,
                       std::initializer_list<render::ScreenPointF> points, bool closed)
{
    for (const render::ScreenPointF& p : points) {
        batch.xs.push_back(p.x);
        batch.ys.push_back(p.y);
    }
    batch.runs.push_back(static_cast<std::uint32_t>(points.size()));
    batch.closed.push_back(closed ? 1 : 0);
}

void MapCanvas::addCircle(render::OverlayBatch& batch, float cx, float cy, float radius)
{
    // A polygon, not a circle primitive: the overlay carries runs of points and
    // nothing else, so every backend draws the same shape without needing an
    // ellipse call of its own. Twenty-four segments is smooth at the six-pixel
    // radius this is used at and is not worth making adaptive.
    constexpr int kSegments = 24;
    const auto before       = static_cast<std::uint32_t>(batch.xs.size());
    for (int i = 0; i < kSegments; ++i) {
        const double a = 2.0 * M_PI * i / kSegments;
        batch.xs.push_back(cx + radius * static_cast<float>(std::cos(a)));
        batch.ys.push_back(cy + radius * static_cast<float>(std::sin(a)));
    }
    batch.runs.push_back(static_cast<std::uint32_t>(batch.xs.size()) - before);
    batch.closed.push_back(1);
}

void MapCanvas::buildOverlay()
{
    overlay_.clear();
    overlay_used_            = 0;
    overlay_.background_rgba = palette_.canvas.rgba();

    buildGrid();
    buildSelection();

    // Rubber band for the running interactive command. It runs to the SNAPPED
    // point when an aid has fired, because that is where the segment will land.
    if (auto* session = controller_.session();
        session && session->waiting() && session->prompt().has_rubber_band && cursor_valid_) {
        const auto from = view_.to_screen(session->prompt().rubber_origin);

        QPointF to = cursor_;
        if (snap_preview_valid_) {
            const auto snapped = view_.to_screen(snap_preview_.point);
            to                 = QPointF(snapped.x, snapped.y);
        }

        render::OverlayBatch& batch = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);
        addRun(batch, {render::to_f(from), toScreenF(to)}, false);
    }

    buildSelectionBox();
    buildCrosshair();
    buildSnapMarker();

    // Developer HUD. Dear ImGui replaces this once the GPU canvas lands; it is a
    // debug layer and never a user-facing feature (piricad.md §6.3), so it is off
    // unless the developer asks for it.
    if (!debug_hud_) return;

    overlay_.labels.push_back(
        render::OverlayLabel{palette_.hud.rgba(), 8.0f, 22.0f, 0.0f,
                             QStringLiteral("%1  |  %2 nesne  |  %3 tepe  |  %4 elenen  |  %5 µs")
                                 .arg(backendName())
                                 .arg(draw_.entity_count)
                                 .arg(draw_.vertex_count)
                                 .arg(draw_.culled_count)
                                 .arg(last_frame_us_)
                                 .toStdString()});
}

void MapCanvas::paintEvent(QPaintEvent*)
{
    QElapsedTimer timer;
    timer.start();

    rebuildScene();
    buildOverlay();

    // Everything below this line is the backend's. This widget knows WHAT is on
    // screen; it does not know how any of it is drawn, which is what lets the GPU
    // backend replace the painter one without touching this file (render.md R1,
    // CLAUDE.md Article 8.1).
    render::FrameContext ctx;
    ctx.width_px           = width();
    ctx.height_px          = height();
    ctx.device_pixel_ratio = static_cast<float>(devicePixelRatioF());
    ctx.target             = this;

    backend_->render(draw_, overlay_, ctx);

    last_frame_us_ = static_cast<int>(timer.nsecsElapsed() / 1000);
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
