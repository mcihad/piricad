// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/map_canvas.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/core/arc.hpp"
#include "piricad/core/circle.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/render/backend.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShortcut>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QScreen>
#include <QWheelEvent>

#include <cmath>
#include <string>

namespace piricad::app {

MapCanvas::MapCanvas(Controller& controller, QWidget* parent)
    : CanvasSurface(parent), controller_(controller), backend_(make_canvas_backend())
{
#if PIRICAD_HAVE_RHI
    // FOUR SAMPLES. A GPU pipeline rasterises a hard edge, and at a 1.5 px stroke
    // that lands on two pixel columns or three depending on where the line falls
    // — so a hatch whose spacing is uniform comes out with one line in every set
    // looking twice as heavy as its neighbours. QPainter antialiases and the two
    // engines have to agree about what a published çizgi tipi looks like.
    //
    // Four and not eight: the difference is invisible at these widths and the
    // fragment cost is not, and Article 7 gives the frame 16 ms.
    setSampleCount(4);
#endif
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
    tokens_  = mode == ThemeMode::Dark ? &darkTokens() : &lightTokens();
    update();
}

QString MapCanvas::backendName() const
{
    return QString::fromStdString(backend_->name());
}

const core::Document& MapCanvas::document() const
{
    return controller_.document();
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
    // The screen this widget is on decides how big a paper millimetre is. A
    // gösterim the annex prints at 8 mm has to arrive 8 mm tall, and only the
    // screen knows how many pixels that is; asking it here rather than caching it
    // is what makes the symbol keep its size when the window is dragged onto a
    // second monitor with a different resolution.
    if (const QScreen* on = screen(); on != nullptr && on->logicalDotsPerInch() > 0.0)
        options_.pixels_per_paper_mm = on->logicalDotsPerInch() / 25.4;

    render::build_scene(controller_.document(), view_, options_, draw_);
}

void MapCanvas::reloadGridSettings()
{
    const core::Settings& store = controller_.bus().app_settings();

    grid_.visible  = store.get("core.izgara.gorunur").as_bool();
    grid_.adaptive = store.get("core.izgara.mod").as_enum() == 0;
    grid_.step     = store.get("core.izgara.adim").as_length();
    grid_.major    = static_cast<int>(store.get("core.izgara.ana_cizgi").as_int());

    // Read once, here, and never in `paintEvent`. A setting lookup is a folded
    // Turkish string compare against a catalogue; doing forty of them inside the
    // 16 ms frame budget (Article 7) would be paying for a preference on every
    // mouse move.
    const auto colour = [&](const char* id) {
        return static_cast<std::uint32_t>(store.get(id).as_int());
    };

    look_.ruler        = store.get("core.cetvel.gorunur").as_bool();
    look_.ruler_px     = static_cast<int>(store.get("core.cetvel.kalinlik").as_int());
    look_.ruler_unit   = static_cast<int>(store.get("core.cetvel.birim").as_enum());
    look_.scale_bar    = store.get("core.harita.olcek_cubugu").as_bool();
    look_.north        = store.get("core.harita.kuzey_oku").as_bool();
    look_.readout      = store.get("core.harita.koordinat_gostergesi").as_bool();
    look_.cursor       = static_cast<int>(store.get("core.harita.imlec").as_enum());
    look_.cursor_px    = static_cast<int>(store.get("core.harita.imlec_boyu").as_int());
    look_.zoom_percent = static_cast<int>(store.get("core.harita.yakinlastirma_adimi").as_int());
    look_.invert_wheel = store.get("core.harita.tekerlek_ters").as_bool();
    look_.marker_px    = static_cast<int>(store.get("core.yakalama.isaret_boyu").as_int());
    look_.snap_tip     = store.get("core.yakalama.ipucu").as_bool();
    look_.dynamic_input = store.get("core.arayuz.dinamik_girdi").as_bool();
    look_.angle_unit    = static_cast<int>(store.get("core.aci.birim").as_enum());
    look_.step          = controller_.bus().session_settings().get("core.yakalama.adim").as_length();

    look_.marker_rgba     = colour("core.yakalama.isaret_rengi");
    look_.grid_rgba       = colour("core.izgara.renk");
    look_.grid_major_rgba = colour("core.izgara.ana_renk");
    look_.selection_rgba  = colour("core.secim.renk");

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

    // THE SAME FUNCTION THE SNAP ENGINE CALLS. Computing it here and again there
    // is how the drawn grid and the snapped grid came apart: the lines were 50 m
    // apart and the clicks landed on 10 m. `grid_step_in_force` is the one answer
    // and both readers ask it.
    //
    // It returns 0 when nothing should be drawn — a step below one millimetre, or
    // one so fine the lines merge into a flat wash that hides the drawing.
    // Refusing is the honest answer; silently substituting another step would make
    // the grid lie about the distance it represents.
    const core::Mm step =
        core::grid_step_in_force(grid_.step, grid_.adaptive, view_.mm_per_pixel());
    if (step <= 0) return;

    const auto step_mm = static_cast<double>(step);

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
    const std::size_t minor = nextBatch(chosen(look_.grid_rgba, palette_.grid.rgba()), 1.0f, false);
    const std::size_t major =
        nextBatch(chosen(look_.grid_major_rgba, palette_.gridMajor.rgba()), 1.0f, false);

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

    // The marker promises "let go here and this is what you get", so it is drawn
    // exactly when something is about to take a point: a command that is asking
    // for one, or a corner being dragged. Nothing else is a promise the program
    // can keep.
    command::Session* session = controller_.session();
    const bool asking         = session && session->waiting();
    if (!asking && !dragging_grip_) return;

    command::Bus& bus                = controller_.bus();
    const command::AidSettings& aids = bus.aid_settings();
    const core::Point2 aim = view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    // The SAME base the command will use, so the marker and the result cannot
    // disagree. `KÖŞETAŞI` measures from the corner being moved and `KÖŞEEKLE`
    // from the corner the edge leaves; a drag that previewed against some other
    // origin would put dik mod and kutupsal on a different ray than the one the
    // corner actually lands on.
    const bool has_base =
        asking ? session->prompt().has_rubber_band : drag_grip_.valid();
    const core::Point2 base =
        asking ? session->prompt().rubber_origin : drag_grip_.base;

    const core::SnapResult r =
        bus.aids().resolve(controller_.document(), aids, aim, has_base, base);
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

    // Taken by reference AFTER the last `nextBatch` of this function, which is
    // the only shape in which that is safe: nothing below grows the vector.
    render::OverlayBatch& batch =
        overlay_.batches[nextBatch(palette_.selection.rgba(), 3.0f, false)];

    // Walked per selected entity, never per document entity: a selection is
    // O(hundreds) and the frame budget belongs to the drawing (§10.1).
    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;

        // THE SHAPE, not the stored vertices. A circle keeps a centre and a radius
        // handle in its ring; highlighting those draws a line pointing east over a
        // circle the user can see is selected nowhere.
        curve_scratch_x_.clear();
        curve_scratch_y_.clear();
        const bool curve = table.kind[e] == core::kCircleKind || table.kind[e] == core::kArcKind;
        if (table.kind[e] == core::kCircleKind)
            core::circle_outline(core::circle_centre_of(geom, table.slot[e]),
                                 core::circle_radius_of(geom, table.slot[e]), curve_scratch_x_,
                                 curve_scratch_y_);
        else if (table.kind[e] == core::kArcKind)
            core::arc_outline(core::arc_centre_of(geom, table.slot[e]),
                              core::arc_radius_of(geom, table.slot[e]),
                              core::arc_start_of(geom, table.slot[e]),
                              core::arc_end_of(geom, table.slot[e]), curve_scratch_x_,
                              curve_scratch_y_);

        const core::RingSpan span = geom.rings_of(table.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = curve ? std::span<const core::Mm>(curve_scratch_x_) : geom.ring_xs(r);
            const auto ys = curve ? std::span<const core::Mm>(curve_scratch_y_) : geom.ring_ys(r);
            if (xs.size() < 2) continue;

            const auto before = static_cast<std::uint32_t>(batch.xs.size());
            for (std::size_t v = 0; v < xs.size(); ++v) {
                const render::ScreenPointF q =
                    render::to_f(view_.to_screen(core::Point2{xs[v], ys[v]}));
                batch.xs.push_back(q.x);
                batch.ys.push_back(q.y);
            }
            batch.runs.push_back(static_cast<std::uint32_t>(batch.xs.size()) - before);
            // A circle closes; an arc does not.
            batch.closed.push_back(
                (table.kind[e] == core::kCircleKind ||
                 (!curve && geom.ring_role[r] != core::RingRole::Open))
                    ? 1
                    : 0);
        }
    }
}

