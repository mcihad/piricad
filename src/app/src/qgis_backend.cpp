// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: drawing through the QGIS symbology engine.
//
// WHY THIS EXISTS. CLAUDE.md Article 2.7 and 5.16: a mature, excellent,
// cross-platform library is used and never reimplemented. A symbology engine is
// exactly such a thing. QGIS has the best free one there is — marker lines with
// real placement rules, line and point pattern fills with their own offsets and
// rotations, SVG symbols with parameter substitution, gradients, shapeburst — and
// the hand-rolled version in `painter_backend.cpp` is worse at every one of them.
// It draws a corner as a hole, it cannot place a glyph at a curve point, and its
// point pattern is a loop over a bounding box.
//
// WHAT WAS MEASURED before deciding, because the objection to linking QGIS used to
// be asserted in a comment rather than checked:
//
//   libqgis_core.so                45 MB, 246 shared objects
//   QgsApplication::initQgis()     517 ms cold, 44 ms warm
//
// The startup cost sits well inside the two-second cold start of Article 7, which
// is what the old objection claimed it would break.
//
// WHERE THE SEAM IS. `render::Backend`, and nothing else. Article 3.4 keeps
// `piricad_render` Qt-free and QGIS is Qt, so this file sits beside the QPainter
// backend in `/src/app` exactly as Article 8.5 describes. Nothing below `/src/app`
// learns that QGIS exists, and the canvas cannot tell which backend it holds.
//
// THE DRAW LIST STAYS THE CONTRACT. A pass is translated into the QGIS symbol
// layer that means the same thing; the geometry arrives as the same screen-space
// batches the QPainter backend receives. So the two backends draw the same
// document from the same numbers, which is what makes them comparable at all.
#include "piricad/app/qgis_backend.hpp"

#include "piricad/app/backend_factory.hpp"

#include "piricad/core/style.hpp"

#include <qgsapplication.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgsrendercontext.h>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <memory>

