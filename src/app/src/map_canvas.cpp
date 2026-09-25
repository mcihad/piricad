// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/map_canvas.hpp"

#include "kentos_cad/app/backend_factory.hpp"
#include "kentos_cad/app/controller.hpp"
#include "kentos_cad/app/text_engine.hpp"
#include "kentos_cad/command/aids.hpp"
#include "kentos_cad/command/ghost.hpp"
#include "kentos_cad/command/path_edit.hpp"
#include "kentos_cad/core/angle.hpp"
#include "kentos_cad/core/arc.hpp"
#include "kentos_cad/core/area_edit.hpp"
#include "kentos_cad/core/block_reference.hpp"
#include "kentos_cad/core/break_run.hpp"
#include "kentos_cad/core/circle.hpp"
#include "kentos_cad/core/corner.hpp"
#include "kentos_cad/core/curve_path.hpp"
#include "kentos_cad/core/dimension.hpp"
#include "kentos_cad/core/dimension_link.hpp"
#include "kentos_cad/core/ellipse.hpp"
#include "kentos_cad/core/fillet.hpp"
#include "kentos_cad/core/grips.hpp"
#include "kentos_cad/core/guide.hpp"
#include "kentos_cad/core/identity.hpp"
#include "kentos_cad/core/offset.hpp"
#include "kentos_cad/core/outline.hpp"
#include "kentos_cad/core/parallel.hpp"
#include "kentos_cad/core/pick.hpp"
#include "kentos_cad/core/polygon.hpp"
#include "kentos_cad/core/settings.hpp"
#include "kentos_cad/core/spline.hpp"
#include "kentos_cad/core/trig.hpp"
#include "kentos_cad/core/trim_curve.hpp"
#include "kentos_cad/render/backend.hpp"
#include "kentos_cad/render/snap_marker.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QShortcut>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace kentos::app {

MapCanvas::MapCanvas(Controller& controller, QWidget* parent)
    : CanvasSurface(parent), controller_(controller), backend_(make_canvas_backend())
{
#if KENTOS_HAVE_RHI
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

    // THE TEXT BOX LIVES AS LONG AS THE QUESTION IT ANSWERS. It closed on Enter
    // and on Esc only, so a caption given up any other way — another tool
    // pressed on the ribbon, the command cancelled at the command line — left
    // an empty box floating over the drawing, and it took every click that
    // landed on it: a dimension placed there simply never happened.
    connect(&controller_, &Controller::promptChanged, this, [this](const QString&) {
        const bool asking_text =
            controller_.awaitingInput() && controller_.promptKind() == command::ParamKind::Text;
        const bool open = text_editor_ != nullptr && text_editor_->isVisible();
        // A QUESTION THAT SAYS WHERE ITS WORDS WILL STAND opens the box there
        // (TODOS C-12): a leader's words are typed beside its landing, not at
        // the bottom of the window after the last point was clicked.
        if (asking_text && !open) {
            const command::Session* live = controller_.session();
            if (live != nullptr && live->prompt().text_at) {
                const render::ScreenPoint at = view_.to_screen(*live->prompt().text_at);
                openTextEditor(QPointF(at.x, at.y),
                               live->prompt().text_leftward ? BoxAlign::Right : BoxAlign::Left);
            }
            return;
        }
        if (!open || asking_text) return;
        closeTextEditor();
    });
}

void MapCanvas::publishViewScale()
{
    // The only number the aid layer cannot work out for itself. Everything else
    // about snapping — modes, ortho, polar step, grid — lives in the settings and
    // is readable by every client (kentos_cad/command/aids.hpp).
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

bool MapCanvas::hasGpuContext() const
{
#if KENTOS_HAVE_RHI
    return rhi() != nullptr;
#else
    return true;
#endif
}

QString MapCanvas::backendName() const
{
    return QString::fromStdString(backend_->name());
}

bool MapCanvas::backendIsGpu() const
{
    return backend_ && backend_->gpu();
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

void MapCanvas::zoomToBox(const core::Box2& box)
{
    if (box.empty()) return;
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

void MapCanvas::setCentre(core::Point2 centre)
{
    view_.set_centre(centre, view_.mm_per_pixel());
    snap_preview_valid_ = false;
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
    CanvasSurface::resizeEvent(event);
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

    look_.ruler           = store.get("core.cetvel.gorunur").as_bool();
    look_.ruler_px        = static_cast<int>(store.get("core.cetvel.kalinlik").as_int());
    look_.ruler_unit      = static_cast<int>(store.get("core.cetvel.birim").as_enum());
    look_.scale_bar       = store.get("core.harita.olcek_cubugu").as_bool();
    look_.north           = store.get("core.harita.kuzey_oku").as_bool();
    look_.readout         = store.get("core.harita.koordinat_gostergesi").as_bool();
    look_.hint_px         = static_cast<int>(store.get("core.harita.ipucu_boyu").as_int());
    look_.cursor          = static_cast<int>(store.get("core.harita.imlec").as_enum());
    look_.cursor_px       = static_cast<int>(store.get("core.harita.imlec_boyu").as_int());
    look_.marker_px       = static_cast<int>(store.get("core.yakalama.isaret_boyu").as_int());
    look_.snap_tip        = store.get("core.yakalama.ipucu").as_bool();
    look_.dynamic_input   = store.get("core.arayuz.dinamik_girdi").as_bool();
    look_.pick_px         = static_cast<double>(store.get("core.secim.tolerans").as_int());
    options_.line_weights = store.get("core.harita.kalinlik").as_bool();
    if (underMouse()) applyPointer();
    // Through the bus, which knows which store each of the two settings lives
    // in: the unit is a PROJECT setting and reading it from the app store — as
    // this did — answered the fallback, so `AYAR açı_birimi derece` never
    // reached the readout.
    look_.angle = controller_.bus().angle_convention();
    look_.step  = controller_.bus().session_settings().get("core.yakalama.adim").as_length();

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
    const bool picking_point  = capture_ == Capture::Point;
    if (!asking && !dragging_grip_ && !picking_point) return;

    command::Bus& bus = controller_.bus();
    // The SAME aids the command will apply to this prompt (`command::aids_for`).
    const command::AidSettings aids =
        asking ? command::aids_for(bus.aid_settings(), session->prompt()) : bus.aid_settings();
    const core::Point2 aim = view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    // The SAME base the command will use, so the marker and the result cannot
    // disagree. `KÖŞETAŞI` measures from the corner being moved and `KÖŞEEKLE`
    // from the corner the edge leaves; a drag that previewed against some other
    // origin would put dik mod and kutupsal on a different ray than the one the
    // corner actually lands on.
    const bool has_base =
        asking ? command::aimed_from_origin(session->prompt()) : drag_grip_.valid();
    const core::Point2 base = asking ? session->prompt().rubber_origin : drag_grip_.base;

    // AND THE SAME MARKS AND THE SAME RUN: the tracking marks the user left, and
    // the corners of the run in progress (`command::pending_run`) — the first
    // corner of an ALAN is not in the document yet, and closing on it is the
    // most common snap a parcel boundary has. The command resolves the click
    // with both; a marker that left them out promised a different point.
    const command::PendingRun run =
        asking ? command::pending_run(session->prompt()) : command::PendingRun{};
    const core::SnapResult r = bus.aids().resolve(controller_.document(), aids, aim, has_base, base,
                                                  bus.tracking_marks(), run);
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

    // MORE THAN ONE THING UNDER THE CURSOR IS A QUESTION, not a tie to break
    // silently. On a plan sheet a click lands on a parcel, on the boundary that
    // closes it and on the ada boundary over that; `pick_nearest` answers with
    // one of them, correctly and unhelpfully. The shell asks.
    //
    // The candidates are READ here and chosen elsewhere: this emits, the window
    // that opens sends `SEÇ`, and the document is never touched from a widget
    // (Article 5.9). The radius is the command's own `pick_radius`, so the list
    // holds exactly what `SEÇ mod=NOKTA` would have been choosing between.
    const bool picking =
        controller_.awaitingInput() && controller_.promptKind() == command::ParamKind::Selection;
    std::int64_t order = 0; // `sira`: which of the things under the cursor; 0, the nearest
    if (!is_box) {
        std::vector<core::EntityId> under;
        core::pick_all(controller_.document(), a, controller_.bus().aid_settings().pick_radius,
                       under);
        // UNLESS THE QUESTION ALREADY SAYS WHICH. A command that acts on
        // hatches only asks for hatches, and the parcel under this one is not
        // an answer to it: the one hatch there is taken, by its place in the
        // same list `SEÇ mod=NOKTA sira=` counts.
        if (under.size() > 1 && picking && controller_.promptPickKind() != core::kNoKind) {
            const core::EntityTable& table = controller_.document().entities();
            std::size_t matches            = 0;
            for (std::size_t i = 0; i < under.size(); ++i)
                if (table.kind[under[i]] == controller_.promptPickKind()) {
                    ++matches;
                    order = static_cast<std::int64_t>(i) + 1;
                }
            if (matches != 1) order = 0;
        }
        if (under.size() > 1 && order == 0) {
            emit pickAmbiguous(under, mods);
            return;
        }
    }

    command::Args args;
    args.set("mod", command::Value::text(is_box ? "KUTU" : "NOKTA"));
    args.set("noktalar", is_box ? command::Value::points({a, b}) : command::Value::points({a}));
    if (order > 1) args.set("sira", command::Value::number(static_cast<double>(order)));

    // QGIS keys, because that is where the CBS half of this product's users come
    // from: Shift adds, Ctrl removes, a plain click replaces.
    //
    // WHILE A COMMAND IS ASKING WHICH OBJECTS, a plain click ADDS. The question
    // is "which ones", plural, and a click that replaced the answer so far made
    // BİRLEŞTİR — which needs two — impossible to answer by pointing: the second
    // object threw the first away. Ctrl still removes; Shift adds as it did.
    if (mods.testFlag(Qt::ShiftModifier) || (picking && !mods.testFlag(Qt::ControlModifier)))
        args.set("islem", command::Value::text("EKLE"));
    else if (mods.testFlag(Qt::ControlModifier))
        args.set("islem", command::Value::text("ÇIKAR"));

    controller_.runInvocation(
        command::Invocation{"core.select", std::move(args), command::Origin::Gui});
}

void MapCanvas::buildSelection()
{
    selection_runs_ = 0;
    // Not named `slots`: Qt defines that as a macro (qobjectdefs.h).
    const auto& selected = controller_.selectedSlots();
    if (selected.empty()) return;

    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();

    // A CLIPPED REFERENCE SHOWS ITS BOUNDARY while it is selected, dashed: the
    // frame AutoCAD shows on the screen and never prints, so a user can see
    // where the drawing stops on purpose (`BlockReference::clip`). Its batch
    // is made first — the selection's own is taken by reference below and
    // must be the last one made.
    clip_frames_             = 0;
    const std::size_t frames = nextBatch(palette_.selection.rgba(), 1.5f, true);
    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e) || table.kind[e] != core::kBlockReferenceKind)
            continue;
        auto ref = core::block_reference_of(geom, table.slot[e]);
        if (!ref || ref.value().clip.empty() || ref.value().block >= doc.blocks().size()) continue;
        const core::BlockReference& placed = ref.value();
        const core::Point2 at              = core::block_reference_insertion(geom, table.slot[e]);
        const core::Point2 base            = doc.blocks().at(placed.block).base;
        for (int row = 0; row < static_cast<int>(placed.rows); ++row)
            for (int col = 0; col < static_cast<int>(placed.columns); ++col) {
                clip_frame_.clear();
                for (const core::Point2 p : placed.clip)
                    clip_frame_.push_back(render::to_f(
                        view_.to_screen(core::place_block_point(placed, at, base, p, col, row))));
                addRun(frames, clip_frame_, true);
                ++clip_frames_;
            }
    }

    // Taken by reference AFTER the last `nextBatch` of this function, which is
    // the only shape in which that is safe: nothing below grows the vector.
    render::OverlayBatch& batch =
        overlay_.batches[nextBatch(palette_.selection.rgba(), 3.0f, false)];

    // Walked per selected entity, never per document entity: a selection is
    // O(hundreds) and the frame budget belongs to the drawing (§10.1).
    for (core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;

        // A CAPTION IS OUTLINED AROUND ITS LETTERS. Its ring is the hairline under
        // them, so highlighting the ring drew a rule beneath a word and left the
        // word looking untouched — the same complaint as the circle below, in a
        // different disguise. `core::text_quad` owns that shape; the pick test
        // and this outline have to agree about where the caption is.
        if (std::array<core::Point2, 4> quad; core::text_quad(doc, e, quad)) {
            const auto before = static_cast<std::uint32_t>(batch.xs.size());
            for (const core::Point2 corner : quad) {
                const render::ScreenPointF q = render::to_f(view_.to_screen(corner));
                batch.xs.push_back(q.x);
                batch.ys.push_back(q.y);
            }
            batch.runs.push_back(static_cast<std::uint32_t>(batch.xs.size()) - before);
            batch.closed.push_back(1);
            continue;
        }

        // THE SHAPE, not the stored vertices. A circle keeps a centre and a radius
        // handle in its ring; highlighting those draws a line pointing east over a
        // circle the user can see is selected nowhere. An ellipse keeps a centre
        // and two axis ends, and highlighting those drew a triangle inside it.
        // Every kind that is not its vertices goes through the one tessellator
        // the picture uses (`core::curve_outline`), so the outline and the
        // drawing agree.
        curve_scratch_x_.clear();
        curve_scratch_y_.clear();
        const bool curve =
            table.kind[e] != core::kPolylineKind && table.kind[e] != core::kPointKind;
        bool curve_closed = table.kind[e] != core::kArcKind;
        if (table.kind[e] == core::kCircleKind)
            core::circle_outline(core::circle_centre_of(geom, table.slot[e]),
                                 core::circle_radius_of(geom, table.slot[e]), curve_scratch_x_,
                                 curve_scratch_y_);
        else if (table.kind[e] == core::kArcKind)
            core::arc_outline(
                core::arc_centre_of(geom, table.slot[e]), core::arc_radius_of(geom, table.slot[e]),
                core::arc_start_of(geom, table.slot[e]), core::arc_end_of(geom, table.slot[e]),
                curve_scratch_x_, curve_scratch_y_);
        else if (curve) {
            // EVERY RUN THE KIND DRAWS. A block reference draws each of its
            // members, a hatch its boundary and its holes; outlining only the
            // first run lit one line of a selected symbol and left the rest
            // looking unselected (TODOS C-07).
            core::EmitBuffer outline;
            if (core::entity_outline(doc, e, outline) && outline.run_total() > 0) {
                std::size_t at = 0;
                for (std::size_t r = 0; r < outline.run_total(); ++r) {
                    const std::size_t n = outline.run_count[r];
                    // A fill-only run's edge is a clip's cut; the highlight
                    // lights what is drawn (`EmitBuffer::run_fill_only`).
                    if (n >= 2 && outline.run_edge(r)) {
                        const auto before = static_cast<std::uint32_t>(batch.xs.size());
                        for (std::size_t v = at; v < at + n; ++v) {
                            const render::ScreenPointF q = render::to_f(
                                view_.to_screen(core::Point2{outline.xs[v], outline.ys[v]}));
                            batch.xs.push_back(q.x);
                            batch.ys.push_back(q.y);
                        }
                        batch.runs.push_back(static_cast<std::uint32_t>(batch.xs.size()) - before);
                        batch.closed.push_back(outline.run_closed[r] != 0 ? 1 : 0);
                        ++selection_runs_;
                    }
                    at += n;
                }
            }
            continue;
        }

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
                (curve ? curve_closed : geom.ring_role[r] != core::RingRole::Open) ? 1 : 0);
            ++selection_runs_;
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

        // EVERY OTHER KIND OFFERS ITS GRIPS — a circle's centre and quadrants, an
        // arc's ends, an ellipse's axis ends, a dimension's definition points —
        // from the one table `KÖŞETAŞI` edits by (core/grips.hpp). Only a
        // polyline has EDGES a new corner can go into, so only it is searched
        // for an edge hit below.
        const bool locked = !doc.editable(e);
        if (table.kind[e] != core::kPolylineKind) {
            const auto grips = core::grip_places(doc, e);
            for (std::size_t i = 0; i < grips.size(); ++i) {
                const render::ScreenPoint p = view_.to_screen(grips[i].at);
                const double dx             = p.x - where.x();
                const double dy             = p.y - where.y();
                const double d2             = dx * dx + dy * dy;
                if (d2 < corner_best) {
                    corner_best = d2;
                    corner_hit  = Grip{e,           static_cast<std::int64_t>(i + 1),
                                      false,       grips[i].at,
                                      grips[i].at, locked};
                }
            }
            continue;
        }

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
                    corner_hit  = Grip{e,
                                      number,
                                      false,
                                      core::Point2{xs[v], ys[v]},
                                      core::Point2{xs[v], ys[v]},
                                      locked};
                }

                // The edge LEAVING this corner. On an open ring the last vertex has
                // none — a polyline's ends are not joined (R10) — which is exactly
                // what `KÖŞEEKLE` refuses, so the canvas never offers it either.
                const bool last = v + 1 == xs.size();
                if (last && !closed) continue;

                const std::size_t next      = last ? 0 : v + 1;
                const render::ScreenPoint q = view_.to_screen(core::Point2{xs[next], ys[next]});

                const double ex   = q.x - p.x;
                const double ey   = q.y - p.y;
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
                    edge_hit        = Grip{
                        e, number, true, view_.to_world(foot), core::Point2{xs[v], ys[v]}, locked};
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

std::vector<std::pair<core::EntityId, std::size_t>> MapCanvas::sharedGripsAt(core::Point2 at) const
{
    std::vector<std::pair<core::EntityId, std::size_t>> out;
    const core::Document& doc = controller_.document();
    for (const core::EntityId e : controller_.selectedSlots()) {
        if (e >= doc.entities().size() || !doc.entities().visible(e)) continue;
        const auto grips = core::entity_grips(doc, e);
        for (std::size_t i = 0; i < grips.size(); ++i)
            if (grips[i].at == at) out.emplace_back(e, i);
    }
    return out;
}