MapCanvas::Grip MapCanvas::gripAt(const QPointF& where) const
{
    const auto& selected = controller_.selectedSlots();
    if (selected.empty()) return {};

    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();

    // A screen aperture, not a world one: the user aims at what they can see, so
    // the same handful of pixels has to work at 1:100 and at 1:100 000. This is
    // the rule `core/snap.hpp` states for the snap tolerance and the reason it
    // takes a world radius computed one layer up.
    constexpr double kCornerPx = 7.0;
    constexpr double kEdgePx   = 5.0;

    Grip corner_hit;
    double corner_best = kCornerPx * kCornerPx;
    Grip edge_hit;
    double edge_best = kEdgePx * kEdgePx;

    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;

        // A CURVE HAS NO CORNERS. Its stored vertices are its definition — a
        // circle's are a centre and a radius handle — and `KÖŞETAŞI` refuses them
        // for that reason. Offering a handle the command will then refuse is worse
        // than offering none: it looks broken rather than deliberate.
        if (table.kind[e] != core::kPolylineKind) continue;

        const core::RingSpan span = geom.rings_of(table.slot[e]);

        // The corner number runs across the object's rings, so it keeps counting
        // from one ring into the next. `locate()` in commands/vertex.cpp walks the
        // same order; the two have to agree or a drag edits a different corner
        // than the one under the pointer.
        std::int64_t number = 0;

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs     = geom.ring_xs(r);
            const auto ys     = geom.ring_ys(r);
            const bool closed = geom.ring_role[r] != core::RingRole::Open;

            for (std::size_t v = 0; v < xs.size(); ++v) {
                ++number;
                const render::ScreenPoint p = view_.to_screen(core::Point2{xs[v], ys[v]});
                const double dx             = p.x - where.x();
                const double dy             = p.y - where.y();
                const double d2             = dx * dx + dy * dy;
                if (d2 < corner_best) {
                    corner_best = d2;
                    corner_hit  = Grip{e, number, false, core::Point2{xs[v], ys[v]},
                                       core::Point2{xs[v], ys[v]}};
                }

                // The edge LEAVING this corner. On an open ring the last vertex has
                // none — a polyline's ends are not joined (R10) — which is exactly
                // what `KÖŞEEKLE` refuses, so the canvas never offers it either.
                const bool last = v + 1 == xs.size();
                if (last && !closed) continue;

                const std::size_t next      = last ? 0 : v + 1;
                const render::ScreenPoint q = view_.to_screen(core::Point2{xs[next], ys[next]});

                const double ex = q.x - p.x;
                const double ey = q.y - p.y;
                const double len2 = ex * ex + ey * ey;
                if (len2 <= 0.0) continue;

                double t = ((where.x() - p.x) * ex + (where.y() - p.y) * ey) / len2;
                t        = std::clamp(t, 0.0, 1.0);

                const double fx = p.x + t * ex - where.x();
                const double fy = p.y + t * ey - where.y();
                const double f2 = fx * fx + fy * fy;
                if (f2 < edge_best) {
                    edge_best = f2;
                    // The new corner starts where the pointer pressed, projected
                    // onto the edge, so it does not jump before the drag begins.
                    const auto foot = render::ScreenPoint{p.x + t * ex, p.y + t * ey};
                    edge_hit = Grip{e, number, true, view_.to_world(foot),
                                    core::Point2{xs[v], ys[v]}};
                }
            }
        }
    }

    // A corner wins over the edges that meet at it: within a few pixels of a
    // corner both are hit, and a user aiming there means to move the corner
    // rather than to grow a new one beside it.
    if (corner_hit.valid()) return corner_hit;
    return edge_hit;
}

