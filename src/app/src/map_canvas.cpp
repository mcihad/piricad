// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/map_canvas.hpp"

#include "piricad/app/backend_factory.hpp"
#include "piricad/app/controller.hpp"
#include "piricad/core/settings.hpp"
#include "piricad/render/backend.hpp"

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QScreen>
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
    tokens_  = mode == ThemeMode::Dark ? &darkTokens() : &lightTokens();
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

    // Taken by reference AFTER the last `nextBatch` of this function, which is
    // the only shape in which that is safe: nothing below grows the vector.
    render::OverlayBatch& batch =
        overlay_.batches[nextBatch(palette_.selection.rgba(), 3.0f, false)];

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

std::string trimmed(double value, int places)
{
    std::string out = QString::number(value, 'f', places).toStdString();
    if (out.find('.') == std::string::npos) return out;
    while (!out.empty() && out.back() == '0')
        out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    return out;
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

void MapCanvas::addRun(std::size_t index, std::initializer_list<render::ScreenPointF> points,
                       bool closed)
{
    render::OverlayBatch& batch = overlay_.batches[index];
    for (const render::ScreenPointF& p : points) {
        batch.xs.push_back(p.x);
        batch.ys.push_back(p.y);
    }
    batch.runs.push_back(static_cast<std::uint32_t>(points.size()));
    batch.closed.push_back(closed ? 1 : 0);
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
        if (session->prompt().rubber_shape == command::RubberShape::Rectangle) {
            // THE FACE, not its diagonal. A rectangle previewed as one line tells
            // the user nothing about what the next click will make, and with the
            // diagonal lock held it is the difference between seeing a square and
            // finding out you drew one.
            const render::ScreenPointF a = render::to_f(from);
            const render::ScreenPointF b = toScreenF(to);
            addRun(batch, {{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}}, true);
        } else {
            addRun(batch, {render::to_f(from), toScreenF(to)}, false);
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

std::vector<int> MapCanvas::timeFrames(int rounds)
{
    // Through the real paint path, not a private one: a number measured on a
    // shortcut is a number about the shortcut. `repaint()` paints synchronously,
    // so `last_frame_us_` is the round that just finished.
    std::vector<int> costs;
    costs.reserve(static_cast<std::size_t>(std::max(0, rounds)));
    for (int i = 0; i < rounds; ++i) {
        repaint();
        // The BACKEND's share. Scene rebuild and overlay run in the same paint
        // and, on a sheet of patterned parcels, dwarf it — which is a fact about
        // where the time goes, not about which backend to keep, so the two are
        // reported apart.
        costs.push_back(last_draw_us_);
        scene_costs_.push_back(last_scene_us_);
    }
    return costs;
}

void MapCanvas::paintEvent(QPaintEvent*)
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
    // Cast HERE, not in the backend: QWidget inherits QObject and QPaintDevice
    // both, and passing a QWidget* through a void* to be read as a QPaintDevice*
    // hands over the wrong address.
    ctx.target = static_cast<QPaintDevice*>(this);

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