QString MapCanvas::gripLine(const Grip& grip) const
{
    const core::Document& doc = controller_.document();
    const auto key_of         = [&doc](core::EntityId e) {
        return QString::number(core::raw(doc.entities().key[e]));
    };
    if (grip.insert)
        return QStringLiteral("KÖŞEEKLE nesne=%1 kose=%2")
            .arg(key_of(grip.entity))
            .arg(grip.corner);
    // THE CLICKED OBJECT FIRST, so `kose` names its grip; every other selected
    // object with a grip at the same place follows it.
    QStringList keys{key_of(grip.entity)};
    if (!grip.locked)
        for (const auto& [e, index] : sharedGripsAt(grip.at))
            if (e != grip.entity && !keys.contains(key_of(e))) keys << key_of(e);
    return QStringLiteral("KÖŞETAŞI nesne=%1 kose=%2")
        .arg(keys.join(QLatin1Char(' ')))
        .arg(grip.corner);
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

        // EVERY OTHER SELECTED OBJECT SHARING THE CORNER follows it too, drawn
        // by its own kind as it will be (`core::grip_preview`).
        if (!drag_grip_.insert) {
            const std::size_t shared = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);
            for (const auto& [other, index] : sharedGripsAt(drag_grip_.at)) {
                if (other == drag_grip_.entity) continue;
                core::EmitBuffer buf;
                if (core::grip_preview(doc, other, index, to, buf)) addEmitRuns(shared, buf);
            }
        }

        const core::EntityId e = drag_grip_.entity;
        if (e < table.size() && table.visible(e) && table.kind[e] != core::kPolylineKind) {
            // The shape the grip table says this drag makes, drawn by the kind's
            // own outline (`core::grip_preview`): a circle stays a circle while
            // its quadrant is pulled.
            const std::size_t batch = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);
            core::EmitBuffer buf;
            if (drag_grip_.corner >= 1 &&
                core::grip_preview(doc, e, static_cast<std::size_t>(drag_grip_.corner - 1), to,
                                   buf))
                addEmitRuns(batch, buf);
        } else if (e < table.size() && table.visible(e)) {
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

    // A SPLINE'S HANDLES ARE OFF THE CURVE: its control points, joined by a
    // thin dashed frame so it reads which handle pulls which part (TODOS C-07).
    // Drawn under the handles, in the selection's ink.
    const std::size_t frame = nextBatch(palette_.selection.rgba(), 1.0f, true);
    for (const core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e) || table.kind[e] != core::kSplineKind) continue;
        const auto grips = core::entity_grips(doc, e);
        std::vector<render::ScreenPointF> run;
        run.reserve(grips.size());
        for (const core::GripPoint& g : grips)
            run.push_back(render::to_f(view_.to_screen(g.at)));
        if (run.size() >= 2) addRun(frame, run, false);
    }

    // The handles themselves, drawn over the outline so a corner is grabbable
    // wherever two objects meet — in the lock's ink on an object that cannot be
    // edited now, so the lock shows rather than the handles going missing.
    const std::size_t plain = nextBatch(palette_.selection.rgba(), 1.0f, false);
    const std::size_t lit   = nextBatch(tokens_->accent.rgba(), 2.0f, false);
    const std::size_t lock  = nextBatch(tokens_->warn.rgba(), 1.0f, false);

    constexpr float kHalf = 3.0f;

    // A HOT GRIP LIGHTS EVERY GRIP AT ITS PLACE: the corner two selected
    // parcels share moves in both, and both are shown to.
    const bool hovering = hover_grip_.valid() && !hover_grip_.insert && !hover_grip_.locked;
    for (const core::EntityId e : selected) {
        if (e >= table.size() || !table.visible(e)) continue;
        const bool frozen = !doc.editable(e);
        if (table.kind[e] != core::kPolylineKind) {
            // The kind's grips: a square where a point is a point, a circle where
            // a handle sets a size (radius, arc bend, caption).
            const auto grips = core::grip_places(doc, e);
            for (std::size_t i = 0; i < grips.size(); ++i) {
                const render::ScreenPointF p = render::to_f(view_.to_screen(grips[i].at));
                const bool hot               = !frozen && hovering && grips[i].at == hover_grip_.at;
                std::size_t ink              = hot ? lit : plain;
                if (frozen) ink = lock;
                const core::GripRole role = grips[i].role;
                if (role == core::GripRole::Radius || role == core::GripRole::ArcMid ||
                    role == core::GripRole::Caption || role == core::GripRole::Rotation)
                    addCircle(ink, p.x, p.y, kHalf);
                else
                    addRun(ink,
                           {{p.x - kHalf, p.y - kHalf},
                            {p.x + kHalf, p.y - kHalf},
                            {p.x + kHalf, p.y + kHalf},
                            {p.x - kHalf, p.y + kHalf}},
                           true);
            }
            continue;
        }

        const core::RingSpan span = geom.rings_of(table.slot[e]);

        for (std::uint32_t r = span.first; r < span.first + span.count; ++r) {
            const auto xs = geom.ring_xs(r);
            const auto ys = geom.ring_ys(r);

            for (std::size_t v = 0; v < xs.size(); ++v) {
                const render::ScreenPointF p =
                    render::to_f(view_.to_screen(core::Point2{xs[v], ys[v]}));

                const bool hot =
                    !frozen && hovering && core::Point2{xs[v], ys[v]} == hover_grip_.at;

                std::size_t ink = hot ? lit : plain;
                if (frozen) ink = lock;
                addRun(ink,
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
    // A PRESS AND RELEASE WITHOUT TRAVEL MAKES THE GRIP HOT, as in every CAD:
    // the command starts on it and asks for the new place, the object follows
    // the pointer, and the next click puts it down — Esc leaves it where it was.
    // It used to do nothing at all, so a user who clicked a corner and moved
    // the mouse saw nothing happen (TODOS C-07).
    const QPointF moved = cursor_ - drag_anchor_;
    if (moved.manhattanLength() < QApplication::startDragDistance()) {
        controller_.beginOneShot(gripLine(drag_grip_), command::Origin::Gui);
        return;
    }

    const core::Point2 world = view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

    // A CORNER SELECTED OBJECTS SHARE moves in all of them, in one step: named
    // by its place, so every one of them finds its own grip there.
    if (!drag_grip_.insert) {
        std::vector<std::int64_t> keys;
        for (const auto& [e, index] : sharedGripsAt(drag_grip_.at)) {
            const auto key =
                static_cast<std::int64_t>(core::raw(controller_.document().entities().key[e]));
            if (std::ranges::find(keys, key) == keys.end()) keys.push_back(key);
        }
        if (keys.size() > 1) {
            command::Args args;
            args.set("nesne", command::Value::ids(keys));
            args.set("kaynak", command::Value::point(drag_grip_.at));
            args.set("nokta", command::Value::point(world));
            controller_.runInvocation(
                command::Invocation{"core.vertex_move", std::move(args), command::Origin::Gui});
            return;
        }
    }

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
    args.set("nesne", command::Value::ids({static_cast<std::int64_t>(core::raw(key))}));
    args.set("kose", command::Value::integer(drag_grip_.corner));
    args.set("nokta", command::Value::point(world));

    controller_.runInvocation(
        command::Invocation{drag_grip_.insert ? "core.vertex_insert" : "core.vertex_move",
                            std::move(args), command::Origin::Gui});
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
    // THE MARK IS `render::snap_marker`'S, not a switch here. A switch with a
    // `default: break;` is a silent hole: a mode with no case drew nothing at
    // all, so the aid fired, the point moved, and the marker said it had not.
    // Seven modes had grown that hole — the surface normal, the quadrant, the
    // tangent, the guide, the centroid, the tracking mark and the step — and a
    // test now walks every bit and fails when one draws nothing
    // (render/snap_marker.hpp).
    const render::Marker mark = render::snap_marker(snap_preview_.mode, x, y, h);
    for (const render::MarkerRun& stroke : mark.runs)
        addRun(batch, stroke.points, stroke.closed);
    if (mark.ring > 0.0F) addCircle(batch, x, y, mark.ring);

    // The label says which aid fired. Without it a user cannot tell an endpoint
    // from an intersection when both glyphs sit under the cursor — and with the
    // constructed modes on there is more to tell apart, not less.
    if (look_.snap_tip)
        overlay_.labels.push_back(
            render::OverlayLabel{ink, x + h + 4.0f, y - h - 2.0f, static_cast<float>(look_.hint_px),
                                 false, std::string(core::snap_mode_label(snap_preview_.mode))});
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

/// The direction from `a` to `b`, written under the session's angle convention.
///
/// AZIMUT by default — CLOCKWISE FROM NORTH, in GRAD — not the mathematical
/// angle counter-clockwise from east. That is the number a Turkish surveyor reads
/// off a total station, writes in a traverse sheet and types into a setting-out
/// list, and getting it wrong by ninety degrees or by a sign is the kind of
/// mistake that reaches a parsel corner. A user who set `MOD kural matematik`
/// reads the mathematical angle here too, because what the readout shows and
/// what `@mesafe<açı` means must be one convention (TODOS-CAD P0-4).
std::string bearing_text(core::Point2 a, core::Point2 b, core::AngleConvention convention);

std::string trimmed(double value, int places)
{
    // THE TURKISH DECIMAL COMMA, as every other figure this program prints: the
    // transcript said "20,000 m" and the line being dragged said "10.77 m" beside
    // "124,2238 grad" — two conventions in one label. The comma is put in after
    // formatting, so the digits are the ones `QString::number` produced.
    std::string out       = QString::number(value, 'f', places).toStdString();
    const std::size_t dot = out.find('.');
    if (dot == std::string::npos) return out;
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.') {
        out.pop_back();
        return out;
    }
    out[dot] = ',';
    return out;
}

std::string bearing_text(core::Point2 a, core::Point2 b, core::AngleConvention convention)
{
    if (a == b) return {};

    // The one direction-and-text pair the whole program uses — ÖLÇ and
    // APLİKASYON print through the same two functions — so the figure on the
    // dragged line is the figure the report will show.
    return core::angle_text(core::direction_turns(a, b, convention.rule), convention.unit);
}

} // namespace

void MapCanvas::buildGuides()
{
    const core::GuideStore& guides = controller_.document().guides();
    if (guides.empty() && dragging_guide_ < 0) return;

    // The aid colour, dashed, one batch for all of them.
    const std::size_t batch = nextBatch(tokens_->warn.rgba(), 1.0f, true);
    const auto band         = static_cast<float>(look_.ruler ? look_.ruler_px : 0);

    // THROUGH `render::to_f`, never by casting a coordinate. A TUREF easting is
    // 4·10^8 millimetres and a float has 24 bits of mantissa, so narrowing an
    // absolute world value costs metres on screen — the offset is subtracted
    // first, inside the view, and this uses the one helper that does it
    // (render.md R2, P1).
    for (std::size_t i = 0; i < guides.size(); ++i) {
        if (guides.axis(i) == core::GuideAxis::Angled) {
            // EXTENDED IN SCREEN SPACE, not in the world. Running the line a huge
            // distance in millimetres and letting the view transform it is the
            // obvious way and it does not work: ten thousand kilometres past the
            // origin lands at a screen coordinate no float carries usefully, and
            // the widening pass then draws nothing at all — the 45° guide of the
            // first attempt was simply absent from the frame.
            //
            // So the DIRECTION comes from the world, once, by projecting the point
            // and a metre along it; the ENDS are that direction extended by a
            // bounded number of pixels. `kReach` is a pixel count larger than any
            // window and small enough to stay exact in a float, which is the whole
            // requirement. A ray runs one way only, and its near end is its own
            // point.
            constexpr float kReach = 1.0e5F;
            const core::SinCos dir = core::sin_cos_udeg(guides.angle(i));
            const core::Point2 at  = guides.through(i);
            const core::Point2 step{at.x + core::mm_round(dir.cos * 1000.0),
                                    at.y + core::mm_round(dir.sin * 1000.0)};

            const render::ScreenPointF origin = render::to_f(view_.to_screen(at));
            const render::ScreenPointF along  = render::to_f(view_.to_screen(step));
            const float dx                    = along.x - origin.x;
            const float dy                    = along.y - origin.y;
            const float len                   = std::hypot(dx, dy);
            if (len <= 0.0F) continue; ///< zoomed so far out that a metre is nothing

            const float ux = dx / len;
            const float uy = dy / len;
            const render::ScreenPointF ahead{origin.x + ux * kReach, origin.y + uy * kReach};
            const render::ScreenPointF back =
                guides.ray(i)
                    ? origin
                    : render::ScreenPointF{origin.x - ux * kReach, origin.y - uy * kReach};
            addRun(batch, {back, ahead}, false);
            continue;
        }
        if (guides.axis(i) == core::GuideAxis::Horizontal) {
            const render::ScreenPointF p =
                render::to_f(view_.to_screen(core::Point2{0, guides.coordinate(i)}));
            if (p.y < band || p.y > static_cast<float>(height())) continue;
            addRun(batch, {{band, p.y}, {static_cast<float>(width()), p.y}}, false);
        } else {
            const render::ScreenPointF p =
                render::to_f(view_.to_screen(core::Point2{guides.coordinate(i), 0}));
            if (p.x < band || p.x > static_cast<float>(width())) continue;
            addRun(batch, {{p.x, band}, {p.x, static_cast<float>(height())}}, false);
        }
    }

    // The one under the cursor while it is being dragged, so the user sees where
    // it will land before they let go.
    if (dragging_guide_ >= 0 && cursor_valid_) {
        // `toScreenF`, not a cast: the cursor is already in widget pixels, and
        // using the one conversion every other overlay uses keeps this line out of
        // the narrowing rule's way as well as out of its heuristic's.
        const render::ScreenPointF at = toScreenF(cursor_);
        if (dragging_guide_ == 0)
            addRun(batch, {{band, at.y}, {static_cast<float>(width()), at.y}}, false);
        else
            addRun(batch, {{at.x, band}, {at.x, static_cast<float>(height())}}, false);
    }
}

void MapCanvas::buildTracking()
{
    // THE TRACES, so a mark is visible as a line and not only as a place the
    // cursor jumps to. A user who cannot see what they acquired cannot tell a
    // trace from a snap that happened to agree with it.
    const auto& marks = controller_.bus().tracking_marks();
    if (marks.empty()) return;

    // THE AID COLOUR, dashed — the same ink a snap marker and a drafting guide
    // use. `warn` means exactly one thing in this program and a trace is that
    // thing; the accent means selection, active tool and primary action, and a
    // trace is none of those (design.md §1.2).
    const std::size_t batch = nextBatch(tokens_->warn.rgba(), 1.0f, true);
    const auto w            = static_cast<float>(width());
    const auto h            = static_cast<float>(height());

    for (const core::Point2 at : marks) {
        const render::ScreenPointF p = render::to_f(view_.to_screen(at));
        addRun(batch, {{0.0F, p.y}, {w, p.y}}, false);
        addRun(batch, {{p.x, 0.0F}, {p.x, h}}, false);

        // AND THE MARK ITSELF, as a small square: the two lines cross at every
        // trace pair, so the crossing alone does not say which points were
        // acquired.
        constexpr float kMark = 4.0F;
        addRun(batch,
               {{p.x - kMark, p.y - kMark},
                {p.x + kMark, p.y - kMark},
                {p.x + kMark, p.y + kMark},
                {p.x - kMark, p.y + kMark},
                {p.x - kMark, p.y - kMark}},
               false);
    }
}

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
    for (unsigned q = 0; q < 4; ++q) {
        const float sx = (q & 1U) != 0U ? -1.0f : 1.0f;
        const float sy = (q & 2U) != 0U ? -1.0f : 1.0f;
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

    // ABOVE THE SCALE BAR, and clear of the canvas floor.
    //
    // It sat on the baseline `height() - 4`, which put it half outside the canvas
    // — every descender was cut off — and directly under the scale bar's own three
    // readings, which are drawn at `height() - 20` in the same corner. Two
    // unrelated numbers a few pixels apart in one corner read as one garbled
    // reading, which is what a user sees and reports.
    //
    // The bar's geometry is `buildScaleBar`'s and is restated here rather than
    // shared, because the two are laid out against the same corner and a reader
    // of either needs to see why the other is where it is. Aligned to the same
    // left inset as the bar for the same reason.
    constexpr float kBarTop    = 39.0f; ///< buildScaleBar: kFromFoot + kBarHeight
    constexpr float kInset     = 16.0f; ///< buildScaleBar: from the canvas's left
    constexpr float kClearance = 9.0f;  ///< above the bar, or above the floor

    const auto band      = static_cast<float>(look_.ruler ? look_.ruler_px : 0);
    const float foot     = static_cast<float>(height());
    const float baseline = look_.scale_bar ? foot - kBarTop - kClearance : foot - kClearance;

    overlay_.labels.push_back(render::OverlayLabel{palette_.gridMajor.rgba(), band + kInset,
                                                   baseline, static_cast<float>(look_.hint_px),
                                                   true, text});
}

bool MapCanvas::acceptGuide()
{
    // ENTER ACCEPTS THE FIGURE: while a face is being pulled to a wanted area,
    // Enter sends the point that lands it exactly on the figure, wherever the
    // hand happens to be. Reached from the canvas and from the command line,
    // because focus is on the line far more often than on the drawing.
    const auto* session = controller_.session();
    if (session == nullptr || !session->waiting() ||
        session->prompt().rubber_shape != command::RubberShape::AreaEdit)
        return false;
    const core::AreaGhost ghost = areaGhost();
    if (ghost.points.empty()) return false;
    controller_.supplyPoint(ghost.commit);
    return true;
}

bool MapCanvas::finishPointRun()
{
    // ENTER FINISHES THE SHAPE, exactly as the right button does.
    //
    // A command that reads points until the next one does not come — ÇİZGİ, ALAN,
    // ÇOKLUÇİZGİ, DİKAYAK — is FINISHED by saying "that is all", and the right
    // button was the only way to say it. Enter fell through to nothing, so the
    // only key that ended such a run was Esc, and Esc also PUTS THE TOOL AWAY
    // (`Controller::finishInteractive` versus `cancelInteractive`). The user had
    // to reach for the tool again after every shape and read it as the tool being
    // dropped after every draw.
    //
    // It is also keyboard parity: a capability reachable only with a mouse is one
    // the program may not ship (CLAUDE.md 5.15, ui.md R21).
    //
    // ONLY FOR A POINT PROMPT. A selection is answered by `supplyPickedObjects`,
    // a wanted area by `acceptGuide`, and a name or a number by typing it — an
    // empty Enter at those means "I have nothing to say" and must not end the run.
    const auto* session = controller_.session();
    if (session == nullptr || !session->waiting() ||
        session->prompt().kind != command::ParamKind::Point)
        return false;

    controller_.finishInteractive();
    snap_preview_valid_ = false;
    update();
    return true;
}

core::AreaGhost MapCanvas::areaGhost() const
{
    core::AreaGhost none;
    const auto* session = controller_.session();
    if (session == nullptr || !session->waiting()) return none;
    const auto request = core::decode_area_edit(session->prompt().rubber_payload);
    if (!request) return none;
    const core::Document& doc = controller_.document();
    const core::EntityId e =
        doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(request->key)));
    if (e == core::kNoEntity || !doc.alive(e)) return none;
    const core::RingSpan rs = doc.geometry().rings_of(doc.entities().slot[e]);
    if (rs.count == 0) return none;
    const auto xs = doc.geometry().ring_xs(rs.first);
    const auto ys = doc.geometry().ring_ys(rs.first);
    std::vector<core::Point2> ring;
    ring.reserve(xs.size());
    for (std::size_t v = 0; v < xs.size(); ++v)
        ring.push_back(core::Point2{xs[v], ys[v]});
    // The snap reach is a few PIXELS, whatever the zoom, like every other snap.
    const core::Mm snap = core::mm_round(8.0 * view_.mm_per_pixel());
    return core::area_edit_ghost(ring, *request, cursorWorld(), std::max<core::Mm>(snap, 1));
}