void MapCanvas::buildGrips()
{
    const auto& selected = controller_.selectedSlots();
    if (selected.empty()) return;

    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();

    // While a corner is being dragged, the shape it WOULD make is drawn first, so
    // the user sees the two edges that follow the corner rather than a bare
    // handle floating away from an unchanged outline.
    if (dragging_grip_ && drag_grip_.valid() && cursor_valid_) {
        const core::Point2 to = snap_preview_valid_
                                    ? snap_preview_.point
                                    : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

        const core::EntityId e = drag_grip_.entity;
        if (e < table.size() && table.visible(e)) {
            const core::RingSpan span = geom.rings_of(table.slot[e]);
            const std::size_t batch   = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);

            std::int64_t number = 0;
            for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
                const auto xs = geom.ring_xs(r);
                const auto ys = geom.ring_ys(r);

                std::vector<render::ScreenPointF> run;
                run.reserve(xs.size() + 1);
                for (std::size_t v = 0; v < xs.size(); ++v) {
                    ++number;
                    if (number == drag_grip_.corner && !drag_grip_.insert) {
                        run.push_back(render::to_f(view_.to_screen(to)));
                        continue;
                    }
                    run.push_back(render::to_f(view_.to_screen(core::Point2{xs[v], ys[v]})));
                    // An inserted corner goes AFTER the one it was measured from,
                    // which is the same place `KÖŞEEKLE` will put it.
                    if (number == drag_grip_.corner && drag_grip_.insert)
                        run.push_back(render::to_f(view_.to_screen(to)));
                }
                addRun(batch, run, geom.ring_role[r] != core::RingRole::Open);
            }
        }
    }

    // The handles themselves, drawn over the outline so a corner is grabbable
    // wherever two objects meet.
    const std::size_t plain = nextBatch(palette_.selection.rgba(), 1.0f, false);
    const std::size_t lit   = nextBatch(tokens_->accent.rgba(), 2.0f, false);

    constexpr float kHalf = 3.0f;

    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;
        if (table.kind[e] != core::kPolylineKind) continue; // a curve has no corners

        const core::RingSpan span = geom.rings_of(table.slot[e]);
        std::int64_t number       = 0;

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);

            for (std::size_t v = 0; v < xs.size(); ++v) {
                ++number;
                const render::ScreenPointF p =
                    render::to_f(view_.to_screen(core::Point2{xs[v], ys[v]}));

                const bool hot = hover_grip_.valid() && !hover_grip_.insert &&
                                 hover_grip_.entity == e && hover_grip_.corner == number;

                addRun(hot ? lit : plain,
                       {{p.x - kHalf, p.y - kHalf},
                        {p.x + kHalf, p.y - kHalf},
                        {p.x + kHalf, p.y + kHalf},
                        {p.x - kHalf, p.y + kHalf}},
                       true);
            }
        }
    }

    // The edge the pointer is over, marked where the new corner would appear. A
    // different shape from a corner handle on purpose: it does not move a corner,
    // it makes one.
    if (hover_grip_.valid() && hover_grip_.insert && !dragging_grip_) {
        const render::ScreenPointF p = render::to_f(view_.to_screen(hover_grip_.at));
        addCircle(lit, p.x, p.y, kHalf);
    }
}

void MapCanvas::commitGripDrag()
{
    if (!drag_grip_.valid()) return;

    // A press and release without travel is a CLICK, not a drag, and a click on a
    // corner asks for nothing. Qt's own drag threshold is the right number here:
    // it is what the platform considers a deliberate movement, and using anything
    // else makes this widget feel unlike every other one on the machine.
    const QPointF moved = cursor_ - drag_anchor_;
    if (moved.manhattanLength() < QApplication::startDragDistance()) return;

    const core::Point2 world =
        view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    // The RAW world point, exactly as a click supplies one. Snapping happens once,
    // inside the command layer, on the road every client takes — the marker the
    // canvas drew was a preview of that, never a substitute for it.
    const core::EntityKey key = controller_.document().entities().key[drag_grip_.entity];

    // An ID LIST, because `nesne` is declared `ParamKind::Selection` and the bus
    // validates the shape before the body runs. A bare integer is refused there —
    // silently as far as the canvas is concerned, since a rejected dispatch only
    // writes a line to the transcript, which is exactly how a drag that did
    // nothing at all looked like a drag that could not start.
    command::Args args;
    args.set("nesne",
             command::Value::ids({static_cast<std::int64_t>(core::raw(key))}));
    args.set("kose", command::Value::integer(drag_grip_.corner));
    args.set("nokta", command::Value::point(world));

    controller_.runInvocation(command::Invocation{
        drag_grip_.insert ? "core.vertex_insert" : "core.vertex_move", std::move(args),
        command::Origin::Gui});
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

    const std::size_t batch = nextBatch(line.rgba(), 1.0f, crossing, fill.rgba());

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
    const float h = static_cast<float>(look_.marker_px) * 0.5f; // half size, layout units

    const std::uint32_t ink = chosen(look_.marker_rgba, palette_.snapMarker.rgba());
    const std::size_t batch = nextBatch(ink, 1.8f, false);

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
    case core::SnapNode: // a filled ring: the surveyed monument itself
        // DÜĞÜM had no glyph and fell through to `default`, so snapping to a
        // control point drew nothing at all — the one thing on a cadastral sheet
        // every boundary is measured from, and the marker said it had not fired.
        addCircle(batch, x, y, h * 0.55f);
        addRun(batch, {{x - h, y}, {x - h * 0.55f, y}}, false);
        addRun(batch, {{x + h * 0.55f, y}, {x + h, y}}, false);
        addRun(batch, {{x, y - h}, {x, y - h * 0.55f}}, false);
        addRun(batch, {{x, y + h * 0.55f}, {x, y + h}}, false);
        break;
    case core::SnapPolar:
    case core::SnapOrtho: // diamond: the point is on a locked direction
        addRun(batch, {{x, y - h}, {x + h, y}, {x, y + h}, {x - h, y}}, true);
        break;

    // The constructed points get OPEN glyphs — a shape with a gap in it — so a
    // point this engine built is never mistaken at a glance for a corner the
    // drawing actually contains.
    case core::SnapExtension: // an arrow continuing to the right
        addRun(batch, {{x - h, y}, {x + h, y}}, false);
        addRun(batch, {{x, y - h * 0.6f}, {x + h, y}, {x, y + h * 0.6f}}, false);
        break;
    case core::SnapParallel: // the two strokes of the parallel sign
        addRun(batch, {{x - h * 0.3f, y - h}, {x - h, y + h}}, false);
        addRun(batch, {{x + h, y - h}, {x + h * 0.3f, y + h}}, false);
        break;
    case core::SnapApparent: // a cross with an open corner
        addRun(batch, {{x - h, y - h}, {x + h, y + h}}, false);
        addRun(batch, {{x - h, y + h}, {x, y}}, false);
        break;

    default: break;
    }

    // The label says which aid fired. Without it a user cannot tell an endpoint
    // from an intersection when both glyphs sit under the cursor — and with the
    // constructed modes on there is more to tell apart, not less.
    if (look_.snap_tip)
        overlay_.labels.push_back(
            render::OverlayLabel{ink, x + h + 4.0f, y - h - 2.0f, 0.0f, false,
                                 std::string(core::snap_mode_label(snap_preview_.mode))});
}