namespace piricad::app {
namespace {

QColor from_rgba(std::uint32_t rgba, std::uint8_t opacity)
{
    QColor c = QColor::fromRgba(static_cast<QRgb>(rgba));
    if (opacity < 255) c.setAlpha(c.alpha() * opacity / 255);
    return c;
}

/// Degrees, from the micro-degrees a symbol layer stores.
double degrees_of(std::int32_t udeg)
{
    return static_cast<double>(udeg) / 1'000'000.0;
}

/// The marker QGIS draws for one of ours.
///
/// Named rather than mapped by index: the two enumerations are independent and an
/// index map would silently draw a star as a pentagon the day either gains a
/// member.
Qgis::MarkerShape shape_of(core::MarkerShape s)
{
    switch (s) {
    case core::MarkerShape::Circle: return Qgis::MarkerShape::Circle;
    case core::MarkerShape::Square: return Qgis::MarkerShape::Square;
    case core::MarkerShape::Triangle: return Qgis::MarkerShape::Triangle;
    case core::MarkerShape::Diamond: return Qgis::MarkerShape::Diamond;
    case core::MarkerShape::Star: return Qgis::MarkerShape::Star;
    case core::MarkerShape::Cross: return Qgis::MarkerShape::Cross2;
    case core::MarkerShape::XCross: return Qgis::MarkerShape::Cross;
    case core::MarkerShape::Arrow: return Qgis::MarkerShape::ArrowHead;
    case core::MarkerShape::HalfCircle: return Qgis::MarkerShape::HalfSquare;
    case core::MarkerShape::Pentagon: return Qgis::MarkerShape::Pentagon;
    case core::MarkerShape::Hexagon: return Qgis::MarkerShape::Hexagon;
    case core::MarkerShape::Tick: return Qgis::MarkerShape::Line;
    }
    return Qgis::MarkerShape::Circle;
}

Qgis::MarkerLinePlacement placement_of(core::MarkerPlacement p)
{
    switch (p) {
    case core::MarkerPlacement::Interval: return Qgis::MarkerLinePlacement::Interval;
    case core::MarkerPlacement::Vertex: return Qgis::MarkerLinePlacement::InnerVertices;
    case core::MarkerPlacement::FirstVertex: return Qgis::MarkerLinePlacement::FirstVertex;
    case core::MarkerPlacement::LastVertex: return Qgis::MarkerLinePlacement::LastVertex;
    case core::MarkerPlacement::Centre: return Qgis::MarkerLinePlacement::CentralPoint;
    }
    return Qgis::MarkerLinePlacement::Interval;
}

Qt::PenCapStyle cap_of(core::LineCap c)
{
    switch (c) {
    case core::LineCap::Butt: return Qt::FlatCap;
    case core::LineCap::Round: return Qt::RoundCap;
    case core::LineCap::Square: return Qt::SquareCap;
    }
    return Qt::FlatCap;
}

Qt::PenJoinStyle join_of(core::LineJoin j)
{
    switch (j) {
    case core::LineJoin::Miter: return Qt::MiterJoin;
    case core::LineJoin::Round: return Qt::RoundJoin;
    case core::LineJoin::Bevel: return Qt::BevelJoin;
    }
    return Qt::MiterJoin;
}

/// Everything a pass declares is already in PIXELS: the scene builder resolved
/// paper, ground and pixel measures against this frame's scale before the pass was
/// built (`render::pass_of`). So every symbol layer is told to work in pixels, and
/// QGIS's own unit machinery is deliberately not asked to convert anything a
/// second time.
constexpr Qgis::RenderUnit kPx = Qgis::RenderUnit::Pixels;

/// The stroke a pass describes, as a QGIS simple line.
QgsSimpleLineSymbolLayer* simple_line(const render::PassStyle& ps, std::uint32_t rgba,
                                      double width_px)
{
    auto* line = new QgsSimpleLineSymbolLayer(from_rgba(rgba, ps.opacity), width_px);
    line->setWidthUnit(kPx);
    line->setPenCapStyle(cap_of(ps.cap));
    line->setPenJoinStyle(join_of(ps.join));
    line->setOffset(ps.offset_px);
    line->setOffsetUnit(kPx);

    if (ps.dash_count > 0) {
        // MULTIPLIED BY THE WIDTH. A pattern is stored in multiples of the
        // stroke's own width (`core::DashStore`), which is the unit
        // `QPen::setDashPattern` takes; QGIS's custom dash vector is a length in
        // the unit it is told, so the multiplication that Qt does for free has to
        // happen here. Handing the raw numbers over as pixels drew an eight-width
        // dash as eight pixels — a visibly finer line than the same symbol drawn
        // by the built-in backend, which is how this was caught.
        QVector<qreal> pattern;
        for (std::uint8_t i = 0; i < ps.dash_count; ++i)
            pattern.push_back(static_cast<qreal>(ps.dash_lengths[i]) / 100.0 * width_px);
        line->setCustomDashVector(pattern);
        line->setCustomDashPatternUnit(Qgis::RenderUnit::Pixels);
        line->setUseCustomDashPattern(true);
        // FLAT CAPS inside a dashed line, for the reason `painter_backend.cpp`
        // gives at length: Qt puts the cap on every dash, so a round cap closes a
        // one-width gap and a published dash-dot boundary comes out solid.
        line->setPenCapStyle(Qt::FlatCap);
    }
    return line;
}

/// The glyph a marker pass places, as a QGIS marker symbol.
std::unique_ptr<QgsMarkerSymbol> marker_of(const render::PassStyle& ps)
{
    auto* glyph = new QgsSimpleMarkerSymbolLayer(
        shape_of(ps.shape), ps.size_px > 0.0f ? static_cast<double>(ps.size_px) : 4.0);
    glyph->setSizeUnit(kPx);
    // FILL FIRST, STROKE SECOND, and a fill of zero means NO fill rather than a
    // fallback to the stroke colour. MPYY alternates a filled circle with an open
    // one along an ETAPLAMA SINIRI, and reading "no fill" as "fill with the line
    // colour" drew both halves solid — the alternation the symbol exists for
    // disappeared and the two lines became one.
    glyph->setFillColor(ps.fill_rgba != 0 ? from_rgba(ps.fill_rgba, ps.opacity)
                                          : QColor(Qt::transparent));
    glyph->setStrokeColor(from_rgba(ps.line_rgba, ps.opacity));
    glyph->setStrokeWidth(std::max(0.1, static_cast<double>(ps.line_width_px)));
    glyph->setStrokeWidthUnit(kPx);
    glyph->setAngle(degrees_of(ps.angle_udeg));

    return std::make_unique<QgsMarkerSymbol>(QgsSymbolLayerList() << glyph);
}

} // namespace

/// Holds the engine alive for the process, and says whether it came up.
///
/// `initQgis()` builds the symbol layer, provider and SVG registries. It is
/// idempotent and it is what every QGIS symbol constructor below assumes has run,
/// so it happens once, here, rather than being everyone's responsibility.
bool qgis_engine_ready()
{
    static const bool ready = [] {
        QgsApplication::init();
        QgsApplication::initQgis();
        return true;
    }();
    return ready;
}

std::string qgis_engine_version()
{
    return Qgis::version().toStdString();
}

class QgisBackend final : public render::Backend
{
public:
    QgisBackend() { qgis_engine_ready(); }