core::Point2 MapCanvas::cursorWorld() const
{
    return snap_preview_valid_ ? snap_preview_.point
                               : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});
}

void MapCanvas::addGhost(std::size_t batch, const std::vector<command::GhostRun>& runs)
{
    for (const command::GhostRun& ghost : runs) {
        if (ghost.points.size() < 2) continue;
        std::vector<render::ScreenPointF> run;
        run.reserve(ghost.points.size());
        for (const core::Point2& p : ghost.points)
            run.push_back(render::to_f(view_.to_screen(p)));
        addRun(batch, run, ghost.closed);
    }
}

void MapCanvas::addWorldRun(std::size_t batch, std::span<const core::Mm> xs,
                            std::span<const core::Mm> ys, bool closed, const core::Xform& map)
{
    if (xs.size() < 2) return;
    std::vector<render::ScreenPointF> run;
    run.reserve(xs.size());
    // THROUGH THE TRANSFORM, not along an offset. A ghost used to slide: the
    // helper took a `dx, dy`, so the only thing it could preview was a move, and
    // a turn or a scale previewed as a slide promises the wrong result to the
    // hand that is aiming with it. `core::transformed` is the function the verb
    // itself applies (core/transform.hpp).
    for (std::size_t v = 0; v < xs.size(); ++v)
        run.push_back(
            render::to_f(view_.to_screen(core::transformed(map, core::Point2{xs[v], ys[v]}))));
    addRun(batch, run, closed);
}

void MapCanvas::addEmitRuns(std::size_t batch, const core::EmitBuffer& buf, const core::Xform& map)
{
    for (std::size_t r = 0; r < buf.run_total(); ++r)
        if (buf.run_edge(r))
            addWorldRun(batch, buf.run_xs(r), buf.run_ys(r), buf.run_closed[r] != 0, map);
}

void MapCanvas::addGhost(std::size_t batch, const core::Xform& map,
                         std::span<const std::int64_t> keys)
{
    const core::Document& doc      = controller_.document();
    const core::EntityTable& table = doc.entities();
    const core::RingGeometry& geom = doc.geometry();
    core::EmitBuffer buf;

    // THE OBJECTS THE VERB NAMED, when it named them: `TAŞI nesneler=5` moves
    // object 5 whatever is highlighted, and the ghost has to be of what moves.
    std::vector<core::EntityId> named;
    for (const std::int64_t key : keys)
        if (const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            e != core::kNoEntity && doc.alive(e))
            named.push_back(e);
    const std::vector<core::EntityId>& carried = keys.empty() ? controller_.selectedSlots() : named;

    for (const core::EntityId e : carried) {
        if (e >= table.size() || !table.visible(e)) continue;
        // A caption travels as the box around its letters (`text_quad`), a curve
        // as its drawn form, a polyline as its rings. The DRAWN form is what a
        // ghost shows, and mapping its vertices is right for all four
        // transforms: a turned or mirrored circle is the turned or mirrored
        // outline, and a scaled one is the outline scaled about the same centre.
        if (std::array<core::Point2, 4> quad; core::text_quad(doc, e, quad)) {
            std::vector<render::ScreenPointF> run;
            for (const core::Point2 corner : quad)
                run.push_back(render::to_f(view_.to_screen(core::transformed(map, corner))));
            addRun(batch, run, true);
            if (table.kind[e] == core::kPolylineKind) continue;
        }
        buf.clear();
        if (core::entity_outline(doc, e, buf)) {
            addEmitRuns(batch, buf, map);
            continue;
        }
        const core::RingSpan span = geom.rings_of(table.slot[e]);
        for (std::uint32_t r = span.first; r < span.first + span.count; ++r)
            addWorldRun(batch, geom.ring_xs(r), geom.ring_ys(r),
                        geom.ring_role[r] != core::RingRole::Open, map);
    }
}

void MapCanvas::buildCrosshair()
{
    // Not while a form field is picking: the platform pointer is the pick mark
    // then, and two pointers for one hand is the thing this cross exists to end.
    if (!cursor_valid_ || look_.cursor == 2 || panning_ || capture_ || print_aspect_ > 0.0) return;

    const std::size_t batch      = nextBatch(palette_.crosshair.rgba(), 1.0f, false);
    const render::ScreenPointF c = toScreenF(cursor_);
    const float x = c.x, y = c.y;

    // THE CAD CROSSHAIR: two lines that stop short of the centre, and in the gap
    // the PICK BOX — the square that says what a click will take hold of. The
    // box is the selection tolerance the preference declares, so what it shows
    // is exactly what `SEÇ mod=NOKTA` reaches. It is drawn when a click would
    // SELECT: with no command running, or while one asks which objects. A
    // command asking for a POINT gets the bare cross, because a click then
    // lands a coordinate and nothing is taken hold of.
    const bool asks_point =
        controller_.awaitingInput() && controller_.promptKind() != command::ParamKind::Selection;
    const auto half = static_cast<float>(std::max(2.0, look_.pick_px));
    const float gap = asks_point ? 3.0f : half + 2.0f;

    // Full screen or a short cross, which is the choice every CAD offers and the
    // one people hold opinions about: the long lines line a point up against
    // something far away, the short one keeps the drawing legible.
    const bool full   = look_.cursor == 0;
    const auto arm    = static_cast<float>(look_.cursor_px);
    const float left  = full ? 0.0f : x - arm;
    const float right = full ? static_cast<float>(width()) : x + arm;
    const float top   = full ? 0.0f : y - arm;
    const float down  = full ? static_cast<float>(height()) : y + arm;

    addRun(batch, {{left, y}, {x - gap, y}}, false);
    addRun(batch, {{x + gap, y}, {right, y}}, false);
    addRun(batch, {{x, top}, {x, y - gap}}, false);
    addRun(batch, {{x, y + gap}, {x, down}}, false);

    if (!asks_point)
        addRun(batch,
               {{x - half, y - half},
                {x + half, y - half},
                {x + half, y + half},
                {x - half, y + half}},
               true);
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

void MapCanvas::addCurve(std::size_t batch, const core::CurvePath& path)
{
    curve_scratch_x_.clear();
    curve_scratch_y_.clear();
    core::path_outline(path, curve_scratch_x_, curve_scratch_y_);
    if (curve_scratch_x_.size() >= 2) addWorldRun(batch, curve_scratch_x_, curve_scratch_y_, false);
}

std::size_t MapCanvas::addCutMarks(std::span<const core::PathCrossing> cuts)
{
    // A CROSS WHERE AN EDGE CROSSES, A RING WHERE ONE ONLY TOUCHES: a tangent
    // cuts too, and a user who did not expect it should see that it does.
    // Screen-sized, so a cut reads the same at every zoom.
    constexpr float kArm    = 4.0F;
    const std::size_t marks = nextBatch(tokens_->accent.rgba(), 1.5f, false);
    std::size_t touching    = 0;
    for (const core::PathCrossing& c : cuts) {
        const render::ScreenPointF p = render::to_f(view_.to_screen(c.point));
        if (c.touching) {
            addCircle(marks, p.x, p.y, kArm);
            ++touching;
            continue;
        }
        addRun(marks, {{p.x - kArm, p.y - kArm}, {p.x + kArm, p.y + kArm}}, false);
        addRun(marks, {{p.x - kArm, p.y + kArm}, {p.x + kArm, p.y - kArm}}, false);
    }
    return touching;
}

void MapCanvas::addImpliedEdges(const core::Document& doc, core::EntityId target,
                                const core::TrimGuide& guide,
                                std::span<const core::PathCrossing> cuts)
{
    // WHERE AN EDGE WAS CARRIED ON TO CUT: from the real end of the edge to the
    // cut it reaches, dotted — the boundary the user did not draw but is using.
    // A straight end runs on along its line; an arc's end round its circle.
    core::TrimGuide plain                   = guide;
    plain.carry                             = false;
    const std::vector<core::CurvePath> real = core::cutting_edges(doc, target, plain);
    const std::size_t implied               = nextBatch(tokens_->accent.rgba(), 1.0f, true);
    for (const core::PathCrossing& c : cuts) {
        const core::CurvePath* nearest = nullptr;
        double best                    = -1.0;
        for (const core::CurvePath& edge : real) {
            if (edge.pieces.empty()) continue;
            const core::Point2 on = core::point_at(edge, core::place_of(edge, c.point));
            const double d        = core::distance_squared(on, c.point);
            if (best < 0.0 || d < best) {
                best    = d;
                nearest = &edge;
            }
        }
        if (nearest == nullptr || best <= 1.0) continue; ///< on the edge itself
        const core::PathPiece& head = nearest->pieces.front();
        const core::PathPiece& tail = nearest->pieces.back();
        const bool at_start =
            core::distance_squared(head.from, c.point) <= core::distance_squared(tail.to, c.point);
        const core::PathPiece& end = at_start ? head : tail;
        core::CurvePath run_on;
        core::PathPiece piece;
        if (end.kind == core::PathPiece::Kind::Arc) {
            // Round its circle the way the edge is walked (`core::arc_piece`).
            piece = core::arc_piece(end.centre, end.radius, at_start ? c.point : end.to,
                                    at_start ? end.from : c.point, end.sweep_udeg >= 0);
        } else {
            piece.from = at_start ? end.from : end.to;
            piece.to   = c.point;
        }
        run_on.pieces.push_back(piece);
        addCurve(implied, run_on);
    }
}

void MapCanvas::addReadout(float x, float y, const std::string& text)
{
    overlay_.labels.push_back(render::OverlayLabel{tokens_->readout.rgba(), x, y,
                                                   static_cast<float>(look_.hint_px), false, text});
}

void MapCanvas::addDimensionGhost(std::size_t batch, core::DimensionDef def,
                                  std::span<const core::Point2> picks, core::Point2 where,
                                  core::Mm text_height, bool fixed_rotation)
{
    core::DimensionLayout layout;
    if (!core::dimension_layout(def, picks, where, text_height, layout, fixed_rotation)) return;
    const std::string figure = core::dimension_text(def, controller_.bus().drawing_unit());
    core::dimension_fit(def, layout, figure, text_height);
    const auto base = core::dimension_baseline(layout.text_centre, layout.text_dir_x,
                                               layout.text_dir_y, text_height, figure);
    const std::vector<core::Point2> baseline{base[0], base[1]};
    const core::RingGeometry::RingInput rings[2]{{baseline, core::RingRole::Open, 0},
                                                 {layout.defs, core::RingRole::Open, 0}};
    core::RingGeometry scratch;
    const std::vector<std::uint8_t> payload = core::encode_dimension(def);
    if (auto slot = scratch.append(rings, payload)) {
        core::EmitBuffer buf;
        core::dimension_outline(scratch, slot.value(), buf);
        addEmitRuns(batch, buf);
    }
    // AND THE FIGURE IT WILL WRITE, WHERE IT WILL WRITE IT (TODOS C-17). The
    // rubber band's own readout measured from the first point to the cursor —
    // the distance to the dimension line, which is not what a dimension says —
    // and a user reading it took the line's offset for the measurement.
    addCentredReadout(layout.text_centre, figure);
    guide_label_ = figure;
}

void MapCanvas::addCentredReadout(core::Point2 at, const std::string& text)
{
    // Measured in the face the label is drawn in, at its size, so the middle
    // of the figure is the middle of the caption it stands for.
    QFont face = font();
    face.setPixelSize(std::max(1, look_.hint_px));
    const QFontMetricsF metrics(face);
    const render::ScreenPointF on = render::to_f(view_.to_screen(at));
    const auto half_width =
        static_cast<float>(metrics.horizontalAdvance(QString::fromStdString(text)) / 2.0);
    const auto half_height = static_cast<float>((metrics.ascent() - metrics.descent()) / 2.0);
    addReadout(on.x - half_width, on.y + half_height, text);
}

double MapCanvas::addAngleSweep(std::size_t batch, core::Point2 vertex, core::Point2 arm_a,
                                core::Point2 arm_b)
{
    // THE SWEEP, drawn at a radius that is READABLE rather than at the arms' own
    // length: an angle between a 2 cm arm and a 40 m one has to be legible at
    // both ends, so the mark sits a fixed number of pixels from the vertex like
    // every other mark this canvas draws.
    const core::Mm reach       = core::mm_round(28.0 * view_.mm_per_pixel());
    const core::AngleRule rule = look_.angle.rule;
    const double to_a          = core::direction_turns(vertex, arm_a, rule);
    const double to_b          = core::direction_turns(vertex, arm_b, rule);
    double between             = to_b - to_a;
    between -= std::floor(between);

    if (reach > 0 && between > 0.0) {
        // THE SWEEP THE COMMAND REPORTS, not the shorter one. Drawing the short
        // way round while writing the other number beside it was the very thing
        // this pass has been removing: the picture said one angle and the reading
        // said another. The sweep runs from the FIRST arm to the second in the
        // rule's own direction, which is how the command defines it — and why
        // the order the two arms are picked in is a choice rather than noise.
        const core::Point2 at_a =
            vertex + core::polar_offset_turns(core::mm_to_metres(reach), to_a, rule);
        const core::Point2 at_b =
            vertex + core::polar_offset_turns(core::mm_to_metres(reach), to_b, rule);
        curve_scratch_x_.clear();
        curve_scratch_y_.clear();
        // `arc_outline` sweeps counter-clockwise from start to end. Under semt an
        // increasing angle turns CLOCKWISE, so going from a to b that way is going
        // counter-clockwise from b to a.
        const bool ccw_from_a = rule != core::AngleRule::Semt;
        core::arc_outline(vertex, reach, ccw_from_a ? at_a : at_b, ccw_from_a ? at_b : at_a,
                          curve_scratch_x_, curve_scratch_y_);
        addWorldRun(batch, curve_scratch_x_, curve_scratch_y_, false);
    }
    return between;
}

void MapCanvas::noteDocumentChange()
{
    if (const std::uint64_t now = controller_.document().revision(); now != seen_revision_) {
        seen_revision_ = now;
        ++edits_;
    }
}

void MapCanvas::addMeasureMark(const command::MeasureMark& mark)
{
    noteDocumentChange();
    marks_.push_back(StoredMark{mark, edits_});
    update();
}

void MapCanvas::clearMeasureMarks()
{
    if (marks_.empty()) return;
    marks_.clear();
    update();
}

void MapCanvas::buildMeasureMarks()
{
    // A MARK OLDER THAN THE DRAWING DESCRIBES A DRAWING THAT IS GONE: a length
    // left beside a boundary that has since moved is a wrong number in the right
    // place, which is the worst kind.
    noteDocumentChange();
    std::erase_if(marks_, [this](const StoredMark& m) { return m.edits != edits_; });
    if (marks_.empty()) return;

    const auto screen = [this](core::Point2 p) { return render::to_f(view_.to_screen(p)); };
    QColor wash       = tokens_->accent;
    wash.setAlpha(34);

    for (const StoredMark& stored : marks_) {
        const command::MeasureMark& m = stored.mark;
        switch (m.shape) {
        case command::MeasureMark::Shape::Run: {
            const std::size_t line = nextBatch(tokens_->accent.rgba(), 1.6f, false);
            std::vector<render::ScreenPointF> drawn;
            drawn.reserve(m.points.size());
            for (const core::Point2& p : m.points)
                drawn.push_back(screen(p));
            addRun(line, drawn, false);
            for (const render::ScreenPointF& v : drawn)
                addCircle(line, v.x, v.y, 3.0f);
            for (std::size_t i = 1; i < drawn.size() && i - 1 < m.labels.size(); ++i)
                addReadout((drawn[i - 1].x + drawn[i].x) * 0.5F + 6.0F,
                           (drawn[i - 1].y + drawn[i].y) * 0.5F - 6.0F, m.labels[i - 1]);
            // The total, when there is one, beside the last point.
            if (!drawn.empty() && m.labels.size() == drawn.size())
                addReadout(drawn.back().x + 10.0F, drawn.back().y + 18.0F, m.labels.back());
            break;
        }
        case command::MeasureMark::Shape::Ring: {
            if (m.points.size() < 3) break;
            const std::size_t face = nextBatch(tokens_->accent.rgba(), 1.6f, false, wash.rgba());
            curve_scratch_x_.clear();
            curve_scratch_y_.clear();
            double cx = 0.0;
            double cy = 0.0;
            for (const core::Point2& p : m.points) {
                curve_scratch_x_.push_back(p.x);
                curve_scratch_y_.push_back(p.y);
                const render::ScreenPointF v = screen(p);
                cx += static_cast<double>(v.x);
                cy += static_cast<double>(v.y);
            }
            addWorldRun(face, curve_scratch_x_, curve_scratch_y_, true);
            if (!m.labels.empty()) {
                const auto n = static_cast<double>(m.points.size());
                addReadout(static_cast<float>(cx / n) - 40.0F, static_cast<float>(cy / n),
                           m.labels.front());
            }
            break;
        }
        case command::MeasureMark::Shape::Angle: {
            if (m.points.size() < 3) break;
            const std::size_t line = nextBatch(tokens_->accent.rgba(), 1.6f, false);
            addRun(line, {screen(m.points[0]), screen(m.points[1])}, false);
            addRun(line, {screen(m.points[0]), screen(m.points[2])}, false);
            (void)addAngleSweep(line, m.points[0], m.points[1], m.points[2]);
            if (!m.labels.empty()) {
                const render::ScreenPointF v = screen(m.points[0]);
                addReadout(v.x + 32.0F, v.y - 12.0F, m.labels.front());
            }
            break;
        }
        case command::MeasureMark::Shape::Point: {
            if (m.points.empty()) break;
            const std::size_t line       = nextBatch(tokens_->accent.rgba(), 1.6f, false);
            const render::ScreenPointF v = screen(m.points.front());
            addRun(line, {{v.x - 6.0F, v.y}, {v.x + 6.0F, v.y}}, false);
            addRun(line, {{v.x, v.y - 6.0F}, {v.x, v.y + 6.0F}}, false);
            addCircle(line, v.x, v.y, 3.0f);
            if (!m.labels.empty()) addReadout(v.x + 10.0F, v.y - 10.0F, m.labels.front());
            break;
        }
        case command::MeasureMark::Shape::Gap: {
            // AN OPEN END, IN THE WARNING INK: a ring round the end that meets
            // nothing and a dashed line across the gap to the nearest linework,
            // its width written on it. It is why the region did not close, shown
            // where it is rather than described.
            if (m.points.empty()) break;
            const std::size_t line       = nextBatch(tokens_->warn.rgba(), 1.8f, false);
            const render::ScreenPointF v = screen(m.points.front());
            addCircle(line, v.x, v.y, 7.0f);
            if (m.points.size() >= 2) {
                const std::size_t dash       = nextBatch(tokens_->warn.rgba(), 1.4f, true);
                const render::ScreenPointF w = screen(m.points[1]);
                addRun(dash, {v, w}, false);
                addCircle(line, w.x, w.y, 3.0f);
                if (!m.labels.empty())
                    addReadout((v.x + w.x) * 0.5F + 8.0F, (v.y + w.y) * 0.5F - 8.0F,
                               m.labels.front());
            } else if (!m.labels.empty()) {
                addReadout(v.x + 10.0F, v.y - 10.0F, m.labels.front());
            }
            break;
        }
        }
    }
}

void MapCanvas::buildBrokenLinks()
{
    // A DIMENSION THAT NO LONGER MEASURES ANYTHING SAYS SO WHERE IT STANDS
    // (TODOS C-10). The object it was tied to was erased; the figure beside it
    // is what that object used to measure, and nothing on the sheet would tell
    // the engineer who signs it. A struck ring in the warning ink at each point
    // that lost its object, and the words once per dimension. The canvas draws
    // it, a print does not: it is a question for the author, not the reader.
    const core::Document& doc       = controller_.document();
    const core::DimLinkTable& table = doc.dimension_links();
    const core::EntityTable& ents   = doc.entities();
    const core::RingGeometry& geom  = doc.geometry();
    const auto w                    = static_cast<float>(width());
    const auto h                    = static_cast<float>(height());

    // A FIGURE TYPED BY HAND IS SAID TO BE ONE (TODOS C-10), with the measured
    // figure beside it, because on the sheet the two are printed in the same
    // ink. On the canvas only: the print shows what the author wrote.
    if (doc.revision() != manual_revision_) {
        manual_revision_ = doc.revision();
        manual_dims_.clear();
        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if (ents.kind[e] != core::kDimensionKind || !doc.alive(e)) continue;
            auto def = core::dimension_of(geom, ents.slot[e]);
            if (def && core::dimension_text_is_manual(def.value())) manual_dims_.push_back(e);
        }
    }
    const core::DrawingUnit unit = controller_.bus().drawing_unit();
    for (const core::EntityId e : manual_dims_) {
        if (e >= ents.size() || !doc.alive(e) || !ents.visible(e)) continue;
        const std::uint32_t row   = ents.slot[e];
        const core::RingSpan span = geom.rings_of(row);
        if (span.count < 1 || geom.ring_count[span.first] < 1) continue;
        const render::ScreenPointF c = render::to_f(view_.to_screen(geom.vertex(span.first, 0)));
        if (c.x < -200.0F || c.y < -40.0F || c.x > w + 40.0F || c.y > h + 40.0F) continue;
        auto def = core::dimension_of(geom, row);
        if (!def) continue;
        // BESIDE THE CAPTION, whichever way it reads: to the right of the box
        // its letters fill, level with their middle — below a vertical caption
        // would be across its own dimension line.
        float at_x = c.x + 10.0F;
        float at_y = c.y + 4.0F;
        if (std::array<core::Point2, 4> quad; core::text_quad(doc, e, quad)) {
            float right = -1e9F;
            float top   = 1e9F;
            float foot  = -1e9F;
            for (const core::Point2 corner : quad) {
                const render::ScreenPointF q = render::to_f(view_.to_screen(corner));
                right                        = std::max(right, q.x);
                top                          = std::min(top, q.y);
                foot                         = std::max(foot, q.y);
            }
            at_x = right + 10.0F;
            at_y = (top + foot) * 0.5F + 4.0F;
        }
        overlay_.labels.push_back(render::OverlayLabel{
            tokens_->warn.rgba(), at_x, at_y, static_cast<float>(look_.hint_px), false,
            tr("elle yazılmış · ölçülen %1")
                .arg(QString::fromStdString(core::dimension_value_text(def.value(), unit)))
                .toStdString()});
    }

    // A CAPTION WITH A LETTER THE TYPEFACE DOES NOT HAVE (TODOS C-12). The
    // letter prints as the face's own empty box — on the screen and on the
    // sheet alike, never borrowed from another font — and a box does not say
    // what it stands for. Said beside the caption, on the canvas only. Plain
    // ASCII is always in the face, so only a caption with other letters is
    // asked, and each distinct caption once.
    if (doc.revision() != glyph_revision_) {
        glyph_revision_ = doc.revision();
        glyph_texts_.clear();
        if (glyph_notes_.size() > 4096) glyph_notes_.clear();
        for (core::EntityId e = 0; e < ents.size(); ++e) {
            if (!doc.alive(e)) continue;
            const std::uint32_t row = ents.slot[e];
            if (!doc.texts().has(row)) continue;
            const std::string_view text = doc.texts().text(row);
            if (std::ranges::all_of(
                    text, [](char c) { return (static_cast<unsigned char>(c) & 0x80u) == 0; }))
                continue;
            auto [known, fresh] = glyph_notes_.try_emplace(std::string(text));
            if (fresh) {
                QStringList named;
                const std::vector<command::MissingGlyph> missing = missing_glyphs(text);
                for (std::size_t i = 0; i < missing.size() && i < 3; ++i)
                    named << QStringLiteral("%1 (U+%2)")
                                 .arg(QString::fromStdString(missing[i].utf8))
                                 .arg(missing[i].code, 4, 16, QLatin1Char('0'))
                                 .toUpper();
                if (missing.size() > 3) named << QStringLiteral("…");
                if (!named.isEmpty())
                    known->second = tr("yazı tipinde yok: %1")
                                        .arg(named.join(QStringLiteral(", ")))
                                        .toStdString();
            }
            if (!known->second.empty()) glyph_texts_.push_back(e);
        }
    }
    for (const core::EntityId e : glyph_texts_) {
        if (e >= ents.size() || !doc.alive(e) || !ents.visible(e)) continue;
        std::array<core::Point2, 4> quad;
        if (!core::text_quad(doc, e, quad)) continue;
        float right = -1e9F;
        float top   = 1e9F;
        float foot  = -1e9F;
        for (const core::Point2 corner : quad) {
            const render::ScreenPointF q = render::to_f(view_.to_screen(corner));
            right                        = std::max(right, q.x);
            top                          = std::min(top, q.y);
            foot                         = std::max(foot, q.y);
        }
        if (right < -40.0F || foot < -40.0F || top > h + 40.0F || right > w + 400.0F) continue;
        const auto found = glyph_notes_.find(std::string(doc.texts().text(ents.slot[e])));
        if (found == glyph_notes_.end()) continue;
        // Under the "typed by hand" note when a dimension carries both.
        const float below = std::ranges::find(manual_dims_, e) != manual_dims_.end()
                                ? static_cast<float>(look_.hint_px) + 4.0F
                                : 0.0F;
        overlay_.labels.push_back(render::OverlayLabel{
            tokens_->warn.rgba(), right + 10.0F, (top + foot) * 0.5F + 4.0F + below,
            static_cast<float>(look_.hint_px), false, found->second});
    }

    // A HATCH WHOSE BOUNDARY IS GONE (TODOS C-11): it fills the shape the
    // boundary had, and says so at its middle.
    for (const core::EntityId hatch : doc.hatch_links().linked()) {
        if (!doc.alive(hatch) || !ents.visible(hatch)) continue;
        bool broken = false;
        for (const core::HatchSource& s : *doc.hatch_links().get(hatch))
            broken = broken || s.broken;
        if (!broken) continue;
        const core::Box2 box = ents.box_of(hatch);
        if (box.empty()) continue;
        const render::ScreenPointF v = render::to_f(view_.to_screen(
            core::Point2{(box.min_x + box.max_x) / 2, (box.min_y + box.max_y) / 2}));
        if (v.x < -8.0F || v.y < -8.0F || v.x > w + 8.0F || v.y > h + 8.0F) continue;
        const std::size_t mark = nextBatch(tokens_->warn.rgba(), 1.8f, false);
        addCircle(mark, v.x, v.y, 7.0f);
        addRun(mark, {{v.x - 5.0F, v.y + 5.0F}, {v.x + 5.0F, v.y - 5.0F}}, false);
        overlay_.labels.push_back(render::OverlayLabel{
            tokens_->warn.rgba(), v.x + 12.0F, v.y + 4.0F, static_cast<float>(look_.hint_px), false,
            tr("sınır bağı koptu").toStdString()});
    }

    if (table.empty()) return;
    std::size_t line = 0;
    bool started     = false;
    for (const core::EntityId dim : table.linked()) {
        if (!doc.alive(dim) || !ents.visible(dim)) continue;
        const core::RingSpan span = geom.rings_of(ents.slot[dim]);
        if (span.count < 2) continue;
        const std::uint32_t defs = span.first + 1;
        bool labelled            = false;
        for (const core::DimLink& l : *table.get(dim)) {
            if (!l.broken || l.point >= geom.ring_count[defs]) continue;
            const render::ScreenPointF v =
                render::to_f(view_.to_screen(geom.vertex(defs, l.point)));
            if (v.x < -8.0F || v.y < -8.0F || v.x > w + 8.0F || v.y > h + 8.0F) continue;
            if (!started) {
                line    = nextBatch(tokens_->warn.rgba(), 1.8f, false);
                started = true;
            }
            addCircle(line, v.x, v.y, 6.0f);
            addRun(line, {{v.x - 4.2F, v.y + 4.2F}, {v.x + 4.2F, v.y - 4.2F}}, false);
            if (!labelled) {
                // In the warning ink too, and below the point: a dimension's
                // own figure sits above or beside its line far more often.
                overlay_.labels.push_back(render::OverlayLabel{
                    tokens_->warn.rgba(), v.x + 10.0F, v.y + 20.0F,
                    static_cast<float>(look_.hint_px), false, tr("bağ koptu").toStdString()});
                labelled = true;
            }
        }
    }
}