/// The ruler's numbers, in the unit the preference names.
namespace {

/// One tick's label, and the divisor that turns document millimetres into it.
struct RulerUnit
{
    double per_unit; ///< millimetres in one unit
    const char* suffix;
};

RulerUnit ruler_unit_of(int index) noexcept
{
    switch (index) {
    case 1: return RulerUnit{10.0, "cm"};
    case 2: return RulerUnit{1000000.0, "km"};
    default: return RulerUnit{1000.0, "m"};
    }
}

/// A round step of about `target` millimetres: 1, 2 or 5 times a power of ten.
///
/// The same 1-2-5 ladder every map scale uses, and for the same reason: a ruler
/// whose ticks are 137 m apart is a ruler nobody can read a distance off.
double nice_step(double target) noexcept
{
    if (target <= 0.0) return 1.0;

    double decade = 1.0;
    while (decade * 10.0 <= target)
        decade *= 10.0;
    while (decade > target)
        decade /= 10.0;

    const double ratio = target / decade;
    if (ratio >= 5.0) return decade * 5.0;
    if (ratio >= 2.0) return decade * 2.0;
    return decade;
}

/// `value` with at most `places` decimals and no trailing zeroes, because a ruler
/// reading "120.000" is three characters of noise on every tick.
/// `458 000` — thousands separated by a space, the way a Turkish map sheet
/// prints a coordinate. Not `QLocale`: tr_TR puts a full stop there, and a full
/// stop in a coordinate is a decimal point to every reader of that sheet.
std::string spaced(double value)
{
    const auto whole   = static_cast<long long>(std::llround(value));
    std::string digits = std::to_string(whole < 0 ? -whole : whole);
    for (std::size_t at = digits.size(); at > 3;) {
        at -= 3;
        digits.insert(at, 1, ' ');
    }
    return whole < 0 ? "-" + digits : digits;
}

/// The bearing from `a` to `b`, written in the project's angle unit.
///
/// AZIMUT, which is measured CLOCKWISE FROM NORTH — not the mathematical angle
/// counter-clockwise from east. That is the number a Turkish surveyor reads off a
/// total station, writes in a traverse sheet and types into a setting-out list,
/// and getting it wrong by ninety degrees or by a sign is the kind of mistake
/// that reaches a parsel corner.
///
/// GRAD by default (`core.aci.birim`), because a full circle is 400 grad in
/// Turkish triangulation, traverse and setting-out arithmetic.
std::string bearing_text(core::Point2 a, core::Point2 b, int unit);

std::string trimmed(double value, int places)
{
    std::string out = QString::number(value, 'f', places).toStdString();
    if (out.find('.') == std::string::npos) return out;
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    return out;
}

std::string bearing_text(core::Point2 a, core::Point2 b, int unit)
{
    const double dx = static_cast<double>(b.x - a.x);
    const double dy = static_cast<double>(b.y - a.y);
    if (dx == 0.0 && dy == 0.0) return {};

    // Clockwise from north: atan2(east, north), not atan2(north, east). A reading
    // taken the other way round is the mathematical angle, and a surveyor
    // comparing it against an instrument would find every value mirrored about
    // the 50-grad line.
    //
    // This is a LABEL, not a stored value, so `atan2` is allowed here: nothing in
    // §7.3's bit-identity requirement passes through it. The engine's own
    // constraints use `sin_cos_udeg` for exactly that reason.
    double turns = std::atan2(dx, dy) / (2.0 * 3.14159265358979323846);
    if (turns < 0.0) turns += 1.0;

    switch (unit) {
    case 1: return trimmed(turns * 360.0, 3) + "°";
    case 2: return trimmed(turns * 2.0 * 3.14159265358979323846, 5) + " rad";
    default: return trimmed(turns * 400.0, 3) + " grad";
    }
}

} // namespace

void MapCanvas::buildRuler()
{
    if (!look_.ruler) return;

    const auto band = static_cast<float>(look_.ruler_px);
    const auto w    = static_cast<float>(width());
    const auto h    = static_cast<float>(height());
    if (w <= band || h <= band) return;

    const RulerUnit unit = ruler_unit_of(look_.ruler_unit);

    // design.md §7: a 20 px band along the top and the left, a division line
    // every ~100 px, and the reading printed in mono to the right of it. The
    // band is FILLED rather than outlined — the reference draws it as a sunken
    // strip the drawing sits inside, not as two rules over the canvas.
    const std::size_t ground = nextBatch(0, 0.0f, false, tokens_->bgSunken.rgba());
    addRun(ground, {{0.0f, 0.0f}, {w, 0.0f}, {w, band}, {0.0f, band}}, true);
    addRun(ground, {{0.0f, band}, {band, band}, {band, h}, {0.0f, h}}, true);

    const std::size_t edge = nextBatch(tokens_->lineHard.rgba(), 1.0f, false);
    addRun(edge, {{0.0f, band}, {w, band}}, false);
    addRun(edge, {{band, band}, {band, h}}, false);

    // The division step rides the 1-2-5 ladder so the reading is a round number
    // a surveyor can hold in their head, and lands near the reference's 100 px.
    const double step_mm = nice_step(view_.mm_per_pixel() * 100.0);
    if (step_mm <= 0.0) return;

    const core::Box2 seen = view_.visible_box();

    const std::size_t ticks = nextBatch(tokens_->rulerTick.rgba(), 1.0f, false);

    constexpr float kTick  = 9.0f; ///< the division mark's length
    constexpr float kLabel = 8.5f; ///< §3's `cetvel, mikro etiket` size

    // The loop counts ticks rather than accumulating a position, exactly as
    // `buildGrid` does: adding a step a thousand times drifts, and a ruler that
    // drifts is a ruler that lies about the distance it is measuring.
    const auto first_x =
        static_cast<long long>(std::floor(static_cast<double>(seen.min_x) / step_mm));
    const auto last_x =
        static_cast<long long>(std::ceil(static_cast<double>(seen.max_x) / step_mm));
    for (long long i = first_x; i <= last_x; ++i) {
        const double mm = static_cast<double>(i) * step_mm;
        const float x = static_cast<float>(view_.to_screen(core::Point2{core::mm_round(mm), 0}).x);
        if (x < band || x > w) continue;

        addRun(ticks, {{x, band - kTick}, {x, band}}, false);
        overlay_.labels.push_back(render::OverlayLabel{tokens_->textFaint.rgba(), x + 4.0f,
                                                       band - kTick - 1.0f, kLabel, true,
                                                       spaced(mm / unit.per_unit)});
    }

    const auto first_y =
        static_cast<long long>(std::floor(static_cast<double>(seen.min_y) / step_mm));
    const auto last_y =
        static_cast<long long>(std::ceil(static_cast<double>(seen.max_y) / step_mm));
    for (long long i = first_y; i <= last_y; ++i) {
        const double mm = static_cast<double>(i) * step_mm;
        const float y = static_cast<float>(view_.to_screen(core::Point2{0, core::mm_round(mm)}).y);
        if (y < band || y > h) continue;

        // DIVISIONS ONLY, no number. A northing is nine digits and the band is
        // 20 px, so a horizontal string spills onto the drawing and a rotated one
        // costs the backend a transform for something nobody reads off a ruler.
        // The reference leaves it bare too, and the northing under the cursor is
        // already in the status strip where a surveyor looks for it.
        addRun(ticks, {{band - kTick, y}, {band, y}}, false);
    }

    overlay_.labels.push_back(render::OverlayLabel{tokens_->textFaint.rgba(), 3.0f, band - 4.0f,
                                                   kLabel, true, std::string(unit.suffix)});
}