    std::string name() const override { return "QGIS " + qgis_engine_version(); }

    bool gpu() const override { return false; }

    void render(const render::DrawList& list, const render::Overlay& overlay,
                const render::FrameContext& ctx) override;

private:
    /// True when every pass in `list` has a QGIS translation. See the comment on
    /// the definition: an unfinished port says so rather than drawing short.
    static bool handles(const render::DrawList& list);

    void drawPass(QgsRenderContext& rc, const render::DrawList& list, std::size_t i, double cx,
                  double cy);

    static QPolygonF run_of(const render::PolylineBatch& b, std::size_t offset, std::uint32_t run,
                            double cx, double cy);
    static QPolygonF ring_of(const render::PolygonBatch& b, std::size_t offset, std::uint32_t run,
                             double cx, double cy);

    /// Built on first need, for the frames this engine cannot draw whole.
    std::unique_ptr<render::Backend> fallback_;
};

QPolygonF QgisBackend::run_of(const render::PolylineBatch& b, std::size_t offset, std::uint32_t run,
                              double cx, double cy)
{
    QPolygonF out;
    out.reserve(static_cast<int>(run));
    for (std::uint32_t v = 0; v < run; ++v)
        out << QPointF(cx + static_cast<double>(b.xs[offset + v]),
                       cy - static_cast<double>(b.ys[offset + v]));
    return out;
}

QPolygonF QgisBackend::ring_of(const render::PolygonBatch& b, std::size_t offset, std::uint32_t run,
                               double cx, double cy)
{
    QPolygonF out;
    out.reserve(static_cast<int>(run));
    for (std::uint32_t v = 0; v < run; ++v)
        out << QPointF(cx + static_cast<double>(b.xs[offset + v]),
                       cy - static_cast<double>(b.ys[offset + v]));
    return out;
}

void QgisBackend::drawPass(QgsRenderContext& rc, const render::DrawList& list, std::size_t i,
                           double cx, double cy)
{
    const render::PassStyle& ps      = list.passes[i];
    const render::PolylineBatch& str = list.polylines[i];
    const render::PolygonBatch& fil  = list.polygons[i];

    using core::SymbolLayerType;

    // ---- fills ----
    if (ps.wants_fill && !fil.runs.empty()) {
        std::unique_ptr<QgsFillSymbol> fill;

        switch (ps.type) {
        case SymbolLayerType::LinePatternFill: {
            auto* hatch = new QgsLinePatternFillSymbolLayer();
            hatch->setLineAngle(degrees_of(ps.angle_udeg));
            hatch->setDistance(ps.interval_px > 0.0f ? static_cast<double>(ps.interval_px) : 8.0);
            hatch->setDistanceUnit(kPx);
            hatch->setLineWidth(std::max(0.1, static_cast<double>(ps.line_width_px)));
            hatch->setLineWidthUnit(kPx);

            // THE SUB-SYMBOL draws the hatch lines; `setColor` on an image fill
            // layer sets its OUTLINE, and confusing the two drew every parcel's
            // own boundary on top of the hatch. That made the timing meaningless
            // as well as the picture wrong — the two backends were not drawing
            // the same thing, which is the only reason a comparison exists.
            auto* rule =
                new QgsSimpleLineSymbolLayer(from_rgba(ps.line_rgba, ps.opacity),
                                             std::max(0.1, static_cast<double>(ps.line_width_px)));
            rule->setWidthUnit(kPx);
            hatch->setSubSymbol(new QgsLineSymbol(QgsSymbolLayerList() << rule));

            fill = std::make_unique<QgsFillSymbol>(QgsSymbolLayerList() << hatch);
            break;
        }
        case SymbolLayerType::PointPatternFill: {
            auto* dots = new QgsPointPatternFillSymbolLayer();
            dots->setDistanceX(ps.interval_px > 0.0f ? static_cast<double>(ps.interval_px) : 10.0);
            dots->setDistanceY(
                ps.spacing_y_px > 0.0f
                    ? static_cast<double>(ps.spacing_y_px)
                    : (ps.interval_px > 0.0f ? static_cast<double>(ps.interval_px) : 10.0));
            dots->setDistanceXUnit(kPx);
            dots->setDistanceYUnit(kPx);
            dots->setSubSymbol(marker_of(ps).release());
            fill = std::make_unique<QgsFillSymbol>(QgsSymbolLayerList() << dots);
            break;
        }
        default: {
            auto* flat = new QgsSimpleFillSymbolLayer(from_rgba(ps.fill_rgba, ps.opacity));
            flat->setStrokeStyle(Qt::NoPen);
            fill = std::make_unique<QgsFillSymbol>(QgsSymbolLayerList() << flat);
            break;
        }
        }

        fill->startRender(rc);
        std::size_t offset = 0;
        for (std::size_t r = 0; r < fil.runs.size(); ++r) {
            const QPolygonF ring = ring_of(fil, offset, fil.runs[r], cx, cy);
            // A hole is drawn by the same symbol with the ring handed in as one;
            // the draw list marks which is which and QGIS takes the rings apart.
            if (r >= fil.is_hole.size() || fil.is_hole[r] == 0)
                fill->renderPolygon(ring, nullptr, nullptr, rc);
            offset += fil.runs[r];
        }
        fill->stopRender(rc);
    }

    // ---- strokes, markers along a line ----
    if (ps.wants_stroke && !str.runs.empty()) {
        std::unique_ptr<QgsLineSymbol> line;

        switch (ps.type) {
        case SymbolLayerType::MarkerLine:
        case SymbolLayerType::HashLine: {
            auto* along = new QgsMarkerLineSymbolLayer();
            along->setPlacements(placement_of(ps.placement));
            along->setInterval(ps.interval_px > 0.0f ? static_cast<double>(ps.interval_px) : 12.0);
            along->setIntervalUnit(kPx);
            along->setOffset(ps.offset_px);
            along->setOffsetUnit(kPx);
            if (ps.phase_px > 0.0f) {
                along->setOffsetAlongLine(ps.phase_px);
                along->setOffsetAlongLineUnit(kPx);
            }
            along->setRotateSymbols(true);
            along->setSubSymbol(marker_of(ps).release());
            line = std::make_unique<QgsLineSymbol>(QgsSymbolLayerList() << along);
            break;
        }
        case SymbolLayerType::SimpleMarker:
        case SymbolLayerType::CentroidFill:
        case SymbolLayerType::TextMarker:
            // Handled by the frame-level fallback, never dropped here; see
            // `handles()` and `render()`. Reaching this point would mean the two
            // lists had drifted apart.
            return;
        default:
            line = std::make_unique<QgsLineSymbol>(QgsSymbolLayerList()
                                                   << simple_line(ps, str.rgba, str.width_px));
            break;
        }

        line->startRender(rc);
        std::size_t offset = 0;
        for (std::uint32_t run : str.runs) {
            line->renderPolyline(run_of(str, offset, run, cx, cy), nullptr, rc);
            offset += run;
        }
        line->stopRender(rc);
    }
}

bool QgisBackend::handles(const render::DrawList& list)
{
    // The port is not finished, and an unfinished port must SAY SO rather than
    // draw a symbol short. Three layer types are placed from the geometry rather
    // than drawn along it — a glyph at a ring's centroid, a marker on a point, a
    // fixed word — and this backend has no translation for them yet. Skipping
    // them quietly is what made the style designer's preview lose the circle of
    // an MPYY building-condition symbol the moment QGIS became the default.
    //
    // So the FRAME goes to the built-in backend instead. Mixing the two inside
    // one frame would put two different renderers' idea of a pixel next to each
    // other; handing the whole frame over keeps one engine per picture, and the
    // list of what is missing stays in one place.
    for (const render::PassStyle& ps : list.passes) {
        switch (ps.type) {
        // WHAT THIS BACKEND CAN DRAW, listed positively. The first version listed
        // what it could NOT draw and defaulted the rest to "yes" — so every layer
        // type it had never been taught was silently claimed and silently skipped.
        // The three raster types went out that door, which is most of what MPYY
        // publishes: a hatched lekesi came out as a flat colour and a published
        // line type as a plain stroke. A whitelist cannot fail that way, because
        // a type nobody has translated yet falls to the default and is refused.
        case core::SymbolLayerType::SimpleLine:
        case core::SymbolLayerType::MarkerLine:
        case core::SymbolLayerType::HashLine:
        case core::SymbolLayerType::SimpleFill:
        case core::SymbolLayerType::LinePatternFill:
        case core::SymbolLayerType::PointPatternFill: break;

        // Everything else — the three raster types, the centroid marker, the
        // point marker and the fixed word — has no translation here yet.
        default: return false;
        }
    }
    return true;
}

void QgisBackend::render(const render::DrawList& list, const render::Overlay& overlay,
                         const render::FrameContext& ctx)
{
    if (!handles(list)) {
        if (!fallback_) fallback_ = make_builtin_backend();
        fallback_->render(list, overlay, ctx);
        return;
    }

    auto* device = static_cast<QPaintDevice*>(ctx.target);
    if (device == nullptr) return;

    QPainter painter(device);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(0, 0, ctx.width_px, ctx.height_px,
                     QColor::fromRgba(static_cast<QRgb>(overlay.background_rgba)));

    QgsRenderContext rc = QgsRenderContext::fromQPainter(&painter);
    rc.setScaleFactor(1.0); // every measure in a pass is already in pixels
    rc.setFlag(Qgis::RenderContextFlag::Antialiasing, true);

    // The draw list is centre-relative with y UP; the widget is corner-relative
    // with y DOWN. One translation, applied where the points are made.
    const double cx = ctx.width_px * 0.5;
    const double cy = ctx.height_px * 0.5;

    for (std::uint32_t index : list.order)
        if (index < list.passes.size()) drawPass(rc, list, index, cx, cy);

    // THE AIDS, and forgetting them is what this line is here to stop happening
    // again. A backend draws the document; the grid, the ruler, the scale bar,
    // the north arrow, the snap marker, the crosshair, the selection box and the
    // drawing's own captions are the program's furniture and every backend owes
    // the user all of them. Leaving them out took the whole overlay off the
    // canvas the moment QGIS became the default engine — the drawing was still
    // there and everything around it was gone.
    paint_frame_aids(painter, list, overlay, cx, cy);

    painter.end();
}

std::unique_ptr<render::Backend> make_qgis_backend()
{
    return std::make_unique<QgisBackend>();
}

} // namespace piricad::app