void MapCanvas::buildRegionPreview(std::span<const std::uint8_t> payload, core::Point2 at)
{
    auto decoded = core::decode_region_preview(payload);
    if (!decoded) return;
    const core::Document& doc = controller_.document();

    // THE SAME QUERY THE CLICK MAKES, bounded by what is on screen: a region
    // larger than the view is still the command's to find, the preview simply
    // does not guess at it.
    const auto inside_ring = [](const std::vector<core::Point2>& ring, core::Point2 p) {
        bool in = false;
        for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
            const core::Point2 a = ring[i];
            const core::Point2 b = ring[j];
            if ((a.y > p.y) != (b.y > p.y)) {
                const double x = static_cast<double>(a.x) + static_cast<double>(p.y - a.y) *
                                                                static_cast<double>(b.x - a.x) /
                                                                static_cast<double>(b.y - a.y);
                if (static_cast<double>(p.x) < x) in = !in;
            }
        }
        return in;
    };
    RegionCache& cache = region_cache_;
    bool keep          = cache.valid && cache.revision == doc.revision() &&
                std::ranges::equal(cache.payload, payload);
    if (keep) {
        if (cache.found) {
            keep = !cache.rings.empty() && inside_ring(cache.rings.front(), at);
            for (std::size_t h = 1; keep && h < cache.rings.size(); ++h)
                keep = !inside_ring(cache.rings[h], at);
        } else {
            // Nothing closed here a moment ago: ask again once the cursor has
            // travelled a few pixels, not on every one.
            const double moved =
                static_cast<double>(core::segment_length(cache.asked, at)) / view_.mm_per_pixel();
            keep = moved < 12.0;
        }
    }
    if (!keep) {
        core::RegionQuery query;
        query.at             = at;
        query.islands        = decoded.value().islands;
        query.node_tolerance = decoded.value().node_tolerance;
        query.bridge         = decoded.value().bridge;
        for (const std::int64_t key : decoded.value().keys) {
            const core::EntityId e =
                doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
            if (e != core::kNoEntity && doc.alive(e)) query.only.push_back(e);
        }
        const core::Box2 seen = view_.visible_box();
        query.max_reach       = std::max(seen.width(), seen.height());
        cache                 = RegionCache{};
        cache.revision        = doc.revision();
        cache.payload.assign(payload.begin(), payload.end());
        cache.asked                = at;
        cache.valid                = true;
        auto found                 = core::region_at(doc, query);
        const core::Region* region = found ? &found.value() : nullptr;
        if (region != nullptr && region->face.has_value()) {
            const core::NetworkFace& face = *region->face;
            cache.found                   = true;
            const auto drawn              = [](const core::CurvePath& path) {
                std::vector<core::Mm> xs;
                std::vector<core::Mm> ys;
                core::path_outline(path, xs, ys);
                std::vector<core::Point2> ring;
                ring.reserve(xs.size());
                for (std::size_t i = 0; i < xs.size(); ++i)
                    ring.push_back(core::Point2{xs[i], ys[i]});
                return ring;
            };
            cache.rings.push_back(drawn(face.outer.path));
            for (const core::FaceRing& hole : face.holes)
                cache.rings.push_back(drawn(hole.path));
            const auto cm2   = static_cast<std::uint64_t>((face.area + 5000) / 10000);
            std::string frac = std::to_string(cm2 % 100);
            if (frac.size() < 2) frac = "0" + frac;
            cache.label = std::to_string(cm2 / 100) + "," + frac + " m²";
            if (!face.holes.empty())
                cache.label += " · " + std::to_string(face.holes.size()) + " ada";
        } else if (region != nullptr) {
            cache.open = region->open;
        }
    }

    if (cache.found && !cache.rings.empty()) {
        // THE FACE, ITS HOLES PUNCHED OUT. One keyhole run fills it — outer ring,
        // across to each hole and round it the other way — so a pool reads as a
        // hole in the parcel and not as part of it; the rings are stroked apart.
        QColor wash = tokens_->accent;
        wash.setAlpha(40);
        std::vector<render::ScreenPointF> keyhole;
        for (const core::Point2& p : cache.rings.front())
            keyhole.push_back(render::to_f(view_.to_screen(p)));
        const render::ScreenPointF home =
            keyhole.empty() ? render::ScreenPointF{} : keyhole.front();
        for (std::size_t h = 1; h < cache.rings.size(); ++h) {
            keyhole.push_back(home);
            for (const core::Point2& p : cache.rings[h])
                keyhole.push_back(render::to_f(view_.to_screen(p)));
            if (!cache.rings[h].empty())
                keyhole.push_back(render::to_f(view_.to_screen(cache.rings[h].front())));
            keyhole.push_back(home);
        }
        // The fill's own stroke is fully transparent: the keyhole's bridges are
        // how the holes are reached, not lines anybody drew.
        QColor unseen = wash;
        unseen.setAlpha(0);
        addRun(nextBatch(unseen.rgba(), 0.5f, false, wash.rgba()), keyhole, true);
        const std::size_t edge = nextBatch(tokens_->accent.rgba(), 2.2f, false);
        for (const std::vector<core::Point2>& ring : cache.rings) {
            std::vector<render::ScreenPointF> run;
            run.reserve(ring.size());
            for (const core::Point2& p : ring)
                run.push_back(render::to_f(view_.to_screen(p)));
            addRun(edge, run, true);
        }
        const render::ScreenPointF c = render::to_f(view_.to_screen(at));
        addReadout(c.x + 14.0F, c.y + 22.0F, cache.label);
        guide_label_ = cache.label;
        return;
    }
    // NOTHING CLOSES AROUND THE CURSOR: the open ends that keep it from closing,
    // nearest first, in the warning ink the refusal will mark them in.
    const std::size_t warn = nextBatch(tokens_->warn.rgba(), 1.6f, false);
    const std::size_t dash = nextBatch(tokens_->warn.rgba(), 1.2f, true);
    std::size_t shown      = 0;
    std::vector<core::Point2> used; // an end in one gap only, as SINIR marks them
    for (const core::OpenEnd& end : cache.open) {
        if (shown == 3) break;
        if (!end.has_nearest || std::ranges::find(used, end.at) != used.end() ||
            std::ranges::find(used, end.nearest) != used.end())
            continue;
        used.push_back(end.at);
        used.push_back(end.nearest);
        ++shown;
        const render::ScreenPointF v = render::to_f(view_.to_screen(end.at));
        const render::ScreenPointF w = render::to_f(view_.to_screen(end.nearest));
        addCircle(warn, v.x, v.y, 6.0f);
        addCircle(warn, w.x, w.y, 3.0f);
        addRun(dash, {v, w}, false);
        addReadout((v.x + w.x) * 0.5F + 8.0F, (v.y + w.y) * 0.5F - 8.0F,
                   trimmed(static_cast<double>(end.distance) / 1000.0, 3) + " m");
    }
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
    buildMeasureMarks();
    buildBrokenLinks();

    guide_vertices_ = 0;
    guide_label_.clear();

    // Rubber band for the running interactive command. It runs to the SNAPPED
    // point when an aid has fired, because that is where the segment will land.
    if (auto* session = controller_.session();
        session && session->waiting() && session->prompt().has_rubber_band &&
        (cursor_valid_ || session->prompt().rubber_shape == command::RubberShape::Fixed)) {
        const auto from = view_.to_screen(session->prompt().rubber_origin);

        QPointF to = cursor_;
        if (snap_preview_valid_) {
            const auto snapped = view_.to_screen(snap_preview_.point);
            to                 = QPointF(snapped.x, snapped.y);
        }

        const std::size_t batch          = nextBatch(palette_.rubberBand.rgba(), 1.0f, true);
        const command::RubberShape shape = session->prompt().rubber_shape;
        // The bus's, not the look's copy: ÇOKGEN measures its rotation under it,
        // and a ghost must be the object the command makes.
        const core::AngleConvention convention = controller_.bus().angle_convention();
        const std::size_t guide_before         = overlay_.batches[batch].xs.size();

        if (shape == command::RubberShape::Circle || shape == command::RubberShape::Arc) {
            // THE CURVE ITSELF — the circle through the cursor, or YAY's arc once
            // its first end is fixed — from `command::ghost_outline`, which draws
            // it with the kind's own outline and the radius the command will
            // compute. What the guide promises and what the command produces
            // cannot drift apart: it is one computation, and a test proves it.
            const core::Point2 centre = session->prompt().rubber_origin;
            const auto& chain         = session->prompt().rubber_chain;
            const bool arc            = shape == command::RubberShape::Arc && !chain.empty();
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));

            // WHAT IS ALREADY FIXED STAYS DRAWN. For HALKA the chain holds the
            // inner rim point, and the ring being made is the TWO circles: with
            // only the newest one previewed, the second click looked as though it
            // had erased the first. For a sector the chain holds the first edge,
            // and its radius line is what makes the shape read as a slice rather
            // than as a bare arc.
            if (!chain.empty()) {
                if (!arc) {
                    const core::Mm fixed_radius = core::radius_through(centre, chain.front());
                    if (fixed_radius > 0) {
                        curve_scratch_x_.clear();
                        curve_scratch_y_.clear();
                        core::circle_outline(centre, fixed_radius, curve_scratch_x_,
                                             curve_scratch_y_);
                        addWorldRun(batch, curve_scratch_x_, curve_scratch_y_, true);
                    }
                }
                addRun(batch,
                       {render::to_f(view_.to_screen(centre)),
                        render::to_f(view_.to_screen(chain.front()))},
                       false);
            }

            // The radius, so the user can read the size they are setting rather
            // than only see it.
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::CircleBuild) {
            // THE CIRCLE THE CLICK WILL MAKE, by the construction the command
            // named (`command::ghost_outline` over `core::circle_from_guide`).
            //
            // These three methods used to preview a LINE — or, for `ttr`, nothing
            // at all. `ttr` is the one where it matters most: four circles of the
            // radius are tangent to both lines, the user picks one by pointing at
            // a corner, and until now they found out which after the click.
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));

            // The points already fixed, as the run that made them: a diameter's
            // first end, the two rim points — and for `ttr` the two tangent lines
            // AS FAR AS THEY ARE FIXED, from the first one's second point to the
            // radius prompt, so neither leaves the screen while the other is
            // aimed and the fillet is typed against both.
            const auto guide   = core::decode_circle_guide(session->prompt().rubber_payload);
            const bool tangent = guide && guide->build == core::CircleBuild::Tangent;
            const auto& chain  = session->prompt().rubber_chain;
            if (tangent && chain.size() >= 2)
                addRun(batch,
                       {render::to_f(view_.to_screen(chain[0])),
                        render::to_f(view_.to_screen(chain[1]))},
                       false);
            if (tangent && chain.size() >= 4)
                addRun(batch,
                       {render::to_f(view_.to_screen(chain[2])),
                        render::to_f(view_.to_screen(chain[3]))},
                       false);
            else
                addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Fixed) {
            // WHAT THE RUN HAS ALREADY FIXED, and nothing else. No line to the
            // cursor, because the cursor is not answering this question: these
            // are the commands that fix a reference and then ask for NUMBERS,
            // and the reference is not a document object — so once it was given
            // it left the screen and the user was aiming at a baseline they
            // could no longer see. Drawn even when the pointer is off the canvas,
            // which is where a hand that is typing leaves it.
            const auto& chain = session->prompt().rubber_chain;
            if (chain.size() >= 2) {
                std::vector<render::ScreenPointF> run;
                run.reserve(chain.size());
                for (const core::Point2& p : chain)
                    run.push_back(render::to_f(view_.to_screen(p)));
                addRun(batch, run, false);
            }
            // Each fixed point marked, so a reference of ONE — a station with no
            // backsight yet — is visible too.
            for (const core::Point2& p : chain) {
                const render::ScreenPointF at = render::to_f(view_.to_screen(p));
                addCircle(batch, at.x, at.y, 4.0f);
            }
        } else if (shape == command::RubberShape::Candidates) {
            // THE ANSWERS THIS PICK CHOOSES BETWEEN. Two known distances cross at
            // TWO points and two known directions at one; which of the two was
            // meant is not in the numbers, so the user points at it. Both are
            // marked and the one that will be taken — the nearer — is ringed
            // again and joined to the cursor, so the choice is visible before the
            // click rather than after it.
            const auto& chain           = session->prompt().rubber_chain;
            const core::Point2 at       = cursorWorld();
            const core::Point2* nearest = nullptr;
            double best                 = 0.0;
            for (const core::Point2& p : chain) {
                const auto to_it = core::distance_squared(at, p);
                if (nearest == nullptr || to_it < best) {
                    nearest = &p;
                    best    = to_it;
                }
            }
            for (const core::Point2& p : chain) {
                const render::ScreenPointF on = render::to_f(view_.to_screen(p));
                addCircle(batch, on.x, on.y, 4.0f);
            }
            if (nearest != nullptr) {
                const render::ScreenPointF on = render::to_f(view_.to_screen(*nearest));
                const std::size_t lit         = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                addCircle(lit, on.x, on.y, 8.0f);
                addRun(lit, {on, toScreenF(to)}, false);
            }
        } else if (shape == command::RubberShape::Angle &&
                   !session->prompt().rubber_chain.empty()) {
            // THE TWO ARMS AND THE SWEEP BETWEEN THEM, with the reading on it.
            // Both arms used to be previewed as a plain line from the vertex, so
            // the first one left the screen while the second was aimed — and the
            // angle, the only thing the command measures, was not on the canvas
            // at all until the answer was already in the transcript.
            const core::Point2 vertex = session->prompt().rubber_origin;
            const core::Point2 arm_a  = session->prompt().rubber_chain.front();
            const core::Point2 arm_b  = cursorWorld();

            addRun(batch,
                   {render::to_f(view_.to_screen(vertex)), render::to_f(view_.to_screen(arm_a))},
                   false);
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
            const double between = addAngleSweep(batch, vertex, arm_a, arm_b);

            // AND THE READING, which is what the user is here for. The generic
            // dynamic-input label writes the distance and bearing to the cursor;
            // for this shape the number that matters is the angle between the
            // arms, in the session's own unit.
            if (look_.dynamic_input) {
                const render::ScreenPointF at = toScreenF(to);
                const std::string text        = core::angle_text(between, look_.angle.unit);
                overlay_.labels.push_back(
                    render::OverlayLabel{tokens_->readout.rgba(), at.x + 12.0F, at.y - 10.0F,
                                         static_cast<float>(look_.hint_px), false, text});
                guide_label_ = text;
            }
        } else if (shape == command::RubberShape::ArcBuild) {
            // THE ARC THE CLICK WILL MAKE, by the construction the command named
            // (`command::ghost_outline` over `core::arc_from_guide`).
            //
            // Three of YAY's methods previewed a straight LINE, which is the one
            // shape the answer is not; `bby` never even asked which side the
            // curve goes. A curve that is shown as a line is a curve the user
            // finds out about after the click.
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::ArcSweep &&
                   !session->prompt().rubber_chain.empty()) {
            // YAY bma, THE SWEEP SHOWN: the arc from the start round the centre
            // to the cursor's direction, the two radii that bound it, and the
            // sweep written at the cursor in the session's unit — which is the
            // number the click answers with (`Prompt::pick_sweep`).
            const core::Point2 centre = session->prompt().rubber_origin;
            const core::Point2 start  = session->prompt().rubber_chain.front();
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
            addRun(batch,
                   {render::to_f(view_.to_screen(centre)), render::to_f(view_.to_screen(start))},
                   false);
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
            if (look_.dynamic_input) {
                const double sweep =
                    core::arc_sweep_toward(centre, start, cursorWorld(), convention);
                if (sweep > 0.0) {
                    const double turns = sweep * core::udeg_per_angle_unit(convention.unit) /
                                         static_cast<double>(core::kUDegFullCircle);
                    const std::string text        = core::angle_text(turns, convention.unit);
                    const render::ScreenPointF at = toScreenF(to);
                    overlay_.labels.push_back(
                        render::OverlayLabel{tokens_->readout.rgba(), at.x + 12.0F, at.y - 10.0F,
                                             static_cast<float>(look_.hint_px), false, text});
                    guide_label_ = text;
                }
            }
        } else if (shape == command::RubberShape::Rectangle) {
            // THE FACE, not its diagonal: the four corners DİKDÖRTGEN will write,
            // in world coordinates (`command::ghost_outline`). A rectangle
            // previewed as one line tells the user nothing about what the next
            // click will make, and with the diagonal lock held it is the
            // difference between seeing a square and finding out you drew one.
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
        } else if (shape == command::RubberShape::Ellipse &&
                   !session->prompt().rubber_chain.empty()) {
            // THE ELLIPSE the third click will make — or the piece of it, when
            // the run was given its sweep: the first axis is fixed (the chain),
            // and the cursor's reach ACROSS it is the second, by the function
            // ELİPS reads the click with (`core::ellipse_minor_end`).
            const core::Point2 centre = session->prompt().rubber_origin;
            const core::Point2 major  = session->prompt().rubber_chain.front();
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
            addRun(batch,
                   {render::to_f(view_.to_screen(centre)), render::to_f(view_.to_screen(major))},
                   false);
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Curve) {
            // THE CURVE through the control points so far and the cursor, over
            // the degree SPLINE will use and at the kind's own density
            // (`command::ghost_outline`).
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
            // The control polygon, faint, so the hand sees what it is steering.
            std::vector<render::ScreenPointF> polygon;
            for (const core::Point2& p : session->prompt().rubber_chain)
                polygon.push_back(render::to_f(view_.to_screen(p)));
            polygon.push_back(toScreenF(to));
            addRun(batch, polygon, false);
        } else if (shape == command::RubberShape::Dimension &&
                   session->prompt().rubber_chain.size() >= 2) {
            // THE DIMENSION laid out at the cursor — extension lines, dimension
            // line, arrowheads and where its figure stands — by the layout and
            // the fit ÖLÇÜ will use on the click, drawn by the kind's own outline
            // over a scratch record.
            if (auto guide = core::decode_dimension_guide(session->prompt().rubber_payload)) {
                addDimensionGhost(batch, guide->def, session->prompt().rubber_chain, cursorWorld(),
                                  guide->text_height, false);
            }
        } else if (shape == command::RubberShape::DimensionNext &&
                   session->prompt().rubber_chain.size() >= 2) {
            // THE NEXT FIGURE OF A ROW (ZİNCİRÖLÇÜ, BAZÖLÇÜ): from the run's last
            // point, or its base, to the cursor, on the row's line and in its
            // direction — the layout the click makes, drawn by the kind's outline.
            if (auto guide = core::decode_dimension_guide(session->prompt().rubber_payload)) {
                const std::array<core::Point2, 2> picks{session->prompt().rubber_chain[0],
                                                        cursorWorld()};
                addDimensionGhost(batch, guide->def, picks, session->prompt().rubber_chain[1],
                                  guide->text_height, true);
            }
        } else if (shape == command::RubberShape::Block) {
            // THE BLOCK under the cursor, expanded by the code that will draw the
            // reference once it is placed, with the scale, turn and grid the
            // command was given.
            if (auto decoded = core::decode_block_reference(session->prompt().rubber_payload)) {
                core::EmitBuffer buf;
                if (core::expand_block_definition(controller_.document(), cursorWorld(),
                                                  decoded.value(), buf))
                    addEmitRuns(batch, buf);
            }
        } else if (shape == command::RubberShape::AreaEdit) {
            // THE FACE AT THE WANTED AREA. The edge or the corner in hand follows
            // the cursor; within a few pixels of the figure it SNAPS onto it, and
            // the figure's own point is what Enter sends (keyPressEvent). The
            // original stays drawn underneath: nothing changes until the command
            // commits.
            const core::AreaGhost ghost = areaGhost();
            if (!ghost.points.empty()) {
                const std::size_t lit =
                    ghost.snapped ? nextBatch(tokens_->accent.rgba(), 1.5f, false) : batch;
                std::vector<core::Mm> xs;
                std::vector<core::Mm> ys;
                xs.reserve(ghost.points.size());
                ys.reserve(ghost.points.size());
                for (const core::Point2& p : ghost.points) {
                    xs.push_back(p.x);
                    ys.push_back(p.y);
                }
                addWorldRun(lit, xs, ys, true);
                if (look_.dynamic_input) {
                    const render::ScreenPointF at = toScreenF(to);
                    std::string text              = core::format_square_metres(ghost.area);
                    if (ghost.snapped) text += "  ✓ hedef";
                    overlay_.labels.push_back(
                        render::OverlayLabel{tokens_->readout.rgba(), at.x + 12.0f, at.y - 10.0f,
                                             static_cast<float>(look_.hint_px), false, text});
                    guide_label_ = text;
                }
            }
        } else if (shape == command::RubberShape::Polygon) {
            // THE POLYGON THE CLICK WILL MAKE, from the very function that will
            // make it (`core::polygon_from_guide`, through
            // `command::ghost_outline`) and under the bus's own angle convention.
            // A guide computed a second way agrees with the command on the easy
            // cases and diverges exactly where the arithmetic is interesting —
            // this one turned towards the cursor while ÇOKGEN, handed `aci`,
            // drew the polygon at `aci`.
            addGhost(batch, command::ghost_outline(session->prompt(), cursorWorld(), convention));
            // The arm from the centre, so the size being set is readable as a
            // distance and not only as a shape.
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::EdgeRectangle &&
                   session->prompt().rubber_chain.size() >= 2) {
            // THE ROTATED RECTANGLE the third click will make. This branch is
            // why the shape exists: the command used to preview it as a `Ring`
            // with no chain, which is an origin and a cursor, and two points
            // enclose nothing — so the tool drew correctly and showed nothing.
            const auto ghost = command::ghost_outline(session->prompt(), cursorWorld(), convention);
            if (!ghost.empty()) {
                addGhost(batch, ghost);
            } else {
                // On the edge, where there is no rectangle yet: the edge itself,
                // so the hand still sees what it has fixed.
                const auto& chain = session->prompt().rubber_chain;
                addRun(batch,
                       {render::to_f(view_.to_screen(chain[0])),
                        render::to_f(view_.to_screen(chain[1]))},
                       false);
            }
        } else if (shape == command::RubberShape::Corner) {
            // THE CUT THE CLICK WILL MAKE, from the function PAH and YUVARLA make
            // it with (`core::cut_corner`), at the cursor's distance from the
            // corner — which is also what the click answers (`pick_distance`).
            // The corner used to be asked for and then a number typed blind: the
            // user found out on Enter whether 3 m was too much for the edge.
            if (auto decoded = core::decode_corner_preview(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::EntityId e    = doc.slot_of(
                    static_cast<core::EntityKey>(static_cast<std::uint64_t>(decoded.value().key)));
                if (e != core::kNoEntity && doc.alive(e)) {
                    const core::RingSpan span = doc.geometry().rings_of(doc.entities().slot[e]);
                    const auto xs             = doc.geometry().ring_xs(span.first);
                    const auto ys             = doc.geometry().ring_ys(span.first);
                    std::vector<core::Point2> run;
                    run.reserve(xs.size());
                    for (std::size_t v = 0; v < xs.size(); ++v)
                        run.push_back(core::Point2{xs[v], ys[v]});
                    const bool closed =
                        doc.geometry().ring_role[span.first] != core::RingRole::Open;
                    const core::Mm size =
                        core::segment_length(session->prompt().rubber_origin, cursorWorld());
                    // WHAT THE CLICK WILL ANSWER, named: the radius or the cut,
                    // not a length and a bearing — the bearing of the cursor from
                    // the corner answers nothing this prompt asks.
                    if (look_.dynamic_input && size > 0) {
                        const render::ScreenPointF c = toScreenF(to);
                        const std::string text = (decoded.value().fillet ? "yarıçap " : "pah ") +
                                                 trimmed(static_cast<double>(size) / 1000.0, 3) +
                                                 " m";
                        addReadout(c.x + 12.0F, c.y + 24.0F, text);
                        guide_label_ = text;
                    }
                    if (decoded.value().every) {
                        // EVERY CORNER AT ONCE, by `core::cut_every_corner`:
                        // each run as the chain edit will leave it — the
                        // first object and every other one the payload names.
                        const std::size_t lit = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                        const auto draw = [&](const std::vector<core::Point2>& pts, bool shut) {
                            const core::CornerRun all =
                                core::cut_every_corner(pts, shut, size, decoded.value().fillet);
                            curve_scratch_x_.clear();
                            curve_scratch_y_.clear();
                            if (all.bent) {
                                core::path_outline(all.path, curve_scratch_x_, curve_scratch_y_);
                            } else {
                                for (const core::Point2& p : all.ring) {
                                    curve_scratch_x_.push_back(p.x);
                                    curve_scratch_y_.push_back(p.y);
                                }
                            }
                            addWorldRun(lit, curve_scratch_x_, curve_scratch_y_, shut && !all.bent);
                        };
                        draw(run, closed);
                        for (const std::int64_t other : decoded.value().also) {
                            const core::EntityId o = doc.slot_of(
                                static_cast<core::EntityKey>(static_cast<std::uint64_t>(other)));
                            if (o == core::kNoEntity || !doc.alive(o)) continue;
                            const core::RingSpan more =
                                doc.geometry().rings_of(doc.entities().slot[o]);
                            if (more.count != 1) continue;
                            const auto ox = doc.geometry().ring_xs(more.first);
                            const auto oy = doc.geometry().ring_ys(more.first);
                            std::vector<core::Point2> pts;
                            pts.reserve(ox.size());
                            for (std::size_t v = 0; v < ox.size(); ++v)
                                pts.push_back(core::Point2{ox[v], oy[v]});
                            draw(pts, doc.geometry().ring_role[more.first] != core::RingRole::Open);
                        }
                    } else if (auto cut = core::cut_corner(run, closed, decoded.value().at, size,
                                                           decoded.value().fillet)) {
                        const std::size_t lit = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                        const auto add = [&](const std::vector<core::Point2>& pts, bool shut) {
                            curve_scratch_x_.clear();
                            curve_scratch_y_.clear();
                            for (const core::Point2& p : pts) {
                                curve_scratch_x_.push_back(p.x);
                                curve_scratch_y_.push_back(p.y);
                            }
                            addWorldRun(lit, curve_scratch_x_, curve_scratch_y_, shut);
                        };
                        add(cut.value().kept, closed);
                        if (cut.value().arc) {
                            add(cut.value().second, false);
                            curve_scratch_x_.clear();
                            curve_scratch_y_.clear();
                            core::arc_outline(cut.value().centre, cut.value().radius,
                                              cut.value().start, cut.value().end, curve_scratch_x_,
                                              curve_scratch_y_);
                            addWorldRun(lit, curve_scratch_x_, curve_scratch_y_, false);
                        }
                    }
                }
            }
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Stretch) {
            // THE OBJECTS AS THEY WILL BE: every one the window catches, its
            // windowed corners carried by the cursor's offset from the base —
            // `core::stretch_entity`, the call ESNET makes with the click, drawn
            // by each kind's own outline. The window stays drawn, because it is
            // what decides which corners follow.
            if (auto decoded = core::decode_stretch_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::Box2& window  = decoded.value().window;
                const core::Point2 base   = session->prompt().rubber_origin;
                const core::Point2 at     = cursorWorld();
                std::vector<core::EntityId> candidates;
                if (decoded.value().keys.empty()) {
                    core::pick_in_box(doc, window, core::PickMode::Crossing, candidates);
                } else {
                    for (const std::int64_t key : decoded.value().keys) {
                        const core::EntityId e = doc.slot_of(
                            static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                        if (e != core::kNoEntity && doc.alive(e)) candidates.push_back(e);
                    }
                }
                core::EmitBuffer buf;
                for (const core::EntityId e : candidates) {
                    auto stretched =
                        core::stretch_entity(doc, e, window, at.x - base.x, at.y - base.y);
                    if (!stretched) continue;
                    if (const std::optional<core::Stretched>& done = stretched.value(); done)
                        (void)core::edit_preview(doc, e, done->edit, buf);
                }
                addEmitRuns(nextBatch(tokens_->accent.rgba(), 1.5f, false), buf);

                std::vector<render::ScreenPointF> frame;
                for (const core::Point2 corner : {core::Point2{window.min_x, window.min_y},
                                                  core::Point2{window.max_x, window.min_y},
                                                  core::Point2{window.max_x, window.max_y},
                                                  core::Point2{window.min_x, window.max_y}})
                    frame.push_back(render::to_f(view_.to_screen(corner)));
                addRun(batch, frame, true);
            }
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Break) {
            // THE PIECE THAT WILL GO, drawn as going: between the first point and
            // the cursor, along the line, in the ink of a destructive action and
            // dashed — and what stays on either side of it in the accent. From
            // `core::break_run`, the cut KIR makes with the click.
            if (auto decoded = core::decode_break_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::EntityId e    = doc.slot_of(
                    static_cast<core::EntityKey>(static_cast<std::uint64_t>(decoded.value().key)));
                const auto path =
                    e != core::kNoEntity && doc.alive(e) ? core::path_of(doc, e) : std::nullopt;
                if (path) {
                    if (auto cut = core::break_path(*path, session->prompt().rubber_origin,
                                                    cursorWorld())) {
                        const auto add = [this](std::size_t into, const core::CurvePath& piece) {
                            curve_scratch_x_.clear();
                            curve_scratch_y_.clear();
                            core::path_outline(piece, curve_scratch_x_, curve_scratch_y_);
                            addWorldRun(into, curve_scratch_x_, curve_scratch_y_, false);
                        };
                        const std::size_t kept = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                        for (const core::CurvePath& piece : cut.value().kept)
                            add(kept, piece);
                        const std::size_t gone = nextBatch(tokens_->danger.rgba(), 2.5f, true);
                        add(gone, cut.value().gap);

                        // HOW MUCH GOES, measured ALONG the path: the straight
                        // distance from the first point to the cursor is not
                        // the length of a gap that turns a corner or bends.
                        if (look_.dynamic_input) {
                            const core::Mm along         = core::path_length(cut.value().gap);
                            const render::ScreenPointF c = toScreenF(to);
                            const std::string text =
                                "kırılan " + trimmed(static_cast<double>(along) / 1000.0, 3) + " m";
                            addReadout(c.x + 12.0F, c.y + 24.0F, text);
                            guide_label_ = text;
                        }
                    }
                }
            }
        } else if (shape == command::RubberShape::PairCorner) {
            // THE CORNER BETWEEN THE TWO, at the size the cursor's distance from
            // where they meet shows — by `core::fillet_pair` / `chamfer_pair`,
            // the call the click makes (C-06: the preview is the output). The
            // parts that stay and the arc or edge that joins them are drawn;
            // when the size does not fit, the reason is written instead.
            if (auto guide = core::decode_pair_corner_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const auto path_of_key =
                    [&doc](std::int64_t key) -> std::optional<core::CurvePath> {
                    const core::EntityId e =
                        doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                    if (e == core::kNoEntity || !doc.alive(e)) return std::nullopt;
                    return core::path_of(doc, e);
                };
                const auto pa = path_of_key(guide.value().key_a);
                const auto pb = path_of_key(guide.value().key_b);
                if (pa && pb) {
                    const core::Mm size =
                        core::segment_length(session->prompt().rubber_origin, cursorWorld());
                    const core::PairCornerGuide& g = guide.value();
                    auto made                      = g.fillet
                                                         ? core::fillet_pair(*pa, g.pick_a, *pb, g.pick_b, size)
                                                         : core::chamfer_pair(*pa, g.pick_a, *pb, g.pick_b, size, size);
                    const render::ScreenPointF c   = toScreenF(to);
                    std::string text;
                    if (made) {
                        const auto draw = [this](std::size_t into, const core::CurvePath& path) {
                            curve_scratch_x_.clear();
                            curve_scratch_y_.clear();
                            core::path_outline(path, curve_scratch_x_, curve_scratch_y_);
                            addWorldRun(into, curve_scratch_x_, curve_scratch_y_, false);
                        };
                        const std::size_t kept = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                        if (g.trim) {
                            draw(kept, made.value().a);
                            draw(kept, made.value().b);
                        }
                        if (made.value().has_link) {
                            core::CurvePath link;
                            link.pieces.push_back(made.value().link);
                            draw(nextBatch(tokens_->accent.rgba(), 2.5f, false), link);
                        }
                        text = (g.fillet ? "yarıçap " : "mesafe ") +
                               trimmed(static_cast<double>(size) / 1000.0, 3) + " m";
                    } else {
                        text = made.error().message;
                    }
                    if (look_.dynamic_input) {
                        addReadout(c.x + 12.0F, c.y + 24.0F, text);
                        guide_label_ = text;
                    }
                }
            }
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::Split) {
            // THE PIECES THE SPLIT WILL MAKE, each drawn in turn so the cuts read
            // as cuts: the object cut at the points given so far and at the
            // cursor, by the function BÖL cuts with (`command::split_at_points`)
            // — an arc's pieces drawn as arcs, because they will be arcs.
            if (auto decoded = core::decode_break_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::EntityId e    = doc.slot_of(
                    static_cast<core::EntityKey>(static_cast<std::uint64_t>(decoded.value().key)));
                const auto path =
                    e != core::kNoEntity && doc.alive(e) ? core::path_of(doc, e) : std::nullopt;
                if (path) {
                    std::vector<core::Point2> at = session->prompt().rubber_chain;
                    at.push_back(cursorWorld());
                    const std::vector<core::CurvePath> pieces = command::split_at_points(*path, at);
                    const std::size_t even = nextBatch(tokens_->accent.rgba(), 2.0f, false);
                    const std::size_t odd  = nextBatch(palette_.rubberBand.rgba(), 2.0f, true);
                    for (std::size_t i = 0; i < pieces.size(); ++i) {
                        curve_scratch_x_.clear();
                        curve_scratch_y_.clear();
                        core::path_outline(pieces[i], curve_scratch_x_, curve_scratch_y_);
                        addWorldRun(i % 2 == 0 ? even : odd, curve_scratch_x_, curve_scratch_y_,
                                    false);
                    }
                    // A mark at every cut, on the object and not where the hand was.
                    for (const core::Point2& p : at) {
                        const render::ScreenPointF on = render::to_f(
                            view_.to_screen(core::point_at(*path, core::place_of(*path, p))));
                        addCircle(even, on.x, on.y, 4.0f);
                    }
                    if (look_.dynamic_input) {
                        const core::Mm along         = core::path_length(core::sub_path(
                            *path, core::path_start(*path), core::place_of(*path, cursorWorld())));
                        const render::ScreenPointF c = toScreenF(to);
                        const std::string text =
                            std::to_string(pieces.size()) + " parça · baştan " +
                            trimmed(static_cast<double>(along) / 1000.0, 3) + " m";
                        addReadout(c.x + 12.0F, c.y + 24.0F, text);
                        guide_label_ = text;
                    }
                }
            }
        } else if (shape == command::RubberShape::Trim) {
            // WHAT THE CLICK WILL DO TO THE OBJECT UNDER IT: for BUDA the piece it
            // throws away, in the ink of a destructive action and dashed, and every
            // place the edges cut it — the candidates, a touch drawn as a ring; for
            // UZAT the reach it adds, dashed in the accent. The object is the one
            // the click would pick and the edges are the run's
            // (`core::cutting_edges`), and the edit is `core::trim_curve` /
            // `core::extend_curve` — the calls BUDA and UZAT make with the click.
            if (auto decoded = core::decode_trim_guide(session->prompt().rubber_payload)) {
                const core::TrimGuide& guide = decoded.value();
                const core::Document& doc    = controller_.document();
                const core::Point2 at        = cursorWorld();
                const core::EntityId e =
                    core::pick_nearest(doc, at, controller_.bus().aid_settings().pick_radius);
                const std::optional<core::CurvePath> path =
                    e == core::kNoEntity ? std::nullopt : core::path_of(doc, e);
                // A closed polyline is an area: BUDA refuses it, so nothing is shown.
                const bool area =
                    path && path->closed && doc.entities().kind[e] == core::kPolylineKind;
                if (path && !area) {
                    const std::vector<core::CurvePath> edges = core::cutting_edges(doc, e, guide);
                    std::string said;
                    if (guide.extend) {
                        // The object as it is, solid, and the reach alone dashed:
                        // drawn over the extended whole, the dashes vanish into it.
                        if (auto reach = core::extend_curve(*path, edges, at)) {
                            addCurve(nextBatch(tokens_->accent.rgba(), 1.5f, false), *path);
                            addCurve(nextBatch(tokens_->accent.rgba(), 2.5f, true),
                                     reach.value().added);
                            said = "uzantı " +
                                   trimmed(
                                       static_cast<double>(core::path_length(reach.value().added)) /
                                           1000.0,
                                       3) +
                                   " m";
                        }
                    } else if (auto cut = core::trim_curve(*path, edges, at, guide.keep)) {
                        const std::size_t kept = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                        for (const core::CurvePath& piece : cut.value().kept)
                            addCurve(kept, piece);
                        const std::size_t gone = nextBatch(tokens_->danger.rgba(), 2.5f, true);
                        core::Mm going         = 0;
                        for (const core::CurvePath& piece : cut.value().removed) {
                            addCurve(gone, piece);
                            going += core::path_length(piece);
                        }
                        if (guide.carry) addImpliedEdges(doc, e, guide, cut.value().cuts);
                        const std::size_t touching = addCutMarks(cut.value().cuts);
                        said = "atılacak " + trimmed(static_cast<double>(going) / 1000.0, 3) +
                               " m · " + std::to_string(cut.value().cuts.size()) + " kesişim";
                        if (touching != 0) said += " (" + std::to_string(touching) + " teğet)";
                    }
                    if (look_.dynamic_input && !said.empty()) {
                        const render::ScreenPointF c = toScreenF(to);
                        addReadout(c.x + 12.0F, c.y + 24.0F, said);
                        guide_label_ = said;
                    }
                }
            }
        } else if (shape == command::RubberShape::TrimFence) {
            // THE WHOLE FENCE'S EDIT before Enter applies it: the fence run on to
            // the cursor in the rubber band's own ink, every piece it would take
            // marked as going — or every reach it would add — on every object it
            // crosses. Drawn from `core::plan_fence`, the plan the command applies.
            if (auto decoded = core::decode_trim_guide(session->prompt().rubber_payload)) {
                const core::TrimGuide& guide    = decoded.value();
                const core::Document& doc       = controller_.document();
                std::vector<core::Point2> fence = session->prompt().rubber_chain;
                fence.push_back(cursorWorld());
                curve_scratch_x_.clear();
                curve_scratch_y_.clear();
                for (const core::Point2& p : fence) {
                    curve_scratch_x_.push_back(p.x);
                    curve_scratch_y_.push_back(p.y);
                }
                addWorldRun(batch, curve_scratch_x_, curve_scratch_y_, false);

                const core::FencePlan plan = core::plan_fence(doc, fence, guide);
                const std::size_t kept     = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                const std::size_t gone     = nextBatch(
                    (guide.extend ? tokens_->accent : tokens_->danger).rgba(), 2.5f, true);
                std::size_t pieces = 0;
                for (const core::FenceEdit& edit : plan.edits) {
                    if (guide.extend) {
                        if (auto now = core::path_of(doc, edit.target)) addCurve(kept, *now);
                        addCurve(gone, edit.reach.added);
                        pieces += edit.reach.added.pieces.size();
                        continue;
                    }
                    for (const core::CurvePath& piece : edit.cut.kept)
                        addCurve(kept, piece);
                    for (const core::CurvePath& piece : edit.cut.removed)
                        addCurve(gone, piece);
                    pieces += edit.cut.removed.size();
                }
                if (look_.dynamic_input) {
                    std::string said = std::to_string(pieces) +
                                       (guide.extend ? " uç uzatılacak" : " parça budanacak");
                    if (plan.passed_over != 0)
                        said += " · " + std::to_string(plan.passed_over) + " nesne atlanır";
                    const render::ScreenPointF c = toScreenF(to);
                    addReadout(c.x + 12.0F, c.y + 24.0F, said);
                    guide_label_ = said;
                }
            }
        } else if (shape == command::RubberShape::Grip) {
            // THE OBJECT AS IT WILL BE with its corner — or a new one — at the
            // cursor: the two edges that meet there follow it. Drawn from the
            // edit the command makes (`core::grip_preview`,
            // `core::insert_preview`), so a corner that would make the ring
            // cross itself shows nothing rather than something it cannot do.
            if (auto decoded = core::decode_grip_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const auto slot_of        = [&doc](std::int64_t key) {
                    return doc.slot_of(
                        static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                };
                const core::EntityId e = slot_of(decoded.value().key);
                if (e != core::kNoEntity && doc.alive(e)) {
                    const std::size_t lit = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                    if (decoded.value().insert) {
                        core::EmitBuffer buf;
                        if (core::insert_preview(doc, e, decoded.value().index, cursorWorld(), buf))
                            addEmitRuns(lit, buf);
                    } else {
                        // EVERY GRIP THE GUIDE NAMES, grouped by object and moved
                        // in turn (`core::move_grips`): a shared corner shows in
                        // every object that shares it.
                        std::vector<std::pair<core::EntityId, std::vector<core::GripMove>>> each;
                        const auto add = [&](core::EntityId slot, std::size_t index) {
                            if (slot == core::kNoEntity || !doc.alive(slot)) return;
                            for (auto& [s2, moves] : each)
                                if (s2 == slot) {
                                    moves.push_back(core::GripMove{index, cursorWorld()});
                                    return;
                                }
                            each.push_back({slot, {core::GripMove{index, cursorWorld()}}});
                        };
                        add(e, decoded.value().index);
                        for (const core::GripGuide::More& m : decoded.value().also)
                            add(slot_of(m.key), m.index);
                        for (const auto& [slot, moves] : each) {
                            auto edit = core::move_grips(doc, slot, moves);
                            core::EmitBuffer buf;
                            if (edit && core::edit_preview(doc, slot, edit.value(), buf))
                                addEmitRuns(lit, buf);
                        }
                    }
                }
            }
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (shape == command::RubberShape::EdgeArc) {
            // THE EDGE BENT THROUGH THE CURSOR, the rest of the object as it is:
            // `core::path_with_arc_edge`, the call KENARTÜRÜ makes with the click
            // — and the radius it would have, beside the cursor (TODOS C-07).
            if (auto decoded = core::decode_edge_guide(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::EntityId e    = doc.slot_of(
                    static_cast<core::EntityKey>(static_cast<std::uint64_t>(decoded.value().key)));
                const auto path =
                    e != core::kNoEntity && doc.alive(e) ? core::path_of(doc, e) : std::nullopt;
                if (path) {
                    const auto bent =
                        core::path_with_arc_edge(*path, decoded.value().edge, cursorWorld());
                    if (bent) {
                        curve_scratch_x_.clear();
                        curve_scratch_y_.clear();
                        core::path_outline(bent.value(), curve_scratch_x_, curve_scratch_y_);
                        addWorldRun(nextBatch(tokens_->accent.rgba(), 1.5f, false),
                                    curve_scratch_x_, curve_scratch_y_, bent.value().closed);
                        if (look_.dynamic_input) {
                            const render::ScreenPointF c = toScreenF(to);
                            const std::string text =
                                "yarıçap " +
                                trimmed(static_cast<double>(
                                            bent.value().pieces[decoded.value().edge].radius) /
                                            1000.0,
                                        3) +
                                " m";
                            addReadout(c.x + 12.0F, c.y + 24.0F, text);
                            guide_label_ = text;
                        }
                    }
                }
            }
        } else if (shape == command::RubberShape::Region) {
            // THE REGION THE CLICK WILL FIND, found by the same call (TODOS C-09).
            buildRegionPreview(session->prompt().rubber_payload, cursorWorld());
        } else if (shape == command::RubberShape::MeasureRun) {
            // THE RUN MEASURED SO FAR AND THE NEXT SEGMENT TO THE CURSOR, each
            // segment's length written on it and the running total at the
            // cursor — the tape laid along a boundary, read as it goes out.
            std::vector<core::Point2> run = session->prompt().rubber_chain;
            run.push_back(cursorWorld());
            std::vector<render::ScreenPointF> drawn;
            drawn.reserve(run.size());
            for (const core::Point2& p : run)
                drawn.push_back(render::to_f(view_.to_screen(p)));
            addRun(batch, drawn, false);

            core::Mm total = 0;
            for (std::size_t i = 1; i < run.size(); ++i) {
                const core::Mm side = core::segment_length(run[i - 1], run[i]);
                total += side;
                if (i + 1 < run.size())
                    addReadout((drawn[i - 1].x + drawn[i].x) * 0.5F + 6.0F,
                               (drawn[i - 1].y + drawn[i].y) * 0.5F - 6.0F,
                               trimmed(static_cast<double>(side) / 1000.0, 3) + " m");
            }
            if (look_.dynamic_input && run.size() >= 2) {
                // ON THE SEGMENT, at its middle, like every guide's reading: beside
                // the cursor it sat on the snap marker's own name. The total goes
                // UNDER the cursor, where nothing else is written.
                const core::Point2 last      = run[run.size() - 2];
                const render::ScreenPointF a = drawn[drawn.size() - 2];
                const render::ScreenPointF b = drawn.back();
                const std::string segment =
                    trimmed(static_cast<double>(core::segment_length(last, run.back())) / 1000.0,
                            3) +
                    " m  " + bearing_text(last, run.back(), look_.angle);
                addReadout((a.x + b.x) * 0.5F + 8.0F, (a.y + b.y) * 0.5F - 6.0F, segment);
                std::string text = segment;
                if (run.size() > 2) {
                    const std::string sum =
                        "toplam " + trimmed(static_cast<double>(total) / 1000.0, 3) + " m";
                    addReadout(b.x + 12.0F, b.y + 24.0F, sum);
                    text += "  ·  " + sum;
                }
                guide_label_ = text;
            }
        } else if (shape == command::RubberShape::MeasureRing) {
            // THE FACE THE CORNERS SO FAR AND THE CURSOR ENCLOSE, washed in the
            // accent, with its area and perimeter at the cursor — ALANÖLÇ's
            // answer as the corners are placed, not after the last one.
            std::vector<core::Point2> ring = session->prompt().rubber_chain;
            ring.push_back(cursorWorld());
            QColor wash = tokens_->accent;
            wash.setAlpha(40);
            const std::size_t face =
                ring.size() >= 3 ? nextBatch(tokens_->accent.rgba(), 1.5f, false, wash.rgba())
                                 : batch;
            curve_scratch_x_.clear();
            curve_scratch_y_.clear();
            for (const core::Point2& p : ring) {
                curve_scratch_x_.push_back(p.x);
                curve_scratch_y_.push_back(p.y);
            }
            addWorldRun(face, curve_scratch_x_, curve_scratch_y_, ring.size() >= 3);
            if (look_.dynamic_input && ring.size() >= 3) {
                const core::Mm2 signed_area = core::ring_area(ring);
                core::Mm perimeter          = 0;
                for (std::size_t i = 0; i < ring.size(); ++i)
                    perimeter += core::segment_length(ring[i], ring[(i + 1) % ring.size()]);
                const std::string text =
                    core::format_square_metres(signed_area < 0 ? -signed_area : signed_area) +
                    "  ·  çevre " + trimmed(static_cast<double>(perimeter) / 1000.0, 3) + " m";
                // IN THE FACE, where the area is: beside the cursor it fought the
                // snap marker's name, and the figure belongs to the whole shape.
                double cx = 0.0;
                double cy = 0.0;
                for (const core::Point2& p : ring) {
                    const render::ScreenPointF v = render::to_f(view_.to_screen(p));
                    cx += static_cast<double>(v.x);
                    cy += static_cast<double>(v.y);
                }
                const auto n = static_cast<double>(ring.size());
                addReadout(static_cast<float>(cx / n) - 40.0F, static_cast<float>(cy / n), text);
                guide_label_ = text;
            }
        } else if (shape == command::RubberShape::Parallel) {
            // THE PARALLELS THE CLICK WILL MAKE, each on the side of its object
            // the cursor is on — `core::parallel_side_at` and
            // `core::entity_parallel`, the two calls OFSET makes with the click.
            // The distance was typed; the cursor only chooses the side, which is
            // the one thing a number cannot say.
            if (auto decoded = core::decode_parallel_preview(session->prompt().rubber_payload)) {
                const core::Document& doc = controller_.document();
                const core::Point2 at     = cursorWorld();
                const std::size_t lit     = nextBatch(tokens_->accent.rgba(), 1.5f, false);
                for (const std::int64_t key : decoded.value().keys) {
                    const core::EntityId e =
                        doc.slot_of(static_cast<core::EntityKey>(static_cast<std::uint64_t>(key)));
                    if (e == core::kNoEntity || !doc.alive(e)) continue;
                    auto side = core::parallel_side_at(doc, e, at);
                    if (!side) continue;
                    auto made = core::entity_parallel(doc, e, decoded.value().distance,
                                                      side.value(), decoded.value().join);
                    if (!made) continue;
                    for (const core::ParallelPiece& piece : made.value().pieces) {
                        curve_scratch_x_.clear();
                        curve_scratch_y_.clear();
                        bool shut = false;
                        switch (piece.shape) {
                        case core::ParallelPiece::Shape::Run:
                            for (const core::Point2& p : piece.run) {
                                curve_scratch_x_.push_back(p.x);
                                curve_scratch_y_.push_back(p.y);
                            }
                            shut = piece.closed;
                            break;
                        case core::ParallelPiece::Shape::Face:
                            for (const core::Polygon& face : piece.faces) {
                                std::vector<core::Mm> xs;
                                std::vector<core::Mm> ys;
                                for (const core::Point2& p : face.exterior) {
                                    xs.push_back(p.x);
                                    ys.push_back(p.y);
                                }
                                addWorldRun(lit, xs, ys, true);
                                for (const std::vector<core::Point2>& hole : face.holes) {
                                    xs.clear();
                                    ys.clear();
                                    for (const core::Point2& p : hole) {
                                        xs.push_back(p.x);
                                        ys.push_back(p.y);
                                    }
                                    addWorldRun(lit, xs, ys, true);
                                }
                            }
                            break;
                        case core::ParallelPiece::Shape::Circle:
                            core::circle_outline(piece.centre, piece.radius, curve_scratch_x_,
                                                 curve_scratch_y_);
                            shut = true;
                            break;
                        case core::ParallelPiece::Shape::Arc:
                            core::arc_outline(piece.centre, piece.radius, piece.start, piece.end,
                                              curve_scratch_x_, curve_scratch_y_);
                            break;
                        }
                        if (curve_scratch_x_.size() >= 2)
                            addWorldRun(lit, curve_scratch_x_, curve_scratch_y_, shut);
                    }
                }
                if (look_.dynamic_input) {
                    const render::ScreenPointF c = toScreenF(to);
                    const std::string text =
                        "paralel " +
                        trimmed(static_cast<double>(decoded.value().distance) / 1000.0, 3) + " m";
                    addReadout(c.x + 12.0F, c.y + 24.0F, text);
                    guide_label_ = text;
                }
            }
        } else if (shape == command::RubberShape::Ghost) {
            // THE OBJECTS THEMSELVES, under the transform the cursor implies:
            // where TAŞI will put them, how far round DÖNDÜR will turn them, how
            // much bigger ÖLÇEKLE will make them, which way AYNALA will flip
            // them. The verb says WHICH transform in the payload and
            // `core::ghost_xform` turns the cursor into it — the same call the
            // verb makes when it takes the answer, so the ghost is the result
            // rather than a guess at it.
            //
            // A payload-less ghost is a move, which is what every one of them
            // was before the four kinds existed.
            core::GhostSpec spec{};
            if (auto decoded = core::decode_ghost_spec(session->prompt().rubber_payload))
                spec = decoded.value();

            const core::Point2 base = session->prompt().rubber_origin;
            const core::Xform step  = core::ghost_xform(spec, base, cursorWorld());

            // REPEATED FOR A COMMAND THAT REPEATS ITS STEP. Only a translation
            // composes with itself by simple multiples; the other three are
            // previewed once, which is all any of them applies.
            addGhost(batch, step, spec.keys);
            if (step.kind == core::Xform::Kind::Translate)
                for (std::int64_t copy = 2; copy <= spec.copies; ++copy) {
                    core::Xform further = step;
                    further.dx          = step.dx * copy;
                    further.dy          = step.dy * copy;
                    addGhost(batch, further, spec.keys);
                }
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
        } else if (const auto& chain = session->prompt().rubber_chain; !chain.empty()) {
            // THE WHOLE SHAPE SO FAR, not only its newest edge. A command whose
            // geometry cannot reach the document until it is complete (ALAN) has
            // nothing else on screen, so drawing one segment made every click
            // look like it had erased the one before it.
            std::vector<render::ScreenPointF> run;
            run.reserve(chain.size() + 1);
            for (const core::Point2& p : chain)
                run.push_back(render::to_f(view_.to_screen(p)));
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
        // Not for the area ghost: its figure is the area, written above.
        //
        // A rubber band that shows only a direction makes the user click, read the
        // result and undo. The length and the bearing belong on the line while it
        // is being dragged — that is what every CAD calls dynamic input, and what
        // a surveyor setting out a 12 cm step needs to see the step working.
        if (look_.dynamic_input && session->prompt().rubber_base &&
            shape != command::RubberShape::AreaEdit && shape != command::RubberShape::Fixed &&
            shape != command::RubberShape::Candidates && shape != command::RubberShape::Angle &&
            shape != command::RubberShape::MeasureRun &&
            shape != command::RubberShape::MeasureRing && shape != command::RubberShape::Parallel &&
            shape != command::RubberShape::Corner && shape != command::RubberShape::Break &&
            shape != command::RubberShape::TrimFence && shape != command::RubberShape::ArcSweep &&
            shape != command::RubberShape::PairCorner && shape != command::RubberShape::EdgeArc &&
            shape != command::RubberShape::Dimension &&
            shape != command::RubberShape::DimensionNext) {
            const core::Point2 from_world = session->prompt().rubber_origin;
            const core::Point2 to_world =
                snap_preview_valid_ ? snap_preview_.point
                                    : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});

            const core::Mm length = core::segment_length(from_world, to_world);
            if (length > 0) {
                std::string text = trimmed(static_cast<double>(length) / 1000.0, 3) + " m";
                text += "  " + bearing_text(from_world, to_world, look_.angle);

                // ON the line, at its middle, lifted clear of it. Beside the
                // cursor it would fight the snap marker and its mode name, which
                // are already there and are about a different thing.
                const render::ScreenPointF a = render::to_f(from);
                const render::ScreenPointF b = toScreenF(to);
                overlay_.labels.push_back(render::OverlayLabel{
                    tokens_->readout.rgba(), (a.x + b.x) * 0.5f + 8.0f, (a.y + b.y) * 0.5f - 6.0f,
                    static_cast<float>(look_.hint_px), false, text});
                guide_label_ = text;
            }
        }
    }

    buildSelectionBox();
    buildGuides();
    buildTracking();
    // THE PRINT FRAME OVER THE DRAWING and under the rulers: it is a window on
    // the drawing, so it has to sit on top of it, and the rulers and the scale
    // bar are the shell's furniture and stay readable over everything.
    buildPrintFrame();
    buildRuler();
    buildScaleBar();
    buildNorthArrow();
    buildZoomStack();
    buildReadout();
    buildCrosshair();
    buildSnapMarker();

    // Developer HUD. Dear ImGui replaces this once the GPU canvas lands; it is a
    // debug layer and never a user-facing feature (kentoscad.md §6.3), so it is off
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
#if KENTOS_HAVE_RHI
    // The GPU's own copy. `grabFramebuffer()` renders a frame and reads it back,
    // so what comes out is what the pipeline drew rather than what the widget
    // system thinks is there.
    const QImage picture = grabFramebuffer();
    if (!picture.isNull()) return picture;

    // NO GPU HERE, AND THE CALLER STILL ASKED FOR A FRAME. Under `offscreen`
    // there is no QRhi to grab with — Qt says "Failed to create dedicated QRhi
    // for grabbing" and hands back a null image — and every probe that reads the
    // scene AFTER a grab then read a scene that was never built. That is not a
    // missing picture, it is a false negative: `canvas-edits` reported that the
    // DAİRE rubber band draws no circle, on a build where it draws one.
    //
    // So the scene and the overlay are built anyway. The picture is still null
    // and the caller still has to cope with that; what it no longer has to cope
    // with is a canvas whose state depends on whether a GPU was present.
    rebuildScene();
    buildOverlay();
    return picture;
#else
    return grab().toImage();
#endif
}

render::FrameStats MapCanvas::frameStats() const
{
    return backend_ ? backend_->stats() : render::FrameStats{};
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

#if KENTOS_HAVE_RHI
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
#if KENTOS_HAVE_RHI
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

    if (event->button() == Qt::LeftButton && look_.ruler) {
        // A PRESS ON A RULER PULLS A GUIDE OUT, which is the gesture every
        // drafting program has had since the drawing board: the ruler is where
        // guides come from. Only when nothing is being asked for — a command
        // waiting on a point owns every click on this widget.
        const auto band = static_cast<double>(look_.ruler_px);
        if (!controller_.awaitingInput()) {
            if (event->position().y() < band && event->position().x() >= band) {
                dragging_guide_ = 0; // horizontal: dragged down out of the top ruler
                cursor_         = event->position();
                cursor_valid_   = true;
                update();
                return;
            }
            if (event->position().x() < band && event->position().y() >= band) {
                dragging_guide_ = 1; // vertical: dragged right out of the left ruler
                cursor_         = event->position();
                cursor_valid_   = true;
                update();
                return;
            }
        }
    }

    // THE PRINT FRAME takes the right button and nothing else: the left drag
    // pans the map under the frame, which is the whole gesture (see
    // `beginPrintFrame`). Esc does the same through `keyPressEvent`.
    if (print_aspect_ > 0.0 && event->button() == Qt::RightButton) {
        endPrintFrame();
        return;
    }
    if (print_aspect_ > 0.0 && event->button() == Qt::LeftButton) {
        panning_    = true;
        pan_anchor_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // A FORM FIELD'S PICK OWNS THE CLICK while it is armed: the answer goes into
    // the box that asked, and nothing else moves (see `beginCapture`).
    if (capture_ && event->button() == Qt::LeftButton) {
        cursor_       = event->position();
        cursor_valid_ = true;
        if (*capture_ == Capture::Point) {
            updateSnapPreview();
            const core::Point2 world = cursorWorld();
            capture_.reset();
            snap_preview_valid_ = false;
            applyPointer();
            emit pointCaptured(world);
            emit captureEnded();
            update();
            return;
        }
        const core::Point2 at =
            view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
        std::vector<core::EntityId> under;
        core::pick_all(controller_.document(), at, controller_.bus().aid_settings().pick_radius,
                       under);
        if (under.empty()) {
            emit echoRequested(
                tr("Burada nesne yok; bir nesnenin üzerine tıklayın, Esc vazgeçer."));
            return;
        }
        if (under.size() > 1) {
            emit captureAmbiguous(under);
            return;
        }
        finishCapture(controller_.document().key_of(under.front()));
        return;
    }
    if (capture_ && event->button() == Qt::RightButton) {
        cancelCapture();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // A COMMAND ASKING FOR OBJECTS DOES NOT OWN THE CLICK — the selection
        // does. Picking during a command is the same picking as when none is
        // running: the press starts a box, the release sends `SEÇ`, the highlight
        // and the Shift/Ctrl keys behave as they always do. Falling through here
        // is what makes that true, rather than a second hit-test living in the
        // canvas (Article 1.2). Enter then hands the result to the command.
        const bool picking = controller_.awaitingInput() &&
                             controller_.promptKind() == command::ParamKind::Selection;

        if (controller_.awaitingInput() && !picking) {
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
            // the command layer, on the path every client takes (kentoscad.md §2.4,
            // kentos_cad/command/aids.hpp). A canvas that snapped first would be a
            // client with a private route.
            const core::Point2 world =
                view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
            const QPointF at = event->position();
            controller_.supplyPoint(world);

            // THE BOX OPENS ON THE CLICK THAT EARNED IT. METİN asks for its anchor
            // first and its string second, so the prompt turns into a text prompt
            // inside the call above — and waiting for another click would make the
            // user click twice in the same place with nothing to tell them why.
            // UNLESS THE QUESTION SAID WHERE ITS WORDS STAND: it opened the box
            // there already (`promptChanged`), and moving it to the click put a
            // block's attribute value a metre from the number it becomes.
            if (controller_.awaitingInput() &&
                controller_.promptKind() == command::ParamKind::Text && !textEditorOpen())
                openTextEditor(at);

            snap_preview_valid_ = false;
            update();
            return;
        }

        // A grip under the pointer takes the press: the user is reaching for a
        // corner of something already selected, and a selection box started there
        // would throw that selection away on the way to editing it.
        //
        // Not while a command is asking which objects to act on, though: there the
        // press is always part of the answer, and moving a corner instead would
        // edit the drawing in the middle of being asked a question.
        if (const Grip grip = picking ? Grip{} : gripAt(event->position()); grip.valid()) {
            // A LOCKED HANDLE SAYS WHY instead of starting a drag nothing would
            // take: the command itself refuses, with the lock's own sentence
            // and how to open it (TODOS C-07).
            if (grip.locked) {
                controller_.beginOneShot(gripLine(grip), command::Origin::Gui);
                update();
                return;
            }
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

    // SHIFT + RIGHT BUTTON MARKS A POINT FOR TRACKING, which is the hand version
    // of `İZ <nokta>`. The point marked is the SNAPPED one, so a corner is
    // acquired exactly and not to the pixel — acquiring the corner is the whole
    // gesture, and a mark half a millimetre off it would put every trace half a
    // millimetre off too.
    //
    // It goes through the COMMAND, not into the bus directly: a mark made by the
    // mouse and a mark typed at the prompt have to be one thing, and the command
    // is that one thing (Article 1.1).
    if (event->button() == Qt::RightButton && (event->modifiers() & Qt::ShiftModifier) != 0) {
        const core::Point2 at = snap_preview_valid_
                                    ? snap_preview_.point
                                    : view_.to_world(render::ScreenPoint{cursor_.x(), cursor_.y()});
        controller_.runLine(QStringLiteral("İZ %1,%2")
                                .arg(core::mm_to_metres(at.x), 0, 'f', 3)
                                .arg(core::mm_to_metres(at.y), 0, 'f', 3),
                            command::Origin::Gui);
        update();
        return;
    }

    if (event->button() == Qt::RightButton) {
        // THE RIGHT BUTTON FINISHES. While a command is asking WHICH objects it
        // means "those ones, go"; while it is asking for points it means "that is
        // the shape, done" — the run closes on what it has and the tool stays in
        // the hand for the next one. Only Esc puts a tool away
        // (`Controller::finishInteractive` versus `cancelInteractive`). With no
        // command running the button does nothing yet.
        if (!controller_.supplyPickedObjects() && controller_.session())
            controller_.finishInteractive();
        snap_preview_valid_ = false;
        update();
    }
}

void MapCanvas::enterEvent(QEnterEvent* event)
{
    // THE DRAWN CROSSHAIR IS THE POINTER. The platform arrow on top of it was two
    // pointers for one hand, and neither of them was the one a CAD user aims
    // with: the arrow's tip is off the crosshair's centre by its own shape.
    applyPointer();
    QWidget::enterEvent(event);
}

void MapCanvas::leaveEvent(QEvent* event)
{
    cursor_valid_       = false;
    snap_preview_valid_ = false;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void MapCanvas::applyPointer()
{
    if (panning_ || dragging_grip_) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (capture_) {
        setCursor(captureCursor());
        return;
    }
    if (print_aspect_ > 0.0) {
        // An open hand: while the frame is up the gesture is panning the map
        // under it, and the drawn crosshair would say a click places something.
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (look_.cursor == 2 || (text_editor_ != nullptr && text_editor_->isVisible())) {
        unsetCursor();
        return;
    }
    setCursor(Qt::BlankCursor);
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_guide_ >= 0) {
        cursor_       = event->position();
        cursor_valid_ = true;
        update();
        return;
    }

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
    if (event->button() == Qt::LeftButton && dragging_guide_ >= 0) {
        const int axis  = dragging_guide_;
        dragging_guide_ = -1;

        const auto band = static_cast<double>(look_.ruler ? look_.ruler_px : 0);

        // DROPPED BACK ON THE RULER MEANS "no guide after all", which is how
        // Inkscape and every other program spells cancel for this gesture. The
        // press already came off the ruler, so releasing there is a round trip.
        const bool back_on_ruler =
            axis == 0 ? event->position().y() < band : event->position().x() < band;
        if (back_on_ruler) {
            update();
            return;
        }

        const core::Point2 where =
            view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
        const core::Mm coordinate = axis == 0 ? where.y : where.x;

        // THROUGH THE COMMAND, like every other change this widget makes. The
        // drag is a gesture; `KILAVUZ` is the feature, and a script places the
        // same guide with the same line (CLAUDE.md 1.1, 5.9).
        command::Args args;
        args.set("yon", command::Value::text(axis == 0 ? "yatay" : "düşey"));
        args.set("deger", command::Value::integer(coordinate));
        controller_.runInvocation(
            command::Invocation{"core.guide", std::move(args), command::Origin::Gui});

        update();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        applyPointer();
        return;
    }

    // A LEFT-BUTTON PAN ENDS HERE TOO, and it has to be said separately: the
    // middle button is the only one that used to start one, so the release
    // handler only cleared `panning_` for that button. The print frame pans with
    // the LEFT button (`beginPrintFrame`), and without this line the map went on
    // following the mouse after the button came up — a drag that never let go,
    // which is exactly how it was reported.
    if (event->button() == Qt::LeftButton && panning_) {
        panning_ = false;
        cursor_  = event->position();
        applyPointer();
        update();
        return;
    }

    if (event->button() == Qt::LeftButton && dragging_grip_) {
        dragging_grip_ = false;
        cursor_        = event->position();
        cursor_valid_  = true;
        applyPointer();

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

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    // A DOUBLE CLICK ON AN OBJECT OPENS WHAT EDITS IT (TODOS C-17) — a caption
    // its words, a dimension its figure — the gesture every CAD has for it.
    // Only when nothing owns the click: while a command asks for a point or
    // for objects, a form field's pick is armed, the print frame is up or the
    // press is on a ruler, the second press is the press it always was.
    const auto band  = static_cast<double>(look_.ruler ? look_.ruler_px : 0);
    const bool ruler = event->position().x() < band || event->position().y() < band;
    const bool owned = controller_.awaitingInput() || capture_.has_value() || print_aspect_ > 0.0 ||
                       dragging_grip_ || dragging_guide_ >= 0 || ruler;
    if (event->button() == Qt::LeftButton && !owned) {
        const core::Point2 at =
            view_.to_world(render::ScreenPoint{event->position().x(), event->position().y()});
        std::vector<core::EntityId> under;
        core::pick_all(controller_.document(), at, controller_.bus().aid_settings().pick_radius,
                       under);
        if (!under.empty()) {
            selecting_ = false;
            emit entityActivated(controller_.document().key_of(under.front()));
            return;
        }
    }
    mousePressEvent(event);
}

double wheel_zoom_factor(const core::Settings& store, double notches)
{
    if (notches == 0.0) return 1.0;
    if (store.get("core.harita.tekerlek_ters").as_bool()) notches = -notches;

    // The step is a PERCENTAGE of the current scale, so every notch feels the same
    // at every zoom. 20 % is the default and the range is wide on purpose: the
    // people who want three notches per decade and the people who want thirty are
    // both right about their own hands.
    const auto percent = static_cast<double>(store.get("core.harita.yakinlastirma_adimi").as_int());
    return std::pow(1.0 + percent / 100.0, notches);
}

void MapCanvas::wheelEvent(QWheelEvent* event)
{
    const double notches = event->angleDelta().y() / 120.0;
    if (notches == 0.0) return;

    // Through the shared helper rather than through `look_`, so the preview in the
    // import wizard cannot drift from this: one reading of the settings, one
    // direction. The lookup is two folded string compares and this runs on a
    // wheel event, not inside the frame budget.
    view_.zoom_at(render::ScreenPoint{event->position().x(), event->position().y()},
                  wheel_zoom_factor(controller_.bus().app_settings(), notches));
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
    // TEXT, because `MOD` declares `deger` as text (`Param::text`). A boolean
    // here failed validation before the body ever ran: the bus answered "Hata",
    // the echo carried it where nobody was looking, and Ctrl locked nothing at
    // all for as long as this had been written that way.
    command::Args args;
    args.set("ad", command::Value::text("köşegen"));
    args.set("deger", command::Value::text(on ? "evet" : "hayır"));
    controller_.runInvocation(
        command::Invocation{"core.mode", std::move(args), command::Origin::Gui});

    // The preview is what makes the lock legible: the rubber band must snap to
    // the diagonal the moment the key goes down, not at the next mouse move.
    updateSnapPreview();
    update();
}

void MapCanvas::focusOutEvent(QFocusEvent* event)
{
    // A key release is delivered to whoever has focus, so a held key that leaves
    // the canvas — Alt+Tab, a click in the command line — never comes back up
    // here. Both held locks are dropped rather than left standing.
    setDiagonalLock(false);
    setSurfaceNormalLock(false);
    QWidget::focusOutEvent(event);
}

void MapCanvas::setSurfaceNormalLock(bool on)
{
    if (normal_lock_ == on) return;
    normal_lock_ = on;

    // Same road as the diagonal lock, and for the same reason: the key is a way of
    // holding a mode down, not a capability of its own. `MOD yüzey_normali evet`
    // from the command line, from a script or from the AI does exactly this write
    // (Article 1.2, 5.15).
    command::Args args;
    args.set("ad", command::Value::text("yüzey_normali"));
    args.set("deger", command::Value::text(on ? "evet" : "hayır"));
    controller_.runInvocation(
        command::Invocation{"core.mode", std::move(args), command::Origin::Gui});

    updateSnapPreview();
    update();
}

void MapCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control) setDiagonalLock(false);

    // RELEASED UNCONDITIONALLY, unlike the press. The press only engages while a
    // command is picking, but the command can END while the key is still down —
    // and a lock nobody turned off would then quietly steer the NEXT line.
    if (event->key() == Qt::Key_Shift) setSurfaceNormalLock(false);
    QWidget::keyReleaseEvent(event);
}

void MapCanvas::keyPressEvent(QKeyEvent* event)
{
    // Held, not toggled: the lock lasts exactly as long as the key does.
    if (event->key() == Qt::Key_Control) {
        setDiagonalLock(true);
        return;
    }

    // ONLY WHILE A COMMAND IS PICKING. Shift also means "add to the selection"
    // (`dispatchSelection`), and the two never overlap: a box selection is drawn
    // when no command is waiting for a point, and there is no surface to stand
    // normal to before the first point is placed. Guarding on the session is what
    // keeps one key honest in both jobs.
    if (event->key() == Qt::Key_Shift && controller_.session()) {
        setSurfaceNormalLock(true);
        return;
    }

    // ENTER FINISHES THE PICKING — when the canvas is the one holding focus. It
    // usually is not: focus starts on the command line and a tool button is
    // `NoFocus`, so `CommandLine` sees the key first and `MainWindow` routes it to
    // the same body. Both roads, one answer.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // ENTER TAKES THE SHEET. The button's second press does the same thing;
        // this is the keyboard's road to it, because every operation has to be
        // reachable without a mouse (ui.md R21).
        if (print_aspect_ > 0.0) {
            emit printFrameAccepted();
            return;
        }
        if (controller_.supplyPickedObjects()) {
            update();
            return;
        }
        if (acceptGuide()) {
            update();
            return;
        }
        if (finishPointRun()) return;
    }

    if (event->key() == Qt::Key_Escape) {
        // A form field's pick is the innermost thing of all: Esc puts it away and
        // leaves the command, if one is running, exactly where it was.
        if (capture_) {
            cancelCapture();
            return;
        }
        if (print_aspect_ > 0.0) {
            endPrintFrame();
            return;
        }
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
        // Nothing running: ESC clears the measurements left on the canvas and
        // the selection — the second by sending the command, not by reaching
        // into the bus (Article 1.2); the first is view state and never was
        // the document's.
        clearMeasureMarks();
        if (!controller_.bus().selection().empty()) {
            command::Args args;
            args.set("mod", command::Value::text("TEMİZLE"));
            controller_.runInvocation(
                command::Invocation{"core.select", std::move(args), command::Origin::Gui});
        }
        // And the tracking marks made while nothing was running, the same way:
        // through `İZ sil=evet`, which says how many it forgot.
        if (!controller_.bus().tracking_marks().empty()) {
            command::Args args;
            args.set("sil", command::Value::boolean(true));
            controller_.runInvocation(
                command::Invocation{"core.tracking", std::move(args), command::Origin::Gui});
        }
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ------------------------------------------------------- the print frame ----

void MapCanvas::beginPrintFrame(double aspect)
{
    if (aspect <= 0.0) return;
    print_aspect_ = aspect;
    applyPointer();
    setFocus(Qt::OtherFocusReason); // so Esc lands here
    emit printFrameBegan(tr("Yazdırma alanı: haritayı sürükleyip tekerlekle yaklaşın, çerçeveye "
                            "ne giriyorsa yazdırılır. Yazdır'a basmak önizlemeyi açar, Esc "
                            "vazgeçer."));
    update();
}

void MapCanvas::setPrintFrameAspect(double aspect)
{
    if (print_aspect_ <= 0.0 || aspect <= 0.0) return;
    print_aspect_ = aspect;
    update();
}

void MapCanvas::endPrintFrame()
{
    if (print_aspect_ <= 0.0) return;
    print_aspect_ = 0.0;
    applyPointer();
    emit printFrameEnded();
    update();
}

QRectF MapCanvas::printFrameRect() const
{
    if (print_aspect_ <= 0.0) return {};

    // INSET FROM THE VIEWPORT, and past the rulers when they are on: a frame
    // whose edge sat under the ruler would be a frame whose corner cannot be
    // seen. The inset is a fraction of the shorter side, so the frame is the
    // same size relative to the window on every screen.
    const auto band = look_.ruler ? static_cast<double>(look_.ruler_px) : 0.0;
    // A BAND ON ALL FOUR SIDES, whatever the window's shape: an inset of a few
    // fixed pixels left no grey at all along the two sides the sheet's aspect
    // happened to fill, and a sheet whose edge is the window's edge does not
    // read as a sheet. A fraction of the shorter side keeps it the same
    // proportion on a laptop and on a 4K panel.
    const double shorter =
        std::min(static_cast<double>(width()) - band, static_cast<double>(height()) - band);
    const double inset = std::max(18.0, shorter * 0.05);
    const double x0    = band + inset;
    const double y0    = band + inset;
    const double w     = static_cast<double>(width()) - x0 - inset;
    const double h     = static_cast<double>(height()) - y0 - inset;
    if (w <= 8.0 || h <= 8.0) return {};

    double fw = w;
    double fh = w / print_aspect_;
    if (fh > h) {
        fh = h;
        fw = h * print_aspect_;
    }
    return QRectF(x0 + (w - fw) / 2.0, y0 + (h - fh) / 2.0, fw, fh);
}

core::Box2 MapCanvas::printFrameWindow() const
{
    const QRectF frame = printFrameRect();
    if (frame.isEmpty()) return {};

    // The two opposite corners through the view transform. y is DOWN on screen
    // and UP in the document, so the top-left pixel is the top-left of the box
    // in easting and the TOP in northing — hence the min/max below rather than
    // a straight copy.
    const core::Point2 a = view_.to_world(render::ScreenPoint{frame.left(), frame.top()});
    const core::Point2 b = view_.to_world(render::ScreenPoint{frame.right(), frame.bottom()});
    return core::Box2{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
                      std::max(a.y, b.y)};
}

void MapCanvas::buildPrintFrame()
{
    const QRectF frame = printFrameRect();
    if (frame.isEmpty()) return;

    // THE MASK: four rectangles around the frame, filled with the same colour
    // they are outlined in, so the four seams between them are invisible.
    //
    // A MID GREY, not the theme's own background: `bgApp` is nearly white in
    // the light theme, and a nearly-white wash over a white canvas is a wash
    // nobody can see — the first cut of this drew a frame with no dimming
    // outside it at all. A neutral grey darkens a light canvas and lightens a
    // dark one, so "outside the sheet" reads the same in both themes, and the
    // alpha keeps the drawing out there legible because the user is aiming with
    // it.
    QColor veil(0x80, 0x84, 0x88, 132);
    const std::size_t mask = nextBatch(veil.rgba(), 1.0f, false, veil.rgba());
    const auto W           = static_cast<float>(width());
    const auto H           = static_cast<float>(height());
    const auto l           = static_cast<float>(frame.left());
    const auto t           = static_cast<float>(frame.top());
    const auto r           = static_cast<float>(frame.right());
    const auto b           = static_cast<float>(frame.bottom());
    addRun(mask, {{0.0f, 0.0f}, {W, 0.0f}, {W, t}, {0.0f, t}}, true);
    addRun(mask, {{0.0f, b}, {W, b}, {W, H}, {0.0f, H}}, true);
    addRun(mask, {{0.0f, t}, {l, t}, {l, b}, {0.0f, b}}, true);
    addRun(mask, {{r, t}, {W, t}, {W, b}, {r, b}}, true);

    // The sheet's own edge: a hairline, so it reads as the paper's boundary
    // rather than as something drawn.
    QColor edge = tokens_->accent;
    edge.setAlpha(190);
    const std::size_t line = nextBatch(edge.rgba(), 1.0f, false);
    addRun(line, {{l, t}, {r, t}, {r, b}, {l, b}}, true);

    // AND THE FOUR CORNERS, as L marks in the accent: the crop marks a plotter
    // sheet has, and the thing that says "this is a sheet" at a glance.
    const auto arm =
        static_cast<float>(std::min(28.0, std::min(frame.width(), frame.height()) / 6.0));
    const std::size_t mark = nextBatch(tokens_->accent.rgba(), 2.0f, false);
    addRun(mark, {{l, t + arm}, {l, t}, {l + arm, t}}, false);
    addRun(mark, {{r - arm, t}, {r, t}, {r, t + arm}}, false);
    addRun(mark, {{r, b - arm}, {r, b}, {r - arm, b}}, false);
    addRun(mark, {{l + arm, b}, {l, b}, {l, b - arm}}, false);

    // THE SHEET'S CENTRE, as a cross with a gap in it. A pafta is placed by its
    // centre — `YAZDIR merkez=` is the same sheet said as a coordinate — so the
    // point the paper turns around has to be visible, and its coordinates have
    // to be readable off the screen rather than worked out from the corners.
    const auto cx            = static_cast<float>(frame.center().x());
    const auto cy            = static_cast<float>(frame.center().y());
    const float reach        = 13.0f;
    const float hole         = 4.0f;
    const std::size_t centre = nextBatch(tokens_->accent.rgba(), 1.4f, false);
    addRun(centre, {{cx - reach, cy}, {cx - hole, cy}}, false);
    addRun(centre, {{cx + hole, cy}, {cx + reach, cy}}, false);
    addRun(centre, {{cx, cy - reach}, {cx, cy - hole}}, false);
    addRun(centre, {{cx, cy + hole}, {cx, cy + reach}}, false);

    // The reading, in the Turkish convention: `Sağa (Y)` is the easting and
    // `Yukarı (X)` the northing, whatever the members are called in code
    // (model.md R37a). Printing `X` beside a 485 km value tells a surveyor
    // something false.
    const core::Point2 at =
        view_.to_world(render::ScreenPoint{frame.center().x(), frame.center().y()});
    char reading[96];
    (void)std::snprintf(reading, sizeof reading, "Y %.3f  X %.3f", core::mm_to_metres(at.x),
                        core::mm_to_metres(at.y));
    overlay_.labels.push_back(render::OverlayLabel{tokens_->accent.rgba(), cx + reach + 6.0f,
                                                   cy + 4.0f, static_cast<float>(look_.hint_px),
                                                   true, reading});
}

// ------------------------------------------------------ a field's pick ----

void MapCanvas::beginCapture(Capture kind)
{
    capture_            = kind;
    snap_preview_valid_ = false;
    applyPointer();
    setFocus(Qt::OtherFocusReason); // so Esc lands here
    emit captureBegan(kind == Capture::Point
                          ? tr("Sahneden bir nokta tıklayın; köşeler yakalanır. Esc vazgeçer.")
                          : tr("Sahneden bir nesne tıklayın. Esc vazgeçer."));
    update();
}

void MapCanvas::cancelCapture()
{
    if (!capture_) return;
    capture_.reset();
    snap_preview_valid_ = false;
    applyPointer();
    emit captureEnded();
    update();
}

void MapCanvas::finishCapture(core::EntityKey key)
{
    if (!capture_) return;
    capture_.reset();
    applyPointer();
    if (key != core::EntityKey::None) emit objectCaptured(key);
    emit captureEnded();
    update();
}

QCursor MapCanvas::captureCursor() const
{
    // DRAWN, in the accent, at the screen's own ratio: a cross whose arms stop
    // short of the centre for a point, and the same cross with the pick box in
    // the gap for an object — the mark the canvas's own crosshair draws while a
    // command asks the same question, so the two readings agree.
    const qreal dpr = devicePixelRatioF();
    const int size  = 32;
    QPixmap pix(static_cast<int>(size * dpr), static_cast<int>(size * dpr));
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink = tokens_->accent;
    QPen pen(ink, 1.5);
    p.setPen(pen);
    const qreal c   = size / 2.0;
    const qreal gap = capture_ == Capture::Object ? 6.0 : 3.0;
    p.drawLine(QPointF(c, 1.0), QPointF(c, c - gap));
    p.drawLine(QPointF(c, c + gap), QPointF(c, size - 1.0));
    p.drawLine(QPointF(1.0, c), QPointF(c - gap, c));
    p.drawLine(QPointF(c + gap, c), QPointF(size - 1.0, c));
    if (capture_ == Capture::Object) {
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(c - 5.0, c - 5.0, 10.0, 10.0));
    } else {
        p.setBrush(ink);
        p.drawEllipse(QPointF(c, c), 1.2, 1.2);
    }
    p.end();
    return QCursor(pix, size / 2, size / 2);
}

// ------------------------------------------------------ the text editor ----

void MapCanvas::openTextEditor(const QPointF& where, BoxAlign align)
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
            const bool abandon = text_editor_abandons_;
            closeTextEditor();
            if (abandon) controller_.cancelInteractive();
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
    int left    = static_cast<int>(where.x());
    if (align == BoxAlign::Centre) left -= w / 2;
    if (align == BoxAlign::Right) left -= w;
    const int x = std::clamp(left, 0, std::max(0, width() - w));
    const int y = std::clamp(static_cast<int>(where.y()) - h / 2, 0, std::max(0, height() - h));

    text_editor_->setGeometry(x, y, w, h);
    text_editor_->clear();
    text_editor_->show();
    text_editor_->setFocus(Qt::OtherFocusReason);
}

void MapCanvas::editTextAt(core::Point2 world, const QString& text)
{
    const render::ScreenPoint at = view_.to_screen(world);
    openTextEditor(QPointF(at.x, at.y), BoxAlign::Centre);
    text_editor_->setText(text);
    text_editor_->selectAll();
    text_editor_abandons_ = true;
}

std::vector<std::string> MapCanvas::noteTextsForProbe() const
{
    std::vector<std::string> out;
    out.reserve(overlay_.labels.size());
    for (const render::OverlayLabel& label : overlay_.labels)
        out.push_back(label.text);
    return out;
}

bool MapCanvas::textEditorOpen() const noexcept
{
    return text_editor_ != nullptr && text_editor_->isVisible();
}

void MapCanvas::closeTextEditor()
{
    text_editor_abandons_ = false;
    if (text_editor_ == nullptr) return;
    text_editor_->hide();
    text_editor_->clear();
    setFocus(Qt::OtherFocusReason);
}

} // namespace kentos::app