void MapCanvas::buildScaleBar()
{
    if (!look_.scale_bar) return;

    // design.md §7: 180 px, four divisions, `0 / 100 / 200 m` beneath. The bar is
    // a FIXED width and the ground distance it stands for is what changes, which
    // is the opposite of the usual "round distance, whatever width" bar — and it
    // is what the reference draws, because a bar that never changes size never
    // moves the labels under it.
    constexpr float kBarWidth  = 180.0f;
    constexpr float kBarHeight = 7.0f;
    constexpr int kCells       = 4;
    constexpr float kInset     = 16.0f; ///< from the canvas's own left and bottom
    constexpr float kFromFoot  = 32.0f;

    const double mm_per_px = view_.mm_per_pixel();
    if (mm_per_px <= 0.0) return;

    const auto band  = static_cast<float>(look_.ruler ? look_.ruler_px : 0);
    const float left = band + kInset;
    const float top  = static_cast<float>(height()) - kFromFoot - kBarHeight;
    const float cell = kBarWidth / static_cast<float>(kCells);

    if (top <= band || left + kBarWidth > static_cast<float>(width())) return;

    // Alternating cells, filled and empty, so a distance can be counted off the
    // bar rather than estimated against it.
    for (int i = 0; i < kCells; i += 2) {
        // INSIDE the frame by one pixel: the reference draws a 1 px outline
        // around the divisions, not through them, and a fill that reaches the
        // outline swallows it on the two filled cells.
        const std::size_t fill = nextBatch(0, 0.0f, false, tokens_->readoutDim.rgba());
        const float x0         = left + static_cast<float>(i) * cell + 1.0f;
        addRun(fill,
               {{x0, top + 1.0f},
                {x0 + cell - 1.0f, top + 1.0f},
                {x0 + cell - 1.0f, top + kBarHeight - 1.0f},
                {x0, top + kBarHeight - 1.0f}},
               true);
    }

    const std::size_t frame = nextBatch(tokens_->hud.rgba(), 1.0f, false);
    addRun(frame,
           {{left, top},
            {left + kBarWidth, top},
            {left + kBarWidth, top + kBarHeight},
            {left, top + kBarHeight}},
           true);

    const RulerUnit unit = ruler_unit_of(look_.ruler_unit);
    const double whole   = static_cast<double>(kBarWidth) * mm_per_px / unit.per_unit;

    // Three readings under the bar: nothing, half, and all of it — with the unit
    // written once, at the right end, the way a map sheet prints it.
    const float baseline = top + kBarHeight + 12.0f;
    overlay_.labels.push_back(
        render::OverlayLabel{tokens_->textFaint.rgba(), left, baseline, 9.5f, true, "0"});
    overlay_.labels.push_back(render::OverlayLabel{tokens_->textFaint.rgba(),
                                                   left + kBarWidth * 0.5f - 8.0f, baseline, 9.5f,
                                                   true, trimmed(whole * 0.5, 3)});
    overlay_.labels.push_back(
        render::OverlayLabel{tokens_->textFaint.rgba(), left + kBarWidth - 26.0f, baseline, 9.5f,
                             true, trimmed(whole, 3) + " " + std::string(unit.suffix)});
}

void MapCanvas::buildNorthArrow()
{
    if (!look_.north) return;

    // design.md §7: a 74 px disc at the bottom right of the drawing. Up IS north
    // — the view has no rotation yet, so the arrow is drawn straight and this
    // comment is the note that will need changing the day it does.
    constexpr float kRadius   = 37.0f;
    constexpr float kFromEdge = 15.0f;
    constexpr float kFromFoot = 53.0f;

    const float cx  = static_cast<float>(width()) - kFromEdge - kRadius;
    const float cy  = static_cast<float>(height()) - kFromFoot - kRadius;
    const auto band = static_cast<float>(look_.ruler ? look_.ruler_px : 0);
    if (cx - kRadius < band || cy - kRadius < band) return;

    const std::size_t disc = nextBatch(
        tokens_->border.rgba(), 1.0f, false,
        QColor(tokens_->bgSunken.red(), tokens_->bgSunken.green(), tokens_->bgSunken.blue(), 90)
            .rgba());
    addCircle(disc, cx, cy, kRadius);

    // A slim needle with a notched tail: the cartographic north mark, not a
    // solid triangle, so it reads as an instrument rather than as a cursor.
    const std::size_t needle = nextBatch(tokens_->readoutDim.rgba(), 1.4f, false);
    addRun(needle,
           {{cx, cy - 17.0f}, {cx + 8.0f, cy + 9.0f}, {cx, cy + 3.0f}, {cx - 8.0f, cy + 9.0f}},
           true);

    overlay_.labels.push_back(
        render::OverlayLabel{tokens_->textFaint.rgba(), cx - 3.0f, cy + 24.0f, 10.0f, false, "K"});
}

void MapCanvas::buildZoomStack()
{
    // design.md §7: three 28 px marks at the top right of the drawing — in, out,
    // fit. They are DRAWN rather than made of buttons because they sit over the
    // canvas: a widget there would take the wheel and the drag away from it.
    constexpr float kBox      = 28.0f;
    constexpr float kFromEdge = 16.0f;

    const auto band  = static_cast<float>(look_.ruler ? look_.ruler_px : 0);
    const float left = static_cast<float>(width()) - kFromEdge - kBox;
    const float top  = band + kFromEdge;
    if (left <= band) return;

    zoom_stack_ = QRectF(static_cast<double>(left), static_cast<double>(top),
                         static_cast<double>(kBox), static_cast<double>(kBox * 3.0f));

    const std::size_t panel = nextBatch(
        tokens_->border.rgba(), 1.0f, false,
        QColor(tokens_->bgSunken.red(), tokens_->bgSunken.green(), tokens_->bgSunken.blue(), 230)
            .rgba());
    addRun(panel,
           {{left, top},
            {left + kBox, top},
            {left + kBox, top + kBox * 3.0f},
            {left, top + kBox * 3.0f}},
           true);

    const std::size_t rules = nextBatch(tokens_->lineSoft.rgba(), 1.0f, false);
    for (int i = 1; i < 3; ++i) {
        const float y = top + kBox * static_cast<float>(i);
        addRun(rules, {{left, y}, {left + kBox, y}}, false);
    }

    // `+`, `−` and a frame: three marks a user recognises without a tooltip.
    const std::size_t marks = nextBatch(tokens_->readoutDim.rgba(), 1.3f, false);
    const float cx          = left + kBox * 0.5f;

    const float in = top + kBox * 0.5f;
    addRun(marks, {{cx - 5.0f, in}, {cx + 5.0f, in}}, false);
    addRun(marks, {{cx, in - 5.0f}, {cx, in + 5.0f}}, false);

    const float out = top + kBox * 1.5f;
    addRun(marks, {{cx - 5.0f, out}, {cx + 5.0f, out}}, false);

    const float fit = top + kBox * 2.5f;
    for (int q = 0; q < 4; ++q) {
        const float sx = (q & 1) ? -1.0f : 1.0f;
        const float sy = (q & 2) ? -1.0f : 1.0f;
        const float ox = cx + sx * 6.0f;
        const float oy = fit + sy * 5.0f;
        addRun(marks, {{ox - sx * 3.0f, oy}, {ox, oy}, {ox, oy - sy * 3.0f}}, false);
    }
}

void MapCanvas::buildReadout()
{
    if (!look_.readout || !cursor_valid_) return;

    // The SNAPPED point when an aid has fired, because that is the coordinate the
    // click will produce. Showing the raw cursor there would be showing a number
    // the drawing is never going to contain.
    const core::Point2 at = snap_preview_valid_
                                ? snap_preview_.point
                                : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    const std::string text = "S " + trimmed(static_cast<double>(at.x) / 1000.0, 3) + "   Y " +
                             trimmed(static_cast<double>(at.y) / 1000.0, 3);

    overlay_.labels.push_back(render::OverlayLabel{
        palette_.gridMajor.rgba(), 12.0f, static_cast<float>(height() - 4), 0.0f, true, text});
}

void MapCanvas::buildCrosshair()
{
    if (!cursor_valid_ || look_.cursor == 2) return;

    const std::size_t batch      = nextBatch(palette_.crosshair.rgba(), 1.0f, false);
    const render::ScreenPointF c = toScreenF(cursor_);
    const float x = c.x, y = c.y;

    // Full screen or a short cross, which is the choice every CAD offers and the
    // one people hold opinions about: the long lines line a point up against
    // something far away, the short one keeps the drawing legible.
    if (look_.cursor == 0) {
        addRun(batch, {{x, 0.0f}, {x, static_cast<float>(height())}}, false);
        addRun(batch, {{0.0f, y}, {static_cast<float>(width()), y}}, false);
        return;
    }

    const auto arm = static_cast<float>(look_.cursor_px);
    addRun(batch, {{x - arm, y}, {x + arm, y}}, false);
    addRun(batch, {{x, y - arm}, {x, y + arm}}, false);
}

std::size_t MapCanvas::nextBatch(std::uint32_t rgba, float width_px, bool dashed,
                                 std::uint32_t fill_rgba)
{
    // Reuses the batch this position held on the previous frame, buffers and all.
    // The overlay is rebuilt on EVERY MOUSE MOVE, so allocating here would allocate
    // on every mouse move (render.md R20, P6).
    if (overlay_used_ == overlay_.batches.size()) overlay_.batches.emplace_back();

    const std::size_t at        = overlay_used_++;
    render::OverlayBatch& batch = overlay_.batches[at];
    batch.rgba                  = rgba;
    batch.fill_rgba             = fill_rgba;
    batch.width_px              = width_px;
    batch.dashed                = dashed;
    return at;
}

render::ScreenPointF MapCanvas::toScreenF(const QPointF& p)
{
    // A widget-space QPointF is already a pixel — a cursor position, a drag
    // anchor. It goes through the same one narrowing as everything else so there
    // is exactly one place in the canvas where a coordinate becomes a float.
    return render::to_f(render::ScreenPoint{p.x(), p.y()});
}

void MapCanvas::addRun(std::size_t index, std::span<const render::ScreenPointF> points, bool closed)
{
    render::OverlayBatch& batch = overlay_.batches[index];
    for (const render::ScreenPointF& p : points) {
        batch.xs.push_back(p.x);
        batch.ys.push_back(p.y);
    }
    batch.runs.push_back(static_cast<std::uint32_t>(points.size()));
    batch.closed.push_back(closed ? 1 : 0);
}

void MapCanvas::addRun(std::size_t index, std::initializer_list<render::ScreenPointF> points,
                       bool closed)
{
    addRun(index, std::span<const render::ScreenPointF>(points.begin(), points.size()), closed);
}

void MapCanvas::addCircle(std::size_t index, float cx, float cy, float radius)
{
    render::OverlayBatch& batch = overlay_.batches[index];
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

    // THE GRID FIRST, and the count that follows is what puts it under the
    // drawing. It is the paper, not an annotation over the map.
    buildGrid();
    overlay_.beneath = overlay_used_;

    buildSelection();
    buildGrips();

    guide_vertices_ = 0;
    guide_label_.clear();

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

        const std::size_t batch = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);
        const command::RubberShape shape = session->prompt().rubber_shape;
        const std::size_t guide_before   = overlay_.batches[batch].xs.size();

        if (shape == command::RubberShape::Circle || shape == command::RubberShape::Arc) {
            // THE CURVE ITSELF. Drawn by the same code the document is drawn with
            // (`core::circle_outline`), so what the guide promises and what the
            // command produces cannot drift apart — a preview computed a second
            // way is a preview that is eventually wrong.
            const core::Point2 centre = session->prompt().rubber_origin;
            const core::Point2 rim =
                snap_preview_valid_ ? snap_preview_.point
                                    : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

            const double dx       = core::mm_to_metres(rim.x - centre.x);
            const double dy       = core::mm_to_metres(rim.y - centre.y);
            const core::Mm radius = core::mm_round(std::sqrt(dx * dx + dy * dy) *
                                                   static_cast<double>(core::kMmPerMetre));

            // An ARC guide once the first end is fixed: `rubber_chain` carries it,
            // so the guide sweeps from there to the cursor exactly as the command
            // will. Before that — and for DAİRE throughout — the guide is the
            // whole circle, because what is being chosen at that moment IS a
            // radius, and a radius is a circle.
            const auto& chain = session->prompt().rubber_chain;
            const bool arc    = shape == command::RubberShape::Arc && !chain.empty();

            const core::Mm draw_radius =
                arc ? [&] {
                    const double ax = core::mm_to_metres(chain.front().x - centre.x);
                    const double ay = core::mm_to_metres(chain.front().y - centre.y);
                    return core::mm_round(std::sqrt(ax * ax + ay * ay) *
                                          static_cast<double>(core::kMmPerMetre));
                }()
                    : radius;

            if (draw_radius > 0) {
                curve_scratch_x_.clear();
                curve_scratch_y_.clear();
                if (arc)
                    core::arc_outline(centre, draw_radius, chain.front(), rim, curve_scratch_x_,
                                      curve_scratch_y_);
                else
                    core::circle_outline(centre, draw_radius, curve_scratch_x_, curve_scratch_y_);

                std::vector<render::ScreenPointF> run;
                run.reserve(curve_scratch_x_.size());
                for (std::size_t v = 0; v < curve_scratch_x_.size(); ++v)
                    run.push_back(render::to_f(view_.to_screen(
                        core::Point2{curve_scratch_x_[v], curve_scratch_y_[v]})));
                addRun(batch, run, !arc);
            }

            // The radius, so the user can read the size they are setting rather
            // than only see it.
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Rectangle) {
            // THE FACE, not its diagonal. A rectangle previewed as one line tells
            // the user nothing about what the next click will make, and with the
            // diagonal lock held it is the difference between seeing a square and
            // finding out you drew one.
            const render::ScreenPointF a = render::to_f(from);
            const render::ScreenPointF b = toScreenF(to);
            addRun(batch, {{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}}, true);
        } else if (const auto& chain = session->prompt().rubber_chain; !chain.empty()) {
            // THE WHOLE SHAPE SO FAR, not only its newest edge. A command whose
            // geometry cannot reach the document until it is complete (ALAN) has
            // nothing else on screen, so drawing one segment made every click
            // look like it had erased the one before it.
            std::vector<render::ScreenPointF> run;
            run.reserve(chain.size() + 1);
            for (const core::Point2& p : chain) run.push_back(render::to_f(view_.to_screen(p)));
            run.push_back(toScreenF(to));

            // Closed for a ring, because the edge back to the first corner is as
            // real as the one the cursor is dragging: ALAN never asks the user to
            // repeat the closing point, so the preview is where they see it.
            addRun(batch, run, session->prompt().rubber_shape == command::RubberShape::Ring);
        } else {
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        }

        guide_vertices_ = overlay_.batches[batch].xs.size() - guide_before;

        // ---- what the guide MEASURES, written on it ----
        //
        // A rubber band that shows only a direction makes the user click, read the
        // result and undo. The length and the bearing belong on the line while it
        // is being dragged — that is what every CAD calls dynamic input, and what
        // a surveyor setting out a 12 cm step needs to see the step working.
        if (look_.dynamic_input) {
            const core::Point2 from_world = session->prompt().rubber_origin;
            const core::Point2 to_world =
                snap_preview_valid_ ? snap_preview_.point
                                    : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

            const core::Mm length = core::segment_length(from_world, to_world);
            if (length > 0) {
                std::string text = trimmed(static_cast<double>(length) / 1000.0, 3) + " m";
                text += "  " + bearing_text(from_world, to_world, look_.angle_unit);

                // ON the line, at its middle, lifted clear of it. Beside the
                // cursor it would fight the snap marker and its mode name, which
                // are already there and are about a different thing.
                const render::ScreenPointF a = render::to_f(from);
                const render::ScreenPointF b = toScreenF(to);
                overlay_.labels.push_back(render::OverlayLabel{
                    tokens_->readout.rgba(), (a.x + b.x) * 0.5f + 8.0f, (a.y + b.y) * 0.5f - 6.0f,
                    0.0f, false, text});
                guide_label_ = text;
            }
        }
    }

    buildSelectionBox();
    buildRuler();
    buildScaleBar();
    buildNorthArrow();
    buildZoomStack();
    buildReadout();
    buildCrosshair();
    buildSnapMarker();

    // Developer HUD. Dear ImGui replaces this once the GPU canvas lands; it is a
    // debug layer and never a user-facing feature (piricad.md §6.3), so it is off
    // unless the developer asks for it.
    if (!debug_hud_) return;

    overlay_.labels.push_back(
        render::OverlayLabel{palette_.hud.rgba(), 8.0f, 22.0f, 0.0f, true,
                             QStringLiteral("%1  |  %2 nesne  |  %3 tepe  |  %4 elenen  |  %5 µs")
                                 .arg(backendName())
                                 .arg(draw_.entity_count)
                                 .arg(draw_.vertex_count)
                                 .arg(draw_.culled_count)
                                 .arg(last_frame_us_)
                                 .toStdString()});
}

QImage MapCanvas::grabCanvas()
{
#if PIRICAD_HAVE_RHI
    // The GPU's own copy. `grabFramebuffer()` renders a frame and reads it back,
    // so what comes out is what the pipeline drew rather than what the widget
    // system thinks is there.
    return grabFramebuffer();
#else
    return grab().toImage();
#endif
}

std::vector<int> MapCanvas::timeFrames(int rounds)
{
    // Through the real paint path, not a private one: a number measured on a
    // shortcut is a number about the shortcut.
    //
    // `grabCanvas()`, NOT `repaint()`. Repainting a widget the window system has
    // not exposed — which is every headless run, and any run whose screen is
    // locked — does nothing at all, so the timer measured a paint that had not
    // happened and the harness reported `0 us`. A backend that draws nothing in
    // no time reads as infinitely fast, which is the worst possible answer from a
    // tool whose whole job is to say which backend is quicker.
    //
    // The grab forces the frame in both builds and costs a readback, but the
    // readback lands AFTER `last_draw_us_` is recorded, so the number reported is
    // still the backend's own share.
    std::vector<int> costs;
    costs.reserve(static_cast<std::size_t>(std::max(0, rounds)));
    for (int i = 0; i < rounds; ++i) {
        (void)grabCanvas();
        // The BACKEND's share. Scene rebuild and overlay run in the same paint
        // and, on a sheet of patterned parcels, dwarf it — which is a fact about
        // where the time goes, not about which backend to keep, so the two are
        // reported apart.
        costs.push_back(last_draw_us_);
        scene_costs_.push_back(last_scene_us_);
    }
    return costs;
}

#if PIRICAD_HAVE_RHI
void MapCanvas::render(QRhiCommandBuffer* cb)
#else
void MapCanvas::paintEvent(QPaintEvent*)
#endif
{
    QElapsedTimer timer;
    timer.start();

    rebuildScene();
    buildOverlay();
    last_scene_us_ = static_cast<int>(timer.nsecsElapsed() / 1000);

    // Everything below this line is the backend's. This widget knows WHAT is on
    // screen; it does not know how any of it is drawn, which is what lets the GPU
    // backend replace the painter one without touching this file (render.md R1,
    // CLAUDE.md Article 8.1).
    render::FrameContext ctx;
    ctx.width_px           = width();
    ctx.height_px          = height();
    ctx.device_pixel_ratio = static_cast<float>(devicePixelRatioF());
#if PIRICAD_HAVE_RHI
    // The GPU frame's handles, packed by the factory. Packing them HERE would put
    // backend knowledge in the widget, which render.md R1 keeps out of it.
    ctx.target = rhi_frame_target(rhi(), cb, renderTarget());
#else
    // Cast HERE, not in the backend: QWidget inherits QObject and QPaintDevice
    // both, and passing a QWidget* through a void* to be read as a QPaintDevice*
    // hands over the wrong address.
    ctx.target = static_cast<QPaintDevice*>(this);
#endif

    backend_->render(draw_, overlay_, ctx);

    last_frame_us_ = static_cast<int>(timer.nsecsElapsed() / 1000);
    last_draw_us_  = last_frame_us_ - last_scene_us_;
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
            // WHAT IS BEING ASKED FOR decides what a click does. A command that
            // wants a string is not answered by a coordinate, and a click that
            // sent one anyway is what left METİN waiting forever: the anchor went
            // in, the prompt turned into "Yazılacak metin", and every further
            // click supplied another point the command was not asking for.
            if (controller_.promptKind() == command::ParamKind::Text) {
                openTextEditor(event->position());
                return;
            }

            // A click is one input value for the running command, and it is the RAW
            // world point. Snapping is not applied here: it happens once, inside
            // the command layer, on the path every client takes (piricad.md §2.4,
            // piricad/command/aids.hpp). A canvas that snapped first would be a
            // client with a private route.
            const core::Point2 world =
                view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
            const QPointF at = event->position();
            controller_.supplyPoint(world);

            // THE BOX OPENS ON THE CLICK THAT EARNED IT. METİN asks for its anchor
            // first and its string second, so the prompt turns into a text prompt
            // inside the call above — and waiting for another click would make the
            // user click twice in the same place with nothing to tell them why.
            if (controller_.awaitingInput() &&
                controller_.promptKind() == command::ParamKind::Text)
                openTextEditor(at);

            snap_preview_valid_ = false;
            update();
            return;
        }

        // A grip under the pointer takes the press: the user is reaching for a
        // corner of something already selected, and a selection box started there
        // would throw that selection away on the way to editing it.
        if (const Grip grip = gripAt(event->position()); grip.valid()) {
            drag_grip_     = grip;
            dragging_grip_ = true;
            drag_anchor_   = event->position();
            cursor_        = event->position();
            cursor_valid_  = true;
            setCursor(Qt::ClosedHandCursor);
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

    // Which corner the pointer could take hold of. Only while nothing is being
    // dragged and no command is asking for a point: during a drag the answer is
    // already decided, and during a command the click belongs to the command.
    if (!dragging_grip_ && !panning_ && !selecting_ && !controller_.awaitingInput())
        hover_grip_ = gripAt(cursor_);
    else if (!dragging_grip_)
        hover_grip_ = Grip{};

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

    if (event->button() == Qt::LeftButton && dragging_grip_) {
        dragging_grip_ = false;
        cursor_        = event->position();
        cursor_valid_  = true;
        unsetCursor();

        commitGripDrag();

        // The corner numbering has changed under an insert, and the geometry under
        // both, so whatever was remembered about the old shape is stale.
        drag_grip_          = Grip{};
        hover_grip_         = Grip{};
        snap_preview_valid_ = false;
        update();
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
    double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) return;
    if (look_.invert_wheel) steps = -steps;

    // The step is a PERCENTAGE of the current scale, so every notch feels the same
    // at every zoom. 20 % is the default and the range is wide on purpose: the
    // people who want three notches per decade and the people who want thirty are
    // both right about their own hands.
    const double factor = 1.0 + static_cast<double>(look_.zoom_percent) / 100.0;
    view_.zoom_at(render::ScreenPoint{event->position().x(), event->position().y()},
                  std::pow(factor, steps));
    publishViewScale();
    updateSnapPreview();
    emit viewChanged();
    update();
}

void MapCanvas::setDiagonalLock(bool on)
{
    if (diagonal_lock_ == on) return;
    diagonal_lock_ = on;

    // THROUGH THE BUS, as a setting write, because the same lock has to be
    // reachable by typing `MOD köşegen=evet` and by a script (Article 1.2,
    // 5.15). Ctrl is a way of holding a mode down, not a capability of its own —
    // otherwise "draw me a square" would be a thing only a mouse could ask for.
    command::Args args;
    args.set("ad", command::Value::text("köşegen"));
    args.set("deger", command::Value::boolean(on));
    controller_.runInvocation(
        command::Invocation{"core.mode", std::move(args), command::Origin::Gui});

    // The preview is what makes the lock legible: the rubber band must snap to
    // the diagonal the moment the key goes down, not at the next mouse move.
    updateSnapPreview();
    update();
}

void MapCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) setDiagonalLock(false);
    QWidget::keyReleaseEvent(event);
}

void MapCanvas::keyPressEvent(QKeyEvent* event)
{
    // Held, not toggled: the lock lasts exactly as long as the key does.
    if (event->key() == Qt::Key_Control) {
        setDiagonalLock(true);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        // The text box first: it is the innermost thing open, and ESC in it means
        // "not this caption" rather than "not this command". A second ESC then
        // cancels METİN, which is the nesting a user expects.
        if (text_editor_ != nullptr && text_editor_->isVisible()) {
            closeTextEditor();
            update();
            return;
        }
        if (selecting_) {
            selecting_ = false;
            update();
            return;
        }
        if (controller_.session()) {
            controller_.cancelInteractive();
            closeTextEditor();
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

// ------------------------------------------------------ the text editor ----

void MapCanvas::openTextEditor(const QPointF& where)
{
    if (text_editor_ == nullptr) {
        text_editor_ = new QLineEdit(this);
        text_editor_->setObjectName(QStringLiteral("canvasTextEditor"));
        text_editor_->setAccessibleName(tr("Çizime yazılacak metin"));
        text_editor_->setMinimumWidth(180);

        // ENTER COMMITS, ESC CANCELS, and both go through the controller rather
        // than touching the document: this box is a client of the command bus
        // like every other (CLAUDE.md Article 1.2). `editingFinished` is NOT used
        // — it also fires on focus loss, which would commit a caption the user was
        // walking away from.
        // ESC INSIDE THE BOX. A focused QLineEdit consumes the key, so the
        // canvas handler never sees it; without this the box could only be
        // dismissed by typing something and pressing Enter.
        auto* give_up = new QShortcut(QKeySequence(Qt::Key_Escape), text_editor_);
        give_up->setContext(Qt::WidgetShortcut);
        connect(give_up, &QShortcut::activated, this, [this] {
            closeTextEditor();
            update();
        });

        connect(text_editor_, &QLineEdit::returnPressed, this, [this] {
            const QString typed = text_editor_->text();
            closeTextEditor();
            if (!typed.isEmpty()) controller_.supplyText(typed);
            update();
        });
    }

    // Placed where the caption will start, and nudged back inside when the click
    // was near the right or bottom edge — a box drawn off the canvas is a box the
    // user cannot type into.
    const int w = std::max(180, width() / 4);
    const int h = text_editor_->sizeHint().height();
    const int x = std::clamp(static_cast<int>(where.x()), 0, std::max(0, width() - w));
    const int y = std::clamp(static_cast<int>(where.y()) - h / 2, 0, std::max(0, height() - h));

    text_editor_->setGeometry(x, y, w, h);
    text_editor_->clear();
    text_editor_->show();
    text_editor_->setFocus(Qt::OtherFocusReason);
}

void MapCanvas::closeTextEditor()
{
    if (text_editor_ == nullptr) return;
    text_editor_->hide();
    text_editor_->clear();
    setFocus(Qt::OtherFocusReason);
}

} // namespace piricad::app
